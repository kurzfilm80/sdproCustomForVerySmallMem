#ifdef SDPRO_INVESTMENT
#include "InvestmentSummary.h"

#include "ApiRefresh.h"
#include "InvestmentModel.h"
#include "InvestmentViewModel.h"
#include "WifiPower.h"
#include "fonts/InterTightBold18.h"

#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <cstring>

namespace {
constexpr uint16_t kInvestmentHttpTimeoutMs = 12000;
constexpr uint32_t kInvestmentRefreshIntervalMs = 6UL * 60UL * 60UL * 1000UL;
constexpr char kInvestmentCsvUrl[] =
    "https://docs.google.com/spreadsheets/d/e/"
    "2PACX-1vSKM5jCdWT-tHl2d17txTGX6aVYidqItObi6crX5FXJlYUkmwD0civGzWwgpgckSMRKCgmgzVO6WOoC"
    "/pub?gid=827857148&single=true&output=csv";
constexpr char kRetirementGoalsCsvUrl[] =
    "https://docs.google.com/spreadsheets/d/e/"
    "2PACX-1vRLrnu8pLXR_YMGIy_IFYYrUpCfCC2mIvK3EmNTD75C4LQqi1RbLY0kd_CAuLS6G6gYBuMBYWZY9Y8E"
    "/pub?gid=827857148&single=true&output=csv";
constexpr char kRetirementDetailsCsvUrl[] =
    "https://docs.google.com/spreadsheets/d/e/"
    "2PACX-1vRLrnu8pLXR_YMGIy_IFYYrUpCfCC2mIvK3EmNTD75C4LQqi1RbLY0kd_CAuLS6G6gYBuMBYWZY9Y8E"
    "/pub?gid=379347683&single=true&output=csv";
RetirementThemeProductMap pendingRetirementProducts;
InvestmentSummary summary{};
InvestmentRegionSummary regionSummary{};
RetirementSummary retirementSummary{};
InvestmentPageCycle pageCycle;
ApiRefresh summaryRefresh(kInvestmentRefreshIntervalMs);
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
uint8_t summaryParseFlags = 0;

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
  client.setTimeout(kInvestmentHttpTimeoutMs);
  HTTPClient http;
  http.setTimeout(kInvestmentHttpTimeoutMs);
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
    stream.setTimeout(kInvestmentHttpTimeoutMs);
    ok = parser(stream, next);
    if (!ok) Serial.println(F("Investment CSV did not match the published sheet layout"));
  }
  http.end();
  return ok;
}

constexpr int16_t kInvestmentScrollTop = 40;
constexpr int16_t kInvestmentScrollHeight = 160;
constexpr int16_t kInvestmentFooterTop = 200;
constexpr int16_t kInvestmentFooterHeight = 40;
constexpr int16_t kInvestmentFirstRowBaseline = 70;
constexpr int16_t kInvestmentTotalBaseline = 222;

InvestmentDisplayPage currentDisplayPage(uint8_t page) {
  return investmentDisplayPage(page, summary, regionSummary, retirementSummary);
}

void drawHeader(TFT_eSPI &display, uint8_t page) {
  static constexpr const char *kTitles[kInvestmentPageCount] = {
      "ASSET", "STOCK REGION", "STOCK THEME"};
  display.setFreeFont(&InterTightBold18);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setTextDatum(L_BASELINE);
  display.drawString(kTitles[page], 4, 24);
}

