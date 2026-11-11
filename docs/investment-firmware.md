# SD PRO investment-summary firmware

The separate `sdpro-investment` profile rotates between two account pages:
**INVESTMENT** and **RETIREMENT**. Each page combines its asset classes,
regional holdings and stock-goal allocations in one bottom-to-top scrolling
list. Four rows scroll at a time; the account's asset total remains fixed at
the bottom. There are no table rules, and category labels are abbreviated to
five characters or fewer.

Amounts are rounded to whole millions of KRW. SHARE uses the published
allocation rate for goals and the corresponding account total for assets and
regions. Available cost-basis returns (`P/L / (holdings - P/L)`) are red when
positive and blue when negative. Return is shown as `--` where the source does
not provide profit data.

## Build and install

From the repository root, run `make build-firmwares`. The image is exported as
`dist/sdpro-investment.bin`; a focused build is `pio run -e sdpro-investment`
from `firmware/`. The profile uses the same SD PRO ESP8266 board, display pins,
flash layout and network/OTA recovery path as the weather and ticker images.
No upload is performed by building the image.

Open the shared device portal at `/` to configure Wi-Fi and firmware updates.
The display fetches the investment goals and investment details worksheets,
plus the retirement goals worksheet, after Wi-Fi connects. Each account stays
on screen until its full list has scrolled, then the other account appears.
Each worksheet refreshes every six hours and retries failures with a capped
backoff of up to five minutes. A failed refresh retains its last successful
values; incomplete stock-goal data is retried rather than counted as a
successful refresh. `/api/v1/status` reports the individual fetch states as
`investment*`, `investmentRegions*`, and `investmentRetirement*` fields.
Requests are synchronous with a six-second timeout, so a slow connection can
briefly delay local web and OTA handling.

## Data source and limitations

The firmware requests the published **Investment Goals** worksheet
(`gid=827857148`), including its **Stock Detailed Investment Goals** section,
and the published **Investment Details** worksheet (`gid=379347683`) as CSV.
It discovers the required columns from their headers rather than relying on
fixed column numbers, tolerates reordered or additional columns, and aggregates
the listed stock positions by region. The parser is streamed with a 16 KiB
response limit; it does not buffer a complete worksheet. It rejects incomplete
or malformed values instead of replacing cached data.

Retirement pages use the published **Investment Goals** worksheet
(`gid=827857148`) from the retirement workbook. It provides the retirement asset
summary, stock-region totals and detailed stock-goal allocations. The three
retirement pages display asset P/L, regional stock allocation and detailed
stock-category allocation, respectively. If the retirement worksheet omits a
zero-valued asset category, the firmware treats it as zero only when the
reported asset total still matches the parsed categories.

Both workbooks are publicly published. Anyone with access to their published
URLs may be able to read their contents; do not use this build with sheets that
should remain private. The ESP8266 HTTPS client follows the Google redirect
but, like the existing firmware's public-data clients, does not authenticate
the server certificate. The data is informational, not financial advice. The
firmware makes read-only requests and does not send holdings back to Google;
Google still receives ordinary connection metadata such as the network IP
address.

The image is dedicated to these investment and retirement summaries and does
not include weather pages or the Yahoo ticker editor.
