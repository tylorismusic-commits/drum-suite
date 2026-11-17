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

inline std::string formatParamValue(const Shared&, uint32_t id, double value) {
  const ParamSpec& s = kParamSpecs[id];
  if (s.labels.count > 0) {
    const int i = std::clamp(static_cast<int>(std::lround(value - s.min)), 0,
                             s.labels.count - 1);
    return s.labels.names[i];
  }
  char buf[64];
  if (s.toggled) return value > 0.5 ? "On" : "Off";
  if (id == P_GHOST_NOTE || id == P_GHOST_CC) {
    std::snprintf(buf, sizeof(buf), "%.0f", value);
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

inline bool parseParamValue(const Shared&, uint32_t id,
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
  char* end = nullptr;
  double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) return false;
  v = std::min(std::max(v, s.min), s.max);
  *out = v;
  return true;
}

} // namespace drum
