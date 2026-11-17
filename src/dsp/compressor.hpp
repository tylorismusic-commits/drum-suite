// Drum Suite — compressor
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Three drum-relevant models from Comp Suite: the blue-stripe 76 FET, the 2A
// optical leveler and the SSL-style bus VCA.  The models are reused verbatim
// under src/dsp/comp; this wrapper maps the Drum Suite controls to their
// shared control surface and applies the dry/wet mix.

#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/db.hpp>

#include "comp/controls.hpp"
#include "comp/fet.hpp"
#include "comp/optical.hpp"
#include "comp/vca.hpp"
#include "controls.hpp"

namespace drum {

class Compressor {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    fet_.prepare(sr_, comp::FetLimiter::Variant::Blue);
    opto_.prepare(sr_, comp::OptoLeveler::Variant::LA2A);
    vca_.prepare(sr_);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    comp::Controls cc{};
    const float t = std::clamp(c.compThresholdDb, -40.0f, 0.0f);
    // The 76 has no threshold: its input knob is the drive.  Calibrate the
    // shared Threshold control so the default positions give a few dB of GR
    // instead of pinning the FET.
    const float fetInputDb = -18.0f + 0.45f * (t + 20.0f);
    cc.input = std::pow(std::clamp((fetInputDb + 80.0f) / 103.0f, 0.0f, 1.0f),
                        2.0f);
    // 2A: Peak Reduction.  VCA: a real threshold in dB.
    cc.threshold = std::clamp((-t - 8.0f) / 32.0f, 0.0f, 1.0f);
    cc.ratio = std::clamp(c.compRatio, 0.0f, 8.0f);
    cc.attack = std::clamp(c.compAttack, 0.0f, 1.0f);
    cc.release = std::clamp(c.compRelease, 0.0f, 1.0f);
    cc.makeup = std::clamp(c.compMakeup, 0.0f, 1.0f);
    cc.output = 0.5f;
    cc.sidechainHpf = std::clamp(c.compScHpf, 0.0f, 1.0f);
    cc.link = 1.0f;
    cc.vcaAutoRelease = false;

    const int model = std::clamp(c.compModel, 0, 2);
    trim_ = model == 1 ? 1.0f : hdsp::dbToGain(comp::kModelTrimDb[model]);
    if (model != model_) {
      model_ = model;
      resetActive();
    }
    switch (model_) {
      case 0:
        fet_.setControls(cc);
        break;
      case 1:
        // The 2A has no attack/release controls; program-dependent release is
        // part of the model.  Ratio above the halfway point selects Limit.
        opto_.setControls(cc);
        break;
      default: {
        comp::Controls vc = cc;
        vc.threshold = std::clamp((t + 40.0f) / 40.0f, 0.0f, 1.0f);
        vca_.setControls(vc);
        break;
      }
    }
    mix_ = std::clamp(c.compMix, 0.0f, 1.0f);
    on_ = c.compOn;
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    float grMax = 0.0f;
    if (on_) {
      for (int i = 0; i < frames; ++i) {
        const float dryL = left[i];
        const float dryR = right[i];
        float wetL = dryL;
        float wetR = dryR;
        float gr = 0.0f;
        switch (model_) {
          case 0:
            gr = fet_.process(wetL, wetR);
            break;
          case 1:
            gr = opto_.process(wetL, wetR);
            break;
          default:
            gr = vca_.process(wetL, wetR);
            break;
        }
        grMax = std::max(grMax, gr);
        left[i] = dryL + (wetL * trim_ - dryL) * mix_;
        right[i] = dryR + (wetR * trim_ - dryR) * mix_;
      }
    }
    meters.compGrDb = grMax;
  }

  void reset() noexcept { resetActive(); }

private:
  void resetActive() noexcept {
    switch (model_) {
      case 0:
        fet_.reset();
        break;
      case 1:
        opto_.reset();
        break;
      default:
        vca_.reset();
        break;
    }
  }

  float sr_ = 48000.0f;
  int model_ = 0;
  bool on_ = false;
  float mix_ = 1.0f;
  float trim_ = 1.0f;
  comp::FetLimiter fet_;
  comp::OptoLeveler opto_;
  comp::BusVca vca_;
};

} // namespace drum
