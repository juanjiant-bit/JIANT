// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// The editor's picture, from the plugin's own code (its processor and editor linked in, as the standalone app runs
// them): a second of a held chord, PLAY, then the window painted to a PNG at x2.
//   editor_shot out.png
#include "../Source/JiantProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) return 2;
    std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
    p->setRateAndBufferSizeDetails(44100.0, 512);
    p->prepareToPlay(44100.0, 512);
    juce::AudioBuffer<float> buf(2, 512);
    for (int b = 0; b < 120; ++b) {
        juce::MidiBuffer midi;
        if (b == 2) for (int n : {57, 60, 64}) midi.addEvent(juce::MidiMessage::noteOn(1, n, uint8_t(100)), 0);
        p->processBlock(buf, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditorIfNeeded());
    ed->setVisible(true);
    for (int i = 0; i < 10; ++i) {                     // the editor's timer brings the screen in
        juce::MidiBuffer none;
        p->processBlock(buf, none);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(40);
    }
    auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
    juce::FileOutputStream os(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    os.setPosition(0);
    os.truncate();
    bool ok = os.openedOk() && juce::PNGImageFormat().writeImageToStream(img, os);
    std::printf(ok ? "editor_shot: %s\n" : "editor_shot: could not write %s\n", argv[1]);
    p->editorBeingDeleted(ed.get());
    ed.reset();
    return ok ? 0 : 1;
}
