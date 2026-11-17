// Drum Suite — true-peak limiter
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Lookahead limiter based on the Mastering Suite design, simplified for the
// drum channel.  The delay is fixed at 2 ms + the half-band filter latency, so
// the Lookahead knob changes how early the detector acts without moving host
// delay compensation.  True Peak selects 4x oversampled detection; otherwise
// the detector runs at the host rate and uses sample peaks.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <hdsp/db.hpp>
#include <hdsp/oversample.hpp>
#include <hdsp/smoother.hpp>
#include <hdsp/truepeak.hpp>

#include "controls.hpp"

namespace drum {

inline float limiterCeilingDb(float value) noexcept {
  return -12.0f + 12.0f * std::clamp(value, 0.0f, 1.0f);
}

inline float limiterReleaseMs(float value) noexcept {
  return 20.0f * std::pow(50.0f, std::clamp(value, 0.0f, 1.0f));
}

struct LimiterControls {
  bool enabled = true;
  float ceiling = 0.95f;
  float release = 0.5f;
  float lookahead = 0.5f;
  bool truePeak = true;
};

class LookaheadLimiter {
public:
  static constexpr int kMaxBlock = 1024;
  static constexpr int kFilterLatency = 45;

  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    maxLookahead_ = std::max(1, static_cast<int>(0.002f * sr_));
    const int maxOsLook = maxLookahead_ * 4;
    delayL_.assign(static_cast<size_t>(maxOsLook + 1), 0.0f);
    delayR_.assign(static_cast<size_t>(maxOsLook + 1), 0.0f);
    gainDelay_.assign(static_cast<size_t>(maxOsLook + 1), 0.0f);
    osL_.assign(static_cast<size_t>(kMaxBlock * 4), 0.0f);
    osR_.assign(static_cast<size_t>(kMaxBlock * 4), 0.0f);
    padL_.assign(static_cast<size_t>(kFilterLatency + 1), 0.0f);
    padR_.assign(static_cast<size_t>(kFilterLatency + 1), 0.0f);
    osFactor_ = 0;
    configure(controls_.truePeak ? 2 : 0);
    setControls(controls_);
  }

  void setControls(const LimiterControls& c) noexcept {
    const bool wasEnabled = controls_.enabled;
    controls_ = c;
    const int os = c.truePeak ? 2 : 0;
    if (os != osSetting_) configure(os);
    ceiling_ = hdsp::dbToGain(limiterCeilingDb(c.ceiling) - 0.05f);
    lookahead_ = static_cast<int>(
        (0.0002f + 0.0018f * std::clamp(c.lookahead, 0.0f, 1.0f)) * sr_);
    lookahead_ = std::max(1, std::min(lookahead_, maxLookahead_));
    releaseCoeff_ =
        hdsp::timeCoeff(limiterReleaseMs(c.release), sr_ * osFactor_);
    attackCoeff_ = hdsp::timeCoeff(0.1f, sr_ * osFactor_);
    if (c.enabled && !wasEnabled) {
      gain_ = 1.0f;
      grDb_ = 0.0f;
      tpL_.reset();
      tpR_.reset();
    }
  }

  void process(float* left, float* right, int frames) noexcept {
    grDb_ = 0.0f;
    for (int offset = 0; offset < frames; offset += kMaxBlock) {
      const int n = std::min(kMaxBlock, frames - offset);
      if (osFactor_ == 1) {
        processSamples(left + offset, right + offset, n);
        applyPad(left + offset, right + offset, n);
      } else {
        os_.up(left + offset, right + offset, n, osL_.data(), osR_.data());
        processSamples(osL_.data(), osR_.data(), n * osFactor_);
        os_.down(osL_.data(), osR_.data(), n, left + offset, right + offset);
        applyPad(left + offset, right + offset, n);
      }
    }
  }

  // Bypass path that preserves the limiter's fixed latency.
  void processBypass(float* left, float* right, int frames) noexcept {
    const int d = maxLookahead_ + kFilterLatency;
    for (int i = 0; i < frames; ++i) {
      const int w = write_ % static_cast<int>(delayL_.size());
      delayL_[static_cast<size_t>(w)] = left[i];
      delayR_[static_cast<size_t>(w)] = right[i];
      const int r = (write_ - d + static_cast<int>(delayL_.size())) %
                    static_cast<int>(delayL_.size());
      left[i] = delayL_[static_cast<size_t>(r)];
      right[i] = delayR_[static_cast<size_t>(r)];
      write_ = (write_ + 1) % static_cast<int>(delayL_.size());
    }
    grDb_ = 0.0f;
  }

  void reset() noexcept {
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    std::fill(gainDelay_.begin(), gainDelay_.end(), 0.0f);
    std::fill(padL_.begin(), padL_.end(), 0.0f);
    std::fill(padR_.begin(), padR_.end(), 0.0f);
    write_ = 0;
    padWrite_ = 0;
    gain_ = 1.0f;
    grDb_ = 0.0f;
    tpL_.reset();
    tpR_.reset();
  }

  int latencySamples() const noexcept { return maxLookahead_ + kFilterLatency; }
  float gainReductionDb() const noexcept { return grDb_; }

private:
  void configure(int setting) noexcept {
    osSetting_ = std::clamp(setting, 0, 2);
    osFactor_ = osSetting_ == 2 ? 4 : (osSetting_ == 1 ? 2 : 1);
    os_.prepare(osFactor_);
    padDelay_ = kFilterLatency - os_.latencySamples();
    if (padDelay_ < 0) padDelay_ = 0;
    reset();
  }

