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

## Investment startup investigation — 2026-11-15

Reported symptom: LOADING followed by repeated restarts. A read-only request to
`192.168.0.5/api/v1/status` failed to connect; no reset log was available, so the
physical reset cause remains unconfirmed. No firmware upload was performed.

The investment CSV row occupied 1,584 bytes on the ESP8266 continuation stack
while reading a TLS stream. Move it to a single shared static workspace for the
synchronous ESP8266 parsers; native parsers retain local rows. Disassembly of the
investment build shows parser stack frames reduced from 1,984 to 400 bytes
(general goals), 2,208 to 624 bytes (retirement goals), and 1,968 to 384 bytes
(retirement details). This leaves more of the 4 KiB stack for TLS stream reads.

The focused investment build and native/portal/layout/export regression suite
pass, including the shared workspace parser path under AddressSanitizer and
UndefinedBehaviorSanitizer. The exported image is 527,040 bytes, within the
1,044,464-byte OTA limit. Device uptime, reset reason and successful live account
fetches still require verification after an explicitly authorized upload.


## 투자 화면 스크롤 규칙 변경 — 2026-11-17

- 5줄 이상: 첫 4줄에서 1.5초 정지, 행 사이 정지 없이 목록을 2회 스크롤,
  마지막 4줄에서 1.5초 정지 후 다음 페이지로 이동한다.
- 1회 완료 후 첫 4줄로 즉시 복귀하며 반복 사이에 별도 정지하지 않는다.
- 4줄 이하: 스크롤 없이 10초 표시한다. 페이지 순서와 고정 제목·TOTAL은 유지한다.
- 기존 80ms마다 2px 이동 속도는 유지한다. 비동기 데이터 로딩으로 행 수가
  바뀌면 첫 위치부터 표시 시간을 다시 계산한다.
- 5/7/10행 연속 2회, 시작·종료 1.5초 경계, 1/4행 10초 경계,
  페이지 순환과 로딩 후 표시 시간 재설정을 네이티브 회귀 검사에 추가했다.

9번(192.168.0.5)에 최종 527,104-byte 투자 이미지를 OTA 적용했다.
SHA-256 `a8d0647df556ffce8c29b1e75b31aaeadd1e959ba18e8541bd6d1fc945b5995a`.
전체 `make test-firmwares`, 빌드·용량·이미지·export 일치 검사를 통과했다.
OTA 성공·재부팅·Wi-Fi·포털 HTTP 200 및 투자/지역/테마/퇴직연금 데이터 정상,
파싱 플래그 7을 확인했다. 표시 설정 유지. LCD 육안 확인은 수행하지 않았다.
세부 적용 결과는 `notes/device-inventory.md`의 2026-11-17 기록을 참조한다.
