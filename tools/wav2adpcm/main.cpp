// wav2adpcm: encode a WAV file to X68000 .pcm (MSM6258 ADPCM).  SPDX-License-Identifier: MIT
#include "feeder.hpp"
#include "resampler.hpp"
#include "wav.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace nulladpcm;
int main(int argc, char** argv) {
    std::string in, out;
    uint32_t rate = 15600;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rate" && i + 1 < argc) rate = uint32_t(std::atoi(argv[++i]));
        else if (in.empty()) in = a;
        else if (out.empty()) out = a;
    }
    if (out.empty()) { std::fprintf(stderr, "usage: wav2adpcm [--rate HZ] in.wav out.pcm\n"); return 2; }
    tools::Wav wav;
    std::string err;
    if (!tools::read_wav(in, wav, &err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    // mono mix
    std::vector<int16_t> mono(wav.frames());
    for (size_t i = 0; i < mono.size(); ++i) {
        int32_t s = 0;
        for (unsigned c = 0; c < wav.channels; ++c) s += wav.samples[i * wav.channels + c];
        mono[i] = int16_t(s / int(wav.channels));
    }
    const uint32_t exact = rate == 15600 ? 15625 : rate == 10400 ? 10416 : rate == 7800 ? 7812 : rate == 5200 ? 5208 : 3906;
    if (wav.sample_rate != exact) mono = tools::resample(mono, 1, wav.sample_rate, exact);
    auto bytes = tools::encode_all(mono);
    FILE* f = std::fopen(out.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", out.c_str()); return 1; }
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    std::printf("wrote %s: %zu bytes (%zu samples @ %u Hz)\n", out.c_str(), bytes.size(), mono.size(), exact);
    return 0;
}
