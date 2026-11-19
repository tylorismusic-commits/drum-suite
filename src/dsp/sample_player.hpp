// Drum Suite — multisample player
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thirty-two-voice one-shot/multisample player.  The Bleed Out gate supplies a
// per-sample trigger array; the player adds the selected instrument after the
// gate and applies the direct/sample balance.  Zones are chosen by MIDI key
// and velocity, with SFZ/SF2 round-robin and choke groups.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "controls.hpp"

namespace drum {

class SamplePlayer {
public:
  static constexpr int kVoices = 32;

  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    pendingSize_ = std::max(1, static_cast<int>(0.010f * sr_) + 2);
    pending_.assign(static_cast<size_t>(pendingSize_), 0);
    pendingKey_.assign(static_cast<size_t>(pendingSize_), 36);
    pendingVel_.assign(static_cast<size_t>(pendingSize_), 100);
    reset();
  }

  void reset() noexcept {
    for (auto& v : voices_) v = Voice{};
    nextVoice_ = 0;
    prevPeak_ = 0.0f;
    pendingPos_ = 0;
    pendingMidi_ = 0;
    rr_ = 0;
    rng_ = 0x1234567u;
    prevGate_ = false;
    if (!pending_.empty()) {
      std::fill(pending_.begin(), pending_.end(), 0);
    }
    zoneSeq_.clear();
  }

  void setInstrument(std::shared_ptr<const Instrument> instrument) noexcept {
    instrument_ = std::move(instrument);
    reset();
    if (instrument_) zoneSeq_.assign(instrument_->zones.size(), 0);
  }

  void setSample(std::shared_ptr<const SampleBuffer> sample) noexcept {
    if (!sample) {
      setInstrument(nullptr);
      return;
    }
    auto instrument = std::make_shared<Instrument>();
    Zone z;
    z.sample = std::move(sample);
    instrument->zones.push_back(std::move(z));
    instrument->type = "wav";
    instrument->name = instrument->zones.front().sample->name;
    instrument->path = instrument->zones.front().sample->path;
    setInstrument(std::move(instrument));
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
    triggerThreshold_ =
        0.004f + 0.12f * (1.0f - std::clamp(c.sampleSensitivity, 0.0f, 1.0f));
    offsetSamples_ = std::clamp(
        static_cast<int>(c.sampleOffsetMs * 0.001f * sr_), 0,
        std::max(0, pendingSize_ - 1));
    note_ = std::clamp(c.sampleNote, 0, 127);
    velocity_ = std::clamp(c.sampleMidiVelocity, 1, 127);
    if (!on_) {
      pendingMidi_ = 0;
      if (!pending_.empty()) std::fill(pending_.begin(), pending_.end(), 0);
    } else if (triggerMode_ == 2 || triggerMode_ == 3) {
      pendingMidi_ =
          std::min(8, pendingMidi_ + std::max(0, c.sampleMidiTrigger));
    } else {
      pendingMidi_ = 0;
    }
  }

