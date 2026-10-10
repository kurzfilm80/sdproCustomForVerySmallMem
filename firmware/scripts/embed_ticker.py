#!/usr/bin/env python3
"""Generate the minified, Zopfli-compressed ticker portal."""
from pathlib import Path
import re
import subprocess
import zopfli.gzip
root = Path(__file__).resolve().parents[1]
subprocess.run(["npm", "--prefix", str(root / "web"), "run", "build:ticker"], check=True)
html = (root / "web/ticker.html").read_text().replace(
    '<script src="ticker.min.js"></script>', '<script>' + (root / "web/ticker.min.js").read_text() + '</script>')
payload = zopfli.gzip.compress(re.sub(r">\s+<", "><", html).encode(), numiterations=15)
rows = ["  " + ",".join(f"0x{b:02x}" for b in payload[i:i+16]) + "," for i in range(0,len(payload),16)]
(root / "src/TickerAssets.generated.h").write_text(
    '#pragma once\n#include <Arduino.h>\nconst uint8_t kTickerGzip[] PROGMEM = {\n' + '\n'.join(rows) + '\n};\n')
print(f"Ticker portal: {len(payload)} gzip bytes")
