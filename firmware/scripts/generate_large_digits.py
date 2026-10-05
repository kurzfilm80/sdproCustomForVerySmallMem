#!/usr/bin/env python3
"""Build a small 48px numeric GFX font from the bundled SIL OFL Inter Tight font.

Resampling happens at build-asset generation time, never in the display loop.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
source = (root / "src/fonts/InterTightBold36.h").read_text()
bitmap_source = source.split("InterTightBold36Bitmaps[] PROGMEM = {", 1)[1].split("};", 1)[0]
bitmap = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", bitmap_source))
glyphs = {int(code, 16): tuple(map(int, values.split(',')))
          for values, code in re.findall(r"\{ ([\d, -]+) \}, // U\+([0-9A-F]+)", source)}
output = bytearray()
metadata = []
# Include ':' so the clock can use this large, compact numeric face too.
for code in range(0x2D, 0x3B):
    offset, width, height, advance, ox, oy = glyphs[code]
    w, h = (width * 4 + 2) // 3, (height * 4 + 2) // 3
    bits = []
    for y in range(h):
        for x in range(w):
            bit = min(y * 3 // 4, height - 1) * width + min(x * 3 // 4, width - 1)
            bits.append((bitmap[offset + bit // 8] >> (7 - bit % 8)) & 1)
    metadata.append((len(output), w, h, round(advance * 4 / 3), round(ox * 4 / 3), round(oy * 4 / 3)))
    for index in range(0, len(bits), 8):
        output.append(sum(bit << (7 - position) for position, bit in enumerate(bits[index:index + 8])))
assert len(output) < 65536
rows = ['  ' + ', '.join(f'0x{b:02X}' for b in output[i:i+16]) + ',' for i in range(0,len(output),16)]
text = '#pragma once\n\n// Derived from bundled Inter Tight under SIL Open Font License 1.1.\n'
text += 'const uint8_t InterTightDigits48Bitmaps[] PROGMEM = {\n' + '\n'.join(rows) + '\n};\n'
text += 'const GFXglyph InterTightDigits48Glyphs[] PROGMEM = {\n'
text += '\n'.join('  { ' + ', '.join(map(str,glyph)) + ' }, // U+' + f'{code:04X}'
                  for code,glyph in zip(range(0x2D,0x3B),metadata)) + '\n};\n'
text += 'const GFXfont InterTightDigits48 PROGMEM = {\n  (uint8_t *)InterTightDigits48Bitmaps,\n  (GFXglyph *)InterTightDigits48Glyphs,\n  0x002D, 0x003A, 59\n};\n'
(root/'src/fonts/InterTightDigits48.h').write_text(text)
print(f'48px numeric font: {len(output)} bitmap bytes, {len(metadata)} glyphs')
