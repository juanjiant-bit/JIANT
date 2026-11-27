// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// The MPC Sample's surface as the FM-1's panel: which pad is which key, which button is which, and the knobs as
// JIANT's encoders. Plain logic, no hardware and no threads: JiantApp feeds it the surface's events on the MIDI
// thread and sends what it returns to the core on the audio thread.
//
//   pads 1..16          16 of the FM-1's 27 keys (F3..G5); PAD BANK moves the window: A F3..G#4, B A3..C5,
//                       C C#4..E5, D E4..G5 (SHIFT + PAD BANK back)
//   above the screen    HOME, EDIT, GLO
//   PAD FX  FX          KNOB FX  LFO        CHOP  ENV        16 LEVELS  SCL
//   LOOP    ARP         SEQ      SEQ        SAMPLE SELECT  SAVE
//   PLAY    PLAY        STOP     PLAY when playing           SEQ REC / SAMPLE REC  REC
//   MINUS   OCT -       PLUS     OCT +
//   wheel   SELECT      SHIFT + wheel  PRESETS
//   K1 K2 K3, FADER     KNOB 1..4 (turned, as the FM-1's encoders: one step per position moved)
//   SHIFT + K1          ALGORITHM
//   MAIN VOLUME         MASTER
//   wheel push          back to the MPC's own app (sample-base)
//   SAMPLE              the SAMPLE BROWSER over the screen: the slots, JIANT/Samples, recording (sample_browser.h;
//                       its buttons and the pads are the browser's while it is open)
#pragma once
#include <array>
#include <cstdint>

namespace jiant_port {

// the FM-1's buttons and encoders, as ports/core/jiant_core.h numbers them
enum Fm1Button : uint8_t { FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC, OCTDN, OCTUP, NoButton = 255 };
enum Fm1Encoder : uint8_t { SELECT, ALGO, PRESET, K1, K2, K3, K4, NoEncoder = 255 };

// what the surface asks of the device: whole masks for keys and buttons (as the panel reads them), steps for an
// encoder, the master level
struct Command {
    enum Kind : uint8_t { None, Keys, Buttons, Encoder, Master, Midi, Record } kind = None;   // (Record: the SAMPLE
                             // BROWSER's SAMPLE REC, sample_browser.h: encoder the slot, value the source)
    uint32_t value = 0;      // Keys, Buttons: the mask; Master 0..1023; Midi: status | d1 << 8 | d2 << 16
    uint8_t encoder = 0;
    int32_t steps = 0;
};

constexpr unsigned kKeys = 27, kPads = 16, kWindows = 4;
constexpr std::array<uint8_t, kWindows> kWindowStart {0, 4, 8, 11};

// the MPC's buttons (sample-base surface/controls.h SurfaceButton order) -> the FM-1's
enum MpcButton : uint8_t {
    Sample, Seq, PadFx, KnobFx, Shift, PadBank, Erase, NoteRepeat, TopLeft, TopMiddle, TopRight,
    Chop, Mute, Loop, SixteenLevels, SampleSelect, TapTempo, Minus, Plus, SampleRecord, SeqRecord, Stop, PlayB,
    WheelPush, MpcButtonCount
};
constexpr std::array<uint8_t, MpcButtonCount> kButtonMap {
    NoButton, SEQ, FX, LFO, NoButton, NoButton, NoButton, NoButton, HOME, EDIT, GLO,
    ENV, NoButton, ARP, SCL, SAVE, NoButton, OCTDN, OCTUP, REC, REC, NoButton, PLAY,
    NoButton,
};
enum MpcControl : uint8_t { MainVolume, Knob1, Knob2, Knob3, Fader, Wheel, MpcControlCount };

class PanelMap {
public:
    // a pad down or up: the keys' mask
    Command pad(unsigned pad, bool down)
    {
        if (pad >= kPads)
            return {};
        unsigned key = kWindowStart[window_] + pad;
        if (down) {
            padKey_[pad] = uint8_t(key);
            keys_ |= 1u << key;
        } else if (padKey_[pad] != 0xFF) {
            keys_ &= ~(1u << padKey_[pad]);
            padKey_[pad] = 0xFF;
        }
        return {Command::Keys, keys_};
    }

    // a button down or up; `playing` for STOP (it presses PLAY only while the sequencer runs). A port's own button
    // (SHIFT, PAD BANK) returns None.
    Command button(unsigned b, bool down, bool playing)
    {
        if (b >= MpcButtonCount)
            return {};
        if (b == Shift) {
            shift_ = down;
            return {};
        }
        if (b == PadBank) {
            if (down)
                window_ = (window_ + (shift_ ? kWindows - 1 : 1)) % kWindows;
            return {};
        }
        uint8_t fm = kButtonMap[b];
        if (b == Stop) {
            if (down && !playing)
                return {};
            if (!down && !(held_ & (1u << Stop)))
                return {};
            fm = PLAY;
        }
        if (fm == NoButton)
            return {};
        held_ = down ? held_ | 1u << b : held_ & ~(1u << b);
        buttons_ = 0;
        for (unsigned m = 0; m < MpcButtonCount; ++m)
            if (held_ & (1u << m))
                buttons_ |= 1u << (m == Stop ? uint8_t(PLAY) : kButtonMap[m]);
        return {Command::Buttons, buttons_};
    }

    // an absolute control moved (`syncing`: its position at startup, a reference only) or the wheel turned
    Command control(unsigned c, uint8_t value, int delta, bool syncing)
    {
        if (c == Wheel)
            return {Command::Encoder, 0, uint8_t(shift_ ? PRESET : SELECT), delta};
        if (c == MainVolume)
            return {Command::Master, (uint32_t(value) * 1023u + 63u) / 127u};
        if (c >= MpcControlCount)
            return {};
        int last = position_[c];
        position_[c] = int16_t(value);
        if (syncing || last < 0 || value == last)
            return {};
        static constexpr uint8_t knob[] = {NoEncoder, K1, K2, K3, K4};
        uint8_t enc = (c == Knob1 && shift_) ? uint8_t(ALGO) : knob[c];
        return {Command::Encoder, 0, enc, int32_t(value) - last};
    }

    unsigned window() const { return window_; }
    bool shift() const { return shift_; }
    // the key a pad plays in the window shown now
    unsigned padKey(unsigned pad) const { return kWindowStart[window_] + pad; }
    // an FM-1 button's light, on the MPC button that stands for it (the first one mapped)
    static int mpcButtonFor(unsigned fm)
    {
        for (unsigned m = 0; m < MpcButtonCount; ++m)
            if (kButtonMap[m] == fm)
                return int(m);
        return -1;
    }

private:
    uint32_t keys_ = 0, buttons_ = 0, held_ = 0;
    unsigned window_ = 0;
    bool shift_ = false;
    std::array<uint8_t, kPads> padKey_ = [] { std::array<uint8_t, kPads> a {}; a.fill(0xFF); return a; }();
    std::array<int16_t, MpcControlCount> position_ {-1, -1, -1, -1, -1, -1};
};

}  // namespace jiant_port
