#pragma once
#include <Arduino.h>

#define CFG_NS "dmx-ctrl"

struct Config {
    char     hostname[32];
    bool     ethDhcp;
    uint32_t ethIp;
    uint32_t ethSubnet;
    uint32_t ethGw;
    uint32_t ethDns;
    uint8_t  protocol;      // 0=ArtNet, 1=sACN, 2=Both
    uint16_t artUniverse;   // 0-32767
    uint16_t sacnUniverse;  // 1-63999
    uint16_t dmxStartCh;    // 1-based slot offset
    uint16_t dmxCount;      // slots to output
};

Config &config_get();
void    config_defaults(Config &c);
bool    config_load(Config &c);
bool    config_save(const Config &c);
