// Drum Suite — DSP sanity tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdio>
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
    const int latency = static_cast<int>(kSr * 0.002f);  // fixed lookahead
    float err = 0.0f;
    for (size_t i = latency; i < in.size(); ++i)
      err = std::max(err, std::fabs(r.l[i] - in[i - latency]));
    check(err < 1e-6f, "bypass is the delayed exact input");
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

    const int qa = static_cast<int>(kSr * 0.36f);
    const int qb = static_cast<int>(kSr * 0.56f);
    const int aa = static_cast<int>(kSr * 0.100f);  // first 6 ms: the attack
    const int ab = static_cast<int>(kSr * 0.106f);
    const int sa = static_cast<int>(kSr * 0.125f);  // 25-55 ms: the sustain
    const int sb = static_cast<int>(kSr * 0.155f);
    const int ha = static_cast<int>(kSr * 0.105f);
    const int hb = static_cast<int>(kSr * 0.145f);
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

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
