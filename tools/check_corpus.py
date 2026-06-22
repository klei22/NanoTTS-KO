#!/usr/bin/env python3
"""Render each non-comment line and report parser/render failures."""
import argparse, subprocess, tempfile
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("binary",type=Path);p.add_argument("corpus",type=Path)
a=p.parse_args();failed=[];count=0
with tempfile.TemporaryDirectory() as td:
    for lineno,line in enumerate(a.corpus.read_text(encoding="utf-8").splitlines(),1):
        text=line.strip()
        if not text or text.startswith("#"):continue
        count+=1;out=Path(td)/f"{lineno}.wav"
        r=subprocess.run([str(a.binary),"--text",text,"-o",str(out)],capture_output=True,text=True)
        if r.returncode or not out.exists() or out.stat().st_size<=44:
            failed.append((lineno,text,r.stderr.strip()))
print(f"rendered {count} inputs; failures: {len(failed)}")
for item in failed:print(f"line {item[0]}: {item[1]}: {item[2]}")
raise SystemExit(bool(failed))
