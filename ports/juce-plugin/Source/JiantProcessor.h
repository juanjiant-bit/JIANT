// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM as a VST3 / AU / AUv3 / LV2 instrument and a standalone app (JUCE): the whole firmware (ports/core) inside,
// its screen and the FM-1's panel in the editor (JiantEditor), MIDI in, the host's transport and tempo as a MIDI clock
// (Sync HOST), the music saved in the host's session. The firmware renders at 44.1 kHz in its own blocks; any other
// rate is resampled.
//
// Threads: the core belongs to the audio thread. The editor (message thread) sends the panel through a lock-free queue
// and reads back the screen (a seqlock) and the lights (atomics). A state save asks the audio thread for a snapshot
// and waits; with no audio running it takes the core itself (busy_). One device per process (the firmware's state is
// global): each instance claims its own copy of the core
// (ports/core/jiant_copies.h), up to JIANT_COPIES in a process; one more stays silent and says so.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
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
    struct Command { enum Kind : uint8_t { None, Buttons, Keys, Encoder } kind = None; uint32_t value = 0; uint8_t enc = 0; int32_t steps = 0; };
    void post(const Command& c);                        // the panel into the device
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

    juce::AudioProcessorValueTreeState params;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void applyParams();
    int hostClock(int frames);                           // Sync HOST: the clock this block needs, into clock_
    void render(float* l, float* r, int n);              // n frames at the host's rate
    static float hermite(const float* y, float x);
    void publish();

    const jiant_api* core_ = nullptr;                    // this instance's JIANT (0: all taken, silent)
    bool owner_ = false;
    int copy_ = -1;
    std::atomic<int> busy_ {0};                          // 1 the audio thread is in the core, 2 a save without audio

    // editor -> audio: one producer, one consumer
    static constexpr uint32_t kQueue = 512;
    std::array<Command, kQueue> queue_ {};
    std::atomic<uint32_t> qWrite_ {0}, qRead_ {0};
    std::array<uint8_t, 1024> sysexOut_ {};               // (the editor's replies, one at a time into the MidiBuffer)

    // audio -> editor
    std::atomic<uint32_t> screenSeq_ {0};
    std::array<uint16_t, 240 * 240> screen_ {};
    uint32_t screenDraws_ = ~0u;
    std::atomic<uint32_t> ledButtons_ {0}, ledKeys_ {0}, dimButtons_ {0}, dimKeys_ {0}, cpu_ {0};

    // state
    std::atomic<bool> snapReq_ {false}, snapDone_ {false};
    std::vector<uint8_t> snapshot_;
    size_t snapLen_ = 0;
    std::vector<uint8_t> pending_;
    std::atomic<size_t> pendingLen_ {0};

    // parameters as last applied (audio thread)
    int lastMaster_ = -1, lastSync_ = -1;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JiantProcessor)
};
