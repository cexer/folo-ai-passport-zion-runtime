"""Real compiler/import rejection without changing a live SDK/configuration."""
import argparse,json,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--host-build',type=Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='zion-compat-negative-') as directory:
    project=Path(directory);(project/'main.c').write_text('#include "esp_flash.h"\nvoid app_main(void) {(void)esp_flash_erase_chip(NULL);}\n')
    (project/'compat.json').write_text(json.dumps(dict(id=0x80000006,key='negative',name='Negative',sources=['main.c'])))
    output=project/'output';output.mkdir();(output/'app.zion').write_bytes(b'stale package')
    r=subprocess.run([os.sys.executable,str(root/'tools/build-idf-compat.py'),str(project),'--host-build',str(a.host_build.resolve()),'--output',str(output)],capture_output=True,text=True)
    assert r.returncode!=0 and 'Unsupported host imports' in r.stderr and 'esp_flash_erase_chip' in r.stderr,r.stdout+r.stderr
    assert not (output/'app.zion').exists(),'Failed build retained a stale deliverable'
    print(json.dumps(dict(status='PASS',unsupported_flash_erase_import_rejected=True,stale_package_removed=True)))
