"""Test-build real LVGL glyph decoder and non-destructive UI queue acceptance."""
import argparse
import json
from pathlib import Path
import time
from zion_ui import ZionDevice
p=argparse.ArgumentParser()
p.add_argument('--port',required=True);p.add_argument('--report',required=True)
args=p.parse_args();d=ZionDevice(args.port,initial_key='UP');report={'passed':False,'checks':[]}
try:
    d.wait(busy=0);d.home()
    d.command('ZION FONTS');d.pump(.5)
    assert any('ZION_FONTS ui_bpp=4 small_bpp=4 fallback_bpp=4' in x for x in d.trace)
    assert any('ZION_FONT_TEST pass=1 ' in x for x in d.trace), 'Actual LVGL glyph pixel/fallback comparison failed or was not observed'
    report['checks'].append('Real LVGL A8 decoded all native UI and small ASCII glyphs against raw 4-bpp pixels, plus full 4-bpp fallback')
    initial=d.wait(page=0,busy=0,footer_fixed=1,footer_right=230);report['initial']=initial
    if initial['apps']:
        d.select_list(0);d.key('LONG');d.wait(page=1,selected=0)
        d.key('DOWN');d.key('DOWN');d.key('OK');d.wait(page=2,selected=0)
        d.key('LONG');d.wait(page=1);d.key('LONG');d.wait(page=0)
        report['checks'].append('Management and delete confirmation visited; no delete/default action performed')
    d.select_list(initial['apps']+1);d.key('OK');d.wait(page=3,selected=0)
    d.key('OK');d.wait(page=4,ap=1,busy=0)
    d.key('OK');d.wait(page=3,ap=0,busy=0);d.key('LONG');d.wait(page=0,footer_fixed=1,footer_right=230)
    report['checks'].append('Settings and Wi-Fi page navigation; real AP opened/closed; credentials not changed')
    d.select_list(initial['apps']);d.key('OK');d.wait(page=5,qr=1,busy=0)
    d.key('LONG');final=d.wait(page=0,busy=0,apps=initial['apps'],free=initial['free'],footer_fixed=1,footer_right=230)
    report['checks'].append('Official Mini Program code present; returned to list with installed games/capacity preserved')
    report['checks'].append('Persistent list instructions and right-aligned long-OK hint survive AP actions and returning to the list')
    assert final['default']==initial['default']
    report['final']=final;report['passed']=True
    print('Real device fonts/navigation: PASS (actual LVGL pixel decode/fallback; UI queue, not physical ADC or camera appearance)')
finally:
    report['trace']=d.trace
    Path(args.report).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    d.close()
