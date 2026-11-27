// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
#include "jiant/sample_files.h"
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace jiant_port {

namespace {
uint32_t le(const uint8_t* p, unsigned n)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < n; ++i)
        v |= uint32_t(p[i]) << (8 * i);
    return v;
}
void put(std::vector<uint8_t>& b, uint32_t v, unsigned n)
{
    for (unsigned i = 0; i < n; ++i)
        b.push_back(uint8_t(v >> (8 * i)));
}
bool fail(std::string* error, const char* why)
{
    if (error)
        *error = why;
    return false;
}
}  // namespace

bool wav_read(const std::string& file, Wav& wav, std::string* error)
{
    std::ifstream f(file, std::ios::binary);
    if (!f)
        return fail(error, "cannot open");
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4))
        return fail(error, "not a WAV");
    unsigned format = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t bytes = 0;
    for (size_t at = 12; at + 8 <= d.size();) {
        uint32_t len = le(&d[at + 4], 4);
        size_t body = at + 8, have = std::min<size_t>(len, d.size() - body);
        if (!std::memcmp(&d[at], "fmt ", 4) && have >= 16) {
            format = le(&d[body], 2);
            ch = le(&d[body + 2], 2);
            rate = le(&d[body + 4], 4);
            bits = le(&d[body + 14], 2);
            if (format == 0xFFFE && have >= 26)    // WAVE_FORMAT_EXTENSIBLE: the subformat's first word
                format = le(&d[body + 24], 2);
        } else if (!std::memcmp(&d[at], "data", 4)) {
            data = &d[body];
            bytes = have;
        }
        at = body + len + (len & 1u);
    }
    if (!data || !ch || !rate)
        return fail(error, "no fmt or data chunk");
    bool pcm = format == 1 && (bits == 16 || bits == 24 || bits == 32), flt = format == 3 && bits == 32;
    if (!pcm && !flt)
        return fail(error, "not 16 / 24-bit PCM or 32-bit float");
    unsigned step = ch * bits / 8, take = ch > 1 ? 2 : 1;
    size_t frames = bytes / step;
    wav.rate = rate;
    wav.l.resize(frames);
    wav.r.clear();
    if (take > 1)
        wav.r.resize(frames);
    for (size_t i = 0; i < frames; ++i)
        for (unsigned c = 0; c < take; ++c) {
            const uint8_t* p = data + i * step + c * bits / 8;
            float v;
            if (flt) {
                uint32_t u = le(p, 4);
                std::memcpy(&v, &u, 4);
                v = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.0f;
            } else if (bits == 16) {
                v = float(int16_t(le(p, 2))) / 32768.0f;
            } else if (bits == 24) {
                v = float(int32_t(le(p, 3) << 8) >> 8) / 8388608.0f;
            } else {
                v = float(double(int32_t(le(p, 4))) / 2147483648.0);
            }
            (c ? wav.r : wav.l)[i] = v;
        }
    return frames ? true : fail(error, "no frames");
}

bool wav_write16(const std::string& file, const int16_t* l, const int16_t* r, uint32_t frames, uint32_t ch, uint32_t rate)
{
    if (!l || (ch == 2 && !r) || ch < 1 || ch > 2)
        return false;
    std::vector<uint8_t> b;
    uint32_t bytes = frames * ch * 2;
    b.reserve(44 + bytes);
    b.insert(b.end(), {'R', 'I', 'F', 'F'});
    put(b, 36 + bytes, 4);
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put(b, 16, 4);
    put(b, 1, 2);
    put(b, ch, 2);
    put(b, rate, 4);
    put(b, rate * ch * 2, 4);
    put(b, ch * 2, 2);
    put(b, 16, 2);
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    put(b, bytes, 4);
    for (uint32_t i = 0; i < frames; ++i) {
        put(b, uint16_t(l[i]), 2);
        if (ch == 2)
            put(b, uint16_t(r[i]), 2);
    }
    std::string temp = file + ".new";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (!f)
        return false;
    bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size() && std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok = std::fclose(f) == 0 && ok;
    std::error_code e;
    if (ok)
        fs::rename(temp, file, e);
    return ok && !e;
}

// slots.json: one flat object of "1".."16" -> a file name (no escapes but \" and \\)
SlotMap slots_read(const std::string& file)
{
    SlotMap map;
    std::ifstream f(file);
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto str = [&](size_t& at, std::string& out) {
        out.clear();
        if (at >= s.size() || s[at] != '"')
            return false;
        for (++at; at < s.size() && s[at] != '"'; ++at) {
            if (s[at] == '\\' && at + 1 < s.size())
                ++at;
            out += s[at];
        }
        return at++ < s.size();
    };
    auto space = [&](size_t& at) { while (at < s.size() && std::isspace(uint8_t(s[at]))) ++at; };
    size_t at = 0;
    space(at);
    if (at >= s.size() || s[at++] != '{')
        return map;
    for (;;) {
        std::string k, v;
        space(at);
        if (!str(at, k))
            break;
        space(at);
        if (at >= s.size() || s[at++] != ':')
            break;
        space(at);
        if (!str(at, v))
            break;
        unsigned n = unsigned(std::atoi(k.c_str()));
        if (n >= 1 && n <= kSlots && v.find('/') == std::string::npos && v.find("..") == std::string::npos)
            map[n - 1] = v;                        // (a name in the folder only: no paths)
        space(at);
        if (at < s.size() && s[at] == ',')
            ++at;
    }
    return map;
}

bool slots_write(const std::string& file, const SlotMap& map)
{
    std::string s = "{";
    bool first = true;
    for (unsigned k = 0; k < kSlots; ++k) {
        if (map[k].empty())
            continue;
        s += first ? "\n  \"" : ",\n  \"";
        s += std::to_string(k + 1) + "\": \"";
        for (char c : map[k]) {
            if (c == '"' || c == '\\')
                s += '\\';
            s += c;
        }
        s += '"';
        first = false;
    }
    s += first ? "}\n" : "\n}\n";
    std::string temp = file + ".new";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (!f)
        return false;
    bool ok = std::fwrite(s.data(), 1, s.size(), f) == s.size() && std::fflush(f) == 0 && fsync(fileno(f)) == 0;
    ok = std::fclose(f) == 0 && ok;
    std::error_code e;
    if (ok)
        fs::rename(temp, file, e);
    return ok && !e;
}

std::vector<std::string> wav_files(const std::string& folder)
{
    std::vector<std::string> out;
    std::error_code e;
    if (folder.empty() || !fs::is_directory(folder, e))
        return out;
    for (const auto& entry : fs::directory_iterator(folder, e)) {
        std::string n = entry.path().filename().string(), x = n.size() > 4 ? n.substr(n.size() - 4) : "";
        std::transform(x.begin(), x.end(), x.begin(), [](char c) { return char(std::tolower(uint8_t(c))); });
        if (x == ".wav" && n[0] != '.' && entry.is_regular_file(e))
            out.push_back(n);
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string slot_file_name(unsigned slot)
{
    char b[16];
    std::snprintf(b, sizeof b, "slot%02u.wav", slot + 1);
    return b;
}

}  // namespace jiant_port
