#pragma once

// Athom Ethernet WLED ESP32 DMX Controller — V2 GPIO assignments

// User input
#define PIN_BUTTON      0

// Power relay
#define PIN_RELAY       2

// Addressable LED data outputs
#define PIN_DAT1        5   // default WS281X
#define PIN_DAT2        16
#define PIN_DAT3        4
#define PIN_DAT4        12

// DMX via MAX485
// GPIO13 → MAX485 DI  (UART TX)
// GPIO34 ← MAX485 RO  (UART RX, input-only pin)
// GPIO33 → MAX485 DE/RE (HIGH = drive/transmit)
#define PIN_DMX_TX      13
#define PIN_DMX_RX      34
#define PIN_DMX_EN      33

// I2S digital microphone (I2S PDM)
#define PIN_I2S_SD      35  // data (input-only)
#define PIN_I2S_WS      15  // word select / L/R clock

// Ethernet LAN8720A (RMII — remaining pins are fixed by ESP32 silicon)
// MDC=23, MDIO=18, CLK=GPIO17 (output mode) defined in platformio.ini
