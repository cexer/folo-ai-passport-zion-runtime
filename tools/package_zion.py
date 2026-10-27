#!/usr/bin/env python3
"""Package matching product images and symbols. Never flashes a device."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import verify_firmware
from zion_product import NAME, APP_PROJECT_NAME, brand_merged
from zion_release import export_release

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', default='build/zion')
args = parser.parse_args()
build = root / args.build_dir
config = (build / 'sdkconfig').read_text(encoding='utf8')
for flag in ['CONFIG_ZION_LAUNCHER=y', 'CONFIG_ZION_592_INSTALL=y',
             'CONFIG_ZION_FACTORY_SIZE=2097152', 'CONFIG_MBEDTLS_DYNAMIC_BUFFER=y']:
    assert flag in config, flag
assert 'CONFIG_LV_DRAW_SW_SUPPORT_ARGB8888=y' in config, 'QR decoded rows cannot render without ARGB8888 support'
test_control = 'CONFIG_META_E2E_TEST_CONTROL=y' in config
description = json.loads((build / 'project_description.json').read_text())
version = description['project_version']
assert description['target'] == 'esp32c3'
digest = lambda data: hashlib.sha256(data).hexdigest()
app = (build / 'FoloToy-AI-Passport.bin').read_bytes()
idf_version = app[144:176].split(b'\0', 1)[0].decode('ascii')
assert idf_version.startswith('v5.5.3'), idf_version
elf = (build / 'FoloToy-AI-Passport.elf').read_bytes()
assert app[176:208] == hashlib.sha256(elf).digest()
assert b'ZION_PRODUCT ready' in app and len(app) <= 0x200000
assert (b'ZION KEY UP' in app) == test_control, 'test-control linkage/config mismatch'
assert (b'ZION_QR_PIXELS width=' in app) == test_control, 'QR pixel capture must be test-only'
assert (b'ZION PS ON' in app) == test_control and (b'ZION HOLD ON' in app) == test_control, 'download comparison commands must be test-only'
assert (b'ZION BLE SLOW' in app) == test_control and (b'ZION COEX WIFI' in app) == test_control, 'radio comparison commands must be test-only'
map_text = (build / 'FoloToy-AI-Passport.map').read_text(encoding='utf8', errors='replace')
assert 'zion_delete' in map_text and 'zion_launcher_tick' in map_text, 'product UI/management not linked'
assert 'zion_install_format' in map_text and b'ZION_INSTALL_UI' in app, 'install information UI not linked'
assert 'zion_title_result' in map_text and b'Z592_TRANSFER_END pass=' in app, 'community titles/download telemetry not linked'
assert 'meta_carve_flash_reload' in map_text and b'Z592_CLEANUP reclaimed=' in app, 'failed-install reconciliation/cleanup not linked'
assert 'zion_official_mini_code' in map_text and b'kind=official-mini-code' in app and b'settings_marked=%d' in app, 'official Mini Program code/settings marker not linked'
assert 'zion_font_ui_14' in map_text and 'zion_font_ui_10' in map_text and 'zion_font_14' in map_text, 'product 4bpp fonts/full fallback not linked'
assert b'ZION_FONTS ui_bpp=' in app, 'runtime font profile not linked'
assert (b'ZION_FONT_TEST pass=' in app)==test_control, 'pixel decoder test must be test-only'
assert (root / 'assets/images/folo-mini-code-208.i1').read_bytes() in app, 'official Mini Program code pixels differ in artifact'
app,full=brand_merged(app,(build / 'FoloToy-AI-Passport-full.bin').read_bytes())
(build / 'FoloToy-AI-Passport-full.bin').write_bytes(full)
body = full[:0x10000 + len(app)]
hybrid = body + b'MPUPV2\0\0' + struct.pack('<I', len(body)) + hashlib.sha256(body).digest()
hybrid_name = f'meta-pass_v{version}.bin'
(build / hybrid_name).write_bytes(hybrid)
verify_firmware.verify_protected_layout(full, build)
verify_firmware.verify_upgrade_safety(full)
verify_firmware.verify_single_file_artifact(full, build)
dest = root / 'dist' / (version + ('-test' if test_control else '')) / digest(elf)[:12]
dest.mkdir(parents=True, exist_ok=True)
files = {'zion-full.bin': hybrid, 'app.bin': app, 'app.elf': elf,
         'bootloader.bin': (build / 'bootloader/bootloader.bin').read_bytes(),
         'partitions.bin': (build / 'partition_table/partition-table.bin').read_bytes(),
         'otadata.bin': b'\xff' * 8192}
for name, data in files.items():
    (dest / name).write_bytes(data)
for source, target in [('FoloToy-AI-Passport.map', 'app.map'), ('sdkconfig', 'sdkconfig')]:
    shutil.copyfile(build / source, dest / target)
    files[target] = (dest / target).read_bytes()
manifest = {'product': 'Zion', 'version': version, 'idf': idf_version,
            'display_name': NAME, 'app_project_name': APP_PROJECT_NAME,
            'chip': 'esp32c3', 'factory_bytes': 0x200000,
            'test_control': test_control, 'app_elf_sha256': digest(elf),
            'merged_flash_offset': '0x0', 'legacy_layout_migration': False,
            'flash_layout': {'bootloader.bin': '0x0', 'partitions.bin': '0x8000', 'app.bin': '0x10000', 'otadata.bin': '0x7fe000'},
            'files': {name: {'bytes': len(data), 'sha256': digest(data)} for name, data in files.items()}}
(dest / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf8')
print(f'Zion package: {dest.relative_to(root)}; app={len(app)}; ELF={digest(elf)}')
if not test_control:
    print(f'Zion release: {export_release(dest, root / "release").relative_to(root)}')
