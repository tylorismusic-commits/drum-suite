// Drum Suite — FLAC reader (dr_flac)
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "wav.hpp"

#include "dr_flac.h"

namespace drum {

inline bool readFlacData(const std::string& path, WavData& out,
                         int targetRate = 0) noexcept {
  drflac* flac = drflac_open_file(path.c_str(), nullptr);
  if (!flac) return false;
  const int channels = static_cast<int>(flac->channels);
  const uint64_t frames = flac->totalPCMFrameCount;
  if (channels <= 0 || frames == 0 || frames > 100000000ull) {
    drflac_close(flac);
    return false;
  }
  std::vector<float> interleaved(static_cast<size_t>(frames) * channels);
  const uint64_t got =
      drflac_read_pcm_frames_f32(flac, frames, interleaved.data());
  drflac_close(flac);
  if (got == 0) return false;
  const size_t n = static_cast<size_t>(got);
  out = WavData{};
  out.sampleRate = static_cast<int>(flac->sampleRate);
  out.left.resize(n);
  if (channels > 1) out.right.resize(n);
  for (size_t i = 0; i < n; ++i) {
    out.left[i] = interleaved[i * channels];
    if (channels > 1) out.right[i] = interleaved[i * channels + 1];
  }

  if (targetRate > 0 && out.sampleRate != targetRate) {
    const double ratio = static_cast<double>(out.sampleRate) /
                         static_cast<double>(targetRate);
    auto resample = [&](std::vector<float>& v) {
      if (v.empty()) return;
      const size_t outN = static_cast<size_t>(v.size() / ratio);
      if (outN == 0) {
        v.clear();
        return;
      }
      std::vector<float> r(outN);
      for (size_t i = 0; i < outN; ++i) {
        const double pos = static_cast<double>(i) * ratio;
        const size_t i0 = static_cast<size_t>(pos);
        const double frac = pos - static_cast<double>(i0);
        const float a = v[std::min(i0, v.size() - 1)];
        const float b = v[std::min(i0 + 1, v.size() - 1)];
        r[i] = static_cast<float>(a + (b - a) * frac);
      }
      v = std::move(r);
    };
    resample(out.left);
    resample(out.right);
    out.sampleRate = targetRate;
  }
  return !out.left.empty();
}

} // namespace drum
