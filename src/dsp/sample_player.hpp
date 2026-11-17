// Drum Suite — sample player
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Four-voice one-shot sample player.  The Bleed Out gate supplies a per-sample
// trigger array; the player adds the sample after the gate and applies the
// direct/sample balance.

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>

#include "controls.hpp"

namespace drum {

class SamplePlayer {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    reset();
  }

  void reset() noexcept {
    for (auto& v : voices_) v = Voice{};
    nextVoice_ = 0;
    prevPeak_ = 0.0f;
  }

  void setSample(std::shared_ptr<const SampleBuffer> sample) noexcept {
    sample_ = std::move(sample);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.sampleOn;
    level_ = std::clamp(c.sampleLevel * 0.01f, 0.0f, 2.0f);
    direct_ = std::clamp(c.sampleDirect * 0.01f, 0.0f, 1.0f);
    pitch_ = c.samplePitch;
    start_ = std::clamp(c.sampleStart * 0.01f, 0.0f, 1.0f);
    reverse_ = c.sampleReverse;
    choke_ = c.sampleChoke;
    fadeSamples_ = std::max(1, static_cast<int>(c.sampleFadeMs * 0.001f * sr_));
    triggerMode_ = c.sampleTrigger;
  }

  void process(float* left, float* right, const uint8_t* trigger, int frames,
               Meters& meters) noexcept {
    meters.sampleVoices = 0;
    meters.sampleTriggered = false;
    if (!on_ || !sample_ || sample_->data.empty()) {
      return;
    }
    const auto& data = sample_->data;
    const int len = static_cast<int>(data.size());
    const float rate = static_cast<float>(sample_->sampleRate) / sr_;
    const float stepBase = std::pow(2.0f, pitch_ / 12.0f) * rate;

    for (int i = 0; i < frames; ++i) {
      float peak = std::max(std::fabs(left[i]), std::fabs(right[i]));
      bool fire = trigger && trigger[i];
      if (triggerMode_ == 2) {
        fire = peak > 0.012f && prevPeak_ <= 0.012f;
      }
      prevPeak_ = peak;
      if (fire) {
        triggerVoice(len, stepBase);
        meters.sampleTriggered = true;
      }
      left[i] *= direct_;
      right[i] *= direct_;
      float sum = 0.0f;
      for (auto& v : voices_) {
        if (!v.active) continue;
        const float s = read(data, len, v.pos);
        float env = 1.0f;
        if (v.fadeIn > 0) {
          env *= 1.0f - static_cast<float>(v.fadeIn) /
                             static_cast<float>(fadeSamples_);
          --v.fadeIn;
        }
        const int remaining = reverse_ ? v.pos : (len - 1 - v.pos);
        if (remaining < fadeSamples_) {
          const float f = static_cast<float>(std::max(0, remaining)) /
                          static_cast<float>(fadeSamples_);
          env *= f;
        }
        sum += s * env;
        v.pos += v.step;
        if (v.pos < 0.0f || v.pos >= static_cast<float>(len - 1)) {
          v.active = false;
        }
      }
      const float sampleOut = sum * level_;
      left[i] += sampleOut;
      right[i] += sampleOut;
    }
    for (const auto& v : voices_) {
      if (v.active) ++meters.sampleVoices;
    }
  }

private:
  struct Voice {
    bool active = false;
    float pos = 0.0f;
    float step = 1.0f;
    int fadeIn = 0;
  };

  static float read(const std::vector<float>& data, int len, float pos) noexcept {
    if (len <= 0) return 0.0f;
    if (pos <= 0.0f) return data[0];
    const int i0 = static_cast<int>(pos);
    if (i0 >= len - 1) return data[len - 1];
    const float frac = pos - static_cast<float>(i0);
    return data[static_cast<size_t>(i0)] * (1.0f - frac) +
           data[static_cast<size_t>(i0 + 1)] * frac;
  }

  void triggerVoice(int len, float stepBase) noexcept {
    if (choke_) {
      for (auto& v : voices_) v.active = false;
    }
    Voice& v = voices_[static_cast<size_t>(nextVoice_)];
    nextVoice_ = (nextVoice_ + 1) % 4;
    v.active = true;
    v.step = reverse_ ? -stepBase : stepBase;
    v.pos = reverse_ ? static_cast<float>(len - 1) -
                           start_ * static_cast<float>(len - 1)
                     : start_ * static_cast<float>(len - 1);
    v.fadeIn = fadeSamples_;
  }

  float sr_ = 48000.0f;
  bool on_ = false;
  float level_ = 0.8f;
  float direct_ = 1.0f;
  float pitch_ = 0.0f;
  float start_ = 0.0f;
  bool reverse_ = false;
  bool choke_ = true;
  int fadeSamples_ = 144;
  int triggerMode_ = 0;
  float prevPeak_ = 0.0f;
  int nextVoice_ = 0;
  std::shared_ptr<const SampleBuffer> sample_;
  Voice voices_[4];
};

} // namespace drum
