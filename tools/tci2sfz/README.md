# tci2sfz

Standalone Slate Trigger `.tci` → SFZ + WAV converter.

```bash
python3 tci2sfz.py "Snare Z3.tci" -o ~/tci-sfz
python3 tci2sfz.py /path/to/library -o ~/tci-sfz --bank
```

Output is one SFZ per TCI/mic with velocity layers and round robins, plus the
decoded WAVs and a `MAP.txt` quality report. Load the SFZ in Drum Suite and set
**MIDI Note** to the drum's note; the default follows the Drum Suite drum tab.

The decoder under `vendor/` is the MIT-licensed clean-room implementation from
https://github.com/AidanScroggins/tci2wav (see `vendor/LICENSE`). It is not
affiliated with or supported by Slate. TCI is a proprietary format: only use
files you are licensed to use. Stereo TCI attacks may be missing the first
~2 ms because that part of the format is still undecoded; the decoder reports
this in `MAP.txt`.

Requires Python 3 and numpy.
