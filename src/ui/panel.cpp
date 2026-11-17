// Drum Suite — panel.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/panel.hpp"

#include "presets.hpp"
#include "value_text.hpp"
#include "dsp/wav.hpp"

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <complex>
#include <filesystem>
#include <memory>

#include <hdsp/biquad.hpp>

namespace drum {

namespace {

struct Rgb { double r, g, b; };

struct Skin {
  const char* caption;
  Rgb bodyTop, bodyBottom;
  Rgb legend, legendDim;
  Rgb headerBg;
  Rgb tabOff, tabOn, tabTextOff, tabTextOn;
  Rgb meterFrame, meterFace, meterNeedle, meterScale;
  Rgb knobCap, knobCapEdge, knobSkirt, knobIndicator;
  bool knurled;
  Rgb btnOff, btnOn, btnText, btnTextOn;
  Rgb screw;
  Rgb ghostOn, ghostOff;
};

Skin skinFor(int drum) {
  // One chrome, with a per-drum accent.  The body stays in the Comp Suite
  // material; the accent changes the tabs, indicators and ghost.
  static const Rgb accents[8] = {
      {0.95, 0.55, 0.18},  // Kick amber
      {0.92, 0.22, 0.18},  // Snare red
      {0.95, 0.38, 0.12},  // Tom orange
      {0.95, 0.78, 0.15},  // Hat yellow
      {0.40, 0.78, 0.35},  // Ride green
      {0.30, 0.58, 0.92},  // Room blue
      {0.25, 0.80, 0.78},  // OH teal
      {0.58, 0.60, 0.62},  // Bus grey
  };
  const Rgb a = accents[std::clamp(drum, 0, 7)];
  return {"DRUM SUITE  -  drum channel strip",
          {0.20, 0.18, 0.17}, {0.10, 0.09, 0.08},
          {0.94, 0.90, 0.84}, {0.66, 0.59, 0.52},
          {0.12, 0.10, 0.09},
          {0.28, 0.24, 0.21}, {a.r * 0.65, a.g * 0.65, a.b * 0.65},
          {0.82, 0.76, 0.68}, {0.98, 0.92, 0.82},
          {0.10, 0.08, 0.07}, {0.78, 0.74, 0.66},
          {0.12, 0.10, 0.09}, {0.28, 0.24, 0.20},
          {0.78, 0.74, 0.66}, {0.48, 0.42, 0.34},
          {0.16, 0.13, 0.11}, a,
          true,
          {0.28, 0.24, 0.20}, a,
          {0.88, 0.84, 0.76}, {1.0, 0.96, 0.88},
          {0.52, 0.47, 0.40},
          {1.0, 0.38, 0.12}, {0.42, 0.40, 0.42}};
}

template <size_t N>
std::vector<const char*> sc(const char* const (&labels)[N]) {
  return std::vector<const char*>(labels, labels + N);
}

const char* const kScaleDb12[] = {"-12", "0", "+12"};
const char* const kScaleDb18[] = {"-18", "0", "+18"};
const char* const kScaleGhost[] = {"OFF", "", "", "", "MAX"};

constexpr double kPi = 3.14159265358979323846;
constexpr double kPresetX = 72.0;
constexpr double kPresetY = 14.0;
constexpr double kPresetW = 330.0;
constexpr double kPresetH = 36.0;
constexpr double kListY = 56.0;
constexpr double kListRow = 22.0;
constexpr double kListVisible = 17.0;

std::string sampleDir() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  if (xdg && *xdg) return std::string(xdg) + "/drum-suite/samples";
  const char* home = std::getenv("HOME");
  if (home && *home) return std::string(home) + "/.local/share/drum-suite/samples";
  return "drum-suite/samples";
}

std::vector<std::string> listSampleFiles(const std::string& dir) {
  std::vector<std::string> names;
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return names;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file(ec)) continue;
    std::string name = entry.path().filename().string();
    std::string ext = entry.path().extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".wav") names.push_back(name);
  }
  std::sort(names.begin(), names.end());
  return names;
}

double nowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

double biquadMagDb(const hdsp::Biquad& b, double freq, double sr) {
  const double w = 2.0 * kPi * freq / sr;
  const std::complex<double> z1 = std::polar(1.0, -w);
  const std::complex<double> z2 = z1 * z1;
  const std::complex<double> num =
      static_cast<double>(b.b0) + static_cast<double>(b.b1) * z1 +
      static_cast<double>(b.b2) * z2;
  const std::complex<double> den =
      1.0 + static_cast<double>(b.a1) * z1 + static_cast<double>(b.a2) * z2;
  const double mag = std::abs(num / den);
  return 20.0 * std::log10(std::max(1e-9, mag));
}

std::string prettyPath(const std::string& path) {
  const char* home = std::getenv("HOME");
  if (home && *home) {
    const std::string h(home);
    if (path.rfind(h, 0) == 0) return "~" + path.substr(h.size());
  }
  return path;
}

std::string clippedName(const std::string& name, size_t maxChars) {
  if (name.size() <= maxChars) return name;
  return name.substr(0, maxChars - 1) + "\u2026";
}

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, 3 * kPi / 2);
  cairo_close_path(cr);
}

void setRgb(cairo_t* cr, Rgb c, double a = 1.0) {
  cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

Rgb mix(Rgb a, Rgb b, double t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
          a.b + (b.b - a.b) * t};
}

double knobAngle(double f) { return (135.0 + 270.0 * f) * kPi / 180.0; }
double clamp01(double v) { return std::min(std::max(v, 0.0), 1.0); }

const std::vector<int>& presetOrder() {
  static const std::vector<int> order = [] {
    std::vector<int> v;
    for (int i = 0; i < kPresetCount; ++i) v.push_back(i);
    std::stable_sort(v.begin(), v.end(), [](int a, int b) {
      const int c = std::strcmp(kPresets[a].category, kPresets[b].category);
      if (c != 0) return c < 0;
      return std::strcmp(kPresets[a].name, kPresets[b].name) < 0;
    });
    return v;
  }();
  return order;
}

void ghostPath(cairo_t* cr, double cx, double top, double w, double h) {
  const double half = w * 0.34;
  const double bottom = top + h * 0.78;
  cairo_move_to(cr, cx - half, bottom);
  cairo_line_to(cr, cx - half, top + h * 0.34);
  cairo_arc(cr, cx, top + h * 0.34, half, kPi, 2.0 * kPi);
  cairo_line_to(cr, cx + half, bottom);
  // Pac-Man-style wavy bottom.
  const double step = half * 2.0 / 3.0;
  cairo_line_to(cr, cx + half - step * 0.5, bottom - h * 0.08);
  cairo_line_to(cr, cx + half - step, bottom);
  cairo_line_to(cr, cx + half - step * 1.5, bottom - h * 0.08);
  cairo_line_to(cr, cx - half + step, bottom);
  cairo_line_to(cr, cx - half + step * 0.5, bottom - h * 0.08);
  cairo_close_path(cr);
}

} // namespace

Panel::Panel() {
  grain_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
  unsigned char* data = cairo_image_surface_get_data(grain_);
  const int stride = cairo_image_surface_get_stride(grain_);
  unsigned int seed = 0xB1EEDu;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      seed = seed * 1103515245u + 12345u;
      const unsigned v = (seed >> 16) & 0xff;
      const unsigned a = (v < 128) ? 16u : 0u;
      const unsigned c = (v < 128) ? 0u : 255u;
      unsigned char* px = data + y * stride + x * 4;
      px[0] = static_cast<unsigned char>(c);
      px[1] = static_cast<unsigned char>(c);
      px[2] = static_cast<unsigned char>(c);
      px[3] = static_cast<unsigned char>(a);
    }
  }
  cairo_surface_mark_dirty(grain_);
  fallbackFont_ = cairo_toy_font_face_create(
      "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  font_ = fallbackFont_;
  cairo_font_face_reference(font_);
}

Panel::~Panel() {
  if (grain_) cairo_surface_destroy(grain_);
  if (font_ && font_ != fallbackFont_) cairo_font_face_destroy(font_);
  if (fallbackFont_) cairo_font_face_destroy(fallbackFont_);
  if (ftFace_) FT_Done_Face(static_cast<FT_Face>(ftFace_));
  if (ft_) FT_Done_FreeType(static_cast<FT_Library>(ft_));
}

void Panel::loadFont(const std::string& dir) {
  if (ft_) return;
  std::string path = dir;
  if (!path.empty() && path.back() != '/') path += '/';
  path += "Jost-ExtraBold.ttf";
  FT_Library lib = nullptr;
  if (FT_Init_FreeType(&lib) != 0) return;
  FT_Face face = nullptr;
  if (FT_New_Face(lib, path.c_str(), 0, &face) != 0) {
    FT_Done_FreeType(lib);
    return;
  }
  cairo_font_face_t* font = cairo_ft_font_face_create_for_ft_face(face, 0);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    cairo_font_face_destroy(font);
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    return;
  }
  if (font_ && font_ != fallbackFont_) cairo_font_face_destroy(font_);
  ft_ = lib;
  ftFace_ = face;
  font_ = font;
}

