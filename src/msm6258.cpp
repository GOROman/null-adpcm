// null-adpcm: OKI MSM6258 core.  SPDX-License-Identifier: MIT
//
// Clean-room notice: written from the OKI MSM6258 datasheet (ADPCM algorithm,
// step table, register bits) and X68000 hardware documentation (PPI port C
// assignments). No emulator source was consulted. See docs/CLEANROOM.md.
#include "nulladpcm/msm6258.hpp"

namespace nulladpcm {

const char* version() noexcept { return "0.1.0"; }

// ---- step table: floor(16 * 1.1^i), i = 0..48 --------------------------------
namespace {
struct StepTable {
    uint16_t v[49]{};
    constexpr StepTable() {
        // 1.1^i accumulated in fixed point with enough precision (2^-32) that floor()
        // matches the datasheet values for all 49 entries (verified by test_tables).
        uint64_t x = 16ull << 32; // 16.0
        for (int i = 0; i < 49; ++i) {
            v[i] = static_cast<uint16_t>(x >> 32);
            x = x + x / 10; // * 1.1
        }
    }
};
constexpr StepTable kSteps{};

// Step index adjustment per nibble magnitude (datasheet): -1,-1,-1,-1,+2,+4,+6,+8.
constexpr int8_t kIndexAdjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

inline int16_t clamp12(int32_t v) noexcept {
    return static_cast<int16_t>(v > 2047 ? 2047 : (v < -2048 ? -2048 : v));
}
// Datasheet delta: step/8 + b2*step + b1*step/2 + b0*step/4 (each term truncated).
inline int32_t delta_for(uint32_t step, uint8_t mag) noexcept {
    int32_t d = static_cast<int32_t>(step >> 3);
    if (mag & 4) d += static_cast<int32_t>(step);
    if (mag & 2) d += static_cast<int32_t>(step >> 1);
    if (mag & 1) d += static_cast<int32_t>(step >> 2);
    return d;
}
} // namespace

const uint16_t* step_table() noexcept { return kSteps.v; }

int16_t AdpcmState::decode(uint8_t nibble) noexcept {
    const uint32_t step = kSteps.v[step_index];
    const uint8_t mag = nibble & 7;
    int32_t d = delta_for(step, mag);
    if (nibble & 8) d = -d;
    sample = clamp12(sample + d);
    int32_t idx = step_index + kIndexAdjust[mag];
    step_index = static_cast<uint8_t>(idx < 0 ? 0 : (idx > 48 ? 48 : idx));
    return sample;
}

uint8_t AdpcmState::encode(int16_t target) noexcept {
    const uint32_t step = kSteps.v[step_index];
    int32_t diff = target - sample;
    uint8_t nibble = 0;
    if (diff < 0) {
        nibble = 8;
        diff = -diff;
    }
    // Greedy magnitude selection mirroring the decoder's term structure.
    uint8_t mag = 0;
    int32_t s = static_cast<int32_t>(step);
    if (diff >= s) {
        mag |= 4;
        diff -= s;
    }
    if (diff >= (s >> 1)) {
        mag |= 2;
        diff -= s >> 1;
    }
    if (diff >= (s >> 2)) mag |= 1;
    nibble |= mag;
    decode(nibble);
    return nibble;
}

// ---- chip ----------------------------------------------------------------------

Msm6258::Msm6258(uint32_t clock_hz, Divider div) noexcept : clock_hz_(clock_hz), divider_(div) {}

void Msm6258::reset() noexcept {
    adpcm_.reset();
    data_byte_ = 0;
    rec_byte_ = 0;
    rec_nibbles_ = 0;
    nibble_pos_ = 0;
    has_data_ = false;
    playing_ = false;
    recording_ = false;
    last_out_ = 0;
    underruns_ = 0;
}

void Msm6258::request_data() noexcept {
    if (cb_.data_request) cb_.data_request(cb_.user);
}

void Msm6258::write_command(uint8_t v) noexcept {
    if (v & cmd::kStop) {
        playing_ = false;
        recording_ = false;
        has_data_ = false;
        nibble_pos_ = 0;
        last_out_ = 0;
        return;
    }
    if ((v & cmd::kPlay) && !playing_) {
        adpcm_.reset();
        nibble_pos_ = 0;
        has_data_ = false;
        playing_ = true;
        recording_ = false;
        request_data(); // first byte is requested as soon as playback starts
    }
    if ((v & cmd::kRecord) && !recording_) {
        adpcm_.reset();
        rec_nibbles_ = 0;
        recording_ = true;
        playing_ = false;
    }
}

void Msm6258::write_data(uint8_t v) noexcept {
    data_byte_ = v;
    has_data_ = true;
    nibble_pos_ = 0;
}

uint8_t Msm6258::read_status() const noexcept {
    uint8_t s = 0;
    if (playing_) s |= status::kPlaying;
    if (recording_) s |= status::kRecording;
    return s;
}

int16_t Msm6258::clock() noexcept {
    if (!playing_) return 0;
    if (!has_data_) {
        underruns_++;
        request_data();
        if (!has_data_) return last_out_; // hold the last value until data arrives
    }
    const uint8_t nib = nibble_pos_ == 0 ? (data_byte_ & 0x0F) : (data_byte_ >> 4);
    const int16_t out = adpcm_.decode(nib);
    last_out_ = out;
    if (nibble_pos_ == 0) {
        nibble_pos_ = 1;
    } else {
        nibble_pos_ = 0;
        has_data_ = false;
        request_data(); // the callback may stop the chip; this sample is still output
    }
    return out;
}

void Msm6258::generate(int16_t* out, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) out[i] = static_cast<int16_t>(clock() * 16);
}

void Msm6258::record_sample(int16_t pcm12) noexcept {
    if (!recording_) return;
    const uint8_t nib = adpcm_.encode(clamp12(pcm12));
    if (rec_nibbles_ == 0) {
        rec_byte_ = nib;
        rec_nibbles_ = 1;
    } else {
        rec_byte_ = static_cast<uint8_t>(rec_byte_ | (nib << 4));
        rec_nibbles_ = 0;
        if (cb_.data_ready) cb_.data_ready(cb_.user, rec_byte_);
    }
}

// ---- X68000 glue ----------------------------------------------------------------

void X68kAdpcm::write_ppi_c(uint8_t v) noexcept {
    ppi_c = v;
    const uint8_t div = (v >> 2) & 3;
    chip.set_divider(div == 0 ? Divider::Div1024 : (div == 1 ? Divider::Div768 : Divider::Div512));
    chip.set_clock((v & 0x10) ? kClock4MHz : kClock8MHz);
}

void X68kAdpcm::generate_stereo(int16_t* lr, size_t n) noexcept {
    const bool l = left_enabled(), r = right_enabled();
    for (size_t i = 0; i < n; ++i) {
        const int16_t s = static_cast<int16_t>(chip.clock() * 16);
        lr[2 * i] = l ? s : 0;
        lr[2 * i + 1] = r ? s : 0;
    }
}

} // namespace nulladpcm
