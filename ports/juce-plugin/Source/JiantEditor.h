// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM's editor, made for a DAW (not the FM-1's panel): JIANT's live screen in the middle, large and crisp (an
// integer scale of its 240 x 240), the way into everything the firmware's UI does (the wheel or a drag on it turns
// SELECT, with SHIFT ALGORITHM); its buttons as labelled tabs under it (HOME EDIT FX ENV LFO SCL ARP / SEQ GLO SAVE
// PLAY REC OCT- OCT+); the four page knobs as big rings beside it, SELECT, PRESETS, ALGORITHM and MASTER as small
// ones. On the left the six tracks (number, colour, engine, preset, level, mute; a click selects one) and a one-octave
// strip to audition; on the right the selected track's sound, read from the firmware's own descriptors through the
// web editor's protocol: its engine, its presets, its eight EDIT parameters as rings. The sample browser opens over
// the right side; audio files dropped anywhere go into it. The computer keyboard as the emulator's (Z..' and Q..O the
// keys, 1..= the buttons, [ ] OCT, arrows SELECT / ALGORITHM). The window scales, its shape kept.
#pragma once
#include "JiantProcessor.h"
#include "SamplePanel.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

class JiantEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit JiantEditor(JiantProcessor&);
    ~JiantEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;
    // audio files dropped anywhere: into the sample browser's slot (on a slot of it: that one)
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    static constexpr int kW = 1100, kH = 700;           // the drawing's own units
    void showSamples(bool on);                           // (editor_shot)
    void refreshNow();                                   // the device read again (editor_shot)

private:
    void timerCallback() override;
    enum Kind { None, Button, Key, Knob, Screen, TrackRow, Mute, EngPrev, EngNext, Preset, PresetUp, PresetDown, Edit,
                Samples };
    struct Hit { Kind kind = None; int index = -1; };
    Hit hitTest(juce::Point<float> p) const;            // p in the drawing's units
    juce::Point<float> toPanel(juce::Point<float> p) const;
    void syncButtons();
    void syncKeys();
    void turn(int knob, int n);
    void editTurn(int i, int n);
    void loadSound();                                    // the selected track's engine's presets and descriptors

    void paintTop(juce::Graphics&);
    void paintTracks(juce::Graphics&);
    void paintScreen(juce::Graphics&);
    void paintButtons(juce::Graphics&);
    void paintKnobs(juce::Graphics&);
    void paintSound(juce::Graphics&);
    void paintAudition(juce::Graphics&);

    JiantProcessor& proc_;
    juce::Image screen_ {juce::Image::RGB, 240, 240, true};
    std::array<uint16_t, 240 * 240> frame_ {};
    uint32_t seen_ = 0;
    uint32_t lit_ = 0, litKeys_ = 0;
    // what holds the panel: a finger or the mouse (by source index), a computer key (by key code); latched buttons
    std::map<int, int> heldButtons_, heldKeys_;
    std::map<int, int> keyButtons_, keyKeys_;
    uint32_t latched_ = 0, sentButtons_ = ~0u, sentKeys_ = ~0u;
    struct Drag { int kind = None; int index = -1; float y = 0; float acc = 0; };
    std::map<int, Drag> drags_;
    float angle_[8] = {};
    float scale_ = 1.0f;
    int tick_ = 0;
    SamplePanel samples_;

    // the device, as the protocol reads it
    JiantProcessor::Link link_;
    struct Desc { juce::String label, unit; int fmt = 0, min = 0, max = 127; juce::StringArray names; };
    struct Sound { int engine = -1; juce::StringArray presets; Desc edit[8]; };
    Sound sound_;
    int presetTop_ = 0;                                  // the preset list's first row shown
    int pending_[8] = {};                                // EDIT values set here, not read back yet (or INT_MIN)
    int64_t pendingAt_[8] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JiantEditor)
};
