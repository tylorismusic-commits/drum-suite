// Bleed Out — multiband de-bleed gate
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <hdsp/adaa.hpp>
#include <hdsp/crossover.hpp>
#include <hdsp/db.hpp>
#include <hdsp/smoother.hpp>
#include <hdsp/util.hpp>

#include "controls.hpp"

namespace drum {

inline float flushDenormal(float x) noexcept {
  return (x > -1e-15f && x < 1e-15f) ? 0.0f : x;
}

// The Nolly/Silencer idea: gate/expand the high band from the drum body, not
// from the cymbal wash.  The low/mid detector opens the high-band gate; the
// DeBleed control sets how much the high band is attenuated when the body is
// absent.  The main gate handles the full-band signal; the high-band gate is a
// sidechained expander.
class BleedOut {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    crossDetL_.prepare(sr_);
    crossDetR_.prepare(sr_);
    crossAudL_.prepare(sr_);
    crossAudR_.prepare(sr_);
    lookaheadSamples_ = std::max(1, static_cast<int>(sr_ * 0.002f));
    delaySize_ = std::max(lookaheadSamples_ + 8,
                          static_cast<int>(sr_ * 0.010f) + 4);
    delayL_.assign(static_cast<size_t>(delaySize_), 0.0f);
    delayR_.assign(static_cast<size_t>(delaySize_), 0.0f);
    delayWrite_ = 0;
    fastRelease_ = std::exp(-1.0f / (0.020f * sr_));
    detRelease_ = std::exp(-1.0f / (0.080f * sr_));
    input_.setTime(15.0f, sr_);
    output_.setTime(15.0f, sr_);
    input_.reset(1.0f);
    output_.reset(1.0f);
    reset();
  }

  void setControls(const Controls& c) noexcept {
    controls_ = c;
    drum_ = std::clamp(c.drum, 0, kDrumCount - 1);
    if (drum_ != profileDrum_) {
      profileDrum_ = drum_;
      const Profile& p = kProfiles[drum_];
      crossDetL_.setCrossovers(p.lowXover, p.highXover);
      crossDetR_.setCrossovers(p.lowXover, p.highXover);
      crossAudL_.setCrossovers(p.lowXover, p.highXover);
      crossAudR_.setCrossovers(p.lowXover, p.highXover);
      weights_[0] = p.lowWeight;
      weights_[1] = p.midWeight;
      weights_[2] = p.highWeight;
    }
    const Profile& p = kProfiles[drum_];
    threshold_ = hdsp::dbToGain(c.thresholdDb);
    closeThreshold_ = threshold_ * hdsp::dbToGain(-p.hysteresisDb);
    minGain_ = hdsp::dbToGain(-p.reductionDb);
    deBleed_ = hdsp::clampf(c.deBleed * 0.01f, 0.0f, 1.0f);
    // DeBleed is an attenuation amount, not a linear gain: 50 % should sound
    // like a real cut, 100 % should remove the bleed.
    deBleedFloor_ = hdsp::dbToGain(-48.0f * deBleed_);
    hbAttackCoef_ = 1.0f - hdsp::timeCoeff(0.3f, sr_);
    hbReleaseCoef_ =
        1.0f - hdsp::timeCoeff(std::max(2.0f, p.deBleedReleaseMs), sr_);
    hbHoldSamples_ = static_cast<int>(p.deBleedHoldMs * 0.001f * sr_);
    attackCoef_ = 1.0f - hdsp::timeCoeff(std::max(0.1f, p.attackMs), sr_);
    releaseCoef_ = 1.0f - hdsp::timeCoeff(std::max(1.0f, p.lengthMs), sr_);
    holdSamples_ = static_cast<int>(p.holdMs * 0.001f * sr_);
    lowGain_ = hdsp::dbToGain(p.lowGainDb);
    midGain_ = hdsp::dbToGain(p.midGainDb);
    highGain_ = hdsp::dbToGain(p.highGainDb);
    lowDrive_ = 1.0f + hdsp::clampf(p.lowDrive * 0.01f, 0.0f, 1.0f) * 8.0f;
    midDrive_ = 1.0f + hdsp::clampf(p.midDrive * 0.01f, 0.0f, 1.0f) * 8.0f;
    highDrive_ = 1.0f + hdsp::clampf(p.highDrive * 0.01f, 0.0f, 1.0f) * 8.0f;
    satMix_ = hdsp::clampf(p.satMix * 0.01f, 0.0f, 1.0f);
    ghostAmount_ = hdsp::clampf(c.ghost * 0.01f, 0.0f, 1.0f);
    ghostRange_ = hdsp::dbToGain(-24.0f * ghostAmount_);
    ghostAttack_ = 1.0f - hdsp::timeCoeff(0.5f, sr_);
    ghostRelease_ = 1.0f - hdsp::timeCoeff(50.0f, sr_);
    ghostHoldSamples_ = static_cast<int>(0.012f * sr_);
    if (c.ghostTrigger > 0) {
      midiGhostHold_ = std::max(midiGhostHold_, static_cast<int>(0.060f * sr_));
    }
    input_.setTarget(hdsp::dbToGain(c.inputDb));
    output_.setTarget(hdsp::dbToGain(c.outputDb));
  }