void Panel::text(cairo_t* cr, const char* s, double x, double y, double size,
                 int align, double r, double g, double b) const {
  cairo_set_font_face(cr, font_);
  cairo_set_font_size(cr, size);
  cairo_set_source_rgb(cr, r, g, b);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  double tx = x;
  if (align == 1) tx = x - e.width * 0.5 - e.x_bearing;
  if (align == 2) tx = x - e.width - e.x_bearing;
  cairo_move_to(cr, tx, y);
  cairo_show_text(cr, s);
}

void Panel::layout(const Shared& shared, const PanelState& state) {
  (void)shared;
  (void)state;
  widgets_.clear();

  Widget p{};
  p.kind = Kind::Power;
  p.param = P_POWER;
  p.x = 34;
  p.y = 30;
  p.r = 14;
  widgets_.push_back(p);

  // Page tabs in signal-path order.
  static const char* kPageNames[7] = {"BLEED OUT", "SAMPLE", "EQ", "COMP",
                                      "DRIVE", "CLIP", "LIMIT"};
  const double pageW = (980.0 - 6.0 * 6.0) / 7.0;
  for (int i = 0; i < 7; ++i) {
    Widget t{};
    t.kind = Kind::PageTab;
    t.value = i;
    t.x = 30.0 + i * (pageW + 6.0);
    t.y = 70;
    t.w = pageW;
    t.h = 34;
    t.label = kPageNames[i];
    widgets_.push_back(t);
  }

  // Drum selector row.
  const double tabW = (980.0 - 7.0 * 6.0) / 8.0;
  for (int i = 0; i < 8; ++i) {
    Widget t{};
    t.kind = Kind::DrumTab;
    t.param = P_DRUM;
    t.value = i;
    t.x = 30.0 + i * (tabW + 6.0);
    t.y = 114;
    t.w = tabW;
    t.h = 34;
    t.label = kDrumLabels[i];
    widgets_.push_back(t);
  }

  auto pill = [&](uint32_t param, double x, double y, double w, double h,
                  const char* label) {
    Widget b{};
    b.kind = Kind::Pill;
    b.param = param;
    b.x = x;
    b.y = y;
    b.w = w;
    b.h = h;
    b.label = label;
    widgets_.push_back(b);
  };

  auto toggle = [&](uint32_t param, double x, double y, const char* label) {
    Widget t{};
    t.kind = Kind::Toggle;
    t.param = param;
    t.x = x;
    t.y = y;
    t.w = 1.0;
    t.h = 48;
    t.label = label;
    t.buttonText = {label};
    widgets_.push_back(t);
  };

  auto knob = [&](uint32_t param, double x, double y, double r, const char* label,
                  std::vector<const char*> scale = {}, bool below = false,
                  int steps = 0, double scaleRadius = 1.44) {
    Widget k{};
    k.kind = Kind::Knob;
    k.param = param;
    k.x = x;
    k.y = y;
    k.r = r;
    k.steps = steps;
    k.scaleRadius = scaleRadius;
    k.label = label;
    k.scale = std::move(scale);
    k.labelBelow = below;
    widgets_.push_back(k);
  };

  if (state.page == 0) {
    // Bleed Out: the three controls that matter.
    knob(P_THRESHOLD, 220, 420, 46, "THRESHOLD");
    knob(P_DEBLEED, 500, 420, 46, "DEBLEED");
    knob(P_GHOST, 780, 420, 42, "GHOST AMOUNT", sc(kScaleGhost));
    Widget g{};
    g.kind = Kind::Ghost;
    g.param = P_GHOST_MODE;
    g.x = 850;
    g.y = 390;
    g.w = 72;
    g.h = 72;
    g.label = "GHOST MODE";
    widgets_.push_back(g);
  }

  if (state.page == 1) {
    // Sample page.
    pill(P_SAMPLE_ON, 760, 318, 150, 24, "SAMPLE ON");
    knob(P_SAMPLE_LEVEL, 500, 410, 24, "LEVEL");
    knob(P_SAMPLE_DIRECT, 610, 410, 24, "DIRECT");
    knob(P_SAMPLE_PITCH, 720, 410, 24, "PITCH");
    knob(P_SAMPLE_START, 830, 410, 24, "START");
    toggle(P_SAMPLE_REVERSE, 500, 520, "REVERSE");
    toggle(P_SAMPLE_CHOKE, 610, 520, "CHOKE");
    knob(P_SAMPLE_FADE, 720, 520, 22, "FADE");
    knob(P_SAMPLE_TRIGGER, 830, 520, 22, "TRIGGER",
         sc(kSampleTriggerLabels), false, 3);
  } else if (state.page == 2) {
    // EQ page.
    knob(P_EQ_LOW, 190, 535, 26, "LOW");
    knob(P_EQ_LOW_MID, 310, 535, 26, "LOW MID");
    knob(P_EQ_HIGH_MID, 430, 535, 26, "HIGH MID");
    knob(P_EQ_HIGH, 550, 535, 26, "HIGH");
    knob(P_EQ_HPF, 680, 535, 24, "HPF");
    knob(P_EQ_LPF, 800, 535, 24, "LPF");
  } else if (state.page == 3) {
    // Compressor.  The model tabs sit above the GR history.
    for (int i = 0; i < 3; ++i) {
      Widget b{};
      b.kind = Kind::Button;
      b.param = P_COMP_MODEL;
      b.value = i;
      b.x = 52.0 + i * 108.0;
      b.y = 182.0;
      b.w = 98.0;
      b.h = 28.0;
      b.label = kCompModelLabels[i];
      widgets_.push_back(b);
    }
    pill(P_COMP_ON, 396, 182, 120, 28, "COMP ON");
    const int model = static_cast<int>(shared.get(P_COMP_MODEL) + 0.5);
    knob(P_COMP_THRESHOLD, 170, 445, 26, "THRESHOLD");
    knob(P_COMP_RATIO, 310, 445, 26, model == 1 ? "LIMIT" : "RATIO", {},
         false, 9);
    if (model != 1) {
      knob(P_COMP_ATTACK, 450, 445, 26, "ATTACK");
      knob(P_COMP_RELEASE, 590, 445, 26, "RELEASE");
    }
    knob(P_COMP_MAKEUP, 310, 550, 24, "MAKEUP", {}, true);
    knob(P_COMP_MIX, 450, 550, 24, "MIX", {}, true);
    knob(P_COMP_SC_HPF, 590, 550, 24, "SC HPF", {}, true);
  }

  // Global.  Power is the bypass; the bottom corners are just trim.
  knob(P_INPUT, 130, 575, 22, "INPUT", sc(kScaleDb18));
  knob(P_OUTPUT, 900, 575, 22, "OUTPUT", sc(kScaleDb18));
}

bool Panel::hitWidget(const Widget& w, double x, double y) const {
  switch (w.kind) {
    case Kind::Knob: {
      const double dx = x - w.x, dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.35;
    }
    case Kind::Power: {
      const double dx = x - w.x, dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.4;
    }
    case Kind::Toggle:
      return x >= w.x - w.w * 0.5 && x <= w.x + w.w * 0.5 && y >= w.y &&
             y <= w.y + w.h;
    default:
      return x >= w.x && x <= w.x + w.w && y >= w.y && y <= w.y + w.h;
  }
}

double Panel::presetResetValue(const PanelState& state, uint32_t param) const {
  double value = kParamSpecs[param].def;
  if (state.presetIndex >= 0 && state.presetIndex < kPresetCount) {
    for (int i = 0; i < kPresets[state.presetIndex].count; ++i) {
      const PresetValue& v = kPresets[state.presetIndex].values[i];
      if (v.param == static_cast<int>(param)) {
        value = v.value;
        break;
      }
    }
  }
  return value;
}

