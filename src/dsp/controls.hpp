// Drum Suite — controls.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace drum {

// Drum profiles automate the parts of the gate that should not need a knob:
// reduction, decay/length, attack, hold, hysteresis, multiband tone and
// saturation.  The user only gets Threshold, DeBleed and Ghost.
struct Profile {
  const char* name;
  float lowXover;
  float highXover;
  float lowWeight;
  float midWeight;
  float highWeight;
  float thresholdDb;
  float deBleed;
  // The de-bleed sidechain has its own timing: keep the high band open for the
  // attack, then close it quickly so the cymbal wash does not ride the drum's
  // full gate release.
  float deBleedHoldMs;
  float deBleedReleaseMs;
  float reductionDb;
  float attackMs;
  float holdMs;
  float lengthMs;
  float hysteresisDb;
  float lowGainDb;
  float midGainDb;
  float highGainDb;
  float lowDrive;
  float midDrive;
  float highDrive;
  float satMix;
};

inline constexpr Profile kProfiles[8] = {
    // name, lowX, highX, lowW, midW, highW, thresh, debleed,
    // debleedHold, debleedRelease, reduction, attack, hold, length, hyst,
    // lowG, midG, highG, lowDrive, midDrive, highDrive, satMix
    {"Kick", 180.0f, 2000.0f, 0.80f, 0.15f, 0.05f, -30.0f, 70.0f,
     8.0f, 30.0f, 60.0f, 0.5f, 5.0f, 80.0f, 6.0f, 1.0f, -1.0f, 0.0f,
     10.0f, 0.0f, 0.0f, 20.0f},
    {"Snare", 300.0f, 5000.0f, 0.25f, 0.60f, 0.15f, -32.0f, 80.0f,
     4.0f, 16.0f, 65.0f, 0.8f, 12.0f, 110.0f, 6.0f, -0.5f, 1.0f, 0.5f,
     0.0f, 10.0f, 0.0f, 25.0f},
    {"Tom", 350.0f, 3000.0f, 0.60f, 0.30f, 0.10f, -32.0f, 60.0f,
     10.0f, 35.0f, 45.0f, 1.0f, 15.0f, 160.0f, 8.0f, 1.0f, 0.0f, 0.0f,
     5.0f, 5.0f, 0.0f, 20.0f},
    {"Hat", 1000.0f, 5000.0f, 0.05f, 0.25f, 0.70f, -34.0f, 30.0f,
     4.0f, 15.0f, 40.0f, 0.4f, 4.0f, 45.0f, 6.0f, 0.0f, 0.0f, 0.5f,
     0.0f, 0.0f, 5.0f, 10.0f},
    {"Ride", 1000.0f, 5000.0f, 0.05f, 0.30f, 0.65f, -34.0f, 20.0f,
     5.0f, 20.0f, 30.0f, 0.5f, 8.0f, 80.0f, 6.0f, 0.0f, 0.0f, 0.5f,
     0.0f, 0.0f, 5.0f, 10.0f},
    {"Room", 300.0f, 6000.0f, 0.20f, 0.50f, 0.30f, -36.0f, 20.0f,
     15.0f, 60.0f, 20.0f, 2.0f, 30.0f, 300.0f, 8.0f, 0.0f, 0.5f, 0.0f,
     0.0f, 0.0f, 0.0f, 10.0f},
    {"OH", 300.0f, 8000.0f, 0.10f, 0.45f, 0.45f, -36.0f, 20.0f,
     15.0f, 60.0f, 15.0f, 2.0f, 30.0f, 300.0f, 8.0f, 0.0f, 0.0f, 0.5f,
     0.0f, 0.0f, 0.0f, 10.0f},
    {"Bus", 250.0f, 6000.0f, 0.30f, 0.40f, 0.30f, -40.0f, 0.0f,
     30.0f, 100.0f, 10.0f, 5.0f, 50.0f, 400.0f, 8.0f, 0.5f, 0.5f, 0.5f,
     0.0f, 0.0f, 0.0f, 15.0f},
};

inline constexpr int kDrumCount = 8;

inline constexpr const char* kCompModelLabels[] = {"76", "2A", "VCA"};
inline constexpr const char* kDriveModelLabels[] = {"VALVE", "CONSOLE", "BUS",
                                                    "TAPE", "TRANSFORMER"};
inline constexpr const char* kClipShapeLabels[] = {"SOFT", "HARD", "DIODE"};
inline constexpr const char* kOversampleLabels[] = {"OFF", "2X", "4X"};

