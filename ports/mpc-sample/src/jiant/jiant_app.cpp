// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "jiant/jiant_app.h"
#include "drivers/mixer.h"
#include "drivers/platform.h"
#include "runtime/screen.h"
#include "surface/leds.h"
#include "surface/pads.h"
#include "jiant_core.h"
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
using jiant_port::Command;

namespace {
constexpr const char* kFlashFile = "flash.img";
constexpr const char* kNoData = "-";           // a session with no flash file (the tools)
constexpr uint32_t kQuietBlocks = 345;          // ~1 s without a flash write before it is copied out
constexpr size_t kCopyChunk = 64 * 1024;

// the card's folder for JIANT: the first mounted card under /media
std::string card_folder()
{
    std::error_code error;
    if (!fs::is_directory("/media", error))
        return {};
    for (const auto& entry : fs::directory_iterator("/media", error))
        if (entry.is_directory(error) && fs::exists(entry.path() / "JIANT" / kFlashFile, error))
            return (entry.path() / "JIANT").string();
    return {};
}

const char* const kNotes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
std::string key_name(unsigned key)                // key 0 is F3
{
    unsigned midi = 53 + key;
    return std::string(kNotes[midi % 12]) + std::to_string(int(midi / 12) - 1);
}
}  // namespace

JiantApp::JiantApp(std::string data, uint32_t seed) : folder_(std::move(data))
{
    if (folder_.empty())
        folder_ = card_folder();               // (empty still: the service thread finds the card)
    if (folder_ != kNoData && !folder_.empty()) {
        std::vector<uint8_t> image;
        if (load_flash(folder_ + "/" + kFlashFile, image))
            jiant_flash_restore(image.data(), image.size());
    }
    if (!seed)
        seed = uint32_t(std::chrono::steady_clock::now().time_since_epoch().count()) | 1u;
    jiant_boot(seed, 1);
    load_slots_at_start();
    inL_.resize(audio_frames);
    inR_.resize(audio_frames);
    size_t size = 0;
    jiant_flash(&size);
    flashCopy_.resize(size);
    flashWritesSeen_ = flashCopyWrites_ = jiant_flash_writes();
}

JiantApp::~JiantApp()
{
    // the audio thread has stopped: the music as it is into the autosave, then the flash straight from the core
    if (folder_ != kNoData && !folder_.empty())
        jiant_autosave_now();
    if (folder_ != kNoData && !folder_.empty() && jiant_flash_writes() != flashCopyWrites_) {
        size_t size = 0;
        const uint8_t* image = jiant_flash(&size);
        std::memcpy(flashCopy_.data(), image, size);
        save_flash(folder_);
    }
}

bool JiantApp::load_flash(const std::string& file, std::vector<uint8_t>& image)
{
    size_t size = 0;
    jiant_flash(&size);
    FILE* f = std::fopen(file.c_str(), "rb");
    if (!f)
        return false;
    image.resize(size);
    bool ok = std::fread(image.data(), 1, size, f) == size && std::fgetc(f) == EOF;
    std::fclose(f);
    return ok;
}

// --------------------------------------------------------------------------------------------- MIDI thread ---

void JiantApp::post(const Command& c)
{
    if (c.kind == Command::None)
        return;
    uint32_t w = qWrite_.load(std::memory_order_relaxed);
    if (w - qRead_.load(std::memory_order_acquire) >= kQueue)
        return;                                 // full (1024 events in 3 ms: dropped)
    queue_[w % kQueue] = c;
    qWrite_.store(w + 1, std::memory_order_release);
}

void JiantApp::surface(const SurfaceEvent& event)
{
    using A = jiant_port::SampleBrowser::Action;
    if (event.pad >= 0) {
        unsigned kind = event.midi.status & 0xF0;
        if (kind == 0x90 || kind == 0x80) {
            A a = browser_.pad(unsigned(event.pad), kind == 0x90 && event.midi.value);
            if (a != A::None)
                browser_action(a);
            else
                post(panel_.pad(unsigned(event.pad), kind == 0x90 && event.midi.value));
        }
        return;
    }
    const SurfaceInput& in = event.input;
    if (in.kind == SurfaceInput::Kind::Button && !event.syncing) {   // the browser's buttons first (SAMPLE opens it)
        A a = browser_.button(in.index, in.pressed);
        if (a != A::None) {
            browser_action(a);
            return;
        }
    }
    if (in.kind == SurfaceInput::Kind::Control && in.index == jiant_port::Wheel && browser_.open()) {
        browser_action(browser_.wheel(in.delta));
        return;
    }
    if (in.kind == SurfaceInput::Kind::Button)
        post(panel_.button(in.index, in.pressed, playing_.load(std::memory_order_relaxed)));
    else if (in.kind == SurfaceInput::Kind::Control)
        post(panel_.control(in.index, in.value, in.delta, event.syncing));
    window_.store(panel_.window(), std::memory_order_relaxed);
    shift_.store(panel_.shift(), std::memory_order_relaxed);
}

