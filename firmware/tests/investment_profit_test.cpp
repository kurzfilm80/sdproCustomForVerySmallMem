#include "InvestmentViewModel.h"
#include <cassert>
#include <string>
#include <vector>

struct Input {
  const std::string &text;
  size_t position = 0;
  int read() { return position < text.size() ? uint8_t(text[position++]) : -1; }
};

std::string csv(const std::vector<std::string> &cells) {
  std::string out;
  for (size_t i = 0; i < cells.size(); ++i) {
    if (i) out += ',';
    out += '"';
    for (char c : cells[i]) { if (c == '"') out += '"'; out += c; }
    out += '"';
  }
  return out + '\n';
}

std::string investmentSheet() {
  std::string s = "投資\n투자목표\n";
  s += "자산군,보유금액의 SUM,수익액의 SUM\n";
  for (unsigned i = 1; i <= 4; ++i) s += std::to_string(i) + ".자산,10," + std::to_string(i) + "\n";
  s += "5.주식,1000,-20\n총계,1040,-10\n세부 투자목표\n";
  s += "자산군,구분,보유금액의 SUM,수익액의 SUM\n";
  s += "5.주식,글로벌,100,10\n5.주식,미국,200,-20\n5.주식,중국,300,30\n5.주식,한국,400,-40\n";
  s += "주식 세부투자목표\n세부구분,실투율,수익액의 SUM,보유금액의 SUM\n";
  const char *names[] = {"금융/가치", "바이오/헬스케어", "반도체", "산업/자동차", "소비/콘텐츠/통신", "시장분산", "에너지/인프라", "AI/테크"};
  const int profits[] = {1,-2,3,-4,5,-6,7,-24};
  for (unsigned i = 0; i < 8; ++i) s += std::string(names[i]) + ",10%," + std::to_string(profits[i]) + ',' + (i == 7 ? "300\n" : "100\n");
  return s + "총계,100%,-20,1000\n";
}

std::string retirementGoals(const std::string &valueProduct) {
  std::string s = "투자목표\n자산군,보유금액의 SUM,수익액의 SUM\n";
  for (unsigned i = 1; i <= 6; ++i) s += std::to_string(i) + ".자산,100,0\n";
  s += "총계,600,0\n주식 투자목표\n구분,보유금액의 SUM\n글로벌 총계,1\n미국 총계,2\n중국 총계,3\n한국 총계,4\n총계,10\n";
  s += "주식 세부투자목표\n投資比率,실투율,세부구분,보유금액의 SUM,수익액의 SUM\n";
  const char *names[] = {"가치/배당", "금/원자재", "바이오/헬스케어", "시장분산", "AI/기술"};
  const std::string products[] = {valueProduct, "Gold Mix", "Health Mix", "Index Fund", "Tech Fund"};
  for (unsigned i = 0; i < 5; ++i) {
    std::vector<std::string> row(10);
    row[1] = "20%"; row[2] = names[i]; row[3] = "2"; row[4] = "1";
    row[9] = "  " + products[i] + " | Other " + std::to_string(i) + "  ";
    s += csv(row);
  }
  return s + ",100%,총계,10,5\n";
}

std::string details(const std::string &valueProduct, bool unknown = false) {
  std::vector<std::string> row(19);
  for (unsigned b = 0; b < 2; ++b) {
    const unsigned start = b * 7;
    row[start] = "자산군"; row[start+1] = "구분"; row[start+2] = "상품";
    row[start+3] = "평가금액의 SUM"; row[start+4] = "수익률/이율의 AVERAGE"; row[start+5] = "평가손익의 SUM";
  }
  row[14] = "자산군"; row[15] = "상품"; row[16] = "평가금액의 SUM";
  std::string s = csv(row);
  row.assign(19, "");
  row[0] = "6.주식"; row[1] = "미국"; row[2] = unknown ? "Unclassified Fund" : valueProduct;
  row[3] = "100"; row[5] = "10";
  row[7] = "6.주식"; row[8] = "글로벌"; row[9] = "Tech Fund"; row[10] = "200"; row[12] = "-20";
  // The third parallel block contains closed positions and must never count.
  row[14] = "6.주식"; row[15] = "Closed Fund"; row[16] = "999999"; row[17] = "#REF!";
  s += csv(row);
  row.assign(19, "");
  row[0] = "3.혼합"; row[1] = "한국"; row[2] = "Health Mix"; row[3] = "999"; row[5] = "99";
  row[7] = "총계"; row[10] = "200"; row[12] = "-20"; s += csv(row);
  row.assign(19, ""); row[0] = "총계"; row[3] = "1099"; row[5] = "109"; s += csv(row);
  // A second active account under the left table reuses its own header.
  row.assign(19, ""); row[0] = "자산군"; row[1] = "구분"; row[2] = "상품"; row[3] = "평가금액의 SUM"; row[5] = "평가손익의 SUM"; s += csv(row);
  row.assign(19, ""); row[0] = "6.주식"; row[1] = "한국"; row[2] = "Index Fund"; row[3] = "300"; row[5] = "30"; s += csv(row);
  row.assign(19, ""); row[0] = "총계"; row[3] = "300"; row[5] = "30"; return s + csv(row);
}

