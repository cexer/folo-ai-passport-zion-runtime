"""Reject partial/corrupt frames; verify exact colors in both orientations."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from lcd_capture import rgb888,combine_chunks
for width,height in [(240,320),(320,240)]:
    rows={y:b'\xf8\x00\x07\xe0\x00\x1f'+b'\0\0'*(width-3) for y in range(height)}
    result=rgb888(rows,width,height)
    assert result[:9]==b'\xff\0\0\0\xff\0\0\0\xff' and len(result)==width*height*3
    chunks={y:{at:row[at:at+256] for at in range(0,len(row),256)} for y,row in rows.items()}
    assert rgb888(combine_chunks(chunks,width,height),width,height)==result
    for broken in [{**chunks,0:{0:b'\0'*256}},
                   {**chunks,0:{0:b'\0'*256,128:b'\0'*256}},
                   {**chunks,0:{0:b'\0'*257}}]:
        try:combine_chunks(broken,width,height)
        except AssertionError:pass
        else:raise AssertionError('Corrupt LCD chunks accepted')
    for bad in [{k:v for k,v in rows.items() if k!=height-1},{**rows,0:rows[0][:-2]},{**rows,height:rows[0]}]:
        try:rgb888(bad,width,height)
        except AssertionError:pass
        else:raise AssertionError('Corrupt LCD frame accepted')
print('PASS portrait/landscape RGB565 and incomplete/corrupt rejection')
