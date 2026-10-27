[简体中文](runtime-applications.zh_CN.md) · **English**

# External applications: current experiment

Checkout: `apps/zion-runtime/firmware`, branch `feature/shared-runtime`, ESP-IDF
**6.0.1**. The maintained community product remains `apps/zion-online/firmware`.
Only minimal XiaoZhi is built in; all other examples are externally installed.
No commit, push or publication is part of this experiment.

## Lifetime and shared services

The list enters one native application without reset. Leaving/changing apps
requires power off/on; hold Up at power-on to bypass a default. Long OK belongs
to the app and never restarts or returns to ZION. Legacy complete firmware
retains its reset-and-boot path instead of becoming a native plugin.

The C ABI supplies text, keys, settings, battery, network state, 16 kHz mono
duplex PCM, glyphs, assets and RGB565 drawing. It exposes neither C++ objects
nor ESP-IDF/LVGL handles. This explicit table does not supply general HTTP/MQTT.
The separate source-compatible linking path resolves real SDK HTTP/MQTT, FAT and
Opus calls; see [community validation](community-link-validation.md). XiaoZhi
still uses a static native C++ component and is not an external plugin.

Handoff deletes the manager screen and releases BLE; offline apps also stop
the network worker, free its queue and deinitialize Wi-Fi. The 8 KiB installer
console becomes a 2 KiB USB control console. USB remains reachable and refuses
installation while an app is active; ROM USB flashing is independent. The main
manager task exits, and a one-shot idle hook wakes the app after task cleanup.
Input then goes directly to the app queue. Required display/input/storage
services survive; XiaoZhi retains networking. There is no hot unload.

## Package forms

| Form | Payload | Verified route |
| --- | --- | --- |
| Plugin-only `.zion` | 256-byte ZPK1 header, ELF, optional assets | USB DATA install; direct native entry |
| Dual `.bin` | Complete firmware, ZPK1, `ZIONDUAL` trailer | CLI extracts the plugin; full BIN also boots independently |
| Standalone-only `.bin` | Ordinary complete firmware | Direct flash; USB compatibility install then legacy boot |

The narrow USB compatibility route extracts a factory APP and rejects private
DATA partitions. The inherited community installer has its separate full-image
and DATA path. Official mini-program distribution of native plugins is **not
verified**; ordinary BINs do not automatically become shared-runtime apps.

ZPK1 validates lengths, bounded UTF-8 name/key, identity, ABI/API size, header
CRC32 and payload SHA-256. Stable-ID DATA records use the canonical allocator.
Updates verify a new extent before switching records; failed uncommitted writes
are erased. Uncertain commits preserve potentially live bytes. USB legacy APPs
use reserved DIRTY staging, raw-APP parsing and SHA-derived identity; temporary
storage is reclaimed/recovered. Native default/delete use the existing menu
model; the built-in XiaoZhi cannot be deleted.

## Real source workloads

