# Home Assistant Mini-Display

Turn a tiny Wi-Fi display into a ridiculously configurable dashboard.
**JSON Schema powered. Local. No cloud required. Home Assistant optional.**

## On a real display

Actual 240 × 240 screenshots from a JUZIPi SD PRO, not browser mockups.

| Energy and live power | Free layout and backgrounds | Home conditions |
| :---: | :---: | :---: |
| [![Energy prices, power and history](docs/screenshots/device/page1.png)](docs/screenshots/device/page1.png) | [![Battery and charging on a custom background](docs/screenshots/device/page2.png)](docs/screenshots/device/page2.png) | [![Temperature and humidity](docs/screenshots/device/page3.png)](docs/screenshots/device/page3.png) |
| **Car status** | **Notification over a live dashboard** | **Live weather and scrolling text** |
| [![Fuel and trip information](docs/screenshots/device/page4.png)](docs/screenshots/device/page4.png) | [![Notification overlay above an animated dashboard](docs/screenshots/device/notification.gif)](docs/screenshots/device/notification.gif) | [![Weather dashboard with smoothly scrolling text](docs/screenshots/device/page5.gif)](docs/screenshots/device/page5.gif) |

## Tiny screen, lots of possibilities

- **Flexible layouts:** keep things tidy with automatic rows, or place every
  element exactly where you want it on a free-form canvas.
- **Rich content:** bring together live entities, text, clocks, weather, images,
  progress bars and rings on one tiny screen.
- **Powerful charts:** turn history into line or bar charts, show them on their
  own or behind a live value, and fine-tune their scale, colors and appearance.
- **Conditional visibility:** show or hide complete pages, rows and individual
  cards using live entity values and reusable AND/OR rules.
- **Value mappings:** turn raw entity states into clear, friendly labels and
  transform or clamp numeric values before they reach the screen.
- **Color mappings:** make changing conditions easy to spot by switching text,
  chart and background colors as live values move.
- **Deep styling:** mix custom fonts, animated backgrounds, transparent cards,
  text effects and independent title and value sizing.
- **Lively scenes:** build multi-page dashboards with timed rotation, animated
  transitions and instant on-device previews while editing.
- **Useful notifications:** place temporary, severity-aware messages above any
  dashboard without interrupting its live updates or background animations.
- **Full local control:** tune brightness, refresh rate, fonts, Wi-Fi, time and
  firmware updates from the device's own web interface.

Even animated GIFs fit comfortably: the browser resizes them for the selected
display and converts them to compact RGB565 data before upload. The firmware
decodes one scanline at a time, so smooth animation does not require holding
complete frames in precious display memory.

## Device web panel

Every tab below comes from the device itself. Select any screenshot to open the
complete page at full size.

| Overview | Display |
| :---: | :---: |
| [![Device overview](docs/screenshots/web-panel/web-panel-overview.png)](docs/screenshots/web-panel/web-panel-overview-full.png) | [![Display controls](docs/screenshots/web-panel/web-panel-display.png)](docs/screenshots/web-panel/web-panel-display-full.png) |
| **Network** | **Security** |
| [![Network settings](docs/screenshots/web-panel/web-panel-network.png)](docs/screenshots/web-panel/web-panel-network-full.png) | [![Security settings](docs/screenshots/web-panel/web-panel-security.png)](docs/screenshots/web-panel/web-panel-security-full.png) |
| **Diagnostics** | **Firmware** |
| [![Device diagnostics](docs/screenshots/web-panel/web-panel-diagnostics.png)](docs/screenshots/web-panel/web-panel-diagnostics-full.png) | [![Firmware update](docs/screenshots/web-panel/web-panel-firmware.png)](docs/screenshots/web-panel/web-panel-firmware-full.png) |

## Home Assistant editor

The dedicated integration adds a visual editor in HA's sidebar. Manage multiple
displays and scenes, drag and resize content, and preview changes on the device.
HA supplies entity values, weather forecasts and Recorder history.

![Home Assistant editor with scenes, card settings and live display preview](docs/screenshots/home-assistant/home-assistant-panel.png)

