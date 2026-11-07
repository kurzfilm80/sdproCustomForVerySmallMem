#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

constexpr uint8_t kInvestmentAssetCount = 6;
constexpr uint8_t kInvestmentCsvFieldLimit = 24;
constexpr uint16_t kInvestmentCsvFieldLength = 64;
constexpr uint16_t kInvestmentCsvByteLimit = 16384;
constexpr uint16_t kInvestmentCsvRowLimit = 2048;
constexpr uint8_t kInvestmentRegionCount = 4;
constexpr uint8_t kInvestmentStockGoalCount = 8;
constexpr uint8_t kInvestmentPageCount = 6;

struct InvestmentAmount {
  int32_t holdings = 0;
  int32_t profit = 0;
};

struct InvestmentStockGoal {
  int32_t holdings = 0;
  uint8_t actualRate = 0;
};

struct InvestmentStockGoals {
  InvestmentStockGoal categories[kInvestmentStockGoalCount]{};
  InvestmentStockGoal total{};
  bool valid = false;
};

struct InvestmentSummary {
  InvestmentAmount assets[kInvestmentAssetCount]{};
  InvestmentStockGoals stockGoals{};
  bool valid = false;
};

struct InvestmentRegionSummary {
  InvestmentAmount regions[kInvestmentRegionCount]{};
  InvestmentAmount total{};
  bool valid = false;
};

struct InvestmentPageCycle {
  uint8_t page = 0;
  uint32_t shownAt = 0;
  bool started = false;

  bool advance(uint32_t now) {
    if (!started) {
      started = true;
      shownAt = now;
      return false;
    }
    if (now - shownAt < 12000) return false;
    page = (page + 1) % kInvestmentPageCount;
    shownAt = now;
    return true;
  }
};

struct InvestmentCsvRow {
  char fields[kInvestmentCsvFieldLimit][kInvestmentCsvFieldLength]{};
  bool truncated[kInvestmentCsvFieldLimit]{};
  uint8_t count = 0;
};

template <typename Input>
inline auto investmentCsvReadByte(Input &input, uint8_t &value, int)
    -> decltype(input.readBytes(&value, 1), int()) {
  return input.readBytes(&value, 1) == 1 ? value : -1;
}

template <typename Input>
inline int investmentCsvReadByte(Input &input, uint8_t &, long) {
  return input.read();
}

template <typename Input>
class InvestmentCsvReader {
 public:
  explicit InvestmentCsvReader(Input &input) : input_(input) {}

  bool next(InvestmentCsvRow &row) {
    row = InvestmentCsvRow{};
    uint16_t column = 0;
    uint16_t fieldLength = 0;
    uint16_t rowLength = 0;
    bool fieldStarted = false;
    bool fieldTruncated = false;
    bool quoted = false;
    bool afterQuoted = false;

    const auto finishField = [&]() {
      if (column < kInvestmentCsvFieldLimit) {
        row.fields[column][fieldLength] = '\0';
        row.truncated[column] = fieldTruncated;
        row.count = column + 1;
      }
      ++column;
      fieldLength = 0;
      fieldStarted = false;
      fieldTruncated = false;
      afterQuoted = false;
      return true;
    };
    const auto append = [&](char value) {
      if (column >= kInvestmentCsvFieldLimit) {
        fieldStarted = true;
        return true;
      }
      if (fieldLength + 1 >= kInvestmentCsvFieldLength) {
        fieldTruncated = true;
        return true;
      }
      row.fields[column][fieldLength++] = value;
      fieldStarted = true;
      return true;
    };

    while (rowLength <= kInvestmentCsvRowLimit) {
      int value;
      if (!read(value)) return false;
      if (value < 0) {
        if (quoted || rowLength == 0) return false;
        return finishField();
      }
      ++rowLength;

      if (quoted) {
        if (value != '"') {
          if (!append(static_cast<char>(value))) return false;
          continue;
        }

        int next;
        if (!read(next)) return false;
        if (next == '"') {
          ++rowLength;
          if (!append('"')) return false;
          continue;
        }
        quoted = false;
        afterQuoted = true;
        if (next < 0) return finishField();
        ++rowLength;
        if (next == ',') {
          if (!finishField()) return false;
        } else if (next == '\n') {
          return finishField();
        } else if (next != '\r') {
          return false;
        }
        continue;
      }

      if (afterQuoted) {
        if (value == ',') {
          if (!finishField()) return false;
        } else if (value == '\n') {
          return finishField();
        } else if (value != '\r') {
          return false;
        }
      } else if (value == '"') {
        if (fieldStarted) return false;
        quoted = true;
        fieldStarted = true;
      } else if (value == ',') {
        if (!finishField()) return false;
      } else if (value == '\n') {
        return finishField();
      } else if (value != '\r') {
        if (!append(static_cast<char>(value))) return false;
      }
    }
    return false;
  }

