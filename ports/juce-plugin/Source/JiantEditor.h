// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM's editor: the FM-1 as the browser emulator draws it (web/emu/index.html, its WIDE layout, 1000 x 624):
// the screen, MASTER, SELECT, PRESETS, ALGORITHM and KNOB 1-4, the twelve buttons and OCT - / +, the 27 keys, their
// lights. Mouse or touch (several fingers: AUM on an iPad), the mouse wheel on the knobs, SHIFT-click holds a button,
// and the computer keyboard as the emulator's (Z..' and Q..O the keys, 1..= the buttons, [ ] OCT, arrows SELECT /
// ALGORITHM). The window scales, its shape kept.
#pragma once
#include "JiantProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

class JiantEditor : public juce::AudioProcessorEditor, private juce::Timer
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

    static constexpr int kW = 1000, kH = 624;           // the drawing's own units

private:
    void timerCallback() override;
    enum Kind { None, Button, Key, Knob };
    struct Hit { Kind kind = None; int index = -1; };
    Hit hitTest(juce::Point<float> p) const;            // p in the drawing's units
    juce::Point<float> toPanel(juce::Point<float> p) const;
    void syncButtons();
    void syncKeys();
    void turn(int knob, int n);

    JiantProcessor& proc_;
    juce::Image screen_ {juce::Image::RGB, 240, 240, true};
    std::array<uint16_t, 240 * 240> frame_ {};
    uint32_t seen_ = 0;
    uint32_t lit_ = 0, litKeys_ = 0;
    // what holds the panel: a finger or the mouse (by source index), a computer key (by key code); latched buttons
    std::map<int, int> heldButtons_, heldKeys_;         // source -> button bit / key
    std::map<int, int> keyButtons_, keyKeys_;           // computer key code -> button bit / key
    uint32_t latched_ = 0, sentButtons_ = ~0u, sentKeys_ = ~0u;
    struct Drag { int knob = -1; float y = 0; float acc = 0; };
    std::map<int, Drag> drags_;
    float angle_[8] = {};
    float scale_ = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JiantEditor)
};
