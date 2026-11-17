// Drum Suite — DSP sanity tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "dsp/engine.hpp"
#include "params.hpp"

using namespace drum;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}

constexpr float kSr = 48000.0f;

struct Render {
  std::vector<float> l, r;
  Meters meters;
  float maxCompGrDb = 0.0f;
  bool clipSeen = false;
};

Render run(const Controls& c, const std::vector<float>& in) {
  Engine engine;
  engine.prepare(kSr);
  engine.setControls(c);
  engine.reset();
  if (c.ghostTrigger > 0) engine.setControls(c);
  Render out;
  out.l = in;
  out.r = in;
  const int n = static_cast<int>(in.size());
  for (int start = 0; start < n; start += 128) {
    const int len = std::min(128, n - start);
    engine.process(out.l.data() + start, out.r.data() + start, len, out.meters);
    out.maxCompGrDb = std::max(out.maxCompGrDb, out.meters.compGrDb);
    out.clipSeen = out.clipSeen || out.meters.clipActive;
  }
  return out;
}

Controls base() {
  Controls c;
  c.drum = 1;  // Snare
  c.gateOn = true;
  c.thresholdDb = -30.0f;
  c.deBleed = 80.0f;
  c.ghostMode = false;
  c.ghost = 100.0f;
  c.power = true;
  c.bypass = false;
  c.compOn = false;
  c.driveOn = false;
  c.clipOn = false;
  c.limitOn = false;
  return c;
}

double rms(const std::vector<float>& x, size_t a, size_t b) {
  double sum = 0.0;
  for (size_t i = a; i < b && i < x.size(); ++i) sum += x[i] * x[i];
  return std::sqrt(sum / std::max<size_t>(1, b - a));
}

bool finite(const std::vector<float>& x) {
  for (float v : x) if (!std::isfinite(v)) return false;
  return true;
}

// Single-bin magnitude, used to measure the snare body and the cymbal bleed
// independently.
double toneLevel(const std::vector<float>& x, int a, int b, double freq) {
  const int n = b - a;
  if (n <= 0) return 0.0;
  const double w = 2.0 * 3.14159265358979323846 * freq / kSr;
  const double c = 2.0 * std::cos(w);
  double s0 = 0.0, s1 = 0.0, s2 = 0.0;
  for (int i = a; i < b; ++i) {
    s0 = x[i] + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  const double re = s1 - s2 * std::cos(w);
  const double im = s2 * std::sin(w);
  return std::sqrt(re * re + im * im) * 2.0 / n;
}

double db(double x) { return 20.0 * std::log10(std::max(1e-12, x)); }
} // namespace

