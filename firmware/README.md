# SD PRO Weather / Air Quality firmware

A dedicated ESP8266 firmware for the JUZIPi SD PRO: three-day weather
and air quality forecasts for a selectable city (SEOUL by default), KST NTP clock and direct ST7789 drawing.

By default WEATHER is shown for 15 seconds, then AIR QUALITY for 10 seconds.
The web portal configures rotation, durations or a fixed page. Page changes
render cached data only and never trigger API calls. WEATHER gives today a 60px
primitive icon and up to 48px high temperature, then uses compact 32px rows for
the next two days. AIR QUALITY gives today's PM values up to 48px and keeps later
days compact. Both pages share a fixed-priority date/weekday/city/time header and
show only the actual KST weekday on later rows (`---` before NTP). Forecast dates
are cached and matched to the current KST date, including after midnight.

Air quality requests use `hourly=pm2_5,pm10`, `forecast_days=3` and
`timezone=Asia/Seoul`. Only `hourly.time`, `hourly.pm2_5` and `hourly.pm10` are
retained in a bounded filtered document (4,808 bytes on ESP8266) while parsing.
The 72 hourly samples are grouped by their date and reduced to three independent
PM2.5 and PM10 daily maximums; the document is freed on return. Only these small
daily records remain cached. Missing/null/invalid samples are ignored separately
for each pollutant, and an entirely unavailable day/pollutant displays `--`.

Concentrations are micrograms per cubic meter. Each daily MAX is rounded to the
nearest integer; that same integer determines the grade. PM2.5 boundaries are
15/35/75, and PM10 boundaries are 30/80/150. Grades are GOOD (cyan), NORMAL
(green), BAD (orange), VERY BAD (red). PM2.5 18 and PM10 32 both receive NORMAL.

Failed requests retain the last successful forecast (or display `--` when there
is no valid air-quality cache); AIR QUALITY continues to rotate independently.
Both APIs normally update every 15 minutes; failed requests retry after 30s,
60s, 2min and at most 5min. Each request releases its TLS
client before the other API is called. The web server is serviced between the
requests, and HTTP stream timeouts remain 12 seconds. Networking is synchronous,
so a slow HTTPS connection can temporarily delay a page transition; it cannot
start concurrent requests. Runtime timing and OTA still require hardware testing.

