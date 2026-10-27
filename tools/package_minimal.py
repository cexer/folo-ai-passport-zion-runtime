"""Verify and archive a minimal firmware; never flashes or publishes anything."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import struct


def digest(data):
    return hashlib.sha256(data).hexdigest()


def independent_app_partition(full, table=None):
    """Select the single verified image in factory or OTA-only merged firmware."""
    if table is None:
        table=partitions(full[0x8000:0x8c00])
    images=[]
    for part in table:
        at=part['offset']
        if part['type']!=0 or at>=len(full) or full[at]!=0xe9:
            continue
        try:
            length=image_length(full[at:at+part['size']])
        except (AssertionError,IndexError,struct.error):
            continue
        if length<=part['size']:
            images.append(part)
    assert len(images)==1,'Independent firmware requires one unambiguous verified APP image'
    return images[0]


def image_length(data):
    assert data[0] == 0xE9 and struct.unpack_from('<H', data, 12)[0] == 5
    pos, checksum = 24, 0xEF
    for _ in range(data[1]):
        _, length = struct.unpack_from('<II', data, pos)
        pos += 8
        assert pos + length <= len(data)
        for value in data[pos:pos + length]:
            checksum ^= value
        pos += length
    pos |= 15
    assert data[pos] == checksum, 'ESP segment checksum mismatch'
    end = pos + 1
    if data[23] & 1:
        assert data[end:end + 32] == hashlib.sha256(data[:end]).digest(), 'ESP image SHA mismatch'
        end += 32
    return end


def partitions(data):
    result = []
    for pos in range(0, 0xC00, 32):
        magic = struct.unpack_from('<H', data, pos)[0]
        if magic == 0xEBEB:
            assert data[pos + 16:pos + 32] == hashlib.md5(data[:pos]).digest()
            break
        assert magic == 0x50AA, 'Missing partition MD5 record'
        _, kind, subtype, start, size, label, _ = struct.unpack_from('<HBBII16sI', data, pos)
        assert start >= 0x9000 and size > 0 and start + size <= 0x800000
        result.append(dict(type=kind, subtype=subtype, offset=start, size=size,
                           label=label.split(b'\0')[0].decode()))
    ordered = sorted(result, key=lambda item: item['offset'])
    assert all(a['offset'] + a['size'] <= b['offset'] for a, b in zip(ordered, ordered[1:]))
    return result


def package(build, merged_path, output, profile):
    desc = json.loads((build / 'project_description.json').read_text())
    if profile=='zion-runtime':
        product=json.loads((build.parent/'zion-product.json').read_text(encoding='utf8'))
        assert desc['project_version']==product['version'], 'Stale firmware version; reconfigure first'
    config = Path(desc['config_file']).read_text(encoding='utf8')
    for flag in ['CONFIG_XIAOZHI_TEXT_ONLY=y', 'CONFIG_XIAOZHI_COMPACT_FONT=y',
                 'CONFIG_XIAOZHI_SILENT_PROMPTS=y', 'CONFIG_XIAOZHI_NO_FIRMWARE_UPGRADE=y']:
        assert flag in config, flag
    assert 'CONFIG_LV_USE_LODEPNG=y' not in config
    app = (build / desc['app_bin']).read_bytes()
    elf = (build / desc['app_elf']).read_bytes()
    assert app[176:208] == hashlib.sha256(elf).digest(), 'ELF does not match APP'
    app_length = image_length(app)
    assert app_length == len(app)
    boot = (build / 'bootloader/bootloader.bin').read_bytes()
    assert image_length(boot) == len(boot) and len(boot) <= 0x8000
    table = partitions((build / 'partition_table/partition-table.bin').read_bytes())
    flash = json.loads((build / 'flasher_args.json').read_text())['flash_files']
    merged = merged_path.read_bytes()
    assert len(merged) <= 0x800000
    for at, name in flash.items():
        offset = int(at, 0)
        data = (build / name).read_bytes()
        assert merged[offset:offset + len(data)] == data, name
    app_offsets = [int(at, 0) for at, name in flash.items() if Path(name).name == desc['app_bin']]
    assert len(app_offsets) == 1
    offset = app_offsets[0]
    part = next(p for p in table if p['type'] == 0 and p['offset'] == offset)
    assert len(app) <= part['size']
    end = offset + len(app)
    # Omitting erased padding changes neither the executable nor initialized data.
    assert not merged[end:].strip(b'\xff'), 'There is initialized data after APP'
    body = merged[:end]
    for stored in table:
        if stored['type'] == 1 and stored['subtype'] == 2:
            region = body[stored['offset']:stored['offset'] + stored['size']]
            assert not region.strip(b'\xff'), 'Release contains initialized NVS data'
    if profile == 'standalone':
        assert offset == 0x10000 and part['subtype'] == 0
        assert {p['label'] for p in table} == {'nvs', 'phy_init', 'factory'}
        assert all(p['type'] == 0 or p['subtype'] in [0, 1, 2] for p in table)
    else:
        assert profile == 'zion-runtime' and 'CONFIG_ZION_LAUNCHER=y' in config
        assert b'ZION_RUNTIME_SIM radio=disabled' not in app, 'Simulator-only image cannot be released'
        assert b'ZION_BUILTIN_BOOT_SMOKE' not in app, 'Test-only boot injection must never be packaged'
        assert 'CONFIG_ESP_SYSTEM_MEMPROT=y' not in config, 'Native ELF cannot use the default non-executable heap'
        assert b'LOADED source=external-flash' in app, 'ELF loader is not linked'
        assert b'RETURN restart=1 destination=list' not in app, 'Obsolete OK restart handler'
        for marker in [b'GALLERY_VALIDATED format=FGA1',b'DEMO_SAVE value=',b'SOUNDLAB_GOLDEN_PASS',b'NESCPU_GOLDEN_PASS']:
            assert marker not in app, 'Test applications must be external: '+marker.decode()
        assert part['size'] == 0x260000
        boot_config = (build / 'bootloader/config/sdkconfig.h').read_text()
        boot_size = re.search(r'^#define CONFIG_ZION_FACTORY_SIZE (\d+)$', boot_config, re.M)
        assert boot_size and int(boot_size[1]) == part['size'], 'Bootloader factory geometry differs from APP/table'
        for label, address in [('cardid', 0x356000), ('store', 0x35A000), ('otadata', 0x7FE000)]:
            assert next(p for p in table if p['label'] == label)['offset'] == address
    identity = digest(body)
    stem = 'Xiaozhi-Minsize' if profile == 'standalone' else 'ZION-Runtime'
    name = f'{stem}-{desc["project_version"]}-{identity[:12]}.bin'
    output.mkdir(parents=True, exist_ok=True)
    (output / name).write_bytes(body)
    (output / (name + '.sha256')).write_text(f'{identity}  {name}\n', encoding='ascii')
    archive = build.parent / 'dist' / profile / identity[:12]
    archive.mkdir(parents=True, exist_ok=True)
    for path, target in [(build / desc['app_bin'], 'app.bin'), (build / desc['app_elf'], 'app.elf'),
                         (build / f'{desc["project_name"]}.map', 'app.map'),
                         (build / 'bootloader/bootloader.bin', 'bootloader.bin'),
                         (build / 'bootloader/bootloader.elf', 'bootloader.elf'),
                         (build / 'bootloader/bootloader.map', 'bootloader.map'),
                         (build / 'partition_table/partition-table.bin', 'partitions.bin'),
                         (Path(desc['config_file']), 'sdkconfig'), (build / 'flash_args', 'flash_args')]:
        shutil.copyfile(path, archive / target)
    (archive / 'firmware.bin').write_bytes(body)
    # Retain the original relative files as well, so flash_args is reviewable
    # against a complete set rather than referring to missing OTA/boot images.
    for segment_name in flash.values():
        target = archive / segment_name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build / segment_name, target)
    shutil.copyfile(build / 'flasher_args.json', archive / 'flasher_args.json')
    plugin_manifest=[]
    manifest = dict(profile=profile, version=desc['project_version'], chip=desc['target'],
                    idf=app[144:176].split(b'\0')[0].decode(), flash_offset='0x0',
                    app_offset=hex(offset), app_bytes=len(app), firmware_bytes=len(body),
                    firmware_sha256=identity, elf_sha256=digest(elf),
                    plugins=plugin_manifest,
                    partitions=table, archive=str(archive.resolve()),
                    validation='ESP checksum/SHA, partition MD5/bounds/overlap, ELF and flash segment match',
                    hardware_validation=False,
                    artifacts={str(p.relative_to(archive)): dict(bytes=p.stat().st_size,
                               sha256=digest(p.read_bytes())) for p in archive.rglob('*')
                               if p.is_file() and p.name != 'manifest.json'})
    for path in [archive / 'manifest.json', output / (name + '.json')]:
        path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--merged', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=['standalone', 'zion-runtime'], required=True)
    args = parser.parse_args()
    package(args.build.resolve(), args.merged.resolve(), args.output.resolve(), args.profile)
