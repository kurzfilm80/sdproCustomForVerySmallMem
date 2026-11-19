"""Check the approved 240px single-row investment layout."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
font_text = (root / 'src/fonts/InterTightBold18.h').read_text()
font = {
    int(code, 16): tuple(map(int, fields.split(',')))
    for fields, code in re.findall(r'\{ ([\d, -]+) \}, // U\+([0-9A-F]+)', font_text)
}


def width(text):
    glyphs = [font[ord(char)] for char in text]
    return max(sum(g[3] for g in glyphs),
               sum(g[3] for g in glyphs[:-1]) + glyphs[-1][4] + glyphs[-1][1])


source = (root / 'src/InvestmentSummary.cpp').read_text()
model = (root / 'src/InvestmentModel.h').read_text()
view = (root / 'src/InvestmentViewModel.h').read_text()
assert 'kInvestmentPageCount = 3' in model
assert 'kInvestmentVisibleRows = 4' in model
assert 'kInvestmentScrollRowHeight = 36' in model
assert set(re.findall(r'setFreeFont\(&([^)]+)\)', source)) == {'InterTightBold18'}
assert 'setTextSize' not in source
assert 'drawFittedCell' not in source
for title in ('ASSET', 'STOCK REGION', 'STOCK THEME', 'LOADING', 'ERROR'):
    assert title in source
    assert width(title) <= 232, (title, width(title))
labels = re.findall(r'"([A-Z]+)"', view)
for label in labels:
    assert 4 + width(label) + 4 <= 70, (label, width(label))
for label, share, amount, profit in [
    ('BONDS', '9%', '17', '+61.1%'),
    ('MATER', '14%', '27', '-0.0%'),
    ('CRYPT', '20%', '40', '-31.4%'),
    ('REITS', '3%', '7', '+11.2%'),
    ('STOCK', '54%', '108', '-3.8%'),
    ('TOTAL', '100%', '199', '-7.2%'),
    ('TOTAL', '100%', '4295', '--'),
    ('CASH', '<1%', '<1', '0.0%'),
    ('VALUE', '4%', '6', '+60.8%'),
]:
    assert 4 + width(label) + 4 <= 70
    assert 70 + width(share) + 2 <= 163 - width(amount)
    assert 163 + 2 <= 236 - width(profit)
for text, baseline, top, bottom in [
    ('STOCK REGION', 24, 0, 40), ('BONDS 9% 17 +61.1%', 70, 40, 200),
    ('STOCK 54% 108 -3.8%', 178, 40, 200), ('TOTAL 100% 199 -7.2%', 222, 200, 240),
]:
    for char in text:
        _, _, height, _, _, offset = font[ord(char)]
        assert baseline + offset >= top
        assert baseline + offset + height <= bottom
for value, x in [('row.label', 4), ('share', 70), ('amount', 163), ('returnText', 236)]:
    assert f'display.drawString({value}, {x}, baseline)' in source
assert 'baseline + 38' not in source
assert 'setViewport' in source and 'resetViewport' in source
assert 'TFT_CYAN' in source
assert 'hadData' in source  # Clear loading text before drawing data bands.
print('PASS: three approved pages, uniform 18px text, four single-line rows and pinned total')
