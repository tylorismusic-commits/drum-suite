# Drum Suite

**Version 0.1.0 — Linux x86_64** · GPL-3.0-or-later

A drum channel strip in the Suite visual language. The first working slice is
the **Bleed Out** page — a three-knob multiband de-bleed gate with drum
profiles and ghost-note mode. The remaining pages are scaffolded and follow in
the next milestones.

![Drum Suite — Bleed Out](docs/kick.png)

## Pages

The chain is fixed, left to right:

```text
Input → BLEED OUT → SAMPLE → EQ → COMP → DRIVE → CLIP → LIMIT → Output
```

| Page | Status |
|---|---|
| **BLEED OUT** | Working: Threshold, DeBleed, Ghost + scope |
| **SAMPLE** | Scaffolded; loader/trigger next |
| **EQ** | Scaffolded; 4-band + HPF/LPF next |
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

## Build

Requires CMake 3.16+, a C++17 compiler, Cairo, FreeType, X11 and LV2.

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

0.1.0 is the Drum Suite scaffold plus the Bleed Out module:

- 1040×720 panel with seven page tabs and the eight drum profiles.
- BLEED OUT page fully working (CLAP + LV2).
- Placeholder pages for Sample, EQ, Comp, Drive, Clip and Limit.
- Tests: DSP, panel, layout, geometry and TTL.

Next: implement the Sample loader and EQ, then Comp/Drive/Clip/Limit.

## Licence

GPL-3.0-or-later. See `THIRD_PARTY_NOTICES.md`.
