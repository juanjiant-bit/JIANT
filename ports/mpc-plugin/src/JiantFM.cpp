// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM as an instrument plugin for the Akai MPC standalones (MPC OS 3.10.1+, External Plugin Support), for the
// MPC Sample through hosts that load MPC plugins, and for desktop VST2 hosts. Built on inMusic's MPC Plugin SDK
// framework (MIT; framework/mpc_plugin.h, fetched by ports/mpc-plugin/build.py) and JIANT's core (ports/core): the
// whole firmware runs inside, its six tracks, engines, sequencer, songs, generator and FX, sample for sample as the
// FM-1 plays them.
//
// What the plugin shows is the MPC's parameter screen (plugins there have no screen of their own): the volume, the
// sync, PLAY and the song generator. MIDI plays it: channels 1..4 play tracks 1..4 (GLO > MIDI IN ROUTE CH1-4).
// Sync HOST: the sequencer follows the host's transport and tempo (a MIDI clock made from them); INTERNAL: its own
// tempo, PLAY starts it. The music (the live section, the songs, the user presets) is saved in the host's project.
//
// Threads: every call into the core happens on the audio thread, or while the audio thread is held off (busy_): a
// state save asks the audio thread for a snapshot and waits; when no audio runs it takes the core itself.
// One device per process (the firmware's state is global): a second instance stays silent.

#include "mpc_plugin.h"
#include "jiant_core.h"

#include <chrono>
#include <thread>

namespace {

std::atomic<void*> g_owner{nullptr};          // the instance that plays the device

const char* const kForms[] = {"Loop", "Parts", "Song"};

}  // namespace

