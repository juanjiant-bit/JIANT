// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// The editor's pictures, from the plugin's own code (its processor and editor linked in, as the standalone app runs
// them), at x2: out.png the main view (a second of a held chord), out-samples.png the sample browser open (two
// samples in it), out-sound.png track 2 selected, an FM6 sound in it, its presets and EDIT parameters.
//   editor_shot out.png
#include "../Source/JiantEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void run(juce::AudioProcessor& p, int blocks, std::initializer_list<int> notes = {})
{
    juce::AudioBuffer<float> buf(2, 512);
    for (int b = 0; b < blocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) for (int n : notes) midi.addEvent(juce::MidiMessage::noteOn(1, n, uint8_t(100)), 0);
        p.processBlock(buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    }
}

static bool shot(juce::AudioProcessorEditor& ed, const juce::File& f)
{
    auto img = ed.createComponentSnapshot(ed.getLocalBounds(), true, 2.0f);
    f.deleteFile();
    juce::FileOutputStream os(f);
    bool ok = os.openedOk() && juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf(ok ? "editor_shot: %s\n" : "editor_shot: could not write %s\n", f.getFullPathName().toRawUTF8());
    return ok;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) return 2;
    std::unique_ptr<juce::AudioProcessor> base(createPluginFilter());
    auto& p = *dynamic_cast<JiantProcessor*>(base.get());
    p.setRateAndBufferSizeDetails(44100.0, 512);
    p.prepareToPlay(44100.0, 512);
    run(p, 2);
    run(p, 120, {57, 60, 64});
    std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditorIfNeeded());
    auto& je = *dynamic_cast<JiantEditor*>(ed.get());
    ed->setVisible(true);
    for (int i = 0; i < 12; ++i) run(p, 4);              // the editor's timer brings the screen and the tracks in
    je.refreshNow();
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    auto sibling = [&](const char* tag) { return out.getSiblingFile(out.getFileNameWithoutExtension() + tag + out.getFileExtension()); };
    bool ok = shot(*ed, out);

    // two samples, the browser open
    std::vector<float> a(44100), b(30000);
    for (size_t i = 0; i < a.size(); ++i) a[i] = 0.5f * std::sin(0.03f * float(i)) * std::exp(-float(i) / 9000.0f);
    for (size_t i = 0; i < b.size(); ++i) b[i] = 0.4f * std::sin(0.11f * float(i) + 3.0f * std::sin(0.002f * float(i)));
    p.loadSample(0, a.data(), nullptr, int(a.size()), 44100.0, "KICK 808", "/samples/kick 808.wav");
    p.loadSample(3, b.data(), b.data(), int(b.size()), 48000.0, "GLASS PAD", "/samples/glass pad.flac");
    run(p, 4);
    p.service();
    je.showSamples(true);
    for (int i = 0; i < 4; ++i) run(p, 4);
    ok &= shot(*ed, sibling("-samples"));
    je.showSamples(false);

    // track 2, an FM6 sound (engine 12, preset 3)
    p.postRequest({27, 1});
    p.postRequest({8, 12, 3});
    for (int i = 0; i < 12; ++i) run(p, 4, i == 2 ? std::initializer_list<int> {48, 55} : std::initializer_list<int> {});
    je.refreshNow();
    ok &= shot(*ed, sibling("-sound"));

    p.editorBeingDeleted(ed.get());
    ed.reset();
    return ok ? 0 : 1;
}
