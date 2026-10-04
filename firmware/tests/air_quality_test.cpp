#include <cassert>
#include <fstream>
#include <iostream>
#include <cstring>
#include "SeoulAirQuality.h"
#include "SeoulWeatherData.h"
#include "PageCycle.h"

bool parse(const char *json, SeoulAirQuality &out) {
  StaticJsonDocument<256> filter; filterAirQuality(filter);
  DynamicJsonDocument doc(kAirQualityJsonCapacity);
  const auto error=deserializeJson(doc,json,DeserializationOption::Filter(filter));
  return !error && decodeAirQuality(doc["hourly"],out);
}

int main(int argc,char **argv) {
  const int pm25[]={0,15,16,35,36,75,76};
  const int pm10[]={0,30,31,80,81,150,151};
  const DustGrade expected[]={DustGrade::Good,DustGrade::Good,DustGrade::Normal,
      DustGrade::Normal,DustGrade::Bad,DustGrade::Bad,DustGrade::VeryBad};
  for (unsigned i=0;i<7;++i) {
    assert(dustGrade(pm25[i],true)==expected[i]);
    assert(dustGrade(pm10[i],false)==expected[i]);
  }
  ForecastDate date;
  assert(parseForecastDate("2026-10-05",date));
  assert(!strcmp(forecastWeekdayName(date),"MON"));
  assert(!strcmp(forecastWeekdayName(nextForecastDate(nextForecastDate(date))),"WED"));
  assert(!parseForecastDate("2026-02-29",date));
  assert(parseForecastDate("2024-02-28",date));
  char iso[11]; formatForecastDate(iso,nextForecastDate(date)); assert(!strcmp(iso,"2024-02-29"));
  assert(parseForecastDate("2026-12-31",date));
  formatForecastDate(iso,nextForecastDate(date)); assert(!strcmp(iso,"2027-01-01"));

  SeoulAirQuality out{};
  // Independent PM maxima, null hours, missing middle day, year rollover.
  const char *sample=R"({"ignored":[1,2,3],"hourly":{"time":["2026-12-31T00:00","2026-12-31T23:00","2027-01-01T00:00","2027-01-02T00:00","2027-01-02T01:00"],"pm2_5":[18.4,50.6,null,0,15.5],"pm10":[100,32,null,151.2,null]}})";
  assert(parse(sample,out));
  assert(out.days[0].pm25==51 && out.days[0].pm10==100);
  assert(out.days[0].pm25Valid && out.days[0].pm10Valid);
  assert(!out.days[1].pm25Valid && !out.days[1].pm10Valid);
  assert(!strcmp(out.days[1].date,"2027-01-01"));
  assert(out.days[2].pm25==16 && out.days[2].pm10==151);
  assert(parse(R"({"hourly":{"time":["2026-10-05T00:00"],"pm2_5":[0],"pm10":[null]}})",out));
  assert(out.days[0].pm25Valid && out.days[0].pm25==0 && !out.days[0].pm10Valid);
  assert(!out.days[1].pm25Valid && !out.days[2].pm10Valid);
  assert(parse(R"({"hourly":{"time":["2026-10-05T00:00","2026-10-07T00:00"],"pm2_5":[true,"18"],"pm10":[-1,1e40]}})",out));
  for (auto &day:out.days) assert(!day.pm25Valid && !day.pm10Valid);
  const char *invalid[]={
      R"({"hourly":{"time":["2026-10-05T00:00"],"pm2_5":[18],"pm10":[]}})",
      R"({"hourly":{"time":["2026-02-29T00:00"],"pm2_5":[18],"pm10":[32]}})",
      R"({"hourly":{"time":["2026-10-05T24:00"],"pm2_5":[18],"pm10":[32]}})",
      R"({"current":{"pm2_5":18,"pm10":32}})","{bad json"
  };
  for (const char *json:invalid) {
    out.days[0].pm25=123;
    assert(!parse(json,out)); assert(out.days[0].pm25==123);
  }
  PageCycle cycle; cycle.begin(100);
  assert(!cycle.advance(12099)); assert(cycle.advance(12100));
  assert(cycle.page==ForecastPage::AirQuality);
  assert(!cycle.advance(20099)); assert(cycle.advance(20100));
  assert(cycle.page==ForecastPage::Weather);
  cycle.begin(UINT32_MAX-1000);
  assert(!cycle.advance(10998)); assert(cycle.advance(10999));
  assert(cycle.page==ForecastPage::AirQuality);

  if (argc==3) {
    std::ifstream air(argv[1]); assert(air.good());
    StaticJsonDocument<256> filter; filterAirQuality(filter);
    DynamicJsonDocument doc(kAirQualityJsonCapacity);
    assert(!deserializeJson(doc,air,DeserializationOption::Filter(filter)));
    assert(doc.size()==1 && doc["hourly"].size()==3);
    assert(doc["hourly"]["time"].size()==72);
    assert(decodeAirQuality(doc["hourly"],out));
    for (auto &day:out.days) {
      assert(day.pm25Valid && day.pm10Valid);
      std::cout << day.date << " MAX PM2.5=" << day.pm25 << " PM10=" << day.pm10 << '\n';
    }
    std::cout << "Filtered air JSON memory=" << doc.memoryUsage() << '/' << doc.capacity() << '\n';
    std::ifstream weather(argv[2]); assert(weather.good());
    StaticJsonDocument<256> wf; filterWeather(wf);
    DynamicJsonDocument wd(2048);
    assert(!deserializeJson(wd,weather,DeserializationOption::Filter(wf)));
    assert(wd.size()==1 && wd["daily"].size()==5);
    SeoulWeatherDay days[3]{};
    assert(decodeWeather(wd["daily"],days));
    // Missing dates and null temperatures must fail without publishing.
    wd["daily"]["temperature_2m_max"][1]=nullptr;
    days[0].weatherCode=123;
    assert(!decodeWeather(wd["daily"],days)); assert(days[0].weatherCode==123);
    std::cout << "PASS: real Weather daily JSON and invalid value rejection\n";
  }
  std::cout << "PASS: grades, dates, MAX grouping, missing values, page timings and rollover\n";
}
