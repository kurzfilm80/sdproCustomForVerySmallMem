#pragma once

#include "InvestmentModel.h"

constexpr uint8_t kRetirementAssetCount = 7;
constexpr uint8_t kRetirementRegionCount = 4;
constexpr uint8_t kRetirementStockGoalCount = 5;

struct RetirementThemeProductMap {
  uint64_t hashes[64]{};
  uint8_t themes[64]{};
  uint8_t count = 0;
  bool valid = true;

  int8_t find(uint64_t hash) const {
    for (uint8_t i = 0; i < count; ++i) if (hashes[i] == hash) return themes[i];
    return -1;
  }
  void add(uint64_t hash, uint8_t theme) {
    if (!hash) return;
    const int8_t existing = find(hash);
    if (existing >= 0) { if (existing != theme) valid = false; return; }
    if (count >= 64) { valid = false; return; }
    hashes[count] = hash;
    themes[count++] = theme;
  }
};

struct RetirementSummary {
  InvestmentAmount assets[kRetirementAssetCount]{};
  InvestmentAmount regions[kRetirementRegionCount]{};
  InvestmentAmount regionTotal{};
  InvestmentStockGoal stockGoals[kRetirementStockGoalCount]{};
  InvestmentStockGoal stockGoalTotal{};
  bool valid = false;
  bool regionProfitValid = false;
};

