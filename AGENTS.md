# AGENTS.md

## Project

This directory contains reverse-engineering notes, recovery assets, and custom
firmware for the yellow JUZIPi SD PRO smart weather clock.

Target outcome: a small Home Assistant dashboard display with reliable local
operation, Wi-Fi provisioning, web OTA updates, and recovery paths.

## Confirmed target

- Product/model: JUZIPi SD PRO
- MCU: ESP8266
- Flash: 4 MB, DIO, 40 MHz
- Display: 240 x 240 color TFT
- Current device address during initial work: `10.0.13.2`
- Stock OTA endpoint: `POST /update_ota`
- Stock OTA filename must begin with `SDP`
- Vendor firmware repository: <https://github.com/JUZIPi-tech/SD_PRO>

Do not assume GeekMagic SmallTV pin mappings are identical. Related-project
pinouts may be used as hypotheses only until verified against SD PRO firmware or
hardware behavior.

## Directory layout

- `firmware/`: PlatformIO custom firmware
- `notes/`: pinout, development, and reverse-engineering records
- `stock/`: original vendor binaries, checksums, and rollback assets
- `.venv/`: local PlatformIO environment; generated and ignored by Git

## Development

Run commands from this directory:

```bash
source .venv/bin/activate
make build
make check
make size
```

Use PlatformIO with the pinned `espressif8266` platform. Keep generated files
out of Git.

## Firmware size guardrails

The SD PRO OTA slot is small. Preserve the existing size work and check
`make size` after changes that can affect linked code or embedded assets.

- Do not use `strftime` for the fixed clock and status formats. Use the bounded
  helpers in `TimeFormat.h`; `strftime` pulls several KiB of locale code and
  static RAM into the ESP8266 image.
- Do not use `strtof`, floating-point `scanf`, or generic floating-point
  formatting on ESP8266. Use `DecimalParser` for input and the existing bounded
  number formatting paths. Keep `_printf_float` and `_scanf_float` out of the
  linker flags.
- Use LittleFS only. Do not add SPIFFS calls or restore TFT_eSPI's SPIFFS
  defaults, because that links a second filesystem implementation.
- Keep unused TFT_eSPI built-in fonts disabled. The startup screens reuse the
  project fonts; do not restore `LOAD_FONT2` or `LOAD_FONT4` without measuring
  the cost and proving they are required.
- Preserve the 16-bit `GFXglyph` bitmap offsets applied by
  `scripts/patch_tft_espi.py`. Every bundled GFX bitmap must remain below
  64 KiB; the pre-build check intentionally fails if that assumption changes.
- Preserve compact smooth-font VLW version 12. Its glyph metadata is 7 bytes
  instead of the standard 28 bytes and is used by embedded smooth fonts and
  newly uploaded custom fonts. Continue accepting legacy version 11 uploads.
  Store version 12 user fonts only under the distinct `font-v12-*` paths;
  older firmware assumes every `font1-*` and `font2-*` file is version 11.
- Keep static smooth-font offset indexes 16-bit. Regeneration must fail if an
  indexed font grows beyond the 64 KiB addressable range.
- Preserve RLE encoding for the large built-in coverage fonts. Rendering must
  stay streaming and allocation-free; do not expand a complete glyph or font
  into RAM.
- Build the web UI with the existing Terser settings and embed the web/schema
  gzip streams with Zopfli. Do not commit or embed unminified assets.
- Do not add decompression or allocation to repeated drawing and animation
  paths merely to reduce flash. Any further bitmap compression needs native
  equivalence tests and physical-device timing before adoption.

Previously tested compiler shortcuts are not useful here: full LTO is
incompatible with the pinned prebuilt ESP8266 libraries, tighter function
alignment increased the image, `-fno-threadsafe-statics` saved almost nothing
while weakening initialization, and JavaScript private-property mangling is
unsafe for Lit reactivity.

## Changelog

- Keep `CHANGELOG.md` complete for every material change since the most recent
  released tag. Use `Unreleased` while the target version is unknown, then
  rename that section when preparing a release and state the previous version
  used as its baseline.
- Before finalizing a release section, audit both `git log <previous-tag>..HEAD`
  and the corresponding diff. Do not rely only on commit subjects: one commit
  may contain several important changes or hide a large refactor behind a
  `fix:` prefix.
