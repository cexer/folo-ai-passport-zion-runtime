"""Focused native checks; the inherited full gate is still run separately."""
import argparse,json,shutil,subprocess,sys,tempfile,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default=shutil.which('cc') or shutil.which('clang'))
p.add_argument('--plugins',type=Path,default=root/'build-runtime-plugins')
p.add_argument('--compat',type=Path);args=p.parse_args()
if not args.cc: p.error('Provide a host C compiler with --cc.')
cases={
 'button-events':['tests/test_button_events.c'],
 'link-hooks':['tests/test_link_hooks.c','main/zion_idf_link_hooks.c'],
 'stdio-control':['tests/test_stdio_control.c','tests/firmware/stdio_control.c'],
 'native-stdio-control':['tests/test_stdio_control.c','tests/firmware/native_stdio_control.c'],
 'runtime-model':['tests/test_zion_runtime_model.c','main/zion_runtime_model.c'],
 'elf-loader':['tests/test_zion_elf.c','main/zion_elf.c'],
 'gallery-streaming':['tests/test_gallery_streaming.c','examples/plugins/gallery/upstream/fga_format.c'],
 'zion-policy':['tests/test_zion_policy.c','main/zion_policy.c'],
 'zion-menu':['tests/test_zion_menu.c','main/zion_menu.c'],
 'wire':['tests/test_zion592_wire.c','main/zion592_wire.c'],
 'portal-dns-parser':['tests/test_zion_portal.c','main/zion_portal.c'],
 'codec-sleep-readback':['tests/test_bsp_es8311_sleep_check.c','components/bsp/src/bsp_es8311_sleep_check.c'],
 'codec-recovery':['tests/test_bsp_audio_recovery.c','components/bsp/src/bsp_es8311_sleep_check.c'],
 'runtime-layout':['tests/test_runtime_layout.c','main/meta_carve.c','main/meta_md5.c','main/meta_name.c','main/zion_package_format.c','main/meta_carve_store.c'],
 'package-format':['tests/test_zion_package.c','main/zion_package_format.c','main/meta_carve_store.c','main/meta_carve.c','main/meta_md5.c','main/meta_name.c'],
 'prefs-flash':['tests/test_zion_prefs_flash.c','main/zion_prefs.c','main/zion_policy.c'],
}
results={}
with tempfile.TemporaryDirectory(prefix='zion-runtime-tests-') as directory:
    for name,sources in cases.items():
        exe=Path(directory)/(name+'.exe' if sys.platform=='win32' else name)
        cmd=[args.cc,'-std=c11','-Wall','-Wextra','-Werror','-D_CRT_SECURE_NO_WARNINGS',
             '-I'+str(root/'main'),'-I'+str(root/'components/zion_sdk/include'),
             '-I'+str(root/'examples/plugins/gallery/upstream'),*map(lambda s:str(root/s),sources),'-o',str(exe)]
        if name=='prefs-flash':cmd.append('-I'+str(root/'tests/esp_stubs'))
        if name=='native-stdio-control':cmd+=['-DZION_NATIVE_STDIO_TEST=1','-Dzion_test_fgetc=__wrap_fgetc','-Dzion_test_usb_read=__wrap_usb_serial_jtag_read_bytes']
        if name.startswith('codec-'):cmd+=['-I'+str(root/'components/bsp/src'),'-I'+str(root/'components/bsp/include')]
        if name=='codec-recovery':cmd.insert(1,'-I'+str(root/'tests/audio_stubs'))
        if name=='runtime-layout':
            factory=re.search(r'^CONFIG_ZION_FACTORY_SIZE=(\d+)',(root/'sdkconfig.with-xiaozhi.defaults').read_text(),re.M)
            assert factory;cmd.append('-DCONFIG_ZION_FACTORY_SIZE='+factory[1])
        subprocess.run(cmd,check=True)
        inputs=[[]]
        if name=='elf-loader':
            inputs=[[str(args.plugins/file)] for file in ['counter.elf','gallery.elf','soundlab.elf','nescpu.elf'] if (args.plugins/file).exists()]
            if args.compat:
                compat=json.loads((args.compat/'manifest.json').read_text())
                inputs.append([str(args.compat/'app.elf'),'zion_idf_contract_'+compat['contract']])
        if name=='gallery-streaming': inputs=[[str(args.plugins/'gallery.fga')]]
        if name=='package-format': inputs=[[]]+[[str(root/'release/test-apps'/file)] for file in ['ZION-gallery-0.3.0.zion','ZION-SoundLab-0.3.0.zion','ZION-NESCPU-0.3.0.zion'] if (root/'release/test-apps'/file).exists()]
        for arguments in inputs: subprocess.run([str(exe),*arguments],check=True)
        results[name]='PASS'
subprocess.run([sys.executable,str(root/'tests/test_runtime_contract.py')],check=True)
results['font-and-power-contract']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_zionctl_transport.py')],check=True)
results['usb-client-fragments']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_device_protocol.py')],check=True)
results['device-record-fragments']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_audio_silence.py'),'--cc',args.cc],check=True)
results['test-profile-silence']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_zion_dns_bridge.py'),'--cc',args.cc],check=True)
results['shared-dns-bridge']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_button_profile.py'),'--cc',args.cc],check=True)
results['adc-button-profile']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_adc_pulse.py'),'--cc',args.cc],check=True)
results['timed-adc-input']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_plugin_ota_guard.py'),'--cc',args.cc],check=True)
results['plugin-ota-guard']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_independent_layout.py')],check=True)
results['independent-ota-layout']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_lcd_capture.py')],check=True)
results['native-lcd-geometries']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_clean_streak.py')],check=True)
results['unchanged-base-streak']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_sleep_guard.py'),'--cc',args.cc],check=True)
results['unattended-test-sleep-guard']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_dpp_null_adapter.py'),'--cc',args.cc],check=True)
results['sdk5-dpp-null-adapter']='PASS'
subprocess.run([sys.executable,str(root/'tests/test_usb_control_worker.py'),'--cc',args.cc],check=True)
results['usb-control-worker']='PASS'
out=root/'diagnostics/runtime-host-tests.json';out.parent.mkdir(exist_ok=True)
out.write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
