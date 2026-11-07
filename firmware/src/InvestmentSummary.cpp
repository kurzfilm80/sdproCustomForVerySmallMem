#ifdef SDPRO_INVESTMENT
#include "InvestmentSummary.h"

#include "ApiRefresh.h"
#include "InvestmentModel.h"
#include "RetirementModel.h"
#include "WifiPower.h"
#include "fonts/InterTightBold18.h"
#include "fonts/InterTightBold24.h"
#include "fonts/InterTightCompact13.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <cstring>

namespace {
constexpr uint32_t kInvestmentRefreshIntervalMs = 6UL * 60UL * 60UL * 1000UL;
constexpr char kInvestmentCsvUrl[] =
    "https://docs.google.com/spreadsheets/d/e/"
    "2PACX-1vSKM5jCdWT-tHl2d17txTGX6aVYidqItObi6crX5FXJlYUkmwD0civGzWwgpgckSMRKCgmgzVO6WOoC"
    "/pub?gid=827857148&single=true&output=csv";
constexpr char kInvestmentRegionsCsvUrl[] =
    "https://docs.google.com/spreadsheets/d/e/"
    "2PACX-1vSKM5jCdWT-tHl2d17txTGX6aVYidqItObi6crX5FXJlYUkmwD0civGzWwgpgckSMRKCgmgzVO6WOoC"
    "/pub?gid=379347683&single=true&output=csv";
constexpr char kRetirementGoalsCsvUrl[] =
    "https://docs.google.com/spreadsheets/d/e/"
    "2PACX-1vRLrnu8pLXR_YMGIy_IFYYrUpCfCC2mIvK3EmNTD75C4LQqi1RbLY0kd_CAuLS6G6gYBuMBYWZY9Y8E"
    "/pub?gid=827857148&single=true&output=csv";
InvestmentSummary summary{};
InvestmentRegionSummary regionSummary{};
RetirementSummary retirementSummary{};
InvestmentPageCycle pageCycle;
ApiRefresh summaryRefresh(kInvestmentRefreshIntervalMs);
ApiRefresh regionRefresh(kInvestmentRefreshIntervalMs);
ApiRefresh retirementRefresh(kInvestmentRefreshIntervalMs);
int summaryHttpCode = 0;
int regionHttpCode = 0;
int retirementHttpCode = 0;
bool summaryFetchFailed = false;
bool regionFetchFailed = false;
bool retirementFetchFailed = false;
uint32_t summarySuccessAt = 0;
uint32_t regionSuccessAt = 0;
uint32_t retirementSuccessAt = 0;

template <typename Data, typename Parser>
bool fetch(const char *url, Data &next, int &httpCode, Parser parser) {
  // BearSSL's full 16 KiB receive buffer and protocol overhead need about 16.7 KiB.
  if (ESP.getFreeHeap() < 30000 || ESP.getMaxFreeBlockSize() < 17000) {
    httpCode = -100;
    Serial.println(F("Investment fetch skipped: low heap"));
    return false;
  }

  ActiveWifiTransfer activeTransfer;
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(6000);
  HTTPClient http;
  http.setTimeout(6000);
  http.setReuse(false);
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setRedirectLimit(3);
  if (!http.begin(client, url)) {
    httpCode = -101;
    Serial.println(F("Investment HTTP begin failed"));
    return false;
  }

  http.addHeader("User-Agent", "SDPRO-Investment");
  httpCode = http.GET();
  bool ok = false;
  if (httpCode == HTTP_CODE_OK) {
    Stream &stream = http.getStream();
    stream.setTimeout(6000);
    ok = parser(stream, next);
    if (!ok) Serial.println(F("Investment CSV did not match the published sheet layout"));
  }
  http.end();
  return ok;
}

void drawGrid(TFT_eSPI &display, uint8_t rows) {
  display.drawRect(0, 0, 240, 240, TFT_WHITE);
  display.drawFastVLine(80, 0, 240, TFT_WHITE);
  display.drawFastVLine(160, 0, 240, TFT_WHITE);
  for (uint8_t row = 1; row < rows; ++row) {
    display.drawFastHLine(0, (row * 240) / rows, 240, TFT_WHITE);
  }
}

void drawLabel(TFT_eSPI &display, const char *text, int16_t x,
               int16_t baseline, uint16_t color) {
  display.setTextColor(color, TFT_BLACK);
  display.setTextDatum(L_BASELINE);
  display.setFreeFont(&InterTightCompact13);
  display.drawString(text, x, baseline);
}

void drawLargeValue(TFT_eSPI &display, const char *text, int16_t right,
                    int16_t baseline, uint16_t color) {
  display.setTextColor(color, TFT_BLACK);
  display.setTextDatum(R_BASELINE);
  display.setFreeFont(&InterTightBold24);
  display.drawString(text, right, baseline);
}

void drawMediumValue(TFT_eSPI &display, const char *text, int16_t right,
                     int16_t baseline, uint16_t color) {
  display.setTextColor(color, TFT_BLACK);
  display.setTextDatum(R_BASELINE);
  display.setFreeFont(&InterTightBold18);
  display.drawString(text, right, baseline);
}

void drawSmallProfit(TFT_eSPI &display, int32_t profit, int32_t holdings,
                     int16_t baseline, uint16_t color) {
  char text[16];
  if (!investmentFormatProfit(profit, holdings, text, sizeof(text))) {
    strlcpy(text, "--", sizeof(text));
  }
  drawLabel(display, text, 4, baseline, color);
}

void drawAssetPage(TFT_eSPI &display) {
  static constexpr const char *kAssetNames[kInvestmentAssetCount] = {
      "BONDS", "MATERIAL", "CRYPTO", "REITS", "STOCKS", "TOTAL"};
  const int32_t total = summary.assets[kInvestmentAssetCount - 1].holdings;
  drawGrid(display, kInvestmentAssetCount);
  for (uint8_t i = 0; i < kInvestmentAssetCount; ++i) {
    const InvestmentAmount &amount = summary.assets[i];
    const bool totalRow = i == kInvestmentAssetCount - 1;
    const int16_t top = i * 40;
    const uint16_t labelColor = totalRow ? TFT_YELLOW : TFT_WHITE;
    const uint16_t profitColor = totalRow ? TFT_YELLOW : amount.profit > 0
        ? TFT_RED : amount.profit < 0 ? TFT_BLUE : TFT_LIGHTGREY;
    char holding[16];
    char share[8];
    investmentFormatWholeMillions(amount.holdings, holding, sizeof(holding));
    if (!investmentFormatSharePercent(amount.holdings, total, share, sizeof(share))) {
      strlcpy(share, "--", sizeof(share));
    }

    drawLabel(display, kAssetNames[i], 4, top + 15, labelColor);
    drawSmallProfit(display, amount.profit, amount.holdings, top + 30, profitColor);
    drawLargeValue(display, holding, 156, top + 30, labelColor);
    drawLargeValue(display, share, 236, top + 30, labelColor);
  }
}

void drawRegionsPage(TFT_eSPI &display) {
  static constexpr const char *kRegionNames[kInvestmentRegionCount + 1] = {
      "GLOBAL", "U.S.", "CHINA", "KOREA", "TOTAL"};
  drawGrid(display, kInvestmentRegionCount + 1);
  for (uint8_t i = 0; i <= kInvestmentRegionCount; ++i) {
    const bool totalRow = i == kInvestmentRegionCount;
    const InvestmentAmount &amount = totalRow
        ? regionSummary.total : regionSummary.regions[i];
    const int16_t top = i * 48;
    const uint16_t labelColor = totalRow ? TFT_YELLOW : TFT_WHITE;
    const uint16_t profitColor = totalRow ? TFT_YELLOW : amount.profit > 0
        ? TFT_RED : amount.profit < 0 ? TFT_BLUE : TFT_LIGHTGREY;
    char holding[16];
    char share[8];
    investmentFormatWholeMillions(amount.holdings, holding, sizeof(holding));
    if (!investmentFormatSharePercent(amount.holdings,
            regionSummary.total.holdings, share, sizeof(share))) {
      strlcpy(share, "--", sizeof(share));
    }

    drawLabel(display, kRegionNames[i], 4, top + 17, labelColor);
    drawSmallProfit(display, amount.profit, amount.holdings, top + 34, profitColor);
    drawLargeValue(display, holding, 156, top + 35, labelColor);
    drawLargeValue(display, share, 236, top + 35, labelColor);
  }
}

void drawStockGoalsPage(TFT_eSPI &display) {
  static constexpr const char *kStockGoalNames[kInvestmentStockGoalCount + 1] = {
      "FIN/VALUE", "BIO/HLTH", "SEMIS", "IND/AUTO", "CONSUMER",
      "MARKET", "ENERGY", "AI/TECH", "TOTAL"};
  const InvestmentStockGoals &goals = summary.stockGoals;
  drawGrid(display, kInvestmentStockGoalCount + 1);
  for (uint8_t i = 0; i <= kInvestmentStockGoalCount; ++i) {
    const bool totalRow = i == kInvestmentStockGoalCount;
    const InvestmentStockGoal &goal = totalRow
        ? goals.total : goals.categories[i];
    const int16_t top = (i * 240) / (kInvestmentStockGoalCount + 1);
    const int16_t baseline = top + 19;
    const uint16_t color = totalRow ? TFT_YELLOW : TFT_WHITE;
    char holding[16];
    char actualRate[8];
    investmentFormatWholeMillions(goal.holdings, holding, sizeof(holding));
    snprintf(actualRate, sizeof(actualRate), "%u%%",
        static_cast<unsigned>(goal.actualRate));

    drawLabel(display, kStockGoalNames[i], 4, top + 15, color);
    drawLargeValue(display, holding, 156, baseline, color);
    drawLargeValue(display, actualRate, 236, baseline, color);
  }
}

void drawRetirementAssetsPage(TFT_eSPI &display) {
  static constexpr const char *kAssetNames[kRetirementAssetCount] = {
      "CASH", "BONDS", "MIXED", "MATERIAL", "REITS", "STOCKS", "TOTAL"};
  const int32_t total =
      retirementSummary.assets[kRetirementAssetCount - 1].holdings;
  drawGrid(display, kRetirementAssetCount);
  for (uint8_t i = 0; i < kRetirementAssetCount; ++i) {
    const InvestmentAmount &amount = retirementSummary.assets[i];
    const bool totalRow = i == kRetirementAssetCount - 1;
    const int16_t top = (i * 240) / kRetirementAssetCount;
    const uint16_t labelColor = totalRow ? TFT_YELLOW : TFT_WHITE;
    const uint16_t profitColor = totalRow ? TFT_YELLOW : amount.profit > 0
        ? TFT_RED : amount.profit < 0 ? TFT_BLUE : TFT_LIGHTGREY;
    char holding[16];
    char share[8];
    investmentFormatWholeMillions(amount.holdings, holding, sizeof(holding));
    if (!investmentFormatSharePercent(amount.holdings, total, share, sizeof(share))) {
      strlcpy(share, "--", sizeof(share));
    }

    drawLabel(display, kAssetNames[i], 4, top + 13, labelColor);
    drawSmallProfit(display, amount.profit, amount.holdings, top + 27, profitColor);
    drawMediumValue(display, holding, 156, top + 28, labelColor);
    drawMediumValue(display, share, 236, top + 28, labelColor);
  }
}

void drawRetirementRegionsPage(TFT_eSPI &display) {
  static constexpr const char *kRegionNames[kRetirementRegionCount + 1] = {
      "GLOBAL", "U.S.", "CHINA", "KOREA", "TOTAL"};
  drawGrid(display, kRetirementRegionCount + 1);
  for (uint8_t i = 0; i <= kRetirementRegionCount; ++i) {
    const bool totalRow = i == kRetirementRegionCount;
    const InvestmentAmount &amount = totalRow
        ? retirementSummary.regionTotal : retirementSummary.regions[i];
    const int16_t top = (i * 240) / (kRetirementRegionCount + 1);
    const uint16_t color = totalRow ? TFT_YELLOW : TFT_WHITE;
    char holding[16];
    char share[8];
    investmentFormatWholeMillions(amount.holdings, holding, sizeof(holding));
    if (!investmentFormatSharePercent(amount.holdings,
            retirementSummary.regionTotal.holdings, share, sizeof(share))) {
      strlcpy(share, "--", sizeof(share));
    }

    drawLabel(display, kRegionNames[i], 4, top + 17, color);
    drawLargeValue(display, holding, 156, top + 35, color);
    drawLargeValue(display, share, 236, top + 35, color);
  }
}

void drawRetirementStockGoalsPage(TFT_eSPI &display) {
  static constexpr const char *kStockGoalNames[kRetirementStockGoalCount + 1] = {
      "VALUE", "GOLD", "HEALTH", "MARKET", "AI/TECH", "TOTAL"};
  const uint8_t rows = kRetirementStockGoalCount + 1;
  drawGrid(display, rows);
  for (uint8_t i = 0; i < rows; ++i) {
    const bool totalRow = i == kRetirementStockGoalCount;
    const InvestmentStockGoal &goal = totalRow
        ? retirementSummary.stockGoalTotal : retirementSummary.stockGoals[i];
    const int16_t top = (i * 240) / rows;
    const uint16_t color = totalRow ? TFT_YELLOW : TFT_WHITE;
    char holding[16];
    char actualRate[8];
    investmentFormatWholeMillions(goal.holdings, holding, sizeof(holding));
    snprintf(actualRate, sizeof(actualRate), "%u%%",
        static_cast<unsigned>(goal.actualRate));

    drawLabel(display, kStockGoalNames[i], 4, top + 15, color);
    drawLargeValue(display, holding, 156, top + 30, color);
    drawLargeValue(display, actualRate, 236, top + 30, color);
  }
}
}

