// Drum Suite — panel interaction tests (no X server needed).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <string>

#include "params.hpp"
#include "presets.hpp"
#include "ui/panel.hpp"
#include "value_text.hpp"

using namespace drum;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}
} // namespace

int main() {
  cairo_surface_t* surf =
      cairo_image_surface_create(CAIRO_FORMAT_RGB24, kPanelWidth, kPanelHeight);
  cairo_t* cr = cairo_create(surf);

  {
    Shared shared;
    Panel panel;
    PanelState state;
    panel.draw(cr, shared, state);
    check(shared.get(P_DRUM) == 0.0, "starts on Kick");

    // Snare drum tab.
    panel.pointerDown(shared, state, 211, 131, false);
    panel.pointerUp();
    check(shared.get(P_DRUM) == 1.0, "Snare tab selects Snare");
    check(std::fabs(shared.get(P_THRESHOLD) - kProfiles[1].thresholdDb) < 1e-6,
          "Snare profile applies its threshold");
    check(std::fabs(shared.get(P_DEBLEED) - kProfiles[1].deBleed) < 1e-6,
          "Snare profile applies its DeBleed default");

    // Ghost icon toggles ghost mode.
    panel.pointerDown(shared, state, 886, 430, false);
    panel.pointerUp();
    check(shared.get(P_GHOST_MODE) == 1.0, "ghost icon enables ghost mode");

    // Only three primary knobs are exposed; the rest is automated.
    int primary = 0;
    for (const auto& r : panel.widgetRects()) {
      if (r.knob && (r.param == P_THRESHOLD || r.param == P_DEBLEED ||
                     r.param == P_GHOST)) {
        ++primary;
      }
    }
    check(primary == 3, "primary controls are exactly Threshold, DeBleed and Ghost");

    // Page tabs switch to the placeholder pages and back.
    panel.pointerDown(shared, state, 238, 87, false);
    panel.pointerUp();
    check(state.page == 1, "Sample page tab selects the Sample page");
    panel.pointerDown(shared, state, 97, 87, false);
    panel.pointerUp();
    check(state.page == 0, "Bleed Out page tab returns to the Bleed Out page");
  }

  {
    Shared shared;
    Panel panel;
    PanelState state;
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 72 + 330 - 18, 32, false);
    panel.pointerUp();
    check(state.presetIndex == 0, "preset next arrow applies a preset");
  }

  {
    Shared shared;
    Panel panel;
    PanelState state;
    panel.draw(cr, shared, state);
    int ghostPreset = -1;
    for (int i = 0; i < kPresetCount; ++i) {
      if (std::string(kPresets[i].name) == "Ghost Notes") ghostPreset = i;
    }
    check(ghostPreset >= 0, "Ghost Notes preset exists");
    if (ghostPreset >= 0) {
      panel.applyPreset(shared, state, ghostPreset);
      check(shared.get(P_GHOST_MODE) == 1.0, "Ghost Notes enables ghost mode");
      check(shared.get(P_DRUM) == 1.0, "Ghost Notes selects Snare");
    }
  }

  {
    Shared shared;
    double v = -1.0;
    check(parseParamValue(shared, P_THRESHOLD, "-40 dB", &v) &&
              std::fabs(v + 40.0) < 1e-9,
          "typed threshold accepts -40 dB");
    check(parseParamValue(shared, P_DEBLEED, "85 %", &v) &&
              std::fabs(v - 85.0) < 1e-9,
          "typed DeBleed accepts 85 %");
    check(parseParamValue(shared, P_DRUM, "Snare", &v) &&
              std::fabs(v - 1.0) < 1e-9,
          "typed drum accepts Snare");
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surf);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