void JiantApp::midi(const MidiPortInfo&, const MidiPacket& packet)
{
    unsigned i = 0;
    while (i < packet.size) {
        uint8_t st = packet.bytes[i];
        if (st == 0xF0) {                       // SysEx: not for JIANT here
            while (i < packet.size && packet.bytes[i] != 0xF7)
                ++i;
            ++i;
            continue;
        }
        if (st >= 0xF8) {                       // realtime: the clock, start, continue, stop
            post({Command::Midi, st});
            ++i;
            continue;
        }
        if (st < 0x80 || st >= 0xF0) {          // a stray data byte or system common
            ++i;
            continue;
        }
        unsigned len = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 2 : 3;
        if (i + len > packet.size)
            break;
        uint32_t d1 = packet.bytes[i + 1] & 0x7F, d2 = len == 3 ? packet.bytes[i + 2] & 0x7F : 0;
        post({Command::Midi, st | d1 << 8 | d2 << 16});
        i += len;
    }
}

void JiantApp::midi_lost()
{
    for (uint32_t ch = 0; ch < 16; ++ch)
        post({Command::Midi, (0xB0 | ch) | 123u << 8});   // all notes off
}

void JiantApp::lights(SurfaceLights& lights, Clock::time_point)
{
    uint32_t keys = ledKeys_.load(std::memory_order_relaxed), buttons = ledButtons_.load(std::memory_order_relaxed);
    uint32_t held = session().held.load(std::memory_order_relaxed);
    bool browsing = browserOpen_.load(std::memory_order_relaxed);
    for (unsigned p = 0; p < jiant_port::kPads; ++p) {
        unsigned key = panel_.padKey(p);
        Rgb c {};
        if (browsing)                           // the browser: the slots (lit: loaded; bright: picked)
            c = p == browserSlot_.load(std::memory_order_relaxed) ? Rgb {127, 60, 0}
                : slotFrames_[p].load(std::memory_order_relaxed) ? Rgb {30, 10, 0} : Rgb {2, 1, 0};
        else if (held & (1u << p))
            c = {100, 100, 100};
        else if ((keys >> key) & 1u)
            c = {127, 48, 0};                   // JIANT's orange
        else
            c = {4, 2, 0};
        lights.pad(p, c);
    }
    for (unsigned b = 0; b < surface_bank_lights; ++b)
        lights.bank(b, b == panel_.window());
    for (unsigned fm = 0; fm < 14; ++fm) {
        int m = jiant_port::PanelMap::mpcButtonFor(fm);
        bool on = (buttons >> fm) & 1u;
        if (fm == jiant_port::PLAY)
            on = on || ((buttons >> 14) & 1u);  // PLAY's green
        if (m >= 0)
            lights.button(SurfaceButton(m), on ? ButtonLight::Bright : ButtonLight::Dim);
    }
    lights.button(SurfaceButton::Stop, playing_.load(std::memory_order_relaxed) ? ButtonLight::Bright : ButtonLight::Dim);
    lights.button(SurfaceButton::Sample, browsing ? ButtonLight::Bright : ButtonLight::Dim);
    if (browsing) {
        lights.button(SurfaceButton::TopLeft, ButtonLight::Bright);
        lights.button(SurfaceButton::TopMiddle, auditioning_.load(std::memory_order_relaxed) ? ButtonLight::Bright : ButtonLight::Dim);
        lights.button(SurfaceButton::TopRight, ButtonLight::Bright);
        lights.button(SurfaceButton::SampleRecord, recState_.load(std::memory_order_relaxed) ? ButtonLight::Bright : ButtonLight::Dim);
    }
    uint8_t m = meter_.load(std::memory_order_relaxed);
    lights.meter(0, m);
    lights.meter(1, m);
}

// -------------------------------------------------------------------------------------------- audio thread ---

