#pragma once
#ifdef SDPRO_TICKER
#include <ESP8266WebServer.h>
#include <TFT_eSPI.h>
void tickerLoad(bool filesystemReady);
void tickerRoutes(ESP8266WebServer &server, bool (*authorized)());
bool tickerAdvance(uint32_t now);
bool tickerPoll(uint32_t now);
void tickerDraw(TFT_eSPI &display);
#endif
