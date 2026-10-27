"""Unchanged offline word notebook: reading, audio, study, quiz and progress."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--persistence',action='store_true');a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();d.capture('home')
    if a.persistence:
        for _ in range(4):d.key('DOWN')
        d.key('OK');d.capture('stats-after-reset')
    else:
        d.key('OK');d.capture('categories');d.key('OK');d.capture('words')
        d.key('OK');detail=d.capture('detail');start=len(d.lines);d.key('OK');time.sleep(1)
        r['silenced_audio']=d.proof('I2S_ZERO_OUTPUT|I2S_ZERO_PROGRESS',start,15)
        d.key('DOWN');next_word=d.capture('next-word');r['word_navigation_pixels']=d.changed(detail,next_word,(0,40,240,285))
        d.key('UP');d.key('LONG');d.key('LONG') # details -> all words -> home
        d.key('DOWN');d.key('OK');front=d.capture('study-front')
        d.key('OK');back=d.capture('study-back');r['reveal_pixels']=d.changed(front,back,(0,40,240,285))
        d.key('DOWN');d.key('OK');d.key('LONG')
        d.key('DOWN');d.key('OK');d.capture('quiz-question')
        d.key('DOWN');d.key('UP');d.key('OK');d.capture('quiz-answer')
        for _ in range(9):d.key('OK');d.key('OK')
        d.key('OK');d.capture('quiz-finished');d.key('OK') # summary -> home
        d.key('DOWN');d.key('OK');d.capture('collection');d.key('LONG')
        d.key('DOWN');d.key('OK');time.sleep(2);d.capture('stats')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='stored progress after reset' if a.persistence else 'word/category/detail navigation, actual silent ADPCM playback, study reveal, ten-question quiz, collection page, stats, UP/DOWN/OK/LONG'
    r['unverified']=['Double-OK collection toggle','All 850 audio clips','Physical sound quality and switch contacts']
finally:d.close(r)
print(json.dumps(r,indent=2))