struct CompDefaults {
  int model;
  float thresholdDb;
  float ratio;
  float attack;
  float release;
  float makeup;
  float mix;
  float scHpf;
};

struct DriveDefaults {
  int model;
  float drive;
  float character;
  float tone;
  float mix;
  bool autoGain;
};

inline constexpr DriveDefaults kDriveDefaults[8] = {
    {0, 0.45f, 0.55f, 0.40f, 0.35f, true},  // Kick
    {1, 0.50f, 0.60f, 0.55f, 0.45f, true},  // Snare
    {0, 0.40f, 0.50f, 0.45f, 0.35f, true},  // Tom
    {4, 0.30f, 0.40f, 0.60f, 0.25f, true},  // Hat
    {1, 0.30f, 0.45f, 0.60f, 0.20f, true},  // Ride
    {3, 0.50f, 0.55f, 0.50f, 0.35f, true},  // Room
    {3, 0.40f, 0.50f, 0.55f, 0.30f, true},  // OH
    {2, 0.45f, 0.55f, 0.50f, 0.30f, true},  // Bus
};

struct ClipDefaults {
  bool on;
  int shape;
  float drive;
  float ceiling;
  float mix;
  int oversample;
  bool autoGain;
};

inline constexpr ClipDefaults kClipDefaults[8] = {
    {true, 0, 0.35f, 0.85f, 0.25f, 1, true},  // Kick
    {true, 2, 0.45f, 0.85f, 0.30f, 2, true},  // Snare
    {true, 0, 0.30f, 0.85f, 0.20f, 1, true},  // Tom
    {false, 0, 0.20f, 0.90f, 0.00f, 1, true}, // Hat
    {false, 0, 0.20f, 0.90f, 0.00f, 1, true}, // Ride
    {true, 0, 0.35f, 0.85f, 0.20f, 1, true},  // Room
    {false, 0, 0.25f, 0.90f, 0.00f, 1, true}, // OH
    {true, 2, 0.40f, 0.85f, 0.30f, 2, true},  // Bus
};

inline constexpr CompDefaults kCompDefaults[8] = {
    {0, -14.0f, 3.0f, 0.25f, 0.35f, 0.80f, 1.00f, 0.05f},  // Kick
    {0, -16.0f, 3.0f, 0.15f, 0.40f, 0.80f, 0.90f, 0.25f},  // Snare
    {0, -18.0f, 3.0f, 0.30f, 0.45f, 0.80f, 0.90f, 0.20f},  // Tom
    {1, -20.0f, 2.0f, 0.50f, 0.50f, 0.40f, 0.70f, 0.60f},  // Hat
    {1, -20.0f, 2.0f, 0.50f, 0.50f, 0.40f, 0.70f, 0.60f},  // Ride
    {2, -20.0f, 3.0f, 0.85f, 0.35f, 0.42f, 1.00f, 0.65f},  // Room
    {2, -22.0f, 3.0f, 0.82f, 0.40f, 0.45f, 0.90f, 0.55f},  // OH
    {2, -20.0f, 3.0f, 0.80f, 0.30f, 0.40f, 1.00f, 0.70f},  // Bus
};


struct TransientDefaults {
  bool on;
  float attack;
  float sustain;
  float sensitivity;
};

inline constexpr TransientDefaults kTransientDefaults[8] = {
    {true, 25.0f, 10.0f, 50.0f},   // Kick
    {true, 35.0f, -10.0f, 55.0f},  // Snare
    {true, 25.0f, 5.0f, 50.0f},    // Tom
    {false, 15.0f, -10.0f, 50.0f}, // Hat
    {false, 10.0f, -5.0f, 50.0f},  // Ride
    {true, 20.0f, 10.0f, 50.0f},   // Room
    {true, 15.0f, 5.0f, 50.0f},    // OH
    {true, 15.0f, 5.0f, 50.0f},    // Bus
};

struct EqDefaults {
  float low;
  float lowMid;
  float highMid;
  float high;
  float hpf;
  float lpf;
};

