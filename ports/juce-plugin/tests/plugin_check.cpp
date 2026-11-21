// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// The built JIANT FM plugin loaded as a host loads it (JUCE's VST3 / AU hosting): its description, MIDI notes at
// 44.1 and 48 kHz in several block sizes (sound, then silence after the note-offs, nothing non-finite), the state
// out and back in, a second instance silent, the editor made and closed.
//   plugin_check "<path to JIANT FM.vst3 or .component>"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cmath>
#include <cstdio>

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

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf("usage: plugin_check <plugin>\n"); return 2; }
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

    auto second = fm.createPluginInstance(d, 44100.0, 512, err);
    if (second) {
        Take s = play(*second, 44100.0, 512, 1.0);
        check(s.finite && s.peak == 0.0f, "a second instance stays silent (one device a session)");
        second.reset();
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
