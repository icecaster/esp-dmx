#include "dmx_output.h"
#include "config.h"
#include "pins.h"
#include <esp_dmx.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <string.h>
#include <algorithm>

#define DMX_PORT DMX_NUM_1

static uint8_t           g_dmxBuf[513] = {0}; // byte 0 = start code 0x00
static SemaphoreHandle_t g_dmxMutex;

static void dmx_output_task(void *) {
    uint8_t localBuf[513];
    for (;;) {
        xSemaphoreTake(g_dmxMutex, portMAX_DELAY);
        memcpy(localBuf, g_dmxBuf, 513);
        xSemaphoreGive(g_dmxMutex);

        dmx_write(DMX_PORT, localBuf, 513);
        dmx_send_num(DMX_PORT, 513);
        dmx_wait_sent(DMX_PORT, pdMS_TO_TICKS(50));
    }
}

void dmx_output_init() {
    g_dmxMutex = xSemaphoreCreateMutex();

    // Permanently assert DE HIGH — we only ever transmit, never receive.
    pinMode(PIN_DMX_EN, OUTPUT);
    digitalWrite(PIN_DMX_EN, HIGH);

    dmx_config_t cfg = DMX_CONFIG_DEFAULT;
    bool ok = dmx_driver_install(DMX_PORT, &cfg, nullptr, 0);
    Serial.printf("[dmx] driver_install=%d\n", ok);

    // DMX_PIN_NO_CHANGE for RTS — GPIO33 (DE) is driven manually above
    bool pin_ok = dmx_set_pin(DMX_PORT, PIN_DMX_TX, PIN_DMX_RX, DMX_PIN_NO_CHANGE);
    Serial.printf("[dmx] set_pin=%d TX=%d RX=%d\n", pin_ok, PIN_DMX_TX, PIN_DMX_RX);

    // Stack 4KB: localBuf[513] + task overhead + dmx library calls
    xTaskCreatePinnedToCore(dmx_output_task, "dmx_tx", 4096, nullptr, 10, nullptr, 1);
}

void dmx_output_set(uint16_t channel, uint8_t value) {
    if (channel < 1 || channel > 512) return;
    xSemaphoreTake(g_dmxMutex, portMAX_DELAY);
    g_dmxBuf[channel] = value;
    xSemaphoreGive(g_dmxMutex);
}

void dmx_output_clear() {
    xSemaphoreTake(g_dmxMutex, portMAX_DELAY);
    memset(g_dmxBuf + 1, 0, 512);
    xSemaphoreGive(g_dmxMutex);
}

// Returns channels 1-32 as a compact JSON array (index 0 = ch1).
// Keeping the response small prevents heap fragmentation from repeated polling.
void dmx_output_dump(String &out) {
    uint8_t snap[32];
    xSemaphoreTake(g_dmxMutex, portMAX_DELAY);
    memcpy(snap, g_dmxBuf + 1, 32); // slots 1-32 = DMX channels 1-32
    xSemaphoreGive(g_dmxMutex);

    out.reserve(32 * 4 + 4); // pre-allocate — prevents repeated reallocs
    out = "[";
    for (int i = 0; i < 32; i++) {
        if (i > 0) out += ',';
        out += snap[i];
    }
    out += ']';
}

void dmx_output_update(const uint8_t *data, uint16_t len) {
    const Config &cfg = config_get();
    uint16_t offset = cfg.dmxStartCh;           // 1-based; slot 0 is start code
    uint16_t count  = std::min(len, cfg.dmxCount);
    // Clamp so we never write past the end of the 513-byte buffer
    if (offset + count > 513) count = 513 - offset;

    xSemaphoreTake(g_dmxMutex, portMAX_DELAY);
    memcpy(g_dmxBuf + offset, data, count);
    xSemaphoreGive(g_dmxMutex);
}