void JiantApp::apply(const Command& c)
{
    switch (c.kind) {
    case Command::Keys: jiant_keys(c.value); break;
    case Command::Buttons: jiant_buttons(c.value); break;
    case Command::Encoder: jiant_encoder(static_cast<enum jiant_encoder>(c.encoder), c.steps); break;
    case Command::Master: jiant_master(c.value); break;
    case Command::Midi: jiant_midi(uint8_t(c.value), uint8_t(c.value >> 8), uint8_t(c.value >> 16)); break;
    case Command::Record:                       // SAMPLE REC: start (the source into the slot, until again) or stop
        if (jiant_rec_state(nullptr, nullptr))
            jiant_rec_stop();
        else
            jiant_rec_start(c.encoder, c.value == unsigned(jiant_port::RecSource::Resample) ? JIANT_REC_MASTER : JIANT_REC_INPUT, 0);
        break;
    default: break;
    }
}

void JiantApp::render(const AudioBlock& input, AudioBlock& output)
{
    uint32_t r = qRead_.load(std::memory_order_relaxed), w = qWrite_.load(std::memory_order_acquire);
    for (; r != w; ++r)
        apply(queue_[r % kQueue]);
    qRead_.store(r, std::memory_order_release);
    samples_audio(input);

    float left[audio_frames], right[audio_frames];
    jiant_render(left, right, audio_frames);
    float peak = 0.0f;
    for (unsigned i = 0; i < audio_frames; ++i) {
        float l = std::clamp(left[i], -1.0f, 1.0f), rr = std::clamp(right[i], -1.0f, 1.0f);
        peak = std::max(peak, std::max(std::fabs(l), std::fabs(rr)));
        output[2 * i] = int32_t(std::lrint(double(l) * 2147483647.0));
        output[2 * i + 1] = int32_t(std::lrint(double(rr) * 2147483647.0));
    }
    level_ = std::max(peak, level_ * 0.97f);
    static constexpr float steps[5] = {0.01f, 0.05f, 0.15f, 0.4f, 0.9f};
    uint8_t mask = 0;
    for (unsigned k = 0; k < 5; ++k)
        if (level_ >= steps[k])
            mask |= uint8_t(1u << k);
    meter_.store(mask, std::memory_order_relaxed);
    audition_mix(output);

    playing_.store(jiant_playing() != 0, std::memory_order_relaxed);
    cpu_.store(jiant_cpu_percent(), std::memory_order_relaxed);
    cpuPeak_.store(jiant_cpu_peak_percent(), std::memory_order_relaxed);
    shed_.store(jiant_voices_shed(), std::memory_order_relaxed);
    governed_.store(jiant_load_governed() != 0, std::memory_order_relaxed);
    if (jiant_voices_sounding() > voicesMost_.load(std::memory_order_relaxed))
        voicesMost_.store(jiant_voices_sounding(), std::memory_order_relaxed);
    ledButtons_.store(jiant_led_buttons(), std::memory_order_relaxed);
    ledKeys_.store(jiant_led_keys(), std::memory_order_relaxed);
    publish_screen();
    copy_flash_step();
}

