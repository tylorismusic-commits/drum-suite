# Third-party notices

Drum Suite is GPL-3.0-or-later.

## Included in this repository

| Component | Licence | Use |
|---|---|---|
| **CLAP** headers (`third_party/clap`) | MIT | Plugin API |
| **Jost** ExtraBold (`assets/fonts/Jost-ExtraBold.ttf`) | SIL Open Font License 1.1 | Panel typography |
| **hearth-dsp** (`third_party/hearth-dsp`) | GPL-3.0-or-later | Crossover, ADAA, filters, smoothers |
| **Comp Suite models** (`src/dsp/comp`) | GPL-3.0-or-later | 76 / 2A / VCA compressor DSP reused from Comp Suite |
| **Patina models** (`src/dsp/drive`) | GPL-3.0-or-later | Valve / Console / Bus / Tape / Transformer saturation reused from Patina |
| **hearth-dsp oversampling/ADAA** | GPL-3.0-or-later | Clipper oversampling and anti-aliased soft/diode shapes |

## Reference-only (not vendored in 0.1.0)

| Component | Licence | Use |
|---|---|---|
| **LSP Plugins** | LGPL-3.0 | Trigger, multiband gate, sampler, clipping and limiter references |
| **onsettrigger** (Robin Gareus) | GPL-2.0-or-later (verify) | Onset detection reference |
| **Airwindows** | MIT | Character/voicing references |
| **Drumgizmo** | GPL-3.0 (verify) | Sample mapping reference |

No Black Salt Audio algorithms, presets or assets are included. Black Salt
Audio Silencer is the functional reference only; Bleed Out is an independent
implementation.

## Not shipped

No proprietary samples, presets, artwork, trade dress or algorithms. User
samples and presets are local to the user.
