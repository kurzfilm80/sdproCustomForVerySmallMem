# Wi-Fi setup fix

Fixes first-time Wi-Fi save failure:

    Save failed: {"error":"setup_mode"}

Build:
    cd firmware
    pio run -e sdpro

Upload:
    .pio/build/sdpro/firmware.bin

Use the existing SD PRO Seoul Weather Firmware OTA page to upload it.
No Bootstrap reinstall is needed.
