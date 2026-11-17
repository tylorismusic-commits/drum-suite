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
  P_COMP_ON,
  P_COMP_MODEL,
  P_COMP_THRESHOLD,
  P_COMP_RATIO,
  P_COMP_ATTACK,
  P_COMP_RELEASE,
  P_COMP_MAKEUP,
  P_COMP_MIX,
  P_COMP_SC_HPF,
  P_DRIVE_ON,
  P_DRIVE_MODEL,
  P_DRIVE_DRIVE,
  P_DRIVE_CHARACTER,
  P_DRIVE_TONE,
  P_DRIVE_MIX,
  P_DRIVE_AUTOGAIN,
  P_CLIP_ON,
  P_CLIP_DRIVE,
  P_CLIP_CEILING,
  P_CLIP_SHAPE,
  P_CLIP_MIX,
  P_CLIP_OVERSAMPLE,
  P_CLIP_AUTOGAIN,
  P_LIMIT_ON,
  P_LIMIT_CEILING,
  P_LIMIT_RELEASE,
  P_LIMIT_LOOKAHEAD,
  P_LIMIT_TRUEPEAK,
  P_LIMIT_MIX,
  P_SAMPLE_SENSITIVITY,
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

struct SuggestedRange {
  double lo = 0.0;
  double hi = -1.0;  // hi <= lo means no suggested range
};

