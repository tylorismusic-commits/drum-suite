# Drum Suite

**Version 0.2.0 — Linux x86_64** · GPL-3.0-or-later

A drum channel strip in the Suite visual language. **Bleed Out**, **Sample**
and **EQ** are working; Comp, Drive, Clip and Limit follow in the next
milestones.

![Drum Suite — Bleed Out](docs/kick.png)

## Pages

The chain is fixed, left to right:

```text
Input → BLEED OUT → SAMPLE → EQ → COMP → DRIVE → CLIP → LIMIT → Output
```

| Page | Status |
|---|---|
| **BLEED OUT** | Working: Threshold, DeBleed, Ghost + scope |
| **SAMPLE** | Working: 4-voice loader, gate/transient trigger, pitch, start, reverse, choke, fade |
| **EQ** | Working: low shelf, two peaks, high shelf, HPF/LPF + curve |
| **COMP** | Scaffolded; 76 / 2A / VCA next |
| **DRIVE** | Scaffolded; Patina colours next |
| **CLIP** | Scaffolded; soft / hard / diode next |
| **LIMIT** | Scaffolded; true-peak safety next |

The drum selector across the top chooses the profile: **Kick, Snare, Tom, Hat,
Ride, Room, OH, Bus**. It changes detector focus, gate defaults and sample
category, not the whole preset.

## Bleed Out

The full three-knob de-bleed gate from the standalone Bleed Out:

- **Threshold** — raise until the gate opens on every hit.
- **DeBleed** — high-band attenuation between hits, driven from the drum body.
- **Ghost** — ghost-note amount; the ghost icon toggles ghost mode.
- 2 ms fixed lookahead, profile-automated reduction/decay/attack/saturation.
- Scope with input and gate envelope.

![Drum Suite — Snare ghost mode](docs/snare-ghost.png)

## Sample

The Sample page layers a one-shot over the gated drum. Put mono or stereo WAV
files in:

```text
~/.local/share/drum-suite/samples
```

Click a file in the page to load it. The list refreshes while the editor is
open.

- **Sample On** enables the layer; loading a file turns it on.
- **Level** and **Direct** balance the sample against the gated direct signal.
- **Pitch** ±24 semitones, **Start** moves the playback start point.
- **Reverse**, **Choke** and **Fade** shape the one-shot.
- **Trigger** chooses **Gate** (Bleed Out opens), **Transient** (input
  transient) or **Always** (first transient regardless of the gate).
- Four voices; choke is on by default.

The waveform shows the loaded file. The sample is loaded from disk at runtime;
0.2.0 does not embed the audio in the plugin state yet, so a recalled session
shows the same folder and reloads the file from it.

![Drum Suite — Sample](docs/sample-page.png)

## EQ

Four bands plus filters, all after the gate and sample layer:

- Low shelf at 80 Hz, low-mid peak at 250 Hz, high-mid peak at 1.2 kHz, high
  shelf at 6 kHz, ±12 dB each.
- HPF 20–500 Hz and LPF 500 Hz–20 kHz.
- The curve is drawn from the same biquad coefficients as the audio path.

![Drum Suite — EQ](docs/eq.png)

## Build

Requires CMake 3.16+, a C++20 compiler, Cairo, FreeType, X11 and LV2.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Install for the current user:

```sh
cmake -S . -B build \
  -DCLAP_DIR="$HOME/.clap" \
  -DLV2_DIR="$HOME/.lv2"
cmake --build build -j
cmake --install build
```

## Status

0.2.0 adds the Sample loader and EQ to the 0.1.0 scaffold:

- 1040×720 panel with seven page tabs and the eight drum profiles.
- BLEED OUT page fully working (CLAP + LV2).
- SAMPLE page: folder browser, waveform, 4 voices, trigger from the gate,
  pitch/start/reverse/choke/fade and direct/sample balance.
- EQ page: four bands + HPF/LPF with a live curve.
- Placeholder pages for Comp, Drive, Clip and Limit.
- Tests: DSP, sample player, EQ response, panel loading, layout, geometry and
  TTL.

Next: Comp, then Drive, Clip and Limit.

## Licence

GPL-3.0-or-later. See `THIRD_PARTY_NOTICES.md`.