Air quality data: [Open-Meteo Air Quality API](https://open-meteo.com/en/docs/air-quality-api),
using Copernicus Atmosphere Monitoring Service (CAMS) model data.

## Build

From this `firmware` directory:

```sh
pio run -e sdpro
```

If PlatformIO is not in your PATH on this Mac:

```sh
export PATH="$HOME/.platformio/penv/bin:$PATH"
pio run -e sdpro
```

The image is `.pio/build/sdpro/firmware.bin`. The SD PRO configuration remains
ESP8266, 4MB DIO/40MHz flash, 80MHz CPU, `eagle.flash.4m2m.ld`, and the original
no-CS ST7789 pins, BGR order, inversion and active-low backlight.

## Setup and recovery

Existing EEPROM Wi-Fi credentials, hostname, admin/OTA credentials and
LittleFS `/network.json` continue to load. No filesystem formatting is used.
Without credentials, or after repeated Wi-Fi connection failures, connect to
`SDPRO-Setup-<device ID>` and open `http://192.168.4.1/`.
The portal includes captive DNS and uses the saved recovery AP password when set.

The lightweight page replaces the previous dashboard application. It provides
Wi-Fi settings, NTP server, recovery AP password, admin credentials, brightness
and firmware upload. Blank text fields preserve their saved values. Enable the
open-network checkbox to clear a saved Wi-Fi password. The authentication
checkbox controls settings and OTA passwords and is unchecked by default for
Wi-Fi-only setup. To enable it, enter an admin password (8-32 characters) or
preserve existing saved credentials by leaving the password blank.
Saving network settings restarts the device and enables Web OTA.
Existing static-IP/DNS and DHCP-NTP settings remain effective. Select DHCP to
clear a saved static IP; entering an NTP server selects manual NTP.

Seoul time uses `KST-9`. The old `/display.json` brightness is read as a fallback;
the default brightness is 80% (existing saved values are preserved);
display settings use `/weather-settings.json` with `schemaVersion: 1`.
Old dashboard, image and font files on LittleFS are left intact and are not loaded.

## Web OTA and diagnostics

Open the device address and use the Web OTA form. OTA POST routes are `/update`,
`/update_ota` and `/api/v1/firmware`. Saved OTA enable/authentication settings are
honored. Settings and OTA requests are restricted to the device's local subnet
or recovery AP subnet. Upload failures resume the application; successful uploads
restart after sending the response. No flashing is performed by a build.

`/api/v1/status` reports free heap, maximum free block, DNS, NTP and weather errors,
without returning passwords. Serial output at 115200 baud records DNS lookup,
TLS errors and memory state. A successful forecast refreshes every 15 minutes;
failed requests use independent 30s, 60s, 2min and capped 5min backoff and
preserve the last valid weather and air-quality forecasts.
HTTPS remains enabled with the original BearSSL `setInsecure()` mode.

## Removed features

Home Assistant dashboard/data APIs, images/animation, graphs/history, scene
compiler/rendering/transitions, notifications, custom fonts, screen capture,
ESP32/GeekMagic profiles and their large web application have been removed.
The display renders directly without framebuffers or scene allocations.

The portal uses a small city-form script, minified with Terser and embedded with
the HTML in a Zopfli-compressed flash asset. To regenerate after editing:

```sh
npm --prefix web ci
$HOME/.platformio/penv/bin/python scripts/embed_web.py
```

Successful PlatformIO builds also copy the firmware to
`../../SDP-SeoulWeather.bin` (the parent `SDPRO` folder), replacing the previous
export. The original `.pio/build/sdpro/firmware.bin` remains available.

Build success verifies compilation and image size. Wi-Fi recovery, Web OTA and
TLS memory headroom still require validation on the physical SD PRO.

## Power and runtime audit

Only seven source modules are built: main, device settings, stored schema helpers,
city settings, display settings, weather and air quality. DashboardValues, AnimatedImagePlayback, GraphHistory,
ImageAssets, PageTransitionRenderer, ScenePageCompiler, SceneTextCompiler,
ScreenCapture and Home Assistant polling are absent from both compilation and
runtime. Legacy EEPROM fields remain for compatibility but do not start services.
LittleFS is accessed at boot and when saving settings/brightness; no loop reads,
asset loading, background animation or dashboard refresh occurs. mDNS is removed;
access the web portal by IP or your router's hostname resolution.

HTTP and recovery DNS are serviced on every loop, with a 10ms cooperative delay.
Application state, Wi-Fi, page timing and API scheduling are checked every 100ms.
There is no application busy-wait. WEATHER redraws on entry, a changed displayed
minute or new weather data (initial failure diagnostics also redraw); AIR QUALITY
redraws on entry or new air data. Page intervals default to 15s/10s and are configurable. The air page clock
is refreshed on page entry. TLS requests remain synchronous with existing 12s
network timeouts, so web requests can still wait during a fetch; the short loop
delay does not add a long web/OTA stall. API clients and JSON documents have local
lifetimes, HTTP reuse is disabled and `http.end()` closes completed requests.

Normal operation uses WIFI_STA; a recovered station stops soft AP and DNS.
There are no explicit Wi-Fi scans. WIFI_NONE_SLEEP remains explicit for stability;
modem sleep is available in the pinned SDK but needs physical web/OTA/NTP tests
before enabling. `delay()` yields to the SDK but is not a guarantee of CPU sleep.
GPIO5 PWM is active LOW as confirmed in the existing SD PRO pinout record and
platform profile; 80% brightness writes 20/100 duty at 1kHz. The display form
offers 20/40/60/80/100%; older saved 0–100% values remain valid and appear as
a saved-value option. Preferences persist in schema 1 without changing EEPROM.
Actual wattage, temperature and TLS latency at 80MHz require device measurement.

## Service city

Use **Service city → City → Save city** in the web portal. The built-in locations
are SEOUL (default), BUSAN, INCHEON, DAEJEON, DAEGU, GWANGJU, ULSAN, SUWON and JEJU.
Choose CUSTOM for a 1–20 character printable ASCII name supported by the existing
fonts, latitude from -90 to 90 and longitude from -180 to 180 (up to six decimals).
The form loads the saved selection from authenticated `/api/v1/status` once;
there is no browser background polling. KST and Korean PM grade thresholds remain
unchanged for custom locations.

The LAN-restricted/authenticated `POST /city` validates and saves first, then
clears both RAM forecast caches and retry schedules, redraws the current page
with the selected city and requests fresh data through the normal loop. It does
not reboot or change Wi-Fi, NTP, brightness or OTA settings. Offline changes are
saved and fetched on reconnection. A failed save leaves the running city/cache
unchanged. New-city request failures never show the previous city's data.

`/city-settings.json` contains only `schemaVersion: 1`, `city`, `name`, `latitude`
and `longitude`. Coordinates are decimal strings, parsed with a bounded fixed
point helper without libc float parsing/printing. Presets live in PROGMEM; no
city forecast data is embedded or persisted. EEPROM is unchanged. Missing files
use SEOUL. Invalid known fields are repaired independently and rewritten;
malformed or unsupported documents are quarantined to `/city-settings.invalid`
and only city settings return to defaults. Forecasts remain transient RAM caches.

## Display Settings

The portal's **Save Display Settings** applies settings immediately without
restarting or fetching data. Auto Page Rotation defaults ON; each duration
supports 5, 10, 15, 30 or 60 seconds (WEATHER defaults 15, AIR QUALITY defaults 10).
Turning rotation OFF enables Fixed Page (WEATHER or AIR QUALITY). Both API
schedules continue in fixed mode, and new data redraws only its visible page.
Timers use unsigned elapsed millis and reset on saving display preferences.

Day brightness defaults 80%, with 20/40/60/80/100% choices. Night Mode defaults
OFF, with start 22:00, end 07:00 and 30% night brightness; night choices are
20/30/40/50%. Start is inclusive and end exclusive, including across midnight.
Equal times mean no night interval. Without a valid NTP clock, normal brightness
applies. The loop checks brightness once per changed clock minute and writes PWM
only when the effective brightness changes; no redraw or filesystem access is
needed. A valid KST clock continues scheduling during a Wi-Fi outage.

LAN-restricted/authenticated `POST /display-settings` validates before saving;
failed writes leave live preferences unchanged. `/weather-settings.json` remains
schema 1 with additive fields `autoRotate`, `weatherPageSeconds`, `airPageSeconds`,
`fixedPage` (WEATHER/AIR QUALITY), `brightness`, `nightModeEnabled`, `nightStart`
and `nightEnd` (HH:MM strings), and `nightBrightness`. Missing/invalid fields are
repaired individually; unsupported/malformed files are quarantined locally and
replaced with defaults. Older brightness-only files and legacy `/display.json`
brightness remain compatible. The legacy `/brightness` endpoint saves the full
new document so it cannot erase page/night preferences.

Native configuration and timing tests:

```sh
c++ -std=c++17 -Isrc -I.pio/libdeps/sdpro/ArduinoJson/src \
  tests/display_settings_test.cpp src/StoredConfig.cpp -o /tmp/sdpro-display-test
/tmp/sdpro-display-test
node tests/city_portal_test.cjs
```

## Display layout and colors

Both pages use the same framed header on BLACK: WHITE date `MM/DD` at x=4,
baseline y=31 in 24px; YELLOW time right-aligned to x=236 at y=42 in a compact
48px numeric font; and a CYAN weekday/city lane with an underline at the left of
the clock. City text is limited to that lane so it never overlays the date or
clock. A muted blue-grey rounded border encloses the display and a divider spans
x=6..233 at y=52. All content remains within the 4px safe margin.

WEATHER gives today the large upper region: an 84x84 vector icon at (4,60), an
ORANGE high and CYAN low right-aligned to x=178 at baselines y=105/142 using up
to 48px/36px, and a dedicated CYAN droplet/percentage column at x=204. Temperatures
use a vector degree mark and omit C. Tomorrow and day-after start at y=151/195:
a WHITE 24px KST weekday, 36x36 icon, WHITE high up to 36px, CYAN low up to 24px,
vertical separator and CYAN percentage. Sun/lightning are YELLOW, clouds/fog
LIGHTGREY, rain CYAN and snow WHITE; no weather-state text is rendered.

AIR QUALITY omits an AIR title. PM2.5/PM10 headers use 18px at centers x=60/180.
Today's grade-colored values use up to 48px at baseline y=121, each paired with a
colored face and a 100px rounded grade badge. Tomorrow and day-after start at
y=153/195: the KST weekday uses 24px, the PM2.5 grade gets a compact face, and
values up to 36px sit above colored badges. Values above the display range are
bounded to 999. Grade colors remain CYAN/GREEN/ORANGE/RED, with BLACK badge text.

The small 48px numeric font is generated offline from bundled Inter Tight (SIL
OFL 1.1), with only `-` through `:` glyphs and a 1,593-byte bitmap. There is no
runtime resampling or framebuffer allocation. Regenerate with
`python3 scripts/generate_large_digits.py`; bitmap offsets must stay below 64KiB.
