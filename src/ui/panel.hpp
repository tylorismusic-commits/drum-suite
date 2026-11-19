// Drum Suite — panel.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cairo.h>

#include <memory>
#include <string>
#include <vector>

#include "params.hpp"
#include "user_presets.hpp"

namespace drum {

constexpr int kPanelWidth = 1040;
constexpr int kPanelHeight = 720;

struct PanelState {
  int page = 0;  // 0 Bleed Out, 1 Sample, 2 EQ, 3 Comp, 4 Drive, 5 Clip, 6 Limit
  int drum = 0;  // mirrors P_DRUM for the per-drum recommendations
  int presetIndex = -1;
  bool presetDirty = false;
  int presetOpen = 0;
  int presetScroll = 0;
  bool savePreset = false;
  bool renamePreset = false;
  bool deletePresetConfirm = false;
  bool presetOverwriteConfirm = false;
  std::string presetText;
  bool presetTextFresh = false;
  std::string presetError;
  std::string presetNotice;
  double presetNoticeUntil = 0.0;
  std::vector<UserPreset> userPresets;
  float inLevelL = -120.0f;
  float inLevelR = -120.0f;
  float outLevelL = -120.0f;
  float outLevelR = -120.0f;
  float grDb = 0.0f;
  float detectorDb = -120.0f;
  float gate = 1.0f;
  bool ghostActive = false;
  float scopeIn[Shared::kScopeSize] = {};
  float scopeGate[Shared::kScopeSize] = {};
  float compGrDb = 0.0f;
  float compHistory[Shared::kScopeSize] = {};
  bool clipActive = false;
  int sampleVoices = 0;
  int sampleZone = -1;
  int sampleZoneCount = 0;
  bool sampleActive = false;
  float sampleRate = 48000.0f;
  float limitGrDb = 0.0f;
  float limitHistory[Shared::kScopeSize] = {};
  std::vector<std::string> sampleFiles;
  std::vector<std::string> sampleDirs;
  std::string sampleDirPath;
  int sampleScroll = 0;
  int sampleMode = 0;      // 0 files, 1 zone map
  int zoneSelected = 0;
  int zoneScroll = 0;
  uint32_t pageLocks = 0;  // one bit per processing page
  int sampleScanTicks = 60;
};

class Panel {
public:
  Panel();
  ~Panel();
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;

  void loadFont(const std::string& dir);
  void tick(const Shared& shared, PanelState& state) const;
  void draw(cairo_t* cr, Shared& shared, const PanelState& state);