bool Panel::pointerDown(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  (void)fine;
  if (handlePresetClick(shared, state, x, y)) return true;
  // Sample file list.
  if (state.page == 1 && x >= 52 && x <= 452 && y >= 230 && y <= 420) {
    const int row = static_cast<int>((y - 232) / 23) + state.sampleScroll;
    if (row >= 0 && row < static_cast<int>(state.sampleFiles.size())) {
      loadSample(shared, state, sampleDir() + "/" + state.sampleFiles[row]);
      return true;
    }
  }
  for (const Widget& w : widgets_) {
    if (!hitWidget(w, x, y)) continue;
    switch (w.kind) {
      case Kind::Knob: {
        const double now = nowMs();
        const bool doubleClick = lastClickParam_ == static_cast<int>(w.param) &&
                                 now - lastClickTime_ <= kDoubleClickMs;
        lastClickParam_ = static_cast<int>(w.param);
        lastClickTime_ = now;
        if (doubleClick) {
          shared.set(w.param, presetResetValue(state, w.param));
          state.presetDirty = true;
          dragParam_ = -1;
          return true;
        }
        dragParam_ = static_cast<int>(w.param);
        dragSteps_ = w.steps;
        dragStartY_ = y;
        dragStartValue_ = shared.get(w.param);
        if (dragSteps_ > 1) dragStartValue_ = std::min(dragStartValue_, dragSteps_ - 1.0);
        return false;
      }
      case Kind::Power:
        shared.set(P_POWER, shared.get(P_POWER) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::PageTab:
        state.page = static_cast<int>(w.value);
        return true;
      case Kind::DrumTab: {
        const int drum = static_cast<int>(w.value);
        shared.set(P_DRUM, w.value);
        applyDrumDefaults(shared, drum);
        state.presetDirty = true;
        return true;
      }
      case Kind::Pill:
        shared.set(w.param, shared.get(w.param) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::Ghost:
        shared.set(P_GHOST_MODE, shared.get(P_GHOST_MODE) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::Button:
        shared.set(w.param, w.value);
        state.presetDirty = true;
        return true;
      case Kind::Toggle: {
        const double f = (y - w.y) / w.h;
        shared.set(w.param, f < 0.5 ? 1.0 : 0.0);
        state.presetDirty = true;
        return true;
      }
    }
  }
  return false;
}

bool Panel::pointerMove(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  (void)x;
  if (dragParam_ < 0) return false;
  const uint32_t param = static_cast<uint32_t>(dragParam_);
  const ParamSpec& spec = kParamSpecs[param];
  const double perPixel = spec.stepped
                              ? 1.0 / 18.0
                              : (spec.max - spec.min) / (fine ? 800.0 : 200.0);
  double v = dragStartValue_ + (dragStartY_ - y) * perPixel;
  if (dragSteps_ > 1) v = std::min(v, dragSteps_ - 1.0);
  shared.set(param, v);
  state.presetDirty = true;
  return true;
}

void Panel::pointerUp() { dragParam_ = -1; }

bool Panel::wheel(Shared& shared, PanelState& state, double x, double y,
                  double dy) {
  if (state.presetOpen && x >= kPresetX && x <= kPresetX + kPresetW &&
      y >= kListY && y <= kListY + kListVisible * kListRow + 10) {
    int rows = kPresetCount;
    const char* last = nullptr;
    for (int i : presetOrder()) {
      if (!last || std::strcmp(last, kPresets[i].category) != 0) {
        last = kPresets[i].category;
        ++rows;
      }
    }
    const int maxScroll = std::max(0, rows - static_cast<int>(kListVisible));
    state.presetScroll = std::min(maxScroll, std::max(0, state.presetScroll - (int)dy));
    return true;
  }
  if (state.page == 1 && x >= 52 && x <= 452 && y >= 330 && y <= 600) {
    const int maxScroll =
        std::max(0, static_cast<int>(state.sampleFiles.size()) - 8);
    state.sampleScroll =
        std::min(maxScroll, std::max(0, state.sampleScroll - (dy > 0 ? 1 : -1)));
    return true;
  }
  for (const Widget& w : widgets_) {
    if (w.kind == Kind::Knob && hitWidget(w, x, y)) {
      const ParamSpec& spec = kParamSpecs[w.param];
      if (spec.stepped) {
        shared.set(w.param, shared.get(w.param) - (dy > 0 ? 1.0 : -1.0));
      } else {
        shared.set(w.param, shared.get(w.param) - dy * (spec.max - spec.min) * 0.01);
      }
      state.presetDirty = true;
      return true;
    }
  }
  return false;
}

void Panel::tick(const Shared& shared, PanelState& state) const {
  state.inLevelL = shared.inPeakLDb.load(std::memory_order_relaxed);
  state.inLevelR = shared.inPeakRDb.load(std::memory_order_relaxed);
  state.outLevelL = shared.outPeakLDb.load(std::memory_order_relaxed);
  state.outLevelR = shared.outPeakRDb.load(std::memory_order_relaxed);
  state.grDb = shared.grDb.load(std::memory_order_relaxed);
  state.detectorDb = shared.detectorDb.load(std::memory_order_relaxed);
  state.gate = shared.gate.load(std::memory_order_relaxed);
  state.ghostActive = shared.ghostActive.load(std::memory_order_relaxed);
  state.compGrDb = shared.compGrDb.load(std::memory_order_relaxed);
  {
    const int ch = shared.compHistoryHead.load(std::memory_order_relaxed) &
                   (Shared::kScopeSize - 1);
    for (int i = 0; i < Shared::kScopeSize; ++i) {
      const int idx = (ch + i) & (Shared::kScopeSize - 1);
      state.compHistory[i] =
          shared.compHistory[idx].load(std::memory_order_relaxed);
    }
  }
  if (++state.sampleScanTicks >= 60) {
    state.sampleScanTicks = 0;
    state.sampleFiles = listSampleFiles(sampleDir());
    const int maxScroll = std::max(0, static_cast<int>(state.sampleFiles.size()) - 10);
    state.sampleScroll = std::min(std::max(state.sampleScroll, 0), maxScroll);
  }
  const int head = shared.scopeHead.load(std::memory_order_relaxed) &
                   (Shared::kScopeSize - 1);
  for (int i = 0; i < Shared::kScopeSize; ++i) {
    const int idx = (head + i) & (Shared::kScopeSize - 1);
    state.scopeIn[i] = shared.scopeIn[idx].load(std::memory_order_relaxed);
    state.scopeGate[i] = shared.scopeGate[idx].load(std::memory_order_relaxed);
  }
}

void Panel::draw(cairo_t* cr, Shared& shared, const PanelState& state) {
  layout(shared, state);
  const int drum = std::clamp(static_cast<int>(shared.get(P_DRUM) + 0.5), 0, 7);

  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.07, 0.07, 0.07);
  cairo_paint(cr);

  drawHeader(cr, shared, state);
  drawBody(cr, drum, state.page);
  if (state.page == 0) {
    drawScope(cr, shared, state);
  } else if (state.page == 1) {
    drawSamplePage(cr, shared, state);
  } else if (state.page == 2) {
    drawEqPage(cr, shared, state);
  } else if (state.page == 3) {
    drawCompPage(cr, shared, state);
  } else {
    drawPlaceholderPage(cr, state.page);
  }
  drawLevelMeter(cr, 18, 174, 20, 430, state.inLevelL, state.inLevelR, -60.0,
                 0.0, true);
  drawLevelMeter(cr, 980, 174, 20, 430, state.outLevelL, state.outLevelR,
                 -60.0, 3.0, false);

  for (const Widget& w : widgets_) {
    drawWidget(cr, w, shared, state, drum);
  }

  // While a knob is being dragged, show its exact value in the footer so the
  // three controls never feel blind.
  if (dragParam_ >= 0) {
    const uint32_t p = static_cast<uint32_t>(dragParam_);
    const std::string value =
        std::string(kParamSpecs[p].name) + "   " +
        formatParamValue(shared, p, shared.get(p));
    text(cr, value.c_str(), kPanelWidth * 0.5, 709, 15, 1, 0.98, 0.80, 0.32);
  }

  if (editing_) drawTextEntry(cr, shared);
  if (state.presetOpen) drawPresetList(cr, shared, state);

  cairo_restore(cr);
}

void Panel::drawHeader(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  (void)shared;
  const int drum = static_cast<int>(shared.get(P_DRUM) + 0.5);
  const Skin skin = skinFor(drum);
  cairo_pattern_t* bar = cairo_pattern_create_linear(0, 0, 0, 62);
  cairo_pattern_add_color_stop_rgb(bar, 0.0, skin.headerBg.r * 1.35,
                                   skin.headerBg.g * 1.35, skin.headerBg.b * 1.35);
  cairo_pattern_add_color_stop_rgb(bar, 1.0, skin.headerBg.r * 0.75,
                                   skin.headerBg.g * 0.75, skin.headerBg.b * 0.75);
  cairo_rectangle(cr, 0, 0, kPanelWidth, 62);
  cairo_set_source(cr, bar);
  cairo_fill(cr);
  cairo_pattern_destroy(bar);

  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 0, 0.5);
  cairo_line_to(cr, kPanelWidth, 0.5);
  cairo_stroke(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
  cairo_move_to(cr, 0, 61.5);
  cairo_line_to(cr, kPanelWidth, 61.5);
  cairo_stroke(cr);

  text(cr, "DRUM SUITE", 520, 38, 19, 1, 0.86, 0.82, 0.76);
  drawPresetBar(cr, shared, state);
}

void Panel::drawBody(cairo_t* cr, int drum, int page) const {
  const Skin skin = skinFor(drum);
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, 160, 0, 684);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, skin.bodyTop.r, skin.bodyTop.g,
                                   skin.bodyTop.b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, skin.bodyBottom.r,
                                   skin.bodyBottom.g, skin.bodyBottom.b);
  roundRect(cr, 16, 160, 1008, 524, 6);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);

  if (grain_) {
    cairo_pattern_t* grain = cairo_pattern_create_for_surface(grain_);
    cairo_pattern_set_extend(grain, CAIRO_EXTEND_REPEAT);
    cairo_save(cr);
    cairo_rectangle(cr, 17, 161, 1006, 522);
    cairo_clip(cr);
    cairo_set_source(cr, grain);
    cairo_paint_with_alpha(cr, 0.018);
    cairo_restore(cr);
    cairo_pattern_destroy(grain);
  }

  cairo_set_line_width(cr, 1.0);
  setRgb(cr, {1.0, 1.0, 1.0}, 0.16);
  cairo_move_to(cr, 18, 161.5);
  cairo_line_to(cr, 1022, 161.5);
  cairo_stroke(cr);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.22);
  cairo_move_to(cr, 18, 682.5);
  cairo_line_to(cr, 1022, 682.5);
  cairo_stroke(cr);

  for (double sx : {32.0, 1008.0}) {
    for (double sy : {176.0, 668.0}) {
      cairo_arc(cr, sx, sy, 4.5, 0, 2 * kPi);
      setRgb(cr, skin.screw, 0.9);
      cairo_fill(cr);
    }
  }

  // Drum selector divider.
  setRgb(cr, {0.0, 0.0, 0.0}, 0.24);
  cairo_move_to(cr, 30, 154.5);
  cairo_line_to(cr, 1010, 154.5);
  cairo_stroke(cr);
  setRgb(cr, {1.0, 1.0, 1.0}, 0.08);
  cairo_move_to(cr, 30, 155.5);
  cairo_line_to(cr, 1010, 155.5);
  cairo_stroke(cr);

  // Explain why the Bleed Out page has only three knobs.
  if (page == 0) {
    text(cr, "GATE, TONE AND SATURATION AUTOMATED BY PROFILE",
         kPanelWidth * 0.5, 520, 10, 1, skin.legendDim.r, skin.legendDim.g,
         skin.legendDim.b);
  }

  // Footer.
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
  cairo_rectangle(cr, 0, 684, kPanelWidth, 36);
  cairo_fill(cr);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
  cairo_move_to(cr, 0, 684.5);
  cairo_line_to(cr, kPanelWidth, 684.5);
  cairo_stroke(cr);
  static const char* const kPageCaptions[7] = {
      "BLEED OUT  -  multiband de-bleed gate",
      "SAMPLE  -  loader, pitch and gate trigger",
      "EQ  -  4-band + HPF/LPF",
      "COMP  -  76 / 2A / VCA",
      "DRIVE  -  valve / console / tape (next milestone)",
      "CLIP  -  soft / hard / diode (next milestone)",
      "LIMIT  -  true-peak safety (next milestone)"};
  text(cr, kPageCaptions[std::clamp(page, 0, 6)], kPanelWidth * 0.5, 709, 15,
       1, 0.86, 0.82, 0.76);
}