  void reset() noexcept {
    crossDetL_.reset();
    crossDetR_.reset();
    crossAudL_.reset();
    crossAudR_.reset();
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    delayWrite_ = 0;
    gateEnv_ = 0.0f;
    bodyEnv_ = 0.0f;
    ghostEnv_ = 0.0f;
    hold_ = 0;
    bodyHold_ = 0;
    ghostHold_ = 0;
    midiGhostHold_ = 0;
    gateOpen_ = false;
    triggerPrev_ = false;
    ghostTriggerPrev_ = false;
    envDet_ = 0.0f;
    mainWasHot_ = false;
    for (int i = 0; i < 3; ++i) {
      bandFast_[i] = 0.0f;
    }
    lowSatL_.reset(); lowSatR_.reset();
    midSatL_.reset(); midSatR_.reset();
    highSatL_.reset(); highSatR_.reset();
    input_.snap();
    output_.snap();
  }

  void process(float* left, float* right, int frames, Meters& meters,
               uint8_t* trigger = nullptr) noexcept {
    float inPeakL = 0.0f, inPeakR = 0.0f;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    float grMax = 0.0f;
    float detMax = 0.0f;
    bool ghostSeen = false;

    for (int i = 0; i < frames; ++i) {
      const float rawL = std::isfinite(left[i]) ? left[i] : 0.0f;
      const float rawR = std::isfinite(right[i]) ? right[i] : 0.0f;
      inPeakL = std::max(inPeakL, std::fabs(rawL));
      inPeakR = std::max(inPeakR, std::fabs(rawR));

      delayL_[delayWrite_] = rawL;
      delayR_[delayWrite_] = rawR;
      int read = delayWrite_ - lookaheadSamples_;
      if (read < 0) read += delaySize_;
      const float dryL = delayL_[read];
      const float dryR = delayR_[read];
      delayWrite_ = (delayWrite_ + 1) % delaySize_;

      const float gainIn = input_.next();
      const float detL = rawL * gainIn;
      const float detR = rawR * gainIn;

      float dl, dm, dh, drl, drm, drh;
      crossDetL_.process(detL, dl, dm, dh);
      crossDetR_.process(detR, drl, drm, drh);
      const float bandPeak[3] = {
          std::max(std::fabs(dl), std::fabs(drl)),
          std::max(std::fabs(dm), std::fabs(drm)),
          std::max(std::fabs(dh), std::fabs(drh))};
      for (int b = 0; b < 3; ++b) {
        bandFast_[b] =
            flushDenormal(std::max(bandPeak[b], bandFast_[b] * fastRelease_));
      }
      const float det = weights_[0] * bandFast_[0] +
                        weights_[1] * bandFast_[1] +
                        weights_[2] * bandFast_[2];
      envDet_ += (det - envDet_) * (1.0f - detRelease_);
      envDet_ = flushDenormal(envDet_);
      const float transient = std::max(0.0f, det - envDet_);

      // Main gate.
      if (det > threshold_) {
        gateOpen_ = true;
        hold_ = holdSamples_;
      } else if (det < closeThreshold_) {
        gateOpen_ = false;
      }
      if (hold_ > 0) --hold_;
      const bool target = controls_.gateOn && (gateOpen_ || hold_ > 0);
      if (trigger && target && !triggerPrev_) trigger[i] = 1;
      triggerPrev_ = target;
      gateEnv_ += (target ? (1.0f - gateEnv_) : (0.0f - gateEnv_)) *
                  (target ? attackCoef_ : releaseCoef_);
      gateEnv_ = flushDenormal(gateEnv_);

      // De-bleed sidechain.  It is one-shot, not a level gate: each detected
      // target hit opens the high band for the attack, then the band closes
      // again even while the body is still ringing.  That is what removes
      // cymbal wash *during* the hit instead of only between hits.
      //
      // The trigger is the edge of the *same* detector that opens the main
      // gate.  Anything below the main threshold is bleed, so it must not be
      // able to open the high band: otherwise a quiet snare body that never
      // opens the visible gate still lets its crack through as a short blip.
      if (det > threshold_ && !mainWasHot_) {
        mainWasHot_ = true;
        bodyHold_ = hbHoldSamples_;
      } else if (det < closeThreshold_) {
        mainWasHot_ = false;
      }
      if (bodyHold_ > 0) --bodyHold_;
      const bool bodyTarget = controls_.gateOn && (bodyHold_ > 0);
      bodyEnv_ += (bodyTarget ? (1.0f - bodyEnv_) : (0.0f - bodyEnv_)) *
                  (bodyTarget ? hbAttackCoef_ : hbReleaseCoef_);
      bodyEnv_ = flushDenormal(bodyEnv_);

      // Ghost note mode.
      bool ghostOpen = false;
      if (controls_.ghostMode && ghostAmount_ > 0.0f) {
        const float ghostThreshold = threshold_ * ghostRange_;
        ghostOpen = det > ghostThreshold &&
                    transient > ghostThreshold * 0.20f;
        if (ghostOpen) {
          ghostHold_ = ghostHoldSamples_;
          ghostSeen = true;
        }
      }
      if (trigger && ghostOpen && !ghostTriggerPrev_) trigger[i] = 1;
      ghostTriggerPrev_ = ghostOpen;
      if (ghostHold_ > 0) --ghostHold_;
      if (midiGhostHold_ > 0) {
        --midiGhostHold_;
        ghostOpen = true;
        ghostSeen = true;
      }
      const bool ghostTarget = controls_.ghostMode && ghostAmount_ > 0.0f &&
                               (ghostOpen || ghostHold_ > 0 ||
                                midiGhostHold_ > 0);
      ghostEnv_ += (ghostTarget ? (1.0f - ghostEnv_) : (0.0f - ghostEnv_)) *
                   (ghostTarget ? ghostAttack_ : ghostRelease_);
      ghostEnv_ = flushDenormal(ghostEnv_);
      const float ghostGain =
          minGain_ + (1.0f - minGain_) * ghostEnv_ * ghostAmount_;
      // Gate On = off is a true bypass of the gate, not a closed gate.
      float gateGain = controls_.gateOn
                           ? minGain_ + (1.0f - minGain_) * gateEnv_
                           : 1.0f;
      gateGain = std::max(gateGain, ghostGain);
      float bodyGate = minGain_ + (1.0f - minGain_) * bodyEnv_;
      bodyGate = std::max(bodyGate, ghostGain);

      // High-band de-bleed: when the body is absent, close the high band by
      // DeBleed.  At DeBleed=0 the high band is untouched; at 100 it is 48 dB
      // down between hits.  The ghost envelope also opens it, so ghost notes
      // keep their top end.
      const float bodyPresence =
          (bodyGate - minGain_) / std::max(0.001f, 1.0f - minGain_);
      const float highGate =
          controls_.gateOn
              ? deBleedFloor_ + (1.0f - deBleedFloor_) * bodyPresence
              : 1.0f;

      float al, am, ah, arl, arm, arh;
      const float audL = dryL * gainIn;
      const float audR = dryR * gainIn;
      crossAudL_.process(audL, al, am, ah);
      crossAudR_.process(audR, arl, arm, arh);

      al = saturate(lowSatL_, al, lowDrive_);
      am = saturate(midSatL_, am, midDrive_);
      ah = saturate(highSatL_, ah, highDrive_);
      arl = saturate(lowSatR_, arl, lowDrive_);
      arm = saturate(midSatR_, arm, midDrive_);
      arh = saturate(highSatR_, arh, highDrive_);

      float outL = al * lowGain_ * gateGain + am * midGain_ * gateGain +
                   ah * highGain_ * highGate;
      float outR = arl * lowGain_ * gateGain + arm * midGain_ * gateGain +
                   arh * highGain_ * highGate;
      const float gainOut = output_.next();
      outL *= gainOut;
      outR *= gainOut;

      const float active = controls_.power ? (controls_.bypass ? 0.0f : 1.0f) : 0.0f;
      outL = dryL + active * (outL - dryL);
      outR = dryR + active * (outR - dryR);

      left[i] = std::isfinite(outL) ? outL : 0.0f;
      right[i] = std::isfinite(outR) ? outR : 0.0f;
      outPeakL = std::max(outPeakL, std::fabs(outL));
      outPeakR = std::max(outPeakR, std::fabs(outR));
      grMax = std::max(grMax, -hdsp::gainToDb(gateGain));
      detMax = std::max(detMax, hdsp::gainToDb(det));
    }

    meters.inPeakLDb = hdsp::gainToDb(inPeakL);
    meters.inPeakRDb = hdsp::gainToDb(inPeakR);
    meters.inPeakDb = std::max(meters.inPeakLDb, meters.inPeakRDb);
    meters.outPeakLDb = hdsp::gainToDb(outPeakL);
    meters.outPeakRDb = hdsp::gainToDb(outPeakR);
    meters.outPeakDb = std::max(meters.outPeakLDb, meters.outPeakRDb);
    meters.grDb = std::max(0.0f, grMax);
    meters.detectorDb = detMax;
    meters.gate = controls_.gateOn ? gateEnv_ : 1.0f;
    meters.ghostActive = ghostSeen;
  }

