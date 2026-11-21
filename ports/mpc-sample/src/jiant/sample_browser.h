// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// The SAMPLE BROWSER's controls (JiantApp draws it over the screen and owns the surface while it is open). Plain
// logic, no hardware and no threads, as panel_map.h: JiantApp feeds it the surface's events on the MIDI thread and
// carries out the actions it returns (the service thread loads and writes files, the audio thread the core).
//
//   SAMPLE              opens it; SAMPLE again, or the left button above the screen, closes it
//   pads 1..16          pick the slot (lit: it holds a sample; bright: the one picked)
//   wheel               scrolls the files of JIANT/Samples
//   above the screen    left CLOSE, middle AUDITION (the file; again: stop), right LOAD (the file into the slot)
//   ERASE               CLEAR the slot
//   SAMPLE SELECT       the source to record: LINE, MIC, USB, RESAMPLE (JIANT's own output)
//   SAMPLE REC          records the source into the slot (from now; again: stop); the slot is written to the card
#pragma once
#include "jiant/panel_map.h"
#include <cstdint>

namespace jiant_port {

enum class RecSource : uint8_t { Line, Mic, Usb, Resample, Count };
constexpr const char* kRecSourceName[] = {"LINE", "MIC", "USB", "RESAMPLE"};

class SampleBrowser {
public:
    enum class Action : uint8_t { None, Open, Close, Pick, Scroll, Audition, Load, Clear, Source, Record, Swallow };

    bool open() const { return open_; }
    unsigned slot() const { return slot_; }
    unsigned file() const { return file_; }
    RecSource source() const { return source_; }
    void set_files(unsigned n)                      // the folder's count (the service thread's scan)
    {
        files_ = n;
        if (file_ >= n)
            file_ = n ? n - 1 : 0;
    }

    // a button down or up: what the browser does with it (Swallow: nothing, but it is the browser's while open;
    // None: not the browser's, the panel's)
    Action button(unsigned b, bool down)
    {
        if (b == Sample) {
            if (down)
                open_ = !open_;
            return down ? (open_ ? Action::Open : Action::Close) : Action::Swallow;
        }
        if (!open_)
            return Action::None;
        if (!down)
            return Action::Swallow;
        switch (b) {
        case TopLeft: open_ = false; return Action::Close;
        case TopMiddle: return files_ ? Action::Audition : Action::Swallow;
        case TopRight: return files_ ? Action::Load : Action::Swallow;
        case Erase: return Action::Clear;
        case SampleSelect:
            source_ = RecSource((unsigned(source_) + 1) % unsigned(RecSource::Count));
            return Action::Source;
        case SampleRecord: return Action::Record;
        default: return Action::Swallow;
        }
    }
    Action pad(unsigned p, bool down)
    {
        if (!open_ || p >= kPads)
            return open_ ? Action::Swallow : Action::None;
        if (down)
            slot_ = p;
        return down ? Action::Pick : Action::Swallow;
    }
    Action wheel(int delta)
    {
        if (!open_)
            return Action::None;
        if (!files_)
            return Action::Swallow;
        long f = long(file_) + delta;
        file_ = unsigned(f < 0 ? 0 : f >= long(files_) ? long(files_) - 1 : f);
        return Action::Scroll;
    }

private:
    bool open_ = false;
    unsigned slot_ = 0, file_ = 0, files_ = 0;
    RecSource source_ = RecSource::Line;
};

}  // namespace jiant_port