void Panel::drawPlaceholderPage(cairo_t* cr, int page) const {
  static const char* const kNames[7] = {"BLEED OUT", "SAMPLE", "EQ", "COMP",
                                        "DRIVE", "CLIP", "LIMIT"};
  const int p = std::clamp(page, 0, 6);
  const Skin skin = skinFor(0);
  text(cr, kNames[p], 520, 250, 32, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  text(cr, "MODULE RESERVED FOR THE NEXT DRUM SUITE MILESTONE", 520, 280, 10,
       1, skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);

  // Signal-path breadcrumb.
  const double x0 = 170.0, x1 = 870.0;
  const double y = 430.0;
  const double step = (x1 - x0) / 6.0;
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.legendDim, 0.30);
  cairo_move_to(cr, x0, y);
  cairo_line_to(cr, x1, y);
  cairo_stroke(cr);
  for (int i = 0; i < 7; ++i) {
    const double x = x0 + step * i;
    cairo_arc(cr, x, y, i == p ? 5.0 : 3.0, 0, 2 * kPi);
    setRgb(cr, i == p ? skin.btnOn : skin.legendDim, i == p ? 1.0 : 0.45);
    cairo_fill(cr);
    text(cr, kNames[i], x, y + 22, 9.5, 1,
         i == p ? skin.btnOn.r : skin.legendDim.r,
         i == p ? skin.btnOn.g : skin.legendDim.g,
         i == p ? skin.btnOn.b : skin.legendDim.b);
  }
}

void Panel::updateWaveCache(const std::shared_ptr<const SampleBuffer>& sample,
                           int cols) const {
  if (!sample || sample->data.empty() || cols <= 0) {
    waveSample_.reset();
    waveWidth_ = 0;
    waveMin_.clear();
    waveMax_.clear();
    return;
  }
  if (waveSample_ == sample && waveWidth_ == cols) return;
  waveSample_ = sample;
  waveWidth_ = cols;
  waveMin_.assign(static_cast<size_t>(cols), 0.0f);
  waveMax_.assign(static_cast<size_t>(cols), 0.0f);
  const auto& data = sample->data;
  const int len = static_cast<int>(data.size());
  for (int x = 0; x < cols; ++x) {
    const int a = static_cast<int>(static_cast<int64_t>(x) * len / cols);
    const int b = std::max(
        a + 1, static_cast<int>(static_cast<int64_t>(x + 1) * len / cols));
    float lo = 0.0f, hi = 0.0f;
    for (int i = a; i < b && i < len; ++i) {
      lo = std::min(lo, data[static_cast<size_t>(i)]);
      hi = std::max(hi, data[static_cast<size_t>(i)]);
    }
    waveMin_[static_cast<size_t>(x)] = lo;
    waveMax_[static_cast<size_t>(x)] = hi;
  }
}

