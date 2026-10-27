#!/usr/bin/env python3
"""Explicit segmented flashing of a verified Zion package; preserves cardid."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
from verify_firmware import compatible_zion_layout

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--package', required=True)
parser.add_argument('--fresh', action='store_true', help='Explicitly clear games/settings for a new layout; preserve identity')
parser.add_argument('--no-compress', action='store_true', help='Use uncompressed transfer if the stub rejects the compressed stream')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
package = Path(args.package).resolve()
manifest = json.loads((package / 'manifest.json').read_text())
assert manifest['product'] == 'Zion' and manifest['chip'] == 'esp32c3' and manifest['factory_bytes'] == 0x200000
for name in ['app.bin', 'app.elf', 'bootloader.bin', 'partitions.bin', 'otadata.bin']:
    data = (package / name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == manifest['files'][name]['sha256'], name
assert hashlib.sha256((package / 'app.elf').read_bytes()).hexdigest() == manifest['app_elf_sha256']
app = (package / 'app.bin').read_bytes()
assert app[176:208].hex() == manifest['app_elf_sha256'] and len(app) <= 0x200000
stage = root / 'build/flash-staging' / manifest['app_elf_sha256'][:12]
stage.mkdir(parents=True, exist_ok=True)
logs = []

def esptool(arguments):
    result = subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32c3', '--port', args.port,
                             '--baud', '460800', '--before', 'usb_reset', '--after', 'hard_reset'] + arguments,
                            capture_output=True, text=True, encoding='utf8', errors='replace')
    safe = re.sub(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', '[MAC]', result.stdout + result.stderr)
    logs.append(safe)
    (stage / 'flash-sanitized.log').write_text('\n'.join(logs), encoding='utf8')
    if result.returncode:
        raise RuntimeError(safe[-1800:])
    return safe

table = stage / 'live-partitions.bin'
esptool(['read_flash', '0x8000', '0xc00', str(table)])
live = table.read_bytes()
factory_size = None
for at in range(0, len(live), 32):
    magic, kind, subtype, offset, size = struct.unpack_from('<HBBII', live, at)
    if magic != 0x50aa:
        break
    if kind == 0 and subtype == 0 and offset == 0x10000:
        factory_size = size
if (factory_size != 0x200000 or not compatible_zion_layout(live, (package / 'partitions.bin').read_bytes())) and not args.fresh:
    raise SystemExit('Layout mismatch: this device requires an explicitly authorized --fresh installation.')
padded = stage / 'factory-padded.bin'
padded.write_bytes(app + b'\xff' * (0x200000 - len(app)))
segments = [(0, package / 'bootloader.bin'), (0x8000, package / 'partitions.bin'),
            (0x10000, padded), (0x7fe000, package / 'otadata.bin')]
if args.fresh:
    for offset, size, name in [(0x9000, 0x6000, 'nvs'), (0x210000, 0x146000, 'pool0'),
                               (0x35a000, 0x6000, 'store'), (0x360000, 0x49e000, 'pool1')]:
        f = stage / f'empty-{name}.bin'
        f.write_bytes(b'\xff' * size)
        segments.append((offset, f))
segments.sort()
for (offset, file), (next_offset, _) in zip(segments, segments[1:]):
    assert offset + file.stat().st_size <= next_offset
for offset, file in segments:
    assert offset + file.stat().st_size <= 0x356000 or offset >= 0x35a000, 'identity overlap'
arguments = ['write_flash', '--flash_mode', 'keep', '--flash_freq', 'keep', '--flash_size', 'keep']
if args.no_compress:
    arguments.append('--no-compress')
for offset, file in segments:
    arguments += [hex(offset), str(file)]
output = esptool(arguments)
assert output.count('Hash of data verified.') == len(segments)
print(f'Flash PASS: {args.port}; {len(segments)} segments; cardid preserved; test_control={manifest["test_control"]}; ELF={manifest["app_elf_sha256"]}')
