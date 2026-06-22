#!/usr/bin/env python3
import argparse, wave
from pathlib import Path
p=argparse.ArgumentParser(description="Convert NanoTTS PCM16LE to WAV")
p.add_argument("input",type=Path);p.add_argument("output",type=Path)
p.add_argument("--rate",type=int,default=16000)
a=p.parse_args();raw=a.input.read_bytes()
if len(raw)%2: raise SystemExit("input byte count is not even")
with wave.open(str(a.output),"wb") as w:
    w.setnchannels(1);w.setsampwidth(2);w.setframerate(a.rate);w.writeframes(raw)
