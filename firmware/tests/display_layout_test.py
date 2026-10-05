"""Check actual bundled glyph bounds for the 240x240 baseline layouts."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
fonts = {}
for size, name in [(13, "InterTightCompact13"), (18, "InterTightBold18"),
                   (24, "InterTightBold24"), (36, "InterTightBold36"), (48, "InterTightDigits48")]:
    text = (root / "src/fonts" / (name + ".h")).read_text()
    fonts[size] = {int(code, 16): tuple(map(int, fields.split(",")))
                   for fields, code in re.findall(r"\{ ([\d, -]+) \}, // U\+([0-9A-F]+)", text)}

def width(text, size):
    if size == 6: return 6 * len(text)
    glyphs = [fonts[size][ord(c)] for c in text]
    return sum(g[3] for g in glyphs[:-1]) + glyphs[-1][4] + glyphs[-1][1]

def fit(text, limit, maximum=24):
    return next(size for size in [48,36,24,18,13,6] if size<=maximum and width(text,size)<=limit)

def box(text, size, x, baseline, anchor='left'):
    advance = width(text, size)
    x -= advance if anchor=='right' else advance//2 if anchor=='center' else 0
    pixels = []
    for character in text:
        _, w, h, dx, ox, oy = fonts[size][ord(character)] if size != 6 else (0,6,8,6,0,0)
        if w and h: pixels.append((x+ox,baseline+oy,x+ox+w,baseline+oy+h))
        x += dx
    result = (min(p[0] for p in pixels),min(p[1] for p in pixels),
              max(p[2] for p in pixels),max(p[3] for p in pixels))
    assert result[0]>=4 and result[1]>=4 and result[2]<=236 and result[3]<=236, (text,result)
    return result

def disjoint(boxes):
    for i, a in enumerate(boxes):
        for b in boxes[i+1:]:
            assert a[2]<=b[0] or b[2]<=a[0] or a[3]<=b[1] or b[3]<=a[1], (a,b)

# Header priorities are fixed: a yellow numeric 48px clock, date 24, and a
# compact centre lane for weekday/city.  This includes the widest valid time.
for weekday in ['MON','TUE','WED','THU','FRI','SAT','SUN','---']:
    city='SEOUL'
    header=[box('12/31',24,4,31),box('23:59',48,236,42,'right'),
            box(weekday,13,85,29,'center'),box(city,13,80,47,'center')]
    disjoint(header)

# WEATHER: an 84px primary icon, 48px high temperature, and a precipitation column.
for number in ['--','23','-10','-100']:
    high_size=fit(number,78,48); low_size=fit(number,78,36)
    percent_size=fit('100%',38,24)
    disjoint([(4,60,88,144),
              box(number,high_size,178,105,'right'),
              (181,105-high_size*3//4,190,114-high_size*3//4),
              box(number,low_size,178,142,'right'),
              (181,142-low_size*3//4,190,151-low_size*3//4),
              (204,69,228,93),box('100%',percent_size,216,130,'center')])

# WEATHER compact rows use weekday, icon, temperatures, and only the percent.
for row in [151,195]:
    for weekday in ['WED','MON','---']:
        for number in ['23','-10','-100','--']:
            high_size=fit(number,42,36); low_size=fit(number,34,24)
            percent_size=fit('100%',38,18)
            disjoint([box(weekday,24,5,row+30),(62,row+3,98,row+39),
                      box(number,high_size,144,row+30,'right'),
                      (147,row+30-high_size*3//4,156,row+39-high_size*3//4),
                      box(number,low_size,187,row+30,'right'),
                      (190,row+30-low_size*3//4,195,row+35-low_size*3//4),
                      box('100%',percent_size,234,row+30,'right')])

# AIR QUALITY: grade face, large reading, and badge fit into each today half.
for number in ['--','0','18','100','999']:
    today=[box('PM2.5',18,60,76,'center'),box('PM10',18,180,76,'center'),
           (82,86,118,122),(126,87,160,121)]
    for x in [48,199]:
        today.append(box(number,fit(number,62,48),x,121,'center'))
    today.extend([(10,125,110,145),(130,125,230,145)])
    disjoint(today)

    for row in [153,195]:
        future=[box('WED',24,4,row+29),(72,row+4,100,row+32)]
        future.extend([box(number,fit(number,48,36),134,row+26,'center'),
                       (106,row+29,166,row+44),
                       box(number,fit(number,48,36),207,row+26,'center'),
                       (175,row+29,235,row+44)])
        disjoint(future)

# Extreme valid API values are bounded to 999 rather than clipping.
box('999',fit('999',62,48),48,121,'center')
box('999',fit('999',48,36),207,221,'center')

print('PASS: framed high-contrast layout, large yellow clock, weather priority, grade faces and bounded badges')
