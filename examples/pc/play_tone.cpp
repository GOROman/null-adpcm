// Encodes a 440 Hz tone to ADPCM, plays it back through the chip model and writes a WAV.
// SPDX-License-Identifier: MIT
#include "feeder.hpp"
#include "wav.hpp"
#include <cmath>
#include <cstdio>
int main(int argc, char** argv) {
    using namespace nulladpcm;
    const uint32_t rate = 15625;
    std::vector<int16_t> pcm(rate);
    for (size_t i = 0; i < pcm.size(); ++i) pcm[i] = int16_t(12000 * std::sin(2 * M_PI * 440 * i / rate));
    auto bytes = tools::encode_all(pcm);
    auto out = tools::decode_all(bytes, kClock8MHz, Divider::Div512);
    tools::Wav wav;
    wav.channels = 1;
    wav.sample_rate = rate;
    wav.samples = out;
    const char* path = argc > 1 ? argv[1] : "tone.wav";
    std::string err;
    if (!tools::write_wav(path, wav, &err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    std::printf("wrote %s\n", path);
    return 0;
}
