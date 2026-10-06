// null-adpcm: clean-room OKI MSM6258 ADPCM emulator (the X68000 ADPCM chip).
// SPDX-License-Identifier: MIT
#ifndef NULLADPCM_MSM6258_HPP
#define NULLADPCM_MSM6258_HPP

#include <cstddef>
#include <cstdint>

namespace nulladpcm {

/// Clocks the X68000 feeds the MSM6258 (selected by the PPI).
constexpr uint32_t kClock8MHz = 8000000;
constexpr uint32_t kClock4MHz = 4000000;

/// Sample-rate divider selected by the S1/S2 pins (PPI port C bits 2..3 on the X68000).
enum class Divider : uint8_t { Div1024 = 0, Div768 = 1, Div512 = 2 };

constexpr uint32_t divider_value(Divider d) noexcept {
    return d == Divider::Div1024 ? 1024u : (d == Divider::Div768 ? 768u : 512u);
}

/// Command register bits (write to the command port).
namespace cmd {
constexpr uint8_t kStop = 0x01;
constexpr uint8_t kPlay = 0x02;
constexpr uint8_t kRecord = 0x04;
} // namespace cmd

/// Status register bits (read from the command port).
namespace status {
constexpr uint8_t kRecording = 0x40;
constexpr uint8_t kPlaying = 0x80;
} // namespace status

struct Callbacks {
    /// Called when the chip has consumed its data byte and needs the next one
    /// (the DMA request line). Supply it with write_data() from inside or later.
    void (*data_request)(void* user) = nullptr;
    /// Called with each recorded ADPCM byte while recording.
    void (*data_ready)(void* user, uint8_t byte) = nullptr;
    void* user = nullptr;
};

/// Stateless OKI ADPCM step: decodes one 4-bit nibble.
/// Shared by the chip model and the encoder.
struct AdpcmState {
    int16_t sample = 0; // 12-bit signed, -2048..2047
    uint8_t step_index = 0; // 0..48
    void reset() noexcept {
        sample = 0;
        step_index = 0;
    }
    /// Applies one nibble and returns the new 12-bit sample.
    int16_t decode(uint8_t nibble) noexcept;
    /// Chooses the nibble that best approximates `target` (12-bit), applies it and returns it.
    uint8_t encode(int16_t target) noexcept;
};

/// Step size table: floor(16 * 1.1^i) for i = 0..48 (the OKI datasheet table).
const uint16_t* step_table() noexcept;

/// One MSM6258. Copyable, no heap, no globals.
class Msm6258 {
public:
    explicit Msm6258(uint32_t clock_hz = kClock8MHz, Divider div = Divider::Div512) noexcept;

    void reset() noexcept;
    void set_callbacks(const Callbacks& cb) noexcept { cb_ = cb; }
    void set_clock(uint32_t clock_hz) noexcept { clock_hz_ = clock_hz; }
    void set_divider(Divider d) noexcept { divider_ = d; }
    uint32_t clock_hz() const noexcept { return clock_hz_; }
    Divider divider() const noexcept { return divider_; }
    /// ADPCM sample rate: clock / divider (e.g. 8 MHz / 512 = 15625 Hz).
    uint32_t sample_rate() const noexcept { return clock_hz_ / divider_value(divider_); }

    // --- register interface (X68000: command at $E92001, data at $E92003) ----
    void write_command(uint8_t v) noexcept;
    void write_data(uint8_t v) noexcept; // one byte = two nibbles, low nibble first
    uint8_t read_status() const noexcept;
    uint8_t read_data() const noexcept { return rec_byte_; } // last recorded byte

    bool playing() const noexcept { return playing_; }
    bool recording() const noexcept { return recording_; }
    /// True while the data byte register is empty and the chip wants more data.
    bool data_requested() const noexcept { return playing_ && !has_data_; }

    // --- audio -----------------------------------------------------------------
    /// Advances one ADPCM sample period; returns the 12-bit output (0 when stopped).
    int16_t clock() noexcept;
    /// Renders n samples at the ADPCM rate, 12-bit values scaled to 16-bit range.
    void generate(int16_t* out, size_t n) noexcept;
    /// Feeds one 12-bit PCM sample while recording (the ADC input).
    void record_sample(int16_t pcm12) noexcept;

    uint32_t underruns() const noexcept { return underruns_; }
    const AdpcmState& state() const noexcept { return adpcm_; }

private:
    void request_data() noexcept;

    AdpcmState adpcm_{};
    Callbacks cb_{};
    uint32_t clock_hz_ = kClock8MHz;
    Divider divider_ = Divider::Div512;
    uint8_t data_byte_ = 0;
    uint8_t rec_byte_ = 0;
    uint8_t rec_nibbles_ = 0;
    uint8_t nibble_pos_ = 0; // 0 = low nibble next, 1 = high nibble next
    bool has_data_ = false;
    bool playing_ = false;
    bool recording_ = false;
    int16_t last_out_ = 0;
    uint32_t underruns_ = 0;
};

/// X68000 glue: the PPI (8255 at $E9A001..) port C selects clock, divider and pan.
/// Bit layout (PPI port C): bit0 = left mute, bit1 = right mute, bits3..2 = divider
/// (0: 1/1024, 1: 1/768, 2: 1/512, 3: reserved -> 1/512), bit4 = clock (0: 8 MHz, 1: 4 MHz).
struct X68kAdpcm {
    Msm6258 chip;
    uint8_t ppi_c = 0;

    X68kAdpcm() noexcept : chip(kClock8MHz, Divider::Div512) {}
    void write_ppi_c(uint8_t v) noexcept;
    bool left_enabled() const noexcept { return !(ppi_c & 1); }
    bool right_enabled() const noexcept { return !(ppi_c & 2); }
    /// Renders n stereo frames (int16, interleaved) at chip.sample_rate().
    void generate_stereo(int16_t* lr, size_t n) noexcept;
};

const char* version() noexcept;

} // namespace nulladpcm
#endif
