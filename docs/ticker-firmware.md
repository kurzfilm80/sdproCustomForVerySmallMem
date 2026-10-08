# SD PRO weather and ticker firmware

## Images and build

`sdpro-weather.bin` keeps the existing weather and air-quality firmware.
`sdpro-ticker.bin` is a separate application, using the same SD PRO display pins,
4 MB DIO/40 MHz flash and `eagle.flash.4m2m.ld` layout. `sdpro` remains an alias
for the legacy weather build. Switching images preserves EEPROM and LittleFS.
The ticker image does not poll weather or air-quality services.

Use Python 3.12 and Node.js 24 (the portal tests verify modern Unicode-v patterns).

```sh
python3 -m venv .venv
.venv/bin/pip install platformio==6.1.19 zopfli
make build-firmwares
make test-firmwares
```

The build exports `dist/sdpro-weather.bin` and `dist/sdpro-ticker.bin`; these
are not tracked. Both profiles share the existing 2 MB LittleFS layout and
roughly 1 MB maximum executable image size. Export rejects images above
1,044,464 bytes, but the running device's `ESP.getFreeSketchSpace()` is the
final authority for OTA. Do not upload the ESP32 partition tables or a filesystem
image. `make build-firmwares` installs the pinned dependencies and generates the
minified, Zopfli-compressed web assets before PlatformIO runs.

## Ticker scope and provenance

Adapted from `giovi321/smalltv-mod` revision
`9201e86362164177941b5a1754acf5161ae269c0`: `StockClient.cpp` filtered Yahoo
chart parsing, finite close sampling and daily change calculation; `TickerMode.cpp`
price/change/sparkline rendering design; `StockData.h` cached data/error/retry model.
WTFPL v2 attribution and original license are in `THIRD_PARTY.md` and
`docs/smalltv-mod-LICENSE.txt`. SD PRO integration and web UI are MIT.
The earlier user's lean BIN was not available in this checkout, so exact binary
feature parity with that historical artifact is not established.

This memory-bounded port supports up to 8 Yahoo symbols (stocks, ETFs, crypto,
FX), daily price/change, a 1-day chart with at most 32 samples, quantity/unit cost
and per-position P/L percentage. It uses 3–120 second rotation and 60–3600 second
per-symbol refresh, alternating Yahoo mirrors on retries. Cached quotes survive
transient errors in RAM and are labeled STALE; prices are not persisted across
power loss. Settings survive reboot and OTA. No API key is required.

The entire upstream multi-mode firmware is not imported. Upstream cash.ch,
custom-webhook feeds, arbitrary chart ranges, aggregated portfolio page, advanced
styling, radar, usage, HA/MQTT and WireGuard are not part of this port. The current
upstream lean build has additional modes; it must never be flashed to SD PRO
without adapting its hardware mapping.

Yahoo HTTPS uses `setInsecure()`, matching upstream/public-price handling: traffic
is encrypted but server identity is not authenticated. Quotes are informational,
may be delayed and may be rejected by Yahoo (HTTP 429, regional restrictions,
future API changes). HTTP, JSON and low-heap failures retain cached prices and
retry per symbol. A full TLS RX buffer is retained instead of assuming Yahoo
supports MFLN. Heap/block guards are 30,000/20,000 bytes. HTTP operations are
synchronous with 6-second timeouts, so HTTP/display can pause during a fetch;
physical-device timing, heap minima and watchdog behavior remain required checks.

## Local web API and persistent configuration

`/` opens the ticker editor. `/network` and `/update` open the shared management
portal. All ticker routes use the same LAN restriction and configured admin
authentication as the weather API. Auth may be disabled by existing settings;
this is a trusted-LAN device, not an Internet-facing service.

- `GET /api/v1/tickers`: settings, cached quotes, HTTP codes and heap diagnostics.
- `POST /api/v1/tickers`: at most 2048 bytes of JSON, strict validated settings.
- `POST /api/v1/tickers/refresh`: queue a refresh; one symbol is fetched per loop.

```json
{"schemaVersion":1,"rotateSeconds":10,"refreshSeconds":300,"positions":[{"symbol":"AAPL","quantity":2,"cost":150},{"symbol":"BTC-USD","quantity":0,"cost":0}]}
```

Settings are stored in `/ticker-settings.json` through temporary-file replacement.
Invalid known fields are normalized independently when loading. A missing schema
means legacy schema 1; malformed JSON or unsupported schema is quarantined as
`/ticker-settings.invalid` without touching Wi-Fi, weather settings or LittleFS.
POST rejects invalid fields, duplicates or excessive positions instead of silently
losing user input. Empty positions are supported. Quantity/cost must be finite
numbers from 0 to 1e9; accepted symbols are uppercase ASCII letters, digits,
`.-^=_`, with 1–23 bytes. Unknown additive JSON fields are ignored.

## OTA acceptance procedure (requires user approval before upload)

No physical device was uploaded during development. Do all uploads from a computer
on the device's LAN. Obtain approval for the exact device and exact image first.

1. Save the working firmware BIN and SHA-256, record model and IP, and retain UART
   recovery access. This checkout does not contain a stock dump; obtain one before
   replacing stock firmware. Do not read stock `/config` (it exposes Wi-Fi secrets).
2. Inspect the build's RAM/Flash output and BIN SHA-256. Confirm ESP8266 image,
   4 MB flash mode/layout and the current firmware's available sketch space.
3. For existing custom firmware, enable direct OTA if necessary and open
   `http://DEVICE_IP/update`. Select `sdpro-ticker.bin`; keep power stable. For
   stock firmware only, rename a copy to `SDP-Ticker.bin` and use its
   `/update_ota` flow. Never erase flash or upload filesystem partitions.
4. After reboot, verify Wi-Fi and `/api/v1/info` version, then `/api/v1/tickers`.
   Add AAPL and BTC-USD (and a Korean Yahoo symbol such as 005930.KS); confirm
   display rotation, chart, price and stored settings after a power cycle.
5. Test an invalid symbol (e.g. NO_SUCH_SYMBOL_TEST): HTTP/JSON failure should
   show retry state without reboot. Temporarily disrupt Internet access after
   obtaining a quote: the cached price must show STALE, then recover.
6. Observe free heap, maximum block and HTTP availability for at least 30 minutes
   with 8 symbols. Record minimum heap and any watchdog/reset reason. Test OTA
   authorization failures, interrupted upload and retry without losing the portal.
7. Test Wi-Fi recovery with an unavailable AP; confirm setup AP and `/network`
   allow provisioning. This requires a local operator and UART fallback.
8. Upload the verified `sdpro-weather.bin` through `/update` after approval;
   confirm weather/air-quality pages and unchanged city/display/Wi-Fi settings.
   Switch back to ticker and confirm the ticker settings still exist.

Passing host tests/builds does not establish passing hardware OTA or TLS behavior.
Keep the PR unmerged until this acceptance procedure is recorded.
