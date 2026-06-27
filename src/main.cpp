#include <Arduino.h>
#include <Preferences.h>
#include "pins.h"
#include "config.h"
#include "network.h"
#include "dmx_output.h"
#include "artnet_handler.h"
#include "webserver.h"

static uint32_t btnHeldSince = 0;
static bool     btnWasLow    = false;

static void button_poll() {
    bool low = digitalRead(PIN_BUTTON) == LOW;
    if (low && !btnWasLow) {
        btnHeldSince = millis();
        btnWasLow    = true;
    } else if (!low) {
        btnWasLow = false;
    }
    // Long-press >3s: factory reset
    if (btnWasLow && (millis() - btnHeldSince) > 3000) {
        Serial.println("Factory reset!");
        Preferences prefs;
        prefs.begin(CFG_NS, false);
        prefs.clear();
        prefs.end();
        delay(200);
        ESP.restart();
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nDMX Controller starting...");

    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY, LOW);

    config_load(config_get());
    network_init();
    dmx_output_init();
    artnet_init();
    webserver_init();

    Serial.println("Ready.");
}

void loop() {
    network_poll();
    artnet_poll();
    webserver_poll();
    button_poll();
}