void drawRow(TFT_eSPI &display, const InvestmentDisplayRow &row,
             int16_t baseline, bool pinnedTotal = false) {
  const uint16_t baseColor = pinnedTotal ? TFT_YELLOW : TFT_WHITE;
  char amount[16] = "--";
  char share[8] = "--";
  char returnText[16] = "--";
  investmentFormatWholeMillions(row.holdings, amount, sizeof(amount));
  if (!investmentFormatSharePercent(row.holdings, row.shareTotal,
                                    share, sizeof(share))) {
    strlcpy(share, "--", sizeof(share));
  }
  if (row.hasProfit &&
      !investmentFormatProfit(row.profit, row.holdings,
                              returnText, sizeof(returnText))) {
    strlcpy(returnText, "--", sizeof(returnText));
  }

  const uint16_t returnColor = !row.hasProfit ? TFT_LIGHTGREY
      : row.profit > 0 ? TFT_RED
      : row.profit < 0 ? TFT_BLUE
      : pinnedTotal ? TFT_YELLOW : TFT_LIGHTGREY;
  display.setFreeFont(&InterTightBold18);
  display.setTextColor(baseColor, TFT_BLACK);
  display.setTextDatum(L_BASELINE);
  display.drawString(row.label, 4, baseline);
  display.drawString(share, 70, baseline);
  display.setTextDatum(R_BASELINE);
  display.drawString(amount, 163, baseline);
  display.setTextColor(returnColor, TFT_BLACK);
  display.drawString(returnText, 236, baseline);
}

void drawInvestmentPage(TFT_eSPI &display, uint8_t page) {
  static bool hasRenderedPage = false;
  static uint8_t lastPage = 0;
  static bool hasDrawnTotal = false;
  static bool hadData = false;
  static InvestmentDisplayRow lastDrawnTotal{};
  const bool pageChanged = !hasRenderedPage || lastPage != page;
  if (pageChanged) {
    if (!hasRenderedPage) {
      display.fillScreen(TFT_BLACK);
    } else {
      display.fillRect(0, 0, 240, kInvestmentScrollTop, TFT_BLACK);
      display.fillRect(0, kInvestmentFooterTop, 240,
                       kInvestmentFooterHeight, TFT_BLACK);
    }
    display.fillRect(0, kInvestmentScrollTop, 240,
                     kInvestmentScrollHeight, TFT_BLACK);
    drawHeader(display, page);
    hasDrawnTotal = false;
  }
  const InvestmentDisplayPage data = currentDisplayPage(page);
  const uint8_t count = data.count;
  hasRenderedPage = true;
  lastPage = page;
  if (!count) {
    hadData = false;
    display.fillRect(0, kInvestmentScrollTop, 240,
                     kInvestmentScrollHeight, TFT_BLACK);
    display.setFreeFont(&InterTightBold18);
    display.setTextDatum(C_BASELINE);
    const bool failed = page == 0 ? summaryFetchFailed
        : page == 1 ? regionFetchFailed || retirementFetchFailed
                    : summaryFetchFailed || retirementFetchFailed;
    display.setTextColor(failed ? TFT_ORANGE : TFT_LIGHTGREY, TFT_BLACK);
    display.drawString(failed ? "ERROR" : "LOADING", 120, 132);
    return;
  }

  if (!hadData) {
    display.fillRect(0, kInvestmentScrollTop, 240,
                     kInvestmentScrollHeight, TFT_BLACK);
  }
  hadData = true;
  display.setViewport(0, kInvestmentScrollTop, 240,
                      kInvestmentScrollHeight, false);
  for (uint8_t slot = 0; slot <= kInvestmentVisibleRows; ++slot) {
    const uint8_t index = pageCycle.scrollIndex + slot;
    const int16_t baseline = kInvestmentFirstRowBaseline +
        slot * kInvestmentScrollRowHeight - pageCycle.scrollOffset;
    const int16_t clearTop = baseline - 20 > kInvestmentScrollTop
        ? baseline - 20 : kInvestmentScrollTop;
    const int16_t clearBottom = baseline + 3 <
            kInvestmentScrollTop + kInvestmentScrollHeight
        ? baseline + 3 : kInvestmentScrollTop + kInvestmentScrollHeight;
    if (clearBottom > clearTop) {
      display.fillRect(0, clearTop, 240, clearBottom - clearTop, TFT_BLACK);
    }
    if (index >= count) continue;
    drawRow(display, data.rows[index], baseline);
  }

  display.resetViewport();

  const InvestmentDisplayRow &total = data.total;
  if (!hasDrawnTotal || total.holdings != lastDrawnTotal.holdings ||
      total.profit != lastDrawnTotal.profit ||
      total.shareTotal != lastDrawnTotal.shareTotal ||
      total.hasProfit != lastDrawnTotal.hasProfit) {
    display.fillRect(0, kInvestmentFooterTop, 240,
                     kInvestmentFooterHeight, TFT_BLACK);
    drawRow(display, total, kInvestmentTotalBaseline, true);
    lastDrawnTotal = total;
    hasDrawnTotal = true;
  }
}

}