  int latencySamples() const noexcept { return lookaheadSamples_; }

private:
  float saturate(hdsp::AsymShaper& shaper, float x, float drive) noexcept {
    if (satMix_ <= 0.0f) return x;
    const float wet = shaper.process(x, drive, 0.0f);
    return x + satMix_ * (wet - x);
  }

  float sr_ = 48000.0f;
  int drum_ = 0;
  int profileDrum_ = -1;
  Controls controls_;

  hdsp::ThreeBandCrossover crossDetL_, crossDetR_;
  hdsp::ThreeBandCrossover crossAudL_, crossAudR_;
  hdsp::AsymShaper lowSatL_, lowSatR_, midSatL_, midSatR_, highSatL_, highSatR_;
  hdsp::Smoothed input_, output_;

  std::vector<float> delayL_, delayR_;
  int delaySize_ = 1;
  int delayWrite_ = 0;
  int lookaheadSamples_ = 96;

  float threshold_ = 1.0f;
  float closeThreshold_ = 1.0f;
  float minGain_ = 0.001f;
  float deBleed_ = 0.7f;
  float deBleedFloor_ = 1.0f;
  float hbAttackCoef_ = 0.1f;
  float hbReleaseCoef_ = 0.001f;
  int hbHoldSamples_ = 0;
  float attackCoef_ = 0.1f;
  float releaseCoef_ = 0.001f;
  int holdSamples_ = 0;
  int hold_ = 0;
  bool gateOpen_ = false;
  bool triggerPrev_ = false;
  bool ghostTriggerPrev_ = false;
  float gateEnv_ = 0.0f;

  bool mainWasHot_ = false;
  int bodyHold_ = 0;
  float bodyEnv_ = 0.0f;

  float lowGain_ = 1.0f, midGain_ = 1.0f, highGain_ = 1.0f;
  float lowDrive_ = 1.0f, midDrive_ = 1.0f, highDrive_ = 1.0f;
  float satMix_ = 0.0f;

  float bandFast_[3] = {};
  float weights_[3] = {0.8f, 0.15f, 0.05f};
  float envDet_ = 0.0f;
  float fastRelease_ = 0.99f;
  float detRelease_ = 0.999f;

  float ghostAmount_ = 0.5f;
  float ghostRange_ = 0.1f;
  float ghostEnv_ = 0.0f;
  float ghostAttack_ = 0.1f;
  float ghostRelease_ = 0.001f;
  int ghostHold_ = 0;
  int ghostHoldSamples_ = 0;
  int midiGhostHold_ = 0;
};

} // namespace bleed
