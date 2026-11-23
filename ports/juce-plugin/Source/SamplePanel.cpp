// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "SamplePanel.h"
#include "JiantLook.h"
#include "jiant_core.h"

namespace {
constexpr int kHead = 34, kRow = 22, kFoot = 64;
}

SamplePanel::SamplePanel(JiantProcessor& p) : proc_(p)
{
    setOpaque(true);
    startTimerHz(10);
}

bool SamplePanel::isAudioFile(const juce::String& path)
{
    static const juce::StringArray ext {".wav", ".wave", ".aif", ".aiff", ".aifc", ".flac", ".mp3", ".ogg", ".caf", ".m4a"};
    for (auto& e : ext)
        if (path.endsWithIgnoreCase(e))
            return true;
    return false;
}

juce::Rectangle<int> SamplePanel::rowRect(int k) const { return {8, kHead + k * kRow, getWidth() - 16, kRow - 2}; }
juce::Rectangle<int> SamplePanel::loadRect() const { return {8, getHeight() - kFoot + 6, (getWidth() - 24) / 2, 26}; }
juce::Rectangle<int> SamplePanel::clearRect() const
{
    auto l = loadRect();
    return l.withX(l.getRight() + 8);
}
juce::Rectangle<int> SamplePanel::closeRect() const { return {getWidth() - 30, 6, 22, 22}; }

int SamplePanel::rowAt(juce::Point<int> p) const
{
    for (int k = 0; k < JiantProcessor::kSlots; ++k)
        if (rowRect(k).contains(p))
            return k;
    return -1;
}

void SamplePanel::select(int k)
{
    selected_ = juce::jlimit(0, JiantProcessor::kSlots - 1, k);
    repaint();
}

void SamplePanel::timerCallback()
{
    int slot = 0, frames = 0;
    int rec = proc_.recState(&slot, &frames);
    int shown = rec ? slot * 1000000 + rec * 100000 + frames / 4410 : -1;
    if (proc_.slotsChanged() != seen_ || shown != recShown_) {
        seen_ = proc_.slotsChanged();
        recShown_ = shown;
        repaint();
    }
}

void SamplePanel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(jlook::body);
    g.fillRect(r);
    g.setColour(jlook::line);
    g.drawRect(r, 1.0f);
    jlook::brackets(g, r.reduced(1), jlook::org, 10.0f, 1.5f);
    g.setFont(jlook::dot(15.0f));
    g.setColour(jlook::cream);
    g.drawText("SAMPLES", 12, 6, getWidth() - 50, 22, juce::Justification::centredLeft);
    g.setColour(jlook::dim);
    g.setFont(jlook::mono(10.0f));
    g.drawText("16 SLOTS  -  DROP FILES", 92, 6, getWidth() - 130, 22, juce::Justification::centredLeft);
    auto x = closeRect().toFloat().reduced(6);
    g.setColour(jlook::ink);
    g.drawLine(x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.5f);
    g.drawLine(x.getRight(), x.getY(), x.getX(), x.getBottom(), 1.5f);

    int recSlot = 0, recFrames = 0;
    const int rec = proc_.recState(&recSlot, &recFrames);
    for (int k = 0; k < JiantProcessor::kSlots; ++k) {
        const auto& s = proc_.slot(k);
        auto rr = rowRect(k).toFloat();
        const bool sel = k == selected_, drop = k == dropRow_;
        g.setColour(sel ? jlook::org.withAlpha(0.14f) : (k & 1) ? jlook::surf : jlook::panel);
        g.fillRect(rr);
        if (sel || drop) {
            g.setColour(drop ? jlook::cyan : jlook::org);
            g.drawRect(rr, 1.25f);
        }
        g.setFont(jlook::mono(11.0f));
        g.setColour(s.frames ? jlook::heat(float(k) / 15.0f) : jlook::faint);
        g.drawText(juce::String(k + 1).paddedLeft('0', 2), rr.withWidth(26).translated(6, 0), juce::Justification::centredLeft);
        juce::String name = s.frames ? s.name : juce::String("-");
        juce::String len;
        if (rec && recSlot == k) {
            name = rec == 1 ? "REC ARMED" : "REC";
            len = juce::String(recFrames / double(JIANT_RATE), 1) + " s";
        } else if (s.frames) {
            len = juce::String(s.frames / double(JIANT_RATE), 2) + " s " + (s.channels > 1 ? "ST" : "MO");
        }
        g.setFont(jlook::dot(13.0f));
        g.setColour(rec && recSlot == k ? jlook::red : s.frames ? jlook::ink : jlook::faint);
        g.drawText(name, rr.withTrimmedLeft(34).withTrimmedRight(84), juce::Justification::centredLeft, true);
        g.setFont(jlook::mono(10.5f));
        g.setColour(jlook::dim);
        g.drawText(len, rr.withTrimmedRight(6), juce::Justification::centredRight);
    }
    // the picked slot's file, LOAD and CLEAR, what last happened
    const auto& s = proc_.slot(selected_);
    g.setFont(jlook::mono(10.0f));
    g.setColour(status_.isNotEmpty() ? jlook::yellow : jlook::dim);
    juce::String info = status_.isNotEmpty() ? status_
                        : s.frames ? (s.file.isNotEmpty() ? juce::File(s.file).getFileName() : juce::String("RECORDED IN JIANT"))
                                   : juce::String("SLOT ") + juce::String(selected_ + 1) + " EMPTY";
    g.drawText(info, 10, getHeight() - kFoot + 36, getWidth() - 20, 20, juce::Justification::centredLeft, true);
    jlook::button(g, loadRect().toFloat(), "LOAD", false);
    jlook::button(g, clearRect().toFloat(), "CLEAR", false, false, jlook::vio);
}