int main() {
  {
    // Bypass with zero lookahead is the exact input.
    Controls c = base();
    c.bypass = true;
    std::vector<float> in(1024);
    for (size_t i = 0; i < in.size(); ++i)
      in[i] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * i / kSr);
    Render r = run(c, in);
    const int latency = static_cast<int>(kSr * 0.002f) + 45 + 141;  // full chain
    float err = 0.0f;
    for (size_t i = latency; i < in.size(); ++i)
      err = std::max(err, std::fabs(r.l[i] - in[i - latency]));
    check(err < 1e-6f, "bypass is the delayed exact input");
  }

  {
    // Global bypass also bypasses sample, EQ and compressor.
    Controls c = base();
    c.bypass = true;
    c.compOn = true;
    c.eqLow = 1.0f;
    c.sampleOn = true;
    std::vector<float> in(2048);
    for (size_t i = 0; i < in.size(); ++i)
      in[i] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * i / kSr);
    Render r = run(c, in);
    const int latency = static_cast<int>(kSr * 0.002f) + 45 + 141;
    float err = 0.0f;
    for (size_t i = latency; i < in.size(); ++i)
      err = std::max(err, std::fabs(r.l[i] - in[i - latency]));
    check(err < 1e-6f, "global bypass skips sample, EQ and compressor");
  }

  {
    // Gate On = off is a true gate bypass, not a closed gate.
    Controls c = base();
    c.gateOn = false;
    std::vector<float> in(4096);
    for (size_t i = 0; i < in.size(); ++i)
      in[i] = 0.01f * std::sin(2.0f * 3.14159265f * 220.0f * i / kSr);
    Render r = run(c, in);
    const double a = rms(in, 1000, 3000);
    const double b = rms(r.l, 1000, 3000);
    std::printf("  gate off rms in=%.6f out=%.6f\n", a, b);
    check(b > a * 0.5, "gate off bypasses the gate instead of muting");
  }

  {
    // A quiet ghost hit is removed without ghost mode and kept with it.  The
    // hit has body (1 kHz) and top (6 kHz) so ghost mode must open both the
    // main gate and the de-bleed high band.
    const int n = static_cast<int>(kSr * 0.30f);
    std::vector<float> in(n, 0.0f);
    const int hit = static_cast<int>(kSr * 0.10f);
    for (int i = 0; i < static_cast<int>(kSr * 0.003f); ++i) {
      const float env = 1.0f - static_cast<float>(i) / (kSr * 0.003f);
      in[hit + i] = 0.020f * env *
                    (std::sin(2.0f * 3.14159265f * 1000.0f * i / kSr) +
                     std::sin(2.0f * 3.14159265f * 6000.0f * i / kSr));
    }
    Controls off = base();
    off.ghostMode = false;
    Render quiet = run(off, in);
    Controls on = base();
    on.ghostMode = true;
    on.ghost = 100.0f;
    Render ghost = run(on, in);
    const double a = rms(quiet.l, hit, hit + static_cast<int>(kSr * 0.02f));
    const double b = rms(ghost.l, hit, hit + static_cast<int>(kSr * 0.02f));
    const int ta = hit + static_cast<int>(kSr * 0.002f);
    const int tb = hit + static_cast<int>(kSr * 0.006f);
    const double topOff = toneLevel(quiet.l, ta, tb, 6000.0);
    const double topOn = toneLevel(ghost.l, ta, tb, 6000.0);
    std::printf("  ghost rms off=%.6f on=%.6f, top %.1f -> %.1f dB\n", a, b,
                db(topOff), db(topOn));
    check(b > a * 1.5, "ghost note mode lets a quiet snare hit through");
    check(db(topOn) - db(topOff) > 12.0,
          "ghost mode keeps the high-frequency top of a ghost note");
  }

  {
    // A MIDI-triggered ghost window reports activity and stays finite.
    Controls c = base();
    c.ghostMode = true;
    c.ghost = 80.0f;
    c.ghostTrigger = 1;
    std::vector<float> in(256, 0.0f);
    Render r = run(c, in);
    check(r.meters.ghostActive, "MIDI ghost trigger opens the ghost window");
    check(finite(r.l), "MIDI ghost output is finite");
  }

  {
    // A loud hit opens the gate.
    const int n = static_cast<int>(kSr * 0.20f);
    std::vector<float> in(n, 0.0f);
    const int hit = static_cast<int>(kSr * 0.05f);
    for (int i = 0; i < static_cast<int>(kSr * 0.005f); ++i) {
      in[hit + i] = 0.5f *
                    std::sin(2.0f * 3.14159265f * 200.0f * i / kSr);
    }
    Render r = run(base(), in);
    const double a = rms(r.l, hit, hit + static_cast<int>(kSr * 0.02f));
    check(a > 0.05, "a loud hit opens the gate");
    check(finite(r.l), "gate output is finite");
  }

  {
    // Extreme settings stay finite and bounded.
    Controls c = base();
    c.deBleed = 100.0f;
    c.ghostMode = true;
    c.ghost = 100.0f;
    std::vector<float> in(48000);
    for (size_t i = 0; i < in.size(); ++i)
      in[i] = 0.5f * std::sin(2.0f * 3.14159265f * 100.0f * i / kSr);
    Render r = run(c, in);
    float peak = 0.0f;
    for (float v : r.l) peak = std::max(peak, std::fabs(v));
    check(finite(r.l) && finite(r.r), "extreme settings stay finite");
    check(peak < 8.0f, "extreme settings do not run away");
  }

  {
    // De-bleed efficacy: a 200 Hz snare body plus continuous 8 kHz cymbal
    // bleed.  DeBleed must cut the bleed between hits while leaving the body
    // and the high-frequency attack of each hit intact.
    const int n = static_cast<int>(kSr * 1.2f);
    std::vector<float> in(n, 0.0f);
    for (int hit : {static_cast<int>(kSr * 0.10f), static_cast<int>(kSr * 0.60f)}) {
      for (int i = 0; i < static_cast<int>(kSr * 0.12f); ++i) {
        const float t = static_cast<float>(i) / kSr;
        in[hit + i] += 0.45f * std::exp(-t / 0.045f) *
                       std::sin(2.0f * 3.14159265f * 200.0f * i / kSr);
        in[hit + i] += 0.18f * std::exp(-t / 0.010f) *
                       std::sin(2.0f * 3.14159265f * 1800.0f * i / kSr);
      }
    }
    for (int i = 0; i < n; ++i)
      in[i] += 0.03f * std::sin(2.0f * 3.14159265f * 8000.0f * i / kSr);

    Controls off = base();
    off.deBleed = 0.0f;
    Controls on = base();
    on.deBleed = 100.0f;
    Render rOff = run(off, in);
    Render rOn = run(on, in);

    const int od = 45 + 141;  // clipper + limiter latency
    const int qa = static_cast<int>(kSr * 0.36f) + od;
    const int qb = static_cast<int>(kSr * 0.56f) + od;
    const int aa = static_cast<int>(kSr * 0.100f) + od;  // first 6 ms: attack
    const int ab = static_cast<int>(kSr * 0.106f) + od;
    const int sa = static_cast<int>(kSr * 0.125f) + od;  // 25-55 ms: sustain
    const int sb = static_cast<int>(kSr * 0.155f) + od;
    const int ha = static_cast<int>(kSr * 0.105f) + od;
    const int hb = static_cast<int>(kSr * 0.145f) + od;
    const double bleedOff = toneLevel(rOff.l, qa, qb, 8000.0);
    const double bleedOn = toneLevel(rOn.l, qa, qb, 8000.0);
    const double bodyOff = toneLevel(rOff.l, ha, hb, 200.0);
    const double bodyOn = toneLevel(rOn.l, ha, hb, 200.0);
    const double attackOff = toneLevel(rOff.l, aa, ab, 8000.0);
    const double attackOn = toneLevel(rOn.l, aa, ab, 8000.0);
    const double sustainOff = toneLevel(rOff.l, sa, sb, 8000.0);
    const double sustainOn = toneLevel(rOn.l, sa, sb, 8000.0);
    std::printf(
        "  debleed: bleed %.1f -> %.1f dB, body %.1f -> %.1f dB, "
        "attack %.1f -> %.1f dB, sustain %.1f -> %.1f dB\n",
        db(bleedOff), db(bleedOn), db(bodyOff), db(bodyOn), db(attackOff),
        db(attackOn), db(sustainOff), db(sustainOn));
    check(db(bleedOff) - db(bleedOn) > 20.0,
          "DeBleed 100 cuts cymbal bleed by more than 20 dB between hits");
    check(db(sustainOff) - db(sustainOn) > 8.0,
          "DeBleed removes cymbal wash during the hit after the attack");
    check(std::fabs(db(bodyOff) - db(bodyOn)) < 1.0,
          "DeBleed leaves the drum body untouched");
    check(std::fabs(db(attackOff) - db(attackOn)) < 1.5,
          "DeBleed keeps the high-frequency attack of the hit");
  }


  {
    // Sample player: a gate trigger fires one voice, fades in, and the voice
    // ends at the end of the buffer.
    SamplePlayer player;
    player.prepare(kSr);
    Controls c;
    c.sampleOn = true;
    c.sampleLevel = 100.0f;
    c.sampleDirect = 0.0f;
    c.sampleFadeMs = 0.5f;
    c.sampleTrigger = 0;
    c.sampleChoke = true;
    auto buffer = std::make_shared<SampleBuffer>();
    buffer->data.assign(1000, 0.5f);
    buffer->sampleRate = static_cast<int>(kSr);
    buffer->name = "test";
    player.setSample(std::shared_ptr<const SampleBuffer>(buffer));
    player.setControls(c);

    Meters meters;
    std::vector<float> l(512, 0.0f), r(512, 0.0f);
    std::vector<uint8_t> trigger(512, 0);
    trigger[0] = 1;
    player.process(l.data(), r.data(), trigger.data(), 512, meters);
    check(meters.sampleTriggered, "gate trigger fires the sample player");
    check(meters.sampleVoices == 1, "one sample voice is active");
    check(rms(l, 24, 500) > 0.25, "sample voice produces audio");

    std::vector<uint8_t> trigger2(600, 0);
    std::vector<float> l2(600, 0.0f), r2(600, 0.0f);
    player.process(l2.data(), r2.data(), trigger2.data(), 600, meters);
    check(meters.sampleVoices == 0, "sample voice ends at the buffer end");

    // +12 semitones consumes the buffer in half the time.
    Controls fast = c;
    fast.samplePitch = 12.0f;
    player.setControls(fast);
    std::vector<uint8_t> trigger3(600, 0);
    std::vector<float> l3(600, 0.0f), r3(600, 0.0f);
    player.process(l3.data(), r3.data(), trigger3.data(), 600, meters);
    check(meters.sampleVoices == 0,
          "octave-up sample finishes in half the samples");

    // Choke keeps one voice; disabling it lets two overlap.
    Controls choke = c;
    choke.sampleChoke = true;
    player.setControls(choke);
    std::vector<float> l4(32, 0.0f), r4(32, 0.0f);
    std::vector<uint8_t> fire(32, 0);
    fire[0] = 1;
    player.process(l4.data(), r4.data(), fire.data(), 32, meters);
    fire[0] = 1;
    player.process(l4.data(), r4.data(), fire.data(), 32, meters);
    check(meters.sampleVoices == 1, "choke retriggers a single sample voice");
    Controls noChoke = c;
    noChoke.sampleChoke = false;
    player.setSample(std::shared_ptr<const SampleBuffer>(buffer));
    player.setControls(noChoke);
    player.process(l4.data(), r4.data(), fire.data(), 32, meters);
    fire[0] = 1;
    player.process(l4.data(), r4.data(), fire.data(), 32, meters);
    check(meters.sampleVoices == 2, "choke off allows overlapping voices");

    // Trigger modes: Gate ignores transients, Transient ignores the gate,
    // Always accepts either.
    std::vector<uint8_t> none(64, 0);
    auto makeHit = [] {
      std::vector<float> v(64, 0.0f);
      v[10] = 0.5f;
      return v;
    };

    Controls gateOnly = c;
    gateOnly.sampleTrigger = 0;
    std::vector<float> hit = makeHit(), hitR = makeHit();
    player.setSample(std::shared_ptr<const SampleBuffer>(buffer));
    player.setControls(gateOnly);
    player.process(hit.data(), hitR.data(), none.data(), 64, meters);
    check(!meters.sampleTriggered,
          "Gate trigger ignores an un-gated input transient");

    Controls transient = c;
    transient.sampleTrigger = 1;
    hit = makeHit();
    hitR = makeHit();
    player.setSample(std::shared_ptr<const SampleBuffer>(buffer));
    player.setControls(transient);
    player.process(hit.data(), hitR.data(), none.data(), 64, meters);
    check(meters.sampleTriggered,
          "Transient trigger fires from the input without a gate event");

    Controls always = c;
    always.sampleTrigger = 2;
    std::vector<uint8_t> gateFire(64, 0);
    gateFire[0] = 1;
    hit = makeHit();
    hitR = makeHit();
    player.setSample(std::shared_ptr<const SampleBuffer>(buffer));
    player.setControls(always);
    player.process(hit.data(), hitR.data(), gateFire.data(), 64, meters);
    check(meters.sampleTriggered,
          "Always trigger accepts a gate event");
  }

  {
    // EQ: low shelf boost at 60 Hz, flat reference, and HPF cut.
    const int n = static_cast<int>(kSr * 0.5f);
    std::vector<float> low(n), mid(n);
    for (int i = 0; i < n; ++i) {
      low[i] = 0.2f * std::sin(2.0f * 3.14159265f * 60.0f * i / kSr);
      mid[i] = 0.2f * std::sin(2.0f * 3.14159265f * 5000.0f * i / kSr);
    }
    Controls flat = base();
    flat.gateOn = false;
    flat.bypass = false;
    Controls boost = flat;
    boost.eqLow = 1.0f;  // +12 dB low shelf
    Controls hp = flat;
    hp.eqHpf = 0.93f;  // ~250 Hz high-pass

    const double flatLow = rms(run(flat, low).l, 400, 2000);
    const double boostLow = rms(run(boost, low).l, 400, 2000);
    const double hpLow = rms(run(hp, low).l, 400, 2000);
    const double flatMid = rms(run(flat, mid).l, 400, 2000);
    const double boostMid = rms(run(boost, mid).l, 400, 2000);
    std::printf("  eq: 60 Hz flat %.1f, boosted %.1f, hpf %.1f dB; 5 kHz %.1f -> %.1f dB\n",
                db(flatLow), db(boostLow), db(hpLow), db(flatMid), db(boostMid));
    check(db(boostLow) - db(flatLow) > 5.0,
          "EQ low shelf boosts the low band");
    check(std::fabs(db(boostMid) - db(flatMid)) < 1.0,
          "EQ leaves the top band flat with a low boost");
    check(db(hpLow) < db(flatLow) - 3.0,
          "EQ high-pass cuts the low band when engaged");
  }


  {
    // Compressor: all three models reduce a loud tone, and mix 0 is a true
    // bypass.
    const int n = static_cast<int>(kSr * 0.5f);
    std::vector<float> tone(n);
    for (int i = 0; i < n; ++i)
      tone[i] = 0.35f * std::sin(2.0f * 3.14159265f * 200.0f * i / kSr);

    Controls comp = base();
    comp.gateOn = false;
    comp.compOn = true;
    comp.compThresholdDb = -15.0f;
    comp.compRatio = 6.0f;
    comp.compAttack = 0.1f;
    comp.compRelease = 0.35f;
    comp.compMakeup = 0.5f;
    comp.compMix = 1.0f;
    comp.compScHpf = 0.0f;

    const double dry = rms(run(base(), tone).l, 2000, 12000);
    for (int model = 0; model < 3; ++model) {
      comp.compModel = model;
      Render r = run(comp, tone);
      const double wet = rms(r.l, 2000, 12000);
      std::printf("  comp model %d: %.1f -> %.1f dB, gr %.1f dB\n", model,
                  db(dry), db(wet), r.maxCompGrDb);
      check(finite(r.l) && finite(r.r), "compressor output is finite");
      check(r.maxCompGrDb > 0.5f, "compressor reports gain reduction");
      check(wet < dry * 0.98, "compressor reduces a loud tone");
    }

    comp.compModel = 2;
    comp.compMix = 0.0f;
    Render bypass = run(comp, tone);
    float err = 0.0f;
    Controls flat = base();
    flat.gateOn = false;
    Render straight = run(flat, tone);
    for (size_t i = 0; i < tone.size(); ++i)
      err = std::max(err, std::fabs(bypass.l[i] - straight.l[i]));
    check(err < 1e-6f, "compressor mix 0 is a true bypass");

    comp.compMix = 1.0f;
    comp.compMakeup = 0.5f;
    Render unity = run(comp, tone);
    comp.compMakeup = 1.0f;
    Render lifted = run(comp, tone);
    check(rms(lifted.l, 2000, 12000) > rms(unity.l, 2000, 12000) * 1.5,
          "compressor makeup adds gain after compression");
  }


  {
    // Drive: every Patina model changes the waveform and stays finite; mix 0
    // is a true bypass; auto gain restores the level.
    const int n = static_cast<int>(kSr * 0.5f);
    std::vector<float> tone(n);
    for (int i = 0; i < n; ++i)
      tone[i] = 0.3f * std::sin(2.0f * 3.14159265f * 1000.0f * i / kSr);

    Controls drive = base();
    drive.gateOn = false;
    drive.driveOn = true;
    drive.driveDrive = 0.85f;
    drive.driveCharacter = 0.5f;
    drive.driveTone = 0.5f;
    drive.driveMix = 1.0f;
    drive.driveAutoGain = false;

    Controls flat = base();
    flat.gateOn = false;
    Render straight = run(flat, tone);
    for (int model = 0; model < 5; ++model) {
      drive.driveModel = model;
      Render r = run(drive, tone);
      double err = 0.0;
      for (size_t i = 2000; i < tone.size(); ++i)
        err = std::max(err, static_cast<double>(std::fabs(r.l[i] - straight.l[i])));
      std::printf("  drive model %d: max deviation %.4f\n", model, err);
      check(finite(r.l) && finite(r.r), "drive output is finite");
      check(err > 0.002, "drive model changes the waveform");
    }

    drive.driveModel = 0;
    drive.driveMix = 0.0f;
    Render dry = run(drive, tone);
    float mixErr = 0.0f;
    for (size_t i = 12000; i < tone.size(); ++i)
      mixErr = std::max(mixErr, std::fabs(dry.l[i] - straight.l[i]));
    check(mixErr < 1e-4f, "drive mix 0 is effectively a bypass");

    drive.driveMix = 1.0f;
    drive.driveAutoGain = false;
    Render noride = run(drive, tone);
    drive.driveAutoGain = true;
    Render gained = run(drive, tone);
    const double dryRms = rms(straight.l, 2000, 20000);
    const double noRms = rms(noride.l, 2000, 20000);
    const double autoRms = rms(gained.l, 2000, 20000);
    std::printf("  drive level: dry %.1f, sat %.1f, auto %.1f dB\n",
                db(dryRms), db(noRms), db(autoRms));
    check(std::fabs(db(autoRms) - db(dryRms)) <
              std::fabs(db(noRms) - db(dryRms)) + 0.5,
          "drive auto gain restores the level");
  }


  {
    // Clipper: hard clip holds the ceiling, all shapes stay finite, mix 0 is
    // the latency-aligned dry path, and the mode does not change latency.
    const int n = static_cast<int>(kSr * 0.5f);
    std::vector<float> tone(n);
    for (int i = 0; i < n; ++i)
      tone[i] = 0.8f * std::sin(2.0f * 3.14159265f * 1000.0f * i / kSr);

    Controls clip = base();
    clip.gateOn = false;
    clip.clipOn = true;
    clip.clipDrive = 1.0f;
    clip.clipCeiling = 0.5f;  // -6 dB
    clip.clipShape = 1;       // hard
    clip.clipMix = 1.0f;
    clip.clipOversample = 0;
    clip.clipAutoGain = false;

    const double ceil = std::pow(10.0, -6.0 / 20.0);
    Render hard = run(clip, tone);
    float peak = 0.0f;
    for (size_t i = 2000; i < tone.size(); ++i)
      peak = std::max(peak, std::fabs(hard.l[i]));
    check(peak <= static_cast<float>(ceil * 1.02),
          "hard clip holds the ceiling");
    check(hard.clipSeen, "clip LED reports clipping");

    for (int shape = 0; shape < 3; ++shape) {
      clip.clipShape = shape;
      Render r = run(clip, tone);
      float p = 0.0f;
      for (size_t i = 2000; i < tone.size(); ++i)
        p = std::max(p, std::fabs(r.l[i]));
      check(finite(r.l) && finite(r.r), "clip shape output is finite");
      check(p <= static_cast<float>(ceil * 1.02),
            "clip shape respects the ceiling");
    }

    clip.clipShape = 0;
    clip.clipMix = 0.0f;
    Controls flat = base();
    flat.gateOn = false;
    Render dry = run(clip, tone);
    Render straight = run(flat, tone);
    float err = 0.0f;
    for (size_t i = 2000; i < tone.size(); ++i)
      err = std::max(err, std::fabs(dry.l[i] - straight.l[i]));
    check(err < 1e-6f, "clip mix 0 is the latency-aligned dry path");

    clip.clipMix = 1.0f;
    for (int os = 0; os < 3; ++os) {
      clip.clipOversample = os;
      Render r = run(clip, tone);
      check(finite(r.l) && finite(r.r), "clip oversampling stays finite");
    }

    Engine engine;
    engine.prepare(kSr);
    check(engine.latencySamples() == 282,
          "full chain reports 282 samples of latency");
  }


  {
    // Limiter: the ceiling holds, GR is reported, mix 0 is the delayed dry
    // path, and the fixed lookahead delay stays in the chain when bypassed.
    const int n = static_cast<int>(kSr * 0.5f);
    std::vector<float> tone(n);
    for (int i = 0; i < n; ++i)
      tone[i] = 0.9f * std::sin(2.0f * 3.14159265f * 200.0f * i / kSr);

    Controls lim = base();
    lim.gateOn = false;
    lim.limitOn = true;
    lim.limitCeiling = 0.5f;  // -6 dB
    lim.limitRelease = 0.5f;
    lim.limitLookahead = 0.5f;
    lim.limitTruePeak = false;
    lim.limitMix = 1.0f;

    Render r = run(lim, tone);
    float peak = 0.0f;
    for (size_t i = 2000; i < tone.size(); ++i)
      peak = std::max(peak, std::fabs(r.l[i]));
    check(peak <= 0.504f, "limiter holds the ceiling");
    check(r.meters.limitGrDb > 1.0f, "limiter reports gain reduction");

    lim.limitMix = 0.0f;
    Render dry = run(lim, tone);
    Controls flat = base();
    flat.gateOn = false;
    Render straight = run(flat, tone);
    float err = 0.0f;
    for (size_t i = 2000; i < tone.size(); ++i)
      err = std::max(err, std::fabs(dry.l[i] - straight.l[i]));
    check(err < 1e-4f, "limiter mix 0 is the latency-aligned dry path");

    lim.limitMix = 1.0f;
    lim.limitTruePeak = true;
    Render tp = run(lim, tone);
    float tpPeak = 0.0f;
    for (size_t i = 2000; i < tone.size(); ++i)
      tpPeak = std::max(tpPeak, std::fabs(tp.l[i]));
    check(finite(tp.l) && tpPeak <= 0.504f,
          "true-peak limiter holds the ceiling");
  }


  {
    // Power is a full-chain bypass, not just a gate bypass.
    Controls c = base();
    c.power = false;
    c.compOn = true;
    c.driveOn = true;
    c.clipOn = true;
    c.limitOn = true;
    c.eqLow = 1.0f;
    std::vector<float> in(2048);
    for (size_t i = 0; i < in.size(); ++i)
      in[i] = 0.20f * std::sin(2.0f * 3.14159265f * 220.0f * i / kSr);
    Render r = run(c, in);
    const int latency = 282;
    float err = 0.0f;
    for (size_t i = latency; i < in.size(); ++i)
      err = std::max(err, std::fabs(r.l[i] - in[i - latency]));
    check(err < 1e-6f, "power off bypasses the whole chain");
  }

  {
    // The complete chain runs at other host rates with sane output and a
    // stable reported latency.
    for (float sr : {44100.0f, 96000.0f}) {
      Engine engine;
      engine.prepare(sr);
      Controls c = base();
      c.gateOn = true;
      c.compOn = true;
      c.driveOn = true;
      c.clipOn = true;
      c.limitOn = true;
      c.eqLow = 0.75f;
      c.eqHighMid = 0.60f;
      c.clipMix = 0.30f;
      c.limitMix = 1.0f;
      engine.setControls(c);

      auto buffer = std::make_shared<SampleBuffer>();
      buffer->data.assign(4800, 0.1f);
      buffer->sampleRate = 48000;
      engine.setSample(std::shared_ptr<const SampleBuffer>(buffer));

      const int n = static_cast<int>(sr * 0.25f);
      std::vector<float> l(static_cast<size_t>(n)), r(static_cast<size_t>(n));
      for (int i = 0; i < n; ++i) {
        l[static_cast<size_t>(i)] =
            0.4f * std::sin(2.0f * 3.14159265f * 120.0f * i / sr);
        r[static_cast<size_t>(i)] =
            0.4f * std::cos(2.0f * 3.14159265f * 120.0f * i / sr);
      }
      Meters meters;
      for (int off = 0; off < n; off += 128) {
        const int len = std::min(128, n - off);
        engine.process(l.data() + off, r.data() + off, len, meters);
      }
      float peak = 0.0f;
      for (float v : l) peak = std::max(peak, std::fabs(v));
      const int expected = 2 * static_cast<int>(sr * 0.002f) + 90;
      std::printf("  full chain %.0f Hz: peak %.3f, latency %d\n", sr, peak,
                  engine.latencySamples());
      check(finite(l) && finite(r), "full chain stays finite at other rates");
      check(peak < 8.0f, "full chain does not run away at other rates");
      check(engine.latencySamples() == expected,
            "full chain latency matches the sample rate");
    }
  }

  {
    bool rangesOk = true;
    int rangeCount = 0;
    for (uint32_t id = 0; id < P_COUNT; ++id) {
      const SuggestedRange r = recommendedRange(id);
      if (r.hi <= r.lo) continue;
      ++rangeCount;
      const ParamSpec& spec = kParamSpecs[id];
      if (r.lo < spec.min || r.hi > spec.max || spec.stepped) rangesOk = false;
    }
    check(rangeCount >= 20 && rangesOk,
          "suggested ranges are valid and inside every parameter range");
  }

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
