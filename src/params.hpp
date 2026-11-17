// Drum Suite — params.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "dsp/controls.hpp"

namespace drum {

enum ParamId : uint32_t {
  P_POWER = 0,
  P_BYPASS,
  P_INPUT,
  P_OUTPUT,
  P_DRUM,
  P_GATE_ON,
  P_THRESHOLD,
  P_DEBLEED,
  P_GHOST_MODE,
  P_GHOST,
  P_GHOST_NOTE,
  P_GHOST_CC,
  P_SAMPLE_ON,
  P_SAMPLE_LEVEL,
  P_SAMPLE_DIRECT,
  P_SAMPLE_PITCH,
  P_SAMPLE_START,
  P_SAMPLE_REVERSE,
  P_SAMPLE_CHOKE,
  P_SAMPLE_FADE,
  P_SAMPLE_TRIGGER,
  P_EQ_LOW,
  P_EQ_LOW_MID,
  P_EQ_HIGH_MID,
  P_EQ_HIGH,
  P_EQ_HPF,
  P_EQ_LPF,
  P_COUNT
};

struct Labels {
  const char* const* names = nullptr;
  int count = 0;
};

template <size_t N>
constexpr Labels labels(const char* const (&arr)[N]) noexcept {
  return Labels{arr, static_cast<int>(N)};
}

struct ParamSpec {
  const char* symbol;
  const char* name;
  const char* module;
  double min;
  double max;
  double def;
  bool stepped;
  bool hidden;
  bool toggled;
  const char* unit;
  Labels labels;
};

inline constexpr const char* kDrumLabels[] = {"Kick", "Snare", "Tom", "Hat",
                                              "Ride", "Room", "OH", "Bus"};
inline constexpr const char* kSampleTriggerLabels[] = {"Gate", "Transient",
                                                       "Always"};


inline constexpr ParamSpec kParamSpecs[P_COUNT] = {
    {"power", "Power", "Global", 0.0, 1.0, 1.0, true, false, true, nullptr, {}},
    {"bypass", "Bypass", "Global", 0.0, 1.0, 0.0, true, false, true, nullptr, {}},
    {"input", "Input", "Global", -18.0, 18.0, 0.0, false, false, false, "dB", {}},
    {"output", "Output", "Global", -18.0, 18.0, 0.0, false, false, false, "dB", {}},

    {"drum", "Drum", "Profile", 0.0, 7.0, 0.0, true, false, false, nullptr,
     labels(kDrumLabels)},

    {"gate_on", "Gate On", "Gate", 0.0, 1.0, 1.0, true, false, true, nullptr, {}},
    {"threshold", "Threshold", "Gate", -60.0, 0.0, -30.0, false, false, false,
     "dB", {}},
    {"debleed", "DeBleed", "Gate", 0.0, 100.0, 70.0, false, false, false, "%",
     {}},

    {"ghost_mode", "Ghost Mode", "Ghost", 0.0, 1.0, 0.0, true, false, true,
     nullptr, {}},
    {"ghost", "Ghost", "Ghost", 0.0, 100.0, 75.0, false, false, false, "%", {}},
    {"ghost_note", "Ghost Note", "MIDI", 0.0, 127.0, 38.0, true, false, false,
     nullptr, {}},
    {"ghost_cc", "Ghost CC", "MIDI", 0.0, 127.0, 74.0, true, false, false,
     nullptr, {}},
    {"sample_on", "Sample", "Sample", 0.0, 1.0, 0.0, true, false, true, nullptr, {}},
    {"sample_level", "Sample Level", "Sample", 0.0, 100.0, 80.0, false, false,
     false, "%", {}},
    {"sample_direct", "Direct", "Sample", 0.0, 100.0, 100.0, false, false,
     false, "%", {}},
    {"sample_pitch", "Pitch", "Sample", -24.0, 24.0, 0.0, false, false, false,
     "st", {}},
    {"sample_start", "Start", "Sample", 0.0, 100.0, 0.0, false, false, false,
     "%", {}},
    {"sample_reverse", "Reverse", "Sample", 0.0, 1.0, 0.0, true, false, true,
     nullptr, {}},
    {"sample_choke", "Choke", "Sample", 0.0, 1.0, 1.0, true, false, true,
     nullptr, {}},
    {"sample_fade", "Fade", "Sample", 0.0, 500.0, 3.0, false, false, false,
     "ms", {}},
    {"sample_trigger", "Trigger", "Sample", 0.0, 2.0, 0.0, true, false, false,
     nullptr, labels(kSampleTriggerLabels)},
    {"eq_low", "Low", "EQ", 0.0, 1.0, 0.5, false, false, false, "dB", {}},
    {"eq_low_mid", "Low Mid", "EQ", 0.0, 1.0, 0.5, false, false, false, "dB", {}},
    {"eq_high_mid", "High Mid", "EQ", 0.0, 1.0, 0.5, false, false, false, "dB", {}},
    {"eq_high", "High", "EQ", 0.0, 1.0, 0.5, false, false, false, "dB", {}},
    {"eq_hpf", "HPF", "EQ", 0.0, 1.0, 0.0, false, false, false, "Hz", {}},
    {"eq_lpf", "LPF", "EQ", 0.0, 1.0, 1.0, false, false, false, "Hz", {}},
};

static_assert(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]) == P_COUNT,
              "kParamSpecs must cover every parameter id");

class Shared {
public:
  static constexpr int kScopeSize = 128;

