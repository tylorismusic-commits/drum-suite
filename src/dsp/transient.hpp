// Drum Suite — transient shaper
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A stereo-linked attack/sustain shaper that sits before the compressor.
// A fast envelope and a slow envelope are compared: their positive difference
// is the transient, and the slow envelope is the body.  Attack and Sustain
// are bipolar so the same control can boost or cut.

#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/db.hpp>

#include "controls.hpp"

namespace drum {

class TransientShaper {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    fastAttack_ = 1.0f - hdsp::timeCoeff(0.2f, sr_);
    fastRelease_ = 1.0f - hdsp::timeCoeff(40.0f, sr_);
    slowAttack_ = 1.0f - hdsp::timeCoeff(15.0f, sr_);
    slowRelease_ = 1.0f - hdsp::timeCoeff(180.0f, sr_);
    gainCoeff_ = 1.0f - hdsp::timeCoeff(1.0f, sr_);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.transientOn;
    attack_ = std::clamp(c.transientAttack * 0.01f, -1.0f, 1.0f);
    sustain_ = std::clamp(c.transientSustain * 0.01f, -1.0f, 1.0f);
    sensitivity_ =
        0.25f + 1.75f * std::clamp(c.transientSensitivity * 0.01f, 0.0f, 1.0f);
  }

  void process(float* left, float* right, int frames,
               Meters& meters) noexcept {
    if (!on_) {
      meters.transientDb = 0.0f;
      return;
    }
    for (int i = 0; i < frames; ++i) {
      const float peak =
          std::max(std::fabs(left[i]), std::fabs(right[i])) * sensitivity_;
      fast_ += (peak - fast_) *
               (peak > fast_ ? fastAttack_ : fastRelease_);
      slow_ += (peak - slow_) *
               (peak > slow_ ? slowAttack_ : slowRelease_);
      const float transient = std::max(0.0f, fast_ - slow_);
      const float db =
          std::clamp(attack_ * transient * 18.0f + sustain_ * slow_ * 6.0f,
                     -18.0f, 18.0f);
      const float target = hdsp::dbToGain(db);
      gain_ += (target - gain_) * gainCoeff_;
      left[i] *= gain_;
      right[i] *= gain_;
    }
    meters.transientDb = hdsp::gainToDb(gain_);
  }

  void reset() noexcept {
    fast_ = 0.0f;
    slow_ = 0.0f;
    gain_ = 1.0f;
  }

private:
  float sr_ = 48000.0f;
  bool on_ = false;
  float attack_ = 0.0f;
  float sustain_ = 0.0f;
  float sensitivity_ = 1.125f;
  float fast_ = 0.0f;
  float slow_ = 0.0f;
  float gain_ = 1.0f;
  float fastAttack_ = 0.0f;
  float fastRelease_ = 0.0f;
  float slowAttack_ = 0.0f;
  float slowRelease_ = 0.0f;
  float gainCoeff_ = 0.0f;
};

} // namespace drum
