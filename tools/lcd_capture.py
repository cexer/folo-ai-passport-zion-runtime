"""Strict reconstruction of complete native RGB565 display rows."""
def combine_chunks(chunks,width,height):
    assert set(chunks)==set(range(height)), 'Missing LCD chunk rows'
    rows={}
    for y,parts in chunks.items():
        row=bytearray()
        for at,data in sorted(parts.items()):
            assert at==len(row) and 0<len(data)<=256, 'Missing, overlapping or oversized LCD chunk'
            row+=data
        assert len(row)==width*2, 'Incomplete LCD chunk row'
        rows[y]=bytes(row)
    return rows

def rgb888(rows,width,height):
    assert (width,height) in ((240,320),(320,240)), 'Unsupported panel geometry'
    assert set(rows)==set(range(height)), 'Missing or out-of-range LCD rows'
    assert all(len(row)==width*2 for row in rows.values()), 'Malformed LCD row width'
    rgb=bytearray()
    for y in range(height):
        row=rows[y]
        for i in range(0,len(row),2):
            v=int.from_bytes(row[i:i+2],'big')
            rgb+=bytes(((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
    return bytes(rgb)
