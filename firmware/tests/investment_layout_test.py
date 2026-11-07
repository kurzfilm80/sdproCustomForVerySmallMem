"""Verify the investment pages fit the actual SD PRO bundled glyphs."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]


def load_font(name):
    text = (root / "src/fonts" / f"{name}.h").read_text()
    return {
        int(code, 16): tuple(map(int, fields.split(",")))
        for fields, code in re.findall(
            r"\{ ([\d, -]+) \}, // U\+([0-9A-F]+)", text
        )
    }


def width(text, font):
    glyphs = [font[ord(char)] for char in text]
    return sum(glyph[3] for glyph in glyphs[:-1]) + glyphs[-1][4] + glyphs[-1][1]


bold18 = load_font("InterTightBold18")
bold24 = load_font("InterTightBold24")
compact13 = load_font("InterTightCompact13")

assets = [
    ("BONDS", "+61.4%", "17", "9%"),
    ("MATERIAL", "-2.9%", "27", "14%"),
    ("CRYPTO", "-29.9%", "40", "20%"),
    ("REITS", "+6.9%", "7", "3%"),
    ("STOCKS", "-2.0%", "108", "54%"),
    ("TOTAL", "-6.2%", "198", "100%"),
]
regions = [
    ("GLOBAL", "+0.7%", "9", "8%"),
    ("U.S.", "+0.3%", "43", "40%"),
    ("CHINA", "-17.7%", "7", "7%"),
    ("KOREA", "-1.8%", "49", "46%"),
    ("TOTAL", "-2.1%", "108", "100%"),
]
stock_goals = [
    ("FIN/VALUE", "11", "11%"),
    ("BIO/HLTH", "11", "10%"),
    ("SEMIS", "23", "21%"),
    ("IND/AUTO", "4", "4%"),
    ("CONSUMER", "2", "2%"),
    ("MARKET", "12", "11%"),
    ("ENERGY", "5", "5%"),
    ("AI/TECH", "39", "37%"),
    ("TOTAL", "108", "100%"),
]
retirement_assets = [
    ("CASH", "0.0%", "0", "0%"),
    ("BONDS", "-0.5%", "10", "14%"),
    ("MIXED", "+4.2%", "19", "27%"),
    ("MATERIAL", "-16.9%", "5", "7%"),
    ("REITS", "--", "0", "0%"),
    ("STOCKS", "+30.0%", "37", "52%"),
    ("TOTAL", "+13.2%", "71", "100%"),
]
retirement_regions = [
    ("GLOBAL", "4", "8%"),
    ("U.S.", "29", "52%"),
    ("CHINA", "2", "3%"),
    ("KOREA", "21", "37%"),
    ("TOTAL", "56", "100%"),
]
retirement_stock_goals = [
    ("VALUE", "7", "12%"),
    ("GOLD", "0", "0%"),
    ("HEALTH", "2", "3%"),
    ("MARKET", "13", "24%"),
    ("AI/TECH", "33", "59%"),
    ("TOTAL", "56", "100%"),
]

for label, profit, holding, share in assets + regions:
    assert 4 + width(label, compact13) + 3 <= 80, label
    assert 4 + width(profit, compact13) + 3 <= 80, profit
    assert width(holding, bold24) <= 72, holding
    assert width(share, bold24) <= 72, share

for label, holding, rate in stock_goals:
    assert 4 + width(label, compact13) + 3 <= 80, label
    assert width(holding, bold24) <= 72, holding
    assert width(rate, bold24) <= 72, rate

for label, profit, holding, share in retirement_assets:
    assert 4 + width(label, compact13) + 3 <= 80, label
    assert 4 + width(profit, compact13) + 3 <= 80, profit
    assert width(holding, bold18) <= 72, holding
    assert width(share, bold18) <= 72, share

for label, holding, share in retirement_regions + retirement_stock_goals:
    assert 4 + width(label, compact13) + 3 <= 80, label
    assert width(holding, bold24) <= 72, holding
    assert width(share, bold24) <= 72, share

assert 5 * 40 + 30 + 3 <= 239
assert 4 * 48 + 35 + 3 <= 239
assert (8 * 240) // 9 + 19 + 3 <= 239
assert (6 * 240) // 7 + 28 + 3 <= 239
assert 4 * 48 + 35 + 3 <= 239
assert 5 * 40 + 30 + 3 <= 239
assert width("FETCH FAILED", bold18) <= 240

print("PASS: all six investment and retirement pages fit their bundled fonts")
