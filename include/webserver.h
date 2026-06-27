#pragma once

void webserver_init();
void webserver_poll();  // call from loop() to process deferred reboot/rebind
