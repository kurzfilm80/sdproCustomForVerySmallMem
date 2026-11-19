#include "InvestmentModel.h"
#include "RetirementModel.h"
#include "InvestmentViewModel.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

class MemoryInput {
 public:
  explicit MemoryInput(const char *text) : text_(text) {}
  int read() {
    if (!text_[position_]) return -1;
    return static_cast<unsigned char>(text_[position_++]);
  }

 private:
  const char *text_;
  size_t position_ = 0;
};

class BlockingMemoryInput {
 public:
  explicit BlockingMemoryInput(const char *text) : text_(text) {}
  int read() { return -1; }
  size_t readBytes(uint8_t *buffer, size_t length) {
    size_t read = 0;
    while (read < length && text_[position_]) {
      buffer[read++] = static_cast<uint8_t>(text_[position_++]);
    }
    return read;
  }

 private:
  const char *text_;
  size_t position_ = 0;
};

const char kSheet[] =
    "header,value\r\n"
    "투자목표,,,,,,,,\r\n"
    "투자비율,실투율,자산군,보유금액의 SUM,수익액의 SUM,목표금액\r\n"
    "9%,9%,1.채권,\"₩17,260,017\",\"₩6,566,695\",\"₩17,260,017\"\r\n"
    "14%,14%,2.원자재,\"₩26,904,380\",\"-₩809,560\",\"₩26,904,380\"\r\n"
    "20%,20%,3.가상자산,\"₩40,152,756\",\"-₩17,162,590\",\"₩40,152,756\"\r\n"
    "3%,3%,4.리츠,\"₩6,666,153\",\"₩427,921\",\"₩6,666,153\"\r\n"
    "54%,54%,5.주식,\"₩107,509,308\",\"-₩2,188,496\",\"₩107,509,308\"\r\n"
    "100%,100%,총계,\"₩198,492,614\",\"-₩13,166,030\"\r\n"
    "세부 투자목표,,,,\r\n"
    "주식 세부투자목표,,,,\r\n"
    "세부구분,보유금액의 SUM,실투율,투자비율\r\n"
    "금융/가치,\"₩11,420,164\",11%,11%\r\n"
    "바이오/헬스케어,\"₩10,973,280\",10%,10%\r\n"
    "반도체,\"₩23,133,867\",21%,21%\r\n"
    "산업/자동차,\"₩3,953,100\",4%,4%\r\n"
    "소비/콘텐츠/통신,\"₩1,761,550\",2%,2%\r\n"
    "시장분산,\"₩11,930,928\",11%,11%\r\n"
    "에너지/인프라,\"₩5,101,858\",5%,5%\r\n"
    "AI/테크,\"₩39,446,920\",37%,37%\r\n"
    "총계,\"₩107,721,667\",100%,100%\r\n";

