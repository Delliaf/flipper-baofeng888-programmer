#pragma once
#include "baofeng_app.h"

bool baofeng_radio_read(BaofengApp* app);
bool baofeng_radio_write(BaofengApp* app);

uint32_t bcd_to_freq_10hz(const uint8_t bcd[4]);
void freq_10hz_to_bcd(uint32_t freq_10hz, uint8_t bcd[4]);

typedef enum {
    ToneTypeNone,
    ToneTypeCTCSS,
    ToneTypeDCS
} ToneType;

ToneType decode_tone(const uint8_t tone[2], uint16_t* value);
void encode_tone(ToneType type, uint16_t value, uint8_t tone[2]);