  bool eof() const { return eof_; }

 private:
  bool read(int &value) {
    if (bytesRead_ >= kInvestmentCsvByteLimit) return false;
    uint8_t byte = 0;
    value = investmentCsvReadByte(input_, byte, 0);
    if (value >= 0) {
      ++bytesRead_;
    } else {
      eof_ = true;
    }
    return true;
  }

  Input &input_;
  uint16_t bytesRead_ = 0;
  bool eof_ = false;
};

inline bool investmentParseAmount(const char *text, int32_t &out) {
  if (!text || !*text) return false;
  const char *cursor = text;
  bool negative = false;
  if (*cursor == '-') {
    negative = true;
    ++cursor;
  }
  if (std::strncmp(cursor, "\xE2\x82\xA9", 3) == 0) {
    cursor += 3;
  }

  uint32_t value = 0;
  uint16_t digits = 0;
  uint16_t groupDigits = 0;
  uint8_t separators = 0;
  for (; *cursor; ++cursor) {
    if (*cursor >= '0' && *cursor <= '9') {
      const uint8_t digit = static_cast<uint8_t>(*cursor - '0');
      if (value > (1000000000UL - digit) / 10) return false;
      value = value * 10 + digit;
      ++digits;
      ++groupDigits;
    } else if (*cursor == ',') {
      if ((separators == 0 && (groupDigits == 0 || groupDigits > 3)) ||
          (separators != 0 && groupDigits != 3)) return false;
      ++separators;
      groupDigits = 0;
    } else {
      return false;
    }
  }
  if (!digits || (separators && groupDigits != 3)) return false;
  out = static_cast<int32_t>(negative ? -static_cast<int64_t>(value) : value);
  return true;
}

inline bool investmentCellEquals(const InvestmentCsvRow &row, uint8_t index,
                                 const char *text) {
  return index < row.count && std::strcmp(row.fields[index], text) == 0;
}

inline bool investmentHeaderEquals(const char *value, const char *expected) {
  while (*value || *expected) {
    while (*value == ' ' || *value == '\t' || *value == '_') ++value;
    while (*expected == ' ' || *expected == '\t' || *expected == '_') ++expected;
    char left = *value;
    char right = *expected;
    if (left >= 'A' && left <= 'Z') left = static_cast<char>(left + ('a' - 'A'));
    if (right >= 'A' && right <= 'Z') right = static_cast<char>(right + ('a' - 'A'));
    if (left != right) return false;
    if (*value) ++value;
    if (*expected) ++expected;
  }
  return true;
}

struct InvestmentColumns {
  int8_t asset = -1;
  int8_t category = -1;
  int8_t product = -1;
  int8_t holdings = -1;
  int8_t profit = -1;
};

struct InvestmentStockGoalColumns {
  int8_t category = -1;
  int8_t holdings = -1;
  int8_t actualRate = -1;
};

inline bool investmentHeaderColumn(const char *value, const char *korean,
                                   const char *english) {
  return investmentHeaderEquals(value, korean) ||
      investmentHeaderEquals(value, english);
}

