# SD PRO Seoul Weather firmware

A dedicated ESP8266 firmware for the JUZIPi SD PRO: three-day Seoul weather
and air quality forecasts, KST NTP clock and direct ST7789 drawing.

WEATHER is shown for 12 seconds, then AIR QUALITY for 8 seconds. Page changes
render cached data only and never trigger API calls. WEATHER uses 36x36 primitive
icons, 24px temperatures and 18px labels. AIR QUALITY uses 36px numbers and a
three-day PM2.5/PM10 table. The third row shows the actual weekday. Forecast dates
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

A failed air quality request displays `--` without affecting weather. A failed
weather request retains its dated cache; AIR QUALITY continues to rotate and
update independently. Both APIs normally update every 15 minutes; failed weather
requests retain the existing 60-second retry. Each request releases its TLS
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
ESP8266, 4MB DIO/40MHz flash, 160MHz CPU, `eagle.flash.4m2m.ld`, and the original
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
new brightness settings use `/weather-settings.json` with `schemaVersion: 1`.
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
a failed request retries after 60 seconds and preserves the last valid forecast.
HTTPS remains enabled with the original BearSSL `setInsecure()` mode.

## Removed features

Home Assistant dashboard/data APIs, images/animation, graphs/history, scene
compiler/rendering/transitions, notifications, custom fonts, screen capture,
ESP32/GeekMagic profiles and their large web application have been removed.
The display renders directly without framebuffers or scene allocations.

`web/index.html` has no JavaScript or npm dependencies. To regenerate its
flash-resident Zopfli-compressed header after editing:

```sh
$HOME/.platformio/penv/bin/python scripts/embed_web.py
```

Successful PlatformIO builds also copy the firmware to
`../../SDP-SeoulWeather.bin` (the parent `SDPRO` folder), replacing the previous
export. The original `.pio/build/sdpro/firmware.bin` remains available.

Build success verifies compilation and image size. Wi-Fi recovery, Web OTA and
TLS memory headroom still require validation on the physical SD PRO.