int main() {
  const std::string longName = std::string(90, 'A') + " 한글 Fund";
  const auto investmentText = investmentSheet();
  Input invInput{investmentText}; InvestmentSummary inv;
  assert(investmentParseCsv(invInput, inv));
  assert(inv.regions.valid && inv.regions.total.holdings == 1000 && inv.regions.total.profit == -20);
  assert(inv.stockGoals.total.hasProfit && inv.stockGoals.total.profit == -20);
  const auto goalText = retirementGoals(longName);
  Input goalInput{goalText}; RetirementSummary retirement; RetirementThemeProductMap map;
  assert(retirementParseGoalsCsv(goalInput, retirement, &map));
  assert(map.valid && map.count == 10);
  const auto detailText = details(longName);
  Input detailInput{detailText};
  assert(retirementParseDetailsCsv(detailInput, retirement, map));
  assert(retirement.assets[2].holdings == 999 && retirement.assets[2].profit == 99);
  assert(retirement.assets[5].holdings == 600 && retirement.assets[5].profit == 20);
  assert(retirement.assets[6].holdings == 1599 && retirement.assets[6].profit == 119);
  assert(retirement.regionTotal.holdings == 600 && retirement.regionTotal.profit == 20);
  assert(retirement.stockGoalTotal.holdings == 600 && retirement.stockGoalTotal.profit == 20);
  assert(retirement.stockGoals[0].holdings == 100 && retirement.stockGoals[0].profit == 10);
  assert(retirement.stockGoals[2].holdings == 0);  // Health Mix excluded from stock themes.
  auto assets = investmentDisplayPage(0, inv, inv.regions, retirement);
  auto regions = investmentDisplayPage(1, inv, inv.regions, retirement);
  auto themes = investmentDisplayPage(2, inv, inv.regions, retirement);
  assert(assets.total.holdings == 2639 && assets.total.profit == 109);
  assert(assets.rows[6].holdings == regions.total.holdings);
  assert(regions.total.holdings == 1600 && regions.total.profit == 0);
  assert(themes.total.holdings == regions.total.holdings && themes.total.profit == regions.total.profit);
  for (const auto &page : {assets, regions, themes}) {
    int64_t holdings = 0, profit = 0;
    for (uint8_t i = 0; i < page.count; ++i) {
      assert(page.rows[i].hasProfit);
      holdings += page.rows[i].holdings; profit += page.rows[i].profit;
    }
    assert(holdings == page.total.holdings && profit == page.total.profit);
  }
  const auto invalidText = details(longName, true);
  Input invalidInput{invalidText}; RetirementSummary retained = retirement;
  assert(!retirementParseDetailsCsv(invalidInput, retained, map));
  assert(retained.assets[6].holdings == retirement.assets[6].holdings);
  auto badTotals = detailText;
  const auto total = badTotals.find("\"1099\""); assert(total != std::string::npos);
  badTotals.replace(total, 6, "\"5000\"");
  Input badInput{badTotals}; assert(!retirementParseDetailsCsv(badInput, retained, map));
  auto badProfit = investmentText;
  const auto final = badProfit.find("총계,100%,-20,1000"); assert(final != std::string::npos);
  badProfit.replace(final, std::string("총계,100%,-20,1000").size(), "총계,100%,-200,1000");
  Input badProfitInput{badProfit}; InvestmentSummary parsed;
  assert(investmentParseCsv(badProfitInput, parsed) && !parsed.stockGoals.valid);
  auto rounding = investmentText;
  // Perturb one theme row by a single won; keep the canonical stock total.
  const auto theme = rounding.find("AI/테크,10%,-24,300");
  rounding.replace(theme, std::string("AI/테크,10%,-24,300").size(), "AI/테크,10%,-24,301");
  Input roundingInput{rounding}; InvestmentSummary normalized;
  assert(investmentParseCsv(roundingInput, normalized));
  assert(normalized.stockGoals.valid && normalized.stockGoals.total.holdings == 1000);
  assert(normalized.stockGoals.categories[7].holdings == 300);
  int32_t boundedHoldings = 1, boundedProfit = INT32_MAX;
  assert(!investmentApplyRoundingResidual(boundedHoldings, boundedProfit, -2, 0));
  assert(!investmentApplyRoundingResidual(boundedHoldings, boundedProfit, 0, 1));
  assert(boundedHoldings == 1 && boundedProfit == INT32_MAX);
  char display[16];
  investmentFormatWholeMillions(123072, display, sizeof(display));
  assert(std::string(display) == "<1");
  investmentFormatWholeMillions(0, display, sizeof(display));
  assert(std::string(display) == "0");
  assert(investmentFormatSharePercent(123072, 267000000, display, sizeof(display)));
  assert(std::string(display) == "<1%");
  map.add(map.hashes[0], 4); assert(!map.valid);  // Ambiguous product classification rejected.
  Input conflictingInput{detailText}; assert(!retirementParseDetailsCsv(conflictingInput, retained, map));
}
