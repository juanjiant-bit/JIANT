// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// The sample browser: JIANT's 16 slots (what SAMPLE, GRAIN and DRUM-X's sample layer play), each its name, length and
// channels; pick a slot, LOAD a file into it (WAV, AIFF, FLAC, MP3, OGG), CLEAR it; a file dropped on a slot goes
// there (the editor takes drops anywhere else into the picked slot). What JIANT records (REC in SAMPLE / GRAIN) shows
// here as it lands. All of it the processor's (JiantProcessor::loadSample), saved with the session.
#pragma once
#include "JiantProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SamplePanel : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit SamplePanel(JiantProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray&, int x, int y) override;
    void fileDragExit(const juce::StringArray&) override;

    int selected() const { return selected_; }
    void select(int k);
    // files into slots from k on (the editor's drop, the tests): the message of the last, "" all loaded
    juce::String load(int k, const juce::StringArray& files);
    void chooseFile();                                   // LOAD: a file chooser for the picked slot
    std::function<void()> onClose;                       // (the editor hides it)
    static bool isAudioFile(const juce::String& path);

private:
    void timerCallback() override;
    int rowAt(juce::Point<int> p) const;
    juce::Rectangle<int> rowRect(int k) const;
    juce::Rectangle<int> loadRect() const, clearRect() const, closeRect() const;

    JiantProcessor& proc_;
    int selected_ = 0, dropRow_ = -1;
    uint32_t seen_ = ~0u;
    juce::String status_;
    std::unique_ptr<juce::FileChooser> chooser_;
    int recShown_ = -1;
};
