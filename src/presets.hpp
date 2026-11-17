// Drum Suite — presets.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>

#include "params.hpp"

namespace drum {

struct PresetValue {
  int param;
  double value;
};

struct Preset {
  const char* category;
  const char* name;
  int drum;
  const PresetValue* values;
  int count;
};

#define PV(p, v) {static_cast<int>(p), (v)}

inline constexpr PresetValue kInit[] = {
    PV(P_POWER, 1), PV(P_BYPASS, 0), PV(P_INPUT, 0), PV(P_OUTPUT, 0),
    PV(P_DRUM, 0), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -30),
    PV(P_DEBLEED, 70), PV(P_GHOST_MODE, 0), PV(P_GHOST, 75),
    PV(P_GHOST_NOTE, 38), PV(P_GHOST_CC, 74)};

inline constexpr PresetValue kKickTight[] = {
    PV(P_POWER, 1), PV(P_DRUM, 0), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -28),
    PV(P_DEBLEED, 75), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kKickBody[] = {
    PV(P_POWER, 1), PV(P_DRUM, 0), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -34),
    PV(P_DEBLEED, 55), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kSnareTight[] = {
    PV(P_POWER, 1), PV(P_DRUM, 1), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -30),
    PV(P_DEBLEED, 85), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kSnareNatural[] = {
    PV(P_POWER, 1), PV(P_DRUM, 1), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -36),
    PV(P_DEBLEED, 55), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kGhostNotes[] = {
    PV(P_POWER, 1), PV(P_DRUM, 1), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -28),
    PV(P_DEBLEED, 80), PV(P_GHOST_MODE, 1), PV(P_GHOST, 75),
    PV(P_GHOST_NOTE, 38), PV(P_GHOST_CC, 74)};

inline constexpr PresetValue kTomRoom[] = {
    PV(P_POWER, 1), PV(P_DRUM, 2), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -34),
    PV(P_DEBLEED, 55), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kHatTight[] = {
    PV(P_POWER, 1), PV(P_DRUM, 3), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -34),
    PV(P_DEBLEED, 30), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kRideNatural[] = {
    PV(P_POWER, 1), PV(P_DRUM, 4), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -38),
    PV(P_DEBLEED, 20), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kRoomGlue[] = {
    PV(P_POWER, 1), PV(P_DRUM, 5), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -38),
    PV(P_DEBLEED, 15), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kOhWide[] = {
    PV(P_POWER, 1), PV(P_DRUM, 6), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -40),
    PV(P_DEBLEED, 10), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

inline constexpr PresetValue kBusGlue[] = {
    PV(P_POWER, 1), PV(P_DRUM, 7), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -42),
    PV(P_DEBLEED, 0), PV(P_GHOST_MODE, 0), PV(P_GHOST, 50)};

#undef PV

inline constexpr Preset kPresets[] = {
    {"Init", "Init", 0, kInit, static_cast<int>(sizeof(kInit) / sizeof(kInit[0]))},
    {"Kick", "Kick — Tight", 0, kKickTight, static_cast<int>(sizeof(kKickTight) / sizeof(kKickTight[0]))},
    {"Kick", "Kick — Body", 0, kKickBody, static_cast<int>(sizeof(kKickBody) / sizeof(kKickBody[0]))},
    {"Snare", "Snare — Tight", 1, kSnareTight, static_cast<int>(sizeof(kSnareTight) / sizeof(kSnareTight[0]))},
    {"Snare", "Snare — Natural", 1, kSnareNatural, static_cast<int>(sizeof(kSnareNatural) / sizeof(kSnareNatural[0]))},
    {"Snare", "Ghost Notes", 1, kGhostNotes, static_cast<int>(sizeof(kGhostNotes) / sizeof(kGhostNotes[0]))},
    {"Toms", "Tom — Room", 2, kTomRoom, static_cast<int>(sizeof(kTomRoom) / sizeof(kTomRoom[0]))},
    {"Hats", "Hat — Tight", 3, kHatTight, static_cast<int>(sizeof(kHatTight) / sizeof(kHatTight[0]))},
    {"Ride", "Ride — Natural", 4, kRideNatural, static_cast<int>(sizeof(kRideNatural) / sizeof(kRideNatural[0]))},
    {"Room", "Room — Glue", 5, kRoomGlue, static_cast<int>(sizeof(kRoomGlue) / sizeof(kRoomGlue[0]))},
    {"Overheads", "OH — Wide", 6, kOhWide, static_cast<int>(sizeof(kOhWide) / sizeof(kOhWide[0]))},
    {"Bus", "Bus — Glue", 7, kBusGlue, static_cast<int>(sizeof(kBusGlue) / sizeof(kBusGlue[0]))},
};

inline constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace drum
