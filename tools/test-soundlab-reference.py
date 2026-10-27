import argparse,subprocess,tempfile,re,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',required=True);a=p.parse_args()
up=root/'examples/plugins/soundlab/upstream';example=up.parent
with tempfile.TemporaryDirectory(prefix='zion-dsp-reference-') as directory:
    outputs=[];pcm=[]
    for variant in ['nabo_voice','nabo_voice_fixed']:
        exe=Path(directory)/(variant+'.exe')
        subprocess.run([a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-Wno-misleading-indentation',
            '-I'+str(up),'-I'+str(example),str(root/'tests/test_soundlab_reference.c'),str(up/(variant+'.c')),'-o',str(exe)],check=True)
        binary=Path(directory)/(variant+'.pcm')
        outputs.append(subprocess.check_output([str(exe),str(binary)],text=True));pcm.append(binary.read_bytes())
    assert outputs[0]==outputs[1],outputs
    assert pcm[0]==pcm[1],'PCM byte mismatch'
    values=re.findall(r'VOICE (\d+) samples=(\d+) hash=([0-9a-f]+)',outputs[0]);assert len(values)==12
    header='#pragma once\nstatic const unsigned expected_samples[12]={'+','.join(x[1] for x in values)+'};\n'
    header+='static const unsigned expected_hash[12]={'+','.join('0x'+x[2]+'u' for x in values)+'};\n'
    (example/'golden.h').write_text(header)
    result=dict(status='PASS',comparison='Unmodified upstream libm initialization versus precomputed-table port; byte-exact PCM comparison',pcm_bytes=len(pcm[0]),
        voices=[dict(mode=int(v),samples=int(n),hash=h) for v,n,h in values])
    out=root/'diagnostics/soundlab';out.mkdir(exist_ok=True)
    (out/'host-reference.json').write_text(json.dumps(result,indent=2)+'\n')
    print(outputs[0]);print('PASS: 12 effects match unmodified upstream')
