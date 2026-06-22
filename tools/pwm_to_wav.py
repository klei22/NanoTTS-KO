#!/usr/bin/env python3
import argparse,array,sys,wave
from pathlib import Path
p=argparse.ArgumentParser(description="Convert NanoTTS uint16le PWM compares to WAV")
p.add_argument("input",type=Path);p.add_argument("output",type=Path)
p.add_argument("--top",type=int,required=True);p.add_argument("--rate",type=int,default=16000)
p.add_argument("--inverted",action="store_true");a=p.parse_args()
if not 1<=a.top<=65535: raise SystemExit("--top must be 1..65535")
raw=a.input.read_bytes()
if len(raw)%2: raise SystemExit("input byte count is not even")
d=array.array("H");d.frombytes(raw)
if sys.byteorder!="little":d.byteswap()
out=array.array("h")
for x in d:
    if x>a.top: raise SystemExit(f"PWM value {x} exceeds TOP {a.top}")
    if a.inverted:x=a.top-x
    y=round(x*65535/a.top-32768);out.append(max(-32768,min(32767,y)))
if sys.byteorder!="little":out.byteswap()
with wave.open(str(a.output),"wb") as w:
    w.setnchannels(1);w.setsampwidth(2);w.setframerate(a.rate);w.writeframes(out.tobytes())
