#pragma once
#include <stdbool.h>
/* Dedicated diagnostic-console context only; uses that task's notification.
 * Timed edges run on the device, independently of USB acknowledgement latency. */
bool zion_test_button_pulse(unsigned key,unsigned milliseconds,void (*dispatch)(unsigned,unsigned));
