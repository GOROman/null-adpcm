// Minimal WAV reader/writer for the null-adpcm tools.  SPDX-License-Identifier: MIT
#ifndef NULLADPCM_TOOLS_WAV_HPP
#define NULLADPCM_TOOLS_WAV_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace nulladpcm::tools {

struct Wav {
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    std::vector<int16_t> samples; // interleaved
    size_t frames() const { return channels ? samples.size() / channels : 0; }
};

bool write_wav(const std::string& path, const Wav& wav, std::string* err = nullptr);
bool read_wav(const std::string& path, Wav& wav, std::string* err = nullptr);

} // namespace nulladpcm::tools
#endif
