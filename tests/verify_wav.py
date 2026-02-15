#!/usr/bin/env python3
import sys
import wave

if len(sys.argv) != 4:
    print("usage: verify_wav.py <wav> <sr> <min_frames>")
    sys.exit(2)

wav_path = sys.argv[1]
expected_sr = int(sys.argv[2])
min_frames = int(sys.argv[3])

with wave.open(wav_path, "rb") as w:
    ch = w.getnchannels()
    sr = w.getframerate()
    frames = w.getnframes()

if ch != 1:
    print(f"expected mono but got channels={ch}")
    sys.exit(1)
if sr != expected_sr:
    print(f"expected sr={expected_sr} but got sr={sr}")
    sys.exit(1)
if frames < min_frames:
    print(f"expected frames >= {min_frames} but got {frames}")
    sys.exit(1)

print(f"ok: channels={ch} sr={sr} frames={frames}")
