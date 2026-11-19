#!/usr/bin/env python3
"""Test the TCI -> SFZ converter with a synthetic Trigger Instrument file."""
import math
import os
import struct
import sys
import tempfile
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

try:
    import numpy  # noqa: F401
    import tci2sfz
except Exception as exc:
    print(f"SKIP: {exc}")
    sys.exit(77)


def wave(frames, amp):
    n = frames - 1
    k = 16
    comp = 8 + n * k
    bits = bytearray([k])
    for i in range(n):
        v = int(amp * math.sin(2 * math.pi * 220 * i / 44100))
        code = (abs(v) & 0x7FFF) | (0x8000 if v < 0 else 0)
        bits += code.to_bytes(2, "big")
    return struct.pack("<BII", 1, comp, frames) + bytes(bits)


def make_tci(path):
    header = bytearray(128)
    header[0:8] = b"TRIGGER "
    tag = b"COMPRESSED INSTRUMENT"
    header[8:8 + len(tag)] = tag
    data = bytearray(header)
    for amp in (1000, 1000, 4000, 12000):
        w = wave(100, amp)
        data += w
        data += struct.pack("<II", 5, len(w))
    data += struct.pack("<II", 6, 0)
    with open(path, "wb") as f:
        f.write(data)


def make_v2(path):
    frames = 401
    head = 200
    k = 16
    tailn = 201
    comp = 4800 + 8 + tailn * k
    blob = bytearray()
    for i in range(head):
        v = int(4000 * math.sin(2 * math.pi * 220 * i / 44100))
        u = (abs(v) & 0x7FFFFF) | (0x800000 if v < 0 else 0)
        blob += u.to_bytes(3, "big")
    blob.append(k)
    for i in range(tailn):
        v = int(2000 * math.sin(2 * math.pi * 220 * (head + i) / 44100))
        code = (abs(v) & 0x7FFF) | (0x8000 if v < 0 else 0)
        blob += code.to_bytes(2, "big")
    blob = blob[:(comp + 7) // 8]
    xml = (f'<trigger_instrument data_count="1" wd0comp1="{comp}" '
           f'wd0frames="{frames}" wd0stereo="0" />')
    foot = b"\0" * 8 + zlib.compress(b"VC2!" + b"\0\0\0\0" + xml.encode())
    with open(path, "wb") as f:
        f.write(bytearray(64))
        f.write(struct.pack("<III", len(blob), 0, 24))
        f.write(blob)
        f.write(foot)


def main():
    with tempfile.TemporaryDirectory(prefix="tci2sfz-test-") as tmp:
        tci = os.path.join(tmp, "TestSnare Z3.tci")
        make_tci(tci)
        out = os.path.join(tmp, "out")
        ok = tci2sfz.convert_one(tci, out, "TestSnare", "Z3")
        assert ok
        sfz = os.path.join(out, "TestSnare", "Z3", "instrument.sfz")
        assert os.path.exists(sfz)
        text = open(sfz, encoding="utf-8").read()
        assert text.count("<region>") == 4
        assert "lovel=0 hivel=41" in text
        assert "lovel=42 hivel=84" in text
        assert "lovel=85 hivel=127" in text
        assert "seq_length=2" in text
        wavs = [n for n in os.listdir(os.path.join(out, "TestSnare", "Z3", "samples"))
                if n.endswith(".wav")]
        assert len(wavs) == 4
        assert os.path.exists(os.path.join(out, "TestSnare", "Z3", "MAP.txt"))
        # V2 raw200 path.
        v2 = os.path.join(tmp, "TestV2 Z3.tci")
        make_v2(v2)
        out2 = os.path.join(tmp, "out2")
        assert tci2sfz.convert_one(v2, out2, "TestV2", "Z3")
        sfz2 = os.path.join(out2, "TestV2", "Z3", "instrument.sfz")
        text2 = open(sfz2, encoding="utf-8").read()
        assert text2.count("<region>") == 1
        assert os.path.exists(os.path.join(
            out2, "TestV2", "Z3", "samples", "TestV2_Z3_V01_RR1.wav"))
    print("tci2sfz: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
