#include "config.h"
#include <Preferences.h>

static Config g_config;

Config &config_get() { return g_config; }

void config_defaults(Config &c) {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(c.hostname, sizeof(c.hostname), "dmx-%04x", (uint16_t)(mac & 0xFFFF));
    c.ethDhcp     = true;
    c.ethIp       = 0;
    c.ethSubnet   = 0;
    c.ethGw       = 0;
    c.ethDns      = 0;
    c.protocol    = 0;
    c.artUniverse = 0;
    c.sacnUniverse = 1;
    c.dmxStartCh  = 1;
    c.dmxCount    = 512;
}

bool config_load(Config &c) {
    config_defaults(c);
    Preferences prefs;
    if (!prefs.begin(CFG_NS, true)) return false;
    prefs.getString("hostname",  c.hostname, sizeof(c.hostname));
    c.ethDhcp      = prefs.getBool("eth_dhcp",   c.ethDhcp);
    c.ethIp        = prefs.getUInt("eth_ip",      c.ethIp);
    c.ethSubnet    = prefs.getUInt("eth_subnet",  c.ethSubnet);
    c.ethGw        = prefs.getUInt("eth_gw",      c.ethGw);
    c.ethDns       = prefs.getUInt("eth_dns",     c.ethDns);
    c.protocol     = prefs.getUChar("protocol",   c.protocol);
    c.artUniverse  = prefs.getUShort("art_univ",  c.artUniverse);
    c.sacnUniverse = prefs.getUShort("sacn_univ", c.sacnUniverse);
    c.dmxStartCh   = prefs.getUShort("dmx_start", c.dmxStartCh);
    c.dmxCount     = prefs.getUShort("dmx_count", c.dmxCount);
    prefs.end();
    return true;
}

bool config_save(const Config &c) {
    Preferences prefs;
    if (!prefs.begin(CFG_NS, false)) return false;
    prefs.putString("hostname",  c.hostname);
    prefs.putBool("eth_dhcp",    c.ethDhcp);
    prefs.putUInt("eth_ip",      c.ethIp);
    prefs.putUInt("eth_subnet",  c.ethSubnet);
    prefs.putUInt("eth_gw",      c.ethGw);
    prefs.putUInt("eth_dns",     c.ethDns);
    prefs.putUChar("protocol",   c.protocol);
    prefs.putUShort("art_univ",  c.artUniverse);
    prefs.putUShort("sacn_univ", c.sacnUniverse);
    prefs.putUShort("dmx_start", c.dmxStartCh);
    prefs.putUShort("dmx_count", c.dmxCount);
    prefs.end();
    return true;
}