inline SuggestedRange recommendedRange(uint32_t id) noexcept {
  switch (id) {
    case P_INPUT:
    case P_OUTPUT:
      return {-6.0, 6.0};
    case P_THRESHOLD:
      return {-45.0, -18.0};
    case P_DEBLEED:
      return {40.0, 90.0};
    case P_GHOST:
      return {30.0, 85.0};
    case P_SAMPLE_LEVEL:
      return {50.0, 100.0};
    case P_SAMPLE_DIRECT:
      return {40.0, 100.0};
    case P_SAMPLE_PITCH:
      return {-7.0, 7.0};
    case P_SAMPLE_START:
      return {0.0, 30.0};
    case P_SAMPLE_FADE:
      return {1.0, 50.0};
    case P_SAMPLE_SENSITIVITY:
      return {0.30, 0.80};
    case P_EQ_LOW:
    case P_EQ_LOW_MID:
    case P_EQ_HIGH_MID:
    case P_EQ_HIGH:
      return {0.42, 0.62};
    case P_EQ_HPF:
      return {0.0, 0.35};
    case P_EQ_LPF:
      return {0.55, 1.0};
    case P_COMP_THRESHOLD:
      return {-32.0, -12.0};
    case P_COMP_ATTACK:
      return {0.1, 0.6};
    case P_COMP_RELEASE:
      return {0.2, 0.7};
    case P_COMP_MAKEUP:
      return {0.45, 0.85};
    case P_COMP_MIX:
      return {0.50, 1.0};
    case P_COMP_SC_HPF:
      return {0.0, 0.50};
    case P_DRIVE_DRIVE:
      return {0.20, 0.65};
    case P_DRIVE_CHARACTER:
      return {0.30, 0.70};
    case P_DRIVE_TONE:
      return {0.35, 0.70};
    case P_DRIVE_MIX:
      return {0.10, 0.50};
    case P_CLIP_DRIVE:
      return {0.20, 0.60};
    case P_CLIP_CEILING:
      return {0.80, 0.95};
    case P_CLIP_MIX:
      return {0.10, 0.40};
    case P_LIMIT_CEILING:
      return {0.90, 1.0};
    case P_LIMIT_RELEASE:
      return {0.20, 0.70};
    case P_LIMIT_LOOKAHEAD:
      return {0.20, 0.80};
    case P_LIMIT_MIX:
      return {0.70, 1.0};
    default:
      return {};
  }
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
    {"comp_on", "Comp On", "Comp", 0.0, 1.0, 1.0, true, false, true, nullptr, {}},
    {"comp_model", "Model", "Comp", 0.0, 2.0, 0.0, true, false, false, nullptr,
     labels(kCompModelLabels)},
    {"comp_threshold", "Threshold", "Comp", -40.0, 0.0, -20.0, false, false,
     false, "dB", {}},
    {"comp_ratio", "Ratio", "Comp", 0.0, 8.0, 4.0, true, false, false, nullptr, {}},
    {"comp_attack", "Attack", "Comp", 0.0, 1.0, 0.25, false, false, false, "%", {}},
    {"comp_release", "Release", "Comp", 0.0, 1.0, 0.35, false, false, false, "%", {}},
    {"comp_makeup", "Makeup", "Comp", 0.0, 1.0, 0.5, false, false, false, "dB", {}},
    {"comp_mix", "Mix", "Comp", 0.0, 1.0, 1.0, false, false, false, "%", {}},
    {"comp_sc_hpf", "SC HPF", "Comp", 0.0, 1.0, 0.05, false, false, false, "Hz",
     {}},
    {"drive_on", "Drive On", "Drive", 0.0, 1.0, 1.0, true, false, true, nullptr,
     {}},
    {"drive_model", "Model", "Drive", 0.0, 4.0, 0.0, true, false, false, nullptr,
     labels(kDriveModelLabels)},
    {"drive_drive", "Drive", "Drive", 0.0, 1.0, 0.45, false, false, false, "%",
     {}},
    {"drive_character", "Character", "Drive", 0.0, 1.0, 0.55, false, false,
     false, "%", {}},
    {"drive_tone", "Tone", "Drive", 0.0, 1.0, 0.40, false, false, false, "%", {}},
    {"drive_mix", "Mix", "Drive", 0.0, 1.0, 0.35, false, false, false, "%", {}},
    {"drive_auto_gain", "Auto Gain", "Drive", 0.0, 1.0, 1.0, true, false, true,
     nullptr, {}},
    {"clip_on", "Clip On", "Clip", 0.0, 1.0, 1.0, true, false, true, nullptr, {}},
    {"clip_drive", "Drive", "Clip", 0.0, 1.0, 0.35, false, false, false, "%", {}},
    {"clip_ceiling", "Ceiling", "Clip", 0.0, 1.0, 0.85, false, false, false, "dB",
     {}},
    {"clip_shape", "Shape", "Clip", 0.0, 2.0, 0.0, true, false, false, nullptr,
     labels(kClipShapeLabels)},
    {"clip_mix", "Mix", "Clip", 0.0, 1.0, 0.25, false, false, false, "%", {}},
    {"clip_oversample", "Oversample", "Clip", 0.0, 2.0, 1.0, true, false, false,
     nullptr, labels(kOversampleLabels)},
    {"clip_auto_gain", "Auto Gain", "Clip", 0.0, 1.0, 1.0, true, false, true,
     nullptr, {}},
    {"limit_on", "Limit On", "Limit", 0.0, 1.0, 1.0, true, false, true, nullptr,
     {}},
    {"limit_ceiling", "Ceiling", "Limit", 0.0, 1.0, 0.967, false, false, false,
     "dB", {}},
    {"limit_release", "Release", "Limit", 0.0, 1.0, 0.5, false, false, false, "%",
     {}},
    {"limit_lookahead", "Lookahead", "Limit", 0.0, 1.0, 0.4, false, false, false,
     "ms", {}},
    {"limit_true_peak", "True Peak", "Limit", 0.0, 1.0, 1.0, true, false, true,
     nullptr, {}},
    {"limit_mix", "Mix", "Limit", 0.0, 1.0, 1.0, false, false, false, "%", {}},
    {"sample_sensitivity", "Sensitivity", "Sample", 0.0, 1.0, 0.5, false, false,
     false, "%", {}},
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
    compGrDb.store(0.0f);
    clipActive.store(false);
    sampleVoices.store(0);
    sampleActive.store(false);
    sampleRate.store(48000.0f);
    limitGrDb.store(0.0f);
    limitHistoryHead.store(0);
    for (int i = 0; i < kScopeSize; ++i) limitHistory[i].store(0.0f);
    compHistoryHead.store(0);
    scopeHead.store(0);
    for (int i = 0; i < kScopeSize; ++i) {
      scopeIn[i].store(-120.0f);
      scopeGate[i].store(0.0f);
      compHistory[i].store(0.0f);
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
  std::atomic<float> compGrDb;
  std::atomic<bool> clipActive;
  std::atomic<int> sampleVoices;
  std::atomic<bool> sampleActive;
  std::atomic<float> sampleRate;
  std::atomic<float> limitGrDb;
  std::atomic<float> limitHistory[kScopeSize];
  std::atomic<int> limitHistoryHead;
  std::atomic<float> compHistory[kScopeSize];
  std::atomic<int> compHistoryHead;
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
  const CompDefaults& c = kCompDefaults[drum];
  s.set(P_COMP_MODEL, c.model);
  s.set(P_COMP_THRESHOLD, c.thresholdDb);
  s.set(P_COMP_RATIO, c.ratio);
  s.set(P_COMP_ATTACK, c.attack);
  s.set(P_COMP_RELEASE, c.release);
  s.set(P_COMP_MAKEUP, c.makeup);
  s.set(P_COMP_MIX, c.mix);
  s.set(P_COMP_SC_HPF, c.scHpf);
  const DriveDefaults& d = kDriveDefaults[drum];
  s.set(P_DRIVE_MODEL, d.model);
  s.set(P_DRIVE_DRIVE, d.drive);
  s.set(P_DRIVE_CHARACTER, d.character);
  s.set(P_DRIVE_TONE, d.tone);
  s.set(P_DRIVE_MIX, d.mix);
  s.set(P_DRIVE_AUTOGAIN, d.autoGain ? 1.0 : 0.0);
  const ClipDefaults& cl = kClipDefaults[drum];
  s.set(P_CLIP_ON, cl.on ? 1.0 : 0.0);
  s.set(P_CLIP_SHAPE, cl.shape);
  s.set(P_CLIP_DRIVE, cl.drive);
  s.set(P_CLIP_CEILING, cl.ceiling);
  s.set(P_CLIP_MIX, cl.mix);
  s.set(P_CLIP_OVERSAMPLE, cl.oversample);
  s.set(P_CLIP_AUTOGAIN, cl.autoGain ? 1.0 : 0.0);
  s.set(P_LIMIT_ON, 1.0);
  s.set(P_LIMIT_CEILING, 0.967);
  s.set(P_LIMIT_RELEASE, 0.5);
  s.set(P_LIMIT_LOOKAHEAD, 0.4);
  s.set(P_LIMIT_TRUEPEAK, 1.0);
  s.set(P_LIMIT_MIX, 1.0);
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
  c.sampleSensitivity = static_cast<float>(s.get(P_SAMPLE_SENSITIVITY));
  c.eqLow = static_cast<float>(s.get(P_EQ_LOW));
  c.eqLowMid = static_cast<float>(s.get(P_EQ_LOW_MID));
  c.eqHighMid = static_cast<float>(s.get(P_EQ_HIGH_MID));
  c.eqHigh = static_cast<float>(s.get(P_EQ_HIGH));
  c.eqHpf = static_cast<float>(s.get(P_EQ_HPF));
  c.eqLpf = static_cast<float>(s.get(P_EQ_LPF));
  c.compOn = s.get(P_COMP_ON) > 0.5;
  c.compModel = static_cast<int>(s.get(P_COMP_MODEL) + 0.5);
  c.compThresholdDb = static_cast<float>(s.get(P_COMP_THRESHOLD));
  c.compRatio = static_cast<float>(s.get(P_COMP_RATIO));
  c.compAttack = static_cast<float>(s.get(P_COMP_ATTACK));
  c.compRelease = static_cast<float>(s.get(P_COMP_RELEASE));
  c.compMakeup = static_cast<float>(s.get(P_COMP_MAKEUP));
  c.compMix = static_cast<float>(s.get(P_COMP_MIX));
  c.compScHpf = static_cast<float>(s.get(P_COMP_SC_HPF));
  c.driveOn = s.get(P_DRIVE_ON) > 0.5;
  c.driveModel = static_cast<int>(s.get(P_DRIVE_MODEL) + 0.5);
  c.driveDrive = static_cast<float>(s.get(P_DRIVE_DRIVE));
  c.driveCharacter = static_cast<float>(s.get(P_DRIVE_CHARACTER));
  c.driveTone = static_cast<float>(s.get(P_DRIVE_TONE));
  c.driveMix = static_cast<float>(s.get(P_DRIVE_MIX));
  c.driveAutoGain = s.get(P_DRIVE_AUTOGAIN) > 0.5;
  c.clipOn = s.get(P_CLIP_ON) > 0.5;
  c.clipShape = static_cast<int>(s.get(P_CLIP_SHAPE) + 0.5);
  c.clipDrive = static_cast<float>(s.get(P_CLIP_DRIVE));
  c.clipCeiling = static_cast<float>(s.get(P_CLIP_CEILING));
  c.clipMix = static_cast<float>(s.get(P_CLIP_MIX));
  c.clipOversample = static_cast<int>(s.get(P_CLIP_OVERSAMPLE) + 0.5);
  c.clipAutoGain = s.get(P_CLIP_AUTOGAIN) > 0.5;
  c.limitOn = s.get(P_LIMIT_ON) > 0.5;
  c.limitCeiling = static_cast<float>(s.get(P_LIMIT_CEILING));
  c.limitRelease = static_cast<float>(s.get(P_LIMIT_RELEASE));
  c.limitLookahead = static_cast<float>(s.get(P_LIMIT_LOOKAHEAD));
  c.limitTruePeak = s.get(P_LIMIT_TRUEPEAK) > 0.5;
  c.limitMix = static_cast<float>(s.get(P_LIMIT_MIX));
  return c;
}

} // namespace drum