const char kRetirementSheet[] =
    "투자총금액,7,070\r\n"
    "투자목표\r\n"
    "투자비율,실투율,자산군,보유금액의 SUM,수익액의 SUM\r\n"
    "0%,0%,1.현금,123072,0\r\n"
    "14%,14%,2.채권,\"9,761,400\",\"-49,932\"\r\n"
    "27%,27%,3.혼합,\"18,922,045\",\"756,301\"\r\n"
    "7%,7%,4.원자재,\"4,860,890\",\"-987,040\"\r\n"
    "52%,52%,6.주식,\"37,031,659\",\"8,541,838\"\r\n"
    "100%,100%,총계,\"70,699,066\",\"8,261,167\"\r\n"
    "세부 투자목표\r\n"
    "주식 투자목표\r\n"
    "투자비율,실투율,구분,세부구분,보유금액의 SUM\r\n"
    "6%,6%,글로벌,금/원자재,\"276,450\"\r\n"
    "63%,63%,글로벌,시장분산,\"2,755,725\"\r\n"
    "30%,30%,글로벌,AI/기술,\"1,318,005\"\r\n"
    ",,글로벌 총계,,\"4,350,180\"\r\n"
    "6%,6%,미국,시장분산,\"1,616,220\"\r\n"
    "94%,94%,미국,AI/기술,\"27,442,395\"\r\n"
    ",,미국 총계,,\"29,058,615\"\r\n"
    "63%,63%,중국,시장분산,\"1,205,560\"\r\n"
    "37%,37%,중국,AI/기술,\"718,630\"\r\n"
    ",,중국 총계,,\"1,924,190\"\r\n"
    "34%,34%,한국,가치/배당,\"6,945,704\"\r\n"
    "9%,9%,한국,바이오/헬스케어,\"1,950,750\"\r\n"
    "38%,38%,한국,시장분산,\"7,910,875\"\r\n"
    "18%,18%,한국,AI/기술,\"3,813,390\"\r\n"
    ",,한국 총계,,\"20,620,719\"\r\n"
    ",,총계,,\"55,953,704\"\r\n"
    "주식 세부투자목표\r\n"
    "투자비율,실투율,세부구분,보유금액의 SUM\r\n"
    "12%,12%,가치/배당,\"6,945,704\"\r\n"
    "0%,0%,금/원자재,\"276,450\"\r\n"
    "3%,3%,바이오/헬스케어,\"1,950,750\"\r\n"
    "24%,24%,시장분산,\"13,488,380\"\r\n"
    "59%,59%,AI/기술,\"33,292,420\"\r\n"
    ",,총계,\"55,953,704\"\r\n";

