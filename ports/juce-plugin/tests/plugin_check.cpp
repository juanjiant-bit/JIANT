// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// The built JIANT FM plugin loaded as a host loads it (JUCE's VST3 / AU hosting): its description, MIDI notes at
// 44.1 and 48 kHz in several block sizes (sound, then silence after the note-offs, nothing non-finite), the state
// out and back in, several instances at once (each its own JIANT, on its own thread; the 9th silent), the editor made and closed.
//   plugin_check "<path to JIANT FM.vst3 or .component>" [a state with samples, from sample_check]
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

static int fails = 0;
static void check(bool ok, const juce::String& what)
{
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    fails += !ok;
}

struct Take { float peak = 0, tail = 0; bool finite = true; };
static Take play(juce::AudioPluginInstance& p, double rate, int block, double seconds)
{
    p.setRateAndBufferSizeDetails(rate, block);
    p.prepareToPlay(rate, block);
    juce::AudioBuffer<float> buf(2, block);
    Take t;
    const int blocks = int(seconds * rate / block), offAt = blocks / 3, tailFrom = blocks - int(0.4 * rate / block);
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0)
            for (int n : {48, 55, 60, 64}) midi.addEvent(juce::MidiMessage::noteOn(1, n, uint8_t(100)), block / 2);
        if (b == offAt)
            for (int n : {48, 55, 60, 64}) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0);
        buf.clear();
        p.processBlock(buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i) {
                float v = buf.getSample(c, i);
                t.finite &= std::isfinite(v);
                t.peak = std::max(t.peak, std::abs(v));
                if (b >= tailFrom) t.tail = std::max(t.tail, std::abs(v));
            }
    }
    p.releaseResources();
    return t;
}

// ---- several JIANTs in one session (ports/core/jiant_copies.h): each instance its own device
struct Out { std::vector<float> l, r; float peak = 0; bool finite = true; };
// one instance's take: its sound (the web editor's PRESET, a SysEx), then note held for half the frames
static Out take(juce::AudioPluginInstance& p, int engine, int note, int frames, int block = 256)
{
    Out o;
    juce::AudioBuffer<float> buf(2, block);
    for (int at = 0, b = 0; at < frames; at += block, ++b) {
        juce::MidiBuffer midi;
        if (b == 0 && engine >= 0) {
            const uint8_t sx[] = {0xF0, 0x7D, 0x46, 0x4C, 8, uint8_t(engine), 0, 0xF7};
            midi.addEvent(sx, int(sizeof sx), 0);
        }
        if (b == 2 && note > 0) midi.addEvent(juce::MidiMessage::noteOn(1, note, uint8_t(100)), 0);
        if (at >= frames / 2 && at - block < frames / 2 && note > 0) midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        buf.clear();
        p.processBlock(buf, midi);
        for (int i = 0; i < block; ++i) {
            float l = buf.getSample(0, i), r = buf.getSample(1, i);
            o.finite &= std::isfinite(l) && std::isfinite(r);
            o.peak = std::max(o.peak, std::max(std::abs(l), std::abs(r)));
            o.l.push_back(l);
            o.r.push_back(r);
        }
    }
    return o;
}
static std::vector<char> stateOf(juce::AudioPluginInstance& p)
{
    juce::MemoryBlock m;
    p.getStateInformation(m);
    auto* c = static_cast<const char*>(m.getData());
    return std::vector<char>(c, c + m.getSize());
}

