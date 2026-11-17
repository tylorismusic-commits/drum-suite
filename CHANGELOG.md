# Changelog

## 0.9.0-alpha.1 — public alpha — 2026-11-17

- Added a stereo-linked transient shaper before the compressor on the COMP
  page: bipolar Attack and Sustain, sensitivity and bypass.
- Per-drum transient defaults are included in the RECOMMENDED guide.
- New transient attack/sustain test using a decaying drum hit.
- Regression-tested every Suite plugin's ctest target; all pass after
  regenerating the Drum Suite LV2 metadata.
- Updated Drum Suite guides and screenshots.

## 0.8.0 — 2026-11-17

- Added the RECOMMENDED button and per-page recommended-value bar. Values
  follow a rock/metal drum cheat sheet (Hardcore Music Studio-style): 4:1, 3-6 dB GR, fast/medium attack,
  100-300 ms release and drum-specific HPF/tone ranges.
- Sample path is saved in CLAP state and the new LV2 state interface, and is
  reloaded when the preset/project is recalled.
- MIDI trigger mode; any note-on fires the sample.
- 0–10 ms trigger offset.
- Five full-chain factory presets.
- Sample browser shortened and Sample On moved so the browser no longer covers
  the global Input knob and the info line is unobstructed.
- LV2 UI receives the host sample rate for the EQ curve and latency display.
- State formats are versioned for future parameter additions.
- Fixed compressor ratio/attack/release display in the guide.

## Unreleased

- Complete per-drum recommendations for gate, EQ, compressor, drive, clip,
  limiter and sample controls.
- Added a per-drum guide strip on every page, target markers on knobs, and a
  header RECOMMENDED button to restore the current drum's guide.
- Double-click now resets a knob to the selected drum's guide.

## 0.7.0 — 2026-11-17

- Audit pass over every module with a real snare loop.
- Fixed compressor gain staging: Comp Suite model output trims are now applied;
  factory profiles sit within about 2 dB of the input level.
- Compressor primary control is labelled Input / Peak Red / Threshold per model.
- Power now bypasses the whole chain, not only the gate.
- True Peak actually switches 4x detection off; sample-peak mode is distinct.
- Sample trigger modes fixed: Gate, Transient (independent onset detector) and
  Always (either source).
- Added Sample Sensitivity, voice-count badge, trigger LED and start marker.
- Reflowed the Sample page: Sample On moved to the top, four voice controls in
  one row, sensitivity/fade/trigger below, reverse/choke as switches, and a
  taller file browser. The waveform info line is no longer covered by a button.
- Added suggested-range arcs on continuous knobs, matching Vocal Suite.
- Drive bypass no longer leaves a residual auto-gain ramp.
- Added full-chain 44.1/96 kHz and power-bypass tests.

## 0.6.0 — 2026-11-17

- LIMIT page: true-peak lookahead limiter with ceiling, release, lookahead and
  mix.
- 4× oversampled true-peak detection; sample-peak mode when True Peak is off.
- Fixed 141-sample limiter latency (2 ms lookahead + half-band filter); the
  full chain reports 282 samples at 48 kHz.
- Gain-reduction history and GR indicator.
- Page layouts inset so nothing collides with the input/output meters.
- Removed the overlapping OUT label on the drive transfer graph.
- Tests: limiter ceiling, GR, true-peak mode, dry path and chain latency.

## 0.5.0 — 2026-11-17

- CLIP page with Soft, Hard and Diode shapes.
- Drive, Ceiling, Mix, Auto Gain and a 250 ms clip LED.
- 1x / 2x / 4x oversampling with a fixed 45-sample clipper latency in every
  mode.
- Soft and diode shapes use ADAA; hard clip is an exact clamp.
- Per-profile clip defaults.
- Tests: ceiling enforcement, clip LED, shape safety, mix bypass, oversampling
  and reported latency.

## 0.4.0 — 2026-11-17

- DRIVE page with five Patina models: Valve, Console, Bus, Tape and
  Transformer.
- Drive, Character, Tone, Mix and auto gain, plus a transfer-curve display.
- Per-profile drive model and defaults.
- Tests: all five drive models, mix bypass and auto-gain level recovery.

## 0.3.0 — 2026-11-17

- COMP page with 76, 2A and VCA models reused from Comp Suite.
- Threshold, Ratio, Attack, Release, Makeup, Mix and sidechain HPF.
- 2A hides attack/release and turns the ratio control into the Limit switch.
- Gain-reduction history and current GR readout.
- Per-profile compressor model and defaults.
- Global bypass now skips sample, EQ and compressor as well as the gate.
- Presets now apply their drum profile's compressor defaults as well as gate
  defaults.
- LV2 TTL uses valid unit URIs (dB, Hz, percent).
- Tests: all three compressor models, GR reporting, mix bypass and makeup.

## 0.2.0 — 2026-11-17

- Sample page: WAV folder browser, waveform, 4-voice one-shot player.
- Sample trigger from the Bleed Out gate, plus Transient and Always modes.
- Level/Direct balance, pitch, start, reverse, choke and fade.
- EQ page: low shelf, low-mid/high-mid peaks, high shelf, HPF and LPF.
- EQ curve drawn from the same biquad coefficients as the audio path.
- Bypass keeps an exact delayed dry path when the EQ is flat.
- C++20 toolchain (atomic shared_ptr for the sample buffer).
- Tests: sample player, gate trigger, choke/pitch, EQ response and sample
  loading.

## 0.1.0 — 2026-11-17

- First Drum Suite scaffold: CLAP + LV2, 1040×720 panel.
- Seven page tabs: BLEED OUT, SAMPLE, EQ, COMP, DRIVE, CLIP, LIMIT.
- Eight drum profiles: Kick, Snare, Tom, Hat, Ride, Room, OH, Bus.
- Working BLEED OUT page: Threshold, DeBleed, Ghost, ghost icon, scope.
- BLEED OUT DSP reused from the standalone Bleed Out 0.1.0.
- Placeholder pages for the remaining six modules.
- Tests: DSP, panel, layout, geometry and TTL.
