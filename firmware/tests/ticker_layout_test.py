"""Verify ticker text and chart separation using the actual bundled glyphs."""
from pathlib import Path
import re
import runpy
from itertools import product

root = Path(__file__).resolve().parents[1]
helpers = runpy.run_path(str(root / "tests/display_layout_test.py"))
width, box, disjoint = (helpers[k] for k in ("width", "box", "disjoint"))
model = (root / "src/TickerModel.h").read_text()
source = (root / "src/Ticker.cpp").read_text()


def baseline(name):
    return int(re.search(rf"kTicker{name}Baseline = (\d+)", model)[1])

graph_tops = re.search(r"return profitVisible \? (\d+) : (\d+)", model)
profit_top, plain_top = map(int, graph_tops.groups())

for name, period in product(["Bitcoin", "SAMSUNG", "WWWWWWWWWWWWWWWWWWWWWWW", "long name with descenders"],
                            ["1D", "5D", "1M", "3M", "6M", "1Y"]):
    name = name[:23]
    name_width = 2 * (236 - width(period, 24) - 6 - 120)
    size = 24
    while width(name, size) > name_width:
        name = name[:-4] + "..."
    name_box = box(name, size, 120, baseline("Name"), "center")
    period_box = box(period, size, 236, baseline("Name"), "right")
    disjoint([name_box, period_box])
    assert period_box[0] - name_box[2] >= 6
    for prefix in ["$", "W", "EUR"]:
        for number in ["0.00", "123.45", "99999999.00", "1000000000.00", "0.000001"]:
            numeric, currency = next((n, c) for n, c in [(48, 36), (36, 36), (24, 24), (18, 18)]
                                     if width(number, n) + width(prefix, c) + 4 <= 232)
            left = 120 - (width(number, numeric) + width(prefix, currency) + 4) // 2
            price_boxes = [box(prefix, currency, left, baseline("Price")),
                           box(number, numeric, left + width(prefix, currency) + 4, baseline("Price"))]
            for change in ["10.00 (5.00%)", "1000000000.00 (1000000000.00%)", "0.00 (0.00%)", "Change unavailable", ""]:
                if not change:
                    assert "if (*changeText) display.drawString" in source
                    continue
                text = change
                icon_width = 0 if change in ["0.00 (0.00%)", "Change unavailable", ""] else 16
                change_size = next((s for s in [24, 18, 13] if width(text, s) <= 232 - icon_width), None)
                if change_size is None:
                    text = change.split("(")[-1].rstrip(")")
                    change_size = next(s for s in [24, 18] if width(text, s) <= 232 - icon_width)
                change_left = 120 - (width(text, change_size) + icon_width) // 2
                change_box = box(text, change_size, change_left + icon_width, baseline("Change"))
                if icon_width:
                    icon_box = (change_left, baseline("Change") - 13, change_left + 13, baseline("Change") - 2)
                    assert icon_box[0] >= 4
                    disjoint([icon_box, change_box, *price_boxes, (4, plain_top, 236, 237)])
                disjoint([name_box, *price_boxes, change_box, (4, plain_top, 236, 237)])
                profit = box("P/L -100.00%", 24, 120, baseline("Profit"), "center")
                disjoint([name_box, *price_boxes, change_box, profit, (4, profit_top, 236, 237)])

print("PASS: equal-size ticker name/period, direction icons, prices and dynamic chart bounds")
