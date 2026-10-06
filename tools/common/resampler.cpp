// SPDX-License-Identifier: MIT
#include "resampler.hpp"

#include <cmath>

namespace nulladpcm::tools {

std::vector<int16_t> resample(const std::vector<int16_t>& in, unsigned channels, uint32_t in_rate,
                              uint32_t out_rate, int taps) {
    if (in_rate == out_rate || in.empty()) return in;
    const size_t in_frames = in.size() / channels;
    const double ratio = double(in_rate) / double(out_rate);
    const double cutoff = ratio > 1.0 ? 1.0 / ratio : 1.0; // relative to in Nyquist
    const size_t out_frames = size_t(double(in_frames) / ratio);
    std::vector<int16_t> out(out_frames * channels);
    const int half = taps / 2;
    for (size_t o = 0; o < out_frames; ++o) {
        const double center = double(o) * ratio;
        const long c0 = long(std::floor(center));
        for (unsigned ch = 0; ch < channels; ++ch) {
            double acc = 0, wsum = 0;
            for (long i = c0 - half + 1; i <= c0 + half; ++i) {
                const double x = (double(i) - center) * cutoff;
                const double sinc = x == 0 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
                const double wpos = (double(i) - center) / half; // -1..1
                if (wpos <= -1 || wpos >= 1) continue;
                const double win = 0.5 * (1 + std::cos(M_PI * wpos)); // Hann
                const double w = sinc * win * cutoff;
                const long idx = i < 0 ? 0 : (size_t(i) >= in_frames ? long(in_frames) - 1 : i);
                acc += w * in[size_t(idx) * channels + ch];
                wsum += w;
            }
            double v = wsum > 0 ? acc / wsum * cutoff : 0;
            if (cutoff == 1.0) v = acc; // upsampling: plain interpolation
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            out[o * channels + ch] = int16_t(std::lrint(v));
        }
    }
    return out;
}

} // namespace nulladpcm::tools
