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

inline constexpr PresetValue kKickModern[] = {
    PV(P_POWER, 1), PV(P_DRUM, 0), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -30),
    PV(P_DEBLEED, 65), PV(P_GHOST_MODE, 0),
    PV(P_COMP_MODEL, 0), PV(P_COMP_THRESHOLD, -20), PV(P_COMP_RATIO, 4),
    PV(P_COMP_ATTACK, 0.20), PV(P_COMP_RELEASE, 0.30), PV(P_COMP_MAKEUP, 0.80),
    PV(P_COMP_MIX, 1.0), PV(P_COMP_SC_HPF, 0.05),
    PV(P_DRIVE_MODEL, 0), PV(P_DRIVE_DRIVE, 0.50), PV(P_DRIVE_CHARACTER, 0.60),
    PV(P_DRIVE_TONE, 0.40), PV(P_DRIVE_MIX, 0.40), PV(P_DRIVE_AUTOGAIN, 1),
    PV(P_CLIP_ON, 1), PV(P_CLIP_SHAPE, 0), PV(P_CLIP_DRIVE, 0.40),
    PV(P_CLIP_CEILING, 0.85), PV(P_CLIP_MIX, 0.30), PV(P_CLIP_OVERSAMPLE, 1),
    PV(P_CLIP_AUTOGAIN, 1), PV(P_LIMIT_ON, 1), PV(P_LIMIT_CEILING, 0.967)};

inline constexpr PresetValue kSnareRoom[] = {
    PV(P_POWER, 1), PV(P_DRUM, 1), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -32),
    PV(P_DEBLEED, 75), PV(P_GHOST_MODE, 0),
    PV(P_COMP_MODEL, 0), PV(P_COMP_THRESHOLD, -24), PV(P_COMP_RATIO, 5),
    PV(P_COMP_ATTACK, 0.12), PV(P_COMP_RELEASE, 0.50), PV(P_COMP_MAKEUP, 0.80),
    PV(P_COMP_MIX, 0.85), PV(P_COMP_SC_HPF, 0.25),
    PV(P_DRIVE_MODEL, 1), PV(P_DRIVE_DRIVE, 0.55), PV(P_DRIVE_CHARACTER, 0.65),
    PV(P_DRIVE_TONE, 0.55), PV(P_DRIVE_MIX, 0.50), PV(P_DRIVE_AUTOGAIN, 1),
    PV(P_CLIP_ON, 1), PV(P_CLIP_SHAPE, 2), PV(P_CLIP_DRIVE, 0.45),
    PV(P_CLIP_CEILING, 0.85), PV(P_CLIP_MIX, 0.35), PV(P_CLIP_OVERSAMPLE, 1),
    PV(P_CLIP_AUTOGAIN, 1), PV(P_LIMIT_ON, 1), PV(P_LIMIT_CEILING, 0.967)};

inline constexpr PresetValue kHatBright[] = {
    PV(P_POWER, 1), PV(P_DRUM, 3), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -36),
    PV(P_DEBLEED, 25), PV(P_GHOST_MODE, 0),
    PV(P_COMP_MODEL, 1), PV(P_COMP_THRESHOLD, -18), PV(P_COMP_RATIO, 2),
    PV(P_COMP_MAKEUP, 0.40), PV(P_COMP_MIX, 0.50), PV(P_COMP_SC_HPF, 0.60),
    PV(P_DRIVE_MODEL, 4), PV(P_DRIVE_DRIVE, 0.30), PV(P_DRIVE_CHARACTER, 0.40),
    PV(P_DRIVE_TONE, 0.65), PV(P_DRIVE_MIX, 0.25), PV(P_DRIVE_AUTOGAIN, 1),
    PV(P_CLIP_ON, 0), PV(P_LIMIT_ON, 1), PV(P_LIMIT_CEILING, 0.967)};

inline constexpr PresetValue kRoomCrunch[] = {
    PV(P_POWER, 1), PV(P_DRUM, 5), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -40),
    PV(P_DEBLEED, 10), PV(P_GHOST_MODE, 0),
    PV(P_COMP_MODEL, 2), PV(P_COMP_THRESHOLD, -14), PV(P_COMP_RATIO, 3),
    PV(P_COMP_ATTACK, 0.35), PV(P_COMP_RELEASE, 0.55), PV(P_COMP_MAKEUP, 0.42),
    PV(P_COMP_MIX, 1.0), PV(P_COMP_SC_HPF, 0.65),
    PV(P_DRIVE_MODEL, 3), PV(P_DRIVE_DRIVE, 0.55), PV(P_DRIVE_CHARACTER, 0.60),
    PV(P_DRIVE_TONE, 0.50), PV(P_DRIVE_MIX, 0.40), PV(P_DRIVE_AUTOGAIN, 1),
    PV(P_CLIP_ON, 1), PV(P_CLIP_SHAPE, 0), PV(P_CLIP_DRIVE, 0.40),
    PV(P_CLIP_CEILING, 0.85), PV(P_CLIP_MIX, 0.25), PV(P_CLIP_OVERSAMPLE, 1),
    PV(P_CLIP_AUTOGAIN, 1), PV(P_LIMIT_ON, 1), PV(P_LIMIT_CEILING, 0.967)};

inline constexpr PresetValue kBusFullGlue[] = {
    PV(P_POWER, 1), PV(P_DRUM, 7), PV(P_GATE_ON, 1), PV(P_THRESHOLD, -45),
    PV(P_DEBLEED, 0), PV(P_GHOST_MODE, 0),
    PV(P_COMP_MODEL, 2), PV(P_COMP_THRESHOLD, -12), PV(P_COMP_RATIO, 4),
    PV(P_COMP_ATTACK, 0.40), PV(P_COMP_RELEASE, 0.60), PV(P_COMP_MAKEUP, 0.38),
    PV(P_COMP_MIX, 1.0), PV(P_COMP_SC_HPF, 0.70),
    PV(P_DRIVE_MODEL, 2), PV(P_DRIVE_DRIVE, 0.45), PV(P_DRIVE_CHARACTER, 0.55),
    PV(P_DRIVE_TONE, 0.50), PV(P_DRIVE_MIX, 0.30), PV(P_DRIVE_AUTOGAIN, 1),
    PV(P_CLIP_ON, 1), PV(P_CLIP_SHAPE, 2), PV(P_CLIP_DRIVE, 0.40),
    PV(P_CLIP_CEILING, 0.85), PV(P_CLIP_MIX, 0.30), PV(P_CLIP_OVERSAMPLE, 2),
    PV(P_CLIP_AUTOGAIN, 1), PV(P_LIMIT_ON, 1), PV(P_LIMIT_CEILING, 0.967)};

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
    {"Kick", "Kick — Modern", 0, kKickModern, static_cast<int>(sizeof(kKickModern) / sizeof(kKickModern[0]))},
    {"Snare", "Snare — Room", 1, kSnareRoom, static_cast<int>(sizeof(kSnareRoom) / sizeof(kSnareRoom[0]))},
    {"Hats", "Hat — Bright", 3, kHatBright, static_cast<int>(sizeof(kHatBright) / sizeof(kHatBright[0]))},
    {"Room", "Room — Crunch", 5, kRoomCrunch, static_cast<int>(sizeof(kRoomCrunch) / sizeof(kRoomCrunch[0]))},
    {"Bus", "Bus — Full Glue", 7, kBusFullGlue, static_cast<int>(sizeof(kBusFullGlue) / sizeof(kBusFullGlue[0]))},
};

inline constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace drum
