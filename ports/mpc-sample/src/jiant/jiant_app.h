// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM as the Akai MPC Sample's whole instrument, on sample-base's runtime (keremimo/sample-base, AGPL-3.0):
// the firmware's core (ports/core) plays at the MPC's 44.1 kHz in 128-frame blocks, the FM-1's very block, its
// 240 x 240 screen in the middle of the MPC's 320 x 240, the surface as the FM-1's panel (panel_map.h), MIDI from
// the DIN and USB ports, and the device's flash kept on the card (JIANT/flash.img).
//
// Threads (sample-base's): the core belongs to the audio thread. The MIDI thread turns the surface and the MIDI
// ports into commands through a lock-free queue; the audio thread hands back the screen (a seqlock), the lights
// (atomics) and a copy of the flash for the service thread to write to the card.
//
// The sample slots (ports/core, the SAMPLE and GRAIN engines, DRUM-X's samples): JIANT/Samples on the card, slots.json
// the slot -> file map, loaded at start. The SAMPLE BROWSER (sample_browser.h) over the screen: the MIDI thread asks,
// the service thread reads WAVs into sample objects (resampled there) and hands them to the audio thread through one
// atomic pointer a slot; it writes a slot JIANT changed (a recording, DESTROY) as slotNN.wav once it rests a second.
#pragma once
#include "runtime/app.h"
#include "jiant/panel_map.h"
#include "jiant/sample_browser.h"
#include "jiant/sample_files.h"
#include "jiant_core.h"
#include <mutex>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

class JiantApp : public App {
public:
    // data: the folder of the device's flash (empty: the card's JIANT folder, found at start and on the service
    // thread). seed: this session's dice (0: from the clock).
    explicit JiantApp(std::string data = {}, uint32_t seed = 0);
    ~JiantApp() override;

    void render(const AudioBlock& input, AudioBlock& output) override;   // audio
    void surface(const SurfaceEvent& event) override;                    // MIDI
    void midi(const MidiPortInfo& port, const MidiPacket& packet) override;
    void midi_lost() override;
    void lights(SurfaceLights& lights, Clock::time_point now) override;  // MIDI
    void draw(juce::Graphics& g) override;                               // screen
    void service(Hardware& hardware) override;                           // service
    std::string status() const override;
    std::string command(const std::string& action, const std::string& argument, Hardware& hardware) override;
    std::string command_names() const override { return "jiant-save"; }

    // for the tools (--self-test, --preview): the core driven straight, no runtime
    void post(const jiant_port::Command& c);
    void paint(juce::Graphics& g);                 // what draw() paints, without the session
    static bool load_flash(const std::string& file, std::vector<uint8_t>& image);
    // the samples' share of the service thread (service() calls it; the tools too: flush writes a changed slot at
    // once, without its second of rest)
    void samples_service(Hardware* hardware, bool flush = false);
    std::string samples_folder() const;

private:
    void apply(const jiant_port::Command& c);      // audio thread
    void samples_audio(const AudioBlock& input);   // audio thread: slots in and out, the input, the snapshot
    void audition_mix(AudioBlock& output);         // audio thread
    void browser_action(jiant_port::SampleBrowser::Action a);   // MIDI thread
    void paint_browser(juce::Graphics& g);         // screen thread
    void load_slots_at_start();                    // constructor
    void publish_screen();                         // audio thread
    void copy_flash_step();                        // audio thread
    bool save_flash(const std::string& folder);    // service thread
    std::string data_folder(Hardware* hardware);   // service thread

    // MIDI thread -> audio thread: one producer, one consumer
    static constexpr unsigned kQueue = 1024;
    std::array<jiant_port::Command, kQueue> queue_ {};
    std::atomic<uint32_t> qWrite_ {0}, qRead_ {0};
    jiant_port::PanelMap panel_;                   // MIDI thread
    std::atomic<bool> playing_ {false};            // audio -> MIDI (STOP)
    std::atomic<unsigned> window_ {0};             // MIDI -> screen
    std::atomic<bool> shift_ {false};