static void severalJiants(juce::AudioPluginFormatManager& fm, const juce::PluginDescription& d, juce::AudioPluginInstance& first)
{
    juce::String err;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> more;
    std::vector<juce::AudioPluginInstance*> four {&first};
    for (int i = 1; i < 4; ++i) {
        more.push_back(fm.createPluginInstance(d, 44100.0, 256, err));
        if (!more.back()) break;
        four.push_back(more.back().get());
    }
    check(four.size() == 4, "4 instances in one process");
    if (four.size() != 4) return;
    for (auto* p : four) {
        p->setRateAndBufferSizeDetails(44100.0, 256);
        p->prepareToPlay(44100.0, 256);
    }
    const int engines[4] = {0, 12, 2, 3}, notes[4] = {48, 60, 67, 55};   // ANALOG, FM6, PHASE, LOFI
    const int frames = 44100;
    Out outs[4];
    {                                                    // all four at once, each on its own thread
        std::vector<std::thread> th;
        for (int i = 0; i < 4; ++i)
            th.emplace_back([&, i] { outs[i] = take(*four[size_t(i)], engines[i], notes[i], frames); });
        for (auto& t : th) t.join();
    }
    bool sound = true, differ = true;
    for (int i = 0; i < 4; ++i) {
        sound &= outs[i].finite && outs[i].peak > 0.001f;
        for (int j = 0; j < i; ++j) differ &= outs[i].l != outs[j].l;
    }
    check(sound, "4 instances on 4 threads at once, each its own sound and note: peaks " +
                     juce::String(outs[0].peak, 3) + " " + juce::String(outs[1].peak, 3) + " " +
                     juce::String(outs[2].peak, 3) + " " + juce::String(outs[3].peak, 3));
    check(differ, "and no two the same");
    {                                                    // a note into one: the other three, running, stay silent
        for (auto* p : four) take(*p, -1, 0, 4 * 44100);  // (the tails gone first)
        std::vector<std::thread> th;
        for (int i = 0; i < 4; ++i)
            th.emplace_back([&, i] { outs[i] = take(*four[size_t(i)], -1, i == 1 ? 64 : 0, frames); });
        for (auto& t : th) t.join();
        check(outs[1].peak > 0.001f && outs[0].peak == 0.0f && outs[2].peak == 0.0f && outs[3].peak == 0.0f,
              "a note into instance 2 sounds there only (peak " + juce::String(outs[1].peak, 3) + ", the others 0)");
    }
    std::vector<char> st[4];
    for (int i = 0; i < 4; ++i) st[i] = stateOf(*four[size_t(i)]);
    check(st[0] != st[1] && st[1] != st[2] && st[0] != st[3], "each instance's state its own (" +
                                                                juce::String(int(st[1].size())) + " bytes)");
    take(*four[2], 10, 0, 2048);                         // instance 3 to DRUM: its state only
    std::vector<char> now[4];
    for (int i = 0; i < 4; ++i) now[i] = stateOf(*four[size_t(i)]);
    check(now[2] != st[2] && now[0] == st[0] && now[1] == st[1] && now[3] == st[3],
          "a sound loaded into instance 3 changes its state, no other's");
    four[2]->setStateInformation(st[2].data(), int(st[2].size()));
    take(*four[2], -1, 0, 1024);
    check(stateOf(*four[2]) == st[2], "and its own state put back restores it");
    four[3]->setStateInformation(st[1].data(), int(st[1].size()));
    Out a = take(*four[1], -1, 60, 22050), b = take(*four[3], -1, 60, 22050);
    check(stateOf(*four[3]) == stateOf(*four[1]) && a.peak > 0.001f && b.peak > 0.001f,
          "instance 2's state into instance 4: the same music in both");

    for (int i = 4; i < 8; ++i) more.push_back(fm.createPluginInstance(d, 44100.0, 256, err));
    bool eight = true;
    for (int i = 4; i < 8; ++i) {
        auto& p = more[size_t(i - 1)];
        eight &= p != nullptr;
        if (p) {
            p->setRateAndBufferSizeDetails(44100.0, 256);
            p->prepareToPlay(44100.0, 256);
            eight &= take(*p, -1, 60, 11025).peak > 0.001f;
        }
    }
    check(eight, "8 instances, each sounds");
    auto ninth = fm.createPluginInstance(d, 44100.0, 256, err);
    if (ninth) {
        ninth->setRateAndBufferSizeDetails(44100.0, 256);
        ninth->prepareToPlay(44100.0, 256);
        Out o = take(*ninth, 0, 60, 22050);
        check(o.finite && o.peak == 0.0f, "a 9th stays silent (all 8 JIANTs taken)");
        ninth.reset();
    }
    more[5].reset();                                     // (the 7th) gone: its JIANT is free again
    auto again = fm.createPluginInstance(d, 44100.0, 256, err);
    if (again) {
        again->setRateAndBufferSizeDetails(44100.0, 256);
        again->prepareToPlay(44100.0, 256);
        Out o = take(*again, -1, 60, 11025);
        check(o.peak > 0.001f, "one closed, a new instance plays in its place");
    }
    again.reset();
    more.clear();
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf("usage: plugin_check <plugin> [state]\n"); return 2; }
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager(fm);
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : fm.getFormats())
        f->findAllTypesForFile(found, argv[1]);
    check(found.size() == 1, "the plugin found (" + juce::String(found.size()) + " in " + juce::String(argv[1]) + ")");
    if (found.isEmpty()) return 1;
    const auto& d = *found[0];
    check(d.name == "JIANT FM" && d.manufacturerName == "Juan Jiant" && d.isInstrument,
          "described as an instrument: " + d.name + " by " + d.manufacturerName + " (" + d.pluginFormatName + ")");
    juce::String err;
    auto p = fm.createPluginInstance(d, 44100.0, 512, err);
    check(p != nullptr, "an instance made " + err);
    if (!p) return 1;
    check(p->acceptsMidi() && p->getTotalNumOutputChannels() == 2, "MIDI in, stereo out");
    check(p->getParameters().size() >= 2, juce::String(p->getParameters().size()) + " parameters");

    for (auto [rate, block] : {std::pair<double, int>{44100.0, 512}, {44100.0, 32}, {48000.0, 96}, {96000.0, 1024}}) {
        Take t = play(*p, rate, block, 3.0);
        check(t.finite && t.peak > 0.001f && t.tail < 0.0005f,
              juce::String(rate / 1000.0, 1) + " kHz, " + juce::String(block) + " frames: the chord sounds (peak " +
                  juce::String(t.peak, 3) + ") and stops (tail " + juce::String(t.tail, 5) + ")");
    }

    juce::MemoryBlock state;
    p->getStateInformation(state);
    check(state.getSize() > 64, "the state: " + juce::String(int(state.getSize())) + " bytes");
    p->setStateInformation(state.getData(), int(state.getSize()));
    Take t = play(*p, 44100.0, 256, 1.5);
    check(t.finite && t.peak > 0.001f, "plays after the state is put back");
    juce::MemoryBlock again;
    p->getStateInformation(again);
    check(again.getSize() == state.getSize(), "and saves the same size again");

    severalJiants(fm, d, *p);

    if (argc > 2 && d.pluginFormatName == "VST3") {    // a state with samples (sample_check's) into the built plugin
        juce::MemoryBlock st;
        juce::File(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2])).loadFileAsData(st);
        auto q = fm.createPluginInstance(d, 44100.0, 256, err);
        check(q && st.getSize() > 40000, "a session's state with its samples: " + juce::String(int(st.getSize())) + " bytes");
        if (q) {
            q->setRateAndBufferSizeDetails(44100.0, 256);
            q->prepareToPlay(44100.0, 256);
            take(*q, -1, 0, 4096);
            // (a VST3 host's state is the component's, in JUCE's hosting wrapped as XML)
            juce::XmlElement wrap("VST3PluginState");
            wrap.createNewChildElement("IComponent")->addTextElement(st.toBase64Encoding());
            juce::MemoryBlock wrapped;
            juce::AudioProcessor::copyXmlToBinary(wrap, wrapped);
            q->setStateInformation(wrapped.getData(), int(wrapped.getSize()));
            Out o = take(*q, -1, 60, 44100);
            juce::MemoryBlock got, back;
            q->getStateInformation(got);
            if (auto x = juce::AudioProcessor::getXmlFromBinary(got.getData(), int(got.getSize())))
                if (auto* c = x->getChildByName("IComponent"))
                    back.fromBase64Encoding(c->getAllSubText());
            check(o.finite && o.peak > 0.02f && back.getSize() + 64 > st.getSize(),
                  "restored: slot 1 plays in SAMPLE (peak " + juce::String(o.peak, 3) + "), saved again with its audio (" +
                      juce::String(int(back.getSize())) + " bytes)");
        }
    }

    if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr) {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditorIfNeeded());
        check(ed != nullptr && ed->getWidth() >= 500, "the editor: " + juce::String(ed ? ed->getWidth() : 0) + " x " +
                                                          juce::String(ed ? ed->getHeight() : 0));
        if (ed) p->editorBeingDeleted(ed.get());
    } else {
        std::printf("  --    no display: the editor not made\n");
    }
    p.reset();
    std::printf(fails ? "plugin check: %d FAILED\n" : "plugin check: all passed\n", fails);
    return fails != 0;
}
