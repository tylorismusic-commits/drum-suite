// Drum Suite — panel layout test
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdio>
#include <vector>

#include <cairo.h>

#include "params.hpp"
#include "ui/panel.hpp"

using namespace drum;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}
constexpr double kPlateX = 16.0;
constexpr double kPlateY = 160.0;
constexpr double kPlateW = 1008.0;
constexpr double kPlateH = 524.0;

bool overlaps(const Panel::WidgetRect& a, const Panel::WidgetRect& b) {
  const double ax2 = a.x + a.w, ay2 = a.y + a.h;
  const double bx2 = b.x + b.w, by2 = b.y + b.h;
  return a.x < bx2 - 0.5 && b.x < ax2 - 0.5 && a.y < by2 - 0.5 &&
         b.y < ay2 - 0.5;
}
} // namespace

int main() {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_RGB24, kPanelWidth, kPanelHeight);
  cairo_t* cr = cairo_create(surface);
  Panel panel;
  Shared shared;
  panel.loadFont("assets/fonts");

  for (int page = 0; page < 7; ++page) {
    for (int drum = 0; drum < kDrumCount; ++drum) {
      PanelState state;
      state.page = page;
      shared.set(P_DRUM, static_cast<double>(drum));
      panel.draw(cr, shared, state);
      const std::vector<Panel::WidgetRect> rects = panel.widgetRects();

      bool onPlate = true;
      for (const auto& r : rects) {
        if (r.w <= 0.0 || r.h <= 0.0 || r.y < kPlateY) continue;
        if (r.x < kPlateX - 0.5 || r.y < kPlateY - 0.5 ||
            r.x + r.w > kPlateX + kPlateW + 0.5 ||
            r.y + r.h > kPlateY + kPlateH + 0.5) {
          std::printf("  page %d drum %d: '%s' at %.0f,%.0f %.0fx%.0f is off plate\n",
                      page, drum, r.label, r.x, r.y, r.w, r.h);
          onPlate = false;
        }
      }
      char label[128];
      std::snprintf(label, sizeof(label), "page %d drum %d controls sit on the plate",
                    page, drum);
      check(onPlate, label);

      bool clean = true;
      for (size_t i = 0; i < rects.size(); ++i) {
        for (size_t j = i + 1; j < rects.size(); ++j) {
          if (!overlaps(rects[i], rects[j])) continue;
          std::printf("  page %d drum %d: '%s' overlaps '%s'\n", page, drum,
                      rects[i].label, rects[j].label);
          clean = false;
        }
      }
      std::snprintf(label, sizeof(label), "page %d drum %d controls do not overlap",
                    page, drum);
      check(clean, label);
    }
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
