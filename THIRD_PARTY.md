# Third-party software

Our code is MIT licensed. This does not replace dependency licenses.

| Component | License | Distributed notices |
| --- | --- | --- |
| Lit, lit-html, lit-element, reactive-element | BSD-3-Clause | `custom_components/mini_display/frontend/Lit-LICENSE.txt` |
| Inter Tight and generated font assets | SIL OFL 1.1 | `firmware/src/fonts/OFL.txt` and `custom_components/mini_display/frontend/InterTight-OFL.txt` |
| ArduinoJson | MIT | Dependency source archive |
| TFT_eSPI and bundled Adafruit fonts | MIT / BSD, see individual files | Dependency source archive |
| Adafruit GFX, BusIO and Arduino_GFX | See individual component licenses | Dependency source archive |
| ESP8266 Arduino core | LGPL-2.1 and component-specific licenses | Dependency source archive |
| ESP32 Arduino core, ESP-IDF and SDK libraries | Component-specific licenses | Dependency source archive |

Firmware releases include `release-sources.tar.gz` alongside the binaries. It
contains the project sources used for the build, the actual installed PlatformIO
platforms and frameworks (including their licenses), library sources, and compiled
objects/archives. SDK packages may include vendor-provided binary components;
their own terms still apply. This is not a claim that every SDK component is MIT.

## Rebuilding and relinking

Extract the source archive and enter `project/`. Install Python 3.12, Node.js 24
and PlatformIO 6.1.19, then run:

```sh
export PLATFORMIO_CORE_DIR="$PWD/.platformio"
npm ci --prefix firmware/web
make PIO=pio build-all
```

The archive records toolchain versions in their package metadata, without bundling
compiler executables. Install those versions for your OS using PlatformIO and
remove the metadata-only tool directories before building. The firmware's
`platformio.ini` and package metadata record the resolved build dependencies.
Modify the framework/library sources in `.platformio/packages` or
`firmware/.pio/libdeps`, then rebuild the desired profile with `pio run -e sdpro`
from `firmware/`. Existing objects and static archives are also retained for
relinking. Upload the resulting image using the documented firmware update flow.
No vendor signing key is required.

Our ESP8266 HTTP parser modification transfers the request body with move
semantics rather than copying it. It is applied by
`firmware/scripts/patch_esp8266_http.py`; the archive contains the patched source.
The affected core remains under its original license. Reverse engineering for
debugging modifications to LGPL-covered components is not restricted by this
project.

When redistributing firmware, include the matching source archive and preserve
its license/copyright files. When distributing the HA integration, retain both
license files in its `frontend` directory. User-uploaded fonts and images are not
relicensed by this project.
