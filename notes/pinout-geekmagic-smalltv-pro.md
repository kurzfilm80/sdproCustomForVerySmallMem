# GeekMagic SmallTV Pro pinout

| Function | Value |
| --- | --- |
| Firmware profile | `geekmagic_smalltv_pro` |
| MCU | ESP32-WROOM-32 |
| Flash | 8 MB |
| Display | ST7789V, 240 x 240 |
| SPI MOSI | GPIO23 |
| SPI SCLK | GPIO18 |
| Display CS | Connected to GND, panel permanently selected |
| Display DC | GPIO2 |
| Display reset | GPIO4 |
| Backlight | GPIO25, active LOW, PWM |
| SPI mode | 3 |
| Pixel order | RGB |
| Color inversion | Enabled |
| Capacitive touch | GPIO32 / T9 |

Source: `GeekMagicClock/smalltv-pro`, `ViToni/esphome-geekmagic-smalltv` and
`giovi321/smalltv-mod`.