class JiantFM : public mpc::Plugin
{
public:
    enum Params { kVolume, kSync, kPlay, kGenre, kSeed, kEnergy, kForm, kGenerate, kNumParams };

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "JIANT FM";
        i.vendor = "Juan Jiant";
        i.uniqueId = MPC_FOURCC('J', 'n', 'F', 'M');   // == "uid" in plugin.json
        i.version = 1000;
        i.isInstrument = true;
        i.numInputs = 0;
        i.numOutputs = 2;
        i.tailSamples = 0;
        return i;
    }

    JiantFM() : mpc::Plugin(makeInfo())
    {
        void* none = nullptr;
        owner_ = g_owner.compare_exchange_strong(none, this);
        if (owner_ && !jiant_booted())
            jiant_boot((uint32_t)std::chrono::steady_clock::now().time_since_epoch().count() | 1u, 1);

        std::vector<std::string> genres = {"Random"};
        for (uint32_t g = 0; g < jiant_genre_count(); ++g)
            genres.push_back(jiant_genre_name(g));
        genres.push_back("Explore");

        addParam(mpc::floatParam("volume", "Volume", 0.0f, 100.0f, 68.0f, "%", 0));
        addParam(mpc::choiceParam("sync", "Sync", {"Internal", "Host"}, 1));
        addParam(mpc::boolParam("play", "Play", false));
        addParam(mpc::choiceParam("genre", "Genre", genres, 0));
        addParam(mpc::intParam("seed", "Seed", 0, 9999, 1));
        addParam(mpc::floatParam("energy", "Energy", 0.0f, 100.0f, 60.0f, "%", 0));
        addParam(mpc::choiceParam("form", "Form", {kForms[0], kForms[1], kForms[2]}, 0));
        addParam(mpc::boolParam("generate", "Generate", false));

        if (owner_)
            snapshot_.resize(jiant_state_max());
    }

    ~JiantFM() override
    {
        if (owner_) {
            if (jiant_playing())
                jiant_play(0);
            g_owner.store(nullptr);
        }
    }

    void prepare(double sampleRate, int maxBlockSize) override
    {
        (void)maxBlockSize;
        step_ = (double)JIANT_RATE / (sampleRate > 0 ? sampleRate : (double)JIANT_RATE);
        resample_ = std::fabs(step_ - 1.0) > 1e-9;
        frac_ = 0.0;
        for (int k = 0; k < 4; ++k) histL_[k] = histR_[k] = 0.0f;
        bufN_ = bufAt_ = 0;
    }

    // On resume and after a state load (the audio thread, or no audio running): the staged music in, the
    // parameters taken as they are now (a load writes them: no generator run, no PLAY edge from it).
    void reset() override
    {
        if (!owner_)
            return;
        if (pendingLen_.load(std::memory_order_acquire)) {
            jiant_state_load(pending_.data(), pendingLen_.load());
            pendingLen_.store(0, std::memory_order_release);
        }
        hostPlaying_ = false;
        applyParams(true);
    }

    void saveExtraState(std::vector<uint8_t>& out) override
    {
        if (!owner_)
            return;
        snapDone_.store(false);
        snapReq_.store(true, std::memory_order_release);
        for (int t = 0; t < 300 && !snapDone_.load(std::memory_order_acquire); ++t)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!snapDone_.load(std::memory_order_acquire)) {      // no audio running: the core is ours to read
            snapReq_.store(false);
            int idle = 0;
            while (!busy_.compare_exchange_weak(idle, 2)) {
                idle = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            snapLen_ = jiant_state_save(snapshot_.data(), snapshot_.size());
            busy_.store(0, std::memory_order_release);
        }
        if (snapLen_ <= snapshot_.size())
            out.assign(snapshot_.begin(), snapshot_.begin() + (std::ptrdiff_t)snapLen_);
    }

    void loadExtraState(const uint8_t* data, size_t size) override
    {
        if (!owner_ || size == 0)
            return;
        pending_.assign(data, data + size);                    // applied by reset(), before the next block
        pendingLen_.store(size, std::memory_order_release);
    }

    bool formatValue(int index, float plain, std::string& out) override
    {
        if (index == kSync) {
            out = plain >= 0.5f ? "Host" : "Internal";
            return true;
        }
        return false;
    }

    void process(mpc::ProcessContext& ctx) override
    {
        float* outL = ctx.outputs[0];
        float* outR = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;
        int busy = 0;
        if (!owner_ || !busy_.compare_exchange_strong(busy, 1, std::memory_order_acquire))
            return;                                             // (cleared already: silence)

        applyParams(false);
        int nclk = 0;
        if (getInt(kSync) == 1)
            nclk = hostClock(ctx);

        int at = 0, ev = 0, ck = 0;
        while (at < ctx.numFrames) {
            while (ev < ctx.numMidiEvents && ctx.midi[ev].sampleOffset <= at) {
                const mpc::MidiEvent& e = ctx.midi[ev++];
                if (e.data[0] >= 0x80 && e.data[0] < 0xF0)      // channel messages (the clock is made below)
                    jiant_midi(e.data[0], e.data[1], e.data[2]);
            }
            while (ck < nclk && clock_[ck].offset <= at)
                jiant_midi(clock_[ck++].status, 0, 0);
            int next = ctx.numFrames;
            if (ev < ctx.numMidiEvents && ctx.midi[ev].sampleOffset < next)
                next = ctx.midi[ev].sampleOffset;
            if (ck < nclk && clock_[ck].offset < next)
                next = clock_[ck].offset;
            if (next <= at)
                next = at + 1;
            render(outL + at, outR ? outR + at : nullptr, next - at);
            at = next;
        }

        if (snapReq_.exchange(false, std::memory_order_acq_rel)) {
            snapLen_ = jiant_state_save(snapshot_.data(), snapshot_.size());
            snapDone_.store(true, std::memory_order_release);
        }
        busy_.store(0, std::memory_order_release);
    }

