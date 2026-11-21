// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "JiantProcessor.h"
#include "JiantEditor.h"
#include "jiant_core.h"
#include <chrono>
#include <cstring>
#include <thread>

namespace {
std::atomic<void*> g_owner {nullptr};                    // the instance that plays the device
constexpr uint32_t kStateMagic = 0x504E544Au;            // "JNTP": the plugin's state, then the core's
}

juce::AudioProcessorValueTreeState::ParameterLayout JiantProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"volume", 1}, "Volume",
                                                      juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 68.0f));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID {"sync", 1}, "Sync",
                                                       juce::StringArray {"Internal", "Host"}, 1));
    return l;
}

JiantProcessor::JiantProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "JIANT", layout())
{
    void* none = nullptr;
    owner_ = g_owner.compare_exchange_strong(none, this);
    if (owner_) {
        if (!jiant_booted())
            jiant_boot(uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u, 1);
        snapshot_.resize(jiant_state_max());
        publish();
    }
}

JiantProcessor::~JiantProcessor()
{
    if (owner_) {
        if (jiant_playing())
            jiant_play(0);
        jiant_buttons(0);
        jiant_keys(0);
        g_owner.store(nullptr);
    }
}

bool JiantProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void JiantProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    step_ = double(JIANT_RATE) / (sampleRate > 0 ? sampleRate : double(JIANT_RATE));
    resample_ = std::abs(step_ - 1.0) > 1e-9;
    frac_ = 0.0;
    std::fill(std::begin(histL_), std::end(histL_), 0.0f);
    std::fill(std::begin(histR_), std::end(histR_), 0.0f);
    bufN_ = bufAt_ = 0;
    (void)samplesPerBlock;
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
    uint32_t draws = jiant_screen_draws();
    if (draws != screenDraws_) {
        screenDraws_ = draws;
        uint32_t s = screenSeq_.load(std::memory_order_relaxed);
        screenSeq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        std::memcpy(screen_.data(), jiant_screen(), sizeof screen_);
        screenSeq_.store(s + 2, std::memory_order_release);
    }
    ledButtons_.store(jiant_led_buttons(), std::memory_order_relaxed);
    ledKeys_.store(jiant_led_keys(), std::memory_order_relaxed);
    cpu_.store(jiant_cpu_percent(), std::memory_order_relaxed);
}

void JiantProcessor::applyParams()
{
    int master = int(std::lround(params.getRawParameterValue("volume")->load() * 10.23f));
    if (master != lastMaster_)
        jiant_master(uint32_t(lastMaster_ = master));
    int sync = int(params.getRawParameterValue("sync")->load());
    if (sync != lastSync_) {
        if (lastSync_ == 1 && hostPlaying_)
            jiant_midi(0xFC, 0, 0);
        jiant_clock_external(sync == 1);
        hostPlaying_ = false;
        lastSync_ = sync;
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
        jiant_render(l, r, uint32_t(n));
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
        l[i] = hermite(histL_, float(frac_));
        if (r)
            r[i] = hermite(histR_, float(frac_));
        frac_ += step_;
    }
}

void JiantProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    int idle = 0;
    if (!owner_ || !busy_.compare_exchange_strong(idle, 1, std::memory_order_acquire))
        return;

    if (size_t len = pendingLen_.load(std::memory_order_acquire)) {   // a session's music, staged by the host
        jiant_state_load(pending_.data(), len);
        pendingLen_.store(0, std::memory_order_release);
    }
    uint32_t r = qRead_.load(std::memory_order_relaxed), w = qWrite_.load(std::memory_order_acquire);
    for (; r != w; ++r) {
        const Command& c = queue_[r % kQueue];
        switch (c.kind) {
        case Command::Buttons: jiant_buttons(c.value); break;
        case Command::Keys: jiant_keys(c.value); break;
        case Command::Encoder: jiant_encoder(static_cast<enum jiant_encoder>(c.enc), c.steps); break;
        default: break;
        }
    }
    qRead_.store(r, std::memory_order_release);
    applyParams();

    const int frames = buffer.getNumSamples();
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
                jiant_midi(d[0], m.getRawDataSize() > 1 ? d[1] : 0, m.getRawDataSize() > 2 ? d[2] : 0);
            ++it;
        }
        while (ck < nclk && clock_[size_t(ck)].offset <= at)
            jiant_midi(clock_[size_t(ck++)].status, 0, 0);
        int next = frames;
        if (it != midi.cend())
            next = std::min(next, (*it).samplePosition);
        if (ck < nclk)
            next = std::min(next, clock_[size_t(ck)].offset);
        if (next <= at)
            next = at + 1;
        render(outL + at, outR ? outR + at : nullptr, next - at);   // (mono: the left side)
        at = next;
    }

    publish();
    if (snapReq_.exchange(false, std::memory_order_acq_rel)) {
        snapLen_ = jiant_state_save(snapshot_.data(), snapshot_.size());
        snapDone_.store(true, std::memory_order_release);
    }
    busy_.store(0, std::memory_order_release);
}

// ---------------------------------------------------------------------------------------------------- the state

void JiantProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream out(dest, false);
    out.writeInt(int(kStateMagic));
    auto xml = params.copyState().createXml();
    juce::String text = xml ? xml->toString() : juce::String();
    out.writeString(text);
    if (!owner_) {
        out.writeInt(0);
        return;
    }
    snapDone_.store(false);
    snapReq_.store(true, std::memory_order_release);
    for (int t = 0; t < 300 && !snapDone_.load(std::memory_order_acquire); ++t)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (!snapDone_.load(std::memory_order_acquire)) {    // no audio running: the core is ours to read
        snapReq_.store(false);
        int idle = 0;
        while (!busy_.compare_exchange_weak(idle, 2)) {
            idle = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        snapLen_ = jiant_state_save(snapshot_.data(), snapshot_.size());
        busy_.store(0, std::memory_order_release);
    }
    size_t len = snapLen_ <= snapshot_.size() ? snapLen_ : 0;
    out.writeInt(int(len));
    out.write(snapshot_.data(), len);
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
    if (!owner_ || len <= 0 || len > size)
        return;
    std::vector<uint8_t> blob(static_cast<size_t>(len));
    if (in.read(blob.data(), len) != len)
        return;
    pending_ = std::move(blob);                          // in before the next block (processBlock)
    pendingLen_.store(pending_.size(), std::memory_order_release);
}

juce::AudioProcessorEditor* JiantProcessor::createEditor() { return new JiantEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JiantProcessor(); }
