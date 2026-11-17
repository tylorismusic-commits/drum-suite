// Drum Suite — engine.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bleedout.hpp"
#include "controls.hpp"

namespace drum {

class Engine {
public:
  void prepare(float sampleRate, int /*maxBlock*/ = 0) noexcept {
    gate_.prepare(sampleRate);
  }

  void setControls(const Controls& c, float /*hostBpm*/ = 0.0f,
                   bool /*hostTempo*/ = false) noexcept {
    gate_.setControls(c);
  }

  void process(float* left, float* right, int frames,
               Meters& meters) noexcept {
    gate_.process(left, right, frames, meters);
  }

  void reset() noexcept { gate_.reset(); }

  int latencySamples() const noexcept { return gate_.latencySamples(); }

private:
  BleedOut gate_;
};

} // namespace drum
