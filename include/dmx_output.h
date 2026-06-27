#pragma once
#include <stdint.h>
#include <Arduino.h>

void dmx_output_init();
void dmx_output_update(const uint8_t *data, uint16_t len);
void dmx_output_set(uint16_t channel, uint8_t value);
void dmx_output_clear();
void dmx_output_dump(String &out);