private:
    struct ClockEvent { int offset; uint8_t status; };
    static const int kMaxClock = 256;

    // the parameters into the device, each when it changed (or all, taken as they are: sync)
    void applyParams(bool sync)
    {
        int vol = (int)std::lround(get(kVolume) * 10.23f);
        if (sync || vol != lastVol_)
            jiant_master((uint32_t)(lastVol_ = vol));
        int s = getInt(kSync);
        if (sync || s != lastSync_) {
            if (lastSync_ == 1 && hostPlaying_)
                jiant_midi(0xFC, 0, 0);
            jiant_clock_external(s == 1);
            hostPlaying_ = false;
            lastSync_ = s;
        }
        bool play = getBool(kPlay);
        if (play != lastPlay_ && s == 0 && !sync)
            jiant_play(play ? 1 : 0);
        lastPlay_ = play;
        bool gen = getBool(kGenerate);
        if (gen && !lastGen_ && !sync)
            jiant_generate((uint32_t)getInt(kGenre), (uint32_t)getInt(kSeed), (uint32_t)std::lround(get(kEnergy)),
                           (uint32_t)getInt(kForm));
        lastGen_ = gen;
    }

    // Sync HOST: START when the host starts (and again after a jump back), the 24 PPQN clock at the frames where
    // the host's beats put it, STOP when it stops
    int hostClock(const mpc::ProcessContext& ctx)
    {
        const mpc::Transport& t = ctx.transport;
        int n = 0;
        bool playing = t.valid && t.playing && t.ppqValid && t.tempoValid && t.bpm > 0;
        if (!playing) {
            if (hostPlaying_)
                clock_[n++] = {0, 0xFC};
            hostPlaying_ = false;
            return n;
        }
        double perFrame = t.bpm / 60.0 / ctx.sampleRate;       // quarter notes per frame
        double start = t.ppqPos, end = start + perFrame * ctx.numFrames;
        if (!hostPlaying_ || start + 1e-6 < lastPpq_) {
            if (hostPlaying_)
                clock_[n++] = {0, 0xFC};
            clock_[n++] = {0, 0xFA};
            hostPlaying_ = true;
        }
        lastPpq_ = end;
        for (double p = std::ceil(start * 24.0 - 1e-9); p / 24.0 < end && n < kMaxClock; p += 1.0) {
            int off = (int)((p / 24.0 - start) / perFrame);
            clock_[n++] = {off < 0 ? 0 : (off >= ctx.numFrames ? ctx.numFrames - 1 : off), 0xF8};
        }
        return n;
    }

    // n frames at the host's rate: the core's own when it is 44.1 kHz, else resampled (4-point Hermite)
    void render(float* l, float* r, int n)
    {
        if (!resample_) {
            jiant_render(l, r, (uint32_t)n);
            return;
        }
        for (int i = 0; i < n; ++i) {
            while (frac_ >= 1.0) {
                if (bufAt_ >= bufN_) {
                    jiant_render(bufL_, bufR_, kBuf);
                    bufN_ = kBuf;
                    bufAt_ = 0;
                }
                for (int k = 0; k < 3; ++k) {
                    histL_[k] = histL_[k + 1];
                    histR_[k] = histR_[k + 1];
                }
                histL_[3] = bufL_[bufAt_];
                histR_[3] = bufR_[bufAt_++];
                frac_ -= 1.0;
            }
            float x = (float)frac_;
            l[i] = hermite(histL_, x);
            if (r)
                r[i] = hermite(histR_, x);
            frac_ += step_;
        }
    }
    static float hermite(const float* y, float x)               // between y[1] and y[2]
    {
        float c1 = 0.5f * (y[2] - y[0]);
        float c2 = y[0] - 2.5f * y[1] + 2.0f * y[2] - 0.5f * y[3];
        float c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
        return ((c3 * x + c2) * x + c1) * x + y[1];
    }

    bool owner_ = false;
    std::atomic<int> busy_{0};                 // 1 the audio thread is in the core, 2 a state save without audio
    std::atomic<bool> snapReq_{false}, snapDone_{false};
    std::vector<uint8_t> snapshot_;
    size_t snapLen_ = 0;
    std::vector<uint8_t> pending_;
    std::atomic<size_t> pendingLen_{0};

    int lastVol_ = -1, lastSync_ = -1;
    bool lastPlay_ = false, lastGen_ = false;
    bool hostPlaying_ = false;
    double lastPpq_ = 0.0;
    ClockEvent clock_[kMaxClock + 2];

    static const int kBuf = 64;
    bool resample_ = false;
    double step_ = 1.0, frac_ = 0.0;
    float histL_[4] = {}, histR_[4] = {};
    float bufL_[kBuf] = {}, bufR_[kBuf] = {};
    int bufN_ = 0, bufAt_ = 0;
};

MPC_PLUGIN_ENTRY(JiantFM)
