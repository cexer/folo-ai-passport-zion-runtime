/* Upstream-specific lifetime repair, not a shared ZION service. app_main's
 * automatic model escapes into vpet_app's persistent pointer. Keep the model
 * alive for the application lifetime without editing the candidate sources. */
#include "vpet_app.h"
static vpet_model_t retained_model;
extern bool __real_vpet_app_start(vpet_model_t *,bool);
bool __wrap_vpet_app_start(vpet_model_t *model,bool battery_ok) {
    retained_model=*model;
    return __real_vpet_app_start(&retained_model,battery_ok);
}
