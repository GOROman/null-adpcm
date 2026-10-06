#include "doctest.h"
#include "feeder.hpp"
#include "nulladpcm/msm6258.hpp"
#include "nulladpcm/msm6258_c.h"
#include <cmath>
#include <vector>
using namespace nulladpcm;

TEST_CASE("step table is floor(16 * 1.1^i)") {
    const uint16_t* t = step_table();
    for (int i = 0; i < 49; ++i) CHECK(t[i] == uint16_t(std::floor(16.0 * std::pow(1.1, i) + 1e-9)));
    CHECK(t[0] == 16);
    CHECK(t[48] == 1552);
}

TEST_CASE("decoder: datasheet delta terms, index adjust and clamping") {
    AdpcmState s;
    // nibble 0 at step 16: +2 (16/8); index -1 clamps at 0
    CHECK(s.decode(0x0) == 2);
    CHECK(s.step_index == 0);
    // nibble 7 (b2 b1 b0): 16 + 8 + 4 + 2 = +30; index +8
    CHECK(s.decode(0x7) == 32);
    CHECK(s.step_index == 8);
    // sign bit
    const int16_t before = s.sample;
    s.decode(0xF);
    CHECK(s.sample < before);
    // clamp to 12 bits
    AdpcmState c;
    for (int i = 0; i < 200; ++i) c.decode(0x7);
    CHECK(c.sample == 2047);
    CHECK(c.step_index == 48);
    for (int i = 0; i < 400; ++i) c.decode(0xF);
    CHECK(c.sample == -2048);
}

TEST_CASE("sample rate from clock and divider; X68000 PPI mapping") {
    Msm6258 a(kClock8MHz, Divider::Div512);
    CHECK(a.sample_rate() == 15625);
    Msm6258 b(kClock4MHz, Divider::Div1024);
    CHECK(b.sample_rate() == 3906);
    X68kAdpcm x;
    x.write_ppi_c(0x00); // 8 MHz, 1/1024, both channels
    CHECK(x.chip.sample_rate() == 7812);
    CHECK(x.left_enabled());
    CHECK(x.right_enabled());
    x.write_ppi_c(0x14); // 4 MHz, 1/768 -> 5208
    CHECK(x.chip.sample_rate() == 5208);
    x.write_ppi_c(0x09); // 1/512 at 8 MHz, left muted
    CHECK(x.chip.sample_rate() == 15625);
    CHECK_FALSE(x.left_enabled());
    CHECK(x.right_enabled());
}

TEST_CASE("playback consumes the low nibble first and requests data every two samples") {
    Msm6258 chip;
    static int requests;
    requests = 0;
    Callbacks cb;
    cb.data_request = [](void*) { ++requests; };
    chip.set_callbacks(cb);
    CHECK(chip.read_status() == 0);
    chip.write_command(cmd::kPlay);
    CHECK((chip.read_status() & status::kPlaying) != 0);
    CHECK(requests == 1); // first byte requested at start
    chip.write_data(0x70); // low nibble 0 (+2), high nibble 7 (+30 at step 16 -> wait, step after nibble 0 stays 0)
    CHECK(chip.clock() == 2);
    CHECK(requests == 1);
    CHECK(chip.clock() == 32);
    CHECK(requests == 2); // byte consumed -> next requested
    CHECK(chip.data_requested());
    // no data: output holds, underrun counted
    CHECK(chip.clock() == 32);
    CHECK(chip.underruns() == 1);
    chip.write_command(cmd::kStop);
    CHECK(chip.read_status() == 0);
    CHECK(chip.clock() == 0);
}

TEST_CASE("encode -> decode round trip of a sine has good SNR") {
    const uint32_t rate = 15625;
    std::vector<int16_t> pcm(rate / 2);
    for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = int16_t(10000 * std::sin(2 * M_PI * 440 * i / rate));
    auto bytes = tools::encode_all(pcm);
    CHECK(bytes.size() == pcm.size() / 2);
    auto out = tools::decode_all(bytes, kClock8MHz, Divider::Div512);
    REQUIRE(out.size() == pcm.size());
    double sig = 0, err = 0;
    for (size_t i = 100; i < pcm.size(); ++i) {
        sig += double(pcm[i]) * pcm[i];
        err += double(pcm[i] - out[i]) * (pcm[i] - out[i]);
    }
    CHECK(10 * std::log10(sig / err) > 20.0);
}

TEST_CASE("recording produces bytes through the data_ready callback") {
    Msm6258 chip;
    static std::vector<uint8_t> got;
    got.clear();
    Callbacks cb;
    cb.data_ready = [](void*, uint8_t b) { got.push_back(b); };
    chip.set_callbacks(cb);
    chip.write_command(cmd::kRecord);
    CHECK((chip.read_status() & status::kRecording) != 0);
    for (int i = 0; i < 10; ++i) chip.record_sample(int16_t(i * 100));
    CHECK(got.size() == 5);
    // decoding the recorded bytes reproduces the ramp approximately
    auto out = tools::decode_all(got, kClock8MHz, Divider::Div512);
    REQUIRE(out.size() == 10);
    CHECK(out[9] > out[0]);
}

TEST_CASE("instances are independent and copyable; C API") {
    Msm6258 a, b;
    a.write_command(cmd::kPlay);
    CHECK(a.playing());
    CHECK_FALSE(b.playing());
    Msm6258 c = a;
    CHECK(c.playing());
    alignas(16) static unsigned char buf[512];
    REQUIRE(nulladpcm_size() <= sizeof buf);
    nulladpcm_t* ch = nulladpcm_create_in(buf, sizeof buf, kClock8MHz, 2);
    REQUIRE(ch);
    CHECK(nulladpcm_sample_rate(ch) == 15625);
    nulladpcm_write_command(ch, 0x02);
    CHECK(nulladpcm_data_requested(ch) == 1);
    nulladpcm_write_data(ch, 0x00);
    int16_t o[2];
    nulladpcm_generate(ch, o, 2);
    CHECK(o[0] == 2 * 16);
}
