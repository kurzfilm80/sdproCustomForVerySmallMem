# Changelog

## Unreleased

### Features

- Add a separate SD PRO ticker image with up to eight Yahoo Finance symbols,
  prices, daily changes, bounded sparkline charts and per-position profit/loss.
- Add a local web ticker editor with persistent settings and manual refresh;
  retain the weather/air-quality firmware and shared Wi-Fi/recovery/OTA controls.

### Refactoring

- Select weather or ticker rendering at compile time while retaining the verified
  SD PRO pin mapping and EEPROM compatibility; isolate ticker settings in LittleFS.

### Testing/tooling

- Build named weather/ticker profiles and export both to ignored `dist/` files;
  fail export when the conservative 1 MiB OTA image ceiling is exceeded.
- Restore the bundled Inter Tight OFL notice and record the WTFPL ticker source.
- Repair standalone build/check commands and CI firmware artifact generation.
- Add native ticker parsing/settings and portal regression tests, and repair
  warning errors in the existing URL-boundary test harness.

- export successful weather bootstrap builds to `../SDPRO-Weather-Bootstrap.bin`,
  including incremental builds
- remove obsolete release version and SHA-256 requirements from `bootstrap-build`
  for the manual-upload weather installer

## v0.3.0

Changes since v0.2.0.

### Features

- detect newer firmware in the device panel, browse released versions and
  install a selected SD PRO image from GitHub without a manual download
- add optional on-display update reminders with configurable duration and
  interval, while keeping every firmware installation explicitly user-started
- show firmware download progress, lock panel controls and keep a
  do-not-disconnect warning visible throughout downloads and OTA writes
- require explicit risk confirmation before installing an older firmware version
- add a version-pinned SD PRO bootstrap that provisions Wi-Fi and installs its
  SHA-256-verified firmware image directly from GitHub
- build and attach a matching bootstrap image to every firmware release

### Fixes

- recover each persisted configuration area independently when stored fields
  are invalid or use an unsupported schema, without resetting valid settings
- keep hardware-accelerated slide and bounce transitions correctly aligned
  during fragmented or low-memory rendering ([#6](https://github.com/piotrkochan/homeassistant-minidisplay/pull/6/changes))
- treat a stable release matching the base development version as current
  instead of reporting an update
- derive development firmware versions from the latest repository tag
- use the complete ESP8266 OTA slot so similarly sized near-limit firmware
  images can update each other repeatedly
- keep compact custom-font files separate from legacy VLW paths so older
  firmware safely falls back to built-in fonts after a downgrade
- pause first-time SD PRO installation through the stock web updater after
  confirming that oversized images may be accepted but written incompletely

### Performance

- reduce SD PRO firmware and font storage without sacrificing rendering quality
  or animation speed:
  - recover about 104 KiB of firmware space with tighter web and schema
    compression, RLE-compressed coverage fonts and compact bundled font metadata
  - store newly uploaded custom fonts with 7-byte glyph metadata instead of
    28-byte records while continuing to support existing font packs
  - keep one filesystem backend and replace heavyweight generic number and time
    conversion paths with bounded firmware-specific implementations

## v0.2.0

Changes since v0.1.1.

### Features

- support animated gif images
- add temporary notification overlays with titles, messages, icons, severity
  levels, configurable placement, stacking and independent timeouts
- send and dismiss notifications through Home Assistant or the local API, with
  configurable API protection
- transform numeric values
- show firmware update warning
- configure marquee step size
- replace doors with cascade
- apply transitions to all pages
- limit full page scrolling to vertical directions
- pace data with display refresh
- configure display refresh rate
- configure marquee effects and timing
- control automatic page rotation
- compress image assets
- synchronize display images
- align free layout text
- add conditional page visibility
- add chart appearance controls
- choose background chart style

### Fixes

- stage image assets transactionally and restore evicted assets when dashboard activation fails
- reclaim obsolete display images when storage is too low for a dashboard update
- compact dashboard payloads from schema
- reduce gif artifacts
- speed up image uploads and animated frame updates
- persist value transformers
- hide free layout coordinates
- stack schema on mobile
- preserve page title bounds
- smooth fast marquee intervals
- batch display value updates
- grow page json memory adaptively
- accelerate push transitions
- preserve disabled marquee settings
- compact dashboard transfers
- protect wifi memory during notifications
- size scrolling text by height
- scroll values in both layouts
- block marquee during transitions
- advance curtain edges together
- preserve rotation during navigation
- align animation tiles and timing
- preserve cropped image pixels
- refresh old and new text bounds
- preserve display spi configuration
- defer navigation rendering
- apply mapped text color to titles
- smooth marquee updates
- restore device panel reactivity
- add preview schema tabs
- remove disabled chart containers
- preserve unit spacing
- hide graph entity selection for card data

### Refactoring

- replace the legacy immediate renderer with one retained scene graph shared by
  row and free layouts, screenshots, live updates and page transitions
- compile cards into bounded render nodes with source dependency indexes, then
  redraw only merged dirty regions instead of rebuilding the complete page
- compose scenes through a reusable RGB565 tile buffer and windowed SPI writes,
  without a full framebuffer or display memory readback
- move page transitions, marquee text and animated images onto one frame
  scheduler and animation timeline
- retain both scenes during motion transitions and transfer complete cropped
  strips, keeping moving layers correctly composed
- cache the active page definition in a right-sized JSON arena and stream page
  discovery instead of repeatedly parsing the complete dashboard
- rework image rendering around sparse RLE row indexes, visible-region decoding
  and reusable decoded rows
- index bundled smooth-font glyphs in flash and render them directly, avoiding
  repeated font allocations between animation bands
- move reusable rendering buffers off the stack and keep repeated drawing paths
  bounded and allocation-free
- split the monolithic firmware entry point into focused modules for scene and
  text compilation, values, settings, fonts, startup screens and diagnostics
- centralize the dashboard schema, generate the integration copy and compact
  device payloads from schema metadata
- extract Home Assistant data pacing, value batching, numeric transforms and
  asset synchronization into dedicated components

### Diagnostics and testing

- capture ESP8266 crash details in RTC memory without storing raw stack data
- add optional cycle, frame, heap, stack and fragmentation profiling for the
  rendering and API paths
- add sanitizer-backed native coverage for scene composition, dirty regions,
  transitions, marquee, fonts, notifications and image decoding
- build the device web panel and run animated-image and notification browser
  regressions in CI
- verify that the canonical dashboard schema and generated integration schema
  remain synchronized
