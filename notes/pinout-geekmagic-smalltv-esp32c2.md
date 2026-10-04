# GeekMagic SmallTV ESP32-C2 pinout

| Function | Value |
| --- | --- |
| Firmware profile | `geekmagic_smalltv_esp32c2` |
| MCU | ESP32-C2 / ESP8684 |
| Flash | 4 MB, DIO, 60 MHz |
| Display | ST7789V, 240 x 240 |
| SPI MOSI | GPIO6 |
| SPI SCLK | GPIO4 |
| Display CS | Connected to GND, panel permanently selected |
| Display DC | GPIO5 |
| Display reset | GPIO1 |
| Backlight | GPIO18, active LOW, PWM |
| SPI mode | 3 |
| Pixel order | RGB |
| Color inversion | Enabled |
| USB serial | CH340C, automatic reset |

Source: `giovi321/smalltv-mod` and its referenced community ESPHome pinout.
