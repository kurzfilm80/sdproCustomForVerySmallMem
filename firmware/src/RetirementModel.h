#pragma once

#include "InvestmentModel.h"

constexpr uint8_t kRetirementAssetCount = 7;
constexpr uint8_t kRetirementRegionCount = 4;
constexpr uint8_t kRetirementStockGoalCount = 5;

struct RetirementSummary {
  InvestmentAmount assets[kRetirementAssetCount]{};
  InvestmentAmount regions[kRetirementRegionCount]{};
  InvestmentAmount regionTotal{};
  InvestmentStockGoal stockGoals[kRetirementStockGoalCount]{};
  InvestmentStockGoal stockGoalTotal{};
  bool valid = false;
};

struct RetirementColumns {
  int8_t category = -1;
  int8_t holdings = -1;
  int8_t actualRate = -1;
};

inline bool retirementFindColumns(const InvestmentCsvRow &row,
                                  RetirementColumns &columns,
                                  const char *categoryHeader) {
  columns = RetirementColumns{};
  for (uint8_t i = 0; i < row.count; ++i) {
    if (row.truncated[i]) continue;
    const char *field = row.fields[i];
    if (columns.category < 0 &&
        (investmentHeaderEquals(field, categoryHeader) ||
         investmentHeaderEquals(field, "category"))) {
      columns.category = i;
    }
    if (columns.holdings < 0 &&
        investmentHeaderColumn(field, "보유금액의 SUM", "holdings")) {
      columns.holdings = i;
    }
    if (columns.actualRate < 0 &&
        investmentHeaderColumn(field, "실투율", "actual rate")) {
      columns.actualRate = i;
    }
  }
  return columns.category >= 0 && columns.holdings >= 0;
}

