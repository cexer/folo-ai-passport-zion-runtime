/* Local transport fixture only: keep the original catalog, MP3 parser, jitter
 * buffer and I2S path. No station-selection or playback logic is replaced. */
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
extern esp_http_client_handle_t __real_esp_http_client_init(const esp_http_client_config_t *);
esp_http_client_handle_t __wrap_esp_http_client_init(const esp_http_client_config_t *cfg) {
    if(!cfg || !cfg->url) return __real_esp_http_client_init(cfg);
    const char *url=NULL;
    if(strstr(cfg->url,"ip-api.com/json") || strstr(cfg->url,"ipwho.is/"))
        url="http://192.168.5.100:18081/geo";
    else if(strstr(cfg->url,"api.radio-browser.info/"))
        url="http://192.168.5.100:18081/stations";
    else if(strstr(cfg->url,"rapi.qtfm.cn/"))
        url="http://192.168.5.100:18081/metadata";
    else if(strstr(cfg->url,"lhttp.qingting.fm/"))
        url="http://192.168.5.100:18081/stream.mp3";
    if(!url) return __real_esp_http_client_init(cfg);
    esp_http_client_config_t copy=*cfg;copy.url=url;
    ESP_LOGI("radio-fixture","HTTP route %s",url);
    return __real_esp_http_client_init(&copy);
}