void Panel::drawSamplePage(cairo_t* cr, const Shared& shared,
                           const PanelState& state) const {
  const int drum = std::clamp(static_cast<int>(shared.get(P_DRUM) + 0.5), 0, 7);
  const Skin skin = skinFor(drum);
  const auto sample = shared.sample();

  // Waveform well.
  const double wx = 470, wy = 190, ww = 500, wh = 118;
  roundRect(cr, wx, wy, ww, wh, 4);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.38);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.legendDim, 0.28);
  roundRect(cr, wx + 0.5, wy + 0.5, ww - 1.0, wh - 1.0, 4);
  cairo_stroke(cr);
  setRgb(cr, skin.legendDim, 0.16);
  cairo_move_to(cr, wx + 6, wy + wh * 0.5);
  cairo_line_to(cr, wx + ww - 6, wy + wh * 0.5);
  cairo_stroke(cr);

  if (sample && !sample->data.empty()) {
    const int cols = static_cast<int>(ww - 8);
    updateWaveCache(sample, cols);
    const double cy = wy + wh * 0.5;
    const double half = wh * 0.46;
    cairo_set_line_width(cr, 1.0);
    setRgb(cr, skin.btnOn, 0.9);
    for (int x = 0; x < cols && x < static_cast<int>(waveMin_.size()); ++x) {
      const double px = wx + 4 + x;
      cairo_move_to(cr, px,
                    cy - std::clamp(static_cast<double>(waveMax_[x]), -1.0, 1.0) * half);
      cairo_line_to(cr, px,
                    cy - std::clamp(static_cast<double>(waveMin_[x]), -1.0, 1.0) * half);
      cairo_stroke(cr);
    }
  } else {
    text(cr, "NO SAMPLE LOADED", wx + ww * 0.5, wy + wh * 0.5 - 6, 14, 1,
         skin.legend.r, skin.legend.g, skin.legend.b);
    text(cr, "PUT WAV FILES IN THE FOLDER BELOW, THEN CLICK ONE", wx + ww * 0.5,
         wy + wh * 0.5 + 15, 9, 1, skin.legendDim.r, skin.legendDim.g,
         skin.legendDim.b);
  }

  text(cr, "SAMPLE WAVEFORM", wx, wy - 10, 9, 0, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);

  if (sample && !sample->data.empty()) {
    char info[160];
    const double seconds = sample->sampleRate > 0
                               ? static_cast<double>(sample->data.size()) /
                                     sample->sampleRate
                               : 0.0;
    std::snprintf(info, sizeof(info), "%s   -   48 kHz   -   %.2f s",
                  clippedName(sample->name, 34).c_str(), seconds);
    text(cr, info, wx, wy + wh + 18, 10, 0, skin.legend.r, skin.legend.g,
         skin.legend.b);
  } else {
    text(cr, "The sample is added after the gate; use the trigger mode to "
             "choose what fires it.",
         wx, wy + wh + 18, 9, 0, skin.legendDim.r, skin.legendDim.g,
         skin.legendDim.b);
  }

  // File browser.
  const double bx = 52, by = 190, bw = 400, bh = 240;
  roundRect(cr, bx, by, bw, bh, 5);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.30);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.legendDim, 0.32);
  roundRect(cr, bx + 0.5, by + 0.5, bw - 1.0, bh - 1.0, 5);
  cairo_stroke(cr);

  text(cr, "SAMPLES", bx + 14, by + 18, 10, 0, skin.legend.r, skin.legend.g,
       skin.legend.b);
  text(cr, clippedName(prettyPath(sampleDir()), 34).c_str(), bx + bw - 14,
       by + 18, 8.5, 2,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  setRgb(cr, skin.legendDim, 0.25);
  cairo_move_to(cr, bx + 12, by + 26.5);
  cairo_line_to(cr, bx + bw - 12, by + 26.5);
  cairo_stroke(cr);

  if (state.sampleFiles.empty()) {
    text(cr, "NO .WAV FILES IN THIS FOLDER", bx + bw * 0.5, by + 92, 10, 1,
         skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
    text(cr, "THE LIST REFRESHES AUTOMATICALLY", bx + bw * 0.5, by + 112, 8.5,
         1, skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  } else {
    const std::string loaded = sample ? sample->name : std::string();
    for (int i = 0; i < 8; ++i) {
      const int index = state.sampleScroll + i;
      if (index >= static_cast<int>(state.sampleFiles.size())) break;
      const std::string& name = state.sampleFiles[static_cast<size_t>(index)];
      const bool selected = name == loaded;
      const double rowY = 232 + i * 23;
      if (selected) {
        roundRect(cr, bx + 8, rowY, bw - 16, 21, 3);
        setRgb(cr, skin.btnOn, 0.20);
        cairo_fill(cr);
      }
      text(cr, clippedName(name, 44).c_str(), bx + 16, rowY + 15, 10, 0,
           selected ? skin.btnTextOn.r : skin.legend.r,
           selected ? skin.btnTextOn.g : skin.legend.g,
           selected ? skin.btnTextOn.b : skin.legend.b);
    }
    if (static_cast<int>(state.sampleFiles.size()) > 8) {
      text(cr, "SCROLL TO BROWSE", bx + bw - 14, by + bh - 8, 8.5, 2,
           skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
    }
  }
}

void Panel::drawEqPage(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  (void)state;
  const int drum = std::clamp(static_cast<int>(shared.get(P_DRUM) + 0.5), 0, 7);
  const Skin skin = skinFor(drum);

  const float sr = 48000.0f;
  const float lowGain = static_cast<float>((shared.get(P_EQ_LOW) - 0.5) * 24.0);
  const float lowMidGain =
      static_cast<float>((shared.get(P_EQ_LOW_MID) - 0.5) * 24.0);
  const float highMidGain =
      static_cast<float>((shared.get(P_EQ_HIGH_MID) - 0.5) * 24.0);
  const float highGain = static_cast<float>((shared.get(P_EQ_HIGH) - 0.5) * 24.0);
  const float hpFreq = eqHpfHz(static_cast<float>(shared.get(P_EQ_HPF)));
  const float lpFreq = eqLpfHz(static_cast<float>(shared.get(P_EQ_LPF)));

  hdsp::Biquad filters[6];
  filters[0].lowShelf(80.0f, lowGain, 0.7f, sr);
  filters[1].peak(250.0f, lowMidGain, 0.9f, sr);
  filters[2].peak(1200.0f, highMidGain, 0.9f, sr);
  filters[3].highShelf(6000.0f, highGain, 0.7f, sr);
  if (shared.get(P_EQ_HPF) > 0.001) filters[4].highPass(hpFreq, 0.707f, sr);
  if (shared.get(P_EQ_LPF) < 0.999) filters[5].lowPass(lpFreq, 0.707f, sr);

  const double gx = 150, gy = 190, gw = 740, gh = 250;
  roundRect(cr, gx, gy, gw, gh, 4);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.38);
  cairo_fill(cr);

  const double dbTop = 24.0, dbBottom = -24.0;
  auto xForHz = [&](double hz) {
    return gx + gw * std::log(std::max(20.0, hz) / 20.0) / std::log(1000.0);
  };
  auto yForDb = [&](double db) {
    return gy + (dbTop - std::clamp(db, dbBottom, dbTop)) /
                    (dbTop - dbBottom) * gh;
  };

  cairo_set_line_width(cr, 1.0);
  for (double db : {24.0, 12.0, 0.0, -12.0, -24.0}) {
    setRgb(cr, skin.legendDim, db == 0.0 ? 0.32 : 0.15);
    cairo_move_to(cr, gx, yForDb(db));
    cairo_line_to(cr, gx + gw, yForDb(db));
    cairo_stroke(cr);
  }
  for (double hz : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0,
                    10000.0}) {
    setRgb(cr, skin.legendDim, 0.12);
    cairo_move_to(cr, xForHz(hz), gy);
    cairo_line_to(cr, xForHz(hz), gy + gh);
    cairo_stroke(cr);
  }

  constexpr int kPoints = 260;
  double curve[kPoints];
  for (int i = 0; i < kPoints; ++i) {
    const double t = static_cast<double>(i) / (kPoints - 1);
    const double hz = 20.0 * std::pow(1000.0, t);
    double db = 0.0;
    for (auto& f : filters) db += biquadMagDb(f, hz, sr);
    curve[i] = db;
  }

  // Shade between the curve and the 0 dB line.
  cairo_new_path(cr);
  cairo_move_to(cr, gx, yForDb(0.0));
  for (int i = 0; i < kPoints; ++i) {
    cairo_line_to(cr, gx + gw * i / (kPoints - 1), yForDb(curve[i]));
  }
  cairo_line_to(cr, gx + gw, yForDb(0.0));
  cairo_close_path(cr);
  setRgb(cr, skin.btnOn, 0.12);
  cairo_fill(cr);

  cairo_new_path(cr);
  for (int i = 0; i < kPoints; ++i) {
    const double x = gx + gw * i / (kPoints - 1);
    if (i == 0) cairo_move_to(cr, x, yForDb(curve[i]));
    else cairo_line_to(cr, x, yForDb(curve[i]));
  }
  cairo_set_line_width(cr, 2.0);
  setRgb(cr, skin.btnOn, 0.95);
  cairo_stroke(cr);

  setRgb(cr, skin.legendDim, 0.35);
  roundRect(cr, gx + 0.5, gy + 0.5, gw - 1.0, gh - 1.0, 4);
  cairo_stroke(cr);

  for (double hz : {20.0, 100.0, 1000.0, 10000.0, 20000.0}) {
    char label[16];
    if (hz >= 1000.0) std::snprintf(label, sizeof(label), "%.0fk", hz / 1000.0);
    else std::snprintf(label, sizeof(label), "%.0f", hz);
    text(cr, label, xForHz(hz), gy + gh + 15, 9, 1, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }
  for (double db : {24.0, 12.0, 0.0, -12.0, -24.0}) {
    char label[16];
    if (db == 0.0) std::snprintf(label, sizeof(label), "0");
    else std::snprintf(label, sizeof(label), "%+.0f", db);
    text(cr, label, gx - 10, yForDb(db) + 3, 9, 2, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }

  char hp[32], lp[32];
  if (shared.get(P_EQ_HPF) > 0.001)
    std::snprintf(hp, sizeof(hp), "HPF %.0f Hz", hpFreq);
  else
    std::snprintf(hp, sizeof(hp), "HPF OFF");
  if (shared.get(P_EQ_LPF) < 0.999)
    std::snprintf(lp, sizeof(lp), "LPF %.0f Hz", lpFreq);
  else
    std::snprintf(lp, sizeof(lp), "LPF OFF");
  text(cr, hp, gx, gy - 10, 9, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
  text(cr, lp, gx + gw, gy - 10, 9, 2, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
}

void Panel::drawCompPage(cairo_t* cr, const Shared& shared,
                         const PanelState& state) const {
  const int drum = std::clamp(static_cast<int>(shared.get(P_DRUM) + 0.5), 0, 7);
  const Skin skin = skinFor(drum);
  const int model = std::clamp(static_cast<int>(shared.get(P_COMP_MODEL) + 0.5),
                               0, 2);

  // Gain-reduction history.
  const double gx = 52, gy = 240, gw = 480, gh = 130;
  roundRect(cr, gx, gy, gw, gh, 4);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.40);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  for (int db = 0; db <= 24; db += 6) {
    const double y = gy + gh * db / 24.0;
    setRgb(cr, skin.legendDim, db == 0 ? 0.30 : 0.14);
    cairo_move_to(cr, gx, y);
    cairo_line_to(cr, gx + gw, y);
    cairo_stroke(cr);
    char label[8];
    std::snprintf(label, sizeof(label), "-%d", db);
    text(cr, label, gx - 8, y + 3, 8.5, 2, skin.legendDim.r, skin.legendDim.g,
         skin.legendDim.b);
  }
  setRgb(cr, skin.legendDim, 0.35);
  roundRect(cr, gx + 0.5, gy + 0.5, gw - 1.0, gh - 1.0, 4);
  cairo_stroke(cr);

  const int n = Shared::kScopeSize;
  const double barW = gw / n;
  setRgb(cr, skin.btnOn, 0.85);
  for (int i = 0; i < n; ++i) {
    const double gr = std::clamp(static_cast<double>(state.compHistory[i]),
                                 0.0, 24.0);
    if (gr < 0.15) continue;
    const double h = gh * gr / 24.0;
    cairo_rectangle(cr, gx + i * barW + 0.5, gy + gh - h,
                    std::max(1.0, barW - 1.0), h);
    cairo_fill(cr);
  }

  text(cr, "GAIN REDUCTION", gx + 24, gy - 8, 9, 0, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
  char current[32];
  std::snprintf(current, sizeof(current), "%.1f dB",
                std::clamp(state.compGrDb, 0.0f, 60.0f));
  text(cr, current, gx + gw, gy - 8, 11, 2, skin.btnOn.r, skin.btnOn.g,
       skin.btnOn.b);

  // Model card.
  const double cx = 560, cy = 240, cw = 410, ch = 130;
  roundRect(cr, cx, cy, cw, ch, 4);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.22);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.legendDim, 0.25);
  roundRect(cr, cx + 0.5, cy + 0.5, cw - 1.0, ch - 1.0, 4);
  cairo_stroke(cr);
  static const char* const kNames[3] = {"76", "2A", "VCA"};
  static const char* const kLines[3] = {
      "FET PEAK LIMITER  -  fast, aggressive drum control",
      "OPTICAL LEVELER  -  program-dependent release",
      "BUS VCA  -  clean, stereo-linked glue"};
  text(cr, kNames[model], cx + 20, cy + 42, 30, 0, skin.btnOn.r, skin.btnOn.g,
       skin.btnOn.b);
  text(cr, kLines[model], cx + 20, cy + 70, 9.5, 0, skin.legend.r,
       skin.legend.g, skin.legend.b);
  if (model == 1) {
    text(cr, "ATTACK AND RELEASE FOLLOW THE PROGRAM", cx + 20, cy + 92, 8.5,
         0, skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  } else {
    char ratio[32];
    const int step = std::clamp(
        static_cast<int>(std::lround(shared.get(P_COMP_RATIO))), 0, 8);
    static const float kRatios[9] = {1.5f, 2.0f, 3.0f, 4.0f, 6.0f,
                                     8.0f, 12.0f, 20.0f, 20.0f};
    if (step >= 8) std::snprintf(ratio, sizeof(ratio), "ALL BUTTONS");
    else std::snprintf(ratio, sizeof(ratio), "%.1f:1", kRatios[step]);
    text(cr, ratio, cx + 20, cy + 94, 9.5, 0, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }
}

void Panel::drawScope(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const {
  const int drum = static_cast<int>(shared.get(P_DRUM) + 0.5);
  const Skin skin = skinFor(drum);
  const double x = 170, y = 174, w = 700, h = 150;

  roundRect(cr, x, y, w, h, 4);
  cairo_set_source_rgba(cr, 0.025, 0.027, 0.030, 0.98);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.45);
  roundRect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 4);
  cairo_stroke(cr);

  // Grid.
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.legendDim, 0.14);
  for (int i = 1; i < 8; ++i) {
    const double gx = x + w * i / 8.0;
    cairo_move_to(cr, gx, y);
    cairo_line_to(cr, gx, y + h);
    cairo_stroke(cr);
  }
  for (int i = 1; i < 4; ++i) {
    const double gy = y + h * i / 4.0;
    cairo_move_to(cr, x, gy);
    cairo_line_to(cr, x + w, gy);
    cairo_stroke(cr);
  }

  auto px = [&](int i) { return x + w * i / (Shared::kScopeSize - 1); };
  auto py = [&](float db) {
    const double f = clamp01((db + 60.0) / 60.0);
    return y + h - h * f;
  };

  // Waveform.
  cairo_move_to(cr, px(0), py(state.scopeIn[0]));
  for (int i = 1; i < Shared::kScopeSize; ++i) {
    cairo_line_to(cr, px(i), py(state.scopeIn[i]));
  }
  cairo_set_line_width(cr, 1.4);
  setRgb(cr, skin.legend, 0.75);
  cairo_stroke(cr);

  // Gate envelope.
  cairo_move_to(cr, px(0), y + h - h * clamp01(state.scopeGate[0]));
  for (int i = 1; i < Shared::kScopeSize; ++i) {
    cairo_line_to(cr, px(i), y + h - h * clamp01(state.scopeGate[i]));
  }
  cairo_set_line_width(cr, 2.0);
  setRgb(cr, skin.btnOn, 0.95);
  cairo_stroke(cr);

  text(cr, "INPUT", x + 8, y + 14, 9, 0, skin.legendDim.r, skin.legendDim.g,
       skin.legendDim.b);
  text(cr, "GATE", x + w - 8, y + 14, 9, 2, skin.btnOn.r, skin.btnOn.g,
       skin.btnOn.b);

  if (state.ghostActive) {
    cairo_set_source_rgba(cr, skin.ghostOn.r, skin.ghostOn.g, skin.ghostOn.b,
                          0.95);
    ghostPath(cr, x + w - 26, y + 26, 18, 22);
    cairo_fill(cr);
  }
}

void Panel::drawLevelMeter(cairo_t* cr, double x, double y, double w, double h,
                           double dbL, double dbR, double minDb, double maxDb,
                           bool labelsRight) const {
  const Skin skin = skinFor(0);
  const double gap = 4.0;
  const double pairWidth = w * 2.0 + gap;
  const double outer = labelsRight ? x + pairWidth : x;

  for (int ch = 0; ch < 2; ++ch) {
    const double bx = x + ch * (w + gap);
    cairo_rectangle(cr, bx, y, w, h);
    cairo_set_source_rgb(cr, 0.055, 0.055, 0.06);
    cairo_fill(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.65);
    cairo_rectangle(cr, bx + 0.5, y + 0.5, w - 1, h - 1);
    cairo_stroke(cr);
  }

  static const double kMarks[] = {3, 0, -6, -12, -24, -36, -48};
  for (double m : kMarks) {
    const double f = (m - minDb) / (maxDb - minDb);
    const double ty = y + h - h * f;
    if (ty < y - 1 || ty > y + h + 1) continue;
    setRgb(cr, skin.legend, 0.55);
    cairo_set_line_width(cr, 1.0);
    const double tx0 = labelsRight ? outer : outer - 4;
    const double tx1 = labelsRight ? outer + 4 : outer;
    cairo_move_to(cr, tx0, ty);
    cairo_line_to(cr, tx1, ty);
    cairo_stroke(cr);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%+.0f", m);
    text(cr, buf, labelsRight ? outer + 8 : outer - 8, ty + 3, 9,
         labelsRight ? 0 : 2, skin.legend.r, skin.legend.g, skin.legend.b);
  }

  const double levels[2] = {dbL, dbR};
  for (int ch = 0; ch < 2; ++ch) {
    const double f = clamp01((levels[ch] - minDb) / (maxDb - minDb));
    const double fh = h * f;
    if (fh <= 0.5) continue;
    const double bx = x + ch * (w + gap);
    cairo_pattern_t* pat = cairo_pattern_create_linear(0, y + h, 0, y);
    const Rgb accent = skin.btnOn;
    cairo_pattern_add_color_stop_rgb(pat, 0.0, accent.r * 0.42,
                                     accent.g * 0.42, accent.b * 0.42);
    cairo_pattern_add_color_stop_rgb(pat, 0.82, accent.r * 0.86,
                                     accent.g * 0.86, accent.b * 0.86);
    cairo_pattern_add_color_stop_rgb(pat, 0.96, 0.93, 0.86, 0.55);
    cairo_pattern_add_color_stop_rgb(pat, 1.0, 0.95, 0.35, 0.28);
    cairo_rectangle(cr, bx, y + h - fh, w, fh);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);
  }
}

void Panel::drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                       const PanelState& state, int drum) const {
  (void)state;
  const Skin skin = skinFor(drum);
  switch (w.kind) {
    case Kind::Power: {
      const bool on = shared.get(P_POWER) > 0.5;
      cairo_arc(cr, w.x, w.y, w.r, 0, 2 * kPi);
      setRgb(cr, {0.12, 0.12, 0.12});
      cairo_fill(cr);
      cairo_arc(cr, w.x, w.y, w.r - 1.5, 0, 2 * kPi);
      setRgb(cr, on ? Rgb{0.36, 0.86, 0.46} : Rgb{0.42, 0.42, 0.42});
      cairo_set_line_width(cr, 2.2);
      cairo_new_path(cr);
      cairo_arc(cr, w.x, w.y, 7, -kPi / 2 + 0.6, -kPi / 2 + 2 * kPi - 0.6);
      cairo_stroke(cr);
      cairo_move_to(cr, w.x, w.y - 9);
      cairo_line_to(cr, w.x, w.y - 1);
      cairo_stroke(cr);
      break;
    }
    case Kind::PageTab:
      drawPill(cr, w, state.page == static_cast<int>(w.value), drum);
      break;
    case Kind::DrumTab: {
      const bool selected = std::fabs(shared.get(P_DRUM) - w.value) < 0.5;
      roundRect(cr, w.x, w.y, w.w, w.h, 4);
      setRgb(cr, selected ? skin.tabOn : skin.tabOff);
      cairo_fill(cr);
      const Rgb tc = selected ? skin.tabTextOn : skin.tabTextOff;
      text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 5, 14, 1, tc.r,
           tc.g, tc.b);
      break;
    }
    case Kind::Knob:
      drawKnob(cr, w, shared.get(w.param), drum);
      break;
    case Kind::Button:
      drawButton(cr, w, std::fabs(shared.get(w.param) - w.value) < 1e-3, drum);
      break;
    case Kind::Toggle:
      drawToggle(cr, w, shared.get(w.param) > 0.5, drum);
      break;
    case Kind::Pill:
      drawPill(cr, w, shared.get(w.param) > 0.5, drum);
      break;
    case Kind::Ghost:
      drawGhost(cr, w, shared.get(w.param) > 0.5, drum);
      break;
  }
}

