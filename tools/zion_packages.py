"""ZPK1 plugin packages and dual-use firmware trailer (no credentials)."""
from pathlib import Path
import struct,hashlib,zlib
HEADER=256
def elf_export(image,name):
    shoff=struct.unpack_from('<I',image,32)[0];count=struct.unpack_from('<H',image,48)[0]
    sections=[struct.unpack_from('<10I',image,shoff+i*40) for i in range(count)]
    for section in sections:
        if section[1]!=2: continue
        strings=sections[section[6]];table=image[strings[4]:strings[4]+strings[5]]
        for at in range(section[4],section[4]+section[5],16):
            offset,value,size,_,_,index=struct.unpack_from('<IIIBBH',image,at)
            if table[offset:].split(b'\0',1)[0].decode()==name:
                assert size==4 and index<len(sections)
                return struct.unpack_from('<I',image,sections[index][4]+value)[0]
    raise ValueError('Missing ELF export '+name)
def make(elf,asset,app_id,key,name,dual=False,storage=None,entry_stack=0,code_cache=0,bluetooth=False,usb=False):
    assert 0x80000000<=app_id<0xffffffff and len(key.encode())<=12 and len(name.encode())<32
    assert elf_export(elf,'zion_plugin_abi')==1
    h=bytearray(HEADER);payload=elf+asset
    struct.pack_into('<4sHH10I',h,0,b'ZPK1',1,HEADER,3 if dual else 1,HEADER+len(payload),app_id,1,
        elf_export(elf,'zion_plugin_api_size'),HEADER,len(elf),HEADER+len(elf),len(asset),0)
    struct.pack_into('<I',h,156,entry_stack)
    h[48:48+len(name.encode())]=name.encode();h[88:88+len(key)]=key.encode()
    if storage:
        filesystem=storage.get('filesystem','fatfs')
        if filesystem not in ('fatfs','spiffs'):
            raise ValueError('Unsupported storage filesystem: '+str(storage['filesystem']))
        label=storage['label'].encode();size=storage['bytes']
        assert len(label)<16 and size>=4096 and size%4096==0
        struct.pack_into('<I',h,44,34 if filesystem=='spiffs' else 2);struct.pack_into('<I',h,136,size);h[140:140+len(label)]=label
    if code_cache:
        assert code_cache%4096==0
        struct.pack_into('<I',h,44,struct.unpack_from('<I',h,44)[0]|4)
        struct.pack_into('<I',h,160,code_cache)
    if bluetooth:
        struct.pack_into('<I',h,44,struct.unpack_from('<I',h,44)[0]|8)
    if usb:
        struct.pack_into('<I',h,44,struct.unpack_from('<I',h,44)[0]|16)
    h[104:136]=hashlib.sha256(payload).digest();struct.pack_into('<I',h,252,zlib.crc32(h[:252]))
    return bytes(h)+payload
def parse(data):
    assert data[:4]==b'ZPK1' and len(data)>=HEADER
    flags,total,app_id,abi,api,elf_off,elf_size,asset_off,asset_size,_=struct.unpack_from('<10I',data,8)
    assert total==len(data) and flags in (1,3) and abi==1
    assert zlib.crc32(data[:252])==struct.unpack_from('<I',data,252)[0]
    assert elf_off==HEADER and asset_off==elf_off+elf_size and asset_off+asset_size==total
    assert hashlib.sha256(data[HEADER:]).digest()==data[104:136]
    return dict(flags=flags,total=total,id=app_id,abi=abi,api_size=api,elf_bytes=elf_size,asset_bytes=asset_size,
        key=data[88:104].split(b'\0')[0].decode(),name=data[48:88].split(b'\0')[0].decode())
def append_dual(firmware,package):
    assert parse(package)['flags']==3
    return firmware+package+struct.pack('<8sII',b'ZIONDUAL',len(package),zlib.crc32(package))
def extract(data):
    if data.startswith(b'ZPK1'): return data,'plugin-only'
    if len(data)>=16 and data[-16:-8]==b'ZIONDUAL':
        size,crc=struct.unpack_from('<II',data,len(data)-8);assert size<=len(data)-16
        package=data[-16-size:-16];assert zlib.crc32(package)==crc and parse(package)['flags']==3
        return package,'dual'
    return None,'standalone-only'
