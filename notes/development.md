# Development environment

## Setup

The repository-local Python environment lives in `.venv/` and contains
PlatformIO. It is intentionally ignored by Git.

```bash
cd /path/to/homeassistant-minidisplay
source .venv/bin/activate
make build
```

## Commands

```bash
make build
make check
make size
make clean
```

Do not add an OTA upload target until the recovery firmware has been tested and
the display pinout has been verified.

## Recovery bootstrap

First custom image deliberately does not touch display GPIOs. It starts an open
setup AP named `SDPRO-Setup-XXXXXX` when no configuration exists or station
connection fails for 20 seconds. Browse to `http://192.168.4.1/`, configure
Wi-Fi, and choose an OTA password of at least 8 characters. Subsequent firmware
uploads use `/update` with HTTP Basic username `admin`.

Built recovery image:

- `dist/SDP-Recovery-0.1.0.bin`
- SHA-256: `9f7cb29d123966df8824ecb20e7d9bd1cbdbe14044496ad2fb60293e50828daa`
- Size: approximately 310 KiB
