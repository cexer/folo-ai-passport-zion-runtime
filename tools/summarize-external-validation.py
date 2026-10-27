"""Bind the external workload receipts to the exact delivered test packages."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'release/test-apps/external-apps-0.3.0.json').read_text())
rows=[]
routes={'plugin-only':['plugin-only'],'dual':['dual-plugin','dual-independent'],
        'standalone-only':['standalone-only','standalone-via-zion']}
for package in manifest['records']:
    body=(root/'release/test-apps'/package['file']).read_bytes()
    assert len(body)==package['bytes'] and hashlib.sha256(body).hexdigest()==package['sha256']
    receipts={}
    for route in routes[package['kind']]:
        file=Path('diagnostics')/package['application']/(route+'-result.json')
        data=json.loads((root/file).read_text())
        if package['application']=='soundlab':assert data['golden_effects']==12
        else:assert 'cycles=819539 hash=1738864307 irq=19 nmi=29 pc=32800' in data['golden']
        receipts[route]={'status':'PASS','receipt':file.as_posix()}
    rows.append(dict(package=package['file'],bytes=package['bytes'],sha256=package['sha256'],routes=receipts))
sim=json.loads((root/'diagnostics/external-apps-sim/result.json').read_text())
management=json.loads((root/'diagnostics/native-management/result.json').read_text())
rejection=json.loads((root/'diagnostics/package-rejection/result.json').read_text())
assert sim['lifecycle']['delete_default'] and management['reinstall_restores_free'] and rejection['status']=='PASS'
releases=sorted((root/'release').glob('ZION-Runtime-0.3.0-*.bin.json'),key=lambda p:p.stat().st_mtime)
host=json.loads(releases[-1].read_text())
assert hashlib.sha256(releases[-1].with_suffix('').read_bytes()).hexdigest()==host['firmware_sha256']
report=dict(schema=1,scope='Experimental SDK6 shared runtime; source-core ports, not complete source apps',
    host=dict(manifest=str(releases[-1].relative_to(root)).replace('\\','/'),bytes=host['firmware_bytes'],
              sha256=host['firmware_sha256'],elf_sha256=host['elf_sha256']),
    build='PASS',host_tests='PASS: eight groups plus byte-exact twelve-effect SoundLab reference',
    device_tests='PASS within the listed routes; serial-injected controls and muted hardware PCM',
    full_repository_gate='FAIL: inherited imported bilingual-document violations; see ../diagnostics/runtime-complete-gate-final.log',
    packages=rows,simulator=sim,native_management=management,corrupt_upgrade=rejection,
    resource_regression='Manager stack retired before plugin entry; SoundLab golden/PCM passed after change',
    limits=['Gallery large contiguous allocation boundary remains; adaptive stripes were required',
            'NES CPU O2 code allocation 48196 bytes failed with largest block 43008; Os code 35228 passed',
            'Trusted native code, shared address space, CONFIG_ESP_SYSTEM_MEMPROT=n, no eFuse changes'],
    unverified=['Mechanical key feel and acoustic fidelity','Cloud/network concurrency',
                'Full source-app UI/features including full NES PPU/APU/gameplay',
                'Official mini-program delivery of plugin or dual packages',
                'Production non-test image on hardware; DeviceTest image was used for automation'])
destination=root/'diagnostics/runtime-external-app-matrix.json'
destination.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'report':str(destination),'packages':len(rows),'routes':sum(len(r['routes']) for r in rows)},indent=2))
