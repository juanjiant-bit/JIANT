// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "JiantProcessor.h"
#include "JiantEditor.h"
#include "jiant_copies.h"
#include <chrono>
#include <climits>
#include <cstring>
#include <thread>

namespace {
// each copy's music as it booted (message thread, while the copy is claimed): a copy given back by one instance and
// claimed by a new one starts that one from there, not from the last one's music
std::vector<uint8_t> g_fresh[JIANT_COPIES];
constexpr uint32_t kStateMagic = 0x504E544Au;            // "JNTP": the plugin's state, then the core's
constexpr uint32_t kSampleMagic = 0x4C504D53u;           // "SMPL": then the sample slots, each slot's audio whole
constexpr int kFifo = 1 << 15;                           // (the input's room at the host's rate)
int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
// a slot's audio as its key: frames, channels, name and every sample (FNV-1a 64)
uint64_t pcmKey(const int16_t* l, const int16_t* r, uint32_t frames, uint32_t ch, const char* name)
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
    mix(frames);
    mix(ch);
    for (const char* c = name; c && *c; ++c) mix(uint8_t(*c));
    for (uint32_t i = 0; i < frames; ++i) {
        mix(uint16_t(l[i]));
        if (ch > 1) mix(uint16_t(r[i]));
    }
    return h | 1u;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout JiantProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"volume", 1}, "Volume",
                                                      juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 68.0f));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {"sync", 1}, "Sync",
                                                       juce::StringArray {"Internal", "Host"}, 1));
    // automation: fixed per track, the same parameter whatever the track plays (each track's LEVEL and the filter's CUT
    // on its sum), and the master DJ filter. They follow the device: what JIANT's own panel turns moves them too
    for (int t = 0; t < kTracks; ++t)
        l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID {"t" + juce::String(t + 1) + "level", 1},
                                                        "T" + juce::String(t + 1) + " Level", 0, 127, 100));
    for (int t = 0; t < kTracks; ++t)
        l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID {"t" + juce::String(t + 1) + "cut", 1},
                                                        "T" + juce::String(t + 1) + " Cutoff", 0, 127, 127));
    l.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID {"djf", 1}, "DJ Filter", -64, 63, 0));
    return l;
}

// stereo out; an input for JIANT's recorder (REC from the input, GRAIN LIVE; a DAW's sidechain), off until the host
// turns it on: not on AU / AUv3, where an instrument with an input is an effect to some hosts
juce::AudioProcessor::BusesProperties JiantProcessor::buses()
{
    auto b = BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
    auto t = juce::PluginHostType::getPluginLoadedAs();
    if (t != wrapperType_AudioUnit && t != wrapperType_AudioUnitv3)
        b = b.withInput("Input", juce::AudioChannelSet::stereo(), false);
    return b;
}

JiantProcessor::JiantProcessor()
    : AudioProcessor(buses()),
      params(*this, nullptr, "JIANT", layout())
{
    formats_.registerBasicFormats();
    for (int t = 0; t < kTracks; ++t)
        hostParams_.push_back(params.getParameter("t" + juce::String(t + 1) + "level"));
    for (int t = 0; t < kTracks; ++t)
        hostParams_.push_back(params.getParameter("t" + juce::String(t + 1) + "cut"));
    hostParams_.push_back(params.getParameter("djf"));
    for (auto& v : sent_)
        v.store(INT_MIN);
    uint32_t k = 0;
    core_ = jiant_copy_claim(&k);
    owner_ = core_ != nullptr;
    if (owner_) {
        copy_ = int(k);
        snapshot_.resize(core_->jiant_state_max());
        auto& fresh = g_fresh[k];
        if (!core_->jiant_booted()) {
            core_->jiant_boot(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u, 1);
            fresh.resize(snapshot_.size());
            fresh.resize(core_->jiant_state_save(fresh.data(), fresh.size()));
        } else {                                         // a copy another instance had: its music back to the start
            if (core_->jiant_playing())
                core_->jiant_play(0);
            if (!fresh.empty())
                core_->jiant_state_load(fresh.data(), fresh.size());
            for (uint32_t s = 0; s < JIANT_SLOTS; ++s)
                core_->jiant_slot_clear(s);
            core_->jiant_service();
        }
        publish();
        refreshSlots();
        // the device as the protocol describes it (INFO), once: the engines' names, the parameters' count
        uint8_t rep[1024];
        const uint8_t infoReq[] = {1};
        size_t n = ask(*core_, infoReq, 1, rep, sizeof rep);
        if (n > 8) {
            size_t at = 1;
            auto str = [&] { juce::String v; while (at < n && rep[at]) v += juce::juce_wchar(rep[at++]); ++at; return v; };
            info_.version = str();
            if (at + 5 <= n) {
                info_.nEngines = rep[at]; info_.pCount = rep[at + 1]; info_.gCount = rep[at + 2]; info_.pE0 = rep[at + 4];
                at += 5;
                for (int e = 0; e < info_.nEngines && at < n; ++e) info_.engines.add(str());
                info_.nTrk = at < n ? rep[at] : 6;
                info_.ok = info_.engines.size() == info_.nEngines && info_.pCount <= kMaxP;
            }
        }
        readLink();
        service_ = std::make_unique<Service>(*this);
        service_->startTimer(100);
    }
}