Prefer another data source? Home Assistant is optional. Send layouts and values
directly through the [local API](#json-schema-powered).

## Install on JUZIPi SD PRO

> [!WARNING]
> A first-time installation on JUZIPi SD PRO requires the matching
> `SDP-HomeAssistant-MiniDisplay-Bootstrap-<VERSION>.bin` from the release.
> Do not upload the full firmware through the original updater.

Tested hardware: **JUZIPi SD PRO, ESP8266/ESP-12F, 4 MB flash, 240 × 240 ST7789**.

The full firmware is too large for the original updater, so the first install
uses a small bootstrap firmware. It downloads one specific release from GitHub, verifies
it and safely installs it. Internet access is needed only during this download.

1. Download `SDP-HomeAssistant-MiniDisplay-Bootstrap-<VERSION>.bin` from the
   matching [release](https://github.com/piotrkochan/homeassistant-minidisplay/releases).
2. Upload it unchanged through the original firmware update page.
3. Follow the instructions shown on the display.

Subsequent updates use the device's **Firmware** page.

### Add Home Assistant

[![Open your Home Assistant instance and add this repository to HACS](https://my.home-assistant.io/badges/hacs_repository.svg)](https://my.home-assistant.io/redirect/hacs_repository/?owner=piotrkochan&repository=homeassistant-minidisplay&category=integration)

1. In HACS, add `https://github.com/piotrkochan/homeassistant-minidisplay` as a
   custom repository, category **Integration**, and install it.
2. Restart HA. Add **Home Assistant Mini-Display** in **Settings → Devices & services**,
   or use the discovered device. Enter its address and panel/API credentials if enabled.
3. Open **Mini Displays** in the sidebar. Choose a display, create a scene and edit
   its pages. Preview on the device, then save.

Manual installation: copy `custom_components/mini_display` into HA's
`config/custom_components/` and restart.

## Hardware profiles

The code is prepared for several GeekMagic SmallTV variants, with separate
build profiles for their MCUs, flash layouts and known display wiring. I
currently own only a **JUZIPi SD PRO**, so that is the only device tested on
real hardware. The other profiles compile, but still need hardware validation.

**Have one of the untested models?** [Open an issue](https://github.com/piotrkochan/homeassistant-minidisplay/issues/new)
with the exact model, MCU and board photos. I can provide a matching test build
and help verify the display, buttons, Wi-Fi and recovery path.

| Model / variant | PlatformIO target | Status |
| --- | --- | --- |
| JUZIPi SD PRO | `sdpro` | ✅ Tested on hardware |
| GeekMagic SmallTV, no-CS wiring | `geekmagic_smalltv_nocs` | ⚠️ Builds, tester needed |
| GeekMagic SmallTV / Ultra, CS on GPIO15 | `geekmagic_smalltv_cs15` | ⚠️ Builds, tester needed |
| GeekMagic SmallTV ESP32-C2 / ESP8684 | `geekmagic_smalltv_esp32c2` | ⚠️ Builds, tester needed |
| GeekMagic SmallTV Pro, ESP32 / 8 MB | `geekmagic_smalltv_pro` | ⚠️ Builds, tester needed |

<details>
<summary>Build firmware locally</summary>

Requires Python 3, Node.js 24, npm and Make. From the repository root:

```bash
python3 -m venv .venv
.venv/bin/pip install platformio==6.1.19 zopfli==0.4.1
npm ci --prefix firmware/web
```

Build the tested JUZIPi SD PRO firmware:

```bash
make build
```

Output: `firmware/.pio/build/sdpro/firmware.bin`.

Or build the shared web interface and the profile matching another SmallTV:

```bash
# GeekMagic SmallTV with no display CS pin
make profile-build PROFILE=geekmagic_smalltv_nocs

# Original GeekMagic SmallTV / Ultra with display CS on GPIO15
make profile-build PROFILE=geekmagic_smalltv_cs15

# GeekMagic SmallTV with ESP32-C2 / ESP8684
make profile-build PROFILE=geekmagic_smalltv_esp32c2

# GeekMagic SmallTV Pro with classic ESP32 and 8 MB flash
make profile-build PROFILE=geekmagic_smalltv_pro
```

Outputs appear under `firmware/.pio/build/<PROFILE>/`. Run `make build-all` to
build every profile and copy the packaged binaries into `dist/`.

For HA frontend development: `npm ci --prefix integration/card`, then
`make card-check card-build`. Native firmware tests: `make test-native`
(requires a C++17 compiler). Memory report: `make elf-report` after building.

</details>

ESP32 profiles produce two different images: use `firmware.factory.bin` only
for the initial full flash over a serial connection, and use `firmware.bin`
only for an OTA update from compatible firmware. They are not interchangeable.
The SD PRO bootstrap and installation steps are exclusively for JUZIPi SD PRO;
other SmallTV models can have different MCUs, partitions and pin wiring. Check
the [hardware notes](notes/) and keep a serial recovery method ready before
testing an unverified profile.

## JSON Schema powered

The [dashboard schema](dashboard/dashboard.schema.json) describes layouts, cards,
styles, visibility and data bindings. The device also serves it at
`/schema/dashboard.schema.json`.

| Endpoint | Purpose |
| --- | --- |
| `PUT /api/v1/dashboard` | Upload a dashboard |
| `PATCH /api/v1/data` | Send values and optional chart history |
| `GET /api/v1/data` | Read retained values and history |
| `GET /api/v1/assets` | List images or download one by id |
| `PUT /api/v1/assets` | Upload an optimized image |
| `DELETE /api/v1/assets` | Delete an image |
| `POST /api/v1/notifications` | Show a temporary notification overlay |
| `DELETE /api/v1/notifications` | Dismiss all visible and queued notifications |
| `GET /api/v1/screenshot` | Capture the display as BMP |

Data keys need not be HA entity IDs. Your own application can send, for example:

```json
{"values":{"power":{"state":"156","available":true}},"render":true}
```

Bind a card to `"source": "power"`. See [the format and API notes](dashboard/README.md).
Send temporary [notification overlays](docs/notifications.md) through the local API
or Home Assistant automations, with a title, message, icon and severity.

Protected endpoints use the configured panel/API credentials. **Keep the display
on a trusted LAN; HTTPS is currently disabled in the default build.**

## Notifications

Notifications are temporary overlays displayed above the current dashboard.
They do not replace the saved scene, and entity updates and automatic page
rotation continue underneath. The queue holds three messages in total. A device
setting controls whether one, two or three are visible at once, and each visible
notification has its own timeout.

![Notification overlay above an animated dashboard](docs/screenshots/device/notification.gif)

Send a notification directly through the local API:

```bash
curl --request POST \
  --header 'Authorization: Bearer YOUR_PANEL_API_PASSWORD' \
  --header 'Content-Type: application/json' \
  --data '{"title":"Charging complete","message":"The battery is ready","icon":"check","severity":"success","durationSeconds":12}' \
  http://DISPLAY_IP/api/v1/notifications
```

`title` or `message` is required. Supported severities are `info`, `success`,
`warning`, `error` and `critical`; built-in icons include `bell`, `info`,
`check`, `warning`, `error`, `power` and `door`. Duration can be 1-300 seconds.
Omit `icon`, `severity`, `durationSeconds` or `position` to use their defaults.

Dismiss every visible and queued notification:

```bash
curl --request DELETE \
  --header 'Authorization: Bearer YOUR_PANEL_API_PASSWORD' \
  http://DISPLAY_IP/api/v1/notifications
```

HTTP Basic authentication with the configured panel/API username and password
is also supported. Notification API protection is enabled by default and is
configured independently under **Display → Notifications**. Set the panel/API
credentials under **Security**. Because the default build uses HTTP, credentials
are not encrypted in transit; keep the display on a trusted LAN.

Home Assistant automations can use the integration actions instead of calling
the endpoint directly:

```yaml
action: mini_display.notify
data:
  device_id: your_display_device_id
  title: Charging complete
  message: The battery is ready
  icon: check
  severity: success
  duration: 12
```

To dismiss all notifications from Home Assistant:

```yaml
action: mini_display.dismiss_notifications
data:
  device_id: your_display_device_id
```

These are the integration's two custom actions. Other display commands are
exposed as Home Assistant entities and use standard Home Assistant actions.

<details>
<summary>Other Home Assistant controls</summary>

| Standard action | Mini-Display entities | Purpose |
| --- | --- | --- |
| `button.press` | Next page, Previous page | Navigate without changing the automatic rotation setting |
| `button.press` | Reload dashboard | Send the active scene to the display again |
| `button.press` | Dismiss notifications | Clear visible and queued notifications |
| `button.press` | Restart | Restart the display |
| `select.select_option` | Active page | Select a page or return to automatic rotation |
| `select.select_option` | Scene | Activate a configured dashboard scene |
| `select.select_option` | Notification position | Change the default overlay position |
| `select.select_option` | Time zone | Change the display clock time zone |
| `switch.turn_on`, `switch.turn_off` | Automatic page rotation | Enable or pause timed page changes |
| `switch.turn_on`, `switch.turn_off` | Periodic data updates | Enable or pause entity-state forwarding |
| `number.set_value` | Brightness | Set display brightness as a percentage and turn the display on |
| `number.set_value` | Pixel shift | Configure periodic content movement to reduce image retention |
| `number.set_value` | Maximum refresh rate | Limit physical display refreshes |
| `light.turn_on`, `light.turn_off` | Display | Control display power and brightness |

Use the entity ID generated for your display. For example:

```yaml
action: button.press
target:
  entity_id: button.kitchen_display_next_page
```

</details>

The device web panel provides notification placement, visible-count and test
controls under **Display → Notifications**. See the
[notification reference](docs/notifications.md) for positions, limits, response
codes and rendering behavior.

GitHub CI checks firmware, native tests, the HA integration and browser interactions
on code changes. Releases require passing checks. HACS validation runs separately
for public repositories and weekly to catch upstream compatibility changes.

## Credits

Hardware research and inspiration:
[JUZIPi SD_PRO](https://github.com/JUZIPi-tech/SD_PRO),
[geekmagic-hacs](https://github.com/adrienbrault/geekmagic-hacs),
[geekmagic-tv-esp8266](https://github.com/bvweerd/geekmagic-tv-esp8266),
[smalltv-mod](https://github.com/giovi321/smalltv-mod).

## License

[MIT](LICENSE). See [third-party notices](THIRD_PARTY.md) for dependencies and assets.

Enjoy your tiny screen? You can [buy me a coffee](https://ko-fi.com/piotrkochan).