// the screen, at most every 6 blocks (17 ms) and only when the firmware drew
void JiantApp::publish_screen()
{
    if (++blocksSinceScreen_ < 6)
        return;
    uint32_t draws = jiant_screen_draws();
    if (draws == screenDraws_)
        return;
    blocksSinceScreen_ = 0;
    screenDraws_ = draws;
    uint32_t s = screenSeq_.load(std::memory_order_relaxed);
    screenSeq_.store(s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    std::memcpy(screenShared_.data(), jiant_screen(), sizeof screenShared_);
    screenSeq_.store(s + 2, std::memory_order_release);
}

// the flash out for the card: once a second has passed without a write, 64 KiB a block (16 blocks, 46 ms); a write
// in between starts it over. The service thread writes the whole copy and clears flashReady_.
void JiantApp::copy_flash_step()
{
    uint32_t writes = jiant_flash_writes();
    if (writes != flashWritesSeen_) {
        flashWritesSeen_ = writes;
        flashQuietBlocks_ = 0;
        flashCopying_ = false;
        return;
    }
    if (!flashCopying_) {
        if (writes == flashCopyWrites_ || flashReady_.load(std::memory_order_acquire) || ++flashQuietBlocks_ < kQuietBlocks)
            return;
        flashCopying_ = true;
        flashCopyAt_ = 0;
    }
    size_t size = 0;
    const uint8_t* image = jiant_flash(&size);
    size_t n = std::min(kCopyChunk, size - flashCopyAt_);
    std::memcpy(flashCopy_.data() + flashCopyAt_, image + flashCopyAt_, n);
    flashCopyAt_ += uint32_t(n);
    if (flashCopyAt_ >= size) {
        flashCopying_ = false;
        flashCopyWrites_ = writes;
        flashReady_.store(true, std::memory_order_release);
    }
}

// ------------------------------------------------------------------------------------------ service thread ---

std::string JiantApp::data_folder(Hardware* hardware)
{
    if (!folder_.empty() || !hardware)
        return folder_;
    for (const auto& space : hardware->platform.storage())
        if (space.path.rfind("/media/", 0) == 0 && !space.read_only)
            return folder_ = space.path + "/JIANT";
    return {};
}

bool JiantApp::save_flash(const std::string& folder)
{
    std::error_code error;
    fs::create_directories(folder, error);
    std::string file = folder + "/" + kFlashFile, temp = file + ".new";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (!f)
        return false;
    bool ok = std::fwrite(flashCopy_.data(), 1, flashCopy_.size(), f) == flashCopy_.size() && std::fflush(f) == 0 &&
              fsync(fileno(f)) == 0;
    ok = std::fclose(f) == 0 && ok;
    if (ok)
        fs::rename(temp, file, error);
    ok = ok && !error;
    if (ok)
        flashSaves_.fetch_add(1, std::memory_order_relaxed);
    return ok;
}

void JiantApp::service(Hardware& hardware)
{
    samples_service(&hardware);
    if (!flashReady_.load(std::memory_order_acquire) || folder_ == kNoData)
        return;
    if (session().card_share.load(std::memory_order_relaxed) != 0)
        return;                                 // the card is lent to a computer: later
    std::string folder = data_folder(&hardware);
    if (folder.empty())
        return;                                 // no card yet: kept until there is one
    save_flash(folder);
    flashReady_.store(false, std::memory_order_release);
}

std::string JiantApp::status() const
{
    return "jiant=" + std::string(jiant_version()) + " saves=" + std::to_string(flashSaves_.load()) +
           " window=" + std::to_string(window_.load()) + " jiant_cpu=" + std::to_string(cpu_.load()) +
           " jiant_cpu_peak=" + std::to_string(cpuPeak_.load()) + " jiant_voices_most=" +
           std::to_string(voicesMost_.load()) + " jiant_shed=" + std::to_string(shed_.load()) +
           " jiant_governed=" + std::to_string(int(governed_.load())) + " slots_loaded=" +
           std::to_string([this] { unsigned n = 0; for (auto& f : slotFrames_) n += f.load() != 0; return n; }()) +
           " slot_loads=" + std::to_string(slotLoads_.load()) + " slot_saves=" + std::to_string(slotSaves_.load()) +
           " sample_rec=" + std::to_string(recState_.load()) + " browser=" + std::to_string(int(browserOpen_.load()));
}

std::string JiantApp::command(const std::string& action, const std::string&, Hardware&)
{
    if (action != "jiant-save")
        return {};
    return flashReady_.load() ? "OK saving" : "OK nothing waiting (saved once the firmware is idle a second)";
}

// ------------------------------------------------------------------------------------------- screen thread ---

void JiantApp::draw(juce::Graphics& g) { paint(g); }

void JiantApp::paint(juce::Graphics& g)
{
    for (int tries = 0; tries < 4; ++tries) {
        uint32_t s = screenSeq_.load(std::memory_order_acquire);
        if (s & 1u)
            continue;
        std::memcpy(screenLocal_.data(), screenShared_.data(), sizeof screenLocal_);
        std::atomic_thread_fence(std::memory_order_acquire);
        if (screenSeq_.load(std::memory_order_relaxed) == s)
            break;
    }
    juce::Image image(juce::Image::RGB, 240, 240, false);
    {
        juce::Image::BitmapData px(image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 240; ++x) {
                uint16_t v = screenLocal_[size_t(y * 240 + x)];
                v = uint16_t(v >> 8 | v << 8);   // the panel's byte order
                uint8_t r = uint8_t((v >> 11) << 3), gg = uint8_t(((v >> 5) & 63) << 2), b = uint8_t((v & 31) << 3);
                reinterpret_cast<juce::PixelRGB*>(px.getPixelPointer(x, y))->setARGB(255, r | r >> 5, gg | gg >> 6, b | b >> 5);
            }
    }
    g.fillAll(juce::Colours::black);
    g.drawImageAt(image, 40, 0);

    if (!font_) {
        try { font_ = load_typeface(); } catch (...) {}
    }
    if (!font_)
        return;
    if (browserOpen_.load(std::memory_order_relaxed))
        paint_browser(g);                       // (the SAMPLE BROWSER over JIANT's screen)
    unsigned w = window_.load(std::memory_order_relaxed);
    unsigned first = jiant_port::kWindowStart[w % jiant_port::kWindows];
    auto text = [&](const juce::String& s, int x, int y, int width, float size, juce::Colour c) {
        g.setColour(c);
        g.setFont(juce::Font(juce::FontOptions(font_).withHeight(size)));
        g.drawText(s, x, y, width, int(size) + 4, juce::Justification::centred, false);
    };
    const juce::Colour orange(0xffff7a00), grey(0xff8a8a8a);
    text("PADS", 0, 8, 40, 11, grey);
    text(juce::String::charToString(juce::juce_wchar('A' + w)), 0, 22, 40, 26, orange);
    text(key_name(first), 0, 58, 40, 11, grey);
    text(key_name(first + 15), 0, 72, 40, 11, grey);
    if (shift_.load(std::memory_order_relaxed))
        text("SHIFT", 0, 210, 40, 10, orange);
    text("JIANT", 280, 8, 40, 11, orange);
    text("CPU", 280, 196, 40, 10, grey);           // the real load of the MPC's processor
    text(juce::String(int(cpu_.load(std::memory_order_relaxed))) + "%", 280, 210, 40, 12,
         governed_.load(std::memory_order_relaxed) ? juce::Colour(0xffff4040) : orange);
}

// ------------------------------------------------------------------------------------------- the samples ---
// JIANT/Samples: the slots' files, slots.json the map (sample_files.h)

std::string JiantApp::samples_folder() const
{
    return folder_.empty() || folder_ == kNoData ? std::string() : folder_ + "/Samples";
}

// at start (before the audio runs): every slot slots.json names, read and loaded
void JiantApp::load_slots_at_start()
{
    std::string dir = samples_folder();
    if (dir.empty())
        return;
    map_ = jiant_port::slots_read(dir + "/slots.json");
    for (unsigned k = 0; k < jiant_port::kSlots; ++k) {
        jiant_port::Wav wav;
        if (map_[k].empty() || !jiant_port::wav_read(dir + "/" + map_[k], wav))
            continue;
        std::string name = map_[k].substr(0, map_[k].size() > 4 ? map_[k].size() - 4 : map_[k].size());
        if (jiant_slot_load(k, wav.l.data(), wav.r.empty() ? nullptr : wav.r.data(), uint32_t(wav.l.size()), wav.rate,
                            name.c_str()) == 0) {
            slotLoads_.fetch_add(1, std::memory_order_relaxed);
            jiant_slot_info_t in;
            jiant_slot_info(k, &in);
            slotFrames_[k].store(in.frames);
            slotCh_[k].store(in.channels);
            std::lock_guard<std::mutex> lock(filesMutex_);
            slotNames_[k] = name;
        }
    }
    jiant_service();
}

// MIDI thread: what the browser asked
void JiantApp::browser_action(jiant_port::SampleBrowser::Action a)
{
    using A = jiant_port::SampleBrowser::Action;
    auto ask = [&](SampleReq::Kind k) {
        uint32_t w = sreqW_.load(std::memory_order_relaxed);
        if (w - sreqR_.load(std::memory_order_acquire) >= sreq_.size())
            return;
        sreq_[w % sreq_.size()] = {k, uint8_t(browser_.slot()), browser_.file()};
        sreqW_.store(w + 1, std::memory_order_release);
    };
    switch (a) {
    case A::Open:
        post({Command::Keys, 0});               // (what the pads held lets go: the pads are the slots now)
        ask(SampleReq::Source);                 // (and a fresh look at the folder)
        break;
    case A::Close:
        ask(SampleReq::Audition);               // (an audition playing stops: the same file asked again)
        break;
    case A::Audition: ask(SampleReq::Audition); break;
    case A::Load: ask(SampleReq::Load); break;
    case A::Clear: ask(SampleReq::Clear); break;
    case A::Source: ask(SampleReq::Source); break;
    case A::Record: post({Command::Record, unsigned(browser_.source()), uint8_t(browser_.slot())}); break;
    default: break;
    }
    browserOpen_.store(browser_.open(), std::memory_order_relaxed);
    browserSlot_.store(browser_.slot(), std::memory_order_relaxed);
    browserFile_.store(browser_.file(), std::memory_order_relaxed);
    browserSource_.store(unsigned(browser_.source()), std::memory_order_relaxed);
    browser_.set_files(fileCount_.load(std::memory_order_relaxed));
}

// audio thread, before the render: the objects the service thread made, the slots cleared, the gens it wrote, the
// device's input into the core; every 16 blocks the slots' state out
void JiantApp::samples_audio(const AudioBlock& input)
{
    for (unsigned k = 0; k < jiant_port::kSlots; ++k)
        if (slotIn_[k].load(std::memory_order_relaxed)) {
            jiant_sample* o = slotIn_[k].exchange(nullptr, std::memory_order_acq_rel);
            if (o)
                jiant_slot_put(k, o);
        }
    if (uint32_t m = clearMask_.exchange(0, std::memory_order_acq_rel))
        for (unsigned k = 0; k < jiant_port::kSlots; ++k)
            if ((m >> k) & 1u)
                jiant_slot_clear(k);
    if (uint32_t m = savedMask_.exchange(0, std::memory_order_acq_rel))
        for (unsigned k = 0; k < jiant_port::kSlots; ++k)
            if ((m >> k) & 1u)
                jiant_slot_saved(k, savedGen_[k].load(std::memory_order_relaxed));
    for (unsigned i = 0; i < audio_frames; ++i) {
        inL_[i] = float(input[2 * i]) * (1.0f / 2147483648.0f);
        inR_[i] = float(input[2 * i + 1]) * (1.0f / 2147483648.0f);
    }
    jiant_input(inL_.data(), inR_.data(), audio_frames);
    if (jiant_sample* a = auditionIn_.load(std::memory_order_relaxed) ? auditionIn_.exchange(nullptr) : nullptr) {
        if (audition_)
            auditionOut_.store(audition_, std::memory_order_release);   // (the service frees it; one at a time)
        audition_ = a;
        auditionAt_ = 0;
    }
    if (auditionStop_.exchange(false) && audition_) {
        auditionOut_.store(audition_, std::memory_order_release);
        audition_ = nullptr;
    }
    auditioning_.store(audition_ != nullptr, std::memory_order_relaxed);
    if (++snapBlocks_ >= 16) {
        uint32_t changed = 0, slot = 0, frames = 0;
        snapBlocks_ = 0;
        for (unsigned k = 0; k < jiant_port::kSlots; ++k) {
            jiant_slot_info_t in;
            jiant_slot_info(k, &in);
            slotFrames_[k].store(in.frames, std::memory_order_relaxed);
            slotCh_[k].store(in.channels, std::memory_order_relaxed);
            slotGen_[k].store(in.gen, std::memory_order_relaxed);
            changed |= uint32_t(in.changed != 0) << k;
        }
        slotChanged_.store(changed, std::memory_order_release);
        recState_.store(uint32_t(jiant_rec_state(&slot, &frames)), std::memory_order_relaxed);
        recSlot_.store(slot, std::memory_order_relaxed);
        recFrames_.store(frames, std::memory_order_relaxed);
    }
}

// audio thread, after the render: the file auditioned, over JIANT's output
void JiantApp::audition_mix(AudioBlock& output)
{
    if (!audition_)
        return;
    uint32_t frames = 0, ch = 0;
    const int16_t* l = jiant_sample_data(audition_, 0, &frames, &ch);
    const int16_t* r = jiant_sample_data(audition_, 1, nullptr, nullptr);
    for (unsigned i = 0; i < audio_frames && auditionAt_ < frames; ++i, ++auditionAt_) {
        int64_t a = int64_t(output[2 * i]) + (int64_t(l[auditionAt_]) << 16) * 3 / 4;
        int64_t b = int64_t(output[2 * i + 1]) + (int64_t(r[auditionAt_]) << 16) * 3 / 4;
        output[2 * i] = int32_t(std::clamp<int64_t>(a, INT32_MIN, INT32_MAX));
        output[2 * i + 1] = int32_t(std::clamp<int64_t>(b, INT32_MIN, INT32_MAX));
    }
    if (auditionAt_ >= frames) {
        auditionOut_.store(audition_, std::memory_order_release);
        audition_ = nullptr;
    }
}

// service thread: the browser's requests (files read, objects made, the map written), the folder scanned, the slots
// JIANT changed written once they rest a second (flush: at once), the input's route, the core's retired objects freed
void JiantApp::samples_service(Hardware* hardware, bool flush)
{
    std::string dir = samples_folder();
    if (dir.empty() && hardware)
        dir = data_folder(hardware).empty() ? std::string() : folder_ + "/Samples";
    if (dir.empty())
        return;
    if (hardware && session().card_share.load(std::memory_order_relaxed) != 0)
        return;                                 // the card is lent to a computer: later
    auto now = std::chrono::steady_clock::now();
    std::error_code error;
    if (now - scanAt_ > std::chrono::seconds(2) || browserOpen_.load(std::memory_order_relaxed) || flush) {
        if (now - scanAt_ > std::chrono::milliseconds(500)) {
            fs::create_directories(dir, error);
            auto list = jiant_port::wav_files(dir);
            std::lock_guard<std::mutex> lock(filesMutex_);
            files_ = std::move(list);
            fileCount_.store(uint32_t(files_.size()), std::memory_order_relaxed);
            scanAt_ = now;
        }
    }
    bool mapDirty = false;
    for (uint32_t r = sreqR_.load(std::memory_order_relaxed); r != sreqW_.load(std::memory_order_acquire);
         sreqR_.store(++r, std::memory_order_release)) {
        SampleReq q = sreq_[r % sreq_.size()];
        std::string file;
        {
            std::lock_guard<std::mutex> lock(filesMutex_);
            if (q.file < files_.size())
                file = files_[q.file];
        }
        if (q.kind == SampleReq::Source) {
            sourceRouted_ = 99;                 // (routed again below)
            continue;
        }
        if (q.kind == SampleReq::Clear) {
            clearMask_.fetch_or(1u << (q.slot % jiant_port::kSlots), std::memory_order_acq_rel);
            map_[q.slot % jiant_port::kSlots].clear();
            std::lock_guard<std::mutex> lock(filesMutex_);
            slotNames_[q.slot % jiant_port::kSlots].clear();
            mapDirty = true;
            continue;
        }
        if (q.kind == SampleReq::Audition && auditioning_.load(std::memory_order_relaxed)) {
            auditionStop_.store(true, std::memory_order_release);   // (again: stop)
            continue;
        }
        if (q.kind == SampleReq::Audition && !browserOpen_.load(std::memory_order_relaxed))
            continue;                           // (the close's: nothing playing)
        jiant_port::Wav wav;
        if (file.empty() || !jiant_port::wav_read(dir + "/" + file, wav))
            continue;
        std::string name = file.substr(0, file.size() > 4 ? file.size() - 4 : file.size());
        jiant_sample* o = jiant_sample_make(wav.l.data(), wav.r.empty() ? nullptr : wav.r.data(), uint32_t(wav.l.size()),
                                            wav.rate, name.c_str());
        if (!o)
            continue;
        if (q.kind == SampleReq::Audition) {
            if (jiant_sample* old = auditionIn_.exchange(o, std::memory_order_acq_rel))
                jiant_sample_free(old);         // (one not taken yet)
            continue;
        }
        unsigned k = q.slot % jiant_port::kSlots;
        if (jiant_sample* old = slotIn_[k].exchange(o, std::memory_order_acq_rel))
            jiant_sample_free(old);
        map_[k] = file;
        mapDirty = true;
        slotLoads_.fetch_add(1, std::memory_order_relaxed);
        changeSeenGen_[k] = 0;
        std::lock_guard<std::mutex> lock(filesMutex_);
        slotNames_[k] = name;
    }
    // the slots JIANT changed: written as slotNN.wav when they rest (a second without a new change), the map naming them
    uint32_t changed = slotChanged_.load(std::memory_order_acquire);
    for (unsigned k = 0; k < jiant_port::kSlots; ++k) {
        if (!((changed >> k) & 1u))
            continue;
        uint32_t gen = slotGen_[k].load(std::memory_order_relaxed);
        if (gen != changeSeenGen_[k]) {
            changeSeenGen_[k] = gen;
            changeSeenAt_[k] = now;
            if (!flush)
                continue;
        }
        if (!flush && now - changeSeenAt_[k] < std::chrono::seconds(1))
            continue;
        const jiant_sample* o = jiant_slot_object(k);   // (whole until this thread's next jiant_service)
        uint32_t frames = 0, ch = 0;
        const int16_t* l = jiant_sample_data(o, 0, &frames, &ch);
        const int16_t* rr = jiant_sample_data(o, 1, nullptr, nullptr);
        std::string file = jiant_port::slot_file_name(k);
        if (!l || !jiant_port::wav_write16(dir + "/" + file, l, rr, frames, ch, JIANT_RATE))
            continue;
        savedGen_[k].store(gen, std::memory_order_relaxed);
        savedMask_.fetch_or(1u << k, std::memory_order_release);
        slotChanged_.fetch_and(~(1u << k), std::memory_order_acq_rel);
        slotSaves_.fetch_add(1, std::memory_order_relaxed);
        map_[k] = file;
        mapDirty = true;
        std::lock_guard<std::mutex> lock(filesMutex_);
        slotNames_[k] = file.substr(0, file.size() - 4);
    }
    if (mapDirty)
        jiant_port::slots_write(dir + "/slots.json", map_);
    // the input the recording takes: LINE / MIC through the codec's route, USB and RESAMPLE with it off
    unsigned src = browserSource_.load(std::memory_order_relaxed);
    if (hardware && src != sourceRouted_ && hardware->mixer.available()) {
        auto s = jiant_port::RecSource(src);
        hardware->mixer.input(s == jiant_port::RecSource::Line ? "line" : s == jiant_port::RecSource::Mic ? "mic" : "off");
        sourceRouted_ = src;
    }
    if (jiant_sample* a = auditionOut_.exchange(nullptr, std::memory_order_acq_rel))
        jiant_sample_free(a);
    jiant_service();
}

// screen thread: the browser over JIANT's screen
void JiantApp::paint_browser(juce::Graphics& g)
{
    if (!font_)
        return;
    auto text = [&](const juce::String& s, int x, int y, int w, float size, juce::Colour c,
                    juce::Justification j = juce::Justification::centredLeft) {
        g.setColour(c);
        g.setFont(juce::Font(juce::FontOptions(font_).withHeight(size)));
        g.drawText(s, x, y, w, int(size) + 4, j, false);
    };
    const juce::Colour orange(0xffff7a00), grey(0xff8a8a8a), white(0xffe8e8e8), bg(0xff101010);
    unsigned slot = browserSlot_.load(std::memory_order_relaxed), file = browserFile_.load(std::memory_order_relaxed);
    uint32_t frames = slotFrames_[slot % jiant_port::kSlots].load(std::memory_order_relaxed);
    g.setColour(bg);
    g.fillRect(40, 0, 240, 240);
    text("SAMPLES", 48, 4, 120, 13, orange);
    text("SLOT " + juce::String(slot + 1), 190, 4, 84, 13, white, juce::Justification::centredRight);
    std::vector<std::string> files;
    juce::String name;
    {
        std::lock_guard<std::mutex> lock(filesMutex_);
        files = files_;
        name = slotNames_[slot % jiant_port::kSlots];
    }
    text(frames ? name + "  " + juce::String(double(frames) / JIANT_RATE, 1) + " s" +
                      (slotCh_[slot % jiant_port::kSlots].load() > 1 ? "  ST" : "  MONO")
                : juce::String("EMPTY"), 48, 22, 226, 11, frames ? white : grey);
    g.setColour(grey);
    g.fillRect(48, 40, 226, 1);
    if (files.empty())
        text("no WAV in JIANT/Samples", 48, 90, 226, 11, grey);
    unsigned first = file > 4 ? file - 4 : 0;
    for (unsigned i = first; i < files.size() && i < first + 9; ++i) {
        int y = 46 + int(i - first) * 16;
        if (i == file) {
            g.setColour(orange.withAlpha(0.25f));
            g.fillRect(46, y, 230, 16);
        }
        text(files[i], 50, y + 1, 222, 11, i == file ? orange : white);
    }
    g.setColour(grey);
    g.fillRect(48, 194, 226, 1);
    unsigned rec = recState_.load(std::memory_order_relaxed);
    text("REC " + juce::String(jiant_port::kRecSourceName[browserSource_.load() % 4]), 48, 198, 120, 11,
         rec ? orange : grey);
    if (rec)
        text(rec == 2 ? "RECORDING " + juce::String(double(recFrames_.load()) / JIANT_RATE, 1) + " s" : juce::String("ARMED"),
             150, 198, 124, 11, orange, juce::Justification::centredRight);
    text("CLOSE", 48, 220, 70, 11, white);
    text(auditioning_.load() ? "STOP" : "PLAY", 125, 220, 70, 11, white, juce::Justification::centred);
    text("LOAD", 204, 220, 70, 11, white, juce::Justification::centredRight);
}
