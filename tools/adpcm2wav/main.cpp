// adpcm2wav: decode an X68000 .pcm (MSM6258 ADPCM) file to WAV.  SPDX-License-Identifier: MIT
#include "feeder.hpp"
#include "resampler.hpp"
#include "wav.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace nulladpcm;
static void usage() {
    std::fprintf(stderr, "usage: adpcm2wav [--rate 3900|5200|7800|10400|15600] [--out-rate HZ] [--pan L|R|LR] in.pcm out.wav\n");
}
int main(int argc, char** argv) {
    std::string in, out, pan = "LR";
    uint32_t rate = 15600, out_rate = 0;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--rate" && i + 1 < argc) rate = uint32_t(std::atoi(argv[++i]));
        else if (a == "--out-rate" && i + 1 < argc) out_rate = uint32_t(std::atoi(argv[++i]));
        else if (a == "--pan" && i + 1 < argc) pan = argv[++i];
        else if (in.empty()) in = a;
        else if (out.empty()) out = a;
        else { usage(); return 2; }
    }
    if (out.empty()) { usage(); return 2; }
    uint32_t clock = kClock8MHz;
    Divider div = Divider::Div512;
    switch (rate) {
    case 3900: clock = kClock4MHz; div = Divider::Div1024; break;
    case 5200: clock = kClock4MHz; div = Divider::Div768; break;
    case 7800: clock = kClock4MHz; div = Divider::Div512; break;
    case 10400: clock = kClock8MHz; div = Divider::Div768; break;
    case 15600: break;
    default: std::fprintf(stderr, "unsupported rate %u\n", rate); return 2;
    }
    FILE* f = std::fopen(in.c_str(), "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", in.c_str()); return 1; }
    std::vector<uint8_t> bytes;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    auto mono = tools::decode_all(bytes, clock, div);
    const uint32_t native = clock / divider_value(div);
    tools::Wav wav;
    wav.channels = 2;
    wav.sample_rate = native;
    wav.samples.resize(mono.size() * 2);
    const bool l = pan.find('L') != std::string::npos, r = pan.find('R') != std::string::npos;
    for (size_t i = 0; i < mono.size(); ++i) {
        wav.samples[2 * i] = l ? mono[i] : 0;
        wav.samples[2 * i + 1] = r ? mono[i] : 0;
    }
    if (out_rate && out_rate != native) {
        wav.samples = tools::resample(wav.samples, 2, native, out_rate);
        wav.sample_rate = out_rate;
    }
    std::string err;
    if (!tools::write_wav(out, wav, &err)) { std::fprintf(stderr, "%s\n", err.c_str()); return 1; }
    std::printf("wrote %s: %zu frames @ %u Hz (ADPCM %u Hz, %zu bytes)\n", out.c_str(), wav.frames(), wav.sample_rate, native, bytes.size());
    return 0;
}