JiantProcessor::~JiantProcessor()
{
    service_.reset();
    if (owner_) {
        for (auto& in : slotIn_)
            if (void* o = in.exchange(nullptr))
                core_->jiant_sample_free(static_cast<jiant_sample*>(o));
        if (core_->jiant_playing())
            core_->jiant_play(0);
        core_->jiant_buttons(0);
        core_->jiant_keys(0);
        jiant_copy_release(core_);
    }
}

bool JiantProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    const auto& in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void JiantProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    step_ = double(JIANT_RATE) / (sampleRate > 0 ? sampleRate : double(JIANT_RATE));
    resample_ = std::abs(step_ - 1.0) > 1e-9;
    frac_ = 0.0;
    std::fill(std::begin(histL_), std::end(histL_), 0.0f);
    std::fill(std::begin(histR_), std::end(histR_), 0.0f);
    bufN_ = bufAt_ = 0;
    const size_t room = size_t(std::max(samplesPerBlock, 0)) + size_t(kFifo);
    inL_.assign(room, 0.0f);
    inR_.assign(room, 0.0f);
    fifoL_.assign(room + size_t(kFifo), 0.0f);
    fifoR_.assign(room + size_t(kFifo), 0.0f);
    fifoN_ = int(std::ceil(kBuf / step_)) + 4;          // (the core renders ahead: its first input is silence)
    inPos_ = 0.0;
    hostPlaying_ = false;
    lastMaster_ = lastSync_ = -1;
}

// ------------------------------------------------------------------------------------------------ the editor's side

void JiantProcessor::post(const Command& c)
{
    uint32_t w = qWrite_.load(std::memory_order_relaxed);
    if (w - qRead_.load(std::memory_order_acquire) >= kQueue)
        return;
    queue_[w % kQueue] = c;
    qWrite_.store(w + 1, std::memory_order_release);
}

bool JiantProcessor::copyScreen(uint16_t* out, uint32_t& seen) const
{
    for (int tries = 0; tries < 4; ++tries) {
        uint32_t s = screenSeq_.load(std::memory_order_acquire);
        if (s & 1u)
            continue;
        if (s == seen)
            return false;
        std::memcpy(out, screen_.data(), sizeof screen_);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (screenSeq_.load(std::memory_order_relaxed) == s) {
            seen = s;
            return true;
        }
    }
    return false;
}

// -------------------------------------------------------------------------------------------- the audio thread

void JiantProcessor::publish()
{
    uint32_t draws = core_->jiant_screen_draws();
    if (draws != screenDraws_) {
        screenDraws_ = draws;
        uint32_t s = screenSeq_.load(std::memory_order_relaxed);
        screenSeq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        std::memcpy(screen_.data(), core_->jiant_screen(), sizeof screen_);
        screenSeq_.store(s + 2, std::memory_order_release);
    }
    ledButtons_.store(core_->jiant_led_buttons(), std::memory_order_relaxed);
    ledKeys_.store(core_->jiant_led_keys(), std::memory_order_relaxed);
    cpu_.store(core_->jiant_cpu_percent(), std::memory_order_relaxed);
    bpm_.store(core_->jiant_bpm(), std::memory_order_relaxed);
    playing_.store(core_->jiant_playing() != 0, std::memory_order_relaxed);
}

size_t JiantProcessor::ask(const jiant_api& c, const uint8_t* cmdArgs, size_t n, uint8_t* reply, size_t cap)
{
    uint8_t req[32], out[1024];
    if (n + 6 > sizeof req)
        return 0;
    while (c.jiant_sysex_out(out, sizeof out)) {}       // (what was queued before: not this request's)
    req[0] = 0xF0; req[1] = 0x7D; req[2] = 0x46; req[3] = 0x4C;
    std::memcpy(req + 4, cmdArgs, n);
    req[4 + n] = 0xF7;
    if (c.jiant_sysex(req, n + 5))
        return 0;
    size_t got = 0;
    for (size_t m; (m = c.jiant_sysex_out(out, sizeof out)) != 0;) {
        if (m > sizeof out) {                            // (too long for here: dropped)
            std::vector<uint8_t> big(m);
            c.jiant_sysex_out(big.data(), m);
            continue;
        }
        if (!got && m >= 6 && out[4] == cmdArgs[0] && reply) {
            got = std::min(m - 5, cap);
            std::memcpy(reply, out + 4, got);
        }
    }
    return got;
}