bool investmentPoll(uint32_t now) {
  if (summaryRefresh.due(now)) {
    InvestmentSummary next{};
    const bool parsed = fetch(kInvestmentCsvUrl, next, summaryHttpCode,
        [](Stream &stream, InvestmentSummary &data) {
          const bool parsed = investmentParseCsv(stream, data);
          summaryParseFlags = (parsed ? 1 : 0) | (data.valid ? 2 : 0) |
                              (data.stockGoals.valid ? 4 : 0);
          return parsed;
        });
    const bool success = parsed && next.stockGoals.valid && next.regions.valid &&
        next.stockGoals.total.hasProfit &&
        next.stockGoals.total.holdings == next.regions.total.holdings &&
        next.stockGoals.total.profit == next.regions.total.profit;
    if (parsed && !next.stockGoals.valid) {
      Serial.println(F("Investment stock goals are incomplete"));
    }
    summaryRefresh.complete(millis(), success);
    summaryFetchFailed = regionFetchFailed = !success;
    regionHttpCode = summaryHttpCode;
    if (success) {
      summary = next;
      summarySuccessAt = millis();
      if (next.regions.valid) {
        regionSummary = next.regions;
        regionSuccessAt = summarySuccessAt;
        regionHttpCode = summaryHttpCode;
        regionFetchFailed = false;
      }
    }
    return true;
  }
  if (retirementRefresh.due(now)) {
    RetirementSummary next{};
    const bool goalsParsed = fetch(kRetirementGoalsCsvUrl, next, retirementHttpCode,
        [](Stream &stream, RetirementSummary &data) {
          return retirementParseGoalsCsv(stream, data, &pendingRetirementProducts);
        });
    const bool success = goalsParsed && pendingRetirementProducts.valid &&
        fetch(kRetirementDetailsCsvUrl, next, retirementHttpCode,
          [](Stream &stream, RetirementSummary &data) {
            return retirementParseDetailsCsv(stream, data, pendingRetirementProducts);
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
  const uint8_t count = currentDisplayPage(pageCycle.page).count;
  return pageCycle.advance(now, count);
}

void investmentDraw(TFT_eSPI &display) {
  drawInvestmentPage(display, pageCycle.page);
}

bool investmentReadTotal(uint8_t page, int64_t &holdings, int64_t &profit) {
  if (!summary.valid || !retirementSummary.valid) return false;
  if (page == 0) {
    holdings = int64_t(summary.assets[5].holdings) + retirementSummary.assets[6].holdings;
    profit = int64_t(summary.assets[5].profit) + retirementSummary.assets[6].profit;
  } else if (page == 1 && regionSummary.valid && retirementSummary.regionProfitValid) {
    holdings = int64_t(regionSummary.total.holdings) + retirementSummary.regionTotal.holdings;
    profit = int64_t(regionSummary.total.profit) + retirementSummary.regionTotal.profit;
  } else if (page == 2 && summary.stockGoals.valid) {
    holdings = int64_t(summary.stockGoals.total.holdings) + retirementSummary.stockGoalTotal.holdings;
    profit = int64_t(summary.stockGoals.total.profit) + retirementSummary.stockGoalTotal.profit;
  } else return false;
  return true;
}

uint8_t investmentTargetParseFlags() { return summaryParseFlags; }
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
