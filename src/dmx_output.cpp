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
        dmx_wait_sent(DMX_PORT, pdMS_TO_TICKS(30));
    }
}

void dmx_output_init() {
    g_dmxMutex = xSemaphoreCreateMutex();

    dmx_config_t cfg = DMX_CONFIG_DEFAULT;
    dmx_driver_install(DMX_PORT, &cfg, nullptr, 0);
    dmx_set_pin(DMX_PORT, PIN_DMX_TX, PIN_DMX_RX, PIN_DMX_EN);

    xTaskCreatePinnedToCore(dmx_output_task, "dmx_tx", 2048, nullptr, 10, nullptr, 1);
}

void dmx_output_update(const uint8_t *data, uint16_t len) {
    const Config &cfg = config_get();
    uint16_t count  = std::min(len, cfg.dmxCount);
    uint16_t offset = cfg.dmxStartCh; // 1-based; slot 0 is start code

    xSemaphoreTake(g_dmxMutex, portMAX_DELAY);
    memcpy(g_dmxBuf + offset, data, count);
    xSemaphoreGive(g_dmxMutex);
}