void SamplePanel::mouseDown(const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    if (closeRect().contains(p)) {
        if (onClose) onClose();
        return;
    }
    if (loadRect().contains(p)) {
        chooseFile();
        return;
    }
    if (clearRect().contains(p)) {
        proc_.clearSample(selected_);
        status_ = {};
        repaint();
        return;
    }
    if (int k = rowAt(p); k >= 0) {
        status_ = {};
        select(k);
    }
}

void SamplePanel::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (int k = rowAt(e.getPosition()); k >= 0) {
        select(k);
        chooseFile();
    }
}

void SamplePanel::chooseFile()
{
    const int k = selected_;
    chooser_ = std::make_unique<juce::FileChooser>("A sample for slot " + juce::String(k + 1), juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<SamplePanel> self(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [self, k](const juce::FileChooser& fc) {
                              if (self && fc.getResult().existsAsFile())
                                  self->load(k, juce::StringArray(fc.getResult().getFullPathName()));
                          });
}

juce::String SamplePanel::load(int k, const juce::StringArray& files)
{
    juce::String why;
    int loaded = 0;
    for (auto& f : files) {
        if (k >= JiantProcessor::kSlots)
            break;
        why = proc_.loadSample(k, juce::File(f));
        if (why.isEmpty())
            ++loaded, ++k;
    }
    status_ = why.isNotEmpty() ? why : loaded > 1 ? juce::String(loaded) + " FILES LOADED" : juce::String();
    if (loaded)
        select(juce::jmin(k - 1, JiantProcessor::kSlots - 1));
    repaint();
    return why;
}

bool SamplePanel::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile(f))
            return true;
    return false;
}

void SamplePanel::fileDragMove(const juce::StringArray&, int x, int y)
{
    int k = rowAt({x, y});
    if (k != dropRow_) {
        dropRow_ = k;
        repaint();
    }
}

void SamplePanel::fileDragExit(const juce::StringArray&)
{
    dropRow_ = -1;
    repaint();
}

void SamplePanel::filesDropped(const juce::StringArray& files, int x, int y)
{
    dropRow_ = -1;
    int k = rowAt({x, y});
    load(k >= 0 ? k : selected_, files);
}