void JiantProcessor::postRequest(std::initializer_list<uint8_t> cmdArgs)
{
    Command c;
    c.kind = Command::Sysex;
    for (uint8_t b : cmdArgs)
        if (c.n < sizeof c.sx)
            c.sx[c.n++] = b;
    post(c);
    if (nowMs() - lastBlockMs_.load(std::memory_order_relaxed) >= 100)
        withCore([](const jiant_api&) {});              // (no audio: in now)
}

std::vector<uint8_t> JiantProcessor::request(std::initializer_list<uint8_t> cmdArgs)
{
    std::vector<uint8_t> req(cmdArgs), rep(1024);
    size_t n = 0;
    if (!withCore([&](const jiant_api& c) { n = ask(c, req.data(), req.size(), rep.data(), rep.size()); }))
        return {};
    rep.resize(n);
    return rep;
}

// (the core's thread) the editor's queue: the panel, notes, the protocol's requests (their replies dropped)
void JiantProcessor::runCommands()
{
    uint32_t r = qRead_.load(std::memory_order_relaxed), w = qWrite_.load(std::memory_order_acquire);
    for (; r != w; ++r) {
        const Command& c = queue_[r % kQueue];
        switch (c.kind) {
        case Command::Buttons: core_->jiant_buttons(c.value); break;
        case Command::Keys: core_->jiant_keys(c.value); break;
        case Command::Encoder: core_->jiant_encoder(static_cast<enum jiant_encoder>(c.enc), c.steps); break;
        case Command::Midi: core_->jiant_midi(uint8_t(c.value), uint8_t(c.value >> 8), uint8_t(c.value >> 16)); break;
        case Command::Sysex: ask(*core_, c.sx, c.n, nullptr, 0); break;
        default: break;
        }
    }
    qRead_.store(r, std::memory_order_release);
}

static int v14(const uint8_t* p) { return int(p[0] | p[1] << 7) - 8192; }

// (the core's thread) the tracks as the device has them: TRACK, then each track's TRACK_DUMP, the DJ filter
void JiantProcessor::readLink()
{
    uint8_t rep[512];
    Link l;
    const uint8_t q[] = {27};
    size_t n = ask(*core_, q, 1, rep, sizeof rep);
    if (n < 3)
        return;
    l.selected = rep[1];
    l.nTrk = std::min<int>(rep[2], kTracks);
    for (int t = 0; t < l.nTrk && 3 + t * 6 + 5 < int(n); ++t) {
        const uint8_t* e = rep + 3 + t * 6;
        l.t[t].engine = e[0]; l.t[t].preset = e[1]; l.t[t].level = v14(e + 2); l.t[t].mute = e[4]; l.t[t].armed = e[5];
    }
    for (int t = 0; t < l.nTrk; ++t) {
        const uint8_t d[] = {29, uint8_t(t)};
        size_t m = ask(*core_, d, 2, rep, sizeof rep);
        for (int i = 0; 4 + 2 * i + 1 < int(m) && i < kMaxP; ++i)
            l.t[t].p[size_t(i)] = int16_t(v14(rep + 4 + 2 * i));
    }
    const uint8_t gq[] = {2, 1, uint8_t(kDjfId)};
    if (ask(*core_, gq, 3, rep, sizeof rep) >= 5)
        l.t[0].p[size_t(kMaxP - 1)] = int16_t(v14(rep + 3));    // (the DJ filter, a global, kept past the tracks')
    for (size_t i = 0; i < sent_.size(); ++i)
        l.sent[i] = sent_[i].load(std::memory_order_relaxed);
    const juce::SpinLock::ScopedTryLockType lock(linkLock_);
    if (lock.isLocked()) {
        l.seq = link_.seq + 1;
        link_ = l;
    }
}

JiantProcessor::Link JiantProcessor::link() const
{
    const juce::SpinLock::ScopedLockType lock(linkLock_);
    return link_;
}

