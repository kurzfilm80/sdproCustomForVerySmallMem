"""Verify the combined investment pages fit the SD PRO 240px display."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
font_text = (root / "src/fonts/InterTightBold18.h").read_text()
font = {
    int(code, 16): tuple(map(int, fields.split(",")))
    for fields, code in re.findall(
        r"\{ ([\d, -]+) \}, // U\+([0-9A-F]+)", font_text
    )
}


def width(text):
    glyphs = [font[ord(char)] for char in text]
    return sum(glyph[3] for glyph in glyphs[:-1]) + glyphs[-1][4] + glyphs[-1][1]


labels = [
    "CASH", "BONDS", "MIXED", "MATER", "REITS", "STOCK",
    "GLOBL", "US", "CHINA", "KOREA", "FIN", "BIO", "SEMI",
    "AUTO", "CONS", "MKT", "ENRG", "TECH", "VALUE", "GOLD",
    "HLTH", "TOTAL",
]
assert all(len(label) <= 5 for label in labels)

# Body and pinned total use one 18px face across all four columns.
for label, share, amount, profit in [
    ("MATER", "7%", "5", "-16.9%"),
    ("CHINA", "21%", "23", "-29.9%"),
    ("VALUE", "12%", "108", "--"),
    ("TOTAL", "100%", "198", "+13.2%"),
]:
    assert width(label) <= 68, (label, width(label))
    assert width(share) <= 48, (share, width(share))
    assert width(amount) <= 58, (amount, width(amount))
    assert width(profit) <= 76, (profit, width(profit))

source = (root / "src/InvestmentSummary.cpp").read_text()
model = (root / "src/InvestmentModel.h").read_text()
assert "drawGrid(" not in source
assert "kInvestmentPageCount = 2" in model
assert "kInvestmentVisibleRows = 4" in model
assert "scrollOffset" in source and "kInvestmentScrollRowHeight" in source
assert 'retirement ? "RETIREMENT" : "INVESTMENT"' in source

print("PASS: two scrolling account pages, five visible data lines, abbreviated labels and balanced columns")
