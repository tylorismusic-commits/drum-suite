// Drum Suite — drive
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One band of Patina's saturator.  The model, drive, character, tone and mix
// controls come straight from Patina; this wrapper adds the drum-focused auto
// gain so a parallel blend does not jump in level.

#pragma once

#include <algorithm>
#include <cmath>

#include "drive/controls.hpp"
#include "drive/saturation.hpp"
#include "controls.hpp"

namespace drum {

class Drive {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    sat_.prepare(sr_);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    patina::BandControls bc{};
    bc.model = std::clamp(c.driveModel, 0,
                          static_cast<int>(patina::Model::Count) - 1);
    bc.enabled = true;
    bc.input = 0.5f;
    bc.drive = std::clamp(c.driveDrive, 0.0f, 1.0f);
    bc.character = std::clamp(c.driveCharacter, 0.0f, 1.0f);
    bc.tone = std::clamp(c.driveTone, 0.0f, 1.0f);
    bc.mix = std::clamp(c.driveMix, 0.0f, 1.0f);
    mix_ = bc.mix;
    bc.level = 0.5f;
    sat_.setControls(bc, -1, 0);
    on_ = c.driveOn;
    autoGain_ = c.driveAutoGain;
    if (!on_) gain_ = 1.0f;
  }

  void process(float* left, float* right, int frames) noexcept {
    if (!on_ || mix_ <= 0.0001f) {
      gain_ = 1.0f;
      return;
    }
    for (int i = 0; i < frames; ++i) {
      const float inL = left[i];
      const float inR = right[i];
      float l = inL;
      float r = inR;
      sat_.process(l, r);
      if (autoGain_) {
        inPow_ += (inL * inL + inR * inR - inPow_) * detK_;
        outPow_ += (l * l + r * r - outPow_) * detK_;
        const float target =
            outPow_ > 1e-12f ? std::sqrt((inPow_ + 1e-12f) / (outPow_ + 1e-12f))
                             : 1.0f;
        target_ = std::clamp(target, 0.25f, 4.0f);
      } else {
        target_ = 1.0f;
      }
      gain_ += (target_ - gain_) * gainK_;
      left[i] = std::isfinite(l * gain_) ? l * gain_ : 0.0f;
      right[i] = std::isfinite(r * gain_) ? r * gain_ : 0.0f;
    }
  }

  void reset() noexcept {
    sat_.reset();
    gain_ = 1.0f;
    target_ = 1.0f;
    inPow_ = 0.0f;
    outPow_ = 0.0f;
    detK_ = 1.0f - std::exp(-1.0f / (0.2f * sr_));
    gainK_ = 1.0f - std::exp(-1.0f / (0.02f * sr_));
  }

  int latencySamples() const noexcept { return 0; }

private:
  float sr_ = 48000.0f;
  bool on_ = false;
  bool autoGain_ = true;
  float mix_ = 1.0f;
  float gain_ = 1.0f;
  float target_ = 1.0f;
  float inPow_ = 0.0f;
  float outPow_ = 0.0f;
  float detK_ = 0.0001f;
  float gainK_ = 0.001f;
  patina::BandSaturator sat_;
};

} // namespace drum
