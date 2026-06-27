#pragma once
#include <stdint.h>

void dmx_output_init();
void dmx_output_update(const uint8_t *data, uint16_t len);
