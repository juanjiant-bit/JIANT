// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// Samples in the plugin (the plugin's own processor and editor, as a host makes them): a WAV written here (48 kHz
// stereo, half a second) dropped on the editor as a DAW drops a file (its filesDropped, the browser's slot 1); SAMPLE's
// first preset (the web editor's PRESET, a SysEx) silent with the slot empty, sounding with it loaded; REC MST 1 from
// inside JIANT (SAMPLE's REC) records a bar into the slot; the state out (the slot's audio in it, FLAC), a new instance
// (another JIANT) takes it: the slot back, the same length and name, sounding. The state is written to the file given,
// for plugin_check to load into the built plugin.
//   sample_check [state.bin]
#include "../Source/JiantProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static int fails;
static void check(bool ok, const juce::String& what)
{
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    fails += !ok;
}

static float play(JiantProcessor& p, std::initializer_list<std::vector<uint8_t>> sysex, int note, int blocks)
{
    juce::AudioBuffer<float> buf(2, 512);
    float peak = 0;
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0)
            for (auto& sx : sysex) midi.addEvent(sx.data(), int(sx.size()), 0);
        if (b == 2 && note) midi.addEvent(juce::MidiMessage::noteOn(1, note, uint8_t(110)), 0);
        if (b == blocks - 8 && note) midi.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        p.processBlock(buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i) peak = std::max(peak, std::abs(buf.getSample(c, i)));
    }
    return peak;
}
// one block, then the service: what was handed to the audio thread is in, the browser's view read
static void settle(JiantProcessor& p)
{
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer none;
    p.processBlock(buf, none);
    p.service();
}
// SET scope 0 (the selected track), id, value
static std::vector<uint8_t> set(int id, int v)
{
    int x = v + 8192;
    return {0xF0, 0x7D, 0x46, 0x4C, 3, 0, uint8_t(id), uint8_t(x & 127), uint8_t(x >> 7), 0xF7};
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const std::vector<uint8_t> sampleOne {0xF0, 0x7D, 0x46, 0x4C, 8, 4, 0, 0xF7};   // PRESET SAMPLE 0: ONE, slot 1
    std::unique_ptr<juce::AudioProcessor> base(createPluginFilter());
    auto& p = *dynamic_cast<JiantProcessor*>(base.get());
    p.setRateAndBufferSizeDetails(44100.0, 512);
    p.prepareToPlay(44100.0, 512);
    check(p.owner(), "an instance with its JIANT (" + juce::String(p.copy() + 1) + " of " + juce::String(JiantProcessor::copies()) + ")");
    float empty = play(p, {sampleOne}, 60, 40);
    check(empty == 0.0f && p.slot(0).frames == 0, "SAMPLE with slot 1 empty: silent");

    // a WAV, 48 kHz stereo: 0.5 s of 330 Hz (left) and 495 Hz (right), fading
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("jiant_sample_check");
    dir.createDirectory();
    auto wav = dir.getChildFile("synth tone.wav");
    {
        juce::AudioBuffer<float> b(2, 24000);
        for (int i = 0; i < 24000; ++i) {
            float env = 0.6f * (1.0f - float(i) / 24000.0f);
            b.setSample(0, i, env * std::sin(2.0f * juce::MathConstants<float>::pi * 330.0f * float(i) / 48000.0f));
            b.setSample(1, i, env * std::sin(2.0f * juce::MathConstants<float>::pi * 495.0f * float(i) / 48000.0f));
        }
        wav.deleteFile();
        std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream>(wav);
        auto w = juce::WavAudioFormat().createWriterFor(os, juce::AudioFormatWriterOptions {}.withSampleRate(48000.0)
                                                                .withNumChannels(2).withBitsPerSample(24));
        check(w && w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()), "a WAV written: " + wav.getFullPathName());
    }

    // dropped on the editor, as a DAW drops a file: into the browser's slot (slot 1)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditorIfNeeded());
        auto* target = dynamic_cast<juce::FileDragAndDropTarget*>(ed.get());
        check(target && target->isInterestedInFileDrag({wav.getFullPathName()}) &&
                  !target->isInterestedInFileDrag({dir.getChildFile("notes.txt").getFullPathName()}),
              "the editor takes audio files dropped on it, not others");
        if (target)
            target->filesDropped({wav.getFullPathName()}, ed->getWidth() / 2, ed->getHeight() / 2);
        p.editorBeingDeleted(ed.get());
    }
    settle(p);
    const auto s = p.slot(0);
    check(s.frames > 22000 && s.frames < 22100 && s.channels == 2 && s.name == "SYNTH TONE" && s.file == wav.getFullPathName(),
          "dropped into slot 1: " + s.name + ", " + juce::String(s.frames) + " frames at 44.1 kHz, " +
              juce::String(int(s.channels)) + " channels");
    float loud = play(p, {sampleOne}, 60, 60);
    check(loud > 0.02f, "SAMPLE plays it (peak " + juce::String(loud, 3) + ")");
    check(p.loadSample(3, dir.getChildFile("nothing.wav")).isNotEmpty(), "a missing file says why: " +
                                                                              p.loadSample(3, dir.getChildFile("nothing.wav")));

    // REC MST 1 (SAMPLE's REC, P_LN4 = id 87, value 6), the transport stopped: a bar of JIANT's own output into slot 2
    // (SLOT, E1 = id 115, 1), slot 1 playing
    {
        play(p, {sampleOne}, 0, 4);
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        auto rec = set(87, 6);
        midi.addEvent(rec.data(), int(rec.size()), 0);
        auto slot2 = set(115, 1);
        midi.addEvent(slot2.data(), int(slot2.size()), 0);
        p.processBlock(buf, midi);
        settle(p);
        int at = 0, frames = 0;
        int st = p.recState(&at, &frames);
        auto back = set(115, 0);                         // (slot 1 sounds into the recording)
        play(p, {back}, 0, 1);
        float pk = 0;
        for (int b = 0; b < 400 && (b < 4 || p.recState()); ++b) {
            juce::MidiBuffer m;
            if (b % 40 == 1) m.addEvent(juce::MidiMessage::noteOn(1, 60, uint8_t(110)), 0);
            if (b % 40 == 30) m.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            p.processBlock(buf, m);
            pk = std::max(pk, buf.getMagnitude(0, 512));
            if (b % 20 == 0) p.service();
        }
        p.service();
        const auto r = p.slot(1);
        check(st == 2 && at == 1 && r.frames > 40000 && r.name.isNotEmpty() && r.file.isEmpty() && pk > 0.01f,
              "REC MST 1 from inside JIANT (state " + juce::String(st) + ", slot " + juce::String(at + 1) + "): slot 2 recorded, " + juce::String(r.frames) + " frames (a bar), named " + r.name);
    }

    juce::MemoryBlock state;
    p.getStateInformation(state);
    check(state.getSize() > 22050 * 2, "the state with two slots' audio: " + juce::String(int(state.getSize())) + " bytes");
    if (argc > 1) {
        juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        out.replaceWithData(state.getData(), state.getSize());
    }
    juce::MemoryBlock again;
    p.getStateInformation(again);
    check(again == state, "saved twice, the same bytes (the FLAC kept)");

    // a new instance, another JIANT: the state in, the slots back
    {
        std::unique_ptr<juce::AudioProcessor> other(createPluginFilter());
        auto& q = *dynamic_cast<JiantProcessor*>(other.get());
        q.setRateAndBufferSizeDetails(48000.0, 512);
        q.prepareToPlay(48000.0, 512);
        check(q.owner() && q.copy() != p.copy(), "a second instance, JIANT " + juce::String(q.copy() + 1));
        q.setStateInformation(state.getData(), int(state.getSize()));
        settle(q);
        const auto a = q.slot(0), b = q.slot(1);
        check(a.frames == s.frames && a.channels == 2 && a.name == s.name && a.file == s.file &&
                  b.frames == p.slot(1).frames && b.name == p.slot(1).name,
              "its slots back from the state: " + a.name + " " + juce::String(a.frames) + ", " + b.name + " " +
                  juce::String(b.frames));
        float there = play(q, {}, 60, 60);
        check(there > 0.02f, "and SAMPLE plays slot 1 there (peak " + juce::String(there, 3) + ", at 48 kHz)");
        juce::MemoryBlock theirs;
        q.getStateInformation(theirs);
        q.clearSample(0);
        settle(q);
        check(q.slot(0).frames == 0 && q.slot(1).frames > 0, "CLEAR empties slot 1, slot 2 kept");
        q.setStateInformation(state.getData(), int(state.getSize()));
        settle(q);
        check(q.slot(0).frames == s.frames, "and the state brings it back");
    }
    dir.deleteRecursively();
    p.releaseResources();
    std::printf(fails ? "juce samples: %d FAILED\n" : "juce samples: all passed\n", fails);
    return fails != 0;
}
