"""Check the real duplicate Nayuki APIs before selecting one shared provider."""
import argparse,hashlib,json,re,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--headers-only',action='store_true');p.add_argument('--cc');p.add_argument('--decode',action='store_true')
p.add_argument('--lvgl-root',type=Path,default=root/'managed_components/lvgl__lvgl')
p.add_argument('--gfx-root',type=Path,default=root/'managed_components/espressif2022__esp_emote_gfx');a=p.parse_args()
paths=[a.lvgl_root/'src/libs/qrcode/qrcodegen.h',a.gfx_root/'src/lib/qrcode/qrcodegen.h']
def clean(t):return re.sub(r'/\*.*?\*/|//[^\n]*','',t,flags=re.S)
texts=[clean(x.read_text(encoding='utf8')) for x in paths]
def compact(t):return re.sub(r'\s+','',t)
for kind,name in [('enum','Ecc'),('enum','Mask'),('enum','Mode'),('struct','Segment')]:
    parts=[re.search(kind+r'\s+qrcodegen_'+name+r'\s*\{.*?\}',t,re.S)[0] for t in texts]
    assert compact(parts[0])==compact(parts[1]),'QR ABI layout mismatch: '+name
def declarations(t):
    return {m[1]:compact(m[0]).replace('enumqrcodegen_Maskmask','intmask') for m in re.finditer(r'(?:bool|int|size_t|struct\s+qrcodegen_Segment)\s+(qrcodegen_\w+)\s*\([^;]+\);',t)}
new,old=map(declarations,texts)
assert len(old)>=13 and all(k in new and v==new[k] for k,v in old.items()),'QR public API mismatch'
result={'headers':'PASS: matching enum/segment layouts and common scalar ABI','common_functions':len(old)}
if not a.headers_only:
    assert a.cc,'--cc required for actual generator conformance'
    with tempfile.TemporaryDirectory(prefix='zion-qr-') as directory:
        d=Path(directory);objects=[]
        for index,base in enumerate([a.lvgl_root/'src/libs/qrcode',a.gfx_root/'src/lib/qrcode']):
            cmd=[a.cc,'-std=c11','-O2','-DLV_CONF_SKIP=1','-DLV_USE_QRCODE=1','-DLV_USE_LOG=0']
            if index==0:cmd+=['-D'+name+'=lvgl_'+name for name in new]
            obj=d/(str(index)+'.obj');subprocess.run([*cmd,'-c',str(base/'qrcodegen.c'),'-o',str(obj)],check=True);objects.append(str(obj))
        (d/'test.c').write_text('''#include "qrcodegen.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
extern bool lvgl_qrcodegen_encodeText(const char *,uint8_t *,uint8_t *,enum qrcodegen_Ecc,int,int,enum qrcodegen_Mask,bool);
int main(void) {
 const char *samples[]={"12345678901234567890","AI PASSPORT ZION","https://ai-passport.folotoy.cn/plays/","DPP:C:81/6;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgAAAAABBBBBCCCCCDDDDD;I:XiaoLiuRen;;","\\xe4\\xb8\\xad\\xe6\\x96\\x87 test"};
 unsigned cases=0,different_auto=0;
 for(unsigned s=0;s<sizeof(samples)/sizeof(samples[0]);s++)for(int e=0;e<4;e++)for(int mask=-1;mask<=0;mask++) {
  uint8_t t1[qrcodegen_BUFFER_LEN_MAX],t2[qrcodegen_BUFFER_LEN_MAX],q1[qrcodegen_BUFFER_LEN_MAX]={0},q2[qrcodegen_BUFFER_LEN_MAX]={0};
  bool ok1=qrcodegen_encodeText(samples[s],t1,q1,(enum qrcodegen_Ecc)e,1,40,(enum qrcodegen_Mask)mask,true);
  bool ok2=lvgl_qrcodegen_encodeText(samples[s],t2,q2,(enum qrcodegen_Ecc)e,1,40,(enum qrcodegen_Mask)mask,true);
  assert(ok1 && ok2 && qrcodegen_getSize(q1)==qrcodegen_getSize(q2));
  int size=qrcodegen_getSize(q1);int equal=!memcmp(q1,q2,(size*size+7)/8+1);
  if(mask<0 && !equal) different_auto++; else assert(equal);
  for(unsigned impl=0;impl<2;impl++) {
   char file[64];snprintf(file,sizeof(file),"qr-%u-%u.bin",cases,impl);
   FILE *f=fopen(file,"wb");assert(f);assert(fwrite(impl?q2:q1,1,sizeof(q1),f)==sizeof(q1));fclose(f);
  }
  cases++;
 }
 printf("%u %u\\n",cases,different_auto);return 0;
}
''')
        exe=d/'test.exe';subprocess.run([a.cc,'-std=c11','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS','-I'+str(paths[1].parent),str(d/'test.c'),*objects,'-o',str(exe)],check=True)
        run=subprocess.run([str(exe)],cwd=d,check=True,capture_output=True,text=True,timeout=30)
        count,differences=map(int,run.stdout.split());result.update(cases=count,auto_mask_differences=differences,fixed_mask_matrices='byte-identical')
        if a.decode:
            import zxingcpp
            from PIL import Image
            samples=['12345678901234567890','AI PASSPORT ZION','https://ai-passport.folotoy.cn/plays/','DPP:C:81/6;K:MDkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDIgAAAAABBBBBCCCCCDDDDD;I:XiaoLiuRen;;','\u4e2d\u6587 test']
            decoded=0
            for f in d.glob('qr-*.bin'):
                case=int(f.stem.split('-')[1]);body=f.read_bytes();size=body[0]
                img=Image.new('L',(size+8,size+8),255)
                for y in range(size):
                    for x in range(size):
                        bit=y*size+x
                        if (body[1+(bit>>3)]>>(bit&7))&1:img.putpixel((x+4,y+4),0)
                code=zxingcpp.read_barcode(img.resize(((size+8)*6,(size+8)*6),Image.Resampling.NEAREST))
                assert code and code.bytes==samples[case//8].encode('utf8'),str(f)
                decoded+=1
            assert decoded==count*2;result['independent_decoder_roundtrips']=decoded
    result['source_sha256']={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths+[x.with_suffix('.c') for x in paths]}
    (root/'diagnostics/qrcode-shared-provider.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
