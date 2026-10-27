"""Record unchanged-base device cases; failures never become a clean pass.

The device result is an explicit evidence file, not inferred from compilation.
Keep this state local with diagnostics; no secrets or serial identifiers needed.
"""
import argparse, hashlib, json
from pathlib import Path
from clean_streak import qualifies,advance

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--host-build', type=Path, required=True)
p.add_argument('--application')
p.add_argument('--candidate', type=Path)
p.add_argument('--module', type=Path)
p.add_argument('--result', type=Path)
p.add_argument('--base-changed', action='store_true')
p.add_argument('--note', default='')
a = p.parse_args()
host = a.host_build.resolve()
sha = lambda data: hashlib.sha256(data).hexdigest()
db = json.loads((host/'compile_commands.json').read_text())
files = {Path(row['file']).resolve() for row in db}
files = {f for f in files if f.is_relative_to(root/'main') or f.is_relative_to(root/'components') or f.is_relative_to(root/'tests/firmware')}
files.update(f for folder in ['main', 'components/bsp/include', 'components/zion_sdk/include']
             for f in (root/folder).rglob('*.h'))
files.update(root/f for f in ['sdkconfig.defaults', 'sdkconfig.with-xiaozhi.defaults',
                              'main/zion_idf_auto_imports.json', 'main/zion_idf_exports.inc'])
snapshot = {str(f.relative_to(root)).replace('\\','/'): sha(f.read_bytes()) for f in sorted(files)}
identity = {'host_elf_sha256': sha((host/'FoloToy-AI-Passport.elf').read_bytes()),
            'bootloader_elf_sha256': sha((host/'bootloader/bootloader.elf').read_bytes()),
            'partitions_sha256': sha((host/'partition_table/partition-table.bin').read_bytes()),
            'sdkconfig_sha256': sha((host/'sdkconfig').read_bytes()),
            'sources_sha256': sha(json.dumps(snapshot,sort_keys=True).encode()),
            'source_files': snapshot}
state_file = root/'diagnostics/community/clean-streak/state.json'
state_file.parent.mkdir(parents=True, exist_ok=True)
state = json.loads(state_file.read_text()) if state_file.exists() else {'required': 3, 'streak': 0, 'cases': []}
old = state.get('base')
changed = old and old != identity
if changed or a.base_changed:
    state['streak'] = 0
    state.setdefault('base_changes',[]).append({
        'previous_host_elf_sha256':old.get('host_elf_sha256') if old else None,
        'host_elf_sha256':identity['host_elf_sha256'],'note':a.note})
state['base'] = identity
if a.application:
    assert a.result and a.candidate and a.module
    result = json.loads(a.result.read_text())
    manifest = json.loads((a.module/'manifest.json').read_text())
    assert manifest['host_elf_sha256'] == identity['host_elf_sha256']
    import subprocess
    revision = subprocess.check_output(['git','-C',str(a.candidate),'rev-parse','HEAD'],text=True).strip()
    dirty = subprocess.check_output(['git','-C',str(a.candidate),'status','--porcelain'],text=True)
    source_unchanged = not dirty and all(sha(Path(s['file']).read_bytes()) == s['sha256'] for s in manifest['sources']+manifest.get('compat_headers',[]))
    clean = qualifies(result,source_unchanged,manifest.get('link_wrappers'),changed or a.base_changed)
    state['streak'] = advance(state['streak'],clean)
    state['cases'].append({'application':a.application,'revision':revision,'clean_pass':clean,
        'source_unchanged':source_unchanged,'base_changed':bool(changed or a.base_changed),
        'host_elf_sha256':identity['host_elf_sha256'],'result':str(a.result),
        'module':str(a.module),'streak_after':state['streak'],'note':a.note})
state_file.write_text(json.dumps(state,indent=2)+'\n')
print(json.dumps({'streak':state['streak'],'required':3,'host':identity['host_elf_sha256'],'state':str(state_file)}))