  bool pointerDown(Shared& shared, PanelState& state, double x, double y,
                   bool fine = false);
  bool pointerMove(Shared& shared, PanelState& state, double x, double y,
                   bool fine = false);
  void pointerUp();
  bool handlePresetClick(Shared& shared, PanelState& state, double x, double y);
  void applyPreset(Shared& shared, PanelState& state, int index) const;
  void drawPresetBar(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawPresetList(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const;
  int presetCount(const PanelState& state) const;
  const char* presetName(const PanelState& state, int index) const;
  const char* presetCategory(const PanelState& state, int index) const;
  std::vector<int> presetOrder(const PanelState& state) const;
  void reloadUserPresets(PanelState& state);
  void rescanUserPresets(PanelState& state);
  void setPresetNotice(PanelState& state, const std::string& text) const;
  void duplicatePreset(Shared& shared, PanelState& state);
  void exportPreset(const Shared& shared, PanelState& state) const;
  bool importPresetFile(Shared& shared, PanelState& state,
                        const std::string& path);
  bool commitPresetName(Shared& shared, PanelState& state);
  bool wheel(Shared& shared, PanelState& state, double x, double y, double dy);
  void beginTextEntry(Shared& shared, PanelState& state, double x, double y);
  static constexpr int kTextEnter = 1;
  static constexpr int kTextEscape = 2;
  static constexpr int kTextBackspace = 3;
  bool textKey(Shared& shared, PanelState& state, int key, char ch);

  bool dragging() const { return dragParam_ >= 0; }

  struct WidgetRect {
    uint32_t param = 0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    const char* label = "";
    bool knob = false;
    bool toggle = false;
  };
  std::vector<WidgetRect> widgetRects() const;

  bool importDroppedFile(Shared& shared, PanelState& state,
                         const std::string& path);
  bool importDroppedFiles(Shared& shared, PanelState& state,
                          const std::vector<std::string>& paths);

private:
  enum class Kind {
    Knob,
    Power,
    PageTab,
    DrumTab,
    Pill,
    Button,
    Toggle,
    Bypass,
    Ghost,
    Guide,
    LoadFolder,
    Zones,
    PageLock,
  };

  struct Widget {
    Kind kind = Kind::Knob;
    uint32_t param = 0;
    double value = 0.0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    double scaleRadius = 1.44;
    int steps = 0;
    const char* label = "";
    std::vector<const char*> scale;
    bool labelBelow = false;
    std::vector<const char*> buttonText;
  };

  void layout(const Shared& shared, const PanelState& state);
  void drawHeader(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawBody(cairo_t* cr, int drum, int page) const;
  void drawPlaceholderPage(cairo_t* cr, int page) const;
  void drawSamplePage(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const;
  void drawZoneOverlay(cairo_t* cr, Shared& shared,
                       const PanelState& state) const;
  bool handleZoneOverlay(Shared& shared, PanelState& state, double x,
                         double y);
  void setZoneValue(Shared& shared, PanelState& state, int field,
                    double value);
  void saveZoneMap(Shared& shared, PanelState& state);
  void loadFolderMap(Shared& shared, PanelState& state, bool roundRobin);
  void sliceSelectedZone(Shared& shared, PanelState& state);
  void applyRecommended(Shared& shared, const PanelState& state);
  void drawEqPage(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawCompPage(cairo_t* cr, const Shared& shared,
                    const PanelState& state) const;
  void drawDrivePage(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawClipPage(cairo_t* cr, const Shared& shared,
                    const PanelState& state) const;
  void drawLimitPage(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void updateWaveCache(const std::shared_ptr<const SampleBuffer>& sample,
                       int cols) const;
  bool loadSample(Shared& shared, PanelState& state,
                  const std::string& path) const;
  void drawScope(cairo_t* cr, const Shared& shared,
                 const PanelState& state) const;
  void drawLevelMeter(cairo_t* cr, double x, double y, double w, double h,
                      double dbL, double dbR, double minDb, double maxDb,
                      bool labelsRight) const;
  void drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                  const PanelState& state, int drum) const;
  void drawKnob(cairo_t* cr, const Widget& w, double value, int drum) const;
  void drawButton(cairo_t* cr, const Widget& w, bool lit, int drum) const;
  void drawToggle(cairo_t* cr, const Widget& w, bool on, int drum) const;
  void drawPill(cairo_t* cr, const Widget& w, bool on, int drum) const;
  void drawGhost(cairo_t* cr, const Widget& w, bool on, int drum) const;
  void drawTextEntry(cairo_t* cr, const Shared& shared) const;
  void text(cairo_t* cr, const char* s, double x, double y, double size,
            int align, double r, double g, double b) const;
  bool hitWidget(const Widget& w, double x, double y) const;
  bool commitTextEntry(Shared& shared, PanelState& state);
  bool hitTextEntry(double x, double y) const;
  double presetResetValue(const PanelState& state, uint32_t param) const;

  std::vector<Widget> widgets_;
  mutable std::shared_ptr<const SampleBuffer> waveSample_;
  mutable int waveWidth_ = 0;
  mutable std::vector<float> waveMin_;
  mutable std::vector<float> waveMax_;
  int dragParam_ = -1;
  int zoneDrag_ = -1;
  bool zoneDragGraph_ = false;
  int dragSteps_ = 0;
  double dragStartY_ = 0.0;
  double dragStartValue_ = 0.0;
  int lastClickParam_ = -1;
  double lastClickTime_ = 0.0;
  static constexpr double kDoubleClickMs = 400.0;

  bool editing_ = false;
  uint32_t editParam_ = 0;
  std::string editText_;
  double editBoxX_ = 0.0;
  double editBoxY_ = 0.0;
  static constexpr double kEditW = 150.0;
  static constexpr double kEditH = 32.0;

  cairo_surface_t* grain_ = nullptr;
  void* ft_ = nullptr;
  void* ftFace_ = nullptr;
  cairo_font_face_t* font_ = nullptr;
  cairo_font_face_t* fallbackFont_ = nullptr;
};

} // namespace drum
