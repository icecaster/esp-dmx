#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>
#include "pins.h"

// LAN8720A ethernet config (mirrors build flags in platformio.ini)
#define ETH_ADDR        1
#define ETH_POWER       -1
#define ETH_MDC         23
#define ETH_MDIO        18
#define ETH_CLK         ETH_CLOCK_GPIO17_OUT

static bool eth_connected = false;

void onEthEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            Serial.println("ETH: started");
            ETH.setHostname("dmx-controller");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("ETH: link up");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.print("ETH: IP ");
            Serial.println(ETH.localIP());
            eth_connected = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH: link down");
            eth_connected = false;
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("ETH: stopped");
            eth_connected = false;
            break;
        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    WiFi.onEvent(onEthEvent);
    ETH.begin(ETH_ADDR, ETH_POWER, ETH_MDC, ETH_MDIO, ETH_PHY_LAN8720, ETH_CLK);

    Serial.println("Setup complete");
}

void loop() {
    // application logic here
}
