# Changelog

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
