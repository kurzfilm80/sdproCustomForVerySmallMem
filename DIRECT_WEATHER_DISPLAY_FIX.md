# Direct Seoul Weather display fix

This version removes the runtime dependency on the original Home Assistant
dashboard renderer for the main screen.

After Wi-Fi connects it directly draws:
- SEOUL
- KST date/day/time
- TODAY / TOMORROW / DAY AFTER
- weather condition
- high / low temperature
- precipitation probability

Weather is refreshed from Open-Meteo every 15 minutes.
The clock/display is redrawn every 30 seconds.

It also includes:
- the first-time Wi-Fi setup-mode save fix
- ESP8266WiFi.h in SeoulWeather.cpp

Build:
    cd firmware
    pio run -e sdpro

Upload:
Use the currently installed firmware's web page:
Firmware OTA -> Upload firmware
and select:
    firmware/.pio/build/sdpro/firmware.bin

Bootstrap is NOT needed again.
