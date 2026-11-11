#ifdef SDPRO_INVESTMENT
#include "InvestmentSummary.h"

#include "ApiRefresh.h"
#include "InvestmentModel.h"
#include "RetirementModel.h"
#include "WifiPower.h"
#include "fonts/InterTightBold18.h"

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

enum class InvestmentRowKind : uint8_t { Section, Data, Error };

struct InvestmentDisplayRow {
  const char *label = "";
  int32_t holdings = 0;
  int32_t profit = 0;
  int32_t shareTotal = 0;
  uint8_t actualRate = 0;
  InvestmentRowKind kind = InvestmentRowKind::Data;
  bool hasProfit = false;
  bool usesActualRate = false;
};

constexpr uint8_t kInvestmentDisplayRowLimit = 22;
constexpr int16_t kInvestmentFirstRowBaseline = 54;
constexpr int16_t kInvestmentTotalBaseline = 222;

void appendSection(InvestmentDisplayRow *rows, uint8_t &count,
                   const char *label) {
  if (count < kInvestmentDisplayRowLimit) {
    rows[count].label = label;
    rows[count++].kind = InvestmentRowKind::Section;
  }
}

void appendError(InvestmentDisplayRow *rows, uint8_t &count) {
  if (count < kInvestmentDisplayRowLimit) {
    rows[count].label = "ERROR";
    rows[count++].kind = InvestmentRowKind::Error;
  }
}

void appendData(InvestmentDisplayRow *rows, uint8_t &count, const char *label,
                const InvestmentAmount &amount, int32_t shareTotal,
                bool hasProfit) {
  if (count >= kInvestmentDisplayRowLimit) return;
  rows[count] = {label, amount.holdings, amount.profit, shareTotal, 0,
                 InvestmentRowKind::Data, hasProfit, false};
  ++count;
}

void appendGoal(InvestmentDisplayRow *rows, uint8_t &count, const char *label,
                const InvestmentStockGoal &goal) {
  if (count >= kInvestmentDisplayRowLimit) return;
  rows[count] = {label, goal.holdings, 0, 0, goal.actualRate,
                 InvestmentRowKind::Data, false, true};
  ++count;
}

uint8_t collectInvestmentRows(bool retirement,
                              InvestmentDisplayRow *rows) {
  uint8_t count = 0;
  if (retirement) {
    if (!retirementSummary.valid) return 0;
    static constexpr const char *kAssetNames[kRetirementAssetCount - 1] = {
        "CASH", "BONDS", "MIXED", "MATER", "REITS", "STOCK"};
    static constexpr const char *kRegionNames[kRetirementRegionCount] = {
        "GLOBL", "US", "CHINA", "KOREA"};
    static constexpr const char *kGoalNames[kRetirementStockGoalCount] = {
        "VALUE", "GOLD", "HLTH", "MKT", "TECH"};
    const InvestmentAmount &assetTotal =
        retirementSummary.assets[kRetirementAssetCount - 1];

    appendSection(rows, count, "ASSET");
    for (uint8_t i = 0; i < kRetirementAssetCount - 1; ++i) {
      appendData(rows, count, kAssetNames[i], retirementSummary.assets[i],
                 assetTotal.holdings, true);
    }
    appendSection(rows, count, "REGION");
    for (uint8_t i = 0; i < kRetirementRegionCount; ++i) {
      appendData(rows, count, kRegionNames[i], retirementSummary.regions[i],
                 retirementSummary.regionTotal.holdings, false);
    }
    appendData(rows, count, "TOTAL", retirementSummary.regionTotal,
               retirementSummary.regionTotal.holdings, false);
    appendSection(rows, count, "GOALS");
    for (uint8_t i = 0; i < kRetirementStockGoalCount; ++i) {
      appendGoal(rows, count, kGoalNames[i], retirementSummary.stockGoals[i]);
    }
    appendGoal(rows, count, "TOTAL", retirementSummary.stockGoalTotal);
    return count;
  }

  static constexpr const char *kAssetNames[kInvestmentAssetCount - 1] = {
      "BONDS", "MATER", "CRYPT", "REITS", "STOCK"};
  static constexpr const char *kRegionNames[kInvestmentRegionCount] = {
      "GLOBL", "US", "CHINA", "KOREA"};
  static constexpr const char *kGoalNames[kInvestmentStockGoalCount] = {
      "FIN", "BIO", "SEMI", "AUTO", "CONS", "MKT", "ENRG", "TECH"};

  if (summary.valid) {
    const InvestmentAmount &assetTotal =
        summary.assets[kInvestmentAssetCount - 1];
    appendSection(rows, count, "ASSET");
    for (uint8_t i = 0; i < kInvestmentAssetCount - 1; ++i) {
      appendData(rows, count, kAssetNames[i], summary.assets[i],
                 assetTotal.holdings, true);
    }
  } else {
    appendSection(rows, count, "ASSET");
    appendError(rows, count);
  }

  if (regionSummary.valid) {
    appendSection(rows, count, "REGION");
    for (uint8_t i = 0; i < kInvestmentRegionCount; ++i) {
      appendData(rows, count, kRegionNames[i], regionSummary.regions[i],
                 regionSummary.total.holdings, true);
    }
    appendData(rows, count, "TOTAL", regionSummary.total,
               regionSummary.total.holdings, true);
  } else {
    appendSection(rows, count, "REGION");
    appendError(rows, count);
  }

  if (summary.stockGoals.valid) {
    appendSection(rows, count, "GOALS");
    for (uint8_t i = 0; i < kInvestmentStockGoalCount; ++i) {
      appendGoal(rows, count, kGoalNames[i], summary.stockGoals.categories[i]);
    }
    appendGoal(rows, count, "TOTAL", summary.stockGoals.total);
  } else {
    appendSection(rows, count, "GOALS");
    appendError(rows, count);
  }
  return count;
}