    // audio -> screen: the 240 x 240 frame under a sequence count (odd while it is written)
    std::atomic<uint32_t> screenSeq_ {0};
    std::array<uint16_t, 240 * 240> screenShared_ {};
    std::array<uint16_t, 240 * 240> screenLocal_ {};   // screen thread's copy
    uint32_t screenDraws_ = ~0u, blocksSinceScreen_ = 0;

    // audio -> MIDI: the lights
    std::atomic<uint32_t> ledButtons_ {0}, ledKeys_ {0};

    // audio -> service: the flash, copied 64 KiB a block once the firmware stopped writing for a while
    std::vector<uint8_t> flashCopy_;
    uint32_t flashWritesSeen_ = 0, flashQuietBlocks_ = 0, flashCopyAt_ = 0, flashCopyWrites_ = 0;
    bool flashCopying_ = false;
    std::atomic<bool> flashReady_ {false};         // a whole copy waits for the service thread
    std::atomic<uint32_t> flashSaves_ {0};
    std::string folder_;                           // service thread (and the constructor)

    // audio -> screen, status: the real load on the MPC's CPU (the firmware's meter), the most voices sounding, and
    // whether anything gave way (voices faded by the guard, the governor's lighter sound)
    std::atomic<uint32_t> cpu_ {0}, cpuPeak_ {0}, voicesMost_ {0}, shed_ {0};
    std::atomic<bool> governed_ {false};

    // the samples. MIDI -> service: what the browser asked (one producer, one consumer)
    struct SampleReq { enum Kind : uint8_t { Load, Clear, Audition, Source } kind; uint8_t slot; uint32_t file; };
    std::array<SampleReq, 64> sreq_ {};
    std::atomic<uint32_t> sreqW_ {0}, sreqR_ {0};
    jiant_port::SampleBrowser browser_;            // MIDI thread
    std::atomic<bool> browserOpen_ {false};
    std::atomic<unsigned> browserSlot_ {0}, browserFile_ {0}, browserSource_ {0};
    // service -> audio: a slot's new object, slots to clear, the gens written to the card, an audition to play
    std::array<std::atomic<jiant_sample*>, jiant_port::kSlots> slotIn_ {};
    std::atomic<uint32_t> clearMask_ {0}, savedMask_ {0};
    std::array<std::atomic<uint32_t>, jiant_port::kSlots> savedGen_ {};
    std::atomic<jiant_sample*> auditionIn_ {nullptr}, auditionOut_ {nullptr};
    std::atomic<bool> auditionStop_ {false};
    jiant_sample* audition_ = nullptr;             // audio thread: playing it
    uint32_t auditionAt_ = 0;
    // audio -> service, screen, MIDI: each slot's frames, channels, gen and changed (every 16 blocks), the recorder
    std::array<std::atomic<uint32_t>, jiant_port::kSlots> slotFrames_ {}, slotGen_ {}, slotCh_ {};
    std::atomic<uint32_t> slotChanged_ {0}, recState_ {0}, recSlot_ {0}, recFrames_ {0};
    std::atomic<bool> auditioning_ {false};
    uint32_t snapBlocks_ = 0;
    std::vector<float> inL_, inR_;                 // audio: the input as floats
    // service thread: the map, the folder's files (the screen reads them: the mutex, never on the audio thread)
    jiant_port::SlotMap map_;
    std::vector<std::string> files_;
    mutable std::mutex filesMutex_;
    std::array<std::string, jiant_port::kSlots> slotNames_;   // (under filesMutex_)
    std::atomic<uint32_t> fileCount_ {0}, slotSaves_ {0}, slotLoads_ {0};
    std::array<uint32_t, jiant_port::kSlots> changeSeenGen_ {};
    std::array<std::chrono::steady_clock::time_point, jiant_port::kSlots> changeSeenAt_ {};
    std::chrono::steady_clock::time_point scanAt_ {};
    unsigned sourceRouted_ = 99;

    float level_ = 0.0f;                           // audio: for the meters
    std::atomic<uint8_t> meter_ {0};
    juce::Typeface::Ptr font_;
};
