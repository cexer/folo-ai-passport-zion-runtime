"""Match intact protocol lines or adjacent fragments with a known prefix."""
import re
def match_record(pattern,lines):
    if not lines:return None
    if re.search(pattern,lines[-1]):return lines[-1]
    prefixes=('ZION_','STANDALONE_')
    # Liquid Glass's own USB/stdio path also splits one ACK into three pieces.
    # Preserve raw lines and join only adjacent fragments up to a small bound.
    for start in range(len(lines)-2,max(-1,len(lines)-9),-1):
        parts=lines[start:]
        if any(re.match(r'^(?:ZION_|STANDALONE_|[IWE] \()',s) for s in parts[1:]):continue
        first=parts[0]
        if not any(p.startswith(first) or first.startswith(p) for p in prefixes):continue
        joined=''.join(parts);found=re.search(pattern,joined)
        # Never acknowledge a new command using a complete old ACK.
        if found and found.end()>len(joined)-len(parts[-1]):return joined
    return None
