// Drum Suite — engine.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <hdsp/biquad.hpp>
#include <hdsp/db.hpp>

#include "bleedout.hpp"
#include "clip.hpp"
#include "compressor.hpp"
#include "controls.hpp"
#include "drive.hpp"
#include "limiter.hpp"
#include "sample_player.hpp"
#include "transient.hpp"

namespace drum {

class Engine {
public:
  void prepare(float sampleRate, int maxBlock = 0) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    gate_.prepare(sr_);
    samplePlayer_.prepare(sr_);
    transient_.prepare(sr_);
    comp_.prepare(sr_);
    drive_.prepare(sr_);
    clip_.prepare(sr_);
    limiter_.prepare(sr_);
    maxBlock_ = std::max(1024, maxBlock > 0 ? maxBlock : 8192);
    triggers_.assign(static_cast<size_t>(maxBlock_), 0);
    applyEq(controls_);
    reset();
  }

  void setControls(const Controls& c, float /*hostBpm*/ = 0.0f,
                   bool /*hostTempo*/ = false) noexcept {
    controls_ = c;
    gate_.setControls(c);
    samplePlayer_.setControls(c);
    applyEq(c);
    transient_.setControls(c);
    comp_.setControls(c);
    drive_.setControls(c);
    clip_.setControls(c);
    limiter_.setControls(c);
  }

  void setSample(std::shared_ptr<const SampleBuffer> sample) noexcept {
    if (!sample) {
      setInstrument(nullptr);
      return;
    }
    auto instrument = std::make_shared<Instrument>();
    Zone z;
    z.sample = std::move(sample);
    instrument->zones.push_back(std::move(z));
    instrument->type = "wav";
    instrument->name = instrument->zones.front().sample->name;
    instrument->path = instrument->zones.front().sample->path;
    setInstrument(std::move(instrument));
    samplePlayer_.reset();
  }

  void setInstrument(std::shared_ptr<const Instrument> instrument) noexcept {
    if (instrument == loadedInstrument_) return;
    loadedInstrument_ = std::move(instrument);
    samplePlayer_.setInstrument(loadedInstrument_);
  }

  void process(float* left, float* right, int frames,
               Meters& meters) noexcept {
    int offset = 0;
    while (offset < frames) {
      const int n = std::min(frames - offset, maxBlock_);
      std::fill(triggers_.begin(), triggers_.begin() + n, 0);
      gate_.process(left + offset, right + offset, n, meters, triggers_.data());
      float peakL = 0.0f, peakR = 0.0f;
      if (!controls_.power || controls_.bypass) {
        meters.sampleVoices = 0;
        meters.sampleTriggered = false;
        meters.compGrDb = 0.0f;
        meters.transientDb = 0.0f;
        meters.clipActive = false;
        meters.limitGrDb = 0.0f;
        clip_.processBypass(left + offset, right + offset, n);
        limiter_.processBypass(left + offset, right + offset, n);
        for (int i = offset; i < offset + n; ++i) {
          peakL = std::max(peakL, std::fabs(left[i]));
          peakR = std::max(peakR, std::fabs(right[i]));
        }
        meters.outPeakLDb = hdsp::gainToDb(peakL);
        meters.outPeakRDb = hdsp::gainToDb(peakR);
        meters.outPeakDb = std::max(meters.outPeakLDb, meters.outPeakRDb);
        offset += n;
        continue;
      }
      samplePlayer_.process(left + offset, right + offset, triggers_.data(), n,
                            meters);

      for (int i = offset; i < offset + n; ++i) {
        float l = left[i];
        float r = right[i];
        if (eqFlat_) {
          if (!std::isfinite(l)) l = 0.0f;
          if (!std::isfinite(r)) r = 0.0f;
          left[i] = l;
          right[i] = r;
          continue;
        }
        l = eqLowL_.process(l);
        r = eqLowR_.process(r);
        l = eqLowMidL_.process(l);
        r = eqLowMidR_.process(r);
        l = eqHighMidL_.process(l);
        r = eqHighMidR_.process(r);
        l = eqHighL_.process(l);
        r = eqHighR_.process(r);
        l = eqHpL_.process(l);
        r = eqHpR_.process(r);
        l = eqLpL_.process(l);
        r = eqLpR_.process(r);
        if (!std::isfinite(l)) l = 0.0f;
        if (!std::isfinite(r)) r = 0.0f;
        left[i] = l;
        right[i] = r;
      }
      transient_.process(left + offset, right + offset, n, meters);
      comp_.process(left + offset, right + offset, n, meters);
      drive_.process(left + offset, right + offset, n);
      clip_.process(left + offset, right + offset, n, meters);
      limiter_.process(left + offset, right + offset, n, meters);
      for (int i = offset; i < offset + n; ++i) {
        peakL = std::max(peakL, std::fabs(left[i]));
        peakR = std::max(peakR, std::fabs(right[i]));
      }
      meters.outPeakLDb = hdsp::gainToDb(peakL);
      meters.outPeakRDb = hdsp::gainToDb(peakR);
      meters.outPeakDb = std::max(meters.outPeakLDb, meters.outPeakRDb);
      offset += n;
    }
  }