  void process(float* left, float* right, const uint8_t* trigger, int frames,
               Meters& meters) noexcept {
    meters.sampleVoices = 0;
    meters.sampleTriggered = false;
    if (!on_ || !instrument_ || instrument_->zones.empty()) {
      pendingMidi_ = 0;
      return;
    }

    for (int i = 0; i < frames; ++i) {
      const float peak = std::max(std::fabs(left[i]), std::fabs(right[i]));
      const bool gateFire = trigger && trigger[i];
      const bool gateEdge = gateFire && !prevGate_;
      prevGate_ = gateFire;
      const bool transientFire =
          peak > triggerThreshold_ && prevPeak_ <= triggerThreshold_;
      bool fire = false;
      switch (triggerMode_) {
        case 0: fire = gateEdge; break;
        case 1: fire = transientFire; break;
        case 2: fire = gateEdge || transientFire; break;
        default: fire = false; break;
      }
      if (pendingMidi_ > 0 &&
          (triggerMode_ == 2 || triggerMode_ == 3)) {
        scheduleTrigger(note_, velocity_);
        --pendingMidi_;
      }
      if (fire) {
        const float scaled = std::clamp(peak * 140.0f, 1.0f, 127.0f);
        scheduleTrigger(note_, static_cast<int>(scaled));
      }
      const bool due = pending_[static_cast<size_t>(pendingPos_)] != 0;
      const int dueKey = pendingKey_[static_cast<size_t>(pendingPos_)];
      const int dueVel = pendingVel_[static_cast<size_t>(pendingPos_)];
      pending_[static_cast<size_t>(pendingPos_)] = 0;
      pendingPos_ = (pendingPos_ + 1) % pendingSize_;
      prevPeak_ = peak;
      if (due) {
        triggerZones(dueKey, dueVel);
        meters.sampleTriggered = true;
      }

      left[i] *= direct_;
      right[i] *= direct_;
      float outL = 0.0f;
      float outR = 0.0f;
      for (auto& v : voices_) {
        if (!v.active) continue;
        const SampleBuffer& s = *v.sample;
        const size_t len = s.data.size();
        if (len == 0) {
          v.active = false;
          continue;
        }
        float sl = 0.0f;
        float sr = 0.0f;
        readSample(s, v.pos, &sl, &sr);
        float env = 1.0f;
        if (v.fadeIn > 0) {
          env *= 1.0f - static_cast<float>(v.fadeIn) /
                             static_cast<float>(fadeSamples_);
          --v.fadeIn;
        }
        const float remaining = v.step >= 0.0f
                                    ? (v.endPos - v.pos)
                                    : (v.pos - v.startPos);
        if (remaining < fadeSamples_) {
          env *= std::max(0.0f, remaining) /
                 static_cast<float>(fadeSamples_);
        }
        outL += sl * env * v.gainL;
        outR += sr * env * v.gainR;
        v.pos += v.step;
        if (v.loop && s.loopStart >= 0 && s.loopEnd > s.loopStart) {
          const float loopStart = std::max(
              v.startPos, static_cast<float>(s.loopStart));
          const float loopEnd =
              std::min(v.endPos, static_cast<float>(s.loopEnd));
          if (loopEnd > loopStart) {
            if (v.step > 0.0f) {
              while (v.pos >= loopEnd) v.pos -= (loopEnd - loopStart);
            } else {
              while (v.pos < loopStart) v.pos += (loopEnd - loopStart);
            }
          }
        }
        if (v.pos < v.startPos || v.pos >= v.endPos) {
          v.active = false;
        }
      }
      left[i] += outL * level_;
      right[i] += outR * level_;
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
    int group = 0;
    float gainL = 1.0f;
    float gainR = 1.0f;
    float startPos = 0.0f;
    float endPos = 1.0f;
    bool loop = false;
    std::shared_ptr<const SampleBuffer> sample;
  };

  static void readSample(const SampleBuffer& s, float pos, float* l,
                         float* r) noexcept {
    const size_t len = s.data.size();
    if (len == 0) {
      *l = 0.0f;
      *r = 0.0f;
      return;
    }
    if (pos <= 0.0f) {
      *l = s.data[0];
      *r = s.dataR.empty() ? *l : s.dataR[0];
      return;
    }
    const size_t i0 = static_cast<size_t>(pos);
    if (i0 >= len - 1) {
      *l = s.data[len - 1];
      *r = s.dataR.empty() ? *l : s.dataR[len - 1];
      return;
    }
    const float frac = pos - static_cast<float>(i0);
    *l = s.data[i0] * (1.0f - frac) + s.data[i0 + 1] * frac;
    if (s.dataR.empty()) {
      *r = *l;
    } else {
      *r = s.dataR[i0] * (1.0f - frac) + s.dataR[i0 + 1] * frac;
    }
  }

  uint32_t random() noexcept {
    rng_ = rng_ * 1664525u + 1013904223u;
    return rng_;
  }

  void scheduleTrigger(int key, int velocity) noexcept {
    if (pending_.empty()) return;
    const int index = (pendingPos_ + offsetSamples_) % pendingSize_;
    pending_[static_cast<size_t>(index)] = 1;
    pendingKey_[static_cast<size_t>(index)] = key;
    pendingVel_[static_cast<size_t>(index)] = velocity;
  }

  void triggerZones(int key, int velocity) noexcept {
    if (!instrument_) return;
    ++rr_;
    if (choke_) {
      for (auto& v : voices_) v.active = false;
    }

    struct Match {
      size_t index = 0;
      bool offBy = false;
      int group = 0;
    };
    std::vector<Match> matches;
    matches.reserve(instrument_->zones.size());
    for (size_t i = 0; i < instrument_->zones.size(); ++i) {
      const Zone& z = instrument_->zones[i];
      if (!z.sample) continue;
      if (key < z.lokey || key > z.hikey) continue;
      if (velocity < z.lovel || velocity > z.hivel) continue;
      if (z.loRand > 0 || z.hiRand < 127) {
        const int r = static_cast<int>(random() % 128u);
        if (r < z.loRand || r > z.hiRand) continue;
      }
      if (zoneSeq_.size() <= i) zoneSeq_.resize(i + 1, 0);
      ++zoneSeq_[i];
      if (z.seqLength > 1) {
        const int pos = static_cast<int>((zoneSeq_[i] - 1) % z.seqLength) + 1;
        if (pos != z.seqPosition) continue;
      }
      matches.push_back({i, z.offBy != 0, z.group});
    }

    // Melodic SFZ/SF2 files often have key ranges around middle C.  If the
    // configured note falls outside every zone, play the first zone rather
    // than silently producing nothing.
    if (matches.empty()) {
      for (size_t i = 0; i < instrument_->zones.size(); ++i) {
        if (instrument_->zones[i].sample) {
          matches.push_back({i, false, instrument_->zones[i].group});
          break;
        }
      }
    }

    for (const auto& m : matches) {
      const Zone& z = instrument_->zones[m.index];
      if (z.offBy != 0) {
        for (auto& v : voices_) {
          if (v.active && v.group == z.offBy) v.active = false;
        }
      }
      Voice& v = voices_[static_cast<size_t>(nextVoice_)];
      nextVoice_ = (nextVoice_ + 1) % kVoices;
      v = Voice{};
      v.active = true;
      v.sample = z.sample;
      v.group = z.group;
      v.loop = z.loop;
      const int root = z.rootKey >= 0
                           ? z.rootKey
                           : (z.sample->rootKey >= 0 ? z.sample->rootKey : key);
      const float semis = static_cast<float>(key - root) + pitch_;
      const float cents = z.tuneCents;
      const float rate = static_cast<float>(z.sample->sampleRate) / sr_;
      const float stepBase =
          std::pow(2.0f, (semis * 100.0f + cents) / 1200.0f) * rate;
      const size_t len = z.sample->data.size();
      v.step = (reverse_ != z.reverse) ? -stepBase : stepBase;
      const float z0 = std::clamp(z.start, 0.0f, 1.0f) *
                       static_cast<float>(len > 0 ? len - 1 : 0);
      const float z1 = std::clamp(z.end, 0.0001f, 1.0f) *
                       static_cast<float>(len > 0 ? len - 1 : 0);
      const float zoneSpan = std::max(1.0f, z1 - z0);
      const float startPos = z0 + start_ * zoneSpan;
      v.pos = v.step >= 0.0f ? startPos
                             : z0 + z1 - startPos;
      v.startPos = z0;
      v.endPos = z1;
      v.fadeIn = fadeSamples_;
      const float gain = std::pow(10.0f, z.gainDb / 20.0f);
      const float pan = std::clamp(z.pan, -1.0f, 1.0f);
      const float angle = (pan + 1.0f) * 0.25f * 3.14159265358979f;
      v.gainL = std::cos(angle) * gain;
      v.gainR = std::sin(angle) * gain;
    }
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
  int offsetSamples_ = 0;
  int pendingMidi_ = 0;
  int pendingPos_ = 0;
  int pendingSize_ = 1;
  int note_ = 36;
  int velocity_ = 100;
  uint32_t rr_ = 0;
  uint32_t rng_ = 0x1234567u;
  std::vector<uint8_t> pending_;
  std::vector<int> pendingKey_;
  std::vector<int> pendingVel_;
  float triggerThreshold_ = 0.02f;
  float prevPeak_ = 0.0f;
  bool prevGate_ = false;
  int nextVoice_ = 0;
  std::vector<uint32_t> zoneSeq_;
  std::shared_ptr<const Instrument> instrument_;
  Voice voices_[kVoices];
};

} // namespace drum