inline bool investmentFindColumns(const InvestmentCsvRow &row,
                                 InvestmentColumns &columns) {
  columns = InvestmentColumns{};
  for (uint8_t i = 0; i < row.count; ++i) {
    if (row.truncated[i]) continue;
    const char *field = row.fields[i];
    if (columns.asset < 0 &&
        (investmentHeaderColumn(field, "자산군", "asset class") ||
         investmentHeaderEquals(field, "asset group"))) columns.asset = i;
    if (columns.category < 0 &&
        investmentHeaderColumn(field, "구분", "category")) columns.category = i;
    if (columns.product < 0 &&
        investmentHeaderColumn(field, "상품", "product")) columns.product = i;
    if (columns.holdings < 0 &&
        (investmentHeaderColumn(field, "보유금액의 SUM", "holdings") ||
         investmentHeaderColumn(field, "금액의 SUM", "amount") ||
         investmentHeaderEquals(field, "holdings sum") ||
         investmentHeaderEquals(field, "holding amount"))) columns.holdings = i;
    if (columns.profit < 0 &&
        (investmentHeaderColumn(field, "수익액의 SUM", "profit") ||
         investmentHeaderColumn(field, "예상수익액의 SUM", "expected profit") ||
         investmentHeaderEquals(field, "profit sum"))) {
      columns.profit = i;
    }
  }
  return columns.asset >= 0 && columns.holdings >= 0 && columns.profit >= 0;
}

inline bool investmentFindStockGoalColumns(
    const InvestmentCsvRow &row, InvestmentStockGoalColumns &columns) {
  columns = InvestmentStockGoalColumns{};
  for (uint8_t i = 0; i < row.count; ++i) {
    if (row.truncated[i]) continue;
    const char *field = row.fields[i];
    if (columns.category < 0 &&
        (investmentHeaderColumn(field, "세부구분", "subcategory") ||
         investmentHeaderEquals(field, "detail category"))) {
      columns.category = i;
    }
    if (columns.holdings < 0 &&
        investmentHeaderColumn(field, "보유금액의 SUM", "holdings")) {
      columns.holdings = i;
    }
    if (columns.actualRate < 0 &&
        (investmentHeaderColumn(field, "실투율", "actual rate") ||
         investmentHeaderEquals(field, "actual allocation"))) {
      columns.actualRate = i;
    }
  }
  return columns.category >= 0 && columns.holdings >= 0 &&
      columns.actualRate >= 0;
}