void Panel::drawPill(cairo_t* cr, const Widget& w, bool on, int drum) const {
  const Skin skin = skinFor(drum);
  roundRect(cr, w.x, w.y, w.w, w.h, 5);
  setRgb(cr, on ? skin.btnOn : skin.btnOff);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.35);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1.0, w.h - 1.0, 5);
  cairo_stroke(cr);
  const Rgb tc = on ? skin.btnTextOn : skin.btnText;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 3.5, 10, 1, tc.r,
       tc.g, tc.b);
}

void Panel::drawGhost(cairo_t* cr, const Widget& w, bool on, int drum) const {
  const Skin skin = skinFor(drum);
  const double cx = w.x + w.w * 0.5;
  const double top = w.y + 2;
  const double gw = w.w * 0.8;
  const double gh = w.h * 0.8;

  // Soft button well.
  cairo_arc(cr, cx, w.y + w.h * 0.5, w.w * 0.5, 0, 2 * kPi);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.30);
  cairo_fill(cr);
  if (on) {
    cairo_arc(cr, cx, w.y + w.h * 0.5, w.w * 0.47, 0, 2 * kPi);
    cairo_set_source_rgba(cr, skin.ghostOn.r, skin.ghostOn.g, skin.ghostOn.b,
                          0.18);
    cairo_fill(cr);
  }

  // Ghost body.
  ghostPath(cr, cx, top, gw, gh);
  const Rgb body = on ? skin.ghostOn : skin.ghostOff;
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, top, 0, top + gh);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, mix(body, {1, 1, 1}, 0.18).r,
                                   mix(body, {1, 1, 1}, 0.18).g,
                                   mix(body, {1, 1, 1}, 0.18).b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, mix(body, {0, 0, 0}, 0.14).r,
                                   mix(body, {0, 0, 0}, 0.14).g,
                                   mix(body, {0, 0, 0}, 0.14).b);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
  cairo_stroke(cr);

  // Eyes.
  const double eyeY = top + gh * 0.40;
  const double eyeR = gw * 0.12;
  for (int side = -1; side <= 1; side += 2) {
    const double ex = cx + side * gw * 0.20;
    cairo_arc(cr, ex, eyeY, eyeR, 0, 2 * kPi);
    cairo_set_source_rgb(cr, 0.97, 0.96, 0.93);
    cairo_fill(cr);
    cairo_arc(cr, ex + side * eyeR * 0.25, eyeY, eyeR * 0.45, 0, 2 * kPi);
    cairo_set_source_rgb(cr, 0.05, 0.04, 0.04);
    cairo_fill(cr);
  }

  text(cr, w.label, cx, w.y + w.h + 13, 9, 1,
       on ? skin.ghostOn.r : skin.legendDim.r,
       on ? skin.ghostOn.g : skin.legendDim.g,
       on ? skin.ghostOn.b : skin.legendDim.b);
}

