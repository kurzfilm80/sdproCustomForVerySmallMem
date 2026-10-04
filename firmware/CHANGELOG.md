# Changelog

## Unreleased

### Features

- Provide a dedicated Seoul weather clock with three daily forecasts and KST NTP time.
- Show separate WEATHER and AIR QUALITY pages for 12 and 8 seconds from cached
  data, with larger fonts, 36px weather icons and the actual third-day weekday.
- Show three-day PM2.5/PM10 daily maximum forecasts with Korean grade thresholds
  and cyan, green, orange and red colors; refresh every 15 minutes.
- Draw 36x36 WMO weather icons with TFT primitives for sun, partial cloud,
  cloud, rain, snow, fog and thunderstorms without image buffers.
- Provide a small Wi-Fi, recovery AP, admin password, brightness and Web OTA portal.

### Fixes

- Allow Wi-Fi-only setup without requiring a separate admin password by default.
- Report Wi-Fi, hostname/username and admin password validation errors separately;
  reject SSIDs that would be truncated when saved.

- Wait 60 seconds after failed weather requests rather than retrying every loop.
- Reject null or invalid weather values and retain the last successful forecast.
- Show unavailable air quality as `--` without affecting the weather forecast.
- Report DNS lookup failures, TLS errors and available heap for weather requests.

### Refactoring

- Remove Home Assistant dashboards, images, animation, graphs, scene compilation,
  transitions, notifications, custom fonts and screenshot code and web dependencies.
- Replace the scene renderer with direct ST7789 drawing and four fixed fonts.
- Filter 72 hourly timestamps and PM2.5/PM10 samples into a bounded document,
  reduce by KST date and retain only three daily MAX records.
- Retain dated weather forecasts and match caches to current KST dates.
- Release each API's TLS buffers before starting the next request.
- Retain SD PRO hardware/flash settings, EEPROM layout and network JSON schema.
- Keep brightness in a separate versioned weather settings document, with existing
  display settings read as a fallback; leave legacy LittleFS assets intact.
- Restrict the firmware build to the confirmed SD PRO profile.

### Testing and tooling

- Export successful builds to `SDP-SeoulWeather.bin` in the parent SDPRO folder.
- Replace dashboard web embedding with a standalone Zopfli-compressed portal.
- Verify the SD PRO PlatformIO build and compare static RAM and image sizes
  against the pre-change local image.
- Test grade boundaries, rounding, date rollover, missing hourly data, daily MAX
  grouping, filtered parsing, live Weather/Air responses and page timer rollover.
