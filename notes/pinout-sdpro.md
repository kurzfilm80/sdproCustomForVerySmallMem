# JUZIPi SD PRO pinout

| Function | Value |
| --- | --- |
| Firmware profile | `sdpro` |
| MCU | ESP8266 / ESP-12F |
| Flash | 4 MB, DIO, 40 MHz |
| Display | ST7789-compatible, 240 x 240 |
| SPI MOSI | GPIO13 |
| SPI SCLK | GPIO14 |
| Display CS | Not connected, panel permanently selected |
| Display DC | GPIO0 |
| Display reset | GPIO2 |
| Backlight | GPIO5, active LOW, PWM |
| SPI mode | 3 |
| Pixel order | BGR |
| Color inversion | Enabled |
| Button | GPIO4 |

Source: `JUZIPi-tech/SD_PRO` and hardware verification.