void drawHeader(TFT_eSPI &display, bool retirement) {
  display.setTextFont(1);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setTextDatum(L_BASELINE);
  display.drawString(retirement ? "RETIREMENT" : "INVESTMENT", 4, 9);
  display.setTextDatum(R_BASELINE);
  display.drawString(retirement ? "2/2" : "1/2", 236, 9);
  display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  display.setTextDatum(L_BASELINE);
  display.drawString("NAME", 4, 21);
  display.setTextDatum(R_BASELINE);
  display.drawString("SHARE", 100, 21);
  display.drawString("AMOUNT", 158, 21);
  display.drawString("RETURN", 236, 21);
}

void drawRow(TFT_eSPI &display, const InvestmentDisplayRow &row,
             int16_t baseline, bool pinnedTotal = false) {
  if (row.kind == InvestmentRowKind::Section) {
    display.setTextFont(1);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setTextDatum(L_BASELINE);
    display.drawString(row.label, 4, baseline - 8);
    return;
  }

  const uint16_t baseColor = pinnedTotal ? TFT_YELLOW
      : row.kind == InvestmentRowKind::Error ? TFT_ORANGE : TFT_WHITE;
  char amount[16] = "--";
  char share[8] = "--";
  char returnText[16] = "--";
  if (row.kind == InvestmentRowKind::Data) {
    investmentFormatWholeMillions(row.holdings, amount, sizeof(amount));
    if (row.usesActualRate) {
      snprintf(share, sizeof(share), "%u%%", unsigned(row.actualRate));
    } else if (!investmentFormatSharePercent(row.holdings, row.shareTotal,
                                             share, sizeof(share))) {
      strlcpy(share, "--", sizeof(share));
    }
    if (row.hasProfit &&
        !investmentFormatProfit(row.profit, row.holdings,
                                returnText, sizeof(returnText))) {
      strlcpy(returnText, "--", sizeof(returnText));
    }
  }

  const uint16_t returnColor = row.kind == InvestmentRowKind::Error
      ? TFT_ORANGE
      : !row.hasProfit ? TFT_LIGHTGREY
      : row.profit > 0 ? TFT_RED
      : row.profit < 0 ? TFT_BLUE
      : pinnedTotal ? TFT_YELLOW : TFT_LIGHTGREY;
  display.setFreeFont(&InterTightBold18);
  display.setTextDatum(L_BASELINE);
  display.setTextColor(baseColor, TFT_BLACK);
  display.drawString(row.label, 4, baseline);
  display.setTextDatum(R_BASELINE);
  display.drawString(share, 100, baseline);
  display.drawString(amount, 158, baseline);
  display.setTextColor(returnColor, TFT_BLACK);
  display.drawString(returnText, 236, baseline);
}

void drawInvestmentPage(TFT_eSPI &display, bool retirement) {
  display.fillScreen(TFT_BLACK);
  drawHeader(display, retirement);
  InvestmentDisplayRow rows[kInvestmentDisplayRowLimit]{};
  const uint8_t count = collectInvestmentRows(retirement, rows);
  if (!count) {
    display.setFreeFont(&InterTightBold18);
    display.setTextDatum(C_BASELINE);
    const bool failed = retirement ? retirementFetchFailed : summaryFetchFailed;
    display.setTextColor(failed ? TFT_ORANGE : TFT_LIGHTGREY, TFT_BLACK);
    display.drawString(failed ? "FETCH FAILED" : "LOADING DATA", 120, 132);
    return;
  }

  for (uint8_t slot = 0; slot < kInvestmentVisibleRows; ++slot) {
    const uint8_t index = pageCycle.scrollIndex + slot;
    if (index >= count) break;
    const int16_t baseline = kInvestmentFirstRowBaseline +
        slot * kInvestmentScrollRowHeight - pageCycle.scrollOffset;
    if (baseline < 43 || baseline > 184) continue;
    drawRow(display, rows[index], baseline);
  }

  if (retirement) {
    const InvestmentAmount &total =
        retirementSummary.assets[kRetirementAssetCount - 1];
    drawRow(display, {"TOTAL", total.holdings, total.profit, total.holdings,
                      100, InvestmentRowKind::Data, true, false},
            kInvestmentTotalBaseline, true);
  } else if (summary.valid) {
    const InvestmentAmount &total = summary.assets[kInvestmentAssetCount - 1];
    drawRow(display, {"TOTAL", total.holdings, total.profit, total.holdings,
                      100, InvestmentRowKind::Data, true, false},
            kInvestmentTotalBaseline, true);
  }
}

}

bool investmentPoll(uint32_t now) {
  if (summaryRefresh.due(now)) {
    InvestmentSummary next{};
    const bool parsed = fetch(kInvestmentCsvUrl, next, summaryHttpCode,
        [](Stream &stream, InvestmentSummary &data) {
          return investmentParseCsv(stream, data);
        });
    const bool success = parsed && next.stockGoals.valid;
    if (parsed && !next.stockGoals.valid) {
      Serial.println(F("Investment stock goals are incomplete"));
    }
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
  InvestmentDisplayRow rows[kInvestmentDisplayRowLimit]{};
  const uint8_t count = collectInvestmentRows(pageCycle.page == 1, rows);
  return pageCycle.advance(now, count);
}

void investmentDraw(TFT_eSPI &display) {
  drawInvestmentPage(display, pageCycle.page == 1);
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
