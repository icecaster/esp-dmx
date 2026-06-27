#pragma once
#include <IPAddress.h>

void      network_init();
bool      network_eth_connected();
IPAddress network_eth_ip();
