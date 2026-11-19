// Drum Suite — typed parameter values
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "params.hpp"

namespace drum {

inline std::string lowerText(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

inline std::string trimText(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) return {};
  const size_t b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

inline std::string noteName(int note) {
  static const char* names[12] = {"C", "C#", "D", "D#", "E", "F",
                                  "F#", "G", "G#", "A", "A#", "B"};
  note = std::clamp(note, 0, 127);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%s%d", names[note % 12], note / 12 - 2);
  return buf;
}

inline bool parseNoteName(const std::string& raw, int* note) {
  const std::string text = lowerText(trimText(raw));
  if (text.empty()) return false;
  int semitone = -1;
  switch (text[0]) {
    case 'c': semitone = 0; break;
    case 'd': semitone = 2; break;
    case 'e': semitone = 4; break;
    case 'f': semitone = 5; break;
    case 'g': semitone = 7; break;
    case 'a': semitone = 9; break;
    case 'b': semitone = 11; break;
    default: return false;
  }
  size_t i = 1;
  if (i < text.size() && (text[i] == '#' || text[i] == 's')) {
    ++semitone;
    ++i;
  } else if (i < text.size() && text[i] == 'b') {
    --semitone;
    ++i;
  }
  while (i < text.size() && text[i] == ' ') ++i;
  if (i >= text.size()) {
    *note = std::clamp((4 + 2) * 12 + semitone, 0, 127);
    return true;
  }
  char* end = nullptr;
  const int octave = static_cast<int>(std::strtol(text.c_str() + i, &end, 10));
  if (end == text.c_str() + i) return false;
  *note = std::clamp((octave + 2) * 12 + semitone, 0, 127);
  return true;
}

inline std::string formatParamValue(const Shared& shared, uint32_t id,
                                   double value) {
  const ParamSpec& s = kParamSpecs[id];
  if (s.labels.count > 0) {
    const int i = std::clamp(static_cast<int>(std::lround(value - s.min)), 0,
                             s.labels.count - 1);
    return s.labels.names[i];
  }
  char buf[64];
  if (s.toggled) return value > 0.5 ? "On" : "Off";
  if (id == P_SAMPLE_RETRIGGER) {
    if (value <= 0.5) return "Off";
    std::snprintf(buf, sizeof(buf), "%.0f ms", value);
    return buf;
  }
  if (id == P_SAMPLE_NOTE) {
    return noteName(static_cast<int>(std::lround(value)));
  }
  if (id == P_GHOST_NOTE || id == P_GHOST_CC) {
    std::snprintf(buf, sizeof(buf), "%.0f", value);
    return buf;
  }
  if (id == P_EQ_LOW || id == P_EQ_LOW_MID || id == P_EQ_HIGH_MID ||
      id == P_EQ_HIGH) {
    std::snprintf(buf, sizeof(buf), "%+.1f dB", (value - 0.5) * 24.0);
    return buf;
  }
  if (id == P_EQ_HPF) {
    std::snprintf(buf, sizeof(buf), "%.0f Hz",
                  eqHpfHz(static_cast<float>(value)));
    return buf;
  }
  if (id == P_EQ_LPF) {
    std::snprintf(buf, sizeof(buf), "%.0f Hz",
                  eqLpfHz(static_cast<float>(value)));
    return buf;
  }
  if (id == P_SAMPLE_PITCH) {
    std::snprintf(buf, sizeof(buf), "%+.1f st", value);
    return buf;
  }
  if (id == P_COMP_RATIO) {
    const int step = std::clamp(static_cast<int>(std::lround(value)), 0, 8);
    const int model = static_cast<int>(shared.get(P_COMP_MODEL) + 0.5);
    if (model == 1 && step >= 4) {
      return "Limit";
    }
    if (step >= 8) {
      return "All buttons";
    }
    std::snprintf(buf, sizeof(buf), "%.1f:1", compRatioValue(value));
    return buf;
  }
  if (id == P_COMP_ATTACK || id == P_COMP_RELEASE) {
    const int model = static_cast<int>(shared.get(P_COMP_MODEL) + 0.5);
    if (model == 1) return "Program";
    const float ms = id == P_COMP_ATTACK
                         ? compAttackMs(model, static_cast<float>(value))
                         : compReleaseMs(model, static_cast<float>(value));
    if (ms < 10.0f) std::snprintf(buf, sizeof(buf), "%.1f ms", ms);
    else std::snprintf(buf, sizeof(buf), "%.0f ms", ms);
    return buf;
  }
  if (id == P_COMP_MAKEUP) {
    std::snprintf(buf, sizeof(buf), "%+.1f dB", (value - 0.5) * 20.0);
    return buf;
  }
  if (id == P_CLIP_CEILING || id == P_LIMIT_CEILING) {
    std::snprintf(buf, sizeof(buf), "%+.1f dB", -12.0 + 12.0 * value);
    return buf;
  }
  if (id == P_LIMIT_LOOKAHEAD) {
    std::snprintf(buf, sizeof(buf), "%.2f ms", 0.2 + 1.8 * value);
    return buf;
  }
  if (id == P_LIMIT_RELEASE) {
    std::snprintf(buf, sizeof(buf), "%.0f ms", 20.0 * std::pow(50.0, value));
    return buf;
  }
  if (id == P_COMP_SC_HPF) {
    std::snprintf(buf, sizeof(buf), "%.0f Hz",
                  20.0 * std::pow(20.0, value));
    return buf;
  }
  if (s.unit && std::strcmp(s.unit, "dB") == 0) {
    std::snprintf(buf, sizeof(buf), "%+.1f dB", value);
    return buf;
  }
  if (s.unit && std::strcmp(s.unit, "ms") == 0) {
    std::snprintf(buf, sizeof(buf), "%.1f ms", value);
    return buf;
  }
  if (s.unit && std::strcmp(s.unit, "%") == 0) {
    std::snprintf(buf, sizeof(buf), "%.0f%%", value);
    return buf;
  }
  std::snprintf(buf, sizeof(buf), "%.2f", value);
  return buf;
}

inline std::string formatEditValue(const Shared& shared, uint32_t id,
                                   double value) {
  return formatParamValue(shared, id, value);
}

inline bool parseParamValue(const Shared& shared, uint32_t id,
                            const std::string& raw, double* out) {
  const ParamSpec& s = kParamSpecs[id];
  const std::string text = lowerText(trimText(raw));
  if (text.empty()) return false;
  for (int i = 0; i < s.labels.count; ++i) {
    if (text == lowerText(s.labels.names[i])) {
      *out = s.min + i;
      return true;
    }
  }
  if (text == "on" || text == "true" || text == "yes") { *out = 1.0; return true; }
  if (text == "off" || text == "false" || text == "no") { *out = 0.0; return true; }
  if (id == P_SAMPLE_RETRIGGER) {
    if (text == "off" || text == "none") {
      *out = 0.0;
      return true;
    }
  }
  if (id == P_SAMPLE_NOTE) {
    int note = 0;
    if (parseNoteName(text, &note)) {
      *out = static_cast<double>(note);
      return true;
    }
  }
  char* end = nullptr;
  double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) return false;
  const std::string suffix = trimText(std::string(end));
  auto hz = [&]() {
    return suffix == "khz" || suffix == "k" ? v * 1000.0 : v;
  };
  auto ms = [&]() {
    return suffix == "s" || suffix == "sec" ? v * 1000.0 : v;
  };
  if (id == P_EQ_LOW || id == P_EQ_LOW_MID || id == P_EQ_HIGH_MID ||
      id == P_EQ_HIGH) {
    v = v / 24.0 + 0.5;
  } else if (id == P_EQ_HPF) {
    v = std::log(std::max(hz(), 20.0) / 20.0) / std::log(25.0);
  } else if (id == P_EQ_LPF) {
    v = std::log(std::max(hz(), 500.0) / 500.0) / std::log(40.0);
  } else if (id == P_COMP_MAKEUP) {
    v = v / 20.0 + 0.5;
  } else if (id == P_COMP_ATTACK || id == P_COMP_RELEASE) {
    const int model = static_cast<int>(shared.get(P_COMP_MODEL) + 0.5);
    const double t = ms();
    if (model == 0) {
      if (id == P_COMP_ATTACK) {
        v = std::round(6.0 * std::log(std::max(t, 0.001) / 0.04) /
                       std::log(40.0)) /
            6.0;
      } else {
        v = std::round(6.0 * std::log(std::max(t, 0.001) / 50.0) /
                       std::log(22.0)) /
            6.0;
      }
    } else if (model == 2) {
      v = id == P_COMP_ATTACK
              ? std::log(std::max(t, 0.01) / 0.1) / std::log(300.0)
              : std::log(std::max(t, 0.01) / 100.0) / std::log(12.0);
    } else {
      return false;  // Program-dependent 2A release/attack.
    }
  } else if (id == P_LIMIT_RELEASE) {
    v = std::log(std::max(ms(), 20.0) / 20.0) / std::log(50.0);
  } else if (id == P_LIMIT_LOOKAHEAD) {
    v = (ms() - 0.2) / 1.8;
  } else if (id == P_CLIP_CEILING || id == P_LIMIT_CEILING) {
    v = (v + 12.0) / 12.0;
  }
  v = std::min(std::max(v, s.min), s.max);
  *out = v;
  return true;
}

} // namespace drum