| Example | Source and port scope |
| --- | --- |
| Gallery | [PhoenixZHC/folotoy_gallery](https://github.com/PhoenixZHC/folotoy_gallery), `00cbde9d2c9f7998f1690e44c7dcc8ed21cbb63f`: FGA1 full/delta parser and scheduler; not the complete web/FAT/music app |
| SoundLab | [Liutupi/nabo-sound-lab](https://github.com/Liutupi/nabo-sound-lab), `e16007ec78a73ac28b9ea3c0b7a3f6a19f30139b`: ADPCM, twelve effects, PCM and settings; no artwork/full menu/ESP-NOW |
| NES CPU | [oyf518/esp32-nes-card](https://github.com/oyf518/esp32-nes-card), `1dba1b9ed026d03d58a7c2d61dfc1b508175f4c9`: unmodified Nofrendo CPU with synthetic ALU/branch/stack/IRQ/NMI workload; not full PPU/APU/BLE gameplay |

Original notices/licenses and provenance remain in each example. No third-party
game ROM or restricted NABO artwork/voice asset is included. SoundLab replaces
initialization-only libm tables with precomputed values and compares all output
PCM bytes against unmodified upstream C for twelve effects. CPU reference:
819,539 cycles, hash 1,738,864,307, PC 32,800, IRQ 19, NMI 29 after 200 slices.

ELFs are restricted RV32IM/ILP32 `ET_REL`. The verified file is read-only mapped
from Flash; relocated code/data live in RAM. File cap is 256 KiB, code budget
64 KiB, data budget 256 KiB; real heap/contiguous-block limits are tighter.
The O2 CPU module required 48,196 contiguous code bytes and failed in a measured
43,008-byte layout. Its Os form uses 35,228 code bytes and passes the same
reference. The standalone CPU core remains O2. Allocation history matters;
these figures are observations, not fixed universal memory budgets.

The profile sets `CONFIG_ESP_SYSTEM_MEMPROT=n`: trusted native code shares one
address space with no process isolation. No eFuse configuration was changed.
RVC/PIC, TLS, general C++ initialization/unwinding and unsupported relocations
are rejected. See [the gallery memory comparison](runtime-memory.md).

## Reproduce and interpret results

Activate `../../../build-tools/Enter-ESP-IDF6.ps1` from this checkout, then run:

```powershell
.\tools\build-runtime.ps1 -DeviceTest -BuildDirectory build-layout-apps
python tools/build-runtime-plugins.py
python tools/build-soundlab-plugin.py
python tools/build-nescpu-plugin.py --opt Os
python tools/test-runtime-host.py --cc PATH_TO_HOST_CLANG
python tools/test-soundlab-reference.py --cc PATH_TO_HOST_CLANG
```

Use `-Simulator` for the explicitly radio/PCM-free simulator profile; a skipped
PCM check is not hardware audio validation. `-DeviceTest` retains hardware and
adds serial key injection. Production packaging uses neither switch and a fresh
directory. Historical caches are in `../build-history`; their absolute paths
must not be reused. Factory size is `0x260000` at `0x10000`; identity remains at
`0x356000`, store `0x35A000`, OTA metadata `0x7FE000`. The old 2 MiB updater is
not compatible with this experimental layout.

Packages: `release/test-apps/`. Device receipts: `diagnostics/soundlab/` and
`diagnostics/nescpu/`; the aggregate matrix distinguishes build, host and device
results. Mechanical feel, camera-observed display, audio fidelity, cloud/network
concurrency and complete source-app ports are not established by serial/muted
tests. The inherited full gate still encounters imported bilingual-document
failures and is not PASS. The 0.2.0 embedded examples and old
`runtime-final-report.json` are historical evidence only.

## 0.3.0 validation checkpoint

The newer 0.4.0 source-compatible build profile and its deliverables are
documented in [ESP-IDF source compatibility](idf-source-compat.md).

The authoritative aggregate is
[`runtime-external-app-matrix.json`](../diagnostics/runtime-external-app-matrix.json),
which binds package SHA-256 values to the individual receipts.

| Workload | Plugin only | Dual package | Standalone only |
| --- | --- | --- | --- |
| SoundLab | 21,720 bytes; native C3 PASS | 993,304 bytes; native and direct flash PASS | 971,568 bytes; direct flash and ZION legacy install PASS |
| NES CPU | 73,992 bytes; native C3 PASS | 1,013,752 bytes; native and direct flash PASS | 939,744 bytes; direct flash and ZION legacy install PASS |

The clean non-test host image is
`release/ZION-Runtime-0.3.0-runtime-9754f23fb304.bin` (2,375,728 bytes),
with a verified matching ELF/debug bundle. This image embeds neither test app.
Real-device workload automation used the DeviceTest profile, rather than this
non-test image. The simulator profile explicitly disables radio and PCM.

The new lifecycle regression passes default auto-entry, Up-held power-on escape,
delete cancellation, deletion of the default plugin, and persistent catalog
updates in PassportSim. Its continuous redraw causes the simulator's
`until_ui_settled` boot marker to time out; execution/golden/state assertions
passed with no HLE tripwire. Hardware menu tests additionally pass default
selection, cancel/delete, built-in deletion protection and reinstall. Deleting
the CPU package reclaimed 77,824 Flash bytes; reinstall restored the previous
free space. The hardware is left at the list with six entries and no default.
Serial ports were closed after testing.

The delete confirmation now names native packages, and the diagnostic default
field includes them. A corrupted full payload was rejected by SHA-256 without
changing the old catalog or free space. See `diagnostics/native-management/`,
`diagnostics/external-apps-sim/` and `diagnostics/package-rejection/`.

Build: PASS. Host tests: PASS. Device tests: PASS within these scopes.
The complete repository gate remains FAIL on inherited imported documentation;
see `../diagnostics/runtime-complete-gate-final.log`. This is an experimental
local fork; no commit, push, publication, or claim of full source-app support
is implied by this checkpoint.
