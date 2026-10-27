#pragma once
static inline void vTaskDelay(unsigned ticks) { (void)ticks; }
static inline void *xTaskGetCurrentTaskHandle(void) {return (void *)1;}
