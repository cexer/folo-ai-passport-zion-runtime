"""Run the real DNS ABI adapter with a recording transport and netif provider."""
import argparse, pathlib, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zion-dns-') as tmp:
    d=pathlib.Path(tmp)
    (d/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n')
    (d/'esp_log.h').write_text('#define ESP_LOGI(...) ((void)0)\n')
    (d/'esp_netif.h').write_text('''#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef struct esp_netif {int dummy;} esp_netif_t;
typedef struct {uint32_t addr;} esp_ip4_addr_t;
typedef struct {esp_ip4_addr_t ip,netmask,gw;} esp_netif_ip_info_t;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);
''')
    (d/'test.c').write_text('''#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "esp_netif.h"
#include "zion_idf_dns.h"
static esp_netif_t ap;
static int starts,stops,failure,missing;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key) {assert(!strcmp(key,"WIFI_AP_DEF"));return missing?NULL:&ap;}
esp_err_t esp_netif_get_ip_info(esp_netif_t *n,esp_netif_ip_info_t *ip) {assert(n==&ap);ip->ip.addr=0x0104a8c0;return 0;}
esp_err_t zion_portal_dns_start(uint32_t ip) {assert(ip==0x0104a8c0);starts++;return failure;}
void zion_portal_dns_stop(void) {stops++;}
int main(void) {
 zion_dns_config_t cfg={1,{{"*","WIFI_AP_DEF",{0}}}};
 assert(!zion_idf_dns_start(NULL));missing=1;assert(!zion_idf_dns_start(&cfg));missing=0;
 failure=-1;assert(!zion_idf_dns_start(&cfg));failure=0;
 void *handle=zion_idf_dns_start(&cfg);assert(handle&&starts==2);
 assert(!zion_idf_dns_start(&cfg));zion_idf_dns_stop((void *)1);assert(!stops);
 zion_idf_dns_stop(handle);assert(stops==1);zion_idf_dns_stop(handle);assert(stops==1);
 cfg.num_of_entries=2;assert(!zion_idf_dns_start(&cfg));cfg.num_of_entries=1;
 cfg.item[0].name="example.com";assert(!zion_idf_dns_start(&cfg));cfg.item[0].name="*";
 cfg.item[0].if_key="WIFI_STA_DEF";assert(!zion_idf_dns_start(&cfg));
 cfg.item[0].if_key=NULL;cfg.item[0].ip.addr=7;assert(!zion_idf_dns_start(&cfg));
 cfg.item[0].ip.addr=0x0104a8c0;handle=zion_idf_dns_start(&cfg);assert(handle);zion_idf_dns_stop(handle);
 return 0;
}
''')
    exe=d/'test.exe'
    subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(d),'-I'+str(root/'main'),
                    str(root/'main/zion_idf_dns.c'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: shared AP DNS start/stop, scope validation and transport failures')