  void processSamples(float* l, float* r, int n) noexcept {
    const int maxD = maxLookahead_ * osFactor_;
    const int delay = maxD;
    const int gainDelay = std::max(0, maxD - lookahead_ * osFactor_);
    const int size = static_cast<int>(delayL_.size());
    for (int i = 0; i < n; ++i) {
      const float inL = l[i];
      const float inR = r[i];
      const float peak = std::max(std::fabs(inL), std::fabs(inR));
      const float target = peak > ceiling_ ? ceiling_ / peak : 1.0f;
      if (target < gain_)
        gain_ += (target - gain_) * (1.0f - attackCoeff_);
      else
        gain_ += (target - gain_) * (1.0f - releaseCoeff_);
      gain_ = std::clamp(gain_, 0.0f, 1.0f);
      if (controls_.enabled)
        grDb_ = std::max(grDb_, -hdsp::gainToDb(std::max(gain_, 1e-9f)));

      const int w = write_ % size;
      delayL_[static_cast<size_t>(w)] = inL;
      delayR_[static_cast<size_t>(w)] = inR;
      gainDelay_[static_cast<size_t>(w)] = controls_.enabled ? gain_ : 1.0f;

      const int ar = (write_ - delay + size) % size;
      const int gr = (write_ - gainDelay + size) % size;
      float outL = delayL_[static_cast<size_t>(ar)];
      float outR = delayR_[static_cast<size_t>(ar)];
      const float g = gainDelay_[static_cast<size_t>(gr)];
      outL *= g;
      outR *= g;
      if (controls_.enabled) {
        outL = std::clamp(outL, -ceiling_, ceiling_);
        outR = std::clamp(outR, -ceiling_, ceiling_);
      }
      l[i] = outL;
      r[i] = outR;
      write_ = (write_ + 1) % size;
    }
  }

  void applyPad(float* l, float* r, int n) noexcept {
    if (padDelay_ <= 0) return;
    const int size = static_cast<int>(padL_.size());
    for (int i = 0; i < n; ++i) {
      padL_[static_cast<size_t>(padWrite_)] = l[i];
      padR_[static_cast<size_t>(padWrite_)] = r[i];
      const int read = (padWrite_ - padDelay_ + size) % size;
      l[i] = padL_[static_cast<size_t>(read)];
      r[i] = padR_[static_cast<size_t>(read)];
      padWrite_ = (padWrite_ + 1) % size;
    }
  }

  float sr_ = 48000.0f;
  LimiterControls controls_;
  hdsp::Oversampler os_;
  hdsp::TruePeakDetector tpL_, tpR_;
  int osSetting_ = -1;
  int osFactor_ = 1;
  int maxLookahead_ = 96;
  int lookahead_ = 48;
  int padDelay_ = 45;
  std::vector<float> delayL_, delayR_, gainDelay_, osL_, osR_, padL_, padR_;
  int write_ = 0;
  int padWrite_ = 0;
  float ceiling_ = 0.97f;
  float gain_ = 1.0f;
  float grDb_ = 0.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
};

class Limiter {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    lim_.prepare(sr_);
    dryL_.assign(kMaxDry, 0.0f);
    dryR_.assign(kMaxDry, 0.0f);
    dryPos_ = 0;
  }

  void setControls(const Controls& c) noexcept {
    LimiterControls lc;
    lc.enabled = c.limitOn;
    lc.ceiling = c.limitCeiling;
    lc.release = c.limitRelease;
    lc.lookahead = c.limitLookahead;
    lc.truePeak = c.limitTruePeak;
    lim_.setControls(lc);
    mix_ = std::clamp(c.limitMix, 0.0f, 1.0f);
    on_ = c.limitOn;
    latency_ = lim_.latencySamples();
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    if (!on_) {
      lim_.processBypass(left, right, frames);
      meters.limitGrDb = 0.0f;
      return;
    }
    const int delay = std::min(latency_, kMaxDry - 1);
    const int start = dryPos_;
    for (int i = 0; i < frames; ++i) {
      dryL_[static_cast<size_t>(dryPos_)] = left[i];
      dryR_[static_cast<size_t>(dryPos_)] = right[i];
      dryPos_ = (dryPos_ + 1) % kMaxDry;
    }
    lim_.process(left, right, frames);
    for (int i = 0; i < frames; ++i) {
      const int r = (start + i - delay + kMaxDry) % kMaxDry;
      const float dl = dryL_[static_cast<size_t>(r)];
      const float dr = dryR_[static_cast<size_t>(r)];
      left[i] = dl + (left[i] - dl) * mix_;
      right[i] = dr + (right[i] - dr) * mix_;
    }
    meters.limitGrDb = lim_.gainReductionDb();
  }

  void processBypass(float* left, float* right, int frames) noexcept {
    lim_.processBypass(left, right, frames);
  }

  void reset() noexcept {
    lim_.reset();
    std::fill(dryL_.begin(), dryL_.end(), 0.0f);
    std::fill(dryR_.begin(), dryR_.end(), 0.0f);
    dryPos_ = 0;
  }

  int latencySamples() const noexcept { return lim_.latencySamples(); }

private:
  static constexpr int kMaxDry = 4096;
  float sr_ = 48000.0f;
  float mix_ = 1.0f;
  bool on_ = false;
  int latency_ = 141;
  int dryPos_ = 0;
  std::vector<float> dryL_, dryR_;
  LookaheadLimiter lim_;
};

} // namespace drum