bool investmentPoll(uint32_t now) {
  if (summaryRefresh.due(now)) {
    InvestmentSummary next{};
    const bool success = fetch(kInvestmentCsvUrl, next, summaryHttpCode,
        [](Stream &stream, InvestmentSummary &data) {
          return investmentParseCsv(stream, data);
        });
    summaryRefresh.complete(millis(), success);
    summaryFetchFailed = !success;
    if (success) {
      summary = next;
      summarySuccessAt = millis();
    }
    return true;
  }
  if (regionRefresh.due(now)) {
    InvestmentRegionSummary next{};
    const bool success = fetch(kInvestmentRegionsCsvUrl, next, regionHttpCode,
        [](Stream &stream, InvestmentRegionSummary &data) {
          return investmentParseRegionsCsv(stream, data);
        });
    regionRefresh.complete(millis(), success);
    regionFetchFailed = !success;
    if (success) {
      regionSummary = next;
      regionSuccessAt = millis();
    }
    return true;
  }
  if (retirementRefresh.due(now)) {
    RetirementSummary next{};
    const bool success = fetch(kRetirementGoalsCsvUrl, next, retirementHttpCode,
        [](Stream &stream, RetirementSummary &data) {
          return retirementParseGoalsCsv(stream, data);
        });
    retirementRefresh.complete(millis(), success);
    retirementFetchFailed = !success;
    if (success) {
      retirementSummary = next;
      retirementSuccessAt = millis();
    }
    return true;
  }
  return false;
}