void Panel::drawKnob(cairo_t* cr, const Widget& w, double value,
                     int drum) const {
  const Skin skin = skinFor(drum);
  const double cx = w.x, cy = w.y, r = w.r;

  setRgb(cr, skin.legendDim, 0.55);
  cairo_set_line_width(cr, 1.0);
  for (int i = 0; i <= 10; ++i) {
    const double a = knobAngle(i / 10.0);
    cairo_move_to(cr, cx + std::cos(a) * r * 1.10, cy + std::sin(a) * r * 1.10);
    cairo_line_to(cr, cx + std::cos(a) * r * 1.19, cy + std::sin(a) * r * 1.19);
    cairo_stroke(cr);
  }

  const ParamSpec& paramSpec = kParamSpecs[w.param];
  if (w.scale.empty() && !paramSpec.stepped) {
    const double a0 = knobAngle(0.0), a1 = knobAngle(1.0);
    text(cr, "\u2212", cx + std::cos(a0) * (r * 1.44),
         cy + std::sin(a0) * (r * 1.44) + 4, 12, 1, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
    text(cr, "+", cx + std::cos(a1) * (r * 1.44),
         cy + std::sin(a1) * (r * 1.44) + 4, 12, 1, skin.legendDim.r,
         skin.legendDim.g, skin.legendDim.b);
  }

  const size_t n = w.scale.size();
  for (size_t i = 0; i < n; ++i) {
    const double f = n > 1 ? static_cast<double>(i) / (n - 1) : 0.0;
    const double a = knobAngle(f);
    const double lx = cx + std::cos(a) * (r * w.scaleRadius);
    const double ly = cy + std::sin(a) * (r * w.scaleRadius);
    text(cr, w.scale[i], lx, ly + 4, 10.5, 1, skin.legend.r, skin.legend.g,
         skin.legend.b);
  }

  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
  cairo_arc(cr, cx + 0.5, cy + 1.5, r * 1.03, 0, 2 * kPi);
  cairo_fill(cr);

  cairo_pattern_t* body = cairo_pattern_create_linear(0, cy - r, 0, cy + r);
  const Rgb top = mix(skin.knobSkirt, {1, 1, 1}, 0.16);
  const Rgb bot = mix(skin.knobSkirt, {0, 0, 0}, 0.30);
  cairo_pattern_add_color_stop_rgb(body, 0.0, top.r, top.g, top.b);
  cairo_pattern_add_color_stop_rgb(body, 1.0, bot.r, bot.g, bot.b);
  cairo_arc(cr, cx, cy, r, 0, 2 * kPi);
  cairo_set_source(cr, body);
  cairo_fill(cr);
  cairo_pattern_destroy(body);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
  cairo_arc(cr, cx, cy, r - 0.5, 0, 2 * kPi);
  cairo_stroke(cr);

  cairo_pattern_t* cap = cairo_pattern_create_radial(
      cx - r * 0.35, cy - r * 0.40, r * 0.05, cx, cy, r * 0.98);
  const Rgb hi = mix(skin.knobCap, {1, 1, 1}, 0.24);
  cairo_pattern_add_color_stop_rgb(cap, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(cap, 0.75, skin.knobCap.r, skin.knobCap.g,
                                   skin.knobCap.b);
  cairo_pattern_add_color_stop_rgb(cap, 1.0, skin.knobCapEdge.r,
                                   skin.knobCapEdge.g, skin.knobCapEdge.b);
  cairo_arc(cr, cx, cy, r * 0.82, 0, 2 * kPi);
  cairo_set_source(cr, cap);
  cairo_fill(cr);
  cairo_pattern_destroy(cap);

  if (skin.knurled) {
    cairo_set_line_width(cr, 0.8);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.22);
    for (int k = 0; k < 60; ++k) {
      const double a = k * 2 * kPi / 60.0;
      cairo_move_to(cr, cx + std::cos(a) * r * 0.86,
                    cy + std::sin(a) * r * 0.86);
      cairo_line_to(cr, cx + std::cos(a) * r * 0.99,
                    cy + std::sin(a) * r * 0.99);
      cairo_stroke(cr);
    }
  }

  const ParamSpec& spec = kParamSpecs[w.param];
  const double vmax = w.steps > 1 ? w.steps - 1 : spec.max;
  const double span = vmax - spec.min;
  const double frac = span > 0.0 ? (std::min(value, vmax) - spec.min) / span : 0.0;
  const double a = knobAngle(clamp01(frac));
  cairo_set_line_width(cr, 2.0);
  setRgb(cr, skin.knobIndicator);
  cairo_move_to(cr, cx + std::cos(a) * r * 0.26, cy + std::sin(a) * r * 0.26);
  cairo_line_to(cr, cx + std::cos(a) * r * 0.78, cy + std::sin(a) * r * 0.78);
  cairo_stroke(cr);

  const double labelY = w.labelBelow ? cy + r * 1.44 + 20 : cy - r * 1.44 - 18;
  text(cr, w.label, cx, labelY, 11, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
}

void Panel::drawButton(cairo_t* cr, const Widget& w, bool lit,
                       int drum) const {
  const Skin skin = skinFor(drum);
  const Rgb base = lit ? skin.btnOn : skin.btnOff;
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, w.y, 0, w.y + w.h);
  const Rgb hi = mix(base, {1, 1, 1}, lit ? 0.10 : 0.07);
  const Rgb lo = mix(base, {0, 0, 0}, 0.10);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, lo.r, lo.g, lo.b);
  roundRect(cr, w.x, w.y, w.w, w.h, 3);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);
  setRgb(cr, mix(base, lit ? Rgb{1, 1, 1} : Rgb{0, 0, 0}, 0.35), 0.9);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 3);
  cairo_stroke(cr);
  const Rgb tc = lit ? skin.btnTextOn : skin.btnText;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 4, 11.5, 1, tc.r,
       tc.g, tc.b);
}

void Panel::drawToggle(cairo_t* cr, const Widget& w, bool on,
                       int drum) const {
  const Skin skin = skinFor(drum);
  roundRect(cr, w.x - w.w * 0.5, w.y, w.w, w.h, 4);
  setRgb(cr, mix(skin.knobSkirt, {0, 0, 0}, 0.3));
  cairo_fill(cr);
  const double leverY = on ? w.y + 12 : w.y + w.h - 12;
  cairo_arc(cr, w.x, leverY, 8, 0, 2 * kPi);
  setRgb(cr, skin.knobCap);
  cairo_fill(cr);
  text(cr, w.buttonText[0], w.x, w.y - 8, 12, 1, skin.legend.r, skin.legend.g,
       skin.legend.b);
  text(cr, on ? "ON" : "OFF", w.x, w.y + w.h + 18, 11, 1, skin.legendDim.r,
       skin.legendDim.g, skin.legendDim.b);
}

void Panel::drawPresetBar(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  const int drum = static_cast<int>(shared.get(P_DRUM) + 0.5);
  const Skin skin = skinFor(drum);
  roundRect(cr, kPresetX, kPresetY, kPresetW, kPresetH, 4);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.12);
  roundRect(cr, kPresetX + 0.5, kPresetY + 0.5, kPresetW - 1, kPresetH - 1, 4);
  cairo_stroke(cr);
  text(cr, "\u2039", kPresetX + 18, kPresetY + 25, 20, 1, 0.72, 0.75, 0.78);
  text(cr, "\u203a", kPresetX + kPresetW - 18, kPresetY + 25, 20, 1, 0.72,
       0.75, 0.78);
  char label[96];
  if (state.presetIndex >= 0 && state.presetIndex < kPresetCount) {
    std::snprintf(label, sizeof(label), "%s%s", kPresets[state.presetIndex].name,
                  state.presetDirty ? " *" : "");
  } else {
    std::snprintf(label, sizeof(label), "Init%s", state.presetDirty ? " *" : "");
  }
  text(cr, label, kPresetX + kPresetW * 0.5, kPresetY + 24, 14, 1, 0.90, 0.92,
       0.94);
  (void)skin;
}

