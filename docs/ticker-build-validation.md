# SD PRO ticker build validation — 2026-10-08

Branch: `feature/ticker-firmware`; starting commit:
`92d88dd982c5f23caf17a85077142e26e9a599d7`.
`main` was not modified or merged. No device was flashed.

## Actual PlatformIO builds

| Profile | BIN bytes | Linked Flash bytes / 1,044,464 | Linked RAM bytes / 81,920 | Result |
| --- | ---: | ---: | ---: | --- |
| sdpro-weather | 574,672 | 570,523 (54.6%) | 34,460 (42.1%) | PASS |
| sdpro-ticker | 575,648 | 571,491 (54.7%) | 35,496 (43.3%) | PASS |

ESP-12E / ESP8266, 80 MHz CPU, 4 MB DIO / 40 MHz Flash,
`eagle.flash.4m2m.ld`. Both image headers: `e9 02 02 40`.
Dependencies: PlatformIO 6.1.19; espressif8266 4.2.1; Arduino ESP8266 core 3.1.2;
GCC 10.3.0; TFT_eSPI 2.5.43; ArduinoJson 6.21.6.

```text
451a4461d27ae233ae91cc77d377400cedf8915be3d936f90ccbc3c406335b88  sdpro-weather.bin
add8feb69855afa8cf48b62aa8b15e26387250556d659da9d7cc494abd9dfb75  sdpro-ticker.bin
```

A separate detached checkout at `92d88dd` produced a weather BIN with the exact
same length and SHA-256. Both `sdpro` and `sdpro-weather` are byte-identical to
that baseline. The ticker image adds 976 BIN bytes and 1,036 linked RAM bytes.
Symbol inspection found no linked SPIFFS, `strftime`, `_printf_float` or
`_scanf_float` symbols in the ticker ELF. Build dependencies download successfully
only after mirror retries in this environment. No C/C++ compiler warnings/errors
remain; the pinned core's `elf2bin.py` emits Python 3.12 SyntaxWarnings about its
legacy regex escape strings. These are dependency warnings, not compile errors.

## Host checks

13 validation groups passed:

- 8 native C++ groups: ticker settings/roundtrip/invalid schema/duplicate and
  excessive symbols/quote parsing/downsampling/timer wrap; time formatting;
  forecast labels; air quality; refresh scheduling; display settings; city
  settings; shared stored-config validation.
- 2 portal groups: weather controls and ticker load/add/remove/save/refresh,
  8-symbol cap, Unicode-v symbol pattern validation, text escaping, stale status
  and save failures. Both source and
  Terser-minified JavaScript were exercised with a simulated DOM/API.
- 3 Python groups: weather layout source invariants; actual URL format/bounds;
  distinct firmware export paths, incremental export and OTA ceiling rejection.

Native tests used address and undefined-behavior sanitizers. LeakSanitizer was
disabled because process enumeration is unavailable in this managed runtime.
`make web-check` and `git diff --check` passed. Browser/device rendering, actual
LittleFS power-loss behavior and actual HTTP server routes were not emulated;
those remain hardware acceptance tests. Existing CI's firmware job now builds
both profiles and runs these regressions; legacy HA packaging tasks are outside
this firmware task.

## Remaining validation / limitations

- A real Yahoo AAPL chart request from this environment returned HTTP 429.
  Host fixtures parse correctly, but live device HTTPS and market-data retrieval
  have not been verified. Verify from the intended home LAN; cached-price retry
  behavior does not guarantee that Yahoo will accept requests.
- No physical OTA, reboot, persisted settings after power cycle, display timing,
  recovery AP or 8-symbol heap/endurance test has been performed. The user must
  approve the exact target device and BIN before upload.
- Only the memory-bounded Yahoo ticker subset described in
  `ticker-firmware.md` is implemented. Exact feature parity with the historical
  `smalltv-mod-firmware-lean.bin` is not established.
- Public-price HTTPS encrypts traffic without certificate verification, matching
  upstream's approach. Keep the device on a trusted LAN.
- This checkout has no stock firmware dump. Obtain and hash a stock backup before
  replacing stock; a verified weather BIN is available for custom-firmware rollback.
- GitHub Actions run 37792478454 passed firmware build/tests/artifact upload,
  integration and metadata. The legacy frontend job failed because
  `export_preview_fonts.py` had already been removed before this branch.
  The follow-up build repair uses the committed `firmware-fonts.generated.json`
  instead. Local `make card-check card-build` passes with unchanged generated
  frontend assets; its final CI result must be checked separately.

## Changed files

Build/CI: `.github/workflows/ci.yml`, `Makefile`, `firmware/platformio.ini`,
`firmware/scripts/export_firmware.py`, `embed_ticker.py`, `test_firmwares.py`,
`.gitignore`, `firmware/web/.gitignore`, `firmware/web/package.json`.

Firmware: `firmware/src/main.cpp`, `Ticker.h`, `TickerModel.h`, `TickerJson.h`,
`Ticker.cpp`. New ticker files are compiled only for the ticker profile.

Web: `firmware/web/ticker.html`, `ticker.js`.

Tests: `firmware/tests/ticker_test.cpp`, `ticker_portal_test.cjs`,
`export_firmware_test.py`, `api_urls_test.py`.

Documentation/notices: `README.md`, `CHANGELOG.md`, `THIRD_PARTY.md`,
`docs/ticker-firmware.md`, `docs/ticker-build-validation.md`,
`docs/smalltv-mod-LICENSE.txt`, `firmware/src/fonts/OFL.txt`.
