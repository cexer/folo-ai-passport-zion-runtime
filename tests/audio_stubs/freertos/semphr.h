#pragma once
#include <stdbool.h>
typedef struct { bool held; } test_audio_mutex_t;
typedef test_audio_mutex_t *SemaphoreHandle_t;
static test_audio_mutex_t test_audio_mutex;
#define pdTRUE 1
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {return &test_audio_mutex;}
static inline int xSemaphoreTake(SemaphoreHandle_t m,unsigned ticks) {(void)ticks;if(m->held)return 0;m->held=true;return 1;}
static inline int xSemaphoreGive(SemaphoreHandle_t m) {m->held=false;return 1;}
static inline void *xSemaphoreGetMutexHolder(SemaphoreHandle_t m) {return m->held?(void *)1:(void *)0;}
