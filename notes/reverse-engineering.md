# Reverse-engineering log

## Device

- Product UI: `Smart Weather Clock`
- Vendor/model: JUZIPi SD PRO
- Device address during analysis: `10.0.13.2`
- Vendor repository: <https://github.com/JUZIPi-tech/SD_PRO>

## Stock image

- File: `SDPro_V1.0.6_20260525_174828.bin`
- SHA-256: `058e2ebcb9a35bc6a8c4e18a5441a26a88fae74d0e7684053a3bdea496cf529b`
- Size: 488304 bytes
- ESP8266 app begins at file offset `0x1000`
- Entry point: `0x401000b8`
- IROM load address: `0x40201010`
- IROM length: `0x6d04c`

## Stock OTA

- Endpoint: `POST /update_ota`
- Multipart field: `update`
- Browser checks that filename begins with `SDP`

## Hardware test result

Custom firmware `0.2.1` confirmed ST7789-compatible display operation using
hardware SPI mode 3: MOSI GPIO13, SCLK GPIO14, DC GPIO0, reset GPIO2, no CS,
and GPIO5 active-LOW backlight.

## Security observation

Stock JavaScript requests `/config`. Its response includes the stored Wi-Fi
password without authentication. The endpoint was not queried during analysis.
