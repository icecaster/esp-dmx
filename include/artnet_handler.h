#pragma once
#include "config.h"
#include <Arduino.h>

void artnet_init();
void artnet_poll();
void artnet_rebind(const Config &c);
void artnet_log_json(String &out);
