/* Only the external HTTPS peer, preflight DNS target and test clock change.
 * HTTP/TLS/certificate validation, JSON parsing, NVS and UI stay real. */
#include "esp_http_client.h"
#include "esp_log.h"
#include "zion_test_endpoint.h"
#include "zion_test_reader.h"
#include <sys/time.h>
#include <netdb.h>
#include <string.h>
extern esp_http_client_handle_t __real_esp_http_client_init(const esp_http_client_config_t *);
esp_http_client_handle_t __wrap_esp_http_client_init(const esp_http_client_config_t *original) {
    if(!original || !original->url || strcmp(original->url,"https://api.deepseek.com/chat/completions"))
        return __real_esp_http_client_init(original);
    esp_http_client_config_t cfg=*original;
    cfg.url="https://" ZION_TEST_MQTT_HOST ":18082/chat/completions";
    cfg.crt_bundle_attach=NULL;cfg.cert_pem=ZION_TEST_MQTT_CA;
    cfg.skip_cert_common_name_check=false;
    ESP_LOGI("reader-fixture","HTTPS_LOCAL certificate_validation=enabled");
    return __real_esp_http_client_init(&cfg);
}
void __wrap_esp_sntp_init(void) {
    struct timeval now={.tv_sec=ZION_TEST_READER_EPOCH};
    settimeofday(&now,NULL);
    ESP_LOGI("reader-fixture","CLOCK_SEEDED source=PC fixture_not_SNTP=1");
}
extern int __real_getaddrinfo(const char *,const char *,const struct addrinfo *,struct addrinfo **);
int __wrap_getaddrinfo(const char *node,const char *service,const struct addrinfo *hints,struct addrinfo **out) {
    if(node && !strcmp(node,"api.deepseek.com")) node=ZION_TEST_MQTT_HOST;
    return __real_getaddrinfo(node,service,hints,out);
}
