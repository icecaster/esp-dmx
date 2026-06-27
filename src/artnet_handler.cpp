#include "artnet_handler.h"
#include "dmx_output.h"
#include "network.h"
#include <WiFiUdp.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_mac.h>
#include <string.h>

#define ARTNET_PORT 6454
#define SACN_PORT   5568
#define LOG_SIZE    15

static WiFiUDP udpArtNet;
static WiFiUDP udpSacn;
static uint8_t rxBuf[640];
static bool    g_announced = false;

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
    out.reserve(count * 80 + 4);
    out = "[";
    for (uint8_t i = 0; i < count; i++) {
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

// ---- ArtPollReply ----------------------------------------------------------

static void send_artpollreply(IPAddress dest) {
    uint8_t pkt[239];
    memset(pkt, 0, sizeof(pkt));

    memcpy(pkt, "Art-Net\0", 8);
    pkt[8] = 0x00;   // OpCode lo: ArtPollReply = 0x2100
    pkt[9] = 0x21;

    IPAddress ip = network_eth_connected() ? network_eth_ip() : IPAddress(192, 168, 4, 1);
    pkt[10] = ip[0]; pkt[11] = ip[1]; pkt[12] = ip[2]; pkt[13] = ip[3];

    pkt[14] = 0x36;  // Port 6454 lo
    pkt[15] = 0x19;  // Port 6454 hi

    pkt[16] = 0x00;  // VersInfoH
    pkt[17] = 0x01;  // VersInfoL

    const Config &c = config_get();
    uint16_t u = c.artUniverse;
    pkt[18] = (u >> 8) & 0x7F;  // NetSwitch
    pkt[19] = (u >> 4) & 0x0F;  // SubSwitch

    pkt[20] = 0xFF; pkt[21] = 0xFF;  // Oem: not registered

    // Status1: indicator unknown, no RDM, firmware booted from flash
    pkt[23] = 0x00;

    strncpy((char*)pkt + 26, c.hostname, 17);  // ShortName (18 bytes)
    snprintf((char*)pkt + 44, 64, "ArtNet/sACN DMX Controller %s", c.hostname);  // LongName
    snprintf((char*)pkt + 108, 64, "#0001 [0000] OK");  // NodeReport

    pkt[172] = 0x00;  // NumPortsHi
    pkt[173] = 0x01;  // NumPortsLo: 1 port
    pkt[174] = 0x80;  // PortTypes[0]: output, DMX512
    pkt[182] = 0x80;  // GoodOutputA[0]: data being transmitted
    pkt[190] = u & 0x0F;  // SwOut[0]: universe low nibble

    pkt[194] = 100;   // AcnPriority
    pkt[200] = 0x00;  // Style: StNode

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    memcpy(pkt + 201, mac, 6);

    pkt[207] = ip[0]; pkt[208] = ip[1];
    pkt[209] = ip[2]; pkt[210] = ip[3];
    pkt[211] = 1;  // BindIndex

    // Status2: supports 15-bit port address (bit3), DHCP capable (bit2)
    pkt[212] = 0x08 | (c.ethDhcp ? 0x06 : 0x00);

    udpArtNet.beginPacket(dest, ARTNET_PORT);
    udpArtNet.write(pkt, sizeof(pkt));
    udpArtNet.endPacket();
}

// ---- ArtNet parsing --------------------------------------------------------

static void parse_artnet(int len, uint32_t srcIp) {
    if (len < 10) return;
    if (memcmp(rxBuf, "Art-Net\0", 8) != 0) return;
    uint16_t opcode = rxBuf[8] | ((uint16_t)rxBuf[9] << 8);

    if (opcode == 0x2000) {  // ArtPoll
        send_artpollreply(IPAddress(srcIp));
        return;
    }

    if (opcode != 0x5000) return;  // Not ArtDmx
    if (len < 18) return;
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

    // Broadcast an unsolicited ArtPollReply once ETH has an IP so controllers
    // that scan on startup discover us without needing to send an ArtPoll first.
    if (!g_announced && network_eth_connected()) {
        g_announced = true;
        send_artpollreply(IPAddress(255, 255, 255, 255));
    }

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
    g_announced = false;
    artnet_init();
}
