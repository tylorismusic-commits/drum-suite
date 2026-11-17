// Drum Suite — controls.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

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
  float ghost = 75.0f;
  int ghostNote = 38;
  int ghostCc = 74;
  int ghostTrigger = 0;
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
};

} // namespace drum