void Panel::drawPresetList(cairo_t* cr, const Shared& shared,
                           const PanelState& state) const {
  const int drum = static_cast<int>(shared.get(P_DRUM) + 0.5);
  const Skin skin = skinFor(drum);
  const double h = kListVisible * kListRow + 10;
  roundRect(cr, kPresetX, kListY, kPresetW, h, 5);
  cairo_set_source_rgba(cr, 0.07, 0.08, 0.09, 0.97);
  cairo_fill(cr);
  cairo_set_line_width(cr, 1.0);
  setRgb(cr, skin.tabOn, 0.7);
  roundRect(cr, kPresetX + 0.5, kListY + 0.5, kPresetW - 1, h - 1, 5);
  cairo_stroke(cr);

  cairo_save(cr);
  roundRect(cr, kPresetX + 1, kListY + 1, kPresetW - 2, h - 2, 4);
  cairo_clip(cr);
  const char* lastCategory = nullptr;
  double y = kListY + 6 - state.presetScroll * kListRow;
  for (int i : presetOrder()) {
    const Preset& p = kPresets[i];
    if (!lastCategory || std::strcmp(lastCategory, p.category) != 0) {
      lastCategory = p.category;
      text(cr, p.category, kPresetX + 14, y + 15, 11, 0, 0.52, 0.66, 0.80);
      y += kListRow;
      if (y > kListY + h) break;
    }
    if (y + kListRow > kListY && y < kListY + h) {
      if (i == state.presetIndex) {
        roundRect(cr, kPresetX + 6, y + 1, kPresetW - 12, kListRow - 2, 3);
        setRgb(cr, skin.tabOn, 0.85);
        cairo_fill(cr);
      }
      text(cr, p.name, kPresetX + 26, y + 15, 12.5, 0,
           i == state.presetIndex ? 1.0 : 0.86,
           i == state.presetIndex ? 1.0 : 0.88,
           i == state.presetIndex ? 1.0 : 0.90);
      static const char* kShort[8] = {"KCK", "SNR", "TOM", "HAT",
                                      "RID", "ROOM", "OH", "BUS"};
      text(cr, kShort[std::clamp(p.drum, 0, 7)], kPresetX + kPresetW - 16,
           y + 15, 10.5, 2, 0.56, 0.60, 0.66);
    }
    y += kListRow;
  }
  cairo_restore(cr);
  (void)shared;
}

void Panel::beginTextEntry(Shared& shared, PanelState& state, double x,
                           double y) {
  (void)shared;
  state.presetOpen = 0;
  for (const Widget& w : widgets_) {
    if (!hitWidget(w, x, y)) continue;
    if (w.kind != Kind::Knob) return;
    editing_ = true;
    editParam_ = w.param;
    editText_.clear();
    editBoxX_ = std::clamp(x + 16.0, 4.0, kPanelWidth - kEditW - 4.0);
    editBoxY_ = y - kEditH - 14.0;
    if (editBoxY_ < 4.0) editBoxY_ = std::min(y + 20.0, kPanelHeight - kEditH - 4.0);
    return;
  }
}

bool Panel::hitTextEntry(double x, double y) const {
  return editing_ && x >= editBoxX_ && x <= editBoxX_ + kEditW &&
         y >= editBoxY_ && y <= editBoxY_ + kEditH;
}

bool Panel::textKey(Shared& shared, PanelState& state, int key, char ch) {
  if (!editing_) return false;
  if (key == kTextEnter) return commitTextEntry(shared, state);
  if (key == kTextEscape) {
    editing_ = false;
    editText_.clear();
    return true;
  }
  if (key == kTextBackspace) {
    if (!editText_.empty()) editText_.pop_back();
    return true;
  }
  if (ch >= 32 && ch < 127 && editText_.size() < 24) editText_.push_back(ch);
  return true;
}

bool Panel::commitTextEntry(Shared& shared, PanelState& state) {
  if (!editing_) return false;
  if (editText_.empty()) {
    editing_ = false;
    return false;
  }
  double value = 0.0;
  if (!parseParamValue(shared, editParam_, editText_, &value)) return false;
  shared.set(editParam_, value);
  state.presetDirty = true;
  editing_ = false;
  editText_.clear();
  return true;
}

void Panel::drawTextEntry(cairo_t* cr, const Shared& shared) const {
  if (!editing_) return;
  const Skin skin = skinFor(0);
  const ParamSpec& spec = kParamSpecs[editParam_];
  const std::string current = formatEditValue(shared, editParam_, shared.get(editParam_));
  const std::string shown = (editText_.empty() ? current : editText_) + "|";
  roundRect(cr, editBoxX_, editBoxY_, kEditW, kEditH, 4);
  cairo_set_source_rgba(cr, 0.035, 0.04, 0.05, 0.98);
  cairo_fill(cr);
  setRgb(cr, skin.tabOn, 0.9);
  cairo_set_line_width(cr, 1.2);
  roundRect(cr, editBoxX_ + 0.5, editBoxY_ + 0.5, kEditW - 1, kEditH - 1, 4);
  cairo_stroke(cr);
  text(cr, spec.name, editBoxX_ + 8, editBoxY_ + 12, 8.0, 0,
       skin.legendDim.r, skin.legendDim.g, skin.legendDim.b);
  const Rgb gold = {0.98, 0.80, 0.32};
  text(cr, shown.c_str(), editBoxX_ + 8, editBoxY_ + 26, 10.5, 0, gold.r,
       gold.g, gold.b);
}

void Panel::applyPreset(Shared& shared, PanelState& state, int index) const {
  if (index < 0 || index >= kPresetCount) return;
  const Preset& p = kPresets[index];
  applyDrumDefaults(shared, p.drum);
  for (int i = 0; i < p.count; ++i) {
    const PresetValue& v = p.values[i];
    if (v.param >= 0 && v.param < static_cast<int>(P_COUNT)) {
      shared.set(static_cast<uint32_t>(v.param), v.value);
    }
  }
  state.presetIndex = index;
  state.presetDirty = false;
  state.presetOpen = 0;
}

bool Panel::loadSample(Shared& shared, PanelState& state,
                       const std::string& path) const {
  std::vector<float> data;
  if (!readWavMono(path, data, 48000) || data.empty()) {
    return false;
  }
  auto buffer = std::make_shared<SampleBuffer>();
  buffer->data = std::move(data);
  buffer->sampleRate = 48000;
  buffer->name = std::filesystem::path(path).filename().string();
  shared.setSample(std::shared_ptr<const SampleBuffer>(buffer));
  shared.set(P_SAMPLE_ON, 1.0);
  state.presetDirty = true;
  return true;
}

bool Panel::handlePresetClick(Shared& shared, PanelState& state, double x,
                              double y) {
  if (y >= kPresetY && y <= kPresetY + kPresetH) {
    if (x >= kPresetX && x <= kPresetX + 36) {
      const int next = state.presetIndex <= 0 ? 0 : state.presetIndex - 1;
      applyPreset(shared, state, next);
      return true;
    }
    if (x >= kPresetX + kPresetW - 36 && x <= kPresetX + kPresetW) {
      const int next = std::min(kPresetCount - 1, state.presetIndex + 1);
      applyPreset(shared, state, next);
      return true;
    }
    if (x > kPresetX + 36 && x < kPresetX + kPresetW - 36) {
      state.presetOpen = state.presetOpen ? 0 : 1;
      if (state.presetOpen) state.presetScroll = 0;
      return true;
    }
  }

  if (!state.presetOpen) return false;
  const double h = kListVisible * kListRow + 10;
  if (x < kPresetX || x > kPresetX + kPresetW || y < kListY || y > kListY + h) {
    state.presetOpen = 0;
    return true;
  }
  double rowY = kListY + 6 - state.presetScroll * kListRow;
  const char* lastCategory = nullptr;
  for (int i : presetOrder()) {
    if (!lastCategory || std::strcmp(lastCategory, kPresets[i].category) != 0) {
      lastCategory = kPresets[i].category;
      rowY += kListRow;
    }
    if (y >= rowY && y < rowY + kListRow) {
      applyPreset(shared, state, i);
      return true;
    }
    rowY += kListRow;
  }
  return true;
}

std::vector<Panel::WidgetRect> Panel::widgetRects() const {
  std::vector<WidgetRect> out;
  out.reserve(widgets_.size());
  for (const Widget& w : widgets_) {
    WidgetRect r;
    r.param = w.param;
    r.label = w.label;
    r.knob = w.kind == Kind::Knob;
    r.toggle = w.kind == Kind::Toggle;
    r.r = w.r;
    if (w.kind == Kind::Knob) {
      r.x = w.x - w.r;
      r.y = w.y - w.r;
      r.w = w.r * 2.0;
      r.h = w.r * 2.0;
    } else if (w.kind == Kind::Toggle) {
      r.x = w.x - w.w * 0.5;
      r.y = w.y;
      r.w = w.w;
      r.h = w.h;
    } else {
      r.x = w.x;
      r.y = w.y;
      r.w = w.w;
      r.h = w.h;
    }
    out.push_back(r);
  }
  return out;
}

} // namespace drum