inline constexpr EqDefaults kEqDefaults[8] = {
    // Kick, snare, tom, hat, ride, room, OH, bus.  Starting points follow the
    // usual rock/metal drum cheat-sheet ranges: kick 30-60 Hz HPF and 3-5 kHz
    // click, snare 80-120 Hz HPF and 4-6 kHz crack, cymbals 180-300 Hz HPF.
    {0.60f, 0.45f, 0.55f, 0.57f, 0.14f, 0.98f},  // Kick
    {0.47f, 0.58f, 0.60f, 0.63f, 0.42f, 1.00f},  // Snare
    {0.58f, 0.54f, 0.48f, 0.53f, 0.28f, 0.98f},  // Tom
    {0.40f, 0.42f, 0.54f, 0.63f, 0.72f, 1.00f},  // Hat
    {0.42f, 0.45f, 0.53f, 0.58f, 0.65f, 1.00f},  // Ride
    {0.52f, 0.49f, 0.54f, 0.58f, 0.40f, 0.97f},  // Room
    {0.46f, 0.47f, 0.53f, 0.60f, 0.45f, 1.00f},  // OH
    {0.52f, 0.50f, 0.51f, 0.54f, 0.05f, 0.95f},  // Bus
};

struct LimitDefaults {
  float ceiling;
  float release;
  float lookahead;
  float mix;
};

inline constexpr LimitDefaults kLimitDefaults[8] = {
    {0.967f, 0.45f, 0.35f, 1.0f},  // Kick
    {0.967f, 0.40f, 0.30f, 1.0f},  // Snare
    {0.970f, 0.50f, 0.40f, 1.0f},  // Tom
    {0.980f, 0.30f, 0.20f, 1.0f},  // Hat
    {0.980f, 0.35f, 0.25f, 1.0f},  // Ride
    {0.970f, 0.55f, 0.50f, 1.0f},  // Room
    {0.970f, 0.50f, 0.45f, 1.0f},  // OH
    {0.950f, 0.60f, 0.50f, 1.0f},  // Bus
};

struct SampleDefaults {
  float level;
  float direct;
  float pitch;
  float start;
  float fadeMs;
  float sensitivity;
  float offsetMs;
  int trigger;
};

inline constexpr SampleDefaults kSampleDefaults[8] = {
    {85.0f, 100.0f, 0.0f, 0.0f, 2.0f, 0.50f, 0.0f, 0},  // Kick
    {85.0f, 100.0f, 0.0f, 0.0f, 1.0f, 0.55f, 0.0f, 0},  // Snare
    {80.0f, 100.0f, 0.0f, 0.0f, 4.0f, 0.50f, 0.0f, 0},  // Tom
    {75.0f, 100.0f, 0.0f, 0.0f, 1.0f, 0.60f, 0.0f, 0},  // Hat
    {75.0f, 100.0f, 0.0f, 0.0f, 1.0f, 0.55f, 0.0f, 0},  // Ride
    {70.0f, 100.0f, 0.0f, 0.0f, 10.0f, 0.50f, 0.0f, 0}, // Room
    {70.0f, 100.0f, 0.0f, 0.0f, 10.0f, 0.50f, 0.0f, 0}, // OH
    {70.0f, 100.0f, 0.0f, 0.0f, 5.0f, 0.50f, 0.0f, 0},  // Bus
};

inline float eqHpfHz(float v) noexcept {
  v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  return 20.0f * std::pow(25.0f, v);
}

inline float eqLpfHz(float v) noexcept {
  v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  return 500.0f * std::pow(40.0f, v);
}

// One loaded sample.  The audio thread only sees an immutable shared_ptr; the
// panel builds a new buffer when the user loads a file.
struct SampleBuffer {
  std::vector<float> data;   // left channel (or mono)
  std::vector<float> dataR;  // right channel; empty for mono
  int sampleRate = 48000;
  int rootKey = -1;          // -1 = use the zone/key without transposition
  float tuneCents = 0.0f;
  int loopStart = -1;
  int loopEnd = -1;
  std::string name;
  std::string path;

  bool stereo() const { return !dataR.empty(); }
  size_t frames() const { return data.size(); }
};

// One region of a multisample instrument.  Drum Suite is a single-channel
// strip, so zones are selected by key and velocity; a drum kit SFZ/SF2 can be
// loaded per instance and played from MIDI notes.
struct Zone {
  std::shared_ptr<const SampleBuffer> sample;
  int lokey = 0;
  int hikey = 127;
  int lovel = 0;
  int hivel = 127;
  int rootKey = -1;
  int seqLength = 0;       // 0/1 = no round robin
  int seqPosition = 1;
  int loRand = 0;
  int hiRand = 127;
  int group = 0;
  int offBy = 0;
  float tuneCents = 0.0f;
  float gainDb = 0.0f;
  float pan = 0.0f;        // -1..1
  float start = 0.0f;      // 0..1 inside the sample
  float end = 1.0f;        // 0..1 inside the sample
  bool loop = false;
  bool reverse = false;
};

