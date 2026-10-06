// Feeds a byte buffer to an Msm6258 through its data-request callback (DMA stand-in).
// SPDX-License-Identifier: MIT
#ifndef NULLADPCM_TOOLS_FEEDER_HPP
#define NULLADPCM_TOOLS_FEEDER_HPP
#include "nulladpcm/msm6258.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>
namespace nulladpcm::tools {
struct Feeder {
    Msm6258* chip = nullptr;
    const uint8_t* data = nullptr;
    size_t size = 0, pos = 0;
    static void on_request(void* user) {
        auto* f = static_cast<Feeder*>(user);
        if (f->pos < f->size) f->chip->write_data(f->data[f->pos++]);
        else f->chip->write_command(cmd::kStop);
    }
    void attach(Msm6258& c, const std::vector<uint8_t>& bytes) {
        chip = &c;
        data = bytes.data();
        size = bytes.size();
        pos = 0;
        Callbacks cb;
        cb.data_request = on_request;
        cb.user = this;
        c.set_callbacks(cb);
    }
    bool done() const { return pos >= size && !chip->playing(); }
};
/// Decodes a whole ADPCM byte buffer at `rate` into 16-bit mono PCM.
inline std::vector<int16_t> decode_all(const std::vector<uint8_t>& bytes, uint32_t clock_hz, Divider div) {
    Msm6258 chip(clock_hz, div);
    Feeder f;
    f.attach(chip, bytes);
    chip.write_command(cmd::kPlay);
    std::vector<int16_t> out;
    out.reserve(bytes.size() * 2);
    while (chip.playing()) out.push_back(static_cast<int16_t>(chip.clock() * 16));
    return out;
}
/// Encodes 16-bit mono PCM to ADPCM bytes (low nibble first).
inline std::vector<uint8_t> encode_all(const std::vector<int16_t>& pcm) {
    AdpcmState st;
    std::vector<uint8_t> out;
    out.reserve(pcm.size() / 2 + 1);
    for (size_t i = 0; i < pcm.size(); i += 2) {
        const uint8_t lo = st.encode(static_cast<int16_t>(pcm[i] >> 4));
        const uint8_t hi = i + 1 < pcm.size() ? st.encode(static_cast<int16_t>(pcm[i + 1] >> 4)) : 0;
        out.push_back(static_cast<uint8_t>(lo | (hi << 4)));
    }
    return out;
}
} // namespace nulladpcm::tools
#endif
