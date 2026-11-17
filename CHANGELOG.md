# Changelog

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
