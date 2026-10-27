/* Reuse the host's real UDP DNS transport for unchanged IDF-example callers. */
#include "zion_idf_dns.h"
#include "zion_portal_net.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>
static bool active;
void *zion_idf_dns_start(zion_dns_config_t *config) {
    if(active || !config || config->num_of_entries!=1 || !config->item[0].name ||
       strcmp(config->item[0].name,"*")) return NULL;
    const zion_dns_item_t *item=&config->item[0];
    if(item->if_key && strcmp(item->if_key,"WIFI_AP_DEF")) return NULL;
    esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t ip;
    if(!ap || esp_netif_get_ip_info(ap,&ip)!=ESP_OK || !ip.ip.addr) return NULL;
    if(!item->if_key && item->ip.addr!=ip.ip.addr) return NULL;
    if(zion_portal_dns_start(ip.ip.addr)!=ESP_OK) return NULL;
    active=true;
    ESP_LOGI("zion-compat","DNS_SHARED wildcard=1 scope=AP");
    return &active;
}
void zion_idf_dns_stop(void *handle) {
    if(handle!=&active || !active) return;
    zion_portal_dns_stop();active=false;
}