- Order sections with `Features` first, followed by `Fixes`, `Refactoring`, and
  any relevant diagnostics or testing/tooling section.
- Describe complete user-facing capabilities at a useful level. Combine a set
  of related commits into one primary feature entry with concise supporting
  details instead of listing every implementation step as a separate feature.
- Give substantial architectural work its own explicit `Refactoring` entries.
  Name the affected subsystem and summarize the new design and practical
  impact; never bury a renderer, storage, protocol, or data-flow rewrite among
  small fixes.
- Include important reliability, performance, memory, migration, diagnostics,
  and build/test changes even when they are not directly visible in the UI.
- Avoid duplicates between sections, but do not remove meaningful coverage when
  consolidating entries. Every material change in the release range must remain
  represented somewhere.
- Use concise present-tense fragments without Conventional Commit prefixes.
  Prefer product language such as "notifications" or "scene renderer" over
  internal function and class names unless those names help users understand
  compatibility or migration impact.

## Firmware requirements

Before first custom OTA image, firmware must provide:

1. Wi-Fi provisioning without hard-coded credentials.
2. Failsafe access point after repeated connection failures.
3. Authenticated or otherwise LAN-restricted web OTA.
4. Visible boot/recovery status when display pins are confirmed.
5. A build small enough for the selected 4 MB OTA partition layout.
6. Watchdog-safe networking and display loops.
7. No cloud dependency for normal operation.

Home Assistant data should use a local API, preferably MQTT or constrained REST
polling. Never embed a long-lived Home Assistant administrator token in source
control. Store device secrets outside tracked files.

## Persistent configuration compatibility

- Give every independently stored JSON document its own positive integer
  `schemaVersion`. A missing version means schema 1 for files created before
  versioning was introduced. The dashboard keeps its existing `version` field.
- Keep additive changes within the current schema and ignore unknown fields.
  Increment the schema before changing the type, meaning or required structure
  of an existing field.
- Add explicit step-by-step migrations before increasing a firmware's supported
  schema version. Never guess how to interpret a newer schema.
- Parse into defaults, validate every known field and only then apply settings.
  Repair invalid fields independently and rewrite the normalized document.
- Quarantine a malformed or unsupported document and replace only that
  configuration area with defaults. Never turn a single-file failure into a
  full LittleFS or device reset.
- Treat `DeviceConfig` as a fixed EEPROM compatibility boundary. Do not resize,
  reorder or repurpose its fields; store future settings in versioned LittleFS
  documents.
- Log recovery reasons without logging Wi-Fi passwords, API credentials or
  other stored secrets.

## Safety rules

- Preserve stock firmware and its SHA-256 before custom flashing.
- Never flash an unverified image merely because another product uses ESP8266
  and ST7789.
- Never erase flash through OTA.
- Do not query or log `/config` from stock firmware because it exposes the saved
  Wi-Fi password without authentication.
- Do not print Wi-Fi passwords, Home Assistant tokens, or OTA credentials.
- Do not enable display GPIOs until pin assignments and ESP8266 boot-strap
  constraints have been checked.
- First custom image must prioritize recovery over dashboard features.
- After each pinout discovery, update `notes/pinout.md` with evidence and
  confidence level.
- After each stock-binary finding, update `notes/reverse-engineering.md`.

## Flash workflow

1. Build and inspect image metadata and size.
2. Verify ESP8266 target, 4 MB flash mode, and OTA-compatible layout.
3. Confirm recovery AP and OTA behavior in code review.
4. Obtain explicit user confirmation immediately before replacing firmware.
5. Upload through stock `/update_ota` using a filename beginning with `SDP`.
6. Verify reboot, Wi-Fi recovery, HTTP OTA, and free heap before display work.
7. Keep UART recovery instructions ready if OTA becomes unavailable.

## Code style

- Prefer small modules with explicit ownership and bounded memory use.
- Avoid dynamic allocation in repeated rendering paths.
- Keep HTTP responses and JSON documents bounded.
- Yield during long ESP8266 operations.
- Comments explain hardware constraints and recovery rationale, not syntax.
- Treat compiler warnings as defects.
