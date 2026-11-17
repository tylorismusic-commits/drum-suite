// Drum Suite — clipper
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A controllable clipper after the drive stage.  Soft and diode shapes use
// first-order ADAA on the oversampled signal; hard clip is the exact clamp.
// Oversampling runs at 1x, 2x or 4x, but the output is always padded to the
// same 45-sample latency so host delay compensation never jumps.

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <hdsp/adaa.hpp>
#include <hdsp/db.hpp>
#include <hdsp/oversample.hpp>

#include "controls.hpp"

namespace drum {

inline float clipShapeValue(int shape, float x) noexcept {
  switch (shape) {
    case 1:
      return std::clamp(x, -1.0f, 1.0f);
    case 2: {
      constexpr float b = 0.22f;
      const float tb = std::tanh(b);
      return (std::tanh(x + b) - tb) / (1.0f - tb);
    }
    default:
      return std::tanh(x);
  }
}

inline float clipCeilingDb(float value) noexcept {
  return -12.0f + 12.0f * std::clamp(value, 0.0f, 1.0f);
}

class Clip {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    const int maxFrames = hdsp::Oversampler::kMaxBaseFrames * 4;
    osL_.assign(static_cast<size_t>(maxFrames), 0.0f);
    osR_.assign(static_cast<size_t>(maxFrames), 0.0f);
    wetL_.assign(static_cast<size_t>(maxFrames), 0.0f);
    wetR_.assign(static_cast<size_t>(maxFrames), 0.0f);
    os_.prepare(1);
    osFactor_ = 1;
    pad_ = kMaxLatency - os_.latencySamples();
    reset();
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.clipOn;
    const int mode = std::clamp(c.clipOversample, 0, 2);
    const int factor = mode == 0 ? 1 : (mode == 1 ? 2 : 4);
    if (factor != osFactor_) {
      os_.prepare(factor);
      osFactor_ = factor;
      pad_ = kMaxLatency - os_.latencySamples();
      adL_.reset();
      adR_.reset();
    }
    const int shape = std::clamp(c.clipShape, 0, 2);
    if (shape != shape_) {
      shape_ = shape;
      adL_.reset();
      adR_.reset();
    }
    driveGain_ = hdsp::dbToGain(24.0f * std::clamp(c.clipDrive, 0.0f, 1.0f));
    ceilingLin_ = hdsp::dbToGain(clipCeilingDb(c.clipCeiling));
    mix_ = std::clamp(c.clipMix, 0.0f, 1.0f);
    autoGain_ = c.clipAutoGain;
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    bool clipped = false;
    if (!on_ || mix_ <= 0.0001f) gain_ = 1.0f;
    double inPow = 0.0;
    for (int i = 0; i < frames; ++i)
      inPow += static_cast<double>(left[i]) * left[i] +
               static_cast<double>(right[i]) * right[i];

    if (on_) {
      int off = 0;
      while (off < frames) {
        const int n = std::min(frames - off, hdsp::Oversampler::kMaxBaseFrames);
        if (osFactor_ == 1) {
          std::copy(left + off, left + off + n, osL_.begin());
          std::copy(right + off, right + off + n, osR_.begin());
        } else {
          os_.up(left + off, right + off, n, osL_.data(), osR_.data());
        }
        const int nf = n * osFactor_;
        for (int i = 0; i < nf; ++i) {
          float l = osL_[static_cast<size_t>(i)] * driveGain_ / ceilingLin_;
          float r = osR_[static_cast<size_t>(i)] * driveGain_ / ceilingLin_;
          if (std::fabs(l) >= 1.0f || std::fabs(r) >= 1.0f) clipped = true;
          osL_[static_cast<size_t>(i)] = shapeSample(l, true) * ceilingLin_;
          osR_[static_cast<size_t>(i)] = shapeSample(r, false) * ceilingLin_;
        }
        if (osFactor_ == 1) {
          std::copy(osL_.begin(), osL_.begin() + n, wetL_.begin());
          std::copy(osR_.begin(), osR_.begin() + n, wetR_.begin());
        } else {
          os_.down(osL_.data(), osR_.data(), n, wetL_.data(), wetR_.data());
        }

        for (int i = 0; i < n; ++i) {
          dryL_[static_cast<size_t>(dryPos_)] = left[off + i];
          dryR_[static_cast<size_t>(dryPos_)] = right[off + i];
          const int dr = (dryPos_ - kMaxLatency + kDelaySize) % kDelaySize;
          const float dl = dryL_[static_cast<size_t>(dr)];
          const float drr = dryR_[static_cast<size_t>(dr)];
          dryPos_ = (dryPos_ + 1) % kDelaySize;

          padL_[static_cast<size_t>(padPos_)] = wetL_[static_cast<size_t>(i)];
          padR_[static_cast<size_t>(padPos_)] = wetR_[static_cast<size_t>(i)];
          const int pr = (padPos_ - pad_ + kDelaySize) % kDelaySize;
          const float wl = padL_[static_cast<size_t>(pr)];
          const float wr = padR_[static_cast<size_t>(pr)];
          padPos_ = (padPos_ + 1) % kDelaySize;

          left[off + i] = dl + (wl - dl) * mix_;
          right[off + i] = drr + (wr - drr) * mix_;
        }
        off += n;
      }
    } else {
      for (int i = 0; i < frames; ++i) {
        dryL_[static_cast<size_t>(dryPos_)] = left[i];
        dryR_[static_cast<size_t>(dryPos_)] = right[i];
        const int dr = (dryPos_ - kMaxLatency + kDelaySize) % kDelaySize;
        left[i] = dryL_[static_cast<size_t>(dr)];
        right[i] = dryR_[static_cast<size_t>(dr)];
        dryPos_ = (dryPos_ + 1) % kDelaySize;
      }
    }