// (message thread) what the device changed (its own panel, a preset, a project) into the host's parameters
void JiantProcessor::syncFromDevice()
{
    Link l = link();
    if (!l.seq || l.nTrk < 1)
        return;
    for (int i = 0; i < int(hostParams_.size()); ++i) {
        const int t = i % kTracks;
        int dev = i < kTracks ? l.t[t].level : i < 2 * kTracks ? l.t[t].p[size_t(kCutId)] : l.t[0].p[size_t(kMaxP - 1)];
        if (i < 2 * kTracks && t >= l.nTrk)
            continue;
        int was = l.sent[size_t(i)];
        if (dev == was)
            continue;
        if (!sent_[size_t(i)].compare_exchange_strong(was, dev))   // (the host moved it since: the host's, newer)
            continue;
        auto* p = hostParams_[size_t(i)];
        float v = p->convertTo0to1(float(dev));
        if (std::abs(p->getValue() - v) > 1e-6f)
            p->setValueNotifyingHost(v);
    }
}

void JiantProcessor::applyParams()
{
    int master = int(std::lround(params.getRawParameterValue("volume")->load() * 10.23f));
    if (master != lastMaster_)
        core_->jiant_master(uint32_t(lastMaster_ = master));
    int sync = int(params.getRawParameterValue("sync")->load());
    if (sync != lastSync_) {
        if (lastSync_ == 1 && hostPlaying_)
            core_->jiant_midi(0xFC, 0, 0);
        core_->jiant_clock_external(sync == 1);
        hostPlaying_ = false;
        lastSync_ = sync;
    }
    // the tracks' levels and cutoffs, the DJ filter: what the host moved (once the device's values are known)
    for (int i = 0; i < int(hostParams_.size()); ++i) {
        const int was = sent_[size_t(i)].load(std::memory_order_relaxed);
        if (was == INT_MIN)
            continue;
        const int v = int(std::lround(hostParams_[size_t(i)]->convertFrom0to1(hostParams_[size_t(i)]->getValue())));
        if (v == was)
            continue;
        const int x = v + 8192;
        const uint8_t lo = uint8_t(x & 127), hi = uint8_t(x >> 7);
        if (i < 2 * kTracks) {
            const uint8_t req[] = {31, uint8_t(i % kTracks), uint8_t(i < kTracks ? 0 : kCutId), lo, hi};
            ask(*core_, req, sizeof req, nullptr, 0);
        } else {
            const uint8_t req[] = {3, 1, uint8_t(kDjfId), lo, hi};
            ask(*core_, req, sizeof req, nullptr, 0);
        }
        sent_[size_t(i)].store(v, std::memory_order_relaxed);
    }
}

int JiantProcessor::hostClock(int frames)
{
    int n = 0;
    auto* ph = getPlayHead();
    auto pos = ph ? ph->getPosition() : std::nullopt;
    bool playing = pos && pos->getIsPlaying() && pos->getPpqPosition() && pos->getBpm() && *pos->getBpm() > 0;
    if (!playing) {
        if (hostPlaying_)
            clock_[size_t(n++)] = {0, 0xFC};
        hostPlaying_ = false;
        return n;
    }
    double perFrame = *pos->getBpm() / 60.0 / getSampleRate();
    double start = *pos->getPpqPosition(), end = start + perFrame * frames;
    if (!hostPlaying_ || start + 1e-6 < lastPpq_) {
        if (hostPlaying_)
            clock_[size_t(n++)] = {0, 0xFC};
        clock_[size_t(n++)] = {0, 0xFA};
        hostPlaying_ = true;
    }
    lastPpq_ = end;
    for (double p = std::ceil(start * 24.0 - 1e-9); p / 24.0 < end && n < kMaxClock; p += 1.0) {
        int off = int((p / 24.0 - start) / perFrame);
        clock_[size_t(n++)] = {juce::jlimit(0, frames - 1, off), 0xF8};
    }
    return n;
}

float JiantProcessor::hermite(const float* y, float x)
{
    float c1 = 0.5f * (y[2] - y[0]);
    float c2 = y[0] - 2.5f * y[1] + 2.0f * y[2] - 0.5f * y[3];
    float c3 = 0.5f * (y[3] - y[0]) + 1.5f * (y[1] - y[2]);
    return ((c3 * x + c2) * x + c1) * x + y[1];
}

