# Changelog

## Unreleased

### Features

- Configure automatic or fixed WEATHER/AIR QUALITY display from the web portal;
  choose 5/10/15/30/60-second durations (defaults 15/10 seconds) and apply without
  rebooting while both APIs keep their independent refresh schedules.
- Set 20/40/60/80/100% day brightness (default 80%) and optional KST night mode,
  with configurable start/end (defaults 22:00/07:00) and 20/30/40/50% night
  brightness (default 30%); handle overnight intervals and unsynchronized time.

- Select SEOUL, BUSAN, INCHEON, DAEJEON, DAEGU, GWANGJU, ULSAN, SUWON or JEJU
  from the web portal, or enter a CUSTOM name and coordinates. Persist only
  location metadata in schema-1 LittleFS settings; retain SEOUL as the default.
- Apply a city change without rebooting or rebuilding: clear both forecast caches,
  reset retry schedules, fetch new Open-Meteo data and show the selected city.

- Provide a dedicated weather clock with three daily forecasts and KST NTP time.
- Show separate WEATHER and AIR QUALITY pages with configurable durations from
  cached data, a shared date/weekday/city/time header and actual KST weekdays.
- Show three-day PM2.5/PM10 daily maximum forecasts with Korean grade thresholds
  and cyan, green, orange and red colors; refresh every 15 minutes.
- Draw scalable WMO weather icons with TFT primitives for sun, partial cloud,
  cloud, rain, snow, fog and thunderstorms without image buffers.
- Provide a small Wi-Fi, recovery AP, admin password, brightness and Web OTA portal.

### Fixes

- Show only actual KST weekdays for tomorrow and day-after, wrap Saturday/Sunday
  correctly and show `---` until time synchronization.
- Improve black-background readability with white labels/temperatures, cyan rain,
  light-grey date/time and missing values and grade colors reserved for PM readings.
- Match the high-contrast framed clock mockup with a yellow 48px clock, cyan
  weekday/city lane, 84px primary weather icon, orange high temperature and
  cyan precipitation column. Add PM grade faces beside today’s large readings,
  retain colored rounded badges and simplify future rows around their values.

- Allow Wi-Fi-only setup without requiring a separate admin password by default.
- Report Wi-Fi, hostname/username and admin password validation errors separately;
  reject SSIDs that would be truncated when saved.

- Back off failed weather and air-quality requests for 30 seconds, 60 seconds,
  2 minutes and up to 5 minutes; retain both last successful forecasts.
- Stop recovery AP and captive DNS after station connectivity returns.
- Reject null or invalid weather values and retain the last successful forecast.
- Show unavailable air quality as `--` without affecting the weather forecast.
- Report DNS lookup failures, TLS errors and available heap for weather requests.

### Refactoring

- Run ESP8266 at 80MHz, remove mDNS and batch application checks at 100ms
  with a 10ms cooperative idle while continuing to service HTTP and captive DNS.
- Redraw only on page entry, new page data, display changes or the clock minute changing;
  use non-blocking configurable page timers.
- Control active-low GPIO5 PWM at 1kHz and default to 80%, preserving saved brightness.
- Keep Wi-Fi sleep disabled pending physical OTA/web/NTP stability validation.

- Remove Home Assistant dashboards, images, animation, graphs, scene compilation,
  transitions, notifications, custom fonts and screenshot code and web dependencies.
- Replace the scene renderer with direct ST7789 drawing and four fixed fonts.
- Filter 72 hourly timestamps and PM2.5/PM10 samples into a bounded document,
  reduce by KST date and retain only three daily MAX records.
- Retain dated weather forecasts and match caches to current KST dates.
- Release each API's TLS buffers before starting the next request.
- Retain SD PRO hardware/flash settings, EEPROM layout and network JSON schema.
- Keep display preferences in an additive schema-1 weather settings document,
  normalizing older brightness-only files and importing legacy display brightness;
  leave EEPROM, city settings and legacy LittleFS assets intact.
- Restrict the firmware build to the confirmed SD PRO profile.

### Testing and tooling

- Check all weekday rollover combinations and unsynchronized placeholders;
  verify shared-header and today/compact-row glyph bounds, numeric fallbacks,
  grade faces and 4px margins; generate a 1.6KiB 48px numeric clock font ahead
  of time with no runtime scaling or additional framebuffer.

- Verify display JSON serialization/normalization, compatibility with brightness-only
  schema-1 settings, independent invalid-field recovery, night interval boundaries,
  fixed/automatic page timers and millis rollover; test saved controls in the portal.

- Test bounded coordinate parsing and name validation, plus saved-city form
  initialization, conditional custom inputs and Terser-minified script behavior;
  verify both API URL templates, coordinate bounds and encoded KST timezone.

- Export successful builds to `SDP-SeoulWeather.bin` in the parent SDPRO folder.
- Replace dashboard web embedding with a standalone Zopfli-compressed portal.
- Verify the SD PRO PlatformIO build and compare static RAM and image sizes
  against the pre-change local image.
- Test grade boundaries, rounding, date rollover, missing hourly data, daily MAX
  grouping, filtered parsing, live Weather/Air responses and page timer rollover.