    outPow_ = 0.0;
    for (int i = 0; i < frames; ++i)
      outPow_ += static_cast<double>(left[i]) * left[i] +
                 static_cast<double>(right[i]) * right[i];

    float target = 1.0f;
    if (autoGain_ && on_ && mix_ > 0.0001f && outPow_ > 1e-12)
      target = std::min(1.0f, static_cast<float>(std::sqrt(inPow / outPow_)));
    gain_ += (target - gain_) * 0.15f;
    if (gain_ != 1.0f) {
      for (int i = 0; i < frames; ++i) {
        left[i] *= gain_;
        right[i] *= gain_;
      }
    }

    if (clipped) ledHold_ = static_cast<int>(0.25f * sr_);
    meters.clipActive = ledHold_ > 0;
    if (ledHold_ > 0) ledHold_ -= frames;
  }

  // Global bypass keeps the clipper's fixed latency in the chain.
  void processBypass(float* left, float* right, int frames) noexcept {
    for (int i = 0; i < frames; ++i) {
      dryL_[static_cast<size_t>(dryPos_)] = left[i];
      dryR_[static_cast<size_t>(dryPos_)] = right[i];
      const int dr = (dryPos_ - kMaxLatency + kDelaySize) % kDelaySize;
      left[i] = dryL_[static_cast<size_t>(dr)];
      right[i] = dryR_[static_cast<size_t>(dr)];
      dryPos_ = (dryPos_ + 1) % kDelaySize;
    }
    ledHold_ = 0;
  }

  void reset() noexcept {
    dryL_.fill(0.0f);
    dryR_.fill(0.0f);
    padL_.fill(0.0f);
    padR_.fill(0.0f);
    dryPos_ = 0;
    padPos_ = 0;
    adL_.reset();
    adR_.reset();
    ledHold_ = 0;
    outPow_ = 0.0;
    gain_ = 1.0f;
  }

  int latencySamples() const noexcept { return kMaxLatency; }

private:
  static constexpr int kMaxLatency = 45;
  static constexpr int kDelaySize = 64;

  float shapeSample(float x, bool left) noexcept {
    if (shape_ == 1) return clipShapeValue(1, x);
    if (shape_ == 2) {
      constexpr double b = 0.22;
      const double tb = std::tanh(b);
      const double norm = 1.0 - tb;
      auto f = [tb](double v) { return std::tanh(v + b) - tb; };
      auto F = [tb](double v) {
        return hdsp::logCosh(v + b) - tb * v - hdsp::logCosh(b);
      };
      hdsp::Adaa& ad = left ? adL_ : adR_;
      const float y = ad.process(x, f, F) / static_cast<float>(norm);
      return std::clamp(y, -1.0f, 1.0f);
    }
    auto f = [](double v) { return std::tanh(v); };
    auto F = [](double v) { return hdsp::logCosh(v); };
    hdsp::Adaa& ad = left ? adL_ : adR_;
    return std::clamp(ad.process(x, f, F), -1.0f, 1.0f);
  }

  float sr_ = 48000.0f;
  bool on_ = false;
  bool autoGain_ = true;
  int shape_ = 0;
  int osFactor_ = 1;
  int pad_ = 45;
  float driveGain_ = 1.0f;
  float ceilingLin_ = 1.0f;
  float mix_ = 1.0f;
  double outPow_ = 0.0;
  float gain_ = 1.0f;
  int ledHold_ = 0;
  hdsp::Oversampler os_;
  hdsp::Adaa adL_, adR_;
  std::vector<float> osL_, osR_, wetL_, wetR_;
  std::array<float, kDelaySize> dryL_{}, dryR_{}, padL_{}, padR_{};
  int dryPos_ = 0;
  int padPos_ = 0;
};

} // namespace drum
