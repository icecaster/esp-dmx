#include "network.h"
#include "config.h"
#include "pins.h"
#include <WiFi.h>
#include <ETH.h>
#include <ESPmDNS.h>

static bool      g_ethConnected  = false;
static IPAddress g_ethIp;
static volatile bool g_mdnsRestart = false;

static void startMdns() {
    const Config &c = config_get();
    MDNS.end();
    if (MDNS.begin(c.hostname)) {
        MDNS.addService("http",   "tcp", 80);
        MDNS.addService("artnet", "udp", 6454);
        MDNS.addService("e131",   "udp", 5568);
        Serial.printf("mDNS: %s.local\n", c.hostname);
    } else {
        Serial.println("mDNS: begin() failed");
    }
}

static void onEthEvent(WiFiEvent_t event) {
    const Config &c = config_get();
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ETH.setHostname(c.hostname);
            Serial.println("ETH: started");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("ETH: link up");
            if (!c.ethDhcp) {
                ETH.config(IPAddress(c.ethIp),
                           IPAddress(c.ethGw),
                           IPAddress(c.ethSubnet),
                           IPAddress(c.ethDns));
            }
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            g_ethIp        = ETH.localIP();
            g_ethConnected = true;
            Serial.print("ETH: IP ");
            Serial.println(g_ethIp);
            // Restart mDNS from the main loop (not here in the event callback)
            // so mdns_init() sees both AP and ETH netifs active.
            g_mdnsRestart = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            g_ethConnected = false;
            Serial.println("ETH: link down");
            break;
        case ARDUINO_EVENT_ETH_STOP:
            g_ethConnected = false;
            Serial.println("ETH: stopped");
            break;
        default:
            break;
    }
}

void network_init() {
    const Config &c = config_get();

    // AP + STA mode required for AP + Ethernet coexistence on IDF 4.4
    WiFi.mode(WIFI_AP_STA);

    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));
    WiFi.softAP(c.hostname);
    Serial.print("AP: SSID=");
    Serial.print(c.hostname);
    Serial.println("  IP=192.168.4.1");

    WiFi.onEvent(onEthEvent);
    ETH.begin(1, -1, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);

    // Start mDNS now for AP coverage. The IDF mDNS component also registers
    // its own handler for IP_EVENT_ETH_GOT_IP, so it will automatically enable
    // on ETH when that event fires — but we also force a restart via network_poll()
    // to be safe (some IDF 4.4 versions miss the ETH interface on init).
    startMdns();
}

void network_poll() {
    if (g_mdnsRestart) {
        g_mdnsRestart = false;
        // Reinitialise mDNS now that ETH has an IP. Calling from loop() (main task)
        // rather than the event callback avoids potential event-loop mutex conflicts.
        startMdns();
    }
}

bool      network_eth_connected() { return g_ethConnected; }
IPAddress network_eth_ip()        { return g_ethIp; }