  Shared() {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      values_[p].store(kParamSpecs[p].def, std::memory_order_relaxed);
      dirty_[p].store(false, std::memory_order_relaxed);
    }
    inPeakDb.store(-120.0f);
    outPeakDb.store(-120.0f);
    inPeakLDb.store(-120.0f);
    inPeakRDb.store(-120.0f);
    outPeakLDb.store(-120.0f);
    outPeakRDb.store(-120.0f);
    grDb.store(0.0f);
    detectorDb.store(-120.0f);
    gate.store(1.0f);
    ghostActive.store(false);
    scopeHead.store(0);
    for (int i = 0; i < kScopeSize; ++i) {
      scopeIn[i].store(-120.0f);
      scopeGate[i].store(0.0f);
    }
  }

  double get(uint32_t p) const noexcept {
    return values_[p].load(std::memory_order_relaxed);
  }

  void set(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) return;
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    values_[p].store(v, std::memory_order_relaxed);
    dirty_[p].store(true, std::memory_order_release);
  }

  void setFromHost(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) return;
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    values_[p].store(v, std::memory_order_relaxed);
  }

  bool takeDirty(uint32_t p) noexcept {
    return dirty_[p].exchange(false, std::memory_order_acquire);
  }

  bool isDirty(uint32_t p) const noexcept {
    return dirty_[p].load(std::memory_order_acquire);
  }

  bool anyDirty() const noexcept {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      if (dirty_[p].load(std::memory_order_acquire)) return true;
    }
    return false;
  }

  std::shared_ptr<const SampleBuffer> sample() const noexcept {
    return sample_.load();
  }

  void setSample(std::shared_ptr<const SampleBuffer> s) noexcept {
    sample_.store(std::move(s));
  }

  uint32_t takeGhostTrigger() noexcept {
    return midiGhostTrigger.exchange(0, std::memory_order_acq_rel);
  }

  std::atomic<float> inPeakDb, outPeakDb;
  std::atomic<float> inPeakLDb, inPeakRDb;
  std::atomic<float> outPeakLDb, outPeakRDb;
  std::atomic<float> grDb, detectorDb, gate;
  std::atomic<bool> ghostActive;
  std::atomic<uint32_t> midiGhostTrigger{0};
  std::atomic<float> scopeIn[kScopeSize];
  std::atomic<float> scopeGate[kScopeSize];
  std::atomic<int> scopeHead;
  std::atomic<std::shared_ptr<const SampleBuffer>> sample_;

private:
  std::atomic<double> values_[P_COUNT];
  std::atomic<bool> dirty_[P_COUNT];
};

inline void applyDrumDefaults(Shared& s, int drum) noexcept {
  drum = std::clamp(drum, 0, kDrumCount - 1);
  // Use set() so the host sees the profile change and does not overwrite it
  // with its cached parameter values.
  s.set(P_THRESHOLD, kProfiles[drum].thresholdDb);
  s.set(P_DEBLEED, kProfiles[drum].deBleed);
}

inline Controls controlsFrom(Shared& s) noexcept {
  Controls c;
  c.drum = static_cast<int>(s.get(P_DRUM) + 0.5);
  c.power = s.get(P_POWER) > 0.5;
  c.bypass = s.get(P_BYPASS) > 0.5;
  c.inputDb = static_cast<float>(s.get(P_INPUT));
  c.outputDb = static_cast<float>(s.get(P_OUTPUT));
  c.gateOn = s.get(P_GATE_ON) > 0.5;
  c.thresholdDb = static_cast<float>(s.get(P_THRESHOLD));
  c.deBleed = static_cast<float>(s.get(P_DEBLEED));
  c.ghostMode = s.get(P_GHOST_MODE) > 0.5;
  c.ghost = static_cast<float>(s.get(P_GHOST));
  c.ghostNote = static_cast<int>(s.get(P_GHOST_NOTE) + 0.5);
  c.ghostCc = static_cast<int>(s.get(P_GHOST_CC) + 0.5);
  c.ghostTrigger = static_cast<int>(s.takeGhostTrigger());
  c.sampleOn = s.get(P_SAMPLE_ON) > 0.5;
  c.sampleLevel = static_cast<float>(s.get(P_SAMPLE_LEVEL));
  c.sampleDirect = static_cast<float>(s.get(P_SAMPLE_DIRECT));
  c.samplePitch = static_cast<float>(s.get(P_SAMPLE_PITCH));
  c.sampleStart = static_cast<float>(s.get(P_SAMPLE_START));
  c.sampleReverse = s.get(P_SAMPLE_REVERSE) > 0.5;
  c.sampleChoke = s.get(P_SAMPLE_CHOKE) > 0.5;
  c.sampleFadeMs = static_cast<float>(s.get(P_SAMPLE_FADE));
  c.sampleTrigger = static_cast<int>(s.get(P_SAMPLE_TRIGGER) + 0.5);
  c.eqLow = static_cast<float>(s.get(P_EQ_LOW));
  c.eqLowMid = static_cast<float>(s.get(P_EQ_LOW_MID));
  c.eqHighMid = static_cast<float>(s.get(P_EQ_HIGH_MID));
  c.eqHigh = static_cast<float>(s.get(P_EQ_HIGH));
  c.eqHpf = static_cast<float>(s.get(P_EQ_HPF));
  c.eqLpf = static_cast<float>(s.get(P_EQ_LPF));
  return c;
}

} // namespace drum
