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

    // The header button restores this drum's complete recommendation.
    shared.set(P_THRESHOLD, -55.0);
    shared.set(P_EQ_LOW, 0.05);
    shared.set(P_LIMIT_CEILING, 0.5);
    panel.pointerDown(shared, state, 920, 32, false);
    panel.pointerUp();
    check(std::fabs(shared.get(P_THRESHOLD) - kProfiles[1].thresholdDb) < 1e-6,
          "Recommended restores the Snare gate setting");
    check(std::fabs(shared.get(P_EQ_LOW) - kEqDefaults[1].low) < 1e-6,
          "Recommended restores the Snare EQ setting");
    check(std::fabs(shared.get(P_LIMIT_CEILING) - kLimitDefaults[1].ceiling) <
              1e-6,
          "Recommended restores the Snare limiter setting");

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

    // Comp page: model tabs and model-specific layout.
    panel.pointerDown(shared, state, 520, 87, false);
    panel.pointerUp();
    check(state.page == 3, "Comp page tab selects the Comp page");
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 211, 196, false);
    panel.pointerUp();
    check(shared.get(P_COMP_MODEL) == 1.0, "2A model tab selects the 2A");
    panel.draw(cr, shared, state);
    bool hasAttack = false;
    for (const auto& r : panel.widgetRects()) {
      if (r.knob && r.param == P_COMP_ATTACK) hasAttack = true;
    }
    check(!hasAttack, "2A hides the attack knob");
    panel.pointerDown(shared, state, 101, 196, false);
    panel.pointerUp();
    check(shared.get(P_COMP_MODEL) == 0.0, "76 model tab selects the 76");
    panel.draw(cr, shared, state);
    hasAttack = false;
    for (const auto& r : panel.widgetRects()) {
      if (r.knob && r.param == P_COMP_ATTACK) hasAttack = true;
    }
    check(hasAttack, "76 shows the attack knob");

    // Drum profiles carry compressor defaults.
    panel.pointerDown(shared, state, 458, 131, false);
    panel.pointerUp();
    check(shared.get(P_DRUM) == 3.0 && shared.get(P_COMP_MODEL) == 1.0,
          "Hat profile selects the 2A compressor");
    panel.pointerDown(shared, state, 88, 131, false);
    panel.pointerUp();
    check(shared.get(P_DRUM) == 0.0 && shared.get(P_COMP_MODEL) == 0.0,
          "Kick profile selects the 76 compressor");

    // Drive page: five model tabs and auto gain.
    panel.pointerDown(shared, state, 660, 87, false);
    panel.pointerUp();
    check(state.page == 4, "Drive page tab selects the Drive page");
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 420, 196, false);
    panel.pointerUp();
    check(shared.get(P_DRIVE_MODEL) == 3.0, "Tape model tab selects Tape");
    panel.pointerDown(shared, state, 778, 196, false);
    panel.pointerUp();
    check(shared.get(P_DRIVE_AUTOGAIN) == 0.0, "Auto Gain pill toggles off");

    // Clip page: shape and oversampling tabs.
    panel.pointerDown(shared, state, 802, 87, false);
    panel.pointerUp();
    check(state.page == 5, "Clip page tab selects the Clip page");
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 316, 196, false);
    panel.pointerUp();
    check(shared.get(P_CLIP_SHAPE) == 2.0, "Diode shape tab selects Diode");
    panel.pointerDown(shared, state, 348, 228, false);
    panel.pointerUp();
    check(shared.get(P_CLIP_OVERSAMPLE) == 2.0, "4X tab selects 4x oversampling");
    check(shared.get(P_CLIP_ON) == 1.0, "Kick profile enables the clip stage");
    panel.pointerDown(shared, state, 447, 196, false);
    panel.pointerUp();
    check(shared.get(P_CLIP_ON) == 0.0, "CLIP ON pill disables the clipper");

    // Limit page: safety controls.
    panel.pointerDown(shared, state, 940, 87, false);
    panel.pointerUp();
    check(state.page == 6, "Limit page tab selects the Limit page");
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 234, 196, false);
    panel.pointerUp();
    check(shared.get(P_LIMIT_TRUEPEAK) == 0.0, "TRUE PEAK pill toggles off");
    panel.pointerDown(shared, state, 107, 196, false);
    panel.pointerUp();
    check(shared.get(P_LIMIT_ON) == 0.0, "LIMIT ON pill disables the limiter");
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
    panel.pointerDown(shared, state, 140, 270, false);
    panel.pointerUp();
    check(shared.sample() != nullptr, "clicking a sample row loads it");
    check(shared.sample() && shared.sample()->data.size() == 4800,
          "loaded sample keeps its length");
    check(shared.sample() && !shared.sample()->path.empty(),
          "loaded sample remembers its file path");
    check(shared.get(P_SAMPLE_ON) > 0.5, "loading a sample enables the layer");
  }

  {
    Shared shared;
    Panel panel;
    PanelState state;
    shared.set(P_DRUM, 1.0);
    shared.set(P_EQ_HPF, 0.0);
    shared.set(P_COMP_MODEL, 2.0);
    panel.draw(cr, shared, state);
    panel.pointerDown(shared, state, 926, 32, false);
    panel.pointerUp();
    check(std::fabs(shared.get(P_EQ_HPF) - kEqDefaults[1].hpf) < 1e-6,
          "recommended button applies the drum EQ guide");
    check(shared.get(P_COMP_MODEL) == static_cast<double>(kCompDefaults[1].model),
          "recommended button applies the drum compressor guide");
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
    int busPreset = -1;
    for (int i = 0; i < kPresetCount; ++i) {
      if (std::string(kPresets[i].name).find("Bus") != std::string::npos)
        busPreset = i;
    }
    check(busPreset >= 0, "Bus preset exists");
    if (busPreset >= 0) {
      panel.applyPreset(shared, state, busPreset);
      check(shared.get(P_COMP_MODEL) == 2.0,
            "Bus preset applies the VCA compressor profile");
    }
    int roomPreset = -1;
    for (int i = 0; i < kPresetCount; ++i) {
      if (std::string(kPresets[i].name) == "Snare — Room") roomPreset = i;
    }
    check(roomPreset >= 0, "full-chain Snare Room preset exists");
    if (roomPreset >= 0) {
      panel.applyPreset(shared, state, roomPreset);
      check(std::fabs(shared.get(P_DRIVE_MIX) - 0.50) < 1e-9 &&
                std::fabs(shared.get(P_CLIP_MIX) - 0.35) < 1e-9,
            "full-chain preset restores drive and clip values");
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
    check(parseParamValue(shared, P_COMP_RATIO, "8:1", &v) &&
              std::fabs(v - 8.0) < 1e-9,
          "typed compressor ratio accepts 8:1");
    check(parseParamValue(shared, P_COMP_MAKEUP, "+6 dB", &v) &&
              std::fabs(v - 0.8) < 1e-9,
          "typed compressor makeup accepts +6 dB");
    check(parseParamValue(shared, P_LIMIT_CEILING, "-1 dB", &v) &&
              std::fabs(-12.0 + 12.0 * v + 1.0) < 1e-9,
          "typed limiter ceiling accepts -1 dB");
    check(parseParamValue(shared, P_LIMIT_RELEASE, "60 ms", &v) &&
              v > 0.2 && v < 0.4,
          "typed limiter release accepts 60 ms without maxing out");
    check(parseParamValue(shared, P_COMP_RELEASE, "60 ms", &v) &&
              v < 0.2,
          "typed compressor release accepts 60 ms without maxing out");
    check(parseParamValue(shared, P_LIMIT_LOOKAHEAD, "1 ms", &v) &&
              std::fabs(v - 0.444444) < 1e-4,
          "typed lookahead accepts 1 ms");
    check(formatParamValue(shared, P_SAMPLE_NOTE, 36.0) == "C1",
          "MIDI note 36 formats as C1");
    check(parseParamValue(shared, P_SAMPLE_NOTE, "D#2", &v) &&
              std::fabs(v - 51.0) < 1e-9,
          "typed MIDI note accepts D#2");
    static const int kGm[8] = {36, 38, 45, 42, 51, 41, 49, 36};
    bool notes = true;
    for (int drum = 0; drum < 8; ++drum) {
      applyDrumDefaults(shared, drum);
      if (std::fabs(shared.get(P_SAMPLE_NOTE) - kGm[drum]) > 1e-9) {
        notes = false;
      }
    }
    check(notes, "each drum tab picks its GM sample note");
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surf);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