int main() {
  MemoryInput validInput(kSheet);
  InvestmentSummary parsed{};
  assert(investmentParseCsv(validInput, parsed));
  assert(parsed.valid);
  assert(parsed.assets[0].holdings == 17260017);
  assert(parsed.assets[0].profit == 6566695);
  assert(parsed.assets[2].profit == -17162590);
  assert(parsed.assets[5].holdings == 198492614);
  assert(parsed.assets[5].profit == -13166030);
  assert(parsed.stockGoals.valid);
  assert(parsed.stockGoals.categories[0].holdings == 11420164);
  assert(parsed.stockGoals.categories[0].actualRate == 11);
  assert(parsed.stockGoals.categories[2].holdings == 23133867);
  assert(parsed.stockGoals.categories[7].actualRate == 37);
  assert(parsed.stockGoals.total.holdings == 107721667);
  assert(parsed.stockGoals.total.actualRate == 100);

  MemoryInput retirementInput(kRetirementSheet);
  RetirementSummary retirement{};
  assert(retirementParseGoalsCsv(retirementInput, retirement));
  assert(retirement.valid);
  assert(retirement.assets[0].holdings == 123072);
  assert(retirement.assets[1].profit == -49932);
  assert(retirement.assets[4].holdings == 0);
  assert(retirement.assets[5].holdings == 37031659);
  assert(retirement.assets[6].holdings == 70699066);
  assert(retirement.assets[6].profit == 8261167);
  assert(retirement.regions[0].holdings == 4350180);
  assert(retirement.regions[1].holdings == 29058615);
  assert(retirement.regions[3].holdings == 20620719);
  assert(retirement.regionTotal.holdings == 55953704);
  assert(retirement.stockGoals[0].holdings == 6945704);
  assert(retirement.stockGoals[2].actualRate == 3);
  assert(retirement.stockGoals[4].holdings == 33292420);
  assert(retirement.stockGoalTotal.holdings == 55953704);
  assert(retirement.stockGoalTotal.actualRate == 100);

  BlockingMemoryInput blockingInput(kSheet);
  InvestmentSummary blockingParsed{};
  assert(investmentParseCsv(blockingInput, blockingParsed));
  assert(blockingParsed.assets[5].holdings == 198492614);

  const char oversizedMetadata[] =
      "ignored,01234567890123456789012345678901234567890123456789012345678901234567890123456789\n"
      "header,value\r\n"
      "투자목표,,,,,,,,\r\n"
      "투자비율,실투율,자산군,보유금액의 SUM,수익액의 SUM,목표금액\r\n"
      "9%,9%,1.채권,\"₩17,260,017\",\"₩6,566,695\",\"₩17,260,017\"\r\n"
      "14%,14%,2.원자재,\"₩26,904,380\",\"-₩809,560\",\"₩26,904,380\"\r\n"
      "20%,20%,3.가상자산,\"₩40,152,756\",\"-₩17,162,590\",\"₩40,152,756\"\r\n"
      "3%,3%,4.리츠,\"₩6,666,153\",\"₩427,921\",\"₩6,666,153\"\r\n"
      "54%,54%,5.주식,\"₩107,509,308\",\"-₩2,188,496\",\"₩107,509,308\"\r\n"
      "100%,100%,총계,\"₩198,492,614\",\"-₩13,166,030\"\r\n";
  MemoryInput oversizedMetadataInput(oversizedMetadata);
  InvestmentSummary oversizedMetadataSummary{};
  assert(investmentParseCsv(oversizedMetadataInput, oversizedMetadataSummary));
  assert(oversizedMetadataSummary.assets[5].holdings == 198492614);
  assert(!oversizedMetadataSummary.stockGoals.valid);

  char formatted[16];
  investmentFormatMillions(parsed.assets[0].holdings, formatted, sizeof(formatted));
  assert(std::strcmp(formatted, "17.26") == 0);
  investmentFormatMillions(parsed.assets[2].profit, formatted, sizeof(formatted));
  assert(std::strcmp(formatted, "-17.16") == 0);
  investmentFormatMillions(12345, formatted, sizeof(formatted));
  assert(std::strcmp(formatted, "0.01") == 0);
  investmentFormatWholeMillions(11420164, formatted, sizeof(formatted));
  assert(std::strcmp(formatted, "11") == 0);
  investmentFormatWholeMillions(107721667, formatted, sizeof(formatted));
  assert(std::strcmp(formatted, "108") == 0);
  investmentFormatWholeMillions(1500000, formatted, sizeof(formatted));
  assert(std::strcmp(formatted, "2") == 0);
  assert(investmentFormatProfit(6566695, 17260017, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "+61.4%") == 0);
  assert(investmentFormatProfit(-13166030, 198492614, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "-6.2%") == 0);
  assert(investmentFormatHoldingShare(17260017, 198492614, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "17.26(9%)") == 0);
  assert(investmentFormatHoldingShare(198492614, 198492614, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "198.49(100%)") == 0);
  assert(investmentFormatSharePercent(17260017, 198492614, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "9%") == 0);
  assert(investmentFormatSharePercent(198492614, 198492614, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "100%") == 0);
  assert(!investmentFormatHoldingShare(100, 0, formatted, sizeof(formatted)));

  const char reorderedQuoted[] =
      "투자목표\n"
      "Title\n"
      "투자비율,실투율,비고,구분,비고,자산군,추가1,추가2,추가3,추가4,추가5,추가6,수익액의 SUM,보유금액의 SUM\n"
      "9%,9%,,채권,,1.채권,,,,,,,\"₩6,566,695\",\"₩17,260,017\"\n"
      "14%,14%,,원자재,,2.원자재,,,,,,,\"-₩809,560\",\"₩26,904,380\"\n"
      "20%,20%,,가상자산,,3.가상자산,,,,,,,\"-₩17,162,590\",\"₩40,152,756\"\n"
      "3%,3%,,리츠,,4.리츠,,,,,,,\"₩427,921\",\"₩6,666,153\"\n"
      "54%,54%,,주식,,5.주식,,,,,,,\"-₩2,188,496\",\"₩107,509,308\"\n"
      "100%,100%,,,,총계,,,,,,,\"-₩13,166,030\",\"₩198,492,614\"\n";
  MemoryInput reorderedInput(reorderedQuoted);
  InvestmentSummary reorderedSummary{};
  assert(investmentParseCsv(reorderedInput, reorderedSummary));
  assert(reorderedSummary.assets[0].holdings == 17260017);
  assert(reorderedSummary.assets[5].profit == -13166030);

  const char regionSheet[] =
      "투자자산,,,,안전자산\n"
      "상품,예상 수익액의 SUM,구분,자산군,추가1,추가2,추가3,추가4,금액의 SUM,예상수익률의 AVERAGE,,자산군,상품,금액의 SUM,예상수익액의 SUM,예상수익률의 AVERAGE\n"
      "Fund A,\"₩100,000\",글로벌,5.주식,,,,,\"₩1,000,000\",10.00%\n"
      "Fund B,\"-₩50,000\",,,,,,,\"₩2,000,000\",-2.50%\n"
      "Fund C,\"₩200,000\",미국,5.주식,,,,,\"₩3,000,000\",6.67%\n"
      "Fund D,\"-₩25,000\",중국,5.주식,,,,,\"₩1,500,000\",-1.67%\n"
      "Fund E,\"₩75,000\",한국,5.주식,,,,,\"₩2,500,000\",3.00%\n"
      ",,,총계,,,,,\"₩10,000,000\",\"₩300,000\"\n";
  MemoryInput regionInput(regionSheet);
  InvestmentRegionSummary regions{};
  assert(investmentParseRegionsCsv(regionInput, regions));
  assert(regions.valid);
  assert(regions.regions[0].holdings == 3000000);
  assert(regions.regions[0].profit == 50000);
  assert(regions.regions[1].holdings == 3000000);
  assert(regions.regions[2].profit == -25000);
  assert(regions.regions[3].holdings == 2500000);
  assert(regions.total.holdings == 10000000);
  assert(regions.total.profit == 300000);
  assert(investmentFormatHoldingShare(regions.regions[0].holdings,
      regions.total.holdings, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "3.00(30%)") == 0);
  assert(investmentFormatHoldingShare(regions.total.holdings,
      regions.total.holdings, formatted, sizeof(formatted)));
  assert(std::strcmp(formatted, "10.00(100%)") == 0);

  const char nativeRegionSheet[] =
      "투자자산,,,,,,,,안전자산\n"
      "자산군,구분,상품,금액의 SUM,예상수익액의 SUM,예상수익률의 AVERAGE\n"
      "5.주식,글로벌,Fund A,\"₩1,000,000\",\"₩100,000\",10.00%\n"
      ",,Fund B,\"₩2,000,000\",\"-₩50,000\",-2.50%\n"
      ",미국,Fund C,\"₩3,000,000\",\"₩200,000\",6.67%\n"
      ",중국,Fund D,\"₩1,500,000\",\"-₩25,000\",-1.67%\n"
      ",한국,Fund E,\"₩2,500,000\",\"₩75,000\",3.00%\n"
      "총계,,,\"₩10,000,000\",\"₩300,000\",3.00%\n";
  MemoryInput nativeRegionInput(nativeRegionSheet);
  InvestmentRegionSummary nativeRegions{};
  assert(investmentParseRegionsCsv(nativeRegionInput, nativeRegions));
  assert(nativeRegions.regions[0].holdings == 3000000);
  assert(nativeRegions.regions[0].profit == 50000);
  assert(nativeRegions.total.holdings == 10000000);
  assert(nativeRegions.total.profit == 300000);

  // All pages combine both accounts; ASSET includes cash and mixed assets.
  InvestmentSummary viewInvestment{};
  InvestmentRegionSummary viewRegions{};
  RetirementSummary viewRetirement{};
  assert(investmentDisplayPage(0, viewInvestment, viewRegions, viewRetirement).count == 0);
  viewInvestment.valid = viewInvestment.stockGoals.valid = true;
  viewInvestment.assets[0] = {17280457, 6556321};
  viewInvestment.assets[5] = {198764090, -15406280};
  assert(investmentDisplayPage(0, viewInvestment, viewRegions, viewRetirement).count == 0);
  assert(investmentDisplayPage(1, viewInvestment, viewRegions, viewRetirement).count == 0);
  assert(investmentDisplayPage(2, viewInvestment, viewRegions, viewRetirement).count == 0);
  viewRegions.valid = true;
  assert(investmentDisplayPage(1, viewInvestment, viewRegions, viewRetirement).count == 0);
  viewRetirement.valid = true;
  viewRetirement.assets[0] = {123072, 0};
  viewRetirement.assets[1] = {9739990, -71342};
  viewRetirement.assets[2] = {18413560, 247816};
  viewRetirement.assets[6] = {69532480, 5442931};
  auto assetPage = investmentDisplayPage(0, viewInvestment, viewRegions, viewRetirement);
  assert(assetPage.count == 7);
  assert(std::strcmp(assetPage.rows[0].label, "CASH") == 0);
  assert(assetPage.rows[0].holdings == 123072);
  assert(assetPage.rows[1].holdings == 27020447);
  assert(assetPage.rows[1].profit == 6484979 && assetPage.rows[1].hasProfit);
  assert(assetPage.rows[2].holdings == 18413560);
  assert(assetPage.total.holdings == 268296570);
  assert(assetPage.total.profit == -9963349);

  for (unsigned i = 0; i < 4; ++i) {
    viewRegions.regions[i] = {int32_t((i + 1) * 1000000), 10000};
    viewRetirement.regions[i].holdings = int32_t((i + 1) * 2000000);
  }
  viewRegions.total = {10000000, 40000};
  viewRetirement.regionTotal.holdings = 20000000;
  auto regionPage = investmentDisplayPage(1, viewInvestment, viewRegions, viewRetirement);
  assert(regionPage.count == 4 && regionPage.total.holdings == 30000000);
  for (unsigned i = 0; i < 4; ++i) {
    assert(regionPage.rows[i].holdings == int64_t((i + 1) * 3000000));
    assert(regionPage.rows[i].shareTotal == 30000000);
    assert(!regionPage.rows[i].hasProfit);  // No partial-account returns.
  }
  for (unsigned i = 0; i < 8; ++i) {
    viewInvestment.stockGoals.categories[i] = {int32_t((i + 1) * 1000000), 99};
  }
  viewInvestment.stockGoals.total.holdings = 36000000;
  for (unsigned i = 0; i < 5; ++i) {
    viewRetirement.stockGoals[i] = {int32_t((i + 1) * 10000000), 1};
  }
  viewRetirement.stockGoalTotal.holdings = 150000000;
  auto themePage = investmentDisplayPage(2, viewInvestment, viewRegions, viewRetirement);
  assert(themePage.count == 10 && themePage.total.holdings == 186000000);
  assert(std::strcmp(themePage.rows[1].label, "HLTH") == 0);
  assert(themePage.rows[1].holdings == 32000000);
  assert(themePage.rows[5].holdings == 46000000);
  assert(themePage.rows[7].holdings == 58000000);
  assert(themePage.rows[8].holdings == 10000000);
  assert(themePage.rows[9].holdings == 20000000);
  int64_t themeHoldings = 0;
  for (const auto &row : themePage.rows) {
    themeHoldings += row.holdings;
    assert(row.shareTotal == 186000000 && !row.hasProfit);
  }
  assert(themeHoldings == themePage.total.holdings);
  char combinedText[32];
  assert(investmentFormatSharePercent(themePage.rows[1].holdings,
      themePage.total.holdings, combinedText, sizeof(combinedText)));
  assert(std::strcmp(combinedText, "17%") == 0);  // Recomputed, never added rates.
  viewRegions.total.holdings = viewRetirement.regionTotal.holdings = INT32_MAX;
  viewRegions.regions[0].holdings = viewRetirement.regions[0].holdings = INT32_MAX;
  regionPage = investmentDisplayPage(1, viewInvestment, viewRegions, viewRetirement);
  assert(regionPage.total.holdings == 4294967294LL);
  assert(regionPage.rows[0].holdings == 4294967294LL);
  investmentFormatWholeMillions(regionPage.total.holdings, combinedText, sizeof(combinedText));
  assert(std::strcmp(combinedText, "4295") == 0);
  assert(investmentFormatSharePercent(regionPage.rows[0].holdings,
      regionPage.total.holdings, combinedText, sizeof(combinedText)));
  assert(std::strcmp(combinedText, "100%") == 0);
  assert(investmentDisplayPage(3, viewInvestment, viewRegions, viewRetirement).count == 0);
  viewInvestment.stockGoals.valid = false;
  assert(investmentDisplayPage(2, viewInvestment, viewRegions, viewRetirement).count == 0);
  assert(investmentDisplayPage(0, viewInvestment, viewRegions, viewRetirement).count == 7);

  // Verify continuous passes, both boundary pauses, static pages and wraparound.
  const uint8_t scrollRowCounts[] = {5, 7, 10};
  for (uint8_t rows : scrollRowCounts) {
    InvestmentPageCycle pages;
    uint32_t now = 1000;
    assert(!pages.advance(now, rows));
    assert(!pages.advance(now + 1499, rows));
    now += 1500;
    assert(!pages.advance(now, rows));
    for (unsigned pass = 0; pass < 2; ++pass) {
      for (unsigned row = 0; row < unsigned(rows - 4); ++row) {
        for (unsigned frame = 0; frame < 18; ++frame) {
          now += 80;
          assert(pages.advance(now, rows));
          assert(pages.page == 0);
        }
        assert(pages.scrollIndex == row + 1);
      }
      if (pass == 0) {
        now += 80;
        assert(pages.advance(now, rows));
        assert(pages.scrollIndex == 0 && pages.scrolling);
      }
    }
    assert(!pages.advance(now + 1499, rows));
    now += 1500;
    assert(pages.advance(now, rows));
    assert(pages.page == 1);
    // First call supplies the new page's row count.
    assert(!pages.advance(now, 4));
    assert(!pages.advance(now + 9999, 4));
    now += 10000;
    assert(pages.advance(now, 4));
    assert(pages.page == 2);
    assert(!pages.advance(now, 1));
    assert(!pages.advance(now + 9999, 1));
    now += 10000;
    assert(pages.advance(now, 1));
    assert(pages.page == 0);
  }
  InvestmentPageCycle changing;
  assert(!changing.advance(0, 0));
  assert(!changing.advance(9000, 4));
  assert(!changing.advance(18999, 4));
  assert(changing.advance(19000, 4));

  const char invalid[] =
      "투자목표\n"
      "투자비율,실투율,자산군,보유금액의 SUM,수익액의 SUM\n"
      "9%,9%,1.채권,\"₩17,260,017\",\"₩6,566,695\"\n"
      "100%,100%,총계,\"₩17,260,017\",\"₩6,566,695\"\n";
  MemoryInput invalidInput(invalid);
  InvestmentSummary unchanged{};
  assert(!investmentParseCsv(invalidInput, unchanged));
  assert(!unchanged.valid);

  const char malformedAmount[] =
      "투자목표\n"
      "투자비율,실투율,자산군,보유금액의 SUM,수익액의 SUM\n"
      "9%,9%,1.채권,\"₩17,26,017\",\"₩6,566,695\"\n";
  MemoryInput malformedInput(malformedAmount);
  assert(!investmentParseCsv(malformedInput, unchanged));
  return 0;
}