inline int8_t retirementRegionIndex(const char *category) {
  static constexpr const char *kKoreanNames[kRetirementRegionCount] = {
      "글로벌 총계", "미국 총계", "중국 총계", "한국 총계"};
  static constexpr const char *kEnglishNames[kRetirementRegionCount] = {
      "global total", "us total", "china total", "korea total"};
  for (uint8_t i = 0; i < kRetirementRegionCount; ++i) {
    if (std::strcmp(category, kKoreanNames[i]) == 0 ||
        investmentHeaderEquals(category, kEnglishNames[i])) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

inline int8_t retirementStockGoalIndex(const char *category) {
  static constexpr const char *kKoreanNames[kRetirementStockGoalCount] = {
      "가치/배당", "금/원자재", "바이오/헬스케어", "시장분산", "AI/기술"};
  static constexpr const char *kEnglishNames[kRetirementStockGoalCount] = {
      "value/dividend", "gold/commodities", "biotech/healthcare",
      "market diversification", "AI/technology"};
  for (uint8_t i = 0; i < kRetirementStockGoalCount; ++i) {
    if (std::strcmp(category, kKoreanNames[i]) == 0 ||
        investmentHeaderEquals(category, kEnglishNames[i])) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

template <typename Input>
inline bool retirementParseGoalsCsv(Input &input, RetirementSummary &out) {
  InvestmentCsvReader<Input> reader(input);
  RetirementSummary parsed{};
  InvestmentColumns assetColumns;
  RetirementColumns allocationColumns;
  RetirementColumns stockGoalColumns;
  uint8_t foundAssets = 0;
  uint8_t foundRegions = 0;
  uint8_t foundStockGoals = 0;
  bool inAssets = false;
  bool foundAssetHeader = false;
  bool inRegions = false;
  bool foundRegionHeader = false;
  bool inStockGoals = false;
  bool foundStockGoalHeader = false;
  bool foundAssetTotal = false;
  bool foundRegionTotal = false;
  bool foundStockGoalTotal = false;

  for (uint8_t rowIndex = 0; rowIndex < 96; ++rowIndex) {
    InvestmentCsvRow row;
    if (!reader.next(row)) {
      if (reader.eof()) break;
      return false;
    }

    if (investmentCellEquals(row, 0, "주식 투자목표") ||
        investmentCellEquals(row, 0, "stock investment goals")) {
      inAssets = false;
      inRegions = true;
      foundRegionHeader = false;
      continue;
    }
    if (investmentCellEquals(row, 0, "주식 세부투자목표") ||
        investmentCellEquals(row, 0, "stock detailed investment goals")) {
      inAssets = false;
      inRegions = false;
      inStockGoals = true;
      foundStockGoalHeader = false;
      continue;
    }
    if (!inAssets && !inRegions && !inStockGoals &&
        (investmentCellEquals(row, 0, "투자목표") ||
         investmentCellEquals(row, 0, "investment goals"))) {
      inAssets = true;
      foundAssetHeader = false;
      continue;
    }

    if (inAssets) {
      if (!foundAssetHeader) {
        foundAssetHeader = investmentFindColumns(row, assetColumns);
        continue;
      }
      if (assetColumns.asset >= row.count ||
          assetColumns.holdings >= row.count ||
          assetColumns.profit >= row.count) continue;
      if (row.truncated[assetColumns.asset] ||
          row.truncated[assetColumns.holdings] ||
          row.truncated[assetColumns.profit]) return false;
      const char *asset = row.fields[assetColumns.asset];
      if (investmentIsTotal(asset)) {
        int64_t holdingsSum = 0;
        for (uint8_t i = 0; i < kRetirementAssetCount - 1; ++i) {
          holdingsSum += parsed.assets[i].holdings;
        }
        if (foundAssets != 0x3F ||
            !investmentParseAmount(row.fields[assetColumns.holdings],
                                   parsed.assets[kRetirementAssetCount - 1].holdings) ||
            !investmentParseAmount(row.fields[assetColumns.profit],
                                   parsed.assets[kRetirementAssetCount - 1].profit) ||
            holdingsSum != parsed.assets[kRetirementAssetCount - 1].holdings) {
          return false;
        }
        foundAssetTotal = true;
        inAssets = false;
        continue;
      }
      if (asset[0] < '1' || asset[0] > '6' || asset[1] != '.') continue;
      const uint8_t index = static_cast<uint8_t>(asset[0] - '1');
      const uint8_t bit = static_cast<uint8_t>(1U << index);
      if ((foundAssets & bit) ||
          !investmentParseAmount(row.fields[assetColumns.holdings],
                                 parsed.assets[index].holdings) ||
          !investmentParseAmount(row.fields[assetColumns.profit],
                                 parsed.assets[index].profit) ||
          parsed.assets[index].holdings < 0) return false;
      foundAssets |= bit;
      continue;
    }

    if (inRegions) {
      if (!foundRegionHeader) {
        foundRegionHeader =
            retirementFindColumns(row, allocationColumns, "구분");
        continue;
      }
      if (allocationColumns.category >= row.count ||
          allocationColumns.holdings >= row.count) continue;
      if (row.truncated[allocationColumns.category] ||
          row.truncated[allocationColumns.holdings]) return false;
      const char *category = row.fields[allocationColumns.category];
      if (investmentIsTotal(category)) {
        int64_t holdingsSum = 0;
        for (const InvestmentAmount &region : parsed.regions) {
          holdingsSum += region.holdings;
        }
        if (foundRegions != 0x0F ||
            !investmentParseAmount(row.fields[allocationColumns.holdings],
                                   parsed.regionTotal.holdings) ||
            holdingsSum != parsed.regionTotal.holdings) return false;
        foundRegionTotal = true;
        inRegions = false;
        continue;
      }
      const int8_t index = retirementRegionIndex(category);
      if (index < 0) continue;
      const uint8_t bit = static_cast<uint8_t>(1U << index);
      if ((foundRegions & bit) ||
          !investmentParseAmount(row.fields[allocationColumns.holdings],
                                 parsed.regions[index].holdings) ||
          parsed.regions[index].holdings < 0) return false;
      foundRegions |= bit;
      continue;
    }

    if (inStockGoals) {
      if (!foundStockGoalHeader) {
        foundStockGoalHeader =
            retirementFindColumns(row, stockGoalColumns, "세부구분");
        if (foundStockGoalHeader &&
            stockGoalColumns.actualRate < 0) return false;
        continue;
      }
      if (stockGoalColumns.category >= row.count ||
          stockGoalColumns.holdings >= row.count ||
          stockGoalColumns.actualRate >= row.count) continue;
      if (row.truncated[stockGoalColumns.category] ||
          row.truncated[stockGoalColumns.holdings] ||
          row.truncated[stockGoalColumns.actualRate]) return false;
      const char *category = row.fields[stockGoalColumns.category];
      if (investmentIsTotal(category)) {
        int64_t holdingsSum = 0;
        uint8_t actualRate = 100;
        const char *totalRateText = row.fields[stockGoalColumns.actualRate];
        const bool validRate = !totalRateText[0] ||
            (investmentParsePercent(totalRateText, actualRate) &&
             actualRate == 100);
        for (const InvestmentStockGoal &goal : parsed.stockGoals) {
          holdingsSum += goal.holdings;
        }
        if (foundStockGoals != ((1U << kRetirementStockGoalCount) - 1) ||
            !investmentParseAmount(row.fields[stockGoalColumns.holdings],
                                   parsed.stockGoalTotal.holdings) ||
            !validRate ||
            holdingsSum != parsed.stockGoalTotal.holdings ||
            actualRate != 100) return false;
        parsed.stockGoalTotal.actualRate = actualRate;
        foundStockGoalTotal = true;
        inStockGoals = false;
        continue;
      }
      const int8_t index = retirementStockGoalIndex(category);
      if (index < 0) continue;
      const uint8_t bit = static_cast<uint8_t>(1U << index);
      if ((foundStockGoals & bit) ||
          !investmentParseAmount(row.fields[stockGoalColumns.holdings],
                                 parsed.stockGoals[index].holdings) ||
          !investmentParsePercent(row.fields[stockGoalColumns.actualRate],
                                  parsed.stockGoals[index].actualRate) ||
          parsed.stockGoals[index].holdings < 0) return false;
      foundStockGoals |= bit;
    }
  }

  parsed.valid = foundAssetTotal && foundRegionTotal && foundStockGoalTotal;
  if (!parsed.valid) return false;
  out = parsed;
  return true;
}