bool investmentAdvance(uint32_t now) {
  return pageCycle.advance(now);
}

void investmentDraw(TFT_eSPI &display) {
  display.fillScreen(TFT_BLACK);
  const bool retirementPage = pageCycle.page >= 3;
  const bool valid = pageCycle.page == 1 ? regionSummary.valid
      : pageCycle.page == 2 ? summary.stockGoals.valid
      : retirementPage ? retirementSummary.valid : summary.valid;
  const bool failed = pageCycle.page == 1 ? regionFetchFailed
      : pageCycle.page == 2 ? summaryFetchFailed || summary.valid
      : retirementPage ? retirementFetchFailed : summaryFetchFailed;
  if (!valid) {
    display.setTextColor(failed ? TFT_ORANGE : TFT_LIGHTGREY, TFT_BLACK);
    display.setFreeFont(&InterTightBold18);
    display.setTextDatum(C_BASELINE);
    display.drawString(failed ? "FETCH FAILED" : "LOADING DATA", 120, 137);
  } else if (pageCycle.page == 1) {
    drawRegionsPage(display);
  } else if (pageCycle.page == 2) {
    drawStockGoalsPage(display);
  } else if (pageCycle.page == 3) {
    drawRetirementAssetsPage(display);
  } else if (pageCycle.page == 4) {
    drawRetirementRegionsPage(display);
  } else if (pageCycle.page == 5) {
    drawRetirementStockGoalsPage(display);
  } else {
    drawAssetPage(display);
  }
}

bool investmentTargetValid() { return summary.valid; }
bool investmentTargetError() { return summaryFetchFailed; }
int investmentTargetHttpCode() { return summaryHttpCode; }
uint32_t investmentTargetAgeSeconds() {
  return summary.valid ? (millis() - summarySuccessAt) / 1000 : 0;
}
bool investmentStockGoalsValid() { return summary.stockGoals.valid; }
bool investmentStockGoalsError() {
  return summaryFetchFailed || (summary.valid && !summary.stockGoals.valid);
}
bool investmentRetirementValid() { return retirementSummary.valid; }
bool investmentRetirementError() { return retirementFetchFailed; }
int investmentRetirementHttpCode() { return retirementHttpCode; }
uint32_t investmentRetirementAgeSeconds() {
  return retirementSummary.valid ? (millis() - retirementSuccessAt) / 1000 : 0;
}
bool investmentRegionsValid() { return regionSummary.valid; }
bool investmentRegionsError() { return regionFetchFailed; }
int investmentRegionsHttpCode() { return regionHttpCode; }
uint32_t investmentRegionsAgeSeconds() {
  return regionSummary.valid ? (millis() - regionSuccessAt) / 1000 : 0;
}
#endif