  void reset() noexcept {
    gate_.reset();
    samplePlayer_.reset();
    transient_.reset();
    comp_.reset();
    drive_.reset();
    clip_.reset();
    limiter_.reset();
    eqLowL_.reset();
    eqLowR_.reset();
    eqLowMidL_.reset();
    eqLowMidR_.reset();
    eqHighMidL_.reset();
    eqHighMidR_.reset();
    eqHighL_.reset();
    eqHighR_.reset();
    eqHpL_.reset();
    eqHpR_.reset();
    eqLpL_.reset();
    eqLpR_.reset();
  }

  int latencySamples() const noexcept {
    return gate_.latencySamples() + clip_.latencySamples() +
           limiter_.latencySamples();
  }

private:
  void applyEq(const Controls& c) noexcept {
    const float low = (c.eqLow - 0.5f) * 24.0f;
    const float lowMid = (c.eqLowMid - 0.5f) * 24.0f;
    const float highMid = (c.eqHighMid - 0.5f) * 24.0f;
    const float high = (c.eqHigh - 0.5f) * 24.0f;
    eqFlat_ = std::fabs(low) < 0.01f && std::fabs(lowMid) < 0.01f &&
              std::fabs(highMid) < 0.01f && std::fabs(high) < 0.01f &&
              c.eqHpf <= 0.001f && c.eqLpf >= 0.999f;
    eqLowL_.lowShelf(80.0f, low, 0.7f, sr_);
    eqLowR_.lowShelf(80.0f, low, 0.7f, sr_);
    eqLowMidL_.peak(250.0f, lowMid, 0.9f, sr_);
    eqLowMidR_.peak(250.0f, lowMid, 0.9f, sr_);
    eqHighMidL_.peak(1200.0f, highMid, 0.9f, sr_);
    eqHighMidR_.peak(1200.0f, highMid, 0.9f, sr_);
    eqHighL_.highShelf(6000.0f, high, 0.7f, sr_);
    eqHighR_.highShelf(6000.0f, high, 0.7f, sr_);
    if (c.eqHpf > 0.001f) {
      const float hp = eqHpfHz(c.eqHpf);
      eqHpL_.highPass(hp, 0.707f, sr_);
      eqHpR_.highPass(hp, 0.707f, sr_);
    } else {
      eqHpL_ = hdsp::Biquad{};
      eqHpR_ = hdsp::Biquad{};
    }
    if (c.eqLpf < 0.999f) {
      const float lp = eqLpfHz(c.eqLpf);
      eqLpL_.lowPass(lp, 0.707f, sr_);
      eqLpR_.lowPass(lp, 0.707f, sr_);
    } else {
      eqLpL_ = hdsp::Biquad{};
      eqLpR_ = hdsp::Biquad{};
    }
  }

  float sr_ = 48000.0f;
  int maxBlock_ = 8192;
  Controls controls_{};
  BleedOut gate_;
  SamplePlayer samplePlayer_;
  TransientShaper transient_;
  Compressor comp_;
  Drive drive_;
  Clip clip_;
  Limiter limiter_;
  std::shared_ptr<const Instrument> loadedInstrument_;
  std::vector<uint8_t> triggers_;
  bool eqFlat_ = true;
  hdsp::Biquad eqLowL_, eqLowR_, eqLowMidL_, eqLowMidR_;
  hdsp::Biquad eqHighMidL_, eqHighMidR_, eqHighL_, eqHighR_;
  hdsp::Biquad eqHpL_, eqHpR_, eqLpL_, eqLpR_;
};

} // namespace drum
