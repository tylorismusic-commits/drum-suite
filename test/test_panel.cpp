// Drum Suite — panel interaction tests (no X server needed).
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cairo.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

void put32(std::ofstream& f, uint32_t v) {
  const char b[4] = {static_cast<char>(v & 0xff),
                     static_cast<char>((v >> 8) & 0xff),
                     static_cast<char>((v >> 16) & 0xff),
                     static_cast<char>((v >> 24) & 0xff)};
  f.write(b, 4);
}

void put16(std::ofstream& f, uint16_t v) {
  const char b[2] = {static_cast<char>(v & 0xff),
                     static_cast<char>((v >> 8) & 0xff)};
  f.write(b, 2);
}

void writeTestWav(const std::filesystem::path& path) {
  const int sr = 48000;
  const int n = 4800;
  std::ofstream f(path, std::ios::binary);
  f.write("RIFF", 4);
  put32(f, 36 + n * 2);
  f.write("WAVEfmt ", 8);
  put32(f, 16);
  put16(f, 1);
  put16(f, 1);
  put32(f, sr);
  put32(f, sr * 2);
  put16(f, 2);
  put16(f, 16);
  f.write("data", 4);
  put32(f, n * 2);
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / sr;
    const double env = std::exp(-t * 18.0);
    const double v = 0.55 * env * std::sin(2.0 * 3.14159265358979 * 180.0 * t);
    put16(f, static_cast<uint16_t>(static_cast<int16_t>(v * 32767.0)));
  }
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
    const std::filesystem::path root = "/tmp/opencode/drum_panel_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "drum-suite/samples", ec);
    writeTestWav(root / "drum-suite/samples/TestHit.wav");
    setenv("XDG_DATA_HOME", root.c_str(), 1);

    Shared shared;
    Panel panel;
    PanelState state;
    panel.draw(cr, shared, state);
    panel.tick(shared, state);
    check(state.sampleFiles.size() == 1 && state.sampleFiles[0] == "TestHit.wav",
          "sample folder scan finds WAV files");
    state.page = 1;
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 100, 240, false);
    panel.pointerUp();
    check(shared.sample() != nullptr, "clicking a sample row loads it");
    check(shared.sample() && shared.sample()->data.size() == 4800,
          "loaded sample keeps its length");
    check(shared.get(P_SAMPLE_ON) > 0.5, "loading a sample enables the layer");
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
    check(parseParamValue(shared, P_EQ_LOW, "+6 dB", &v) &&
              std::fabs(v - 0.75) < 1e-9,
          "typed EQ gain accepts +6 dB");
    check(parseParamValue(shared, P_EQ_LPF, "10000 Hz", &v) &&
              std::fabs(eqLpfHz(static_cast<float>(v)) - 10000.0) < 2.0,
          "typed LPF accepts 10 kHz");
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surf);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
