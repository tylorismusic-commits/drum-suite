// Drum Suite — sample file helper
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "controls.hpp"
#include "instrument.hpp"
#include "wav.hpp"

namespace drum {

// Loads a WAV, SFZ, SF2/3 or a folder of WAVs as a multisample instrument.
inline std::shared_ptr<const Instrument> loadInstrumentFile(
    const std::string& path) noexcept {
  return loadInstrument(path);
}

inline std::shared_ptr<const SampleBuffer> loadSampleFile(
    const std::string& path) noexcept {
  std::vector<float> data;
  if (!readWavMono(path, data, 48000) || data.empty()) {
    return nullptr;
  }
  auto buffer = std::make_shared<SampleBuffer>();
  buffer->data = std::move(data);
  buffer->sampleRate = 48000;
  buffer->name = std::filesystem::path(path).filename().string();
  buffer->path = path;
  return buffer;
}

} // namespace drum
