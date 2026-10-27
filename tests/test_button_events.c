#include "zion_button_events.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    unsigned legacy_clicks=0,current_releases=0;
    const unsigned events[]={0,4,1}; /* actual raw short-click sequence */
    for(unsigned i=0;i<3;i++) {
        unsigned e=events[i];
        /* Liuyao maps every event except PRESS and LONG to CLICK. */
        if(zion_button_event_allowed(15,e) && e!=0 && e!=3) legacy_clicks++;
        if(zion_button_event_allowed(31,e) && e==4) current_releases++;
    }
    assert(legacy_clicks==1 && current_releases==1);
    for(unsigned e=0;e<4;e++) assert(zion_button_event_allowed(15,e));
    assert(!zion_button_event_allowed(15,4));
    assert(!zion_button_event_allowed(31,5));
    assert(!zion_button_event_allowed(31,32));
    assert(!zion_button_event_allowed(0,1));
    // Xiao Liu Ren inserts HOLD=4 before RELEASE=5 in its application header.
    assert(zion_button_event_translate(5,4)==5);
    assert(zion_button_event_translate(4,4)==4);
    for(unsigned e=0;e<4;e++) assert(zion_button_event_translate(5,e)==e);
    puts("PASS: legacy event surface prevents duplicate click; raw-release ABI retained");
    return 0;
}
