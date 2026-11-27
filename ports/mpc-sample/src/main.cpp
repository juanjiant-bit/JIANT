// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// JIANT FM on the Akai MPC Sample: sample-base's runtime (keremimo/sample-base) hosting JiantApp, plus the tools the
// SD boot and the build run without the hardware:
//   jiant --self-test FILE.wav     a note through the whole firmware, written out; 0 when it sounded and stopped
//   jiant --preview FILE.png       the screen after a few seconds of a generated song
//   jiant --bench                  the bench song as fast as the CPU goes: how much of the MPC's CPU JIANT takes
#include "runtime/app.h"
#include "runtime/control_socket.h"
#include "runtime/inventory.h"
#include "runtime/options.h"
#include "runtime/run_app.h"
#include "runtime/screen.h"
#include "jiant/jiant_app.h"
#include "jiant_core.h"
#include "jiant/sample_files.h"
#include <sys/stat.h>
#include <filesystem>
#include <chrono>
#include <cmath>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <iterator>
#include <string>
#include <vector>

namespace {
using jiant_port::Command;

void little(std::ofstream& f, uint32_t v, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        f.put(char(v >> (8 * i)));
}

// seconds of the app's render() into a WAV (16-bit stereo); peak and rms of the whole, rms of the last half second
struct Take { double peak = 0, rms = 0, tail = 0; bool finite = true; };
Take render_take(JiantApp& app, double seconds, const char* wav, double note_off_at)
{
    unsigned blocks = unsigned(seconds * audio_rate / audio_frames);
    std::ofstream f;
    if (wav) {
        f.open(wav, std::ios::binary);
        uint32_t bytes = blocks * audio_frames * 4;
        f.write("RIFF", 4); little(f, 36 + bytes, 4); f.write("WAVEfmt ", 8);
        little(f, 16, 4); little(f, 1, 2); little(f, 2, 2); little(f, audio_rate, 4);
        little(f, audio_rate * 4, 4); little(f, 4, 2); little(f, 16, 2);
        f.write("data", 4); little(f, bytes, 4);
    }
    AudioBlock in {}, out {};
    Take t;
    double energy = 0, tail = 0;
    unsigned tailFrom = blocks - unsigned(0.5 * audio_rate / audio_frames);
    for (unsigned b = 0; b < blocks; ++b) {
        if (note_off_at >= 0 && b == unsigned(note_off_at * audio_rate / audio_frames))
            app.post({Command::Midi, 0x80 | 60u << 8});
        app.render(in, out);
        for (int32_t s : out) {
            double v = s / 2147483648.0;
            t.finite &= std::isfinite(v);
            t.peak = std::max(t.peak, std::fabs(v));
            energy += v * v;
            if (b >= tailFrom)
                tail += v * v;
            if (wav)
                little(f, uint32_t(uint16_t(int16_t(s >> 16))), 2);
        }
    }
    t.rms = std::sqrt(energy / (blocks * audio_frames * 2.0));
    t.tail = std::sqrt(tail / ((blocks - tailFrom) * audio_frames * 2.0));
    if (wav && !f.good())
        throw std::runtime_error("Write the self-test audio");
    return t;
}

int self_test(const char* wav)
{
    auto app = std::make_unique<JiantApp>("-", 1);
    app->post({Command::Master, 800});
    app->post({Command::Midi, 0x90 | 60u << 8 | 100u << 16});
    Take t = render_take(*app, 3.0, wav, 0.6);
    std::cout << "SELF_TEST jiant=" << jiant_version() << " peak=" << t.peak << " rms=" << t.rms << " tail_rms=" << t.tail
              << '\n';
    if (!t.finite || t.peak < 0.001 || t.rms < 0.0001 || t.tail > 0.002)
        throw std::runtime_error("JIANT note/release self-test failed");
    return 0;
}

int preview(const char* png)
{
    auto app = std::make_unique<JiantApp>("-", 1);
    jiant_generate(0, 1234, 70, 0);
    jiant_play(1);
    render_take(*app, 3.0, nullptr, -1);
    juce::Image image(juce::Image::RGB, 320, 240, true);
    {
        juce::Graphics g(image);
        app->paint(g);
    }
    save_png(image, png);
    std::cout << "PREVIEW " << png << '\n';
    return 0;
}

// the flash kept across sessions: --persist-write DIR makes a song and leaves (the app writes DIR/flash.img on its
// way out, as on the MPC when the wheel is pushed); --persist-read DIR, a new process, starts from that file and
// must hold the same music
// the music saved in the device (the songs, their index, the user presets) without the live section: that comes back
// at power-on only with MENU > SYSTEM > RESTORE LAST ON, which JIANT leaves OFF (as on the FM-1)
std::vector<uint8_t> music()
{
    std::vector<uint8_t> state(jiant_state_max()), kept;
    state.resize(jiant_state_save(state.data(), state.size()));
    auto u32 = [&](size_t at) { return uint32_t(state[at]) | uint32_t(state[at + 1]) << 8 | uint32_t(state[at + 2]) << 16 |
                                       uint32_t(state[at + 3]) << 24; };
    for (size_t at = 12, n = u32(8); n--; ) {
        uint32_t id = u32(at), len = u32(at + 4);
        if (id != 0)
            kept.insert(kept.end(), state.begin() + long(at), state.begin() + long(at + 8 + len));
        if (id == 10)                          // (V10.8: the index's rows carry their PLAY mode's tag, song_chain.c
                                               // SONG_MODE_TAG, stamped once no GEN draft is open: the mode alone)
            for (size_t k = 0, row = kept.size() - len + 8; k < 8 && row + 36 * k + 3 < kept.size(); ++k)
                if ((kept[row + 36 * k + 3] & 0xFE) == 0xA0)
                    kept[row + 36 * k + 3] &= 1;
        at += 8 + len;
    }
    return kept;
}
int persist(const char* dir, bool write)
{
    std::string expect = std::string(dir) + "/expect.bin";
    auto app = std::make_unique<JiantApp>(dir, 1);
    if (write) {
        jiant_generate(0, 777, 80, 2);
        render_take(*app, 4.0, nullptr, -1);
        {   // (V10.8: GEN's song is a RAM draft until SAVE > PROJECT saves it, as on the FM-1, and power-off drops it;
            // here it is stored as the editor's backup restores one: the sections and the index into the flash)
            std::vector<uint8_t> st(jiant_state_max());
            st.resize(jiant_state_save(st.data(), st.size()));
            if (jiant_state_load(st.data(), st.size()) != 0)
                throw std::runtime_error("the song could not be stored");
            render_take(*app, 0.5, nullptr, -1);   // (the song's rows take their PLAY mode's tag: song_main.c)
        }
        int saved = jiant_autosave_now();      // (what leaving does; the music after it, the transport stopped)
        auto m = music();
        std::ofstream(expect, std::ios::binary).write(reinterpret_cast<const char*>(m.data()), std::streamsize(m.size()));
        std::cout << "PERSIST wrote " << m.size() << " bytes of music, " << jiant_flash_writes() << " flash writes, autosave "
                  << saved << '\n';
        return 0;
    }
    render_take(*app, 0.5, nullptr, -1);
    std::ifstream f(expect, std::ios::binary);
    std::vector<uint8_t> want((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()), got = music();
    std::cout << "PERSIST read " << got.size() << " bytes of music, expected " << want.size() << '\n';
    if (want.empty() || got != want) {
        std::ofstream(std::string(dir) + "/got.bin", std::ios::binary)
            .write(reinterpret_cast<const char*>(got.data()), std::streamsize(got.size()));
        throw std::runtime_error("the music did not come back from the card (got.bin beside expect.bin)");
    }
    return 0;
}

// how much of the MPC's CPU JIANT takes: the emulator's bench song (FM6 PAD, TRIO, VOICE CHOIR in 4-note chords, a
// drum track) with the busiest reverb, as fast as the CPU goes; then real time against the firmware's own meter
int bench(double seconds)
{
    auto app = std::make_unique<JiantApp>("-", 1);
    jiant_test_heavy();
    jiant_play(1);
    auto t0 = std::chrono::steady_clock::now();
    render_take(*app, seconds, nullptr, -1);
    double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "BENCH jiant=" << jiant_version() << " audio=" << seconds << "s took=" << took << "s realtime_x="
              << seconds / took << " cpu_pct_of_one_core=" << 100.0 * took / seconds << " worst_half_pct="
              << jiant_cpu_peak_percent() << " voices_now=" << jiant_voices_sounding() << " shed="
              << jiant_voices_shed() << " governed=" << jiant_load_governed() << '\n';
    return 0;
}

// the sample slots on the card: --samples-write DIR puts two WAVs in DIR/Samples (48 kHz 24-bit stereo, 22.05 kHz
// float mono) and a slots.json naming them, starts the app on DIR (it loads them), records a bar of JIANT's output
// into slot 5 and has the service write it (DIR/Samples/slot05.wav, the map); --samples-read DIR, a new process, must
// find all three slots as they were
int samples(const char* dir, bool write)
{
    std::string s = std::string(dir) + "/Samples", expect = std::string(dir) + "/expect_slots.txt";
    auto check = [](bool ok, const std::string& what) {
        std::cout << (ok ? "  ok    " : "  FAIL  ") << what << '\n';
        if (!ok)
            throw std::runtime_error("samples: " + what);
    };
    if (write) {
        std::filesystem::create_directories(s);
        {   // tone.wav: 1 s at 48 kHz, 24-bit stereo (440 Hz left, 660 Hz right)
            std::ofstream f(s + "/tone.wav", std::ios::binary);
            uint32_t n = 48000, bytes = n * 6;
            f.write("RIFF", 4); little(f, 36 + bytes, 4); f.write("WAVEfmt ", 8);
            little(f, 16, 4); little(f, 1, 2); little(f, 2, 2); little(f, 48000, 4); little(f, 48000 * 6, 4);
            little(f, 6, 2); little(f, 24, 2); f.write("data", 4); little(f, bytes, 4);
            for (uint32_t i = 0; i < n; ++i) {
                little(f, uint32_t(int32_t(4000000.0 * std::sin(2 * M_PI * 440.0 * i / 48000.0))), 3);
                little(f, uint32_t(int32_t(4000000.0 * std::sin(2 * M_PI * 660.0 * i / 48000.0))), 3);
            }
        }
        {   // noise.wav: 0.5 s at 22.05 kHz, 32-bit float mono
            std::ofstream f(s + "/noise.wav", std::ios::binary);
            uint32_t n = 11025, bytes = n * 4, seed = 1;
            f.write("RIFF", 4); little(f, 36 + bytes, 4); f.write("WAVEfmt ", 8);
            little(f, 16, 4); little(f, 3, 2); little(f, 1, 2); little(f, 22050, 4); little(f, 22050 * 4, 4);
            little(f, 4, 2); little(f, 32, 2); f.write("data", 4); little(f, bytes, 4);
            for (uint32_t i = 0; i < n; ++i) {
                seed = seed * 1664525u + 1013904223u;
                float v = float(int32_t(seed)) / 4294967296.0f;
                uint32_t u;
                std::memcpy(&u, &v, 4);
                little(f, u, 4);
            }
        }
        jiant_port::SlotMap m;
        m[0] = "tone.wav";
        m[2] = "noise.wav";
        check(jiant_port::slots_write(s + "/slots.json", m), "slots.json written");
    }
    auto app = std::make_unique<JiantApp>(dir, 1);
    jiant_slot_info_t a, b, c;
    jiant_slot_info(0, &a);
    jiant_slot_info(2, &b);
    check(a.frames == 44100 && a.channels == 2 && b.frames == 22050 && b.channels == 1 && !std::strcmp(a.name, "tone"),
          "slots 1 and 3 loaded at start from slots.json: " + std::to_string(a.frames) + " frames stereo, " +
              std::to_string(b.frames) + " mono (both at 44.1 kHz now)");
    if (write) {
        uint32_t want = 44100u * 60u / jiant_bpm() * 4u;
        render_take(*app, 0.1, nullptr, -1);
        check(jiant_rec_start(4, JIANT_REC_MASTER, 1) == 0, "a bar of the master armed into slot 5");
        app->post({Command::Midi, 0x90 | 60u << 8 | 110u << 16});
        render_take(*app, 2.5, nullptr, 1.0);
        jiant_slot_info(4, &c);
        check(c.frames == want && c.changed, "recorded: " + std::to_string(c.frames) + " frames, changed");
        render_take(*app, 0.1, nullptr, -1);
        app->samples_service(nullptr, true);
        render_take(*app, 0.1, nullptr, -1);
        jiant_slot_info(4, &c);
        check(std::filesystem::exists(s + "/slot05.wav") && !c.changed, "written to the card as Samples/slot05.wav, saved");
        check(jiant_port::slots_read(s + "/slots.json")[4] == "slot05.wav", "slots.json names it");
        const int16_t* p = jiant_slot_pcm(4, 0);
        uint64_t h = 1469598103934665603ull;
        for (uint32_t i = 0; i < c.frames; ++i)
            h = (h ^ uint16_t(p[i])) * 1099511628211ull;
        std::ofstream(expect) << c.frames << ' ' << h << '\n';
        return 0;
    }
    uint32_t frames = 0;
    uint64_t want = 0, h = 1469598103934665603ull;
    std::ifstream(expect) >> frames >> want;
    jiant_slot_info(4, &c);
    const int16_t* p = jiant_slot_pcm(4, 0);
    for (uint32_t i = 0; p && i < c.frames; ++i)
        h = (h ^ uint16_t(p[i])) * 1099511628211ull;
    check(frames && c.frames == frames && h == want && !c.changed,
          "a new process: slot 5 the recording, sample for sample (" + std::to_string(c.frames) + " frames)");
    {   // the SAMPLE BROWSER as the MPC shows it: SAMPLE pressed, pad 3, the wheel a step (DIR/browser.png)
        auto press = [&](unsigned b) {
            SurfaceEvent e;
            e.input.kind = SurfaceInput::Kind::Button;
            e.input.index = uint8_t(b);
            e.input.pressed = true;
            app->surface(e);
            e.input.pressed = false;
            app->surface(e);
        };
        press(jiant_port::Sample);
        app->samples_service(nullptr, true);
        SurfaceEvent e;
        e.pad = 2;
        e.midi.status = 0x90;
        e.midi.value = 100;
        app->surface(e);
        e.input.kind = SurfaceInput::Kind::Control;
        e.input.index = jiant_port::Wheel;
        e.input.delta = 1;
        e.pad = -1;
        app->surface(e);
        render_take(*app, 0.2, nullptr, -1);
        juce::Image image(juce::Image::RGB, 320, 240, true);
        {
            juce::Graphics g(image);
            app->paint(g);
        }
        save_png(image, (std::string(dir) + "/browser.png").c_str());
        check(app->status().find("browser=1") != std::string::npos, "SAMPLE opens the browser (" + std::string(dir) +
                                                                         "/browser.png)");
    }
    return 0;
}

std::optional<int> run_tool(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--samples-write") == 0)
        return samples(argv[2], true);
    if (argc == 3 && std::strcmp(argv[1], "--samples-read") == 0)
        return samples(argv[2], false);
    if (argc == 2 && std::strcmp(argv[1], "--bench") == 0)
        return bench(20.0);
    if (argc == 3 && std::strcmp(argv[1], "--persist-write") == 0)
        return persist(argv[2], true);
    if (argc == 3 && std::strcmp(argv[1], "--persist-read") == 0)
        return persist(argv[2], false);
    if (argc == 3 && std::strcmp(argv[1], "--self-test") == 0)
        return self_test(argv[2]);
    if (argc == 3 && std::strcmp(argv[1], "--preview") == 0)
        return preview(argv[2]);
    return std::nullopt;
}
}  // namespace

int main(int argc, char** argv)
{
    umask(0077);
    try {
        if (auto code = run_tool(argc, argv))
            return *code;
        auto options = parse_options(argc, argv, "jiant");
        switch (options.mode) {
        case Options::Mode::Run: {
            if (options.app != "jiant")
                throw std::runtime_error("This build runs only JIANT, not " + options.app);
            auto app = std::make_unique<JiantApp>(std::getenv("JIANT_DATA") ? std::getenv("JIANT_DATA") : "");
            return run_app(*app, options);
        }
        case Options::Mode::Control: return send_control_command(options.runtime, options.command);
        case Options::Mode::ControlStream: return stream_control_commands(options.runtime);
        case Options::Mode::Hardware: return print_hardware();
        }
    } catch (const std::exception& error) {
        std::cerr << "jiant: " << error.what() << '\n';
    }
    return 1;
}
