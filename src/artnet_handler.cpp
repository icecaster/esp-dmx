#include "artnet_handler.h"
#include "dmx_output.h"
#include <WiFiUdp.h>
#include <IPAddress.h>
#include <string.h>

#define ARTNET_PORT 6454
#define SACN_PORT   5568

static WiFiUDP udpArtNet;
static WiFiUDP udpSacn;
static uint8_t rxBuf[640];

// ---- ArtNet parsing --------------------------------------------------------

static void parse_artnet(int len) {
    if (len < 18) return;
    if (memcmp(rxBuf, "Art-Net\0", 8) != 0) return;
    uint16_t opcode = rxBuf[8] | ((uint16_t)rxBuf[9] << 8); // little-endian
    if (opcode != 0x5000) return;
    uint16_t universe = rxBuf[14] | ((uint16_t)(rxBuf[15] & 0x7F) << 8);
    if (universe != config_get().artUniverse) return;
    uint16_t dmxLen = ((uint16_t)rxBuf[16] << 8) | rxBuf[17]; // big-endian
    if (dmxLen > 512 || (18 + dmxLen) > (uint16_t)len) return;
    dmx_output_update(rxBuf + 18, dmxLen);
}

// ---- sACN E1.31 parsing ----------------------------------------------------

static const uint8_t ACN_ID[] = "ASC-E1.17\0\0\0"; // 12 bytes

static void parse_sacn(int len) {
    if (len < 126) return;
    // Preamble size must be 0x0010
    if (rxBuf[0] != 0x00 || rxBuf[1] != 0x10) return;
    // ACN Packet Identifier
    if (memcmp(rxBuf + 4, ACN_ID, 12) != 0) return;
    // Framing vector must be VECTOR_E131_DATA_PACKET (0x00000002)
    if (rxBuf[40] != 0x00 || rxBuf[41] != 0x00 ||
        rxBuf[42] != 0x00 || rxBuf[43] != 0x02) return;
    uint16_t universe = ((uint16_t)rxBuf[113] << 8) | rxBuf[114];
    if (universe != config_get().sacnUniverse) return;
    uint16_t propCount = ((uint16_t)rxBuf[123] << 8) | rxBuf[124];
    if (propCount < 1) return;
    if (rxBuf[125] != 0x00) return; // only null start code
    uint16_t dmxLen = propCount - 1;
    if (dmxLen > 512 || (126 + dmxLen) > (uint16_t)len) return;
    dmx_output_update(rxBuf + 126, dmxLen);
}

// ---- Public API ------------------------------------------------------------

void artnet_init() {
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
            len = udpArtNet.read(rxBuf, sizeof(rxBuf));
            parse_artnet(len);
        }
    }

    if (c.protocol == 1 || c.protocol == 2) {
        int len = udpSacn.parsePacket();
        if (len > 0) {
            len = udpSacn.read(rxBuf, sizeof(rxBuf));
            parse_sacn(len);
        }
    }
}

void artnet_rebind(const Config &c) {
    udpArtNet.stop();
    udpSacn.stop();
    artnet_init();
}
