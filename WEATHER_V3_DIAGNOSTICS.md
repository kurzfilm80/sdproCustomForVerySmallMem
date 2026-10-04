# Weather v3

Changes:
- Open-Meteo HTTPS via BearSSL WiFiClientSecure with setInsecure()
- HTTP/JSON/data error diagnostics
- 30 second retry while weather is unavailable
- 15 minute refresh after success
- corrected WMO weather-code labels
- reduced JSON memory with ArduinoJson filter
- Wi-Fi setup and direct display fixes retained

Build:
  cd firmware
  pio run -e sdpro

OTA:
  Upload firmware/.pio/build/sdpro/firmware.bin
  Bootstrap is not required.
