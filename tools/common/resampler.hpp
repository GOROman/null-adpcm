// Windowed-sinc resampler for the tools (float math is fine outside the core).
// SPDX-License-Identifier: MIT
#ifndef NULLADPCM_TOOLS_RESAMPLER_HPP
#define NULLADPCM_TOOLS_RESAMPLER_HPP

#include <cstdint>
#include <vector>

namespace nulladpcm::tools {

/// Resamples interleaved int16 audio from in_rate to out_rate.
std::vector<int16_t> resample(const std::vector<int16_t>& in, unsigned channels, uint32_t in_rate,
                              uint32_t out_rate, int taps = 64);

} // namespace nulladpcm::tools
#endif
