# SD PRO investment-summary firmware

The separate `sdpro-investment` profile rotates through three pages:
**ASSET**, **STOCK REGION**, and **STOCK THEME**. Every title, value, total
and status message uses the same 18px bold font. Four single-line rows show
NAME, SHARE, VALUE and RETURN from left to right, without column headings.
Titles are cyan and each page's TOTAL remains fixed in yellow at the bottom.
Additional rows scroll upward within a clipped body area; page transitions
clear the previous section without full-screen scroll flicker.

ASSET combines both accounts' assets: CASH, BONDS, MIXED, MATER, CRYPT,
REITS and STOCK. Holdings and profit use 64-bit addition, with allocation
recalculated from unrounded amounts. Values round to whole millions of KRW;
positive values and allocations that would round to zero use `<1` and `<1%`.
Rows are independently rounded, so displayed integer percentages can sum to
99% or 101%; the TOTAL is calculated from unrounded amounts.

STOCK REGION and STOCK THEME include only the asset class **stock** in both
accounts. Retirement mixed funds remain in MIXED on ASSET and are excluded
from both stock pages. The ASSET STOCK row and both stock-page totals have
identical holdings and profit for a given pair of cached responses. RETURN
uses combined profit divided by combined cost (`holdings - profit`), rounded
to one decimal; it never averages the accounts' rates. Nonpositive cost shows
`--`. Positive returns are red, negative returns blue.

General investment assets, stock regions and stock themes are parsed from the
same goal-sheet response. The retirement goal sheet supplies only the live
product-to-theme classification; the active DC, IRP and personal-pension
instrument tables supply all retirement amounts and profit in one response.
Closed-position columns are excluded. Stock rows are grouped by region and
by their exact source product classification. Matching healthcare, market
and technology themes are merged across accounts; finance/value and
value/dividend remain distinct source categories.

The bounded parser hashes complete product names before truncating display
fields, including long pipe-separated source lists. Ambiguous, unknown or
oversized product mappings are rejected rather than guessed. A failed parse
retains the previous complete account snapshot. Each active table's published
total is checked against its rows, allowing only the integer rounding error
bound from rounding each position independently. Screen totals use summed
position amounts, keeping every screen internally consistent. General-investment
region/theme rounding residuals are limited to their mathematical row-rounding
bounds and assigned to the largest bucket so their totals match the canonical
stock amount and profit. Larger discrepancies fail validation.

## Build and install

From the repository root, run `make build-firmwares`. The image is exported as
`dist/sdpro-investment.bin`; a focused build is `pio run -e sdpro-investment`
from `firmware/`. The profile uses the same SD PRO ESP8266 board, display pins,
flash layout and network/OTA recovery path as the weather and ticker images.
No upload is performed by building the image.

Open the shared device portal at `/` to configure Wi-Fi and firmware updates.
The display fetches the investment goals worksheet, plus retirement goals and
instrument-detail worksheets, after Wi-Fi connects. Each section with more than four rows pauses for 1.5 seconds, scrolls its full
list twice without row or inter-pass pauses, then pauses for 1.5 seconds before
advancing. Each pass starts with the first four rows and ends with the last four;
the second pass returns directly to the first window. Pages with four or fewer
rows remain still for 10 seconds. Titles and totals stay fixed.
Each account refreshes every six hours and retries failures with a capped
backoff of up to five minutes. A failed refresh retains its last successful
values; incomplete stock-goal data is retried rather than counted as a
successful refresh. `/api/v1/status` reports the individual fetch states as
`investment*`, `investmentRegions*`, and `investmentRetirement*` fields.
`investmentParseFlags` records the last investment CSV parse (bit 0: completed,
bit 1: assets valid, bit 2: themes valid; 7 means all stages succeeded).
Requests are synchronous with a twelve-second timeout, so a slow connection can
briefly delay local web and OTA handling.

## Data source and limitations

The general-investment goal worksheet (`gid=827857148`) supplies its asset,
asset/region detail and stock-theme sections. Column names locate holdings and
profit regardless of reordering. The old separate general-investment detail
request is removed to avoid comparing different live snapshots.

The retirement workbook supplies its goal worksheet (`gid=827857148`) for
classification and instrument detail worksheet (`gid=379347683`) for active
holdings/profit. At most 64 unique product classifications and 32 products
per theme list are retained, without keeping the raw product list in RAM.
The CSV bounds remain 16 KiB per response, 24 fields and 2048 bytes per row.

The status API exposes exact won-denominated totals for audit:
`investmentCombinedAssetsHoldings/Profit`,
`investmentCombinedRegionsHoldings/Profit`, and
`investmentCombinedThemesHoldings/Profit`. These fields appear after both
accounts are valid; the two stock totals should agree exactly.

### 가독성 개선

승인된 시안처럼 ASSET / STOCK REGION / STOCK THEME 세 화면을 표시합니다.
제목·본문·합계·상태 메시지 모두 18px이며, 각 항목을 한 줄로 표시합니다.
ASSET은 두 계좌의 전체 자산을 합산하며, 주식 지역·테마에는 각 계좌의 주식만 포함합니다.
혼합자산은 ASSET의 MIXED에만 표시하고, 모든 화면의 RETURN은 합산 수익액으로 계산합니다.

### 스크롤 및 페이지 전환

- 5줄 이상: 시작 1.5초 정지 → 전체 목록 2회 연속 스크롤 → 끝 1.5초 정지 → 다음 페이지.
- 행 사이와 반복 사이에는 정지하지 않으며, 두 번째 반복은 첫 4줄로 즉시 복귀한다.
- 4줄 이하: 스크롤 없이 10초 표시한 뒤 다음 페이지로 이동한다.
- 제목과 TOTAL은 고정하며 스크롤 속도는 기존과 동일하다.
