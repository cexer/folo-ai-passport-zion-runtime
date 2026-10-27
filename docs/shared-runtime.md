[简体中文](shared-runtime.zh_CN.md) · **English**

# ZION shared runtime and native ELF experiments

Historical 0.2.0 embedded-example notes follow. For the current external-package
implementation and real-device scope, use [runtime-applications.md](runtime-applications.md).

Local checkout: `folo-ai-passport-zion-runtime`, branch `feature/shared-runtime`.
The source snapshot came from `zion-with-xiaozhi` (Git base
`f0e2e28d4a04a287d48bb3d9ace06b1f93c60661`, including its uncommitted work).
The production checkout and original experiment were preserved. No GitHub fork,
commit, push or community publication was performed.

## Runtime model

ZION owns the display/LVGL task, ADC keys, I2C, battery, Wi-Fi provisioning and
connection, integer settings, and 16 kHz mono duplex PCM. Native applications
use a versioned C service table, with no public LVGL/ESP-IDF handles or STL ABI.
The minimal XiaoZhi board/display/audio adapters reuse those services and no
longer initialize a second screen, button unit, Wi-Fi manager or codec device.

The list enters one application directly, without resetting. Leaving requires
power off/on. Hold Up while powering on to bypass a default. Long OK belongs to
the application and **does not restart or return to ZION**. There is no unloading
or second application in the same boot. Installer requests are refused while a
native app owns the frontend; close the provisioning hotspot before launching.

XiaoZhi is still a statically registered native component. Its C++ framework,
protocol and Opus processing are not yet a complete independent C SDK plugin.
The two ELF examples below actually use the new independent loading path.

## Independent modules

`tools/build-runtime-plugins.py` separately compiles RV32IM, ILP32, little-endian
`ET_REL` files without linking ESP-IDF, BSP, LVGL or network libraries into them.
`main/zion_elf.c` validates section/symbol/relocation bounds, ABI and service-table
size, allocates code/data, relocates and resolves a small libc/helper allowlist.
The module receives the host service table as its entry argument. Code and data
use their appropriate C3 instruction/data aliases; the simulator executes the
loaded instructions in guest RAM. The host does not link their entry symbols.

This intentionally restricted loader is not a Linux `.so` implementation. TLS,
C++ initialization/unwinding, arbitrary relocation types, PIC and compressed
instruction modules are not supported. Follow the supplied compiler flags.
The experimental profile sets `CONFIG_ESP_SYSTEM_MEMPROT=n`, because the default
C3 data/instruction split prohibits executable heap allocations. This is a
trusted native-code platform sharing an address space, not process isolation.
No eFuse setting or physical device was changed.

For repeatable tests, independent ELF files and a media fixture are embedded as
**read-only file data**, then loaded/relocated at runtime. Their code is not
statically linked into the host code segment. The package also delivers the ELF
files separately. Filesystem/USB/mini-program installation of new ELF packages
and an extensible plugin catalog are **not implemented yet**. Ordinary community
BINs retain the inherited complete-firmware path; they do not become ELF apps
without source adaptation. The official mini-program plugin distribution
proposal remains unverified.

## Complex community candidate

Selected graphics-heavy candidate: [community play 9](https://ai-passport.folotoy.cn/plays/9/),
[PhoenixZHC/folotoy_gallery](https://github.com/PhoenixZHC/folotoy_gallery), commit
`00cbde9d2c9f7998f1690e44c7dcc8ed21cbb63f`. Its original V1.4.0 source builds
under IDF 5.5.3; APP size is 1,033,392 bytes. This is a representative stress
candidate, not a verified ranking of every community project.

The example ports its original FGA1 parser and animation scheduler. The source
keeps GIF conversion in the phone browser; the device parses FGA1, not GIF/MOV.
The ZION adaptation adds a streaming output sink, with parity tests against the
contiguous decoder. ZION allocates five 30 KiB pixel tiles lazily, avoiding a
150 KiB contiguous allocation. LVGL locking covers decoding and presentation.
All 240 by 320 rendered pixels are checked against an independent decoder.

Measured independent files: counter 3,008 bytes; gallery core 10,356 bytes;
generated FGA1 media 158,140 bytes. The media fixture is an original geometric
pattern with a full frame and delta frames, not borrowed photographs. Pixel
storage still consumes 150 KiB of working RAM. These small module sizes must
not be described as a full port of Gallery: its upload website, FAT storage,
music, recording and complete settings UI are not included in the module.
Source provenance and adaptation scope are in `examples/plugins/gallery/source.json`.

## Build and validation

Use ESP-IDF **6.0.1** for this fork (minimal XiaoZhi requires it):

```powershell
. ..\built-tools\Enter-ESP-IDF6.ps1
.\tools\build-runtime.ps1 -BuildDirectory build-runtime-delivery
.\tools\build-runtime.ps1 -Simulator
python -m pip install --no-user -r tools/requirements-runtime-test.txt
python tools/test-runtime-host.py --cc <host-C-compiler>
python tools/test-runtime-sim.py --sim ..\PassportSim\passportsim-1.0.3-windows-x64\passportsim.exe
```

Use a fresh directory for release validation. The production helper builds,
merges and checks ESP checksums/SHA, ELF binding, partition MD5/bounds, erased
NVS, APP/bootloader/table factory geometry and exact embedded module identities.
Versioned artifacts go to `release/`; matching debug artifacts and independent
modules are in `dist/zion-runtime/<hash12>/`. The full BIN boots from `0x0`;
never write an APP-only file there. Factory remains `0x260000`, with cardid at
`0x356000`, store `0x35A000` and OTA data `0x7FE000`. The old 2 MiB updater is
incompatible. There was no physical flashing.

The simulator profile disables unsupported SDK6 radio/PCM and uses an explicit
XiaoZhi display/input exerciser. ELF relocation/execution, FGA1 decoding,
animation, rendering, keys, NVS and power cycling run real guest code.
Tests cover direct launch, saved configuration after power cycling, default
startup, Up-key escape, long OK without reboot, animation pause/resume and three
pixel-verified frames. `run` returns PASS_WITH_CAVEATS (CLI exit 10), as defined
by PassportSim, not a hardware validation pass. Continuous launcher refresh can
also prevent its UI-settled marker; assertions use actual serial and frames.

Focused host tests cover malformed ELF/ABI rejection and allocation cleanup,
the one-app-per-boot model, key filtering, streaming/full-decoder parity and
tile guards, font coverage, existing menu/policy and installer wire parsing.
The inherited complete gate still fails first on imported XiaoZhi documents
without bilingual peers. It is not claimed as passing.

Exact firmware identities, sizes and results are recorded in
`diagnostics/runtime-final-report.json`; simulator receipts, sanitized serial
and PNGs are under `diagnostics/runtime-sim/`. Device tests: **NOT RUN**.
Unverified: real C3 executable-memory behavior and heap margin, Wi-Fi/BLE/TLS,
activation, microphone/TTS, streamed glyphs and actual mini-program installs.
The older `builtin-xiaozhi` report is historical baseline evidence only.
