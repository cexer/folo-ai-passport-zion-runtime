/* Validation-only transport substitution. Business parsing and MQTT/TLS stay real. */
#include "mqtt_client.h"
#include "zion_test_endpoint.h"
#include <stdio.h>
esp_mqtt_client_handle_t __real_esp_mqtt_client_init(const esp_mqtt_client_config_t *);
esp_mqtt_client_handle_t __wrap_esp_mqtt_client_init(const esp_mqtt_client_config_t *original) {
    esp_mqtt_client_config_t cfg=*original;
    cfg.broker.address.uri=NULL;
    cfg.broker.address.hostname=ZION_TEST_MQTT_HOST;
    cfg.broker.address.port=18884;
    cfg.broker.address.transport=MQTT_TRANSPORT_OVER_SSL;
    cfg.broker.verification.crt_bundle_attach=NULL;
    cfg.broker.verification.certificate=ZION_TEST_MQTT_CA;
    cfg.broker.verification.skip_cert_common_name_check=false;
    puts("ZION_TEST_TRANSPORT local_tls_mqtt=1");
    return __real_esp_mqtt_client_init(&cfg);
}
