#pragma once
#include "config.h"

void artnet_init();
void artnet_poll();
void artnet_rebind(const Config &c);
