"""Generate an exact-configuration import contract for a local runtime build."""
import argparse,hashlib,json,os
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[1];idf=Path(os.environ['IDF_PATH'])
files=[a.build/'config/sdkconfig.h',a.build/'toolchain/cflags',a.build/'specs/picolibc.specs',root/'main/zion_idf_exports.inc',root/'main/zion_idf_compat.c',root/'main/zion_idf_auto_imports.inc']
files += [root/'main'/name for name in ['zion_idf_board.c','zion_idf_storage.c','zion_idf_dns.c','zion_idf_dns.h','zion_elf.c','zion_elf.h','zion_elf_host.c','zion_package.h']]
files += sorted((root/'components/bsp/src').glob('*.c'))
files += sorted((root/'components/bsp/src').glob('*.h'))
files += [root/'main/zion_idf_ota.c',root/'main/zion_idf_ota.h',root/'main/zion_button_events.h']
files += [root/'main/zion_idf_link_hooks.c',root/'main/zion_idf_link_hooks.h']
files += sorted((idf/'components/mbedtls').rglob('*.h'))
files += sorted((root/'managed_components/espressif__esp_websocket_client/include').glob('*.h'))
for directory in [root/'components/bsp/include',root/'managed_components/lvgl__lvgl',idf/'components/esp_wifi/include',idf/'components/wpa_supplicant/esp_supplicant/include',idf/'components/freertos',idf/'components/esp_common/include',
                  idf/'components/nvs_flash/include',idf/'components/esp_timer/include',idf/'components/esp_log/include',idf/'components/log/include',idf/'components/esp_system/include']:
    files+=sorted(directory.rglob('*.h'))
records=[dict(name=('build/'+f.relative_to(a.build).as_posix() if f.is_relative_to(a.build) else f.relative_to(root).as_posix() if f.is_relative_to(root) else 'idf/'+f.relative_to(idf).as_posix()),
              sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for f in files]
fingerprint=hashlib.sha256(json.dumps(records,sort_keys=True).encode()).hexdigest()[:32]
symbol='zion_idf_contract_'+fingerprint
a.output.mkdir(parents=True,exist_ok=True)
header=f'#define ZION_IDF_CONTRACT_HASH "{fingerprint}"\n#define ZION_IDF_CONTRACT_SYMBOL "{symbol}"\n'
target=a.output/'zion_idf_contract.h'
if not target.exists() or target.read_text()!=header:target.write_text(header)
(a.output/'zion-idf-compat.json').write_text(json.dumps(dict(schema=1,fingerprint=fingerprint,symbol=symbol,inputs=records),indent=2)+'\n')
