#include "artnet_handler.h"
#include "dmx_output.h"
#include <WiFiUdp.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#define ARTNET_PORT 6454
#define SACN_PORT   5568
#define LOG_SIZE    15

static WiFiUDP udpArtNet;
static WiFiUDP udpSacn;
static uint8_t rxBuf[640];

// ---- Activity log ----------------------------------------------------------

struct LogEntry {
    uint32_t ms;
    uint8_t  proto;    // 0=ArtNet, 1=sACN
    uint32_t srcIp;
    uint16_t universe;
    uint16_t channels;
};

static LogEntry          g_log[LOG_SIZE];
static uint8_t           g_logHead  = 0;
static uint8_t           g_logCount = 0;
static SemaphoreHandle_t g_logMutex;

static void log_push(uint8_t proto, uint32_t srcIp, uint16_t universe, uint16_t channels) {
    xSemaphoreTake(g_logMutex, portMAX_DELAY);
    g_log[g_logHead] = { millis(), proto, srcIp, universe, channels };
    g_logHead        = (g_logHead + 1) % LOG_SIZE;
    if (g_logCount < LOG_SIZE) g_logCount++;
    xSemaphoreGive(g_logMutex);
}

static String ago_str(uint32_t now, uint32_t ms) {
    uint32_t d = now - ms;
    if (d < 1000) return String(d) + "ms";
    return String(d / 1000) + "." + String((d % 1000) / 100) + "s";
}

void artnet_log_json(String &out) {
    xSemaphoreTake(g_logMutex, portMAX_DELAY);
    uint8_t  count = g_logCount;
    uint8_t  head  = g_logHead;
    LogEntry copy[LOG_SIZE];
    memcpy(copy, g_log, sizeof(g_log));
    xSemaphoreGive(g_logMutex);

    uint32_t now = millis();
    out = "[";
    for (uint8_t i = 0; i < count; i++) {
        // walk backwards from most recent
        uint8_t idx = (head + LOG_SIZE - 1 - i) % LOG_SIZE;
        LogEntry &e = copy[idx];
        if (i > 0) out += ",";
        out += "{\"ago\":\"";
        out += ago_str(now, e.ms);
        out += "\",\"p\":\"";
        out += (e.proto == 0) ? "ArtNet" : "sACN";
        out += "\",\"ip\":\"";
        out += IPAddress(e.srcIp).toString();
        out += "\",\"u\":";
        out += e.universe;
        out += ",\"ch\":";
        out += e.channels;
        out += "}";
    }
    out += "]";
}

// ---- ArtNet parsing --------------------------------------------------------

static void parse_artnet(int len, uint32_t srcIp) {
    if (len < 18) return;
    if (memcmp(rxBuf, "Art-Net\0", 8) != 0) return;
    uint16_t opcode = rxBuf[8] | ((uint16_t)rxBuf[9] << 8);
    if (opcode != 0x5000) return;
    uint16_t universe = rxBuf[14] | ((uint16_t)(rxBuf[15] & 0x7F) << 8);
    if (universe != config_get().artUniverse) return;
    uint16_t dmxLen = ((uint16_t)rxBuf[16] << 8) | rxBuf[17];
    if (dmxLen > 512 || (18 + dmxLen) > (uint16_t)len) return;
    dmx_output_update(rxBuf + 18, dmxLen);
    log_push(0, srcIp, universe, dmxLen);
}

// ---- sACN E1.31 parsing ----------------------------------------------------

static const uint8_t ACN_ID[] = "ASC-E1.17\0\0\0";

static void parse_sacn(int len, uint32_t srcIp) {
    if (len < 126) return;
    if (rxBuf[0] != 0x00 || rxBuf[1] != 0x10) return;
    if (memcmp(rxBuf + 4, ACN_ID, 12) != 0) return;
    if (rxBuf[40] != 0x00 || rxBuf[41] != 0x00 ||
        rxBuf[42] != 0x00 || rxBuf[43] != 0x02) return;
    uint16_t universe = ((uint16_t)rxBuf[113] << 8) | rxBuf[114];
    if (universe != config_get().sacnUniverse) return;
    uint16_t propCount = ((uint16_t)rxBuf[123] << 8) | rxBuf[124];
    if (propCount < 1) return;
    if (rxBuf[125] != 0x00) return;
    uint16_t dmxLen = propCount - 1;
    if (dmxLen > 512 || (126 + dmxLen) > (uint16_t)len) return;
    dmx_output_update(rxBuf + 126, dmxLen);
    log_push(1, srcIp, universe, dmxLen);
}

// ---- Public API ------------------------------------------------------------

void artnet_init() {
    g_logMutex = xSemaphoreCreateMutex();
    const Config &c = config_get();

    if (c.protocol == 0 || c.protocol == 2) {
        udpArtNet.begin(ARTNET_PORT);
    }
    if (c.protocol == 1 || c.protocol == 2) {
        uint16_t u = c.sacnUniverse;
        udpSacn.beginMulticast(IPAddress(239, 255, u >> 8, u & 0xFF), SACN_PORT);
    }
}

void artnet_poll() {
    const Config &c = config_get();

    if (c.protocol == 0 || c.protocol == 2) {
        int len = udpArtNet.parsePacket();
        if (len > 0) {
            uint32_t src = (uint32_t)udpArtNet.remoteIP();
            len = udpArtNet.read(rxBuf, sizeof(rxBuf));
            parse_artnet(len, src);
        }
    }

    if (c.protocol == 1 || c.protocol == 2) {
        int len = udpSacn.parsePacket();
        if (len > 0) {
            uint32_t src = (uint32_t)udpSacn.remoteIP();
            len = udpSacn.read(rxBuf, sizeof(rxBuf));
            parse_sacn(len, src);
        }
    }
}

void artnet_rebind(const Config &c) {
    udpArtNet.stop();
    udpSacn.stop();
    artnet_init();
}
