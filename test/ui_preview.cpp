// Drum Suite — renders the panel pages to PNGs for visual checks.
// Usage: drum_ui_preview <font-dir> <output-dir> [scale]
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

#include "dsp/wav.hpp"
#include "params.hpp"
#include "ui/geometry.hpp"
#include "ui/panel.hpp"

using namespace drum;

namespace {
void fakeScope(Shared& s, float gate) {
  for (int i = 0; i < Shared::kScopeSize; ++i) {
    const float t = static_cast<float>(i) / (Shared::kScopeSize - 1);
    const float env = std::exp(-std::fabs(t - 0.35f) * 18.0f) +
                      std::exp(-std::fabs(t - 0.72f) * 22.0f);
    s.scopeIn[i].store(-60.0f + 48.0f * env, std::memory_order_relaxed);
    s.scopeGate[i].store(gate * std::min(1.0f, env * 2.0f),
                         std::memory_order_relaxed);
  }
}
void loadPreviewSample(Shared& s) {
  const char* home = std::getenv("HOME");
  if (!home || !*home) return;
  const std::filesystem::path dir =
      std::filesystem::path(home) / ".local/share/drum-suite/samples";
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec || !entry.is_regular_file(ec)) continue;
    if (entry.path().extension() != ".wav") continue;
    std::vector<float> data;
    if (!readWavMono(entry.path().string(), data, 48000) || data.empty())
      continue;
    auto buffer = std::make_shared<SampleBuffer>();
    buffer->data = std::move(data);
    buffer->sampleRate = 48000;
    buffer->name = entry.path().filename().string();
    s.setSample(std::shared_ptr<const SampleBuffer>(buffer));
    s.set(P_SAMPLE_ON, 1.0);
    return;
  }
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <font-dir> <output-dir> [scale]\n", argv[0]);
    return 2;
  }
  const std::string fontDir = argv[1];
  const std::string outDir = argv[2];
  const double scale = argc > 3 ? std::atof(argv[3]) : 1.0;
  const UiSize size = sizeForScale(scale);
  const double drawScale = static_cast<double>(size.width) / kPanelWidth;

  Panel panel;
  panel.loadFont(fontDir);

  auto render = [&](int drum, bool ghost, int page, const char* name) {
    Shared shared;
    shared.set(P_DRUM, drum);
    shared.set(P_GHOST_MODE, ghost ? 1 : 0);
    shared.set(P_GHOST, 80);
    shared.inPeakLDb.store(-8.0f);
    shared.inPeakRDb.store(-10.0f);
    shared.outPeakLDb.store(-8.5f);
    shared.outPeakRDb.store(-9.0f);
    shared.grDb.store(ghost ? 5.0f : 8.0f);
    shared.detectorDb.store(-18.0f);
    shared.gate.store(1.0f);
    shared.ghostActive.store(ghost);
    fakeScope(shared, 1.0f);
    if (page == 1) {
      loadPreviewSample(shared);
    } else if (page == 3) {
      if (std::getenv("DRUM_PREVIEW_2A")) shared.set(P_COMP_MODEL, 1);
      shared.compGrDb.store(6.4f);
      for (int i = 0; i < Shared::kScopeSize; ++i) {
        const float t = static_cast<float>(i) / (Shared::kScopeSize - 1);
        const float hit = std::exp(-std::fabs(t - 0.22f) * 26.0f) +
                          std::exp(-std::fabs(t - 0.58f) * 30.0f) +
                          std::exp(-std::fabs(t - 0.86f) * 36.0f);
        shared.compHistory[i].store(1.0f + 15.0f * hit,
                                    std::memory_order_relaxed);
      }
      shared.compHistoryHead.store(0);
    } else if (page == 2) {
      shared.set(P_EQ_LOW, 0.75);
      shared.set(P_EQ_LOW_MID, 0.40);
      shared.set(P_EQ_HIGH_MID, 0.58);
      shared.set(P_EQ_HIGH, 0.70);
      shared.set(P_EQ_HPF, 0.18);
      shared.set(P_EQ_LPF, 0.78);
    }

    PanelState state;
    state.page = page;
    for (int i = 0; i < 8; ++i) panel.tick(shared, state);
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.width, size.height);
    cairo_t* cr = cairo_create(surface);
    cairo_scale(cr, drawScale, drawScale);
    panel.draw(cr, shared, state);
    cairo_destroy(cr);
    const std::string path = outDir + "/" + name + ".png";
    cairo_surface_write_to_png(surface, path.c_str());
    cairo_surface_destroy(surface);
    std::printf("wrote %s\n", path.c_str());
  };

  render(0, false, 0, "kick");
  render(1, true, 0, "snare-ghost");
  for (int page = 1; page < 7; ++page) {
    char name[32];
    std::snprintf(name, sizeof(name), "page%d", page);
    render(1, false, page, name);
  }
  return 0;
}
