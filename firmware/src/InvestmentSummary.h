#pragma once

#include <TFT_eSPI.h>
#include <cstdint>

bool investmentPoll(uint32_t now);
bool investmentAdvance(uint32_t now);
void investmentDraw(TFT_eSPI &display);
bool investmentTargetValid();
bool investmentTargetError();
int investmentTargetHttpCode();
uint32_t investmentTargetAgeSeconds();
bool investmentStockGoalsValid();
bool investmentStockGoalsError();
bool investmentRetirementValid();
bool investmentRetirementError();
int investmentRetirementHttpCode();
uint32_t investmentRetirementAgeSeconds();
bool investmentRegionsValid();
bool investmentRegionsError();
int investmentRegionsHttpCode();
uint32_t investmentRegionsAgeSeconds();
