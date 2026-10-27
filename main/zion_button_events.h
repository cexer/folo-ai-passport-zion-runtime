#pragma once
#include <stdbool.h>
/* Liuyao's pre-RELEASE BSP maps unknown events to CLICK. Same integer enum
 * prefix is insufficient compatibility: forward only declared events. */
static inline bool zion_button_event_allowed(unsigned mask,unsigned phase) {
    return phase<5 && (mask&(1u<<phase))!=0;
}
static inline unsigned zion_button_event_translate(unsigned release,unsigned phase) {
    // Xiao Liu Ren declares HOLD=4, RELEASE=5. Preserve its compiled constants;
    // symbol-name equality alone does not make independently extended enums ABI.
    return phase==4 ? release : phase;
}
