// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM as a VST3 / AU / AUv3 / LV2 instrument and a standalone app (JUCE): the whole firmware (ports/core) inside,
// its screen and the FM-1's panel in the editor (JiantEditor), MIDI in, the host's transport and tempo as a MIDI clock
// (Sync HOST), the music and the sample slots saved in the host's session. The firmware renders at 44.1 kHz in its
// own blocks; any other rate is resampled (the audio input too, for JIANT's recorder).
//
// Threads: the core belongs to the audio thread. The editor (message thread) sends the panel through a lock-free queue
// and reads back the screen (a seqlock) and the lights (atomics). What needs the core from the message thread (a state
// save) runs as a job: the audio thread runs it at the end of a block; with no audio running the message thread takes
// the core itself (busy_). Samples: a file is read and made into a sample object on the message thread, handed to the
// audio thread by pointer (slotIn_), and what the slots let go of is freed by jiant_service on the service timer (the
// message thread), which also reads the slots for the browser and for the state. Each instance claims its own copy of
// the core (ports/core/jiant_copies.h), up to JIANT_COPIES in a process; one more stays silent and says so.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

struct jiant_api;

class JiantProcessor : public juce::AudioProcessor
{
public:
    JiantProcessor();
    ~JiantProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JIANT FM"; }
    bool acceptsMidi() const override { return true; }
    // the standalone app sends MIDI out (its MIDI output, chosen in its settings): the web editor's replies (jiant_sysex);
    // the plugins none (their buses as before: JucePlugin_ProducesMidiOutput 0)
    bool producesMidi() const override { return wrapperType == wrapperType_Standalone; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "JIANT"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    // ---- the editor's side (message thread) ----
    struct Command {
        enum Kind : uint8_t { None, Buttons, Keys, Encoder, Midi, Sysex } kind = None;
        uint32_t value = 0; uint8_t enc = 0; int32_t steps = 0;
        uint8_t sx[14] = {}; uint8_t n = 0;              // Sysex: the editor protocol's request, cmd and args
    };
    void post(const Command& c);                        // the panel into the device
    // a request of the web editor's protocol (web/EDITOR_PROTOCOL.md: cmd, args) into the device, its reply dropped:
    // queued, the audio thread's (or, no audio running, the service timer's)
    void postRequest(std::initializer_list<uint8_t> cmdArgs);
    // .. and one answered: the reply's cmd and args (empty: none). Blocks for up to a block (withCore)
    std::vector<uint8_t> request(std::initializer_list<uint8_t> cmdArgs);
    bool owner() const { return owner_; }
    int copy() const { return copy_; }                   // this instance's JIANT, 0..copies()-1 (-1: none free)
    static int copies();
    // the 240 x 240 frame (RGB565, the panel's byte order) into out when it changed since `seen`; true if copied
    bool copyScreen(uint16_t* out, uint32_t& seen) const;
    uint32_t litButtons() const { return ledButtons_.load(std::memory_order_relaxed); }
    uint32_t litKeys() const { return ledKeys_.load(std::memory_order_relaxed); }
    uint32_t dimButtons() const { return dimButtons_.load(std::memory_order_relaxed); }
    uint32_t dimKeys() const { return dimKeys_.load(std::memory_order_relaxed); }
    uint32_t cpuPercent() const { return cpu_.load(std::memory_order_relaxed); }
    uint32_t bpm() const { return bpm_.load(std::memory_order_relaxed); }
    bool playing() const { return playing_.load(std::memory_order_relaxed); }
    // the core, from the message thread: f runs with the core to itself (on the audio thread at the end of a block, or
    // here when no audio runs); false: no core (a silent instance). Blocks for up to a block's time
    bool withCore(const std::function<void(const jiant_api&)>& f);

    // ---- samples: JIANT's 16 slots (message thread) ----
    static constexpr int kSlots = 16;
    struct Slot { uint32_t frames = 0, channels = 0; juce::String name, file; };
    // a file (WAV, AIFF, FLAC, MP3, OGG; what the format manager reads) into slot (0..15): "" done, else why not.
    // The editor's drop and its browser call this, as the tests do
    juce::String loadSample(int slot, const juce::File& file);
    // PCM into a slot (any rate: resampled), as a file's is
    juce::String loadSample(int slot, const float* l, const float* r, int frames, double rate, const juce::String& name,
                            const juce::String& file = {});
    void clearSample(int slot);
    const Slot& slot(int k) const { return slots_[size_t(k)]; }
    uint32_t slotsChanged() const { return slotsSeq_; }   // counts the changes the browser shows
    juce::AudioFormatManager& formats() { return formats_; }
    // the recorder: 0 idle, 1 armed, 2 recording; its slot and frames so far
    int recState(int* slot = nullptr, int* frames = nullptr) const;
    bool hasInput() const { return getTotalNumInputChannels() > 0; }
    void service();                                     // the service timer's work (the tests call it too)

    // ---- the device as the editor protocol reads it (the native panels) ----
    struct Info {
        bool ok = false;
        int nEngines = 0, pCount = 0, gCount = 0, pE0 = 0, nTrk = 0;
        juce::String version;
        juce::StringArray engines;                       // by engine byte ("-": retired)
    };
    const Info& info() const { return info_; }
    static constexpr int kTracks = 6, kMaxP = 128;
    struct Link {                                        // every ~100 ms: TRACK and each track's TRACK_DUMP
        uint32_t seq = 0;                                // 0: not read yet
        int selected = 0, nTrk = 0;
        struct Track { int engine = 0, preset = 0, level = 0, mute = 0, armed = 0; std::array<int16_t, kMaxP> p {}; } t[kTracks];
        std::array<int, 2 * kTracks + 1> sent {};        // (the host's values as sent when this was read)
    };
    Link link() const;                                   // a copy of the last
    // the host's parameters past Volume and Sync: each track's level and filter cutoff, the master DJ filter (ids
    // frozen by the protocol: P_LEVEL 0, P_FCUT, G_DJF 17), so automation recorded once plays back the same
    static constexpr int kCutId = 113, kDjfId = 17;

    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    static BusesProperties buses();
    void applyParams();
    void runCommands();                                  // (the core's thread) the editor's queue
    void readLink();                                     // (the core's thread) link_ from the device
    void syncFromDevice();                               // (message thread) the host's parameters from the device
    // (the core's thread) one request in, its reply (cmd, args) out; what else the device had queued is dropped
    static size_t ask(const jiant_api& c, const uint8_t* cmdArgs, size_t n, uint8_t* reply, size_t cap);
    int hostClock(int frames);                           // Sync HOST: the clock this block needs, into clock_
    void render(float* l, float* r, int n);              // n frames at the host's rate
    static float hermite(const float* y, float x);
    void publish();
    void handOff();                                      // (the core's thread) the slots' objects in, the clears
    bool handOffPending() const;
    bool idleCore();                                     // no audio running: the core taken here (busy_ 2)
    void refreshSlots();                                 // (service) slots_ from the core
    void writeSamples(juce::MemoryOutputStream& out);
    void readSamples(juce::MemoryInputStream& in);

    const jiant_api* core_ = nullptr;                    // this instance's JIANT (0: all taken, silent)
    bool owner_ = false;
    int copy_ = -1;
    std::atomic<int> busy_ {0};                          // 1 the audio thread is in the core, 2 the message thread
    std::atomic<int64_t> lastBlockMs_ {0};               // when processBlock last ran (ms)

    // editor -> audio: one producer, one consumer
    static constexpr uint32_t kQueue = 512;
    std::array<Command, kQueue> queue_ {};
    std::atomic<uint32_t> qWrite_ {0}, qRead_ {0};
    std::array<uint8_t, 1024> sysexOut_ {};               // (the editor's replies, one at a time into the MidiBuffer)

    // audio -> editor
    std::atomic<uint32_t> screenSeq_ {0};
    std::array<uint16_t, 240 * 240> screen_ {};
    uint32_t screenDraws_ = ~0u;
    std::atomic<uint32_t> ledButtons_ {0}, ledKeys_ {0}, dimButtons_ {0}, dimKeys_ {0}, cpu_ {0}, bpm_ {120};
    std::atomic<bool> playing_ {false};
    std::atomic<int> recState_ {0}, recSlot_ {0}, recFrames_ {0};

    // a job for the core (withCore)
    std::mutex jobMutex_;                                // one job at a time
    std::atomic<const std::function<void(const jiant_api&)>*> job_ {nullptr};
    std::atomic<bool> jobDone_ {false};

    // state
    std::vector<uint8_t> snapshot_;

    // samples
    juce::AudioFormatManager formats_;
    std::array<std::atomic<void*>, kSlots> slotIn_ {};   // jiant_sample* made, for the audio thread to put
    std::atomic<uint32_t> clearMask_ {0};
    std::mutex serviceMutex_;                            // jiant_service and the slots' reads (any thread but audio)
    std::array<Slot, kSlots> slots_ {};
    std::array<juce::String, kSlots> files_ {};          // the file each slot was loaded from (the browser shows it)
    uint32_t slotsSeq_ = 0;
    struct Blob { uint64_t key = 0; juce::MemoryBlock flac; };
    std::array<Blob, kSlots> blobs_ {};                  // each slot's audio as last saved (FLAC), by its PCM's hash
    struct Service : juce::Timer { JiantProcessor& p; explicit Service(JiantProcessor& q) : p(q) {} void timerCallback() override { p.service(); } };
    std::unique_ptr<Service> service_;

    // parameters as last applied (audio thread)
    int lastMaster_ = -1, lastSync_ = -1;
    std::array<std::atomic<int>, 2 * kTracks + 1> sent_ {};   // the host's values as last sent (INT_MIN: none yet)
    int cutMin_ = 0, cutMax_ = 127;
    std::vector<juce::RangedAudioParameter*> hostParams_;     // T1..T6 level, T1..T6 cutoff, DJ filter
    int linkFrames_ = 0;
    uint32_t idleTicks_ = 0;
    Info info_;
    mutable juce::SpinLock linkLock_;
    Link link_;
    bool hostPlaying_ = false;
    double lastPpq_ = 0.0;
    struct ClockEvent { int offset; uint8_t status; };
    static constexpr int kMaxClock = 512;
    std::array<ClockEvent, kMaxClock + 2> clock_ {};

    // resampling (the host's rate is not 44.1 kHz)
    static constexpr int kBuf = 64;
    bool resample_ = false;
    double step_ = 1.0, frac_ = 0.0;
    float histL_[4] = {}, histR_[4] = {}, bufL_[kBuf] = {}, bufR_[kBuf] = {};
    int bufN_ = 0, bufAt_ = 0;
    // the host's input (its rate) for the core (44.1 kHz): this block's, then what the core has not taken yet
    std::vector<float> inL_, inR_, fifoL_, fifoR_;
    int fifoN_ = 0;
    double inPos_ = 0.0;
    float coreInL_[kBuf] = {}, coreInR_[kBuf] = {};
    bool input_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JiantProcessor)
};