struct Instrument {
  std::string name;
  std::string path;
  std::string type;        // wav, wav-folder, sfz, sf2
  std::vector<Zone> zones;

  bool valid() const { return !zones.empty(); }
  std::shared_ptr<const SampleBuffer> preview() const {
    for (const auto& z : zones) {
      if (z.sample && !z.sample->data.empty()) return z.sample;
    }
    return nullptr;
  }
};

struct Controls {
  int drum = 0;  // Kick

  bool power = true;
  bool bypass = false;
  float inputDb = 0.0f;
  float outputDb = 0.0f;

  bool gateOn = true;
  float thresholdDb = -30.0f;  // user
  float deBleed = 70.0f;       // user

  // Ghost note mode.  The amount is user/MIDI controlled; the ghost detector
  // itself is profile-aware.
  bool ghostMode = false;
  float ghost = 50.0f;
  int ghostNote = 38;
  int ghostCc = 74;
  int ghostTrigger = 0;
  float ghostVelocity = 1.0f;

  // Transient shaper.
  bool transientOn = true;
  float transientAttack = 25.0f;   // -100..100
  float transientSustain = 10.0f;  // -100..100
  float transientSensitivity = 50.0f;

  // EQ.
  float eqLow = 0.60f;
  float eqLowMid = 0.45f;
  float eqHighMid = 0.55f;
  float eqHigh = 0.57f;
  float eqHpf = 0.14f;
  float eqLpf = 0.98f;

  // Compressor.
  bool compOn = true;
  int compModel = 0;  // 0 76, 1 2A, 2 VCA
  float compThresholdDb = -14.0f;
  float compRatio = 3.0f;   // 0..8 ladder
  float compAttack = 0.3f;  // 0..1
  float compRelease = 0.5f; // 0..1
  float compMakeup = 0.8f;  // 0.5 = unity
  float compMix = 1.0f;
  float compScHpf = 0.3f;

  // Drive.
  bool driveOn = true;
  int driveModel = 0;  // 0 Valve, 1 Console, 2 Bus, 3 Tape, 4 Transformer
  float driveDrive = 0.45f;
  float driveCharacter = 0.55f;
  float driveTone = 0.45f;
  float driveMix = 0.35f;
  bool driveAutoGain = true;

  // Clipper.
  bool clipOn = true;
  int clipShape = 0;        // 0 Soft, 1 Hard, 2 Diode
  float clipDrive = 0.35f;
  float clipCeiling = 0.85f;
  float clipMix = 0.25f;
  int clipOversample = 1;   // 0 Off, 1 2x, 2 4x
  bool clipAutoGain = true;

  // Limiter.
  bool limitOn = true;
  float limitCeiling = 0.967f;   // about -0.4 dB
  float limitRelease = 0.45f;
  float limitLookahead = 0.35f;
  bool limitTruePeak = true;
  float limitMix = 1.0f;

  // Sample loader.
  bool sampleOn = false;
  float sampleLevel = 80.0f;    // 0..100
  float sampleDirect = 100.0f;  // 0..100
  float samplePitch = 0.0f;     // semitones
  float sampleStart = 0.0f;     // 0..100
  bool sampleReverse = false;
  bool sampleChoke = true;
  float sampleFadeMs = 3.0f;    // 0..500
  int sampleTrigger = 0;        // 0 Gate, 1 Transient, 2 Always, 3 MIDI
  float sampleSensitivity = 0.5f;
  float sampleOffsetMs = 0.0f;
  int sampleMidiTrigger = 0;
  int sampleMidiNote = 36;
  int sampleNote = 36;
  bool samplePhase = false;
  float sampleRetriggerMs = 0.0f;
  int sampleMidiVelocity = 100;
};

struct Meters {
  float inPeakDb = -120.0f;
  float outPeakDb = -120.0f;
  float inPeakLDb = -120.0f;
  float inPeakRDb = -120.0f;
  float outPeakLDb = -120.0f;
  float outPeakRDb = -120.0f;
  float grDb = 0.0f;
  float detectorDb = -120.0f;
  float gate = 1.0f;
  bool ghostActive = false;
  int sampleVoices = 0;
  int sampleZone = -1;
  int sampleZoneCount = 0;
  bool sampleTriggered = false;
  float compGrDb = 0.0f;
  bool clipActive = false;
  float limitGrDb = 0.0f;
  float transientDb = 0.0f;
};

} // namespace drum