inline int8_t investmentStockGoalIndex(const char *category) {
  static constexpr const char *kKoreanNames[kInvestmentStockGoalCount] = {
      "금융/가치", "바이오/헬스케어", "반도체", "산업/자동차",
      "소비/콘텐츠/통신", "시장분산", "에너지/인프라", "AI/테크"};
  static constexpr const char *kEnglishNames[kInvestmentStockGoalCount] = {
      "finance/value", "bio/healthcare", "semiconductors", "industrial/auto",
      "consumer/media/telecom", "market diversification",
      "energy/infrastructure", "AI/tech"};
  for (uint8_t i = 0; i < kInvestmentStockGoalCount; ++i) {
    if (std::strcmp(category, kKoreanNames[i]) == 0 ||
        investmentHeaderEquals(category, kEnglishNames[i])) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

inline bool investmentParsePercent(const char *text, uint8_t &out) {
  if (!text || !*text) return false;
  size_t length = std::strlen(text);
  if (length && text[length - 1] == '%') --length;
  if (!length) return false;
  uint16_t value = 0;
  for (size_t i = 0; i < length; ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    value = value * 10 + static_cast<uint16_t>(text[i] - '0');
    if (value > 100) return false;
  }
  out = static_cast<uint8_t>(value);
  return true;
}

inline int8_t investmentRegionIndex(const char *category) {
  if (std::strcmp(category, "글로벌") == 0 ||
      investmentHeaderEquals(category, "global")) return 0;
  if (std::strcmp(category, "미국") == 0 ||
      investmentHeaderEquals(category, "us") ||
      investmentHeaderEquals(category, "u.s.")) return 1;
  if (std::strcmp(category, "중국") == 0 ||
      investmentHeaderEquals(category, "china")) return 2;
  if (std::strcmp(category, "한국") == 0 ||
      investmentHeaderEquals(category, "korea")) return 3;
  return -1;
}

inline bool investmentIsTotal(const char *value) {
  return std::strcmp(value, "총계") == 0 || investmentHeaderEquals(value, "total");
}

inline bool investmentIsStockGroup(const char *value) {
  if (value[0] != '5' || value[1] != '.') return false;
  return std::strstr(value, "주식") != nullptr ||
      investmentHeaderEquals(value + 2, "stocks") ||
      investmentHeaderEquals(value + 2, "stock");
}

template <typename Input>
inline bool investmentParseCsv(Input &input, InvestmentSummary &out) {
  InvestmentCsvReader<Input> reader(input);
  InvestmentSummary parsed{};
  InvestmentColumns columns;
  InvestmentStockGoalColumns stockGoalColumns;
  uint8_t foundAssets = 0;
  uint8_t foundStockGoals = 0;
  bool inTargetSection = false;
  bool foundHeader = false;
  bool inStockGoalSection = false;
  bool foundStockGoalHeader = false;
  bool stockGoalSectionFailed = false;

  for (uint8_t rowIndex = 0; rowIndex < 96; ++rowIndex) {
    InvestmentCsvRow row;
    if (!reader.next(row)) {
      if (reader.eof()) break;
      return false;
    }

    if (investmentCellEquals(row, 0, "주식 세부투자목표") ||
        investmentCellEquals(row, 0, "stock detailed investment goals")) {
      inTargetSection = false;
      inStockGoalSection = true;
      foundStockGoalHeader = false;
      continue;
    }
    if (!inTargetSection) {
      if (!inStockGoalSection &&
          (investmentCellEquals(row, 0, "투자목표") ||
           investmentCellEquals(row, 0, "investment goals"))) {
        inTargetSection = true;
        foundHeader = false;
      }
      if (inStockGoalSection) {
        if (!foundStockGoalHeader) {
          foundStockGoalHeader =
              investmentFindStockGoalColumns(row, stockGoalColumns);
          continue;
        }
        if (stockGoalColumns.category >= row.count ||
            stockGoalColumns.holdings >= row.count ||
            stockGoalColumns.actualRate >= row.count) continue;
        if (row.truncated[stockGoalColumns.category] ||
            row.truncated[stockGoalColumns.holdings] ||
            row.truncated[stockGoalColumns.actualRate]) {
          stockGoalSectionFailed = true;
          inStockGoalSection = false;
          continue;
        }

        const char *category = row.fields[stockGoalColumns.category];
        if (!category[0]) continue;
        int32_t holdings = 0;
        uint8_t actualRate = 0;
        if (investmentIsTotal(category)) {
          int64_t holdingsSum = 0;
          for (const InvestmentStockGoal &goal : parsed.stockGoals.categories) {
            holdingsSum += goal.holdings;
          }
          const bool validTotal =
              foundStockGoals == ((1U << kInvestmentStockGoalCount) - 1) &&
              investmentParseAmount(row.fields[stockGoalColumns.holdings], holdings) &&
              investmentParsePercent(row.fields[stockGoalColumns.actualRate], actualRate) &&
              holdings >= 0 && holdingsSum == holdings && actualRate == 100;
          if (validTotal) {
            parsed.stockGoals.total.holdings = holdings;
            parsed.stockGoals.total.actualRate = actualRate;
            parsed.stockGoals.valid = true;
          } else {
            stockGoalSectionFailed = true;
          }
          inStockGoalSection = false;
          continue;
        }

        const int8_t index = investmentStockGoalIndex(category);
        if (index < 0) continue;
        const uint8_t bit = static_cast<uint8_t>(1U << index);
        if ((foundStockGoals & bit) ||
            !investmentParseAmount(row.fields[stockGoalColumns.holdings], holdings) ||
            !investmentParsePercent(
                row.fields[stockGoalColumns.actualRate], actualRate) ||
            holdings < 0) {
          stockGoalSectionFailed = true;
          inStockGoalSection = false;
          continue;
        }
        parsed.stockGoals.categories[index] = {holdings, actualRate};
        foundStockGoals |= bit;
        continue;
      }
      continue;
    }
    if (!foundHeader) {
      foundHeader = investmentFindColumns(row, columns);
      continue;
    }
    if (columns.asset >= row.count || columns.holdings >= row.count ||
        columns.profit >= row.count) continue;
    if (row.truncated[columns.asset] || row.truncated[columns.holdings] ||
        row.truncated[columns.profit]) return false;

    const char *asset = row.fields[columns.asset];
    if (investmentIsTotal(asset)) {
      if (foundAssets != 0x1F ||
          !investmentParseAmount(row.fields[columns.holdings], parsed.assets[5].holdings) ||
          !investmentParseAmount(row.fields[columns.profit], parsed.assets[5].profit)) {
        return false;
      }
      parsed.valid = true;
      inTargetSection = false;
      continue;
    }
    if (asset[0] < '1' || asset[0] > '5' || asset[1] != '.') continue;
    const uint8_t index = static_cast<uint8_t>(asset[0] - '1');
    const uint8_t bit = static_cast<uint8_t>(1U << index);
    if ((foundAssets & bit) ||
        !investmentParseAmount(row.fields[columns.holdings], parsed.assets[index].holdings) ||
        !investmentParseAmount(row.fields[columns.profit], parsed.assets[index].profit) ||
        parsed.assets[index].holdings < 0) {
      return false;
    }
    foundAssets |= bit;
  }
  if (!parsed.valid) return false;
  if (stockGoalSectionFailed) parsed.stockGoals = InvestmentStockGoals{};
  out = parsed;
  return true;
}

template <typename Input>
inline bool investmentParseRegionsCsv(Input &input, InvestmentRegionSummary &out) {
  InvestmentCsvReader<Input> reader(input);
  InvestmentRegionSummary parsed{};
  InvestmentColumns columns;
  bool foundHeader = false;
  bool inStocks = false;
  int8_t activeRegion = -1;
  uint8_t foundRegions = 0;

  for (uint8_t rowIndex = 0; rowIndex < 128; ++rowIndex) {
    InvestmentCsvRow row;
    if (!reader.next(row)) return false;
    if (!foundHeader) {
      if (investmentFindColumns(row, columns) && columns.category >= 0 &&
          columns.product >= 0) {
        foundHeader = true;
      }
      continue;
    }

    if (columns.asset < row.count && row.fields[columns.asset][0]) {
      if (row.truncated[columns.asset]) return false;
      const char *asset = row.fields[columns.asset];
      if (investmentIsTotal(asset)) {
        if (foundRegions != 0x0F) return false;
        int64_t totalHoldings = 0;
        int64_t totalProfit = 0;
        for (const auto &region : parsed.regions) {
          totalHoldings += region.holdings;
          totalProfit += region.profit;
        }
        if (totalHoldings > 1000000000 || totalProfit < -1000000000 ||
            totalProfit > 1000000000) return false;
        parsed.total.holdings = static_cast<int32_t>(totalHoldings);
        parsed.total.profit = static_cast<int32_t>(totalProfit);
        parsed.valid = true;
        out = parsed;
        return true;
      }
      inStocks = investmentIsStockGroup(asset);
      if (!inStocks) activeRegion = -1;
    }
    if (!inStocks || columns.category >= row.count) continue;
    if (row.truncated[columns.category] ||
        (columns.product < row.count && row.truncated[columns.product]) ||
        (columns.holdings < row.count && row.truncated[columns.holdings]) ||
        (columns.profit < row.count && row.truncated[columns.profit])) return false;

    const char *category = row.fields[columns.category];
    const int8_t region = investmentRegionIndex(category);
    if (region >= 0) activeRegion = region;
    if (activeRegion < 0 || columns.product >= row.count ||
        !row.fields[columns.product][0] ||
        columns.holdings >= row.count || columns.profit >= row.count) continue;

    InvestmentAmount value;
    if (!investmentParseAmount(row.fields[columns.holdings], value.holdings) ||
        !investmentParseAmount(row.fields[columns.profit], value.profit) ||
        value.holdings < 0) return false;
    InvestmentAmount &total = parsed.regions[activeRegion];
    const int64_t holdings = static_cast<int64_t>(total.holdings) + value.holdings;
    const int64_t profit = static_cast<int64_t>(total.profit) + value.profit;
    if (holdings > 1000000000 || profit < -1000000000 || profit > 1000000000) {
      return false;
    }
    total.holdings = static_cast<int32_t>(holdings);
    total.profit = static_cast<int32_t>(profit);
    foundRegions |= static_cast<uint8_t>(1U << activeRegion);
  }
  return false;
}

inline void investmentFormatMillions(int32_t amount, char *out, size_t size) {
  const bool negative = amount < 0;
  const uint32_t magnitude = static_cast<uint32_t>(
      negative ? -static_cast<int64_t>(amount) : amount);
  const uint32_t hundredths = (magnitude + 5000) / 10000;
  std::snprintf(out, size, "%s%lu.%02lu", negative ? "-" : "",
      static_cast<unsigned long>(hundredths / 100),
      static_cast<unsigned long>(hundredths % 100));
}

inline void investmentFormatWholeMillions(int32_t amount, char *out, size_t size) {
  const bool negative = amount < 0;
  const uint64_t magnitude = static_cast<uint64_t>(
      negative ? -static_cast<int64_t>(amount) : amount);
  const uint64_t millions = (magnitude + 500000) / 1000000;
  std::snprintf(out, size, "%s%lu", negative ? "-" : "",
      static_cast<unsigned long>(millions));
}

inline bool investmentFormatProfit(int32_t profit, int32_t holdings,
                                   char *out, size_t size) {
  const int64_t principal = static_cast<int64_t>(holdings) - profit;
  if (principal <= 0) return false;
  const uint64_t magnitude = static_cast<uint64_t>(
      profit < 0 ? -static_cast<int64_t>(profit) : profit);
  const uint64_t tenths = (magnitude * 1000 + static_cast<uint64_t>(principal) / 2) /
      static_cast<uint64_t>(principal);
  std::snprintf(out, size, "%s%lu.%lu%%", profit > 0 ? "+" : profit < 0 ? "-" : "",
      static_cast<unsigned long>(tenths / 10),
      static_cast<unsigned long>(tenths % 10));
  return true;
}

inline bool investmentFormatSharePercent(int32_t holdings, int32_t total,
                                         char *out, size_t size) {
  if (holdings < 0 || total <= 0) return false;
  const uint32_t percent = static_cast<uint32_t>(
      (static_cast<uint64_t>(holdings) * 100 + total / 2) / total);
  std::snprintf(out, size, "%lu%%", static_cast<unsigned long>(percent));
  return true;
}

inline bool investmentFormatHoldingShare(int32_t holdings, int32_t total,
                                         char *out, size_t size) {
  if (holdings < 0 || total <= 0) return false;
  char amount[16];
  char percent[8];
  investmentFormatMillions(holdings, amount, sizeof(amount));
  if (!investmentFormatSharePercent(holdings, total, percent, sizeof(percent))) {
    return false;
  }
  std::snprintf(out, size, "%s(%s)", amount, percent);
  return true;
}
