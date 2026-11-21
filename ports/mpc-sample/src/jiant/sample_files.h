// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
//
// The sample slots' files on the card (JIANT/Samples): WAV in (16 and 24-bit PCM, 32-bit float; mono, stereo, more
// channels: the first two; any rate: the core resamples), WAV out (16-bit, as the slots hold it), and slots.json, the
// slot -> file map ({"1": "kick.wav", "6": "slot06.wav"}). The service thread's (files), no core state.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace jiant_port {

struct Wav {
    std::vector<float> l, r;                       // r empty: mono
    uint32_t rate = 0;
};
// the file read whole; false (and why in *error) when it is not a WAV this reads
bool wav_read(const std::string& file, Wav& wav, std::string* error = nullptr);
// 16-bit PCM, ch 1 or 2 (r ignored for 1), written to file.new then renamed over file
bool wav_write16(const std::string& file, const int16_t* l, const int16_t* r, uint32_t frames, uint32_t ch,
                 uint32_t rate);

constexpr unsigned kSlots = 16;
using SlotMap = std::array<std::string, kSlots>;   // a file name in the folder, or empty
SlotMap slots_read(const std::string& file);       // missing or broken: all empty
bool slots_write(const std::string& file, const SlotMap& map);
// the .wav files of a folder, sorted by name
std::vector<std::string> wav_files(const std::string& folder);
// slot k's own file when JIANT writes it (a recording, DESTROY): slot01.wav .. slot16.wav
std::string slot_file_name(unsigned slot);

}  // namespace jiant_port
