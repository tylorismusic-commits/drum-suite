// Drum Suite — minimal WAV reader for factory/user IRs
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace drum {

// Reads a PCM 16/24/32 or float32 WAV, averages to mono, and optionally
// resamples to targetRate with linear interpolation.  Returns false on any
// unsupported or malformed file.
inline bool readWavMono(const std::string& path, std::vector<float>& out,
                        int targetRate = 0) noexcept {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  auto readU32 = [&](uint32_t& v) {
    unsigned char b[4];
    if (!in.read(reinterpret_cast<char*>(b), 4)) return false;
    v = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
        (static_cast<uint32_t>(b[2]) << 16) |
        (static_cast<uint32_t>(b[3]) << 24);
    return true;
  };

  char id[4];
  uint32_t riffSize = 0;
  if (!in.read(id, 4) || std::memcmp(id, "RIFF", 4) != 0) return false;
  if (!readU32(riffSize)) return false;
  if (!in.read(id, 4) || std::memcmp(id, "WAVE", 4) != 0) return false;

  uint16_t format = 0, channels = 0, bits = 0, blockAlign = 0;
  uint32_t sampleRate = 0;
  std::vector<unsigned char> data;

  while (in) {
    char chunk[4];
    uint32_t chunkSize = 0;
    if (!in.read(chunk, 4) || !readU32(chunkSize)) break;
    if (chunkSize > 0x7fffffffu) return false;
    if (std::memcmp(chunk, "fmt ", 4) == 0) {
      if (chunkSize < 16 || chunkSize > 1024) return false;
      std::vector<unsigned char> fmt(chunkSize);
      if (!in.read(reinterpret_cast<char*>(fmt.data()), chunkSize)) {
        return false;
      }
      format = static_cast<uint16_t>(fmt[0] | (fmt[1] << 8));
      channels = static_cast<uint16_t>(fmt[2] | (fmt[3] << 8));
      sampleRate = static_cast<uint32_t>(fmt[4]) |
                   (static_cast<uint32_t>(fmt[5]) << 8) |
                   (static_cast<uint32_t>(fmt[6]) << 16) |
                   (static_cast<uint32_t>(fmt[7]) << 24);
      blockAlign = static_cast<uint16_t>(fmt[12] | (fmt[13] << 8));
      bits = static_cast<uint16_t>(fmt[14] | (fmt[15] << 8));
    } else if (std::memcmp(chunk, "data", 4) == 0) {
      if (chunkSize > 256u * 1024u * 1024u) return false;
      data.resize(chunkSize);
      if (chunkSize && !in.read(reinterpret_cast<char*>(data.data()),
                                chunkSize)) {
        return false;
      }
      if (chunkSize & 1u) {
        in.get();
      }
    } else {
      in.seekg(chunkSize + (chunkSize & 1u), std::ios::cur);
    }
  }

  if (channels == 0 || channels > 64 || bits == 0 || data.empty()) {
    return false;
  }
  if (!(format == 1 || format == 3)) return false;
  if (format == 1 && bits != 16 && bits != 24 && bits != 32) return false;
  if (format == 3 && bits != 32) return false;
  if (sampleRate == 0) return false;

  const int bytes = bits / 8;
  if (bytes <= 0) return false;
  const size_t frameBytes = static_cast<size_t>(channels) * bytes;
  if (blockAlign == 0) {
    blockAlign = static_cast<uint16_t>(std::min<size_t>(frameBytes, 65535));
  }
  if (static_cast<size_t>(blockAlign) < frameBytes) return false;
  const size_t frames = data.size() / blockAlign;
  if (frames == 0) return false;
  std::vector<float> mono(frames, 0.0f);
  for (size_t f = 0; f < frames; ++f) {
    float sum = 0.0f;
    for (int c = 0; c < channels; ++c) {
      const size_t off = f * blockAlign + static_cast<size_t>(c) * bytes;
      float v = 0.0f;
      if (format == 3) {
        float fv = 0.0f;
        std::memcpy(&fv, data.data() + off, sizeof(float));
        v = fv;
      } else if (bits == 16) {
        int16_t iv = 0;
        std::memcpy(&iv, data.data() + off, sizeof(int16_t));
        v = static_cast<float>(iv) / 32768.0f;
      } else if (bits == 24) {
        int32_t iv = static_cast<int32_t>(data[off]) |
                     (static_cast<int32_t>(data[off + 1]) << 8) |
                     (static_cast<int32_t>(data[off + 2]) << 16);
        if (iv & 0x800000) iv |= ~0xFFFFFF;
        v = static_cast<float>(iv) / 8388608.0f;
      } else {
        int32_t iv = 0;
        std::memcpy(&iv, data.data() + off, sizeof(int32_t));
        v = static_cast<float>(static_cast<double>(iv) / 2147483648.0);
      }
      if (!std::isfinite(v)) v = 0.0f;
      sum += v;
    }
    mono[f] = sum / static_cast<float>(channels);
  }

  if (targetRate > 0 && static_cast<int>(sampleRate) != targetRate) {
    const double ratio = static_cast<double>(sampleRate) /
                         static_cast<double>(targetRate);
    const size_t outN = static_cast<size_t>(mono.size() / ratio);
    if (outN == 0) return false;
    std::vector<float> resampled(outN, 0.0f);
    for (size_t i = 0; i < outN; ++i) {
      const double pos = static_cast<double>(i) * ratio;
      const size_t i0 = static_cast<size_t>(pos);
      const double frac = pos - static_cast<double>(i0);
      const float a = mono[std::min(i0, mono.size() - 1)];
      const float b = mono[std::min(i0 + 1, mono.size() - 1)];
      resampled[i] = static_cast<float>(a + (b - a) * frac);
    }
    out = std::move(resampled);
  } else {
    out = std::move(mono);
  }
  return true;
}

} // namespace drum
