#pragma once

#include "InvestmentModel.h"
#include "RetirementModel.h"

// Combined account amounts can exceed the signed 32-bit CSV field boundary.
struct InvestmentDisplayRow {
  const char *label = "";
  int64_t holdings = 0;
  int64_t profit = 0;
  int64_t shareTotal = 0;
  bool hasProfit = false;
};

constexpr uint8_t kInvestmentDisplayRowLimit = 10;

struct InvestmentDisplayPage {
  InvestmentDisplayRow rows[kInvestmentDisplayRowLimit]{};
  InvestmentDisplayRow total{};
  uint8_t count = 0;
};

inline InvestmentDisplayPage investmentDisplayPage(
    uint8_t page, const InvestmentSummary &investment,
    const InvestmentRegionSummary &regions, const RetirementSummary &retirement) {
  InvestmentDisplayPage result{};
  if (page == 0) {
    if (!investment.valid || !retirement.valid) return result;
    static constexpr const char *kNames[] = {
        "CASH", "BONDS", "MIXED", "MATER", "CRYPT", "REITS", "STOCK"};
    static constexpr int8_t kInvestmentMap[] = {-1, 0, -1, 1, 2, 3, 4};
    static constexpr int8_t kRetirementMap[] = {0, 1, 2, 3, -1, 4, 5};
    const auto &investmentTotal = investment.assets[kInvestmentAssetCount - 1];
    const auto &retirementTotal = retirement.assets[kRetirementAssetCount - 1];
    const int64_t total = int64_t(investmentTotal.holdings) + retirementTotal.holdings;
    for (uint8_t i = 0; i < 7; ++i) {
      InvestmentDisplayRow &row = result.rows[result.count++];
      row = {kNames[i], 0, 0, total, true};
      if (kInvestmentMap[i] >= 0) {
        const auto &amount = investment.assets[kInvestmentMap[i]];
        row.holdings += amount.holdings;
        row.profit += amount.profit;
      }
      if (kRetirementMap[i] >= 0) {
        const auto &amount = retirement.assets[kRetirementMap[i]];
        row.holdings += amount.holdings;
        row.profit += amount.profit;
      }
    }
    result.total = {"TOTAL", total,
        int64_t(investmentTotal.profit) + retirementTotal.profit, total, true};
  } else if (page == 1) {
    // Wait for both sources instead of presenting a partial account as a total.
    if (!regions.valid || !retirement.valid) return result;
    static constexpr const char *kNames[kInvestmentRegionCount] = {
        "GLOBL", "US", "CHINA", "KOREA"};
    const int64_t total = int64_t(regions.total.holdings) +
                          retirement.regionTotal.holdings;
    for (uint8_t i = 0; i < kInvestmentRegionCount; ++i) {
      result.rows[result.count++] = {
          kNames[i], int64_t(regions.regions[i].holdings) +
                         retirement.regions[i].holdings,
          int64_t(regions.regions[i].profit) + retirement.regions[i].profit,
          total, retirement.regionProfitValid};
    }
    result.total = {"TOTAL", total,
        int64_t(regions.total.profit) + retirement.regionTotal.profit,
        total, retirement.regionProfitValid};
  } else if (page == 2) {
    if (!investment.stockGoals.valid || !retirement.valid) return result;
    static constexpr const char *kNames[kInvestmentDisplayRowLimit] = {
        "FIN", "HLTH", "SEMI", "AUTO", "CONS", "MKT", "ENRG", "TECH",
        "VALUE", "GOLD"};
    // Merge biotech/healthcare, market diversification and AI/technology.
    // Finance/value and value/dividend are distinct published categories.
    static constexpr uint8_t kRetirementThemeMap[kRetirementStockGoalCount] = {
        8, 9, 1, 5, 7};
    const int64_t total = int64_t(investment.stockGoals.total.holdings) +
                          retirement.stockGoalTotal.holdings;
    result.count = kInvestmentDisplayRowLimit;
    for (uint8_t i = 0; i < result.count; ++i) {
      result.rows[i] = {kNames[i], 0, 0, total, false};
      if (i < kInvestmentStockGoalCount) {
        const auto &goal = investment.stockGoals.categories[i];
        result.rows[i].holdings = goal.holdings;
        result.rows[i].profit = goal.profit;
        result.rows[i].hasProfit = goal.hasProfit;
      }
    }
    for (uint8_t i = 0; i < kRetirementStockGoalCount; ++i) {
      auto &row = result.rows[kRetirementThemeMap[i]];
      const auto &goal = retirement.stockGoals[i];
      if (kRetirementThemeMap[i] >= kInvestmentStockGoalCount) {
        row.hasProfit = goal.hasProfit;
      } else {
        row.hasProfit = row.hasProfit && goal.hasProfit;
      }
      row.holdings += goal.holdings;
      row.profit += goal.profit;
    }
    result.total = {"TOTAL", total,
        int64_t(investment.stockGoals.total.profit) + retirement.stockGoalTotal.profit,
        total, investment.stockGoals.total.hasProfit && retirement.stockGoalTotal.hasProfit};
  }
  return result;
}