void JiantProcessor::render(float* l, float* r, int n)
{
    if (!resample_) {
        core_->jiant_render(l, r, uint32_t(n));
        return;
    }
    for (int i = 0; i < n; ++i) {
        while (frac_ >= 1.0) {
            if (bufAt_ >= bufN_) {
                if (input_) {                            // this much of the input, at 44.1 kHz
                    const double per = 1.0 / step_;
                    for (int k = 0; k < kBuf; ++k, inPos_ += per) {
                        int i = int(inPos_);
                        float f = float(inPos_ - i);
                        bool in = i + 1 < fifoN_;
                        coreInL_[k] = in ? fifoL_[size_t(i)] + f * (fifoL_[size_t(i + 1)] - fifoL_[size_t(i)]) : 0.0f;
                        coreInR_[k] = in ? fifoR_[size_t(i)] + f * (fifoR_[size_t(i + 1)] - fifoR_[size_t(i)]) : 0.0f;
                    }
                    core_->jiant_input(coreInL_, coreInR_, kBuf);
                }
                core_->jiant_render(bufL_, bufR_, kBuf);
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
        l[i] = hermite(histL_, float(frac_));
        if (r)
            r[i] = hermite(histR_, float(frac_));
        frac_ += step_;
    }
}

void JiantProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int frames = buffer.getNumSamples();
    // the host's input (if it gives one), kept before the buffer becomes the output
    const int inCh = getTotalNumInputChannels();
    input_ = owner_ && inCh > 0 && frames <= int(inL_.size());
    if (input_) {
        std::memcpy(inL_.data(), buffer.getReadPointer(0), size_t(frames) * sizeof(float));
        std::memcpy(inR_.data(), buffer.getReadPointer(inCh > 1 ? 1 : 0), size_t(frames) * sizeof(float));
        if (resample_) {                                 // onto the input's queue (its rate), what was taken dropped
            int drop = std::max(0, std::min(int(inPos_) - 1, fifoN_));
            if (drop) {
                std::memmove(fifoL_.data(), fifoL_.data() + drop, size_t(fifoN_ - drop) * sizeof(float));
                std::memmove(fifoR_.data(), fifoR_.data() + drop, size_t(fifoN_ - drop) * sizeof(float));
                fifoN_ -= drop;
                inPos_ -= drop;
            }
            if (fifoN_ + frames <= int(fifoL_.size())) {
                std::memcpy(fifoL_.data() + fifoN_, inL_.data(), size_t(frames) * sizeof(float));
                std::memcpy(fifoR_.data() + fifoN_, inR_.data(), size_t(frames) * sizeof(float));
                fifoN_ += frames;
            }
        }
    }
    buffer.clear();
    int idle = 0;
    if (!owner_ || !busy_.compare_exchange_strong(idle, 1, std::memory_order_acquire))
        return;
    lastBlockMs_.store(nowMs(), std::memory_order_relaxed);

    handOff();
    runCommands();
    applyParams();

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    int nclk = lastSync_ == 1 ? hostClock(frames) : 0;
    auto it = midi.cbegin();
    int at = 0, ck = 0;
    while (at < frames) {
        while (it != midi.cend() && (*it).samplePosition <= at) {
            const auto m = (*it).getMessage();
            const uint8_t* d = m.getRawData();
            if (m.getRawDataSize() >= 1 && d[0] >= 0x80 && d[0] < 0xF0)
                core_->jiant_midi(d[0], m.getRawDataSize() > 1 ? d[1] : 0, m.getRawDataSize() > 2 ? d[2] : 0);
            else if (m.isSysEx())                        // the web editor (web/editor.html), as the FM-1 takes it
                core_->jiant_sysex(d, size_t(m.getRawDataSize()));
            ++it;
        }
        while (ck < nclk && clock_[size_t(ck)].offset <= at)
            core_->jiant_midi(clock_[size_t(ck++)].status, 0, 0);
        int next = frames;
        if (it != midi.cend())
            next = std::min(next, (*it).samplePosition);
        if (ck < nclk)
            next = std::min(next, clock_[size_t(ck)].offset);
        if (next <= at)
            next = at + 1;
        if (input_ && !resample_)                        // the input as it came, before the frames it goes with
            core_->jiant_input(inL_.data() + at, inR_.data() + at, uint32_t(next - at));
        render(outL + at, outR ? outR + at : nullptr, next - at);   // (mono: the left side)
        at = next;
    }

    // the editor's replies and its live pushes: out of the standalone app's MIDI output; a plugin has none (dropped)
    core_->jiant_editor_poll();
    bool out = producesMidi();
    if (out)
        midi.clear();
    for (size_t n; (n = core_->jiant_sysex_out(sysexOut_.data(), sysexOut_.size())) != 0;) {
        if (n > sysexOut_.size()) {                     // (never: editor.c's replies are at most 600 bytes) skipped
            std::vector<uint8_t> big(n);
            core_->jiant_sysex_out(big.data(), n);
            continue;
        }
        if (out)
            midi.addEvent(sysexOut_.data(), int(n), juce::jmax(0, frames - 1));
    }

    publish();
    if ((linkFrames_ += frames) >= int(getSampleRate() * 0.1)) {   // the tracks for the editor, ~10 a second
        linkFrames_ = 0;
        readLink();
    }
    if (auto* j = job_.exchange(nullptr, std::memory_order_acq_rel)) {
        (*j)(*core_);
        jobDone_.store(true, std::memory_order_release);
    }
    busy_.store(0, std::memory_order_release);
}

// ----------------------------------------------------------------------------------------- the core from elsewhere

bool JiantProcessor::idleCore()
{
    for (int t = 0; t < 2000; ++t) {
        int idle = 0;
        if (busy_.compare_exchange_weak(idle, 2, std::memory_order_acquire))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

bool JiantProcessor::withCore(const std::function<void(const jiant_api&)>& f)
{
    if (!owner_)
        return false;
    std::lock_guard<std::mutex> one(jobMutex_);
    // audio running (a block in the last 100 ms): its thread runs f at the end of its next block
    if (nowMs() - lastBlockMs_.load(std::memory_order_relaxed) < 100) {
        jobDone_.store(false, std::memory_order_relaxed);
        job_.store(&f, std::memory_order_release);
        for (int t = 0; t < 300 && !jobDone_.load(std::memory_order_acquire); ++t)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const std::function<void(const jiant_api&)>* mine = &f;
        if (jobDone_.load(std::memory_order_acquire) || !job_.compare_exchange_strong(mine, nullptr)) {
            while (!jobDone_.load(std::memory_order_acquire))   // (taken just now: it finishes)
                std::this_thread::yield();
            return true;
        }
    }
    if (!idleCore())                                     // no audio: the core is ours
        return false;
    handOff();
    runCommands();
    f(*core_);
    publish();
    busy_.store(0, std::memory_order_release);
    return true;
}

// ---------------------------------------------------------------------------------------------------- samples

void JiantProcessor::handOff()
{
    for (int k = 0; k < kSlots; ++k)
        if (slotIn_[size_t(k)].load(std::memory_order_relaxed))
            if (void* o = slotIn_[size_t(k)].exchange(nullptr, std::memory_order_acq_rel))
                core_->jiant_slot_put(uint32_t(k), static_cast<jiant_sample*>(o));
    if (uint32_t m = clearMask_.exchange(0, std::memory_order_acq_rel))
        for (int k = 0; k < kSlots; ++k)
            if ((m >> k) & 1u)
                core_->jiant_slot_clear(uint32_t(k));
    uint32_t slot = 0, fr = 0;
    recState_.store(core_->jiant_rec_state(&slot, &fr), std::memory_order_relaxed);
    recSlot_.store(int(slot), std::memory_order_relaxed);
    recFrames_.store(int(fr), std::memory_order_relaxed);
}

int JiantProcessor::recState(int* slot, int* frames) const
{
    if (slot)
        *slot = recSlot_.load(std::memory_order_relaxed);
    if (frames)
        *frames = recFrames_.load(std::memory_order_relaxed);
    return recState_.load(std::memory_order_relaxed);
}

bool JiantProcessor::handOffPending() const
{
    if (clearMask_.load(std::memory_order_acquire))
        return true;
    for (auto& a : slotIn_)
        if (a.load(std::memory_order_acquire))
            return true;
    return false;
}

juce::String JiantProcessor::loadSample(int k, const float* l, const float* r, int frames, double rate,
                                        const juce::String& name, const juce::String& file)
{
    if (!owner_)
        return "this instance has no JIANT (all " + juce::String(copies()) + " are playing)";
    if (k < 0 || k >= kSlots)
        return "no such slot";
    if (!l || frames <= 0 || rate < 1000.0)
        return "no audio";
    juce::String n = name.toUpperCase().retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_.+#").substring(0, 31);
    jiant_sample* o = core_->jiant_sample_make(l, r, uint32_t(frames), uint32_t(std::lround(rate)), n.toRawUTF8());
    if (!o)
        return "out of memory";
    clearMask_.fetch_and(~(1u << k), std::memory_order_acq_rel);
    if (void* was = slotIn_[size_t(k)].exchange(o, std::memory_order_acq_rel))   // (one not taken yet: never played)
        core_->jiant_sample_free(static_cast<jiant_sample*>(was));
    files_[size_t(k)] = file;
    service();
    return {};
}

juce::String JiantProcessor::loadSample(int k, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> rd(formats_.createReaderFor(file));
    if (!rd)
        return "not an audio file JIANT reads: " + file.getFileName();
    if (rd->sampleRate < 1000.0 || rd->lengthInSamples <= 0 || rd->numChannels < 1)
        return "no audio in " + file.getFileName();
    // up to 60 s at 44.1 kHz (the slot's most), read at the file's rate
    const int64_t most = int64_t(std::ceil(double(JIANT_SLOT_MAX_FRAMES) * rd->sampleRate / JIANT_RATE)) + 64;
    const int frames = int(std::min<int64_t>(rd->lengthInSamples, most));
    const int ch = rd->numChannels > 1 ? 2 : 1;
    juce::AudioBuffer<float> buf(ch, frames);
    if (!rd->read(&buf, 0, frames, 0, true, ch > 1))
        return "could not read " + file.getFileName();
    return loadSample(k, buf.getReadPointer(0), ch > 1 ? buf.getReadPointer(1) : nullptr, frames, rd->sampleRate,
                      file.getFileNameWithoutExtension(), file.getFullPathName());
}

void JiantProcessor::clearSample(int k)
{
    if (!owner_ || k < 0 || k >= kSlots)
        return;
    if (void* was = slotIn_[size_t(k)].exchange(nullptr, std::memory_order_acq_rel))
        core_->jiant_sample_free(static_cast<jiant_sample*>(was));
    clearMask_.fetch_or(1u << k, std::memory_order_acq_rel);
    files_[size_t(k)] = {};
    service();
}

// the service timer (message thread) and after each load: with no audio running the hand-offs go in from here; then
// what the slots let go of is freed, the recorder's room kept, the slots read for the browser
void JiantProcessor::service()
{
    if (!owner_)
        return;
    if (nowMs() - lastBlockMs_.load(std::memory_order_relaxed) >= 100 &&
        (handOffPending() || qRead_.load() != qWrite_.load() || ++idleTicks_ % 3 == 0))
        withCore([this](const jiant_api&) { readLink(); });   // (no audio: the queue, the hand-offs, the tracks from here)
    syncFromDevice();
    std::lock_guard<std::mutex> g(serviceMutex_);
    core_->jiant_service();
    refreshSlots();
}

// (serviceMutex_ held) the slots as the core has them
void JiantProcessor::refreshSlots()
{
    for (int k = 0; k < kSlots; ++k) {
        jiant_slot_info_t in;
        core_->jiant_slot_info(uint32_t(k), &in);
        Slot s;
        s.frames = in.frames;
        s.channels = in.channels;
        s.name = juce::String(in.name);
        s.file = in.frames && !in.changed ? files_[size_t(k)] : juce::String();   // (recorded in JIANT: no file has it)
        auto& was = slots_[size_t(k)];
        if (s.frames != was.frames || s.channels != was.channels || s.name != was.name || s.file != was.file) {
            was = s;
            ++slotsSeq_;
        }
    }
}

// the slots into the state: "SMPL", 1, the count, then each slot that holds audio: its number, name, file, channels,
// frames, and its audio as FLAC (16 bit, 44.1 kHz: lossless, about half the PCM's size). A slot unchanged since the
// last save reuses that save's FLAC
void JiantProcessor::writeSamples(juce::MemoryOutputStream& out)
{
    std::lock_guard<std::mutex> g(serviceMutex_);
    struct One { int k; juce::String name; uint32_t ch, frames; const juce::MemoryBlock* flac; };
    std::vector<One> all;
    juce::FlacAudioFormat flac;
    for (int k = 0; k < kSlots; ++k) {
        const jiant_sample* o = core_->jiant_slot_object(uint32_t(k));   // (whole until this thread's next service)
        uint32_t frames = 0, ch = 0;
        const int16_t* l = core_->jiant_sample_data(o, 0, &frames, &ch);
        const int16_t* r = core_->jiant_sample_data(o, 1, nullptr, nullptr);
        jiant_slot_info_t in;
        core_->jiant_slot_info(uint32_t(k), &in);
        if (!l || !frames)
            continue;
        const uint64_t key = pcmKey(l, r, frames, ch, in.name);
        Blob& b = blobs_[size_t(k)];
        if (b.key != key) {
            b.flac.reset();
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::MemoryOutputStream>(b.flac, false);
            auto wr = flac.createWriterFor(os, juce::AudioFormatWriterOptions {}
                                                   .withSampleRate(double(JIANT_RATE))
                                                   .withNumChannels(int(ch))
                                                   .withBitsPerSample(16)
                                                   .withQualityOptionIndex(5));
            bool ok = wr != nullptr;
            std::vector<int> a(4096), c(4096);
            for (uint32_t at = 0; ok && at < frames; at += 4096) {
                const int n = int(std::min<uint32_t>(4096, frames - at));
                for (int i = 0; i < n; ++i) {
                    a[size_t(i)] = int(l[at + uint32_t(i)]) * 65536;      // (JUCE's integers: 32 bit, left justified)
                    c[size_t(i)] = int(r[at + uint32_t(i)]) * 65536;
                }
                const int* chans[3] = {a.data(), ch > 1 ? c.data() : nullptr, nullptr};
                ok = wr->write(chans, n);
            }
            wr.reset();                                  // (the stream finished)
            b.key = ok ? key : 0;
            if (!ok)
                continue;
        }
        all.push_back({k, juce::String(in.name), ch, frames, &b.flac});
    }
    out.writeInt(int(kSampleMagic));
    out.writeInt(1);
    out.writeInt(int(all.size()));
    for (auto& one : all) {
        out.writeInt(one.k);
        out.writeString(one.name);
        out.writeString(files_[size_t(one.k)]);
        out.writeInt(int(one.ch));
        out.writeInt(int(one.frames));
        out.writeInt(int(one.flac->getSize()));
        out.write(one.flac->getData(), one.flac->getSize());
    }
}

void JiantProcessor::readSamples(juce::MemoryInputStream& in)
{
    uint32_t have = 0;
    juce::FlacAudioFormat flac;
    if (in.getNumBytesRemaining() >= 12 && uint32_t(in.readInt()) == kSampleMagic && in.readInt() == 1) {
        int count = in.readInt();
        for (int i = 0; i < count && i < kSlots && in.getNumBytesRemaining() > 0; ++i) {
            int k = in.readInt();
            juce::String name = in.readString(), file = in.readString();
            int ch = in.readInt(), frames = in.readInt(), size = in.readInt();
            if (k < 0 || k >= kSlots || ch < 1 || ch > 2 || frames <= 0 || frames > int(JIANT_SLOT_MAX_FRAMES) ||
                size <= 0 || size > in.getNumBytesRemaining())
                break;
            juce::MemoryBlock blob;
            in.readIntoMemoryBlock(blob, size);
            std::unique_ptr<juce::AudioFormatReader> rd(flac.createReaderFor(new juce::MemoryInputStream(blob, false), true));
            if (!rd)
                continue;
            juce::AudioBuffer<float> buf(ch, frames);
            buf.clear();
            if (!rd->read(&buf, 0, frames, 0, true, ch > 1))
                continue;
            // (int16 through float and back: the same samples, made at 44.1 kHz)
            if (loadSample(k, buf.getReadPointer(0), ch > 1 ? buf.getReadPointer(1) : nullptr, frames, JIANT_RATE, name,
                           file).isEmpty())
                have |= 1u << k;
        }
    }
    for (int k = 0; k < kSlots; ++k)                     // the session's slots only: the others emptied
        if (!((have >> k) & 1u) && (slots_[size_t(k)].frames || slotIn_[size_t(k)].load()))
            clearSample(k);
}

// ---------------------------------------------------------------------------------------------------- the state

void JiantProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream out(dest, false);
    out.writeInt(int(kStateMagic));
    auto xml = params.copyState().createXml();
    juce::String text = xml ? xml->toString() : juce::String();
    out.writeString(text);
    size_t len = 0;
    if (owner_)
        withCore([&](const jiant_api& c) { len = c.jiant_state_save(snapshot_.data(), snapshot_.size()); });
    if (len > snapshot_.size())
        len = 0;
    out.writeInt(int(len));
    out.write(snapshot_.data(), len);
    if (owner_)
        writeSamples(out);
}

void JiantProcessor::setStateInformation(const void* data, int size)
{
    juce::MemoryInputStream in(data, size_t(size), false);
    if (size < 8 || uint32_t(in.readInt()) != kStateMagic)
        return;
    if (auto xml = juce::parseXML(in.readString()))
        if (xml->hasTagName(params.state.getType()))
            params.replaceState(juce::ValueTree::fromXml(*xml));
    int len = in.readInt();
    if (!owner_ || len < 0 || len > in.getNumBytesRemaining())
        return;
    if (len > 0) {
        std::vector<uint8_t> blob(static_cast<size_t>(len));
        if (in.read(blob.data(), len) != len)
            return;
        withCore([&](const jiant_api& c) { c.jiant_state_load(blob.data(), blob.size()); });
    }
    readSamples(in);
}

int JiantProcessor::copies() { return JIANT_COPIES; }

juce::AudioProcessorEditor* JiantProcessor::createEditor() { return new JiantEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JiantProcessor(); }