struct RetirementColumns {
  int8_t profit = -1;
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
    if (columns.profit < 0 &&
        investmentHeaderColumn(field, "수익액의 SUM", "profit")) {
      columns.profit = i;
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
inline bool retirementParseGoalsCsv(Input &input, RetirementSummary &out,
                                    RetirementThemeProductMap *productMap = nullptr) {
  InvestmentCsvReader<Input> reader(input);
  RetirementSummary parsed{};
  if (productMap) *productMap = RetirementThemeProductMap{};
  InvestmentProductList products;
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
#ifdef ESP8266
    InvestmentCsvRow &row = investmentCsvWorkspace();
#else
    InvestmentCsvRow row;
#endif
    if (!reader.next(row, productMap ? &products : nullptr)) {
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
        if (!investmentParseAmount(row.fields[assetColumns.holdings],
                                   parsed.assets[kRetirementAssetCount - 1].holdings) ||
            !investmentParseAmount(row.fields[assetColumns.profit],
                                   parsed.assets[kRetirementAssetCount - 1].profit) ||
            holdingsSum > INT32_MAX ||
            holdingsSum - parsed.assets[kRetirementAssetCount - 1].holdings > 3 ||
            parsed.assets[kRetirementAssetCount - 1].holdings - holdingsSum > 3) {
          return false;
        }
        parsed.assets[kRetirementAssetCount - 1].holdings = int32_t(holdingsSum);
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
            holdingsSum > INT32_MAX ||
            holdingsSum - parsed.regionTotal.holdings > 2 ||
            parsed.regionTotal.holdings - holdingsSum > 2) return false;
        parsed.regionTotal.holdings = int32_t(holdingsSum);
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
      int32_t profit = 0;
      const bool hasProfit = stockGoalColumns.profit >= 0;
      if (hasProfit && (stockGoalColumns.profit >= row.count ||
          row.truncated[stockGoalColumns.profit] ||
          !investmentParseAmount(row.fields[stockGoalColumns.profit], profit))) return false;
      const char *category = row.fields[stockGoalColumns.category];
      if (investmentIsTotal(category)) {
        int64_t holdingsSum = 0;
        int64_t profitSum = 0;
        uint8_t actualRate = 100;
        const char *totalRateText = row.fields[stockGoalColumns.actualRate];
        const bool validRate = !totalRateText[0] ||
            (investmentParsePercent(totalRateText, actualRate) &&
             actualRate == 100);
        for (const InvestmentStockGoal &goal : parsed.stockGoals) {
          holdingsSum += goal.holdings;
          profitSum += goal.profit;
        }
        if (foundStockGoals != ((1U << kRetirementStockGoalCount) - 1) ||
            !investmentParseAmount(row.fields[stockGoalColumns.holdings],
                                   parsed.stockGoalTotal.holdings) ||
            !validRate ||
            holdingsSum > INT32_MAX || profitSum < INT32_MIN || profitSum > INT32_MAX ||
            holdingsSum - parsed.stockGoalTotal.holdings > 3 ||
            parsed.stockGoalTotal.holdings - holdingsSum > 3 ||
            actualRate != 100 || (hasProfit &&
            (profitSum - profit > 3 || profit - profitSum > 3))) return false;
        parsed.stockGoalTotal.holdings = int32_t(holdingsSum);
        parsed.stockGoalTotal.actualRate = actualRate;
        parsed.stockGoalTotal.profit = int32_t(profitSum);
        parsed.stockGoalTotal.hasProfit = hasProfit;
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
      if (productMap) {
        if (products.overflow) productMap->valid = false;
        for (uint8_t i = 0; i < products.count; ++i) productMap->add(products.hashes[i], index);
      }
      parsed.stockGoals[index].profit = profit;
      parsed.stockGoals[index].hasProfit = hasProfit;
      foundStockGoals |= bit;
    }
  }

  parsed.valid = foundAssetTotal && foundRegionTotal && foundStockGoalTotal;
  if (!parsed.valid) return false;
  out = parsed;
  return true;
}

// Active DC/IRP/personal-pension tables have six columns; the closed-position
// table lacks a region column and is deliberately ignored.
template <typename Input>
inline bool retirementParseDetailsCsv(Input &input, RetirementSummary &out,
                                      const RetirementThemeProductMap &products) {
  if (!products.valid || !products.count) return false;
  InvestmentCsvReader<Input> reader(input);
  RetirementSummary parsed{};
  struct Block {
    bool header = false;
    bool active = false;
    int8_t asset = -1;
    int8_t region = -1;
    int64_t holdings = 0;
    int64_t profit = 0;
    uint8_t positions = 0;
  };
  Block blocks[2];
  uint8_t foundAssets = 0;
  bool foundStocks = false;
  bool foundEnd[2] = {false, false};
  for (uint8_t line = 0; line < 96; ++line) {
#ifdef ESP8266
    InvestmentCsvRow &row = investmentCsvWorkspace();
#else
    InvestmentCsvRow row;
#endif
    if (!reader.next(row)) { if (reader.eof()) break; return false; }
    for (uint8_t b = 0; b < 2; ++b) {
      const uint8_t start = b * 7;
      if (start + 5 >= row.count) continue;
      Block &block = blocks[b];
      if (investmentHeaderEquals(row.fields[start], "자산군")) {
        if (!investmentHeaderEquals(row.fields[start + 1], "구분") ||
            !investmentHeaderEquals(row.fields[start + 2], "상품") ||
            !investmentHeaderEquals(row.fields[start + 3], "평가금액의 SUM") ||
            !investmentHeaderEquals(row.fields[start + 5], "평가손익의 SUM")) return false;
        block = {true, true, -1, -1};
        foundEnd[b] = false;
        continue;
      }
      if (!block.header || !block.active) continue;
      const char *asset = row.fields[start];
      if (investmentIsTotal(asset)) {
        int32_t holdings, profit;
        if (!investmentParseAmount(row.fields[start + 3], holdings) ||
            !investmentParseAmount(row.fields[start + 5], profit)) return false;
        const int64_t tolerance = (block.positions + 1) / 2;
        // Sheets can round each instrument and its aggregate independently.
        if (block.holdings - holdings > tolerance || holdings - block.holdings > tolerance ||
            block.profit - profit > tolerance || profit - block.profit > tolerance) return false;
        block.active = false;
        foundEnd[b] = true;
        continue;
      }
      if (asset[0]) {
        if (asset[0] < '1' || asset[0] > '6' || asset[1] != '.') return false;
        block.asset = asset[0] - '1';
        block.region = -1;
      }
      if (!row.productHashes[b]) continue;
      if (block.asset < 0) return false;
      InvestmentAmount amount;
      if (row.truncated[start] || row.truncated[start + 1] ||
          row.truncated[start + 3] || row.truncated[start + 5] ||
          !investmentParseAmount(row.fields[start + 3], amount.holdings) ||
          !investmentParseAmount(row.fields[start + 5], amount.profit) ||
          amount.holdings < 0) return false;
      const auto add = [](InvestmentAmount &target, const InvestmentAmount &value) {
        const int64_t holdings = int64_t(target.holdings) + value.holdings;
        const int64_t profit = int64_t(target.profit) + value.profit;
        if (holdings > INT32_MAX || profit < INT32_MIN || profit > INT32_MAX) return false;
        target = {int32_t(holdings), int32_t(profit)};
        return true;
      };
      if (!add(parsed.assets[block.asset], amount) ||
          !add(parsed.assets[kRetirementAssetCount - 1], amount)) return false;
      block.holdings += amount.holdings;
      block.profit += amount.profit;
      ++block.positions;
      foundAssets |= uint8_t(1U << block.asset);
      if (block.asset != 5) continue;  // Strictly stocks; mixed funds stay in ASSET.
      if (row.fields[start + 1][0]) block.region = investmentRegionIndex(row.fields[start + 1]);
      if (block.region < 0) return false;
      const int8_t theme = products.find(row.productHashes[b]);
      if (theme < 0) return false;  // Never guess a new product's theme.
      if (!add(parsed.regions[block.region], amount) || !add(parsed.regionTotal, amount)) return false;
      auto &goal = parsed.stockGoals[theme];
      InvestmentAmount themeAmount{goal.holdings, goal.profit};
      if (!add(themeAmount, amount)) return false;
      goal.holdings = themeAmount.holdings;
      goal.profit = themeAmount.profit;
      foundStocks = true;
    }
  }
  if (!foundStocks || !foundAssets || !foundEnd[0] || !foundEnd[1]) return false;
  const auto &stock = parsed.assets[5];
  parsed.stockGoalTotal = {stock.holdings, 100, stock.profit, true};
  for (auto &goal : parsed.stockGoals) goal.hasProfit = true;
  if (parsed.regionTotal.holdings != stock.holdings || parsed.regionTotal.profit != stock.profit) return false;
  parsed.regionProfitValid = parsed.valid = true;
  out = parsed;
  return true;
}
