// SPDX-License-Identifier: MIT
#include "wav.hpp"

#include <cstdio>
#include <cstring>

namespace nulladpcm::tools {

namespace {
void put16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(uint8_t(v & 0xFF));
    b.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, uint16_t(v & 0xFFFF));
    put16(b, uint16_t(v >> 16));
}
uint16_t get16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t* p) { return uint32_t(get16(p) | (uint32_t(get16(p + 2)) << 16)); }
} // namespace

bool write_wav(const std::string& path, const Wav& wav, std::string* err) {
    std::vector<uint8_t> b;
    const uint32_t data_bytes = uint32_t(wav.samples.size() * 2);
    b.insert(b.end(), {'R', 'I', 'F', 'F'});
    put32(b, 36 + data_bytes);
    b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(b, 16);
    put16(b, 1); // PCM
    put16(b, wav.channels);
    put32(b, wav.sample_rate);
    put32(b, wav.sample_rate * wav.channels * 2);
    put16(b, uint16_t(wav.channels * 2));
    put16(b, 16);
    b.insert(b.end(), {'d', 'a', 't', 'a'});
    put32(b, data_bytes);
    b.reserve(b.size() + data_bytes);
    for (int16_t s : wav.samples) put16(b, uint16_t(s));
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    const bool ok = std::fwrite(b.data(), 1, b.size(), f) == b.size();
    std::fclose(f);
    if (!ok && err) *err = "write failed: " + path;
    return ok;
}

bool read_wav(const std::string& path, Wav& wav, std::string* err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        if (err) *err = "cannot open " + path;
        return false;
    }
    std::vector<uint8_t> b;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
    std::fclose(f);
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) {
        if (err) *err = "not a RIFF/WAVE file: " + path;
        return false;
    }
    size_t pos = 12;
    uint16_t fmt_tag = 0, bits = 0;
    wav.channels = 0;
    wav.sample_rate = 0;
    wav.samples.clear();
    while (pos + 8 <= b.size()) {
        const uint32_t sz = get32(&b[pos + 4]);
        const uint8_t* p = &b[pos + 8];
        if (!std::memcmp(&b[pos], "fmt ", 4) && sz >= 16) {
            fmt_tag = get16(p);
            wav.channels = get16(p + 2);
            wav.sample_rate = get32(p + 4);
            bits = get16(p + 14);
        } else if (!std::memcmp(&b[pos], "data", 4)) {
            const size_t avail = std::min<size_t>(sz, b.size() - pos - 8);
            if (fmt_tag != 1 && fmt_tag != 0xFFFE) {
                if (err) *err = "unsupported WAV format tag";
                return false;
            }
            if (bits == 16) {
                wav.samples.resize(avail / 2);
                for (size_t i = 0; i < wav.samples.size(); ++i)
                    wav.samples[i] = int16_t(get16(p + 2 * i));
            } else if (bits == 8) {
                wav.samples.resize(avail);
                for (size_t i = 0; i < avail; ++i) wav.samples[i] = int16_t((p[i] - 128) << 8);
            } else if (bits == 24) {
                wav.samples.resize(avail / 3);
                for (size_t i = 0; i < wav.samples.size(); ++i)
                    wav.samples[i] = int16_t(get16(p + 3 * i + 1));
            } else if (bits == 32) {
                wav.samples.resize(avail / 4);
                for (size_t i = 0; i < wav.samples.size(); ++i)
                    wav.samples[i] = int16_t(get16(p + 4 * i + 2));
            } else {
                if (err) *err = "unsupported bit depth";
                return false;
            }
            return true;
        }
        pos += 8 + sz + (sz & 1);
    }
    if (err) *err = "no data chunk";
    return false;
}

} // namespace nulladpcm::tools
