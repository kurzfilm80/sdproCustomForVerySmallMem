#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <TFT_eSPI.h>
#include <Updater.h>

namespace {

TFT_eSPI display;
ESP8266WebServer server(80);

constexpr char kApName[] = "SDPRO-Weather-Installer";
constexpr char kApPassword[] = "weather1";

bool uploadStarted = false;
bool uploadFailed = false;
size_t uploadedBytes = 0;

const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SD PRO Weather Installer</title>
<style>
body{font-family:system-ui,sans-serif;background:#101820;color:#fff;margin:0;padding:24px}
main{max-width:520px;margin:auto;background:#1c2a36;padding:24px;border-radius:16px}
input,button{width:100%;box-sizing:border-box;margin-top:14px;padding:12px;font:inherit}
button{background:#4fc3f7;border:0;border-radius:8px;font-weight:700}
.warn{color:#ffd166}
</style>
</head>
<body><main>
<h2>SD PRO Weather Installer</h2>
<p>Select the <b>firmware.bin</b> built with PlatformIO environment <b>sdpro</b>.</p>
<p class="warn">Keep power connected until the display restarts.</p>
<form method="POST" action="/update" enctype="multipart/form-data">
<input type="file" name="firmware" accept=".bin,application/octet-stream" required>
<button type="submit">Install firmware</button>
</form>
</main></body></html>
)HTML";

void draw(const String& line1, const String& line2 = "", uint16_t accent = TFT_CYAN) {
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(accent, TFT_BLACK);
  display.drawString("SD PRO", 120, 42, 4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.drawString(line1, 120, 98, 2);
  if (line2.length()) display.drawString(line2, 120, 126, 2);
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.drawString("192.168.4.1", 120, 186, 4);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadStarted = true;
    uploadFailed = false;
    uploadedBytes = 0;

    const uint32_t maxSketchSpace =
        (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

    draw("Receiving firmware...", upload.filename, TFT_GREEN);

    if (!Update.begin(maxSketchSpace, U_FLASH)) {
      uploadFailed = true;
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadFailed) {
      const size_t written = Update.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        uploadFailed = true;
        Update.printError(Serial);
      } else {
        uploadedBytes += written;
      }
    }
    yield();
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!uploadFailed) {
      if (!Update.end(true)) {
        uploadFailed = true;
        Update.printError(Serial);
      }
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    uploadFailed = true;
    Update.end(false);
  }
}

void handleUpdateDone() {
  server.sendHeader("Connection", "close");

  if (!uploadStarted || uploadFailed || Update.hasError()) {
    server.send(500, "text/plain",
                "Firmware update failed. The installer is still running; retry.");
    draw("UPDATE FAILED", "Retry at 192.168.4.1", TFT_RED);
    return;
  }

  server.send(200, "text/html",
              "<html><body><h2>Installed successfully.</h2>"
              "<p>The SD PRO will restart now.</p></body></html>");
  draw("Installed", "Restarting...", TFT_GREEN);
  delay(1500);
  ESP.restart();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  display.init();
  display.setRotation(0);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif

  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApName, kApPassword);

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", kIndexHtml);
  });
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpload);
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();

  draw("Connect Wi-Fi:", kApName);
}

void loop() {
  server.handleClient();
  yield();
}
