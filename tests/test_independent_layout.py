"""Image selection supports factory and OTA-only, rejecting ambiguity/corruption."""
import pathlib,sys,struct
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]/'tools'))
from package_minimal import independent_app_partition,image_length
image=bytearray(48);image[0:2]=b'\xe9\x01';struct.pack_into('<H',image,12,5)
struct.pack_into('<II',image,24,0x3fc80000,4);image[32]=0x13;image[47]=0xef^0x13
assert image_length(image)==48
for subtype,offset in [(0,0x10000),(0x10,0x10000),(0x10,0x20000)]:
    full=bytearray(b'\xff'*0x40000);full[offset:offset+48]=image
    part=dict(type=0,subtype=subtype,offset=offset,size=0x10000)
    assert independent_app_partition(full,[part])==part
    damaged=full[:];damaged[offset+47]^=1
    try:independent_app_partition(damaged,[part]);raise RuntimeError('Accepted damaged image')
    except AssertionError:pass
    other=dict(type=0,subtype=0x11,offset=0x30000,size=0x10000)
    full[0x30000:0x30030]=image
    try:independent_app_partition(full,[part,other]);raise RuntimeError('Accepted two images')
    except AssertionError:pass
print('PASS: independent image factory/OTA-only offsets, corrupt image and multi-image rejection')
