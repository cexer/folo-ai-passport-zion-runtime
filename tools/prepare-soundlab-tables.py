"""Freeze upstream initialization-only libm tables; validate against original C."""
from pathlib import Path
import math,json,hashlib
root=Path(__file__).resolve().parents[1]
up=root/'examples/plugins/soundlab/upstream'
original=(up/'nabo_voice.c').read_text(encoding='utf8')
def cround(x):return math.floor(x+.5) if x>=0 else math.ceil(x-.5)
tables={'SIN':[cround(32767*math.sin(2*math.pi*k/1024)) for k in range(1025)],
        'TANH':[cround(32767*math.tanh(k/128)) for k in range(513)]}
start=original.index('static int16_t SIN[');end=original.index('\n}',original.index('static void tables(void)',start))+2
replacement='\n'.join('static const int16_t '+key+'['+str(len(values))+']={'+','.join(map(str,values))+'};' for key,values in tables.items())
replacement+='\nstatic void tables(void) {}'
(up/'nabo_voice_fixed.c').write_text(original[:start]+replacement+original[end:],encoding='utf8')
provenance=dict(repository='https://github.com/Liutupi/nabo-sound-lab',commit='e16007ec78a73ac28b9ea3c0b7a3f6a19f30139b',
    original_sha256=hashlib.sha256((up/'nabo_voice.c').read_bytes()).hexdigest(),license='MIT (code only; no artwork or voice assets copied)',
    adaptation='Precomputed integer SIN/TANH tables replace initialization-only libm; DSP unchanged')
(up.parent/'source.json').write_text(json.dumps(provenance,indent=2)+'\n')
