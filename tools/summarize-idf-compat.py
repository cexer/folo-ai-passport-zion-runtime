"""Bind the source-compatibility checkpoint to artifacts and measured receipts."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'diagnostics/idf-compat'
def load(relative):return json.loads((root/relative).read_text())
package=load('release/idf-compat-0.4.0/manifest.json')
for entry in package['packages']:
    body=(root/'release/idf-compat-0.4.0'/entry['file']).read_bytes()
    assert hashlib.sha256(body).hexdigest()==entry['sha256'] and len(body)==entry['bytes']
host=load('release/ZION-Runtime-0.4.0-runtime-afa71a9888b0.bin.json')
assert host['elf_sha256']==package['host_elf_sha256'] and not host['plugins']
assert hashlib.sha256((root/'release/ZION-Runtime-0.4.0-runtime-afa71a9888b0.bin').read_bytes()).hexdigest()==host['firmware_sha256']
receipts={name:load('diagnostics/idf-compat/'+name+'-result.json') for name in ['native-final','native-sim','standalone-sim','dual-independent-sim','final-list']}
assert receipts['native-final']['status']=='PASS' and receipts['native-final']['initial_count']==1 and receipts['native-final']['saved_count']==2
assert receipts['native-sim']['bad_contract_rejected']
assert receipts['final-list']['page']==0 and receipts['final-list']['default']==-1
for i in range(1,4):
    log=(out/f'update-{i}.log').read_text(encoding='utf8')
    assert 'ZION_UPLOAD_DONE result=0 stage=installed' in log and 'Guru Meditation' not in log
main=root/'examples/idf-compat/main/main.c'
db=load('build-idf-compat-standalone/compile_commands.json')
assert any(Path(record['file']).resolve()==main.resolve() for record in db)
assert hashlib.sha256(main.read_bytes()).hexdigest()==package['source_main_sha256']
core=root/'examples/plugins/nescpu/upstream/nes6502.c';candidate=root.parent/'candidates/nes-card/components/nofrendo/cpu/nes6502.c'
assert core.read_bytes()==candidate.read_bytes()
report=dict(version='0.4.0-runtime',Build='PASS',Host_tests='PASS',Device_tests='PASS within stated scopes',
    complete_gate='FAIL: inherited imported Markdown checks; ../diagnostics/idf-compat-complete-gate.log',
    host=dict(bytes=host['firmware_bytes'],sha256=host['firmware_sha256'],elf_sha256=host['elf_sha256']),packages=package,
    receipts=receipts,consecutive_device_updates=3,ordinary_source_same_for_both_builds=True,third_party_core_byte_identical=True,
    non_test_production_hardware_execution='NOT RUN',
    prior_failure=dict(log='install-device.log',matching_elf_sha256='264cf4f6ebc1c287b765bab8c0b4df4237b66b737ea3cb88a6b22bf9a03ccfe2',
        decoded_path='esp_flash_read <- NVS page read <- zion_runtime_default_slot during table materialization',
        fix='Cache native default identity; render without NVS handles'),
    unverified=['Full untouched third-party application integration (fixture plus real CPU core only)',
                'General C++ and unlisted SDK imports','Networking/raw driver lifecycle',
                'Official mini-program delivery and community approval','Mechanical key feel and acoustic behavior'],
    final_device='COM5, DeviceTest host, list with seven entries, no default, serial closed')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(report=str(out/'report.json'),packages=len(package['packages']),status='PASS within scope'),indent=2))
