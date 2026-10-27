[简体中文](community-link-validation.zh_CN.md) · **English**

# Community applications through compilation and linking

**Paused at the user's request.** Start from the
[2026-10-28 handoff](community-validation-paused.md) before resuming.

## New-device checkpoint (2026-10-28)

Passport OS (438), unchanged revision `b70879b`, passed plugin startup,
UP/DOWN/short-OK/long-OK, stopwatch, active Tetris and settings on the new COM7
C3/8MB device. Actual LCD images were reviewed. Generic metadata/staging/arena
changes were required, so the clean streak remains **0/3**. Native core flow
previously passed using the identical APP and an identity-preserving relocated
partition layout. The hybrid BIN's extracted plugin passed the same physical
flow on COM7; this is the USB-client path, not a miniapp download test. Scope is tracked in
`diagnostics/community/passport-os/report.json`; no complete ten-app acceptance
or stock merged-image flash is implied.

## Unchanged-base streak checkpoint (2026-10-27)

Current counter: **0/3**. Each new case below exposed missing base providers;
simulator passes do not count as physical-device acceptance. Optimization
background, triggering applications and code references are maintained in
[runtime provenance](runtime-optimization-log.md).

- [Snoopy Passport](https://github.com/Juicebro-star/snoopy-passport), revision
  `6d9200e8112a61b16bf093d8c1f773f980394166`: unchanged six C translation units,
  50 states/200 frames, feeding, five-round game, gallery, status and manual time
  editor passed on the device. Nine exports added. Bundle
  `dist/community-link-validation/19a41bf1b51a/`; the native/hybrid scopes and one
  missing native serial ACK are preserved in its report.
- [Connect Four](https://github.com/Shinku-Chen/ai-passport/tree/feature/connect-four):
  unchanged application and private BLE transport, actual AI moves in both
  starting orders, two-player drops, landscape display, keys and BLE discovery
  start/stop passed under `5f67f4cdc172`. Bundle
  `dist/community-link-validation/connect-four-validated/5f67f4cdc172/`.
  Hybrid device routes remain pending. COM5 disappeared; idle sleep and USB
  movement are possible causes, and the user says the cause is uncertain.
  The DeviceTest-only sleep-to-reset guard is now flashed with the latest host.
- [Liquid Glass UI](https://github.com/EnglandLobster/ai-passport-liquid-glass-ui),
  revision `6e9bbd9d402fe9a1a9c58cbb4009210cd0a01ef1`: unchanged 25 translation
  units, 153,600-byte wallpaper, all three package formats build. Added 44 SDK
  providers. A separate radio-free simulator host executes all eight scenes,
  focus/actions and teardown/reentry; physical testing is pending USB wake.
  Bundle `dist/community-link-validation/liquid-glass-pending/fe2972af1225/`
  contains the physical-target host and explicitly labeled simulator screenshots.
- Far Away was excluded before compilation: the 6,937,968-byte sprite blob alone
  exceeds the 4,841,472-byte maximum install pool. Nothing was pruned to force a pass.

Latest physical-target APP: 2,677,856 bytes; factory capacity 2,678,784 bytes
(`0x28e000`). APP/bootloader configuration and the actual partition table agree.
The protected identity and larger second pool retain their addresses. Compared
with `5f67f4cdc172`, the latest static DRAM BSS grows by 16 bytes; data and the
SRAM alias boundary are unchanged. These are ELF measurements, not a new device
heap measurement. No unrelated old application flows were replayed.

Build: PASS for these scoped DeviceTest/native/simulator products. Host tests:
PASS (23 focused groups). Device tests: PASS for the recorded Snoopy/Connect Four
scopes; NOT RUN for the latest Liquid Glass target. Complete gate: FAIL on the
unchanged imported XiaoZhi bilingual-document checks. This is not a production
release or complete product acceptance. State and pending device actions:
`diagnostics/community/clean-streak/state.json`.

## Octo Radio memory follow-up (2026-10-27)

The former MP3 failure below is superseded by the unchanged-source memory
follow-up in [runtime memory](runtime-memory.md). Read-only relocation caching,
allocation ordering, lazy frame workspace and compact catalog metadata are
general host/loader changes. Module RAM data falls from 29,732 to 22,276 bytes.
Both plugin routes pass 25-second real I2S progress observations without new
allocation failures and pass station changes, pause/resume, animation and settings.
The mixed BIN's independent route also passes; the unchanged independent-only
binary retains its previous pass. Its original adaptive jitter buffer falls
back from 8 KiB to 6 KiB during startup, without changing business sources.

Bundle: `dist/community-link-validation/60cd2e31d2ab/`; host APP 2,654,256 bytes.
This is a muted local DeviceTest bundle, not production firmware. The later
math-only export addition does not trigger another complete radio regression.

## Space Pinball: images, physics and synthesized effects

[Source](https://github.com/YeatsLiao/ai-passport-pinball), revision
`60e331f17f89adf52fe4dedb0aa1e63daec28f4e`. All seven original C units and the
258,072-byte image blob remain unchanged; the candidate is Git clean. Its
official community play ID has not been resolved. The shared host adds real
`cosf`, `expf`, `fabsf` and `sqrtf` providers, reaching 536 automatic imports.

All four routes pass: independent-only, mixed BIN independently flashed from
`0x0`, mixed BIN installed as a plugin, and plugin-only. Actual LCD captures show
settings, guide paging, charged launch, moving physics, pause-menu selection and
resume. A cold reset preserves the saved settings panel pixels. Audio reaches
real I2S with codec mute and zero PCM; no acoustic quality claim is made.

The original game polls ADC and rejects early title input for 0.3 game seconds.
The test waits for the title to settle, verifies menu selection, then uses
500 ms press samples and a 5-second pause hold. Earlier short/early input and
split-USB-record attempts are retained as instrumentation failures. Native ADC
profiles omit callback-edge commands absent from this older BSP. No game logic
or rendering/physics details were rewritten to make the test pass.

Bundle: `dist/community-link-validation/11d86f83df99/`: host APP 2,655,360 bytes,
independent BIN 1,095,936, plugin 346,704, mixed BIN 1,442,656. The plugin is
larger than 256 KiB; code and read-only assets use Flash rather than copying the
whole file to RAM. Loaded code is 29,516 bytes, RAM data 21,508 and directly
borrowed read-only data 261,479. Native remaining heap is approximately 185 KiB
with a 112 KiB largest block; plugin remaining heap is approximately 122 KiB
with a 32--36 KiB largest block. SDK/LVGL allocator configurations differ, so
this difference is not a controlled measurement of framework overhead.

Build: PASS. Host tests: PASS (20 focused groups). Device tests: PASS within
the main-flow and persistence scopes above. Complete gate: FAIL on unchanged
imported XiaoZhi bilingual-document checks. Physical contacts/short-tap feel,
long-duration operation, acoustic quality and exhaustive scoring remain unverified.
See `diagnostics/community/pinball/report.json` for exact measurements.

## V-Pet: real BLE advertising and passive scanning

[Source](https://github.com/zqj372-ops/ai-passport-vpet), revision
`d53b1da151a3d3893be6ba679436036261267958`. All 13 original C units remain
unchanged. This is explicitly an **adapted application case**, not a claim that
the stock upstream firmware passes or that generic SDK exports fix app defects.

The unadapted independent program has an escaping automatic model: `app_main`
passes a stack object's address to `vpet_app_start`, which retains it globally,
then returns. An extra profile source uses GNU `--wrap=vpet_app_start` to retain
the model for the application lifetime. A host test checks independent storage
and return/argument forwarding using the actual candidate model type.

Its native LVGL pool also exhausts during hatching: the matched probe ELF
`f974b41e1728455deb619830ae8cc87c122a43a2a520730494b58560eee892e8`
logs an 88-byte request with 48 bytes free/largest, maximum used 21,272 and
usable pool 21,320. Native test configuration increases the private pool from
24 to 64 KiB; the plugin already uses the host's C-library allocator. Neither
repair changes candidate business sources or belongs to the generic RAM savings.

The shared host enables NimBLE observer mode and adds eight providers: BLE
advertising state/stop, discovery/start-cancel, label text, tick and timer
pause/resume. There are now 544 automatic imports. APP is 2,657,232 bytes and
still fits the original `0x289000` factory partition. IRAM, DRAM dummy/data/BSS
addresses and sizes exactly match the preceding Pinball host; old applications
were not replayed after these additions, per the user's focused-test instruction.

Independent and pure-plugin main flows pass training, status, keys and actual
bidirectional BLE RF. Windows transmits a deterministic 17-byte test battle card;
the ESP receives it and enters battle, while Windows observes the ESP's VPET
manufacturer advertising. This is real radio traffic, not an injected GAP event.
After reset, training/battle counter pixels match the saved status panel. A
settled battle result is allowed to remain unchanged; exhaustive battle rules
are outside this check.

Both mixed-package routes pass actual installation/flashing, startup, keys and
LCD smoke checks. They do not repeat the complete business flow. Native polling
stdio lost an ACK during audio logging; test-owned USB now uses the same buffered
console pattern as the host. App-owned USB remains under application control.

Bundle: `dist/community-link-validation/26b2b3920f2d/`: host APP 2,657,232 bytes,
independent BIN 1,167,968, plugin 134,412, mixed BIN 1,302,396. Module code is
29,192 bytes, RAM data 732, borrowed read-only data 45,539 and relocated read-only
cache 3,012. After BLE activity, native free heap is 82,788 bytes/largest 73,728;
plugin free heap is 103,640/largest 34,816. SDK versions and native private-pool
configuration differ; this is not a controlled framework-overhead subtraction.

Build: PASS. Host tests: PASS (20 core groups plus the lifetime adapter).
Device tests: PASS with the repairs and scopes above; stock native attempts
remain recorded as FAIL. Complete gate retains the imported XiaoZhi document
failure. Two physical ESP peers, all evolutions/long care cycles, physical switch
contacts and acoustic quality remain unverified. Evidence is in
`diagnostics/community/vpet/report.json`; the matched pool-failure ELF/MAP are
preserved under `diagnostics/community/vpet/pool-probe/artifacts/`.

## Capacity preflight: Qiaopi Quiz

[Community play 753](https://ai-passport.folotoy.cn/plays/753/),
[source](https://github.com/liangdabiao/ai-passport-qiaopi), revision
`5871a0ecbc4d1d2af230a1b00dc812e16bd9dd18`. Its 5,298,976-byte audio blob alone
exceeds the current maximum contiguous installation pool of 4,841,472 bytes,
before ELF/font overhead. It was excluded by preflight with full resources
preserved. No device failure or RAM limit is inferred from this capacity check.

## Earlier audio and printer result

Both upstream repositories remain Git clean. Recorder's 8 C++ units and the
printer's 15 C units were built without business-source edits. The exact final
hybrid files were flashed and run independently, then their plugin payloads were
installed through ZION. Separate plugin-only packages were also installed and run.

| Application | Plugin file | Hybrid BIN | Main-flow result |
| --- | --- | --- | --- |
| Recorder | 132,776 bytes | 1,789,272 bytes | PASS: record, save, decode/play, volume keys and HTTP upload |
| Printer monitor | 709,920 bytes | 2,966,512 bytes | PASS: TLS/MQTT telemetry, pages/menu, APSTA and configuration/scan HTTP |

Deleting the recorder reclaimed 2,334,720 bytes including its private 2 MiB FAT
volume and code cache. Reinstallation restored exactly the original free space.
Installation information now counts those resources, rather than only file bytes.

Build: PASS. Host tests: PASS. Device tests: PASS within the scope below.
The complete inherited gate still fails imported XiaoZhi Markdown language/pair
checks; these unrelated checks were not changed. The board is left at the ZION
list with both apps installed and no default app.

The verified local test bundle is
`dist/community-link-validation/25d993e47029/` with `manifest.json` and
`test-report.json`. It contains matching ELF/MAP files and an identity-preserving
segmented flash plan. Its `host/*-0x0.bin` stops at `0x270000`; the separate store
and OTA writes are listed in the manifest. The test profile includes USB controls
and a local MQTT/TLS peer redirect, and is not a community release. Supplied Wi-Fi
credentials were checked to be absent from the archived binaries.

This experiment uses complete upstream business sources without editing their
function bodies or replacing their UI. The independent prefix runs the original
application; a ZION trailer contains an ELF built against the exact runtime SDK.
In these USB tests, `zionctl` extracts that trailer on the PC before transferring
it, instead of installing the duplicated independent prefix. Official miniapp
recognition of this trailer is not validated by these USB tests.

| Application | Source | Integration covered |
| --- | --- | --- |
| [Recorder, community 28](https://ai-passport.folotoy.cn/plays/28/) | [Acolevia/ai-passport-recorder](https://github.com/Acolevia/ai-passport-recorder) | C++17, Opus, microphone/I2S, FAT/wear levelling, background HTTP upload |
| [Bambu Print Pal, community 1209](https://ai-passport.folotoy.cn/plays/1209/) | [ericwubeta/ai-passport-bambu-monitor](https://github.com/ericwubeta/ai-passport-bambu-monitor) | C, large font/image data, animated boot, Wi-Fi/APSTA, TLS/MQTT, configuration HTTP server |

The community author for application 28 is Acolevia; its page currently omits a
GitHub URL. These tests use the author's source repository, not a claim that its
latest commit reproduces the currently downloadable community BIN byte for byte.

## Build changes

Profiles live in `examples/link-apps/recorder` and `bambu-monitor`. They list all
original translation units, includes, build definitions, stack and storage needs.
No candidate business source imports a ZION header or calls an explicit ZION SDK.
The wrapper for `app_main`, constructors and imports is generated separately.

The recorder's independent partition table and plugin storage request both use
2 MiB for recordings. That is a build/layout choice, smaller than upstream's
6.38 MiB, so it can coexist with ZION on 8 MiB Flash. Its existing `recordings`
label, `/rec` path, file format and application storage code are retained.

New shared capabilities include:

- Exact SDK exports for HTTP, TLS/MQTT, FAT, Opus, C++ containers and library helpers.
- Picolibc's supported errno-accessor compilation mode, backed by the host task's
  actual thread-local errno. User-defined TLS, exceptions and RTTI are not supplied.
- Immutable sections without relocation targets borrow the verified Flash mapping.
  Pointer-bearing read-only sections that need relocation remain writable copies.
- Code is relocated for its actual instruction-Flash address, written into a
  reserved tail of the package extent, read back, synchronized and executed there.
  The temporary RAM staging buffer is then freed. Old packages can still use RAM code.
- Executable staging is allocated per section, so a module's entire text does not
  need one contiguous RAM block. Instruction and data mappings use separate MMU
  pages, and executable addresses are checked before entry.
- Optional private FAT storage is allocated with the package, preserved on update,
  and removed with it. Standard partition lookup exposes the owning app's storage.
- LVGL's default theme is enabled for ordinary application styles, and its task
  has the 10 KiB stack required by the printer BSP. The validation USB key task
  uses 4 KiB; entering provisioning consumed about 2.3 KiB, exceeding the old 2 KiB.

There are no configured file/code/data byte quotas. Actual Flash placement, MMU
mapping and RAM allocations must still succeed, including the temporary relocation
buffer. The code cache and required MMU-page padding add installed Flash usage; this does
not duplicate the independent firmware's complete SDK.

## Real-device acceptance and test boundaries

Receipts, exact hashes and LCD-flush captures are in `diagnostics/community/`.
Tests cover primary screens, Up/Down/OK/long presses, and platform main flows.
They do not exhaust application rules. Inputs are injected into the real callback
path; physical switch contact/electrical behavior and acoustic quality are not measured.

The recorder uses real capture, Opus encode, filesystem save, decode/playback and
its unchanged HTTP uploader/server. The printer test uses real Wi-Fi, TLS and MQTT
with a small local peer publishing synthetic telemetry. It does not validate a
real printer, a Bambu account, SMS login or Bambu Cloud availability.

`tests/standalone-driver` adds test control through build/link options only.
`tests/bambu-network-driver` redirects the MQTT transport to the local TLS peer.
Both are validation components, not product features. Wi-Fi credentials are seeded
separately into device NVS and are excluded from firmware packages and source.
The computer has no wireless adapter, so joining the device AP from a phone remains
unverified; the real configuration server is accessed through the device's STA IP.

## Reproduce the plugin and hybrid builds

Activate ESP-IDF 6.0.1, build the matching runtime, then run:

```powershell
python tools/build-idf-compat.py examples/link-apps/recorder --host-build build-layout-apps --output build-link-recorder --standalone-full build-recorder-standalone/recorder-full.bin
python tools/build-idf-compat.py examples/link-apps/bambu-monitor --host-build build-layout-apps --output build-link-bambu --standalone-full build-bambu-standalone/bambu-full.bin
```

Each output contains `app.zion`, `app-dual.bin`, `app.elf` and a source/host manifest.
Independent projects use their own supported SDK: recorder 6.0.1, printer 5.5.3.
Native packages are specific to an exact host contract and require rebuilding when
that contract changes. Only one application runs per boot; leaving or changing it
requires power off/on, with Up held on power-on to bypass a default.

## Sequential HID/USB acceptance: iPhone Agent

The next candidate is [community 1251](https://ai-passport.folotoy.cn/plays/1251/),
from [lewangdev/ai-passport-iphone-agent](https://github.com/lewangdev/ai-passport-iphone-agent),
commit `1ac4c87f23915b057bb79544952fefdf44cfaede`. All 11 original C translation
units, including the Chinese font, remain unchanged and Git clean. The build-only
profile is `examples/link-apps/iphone-agent/compat.json`.

| Artifact | Bytes |
| --- | ---: |
| ZION DeviceTest application | 2,474,416 |
| Independent merged firmware | 2,324,672 |
| Plugin-only package | 822,364 |
| Independent + plugin hybrid BIN | 3,147,052 |

The test host grew by 31,408 bytes. It includes built-in minimal XiaoZhi, USB test
controls and the existing local MQTT redirect; this is not a production release.
Its ELF SHA-256 is `70ca4547ea385cdf8cf2089f13ee5d6dec49484f43cbd90604647a8d2b33d41a`;
contract `c4be2e434eda1fbf4b52afb48d31ae32`. Earlier bundles above retain their
earlier contract and require rebuilding for this host.

The loader maps 758,337 read-only bytes from Flash, copies 2,592 data bytes and
executes 16,388 code bytes from a 20,480-byte instruction cache. The installed
extent at `0x360000` is 872,448 bytes including alignment/cache. File size is not
RAM usage or the full installed footprint.

Reusable changes below the unchanged app:

- Feature bit `8`, requested by `bluetooth: true`, retains reinitializable BLE
  memory. ZION stops/deinitializes its host but skips the irreversible
  `esp_bt_mem_release`. Other apps still release BLE memory. HID services,
  persistent bonds and 47 new exact SDK imports bring the registry to 356 symbols.
- Feature bit `16`, requested by `usb: true`, transfers USB reader ownership.
  ZION's console retires rather than racing the app for bytes; initialization
  reuses an existing driver. Test controls share the app reader via a test-only
  demultiplexer. Production apps retain their own ordinary protocol.

Independent and plugin builds use SDK 5.5.3 and 6.0.1 respectively. The independent
partition configuration preserves `cardid` at `0x356000`. Wi-Fi credentials are
seeded separately into NVS and are absent from firmware artifacts.

Real-device checks cover USB hello/ping, pause/resume, Up/Down screens, long-Up
forgetting bonds, long-OK without reboot and actual LCD captures. The original
app also uses real Wi-Fi/TCP with a local ping-only peer. A real Windows adapter
observes HID UUID `0x1812`, connects and pairs; the device logs
`link encrypted, HID ready`. A neutral `scroll 0 0` returns `@ok` through the real
HID provider, and OK pause/resume remains responsive. No click, movement or typed
text is sent. Windows reserves HID GATT access; iPhone, AirPlay and full AI agent
automation remain unverified.

Both independent and plugin modes emit an upstream `adv start failed: 6` during
Windows pairing, followed by successful encryption and advertising recovery.
That baseline warning is retained. Free heap varies with network/BLE activity;
recorded differences also include SDK/configuration differences.

Evidence: `diagnostics/community/iphone-agent/`. An early harness timeout occurred
because `paused` was logged before the key-injection ACK; either ordering is now
accepted. Incomplete LCD captures are retried as whole frames, never composited.

Pure-independent retesting also recorded a Windows pairing failure and a scan
without a match; a device reset restored pairing, encrypted readiness, neutral
HID reports and pause/resume. The initial failure is not attributed to ZION or
to Windows without further evidence. These earlier attempts remain in the receipts.
The final host is restored to the ZION list with this app installed and no default.
Final placement at `0x270000` was separately loaded and passed the same main flow;
the earlier `0x360000` placement and final placement have the same installed extent.
The matching test bundle is `dist/community-link-validation/70ca4547ea38/`.

## Octo Radio: in-progress audio/network boundary

Candidate: [community play 1562](https://ai-passport.folotoy.cn/plays/1562/),
[upstream](https://github.com/lison-xulicong/octo-radio), revision
`70e071426d018bf56cf17e379acab93002330c5b`. All 19 original C/C++ translation
units, full octopus animation and embedded configuration pages are retained.
Only the external build/link profile and test instrumentation are added.

The independent build passed catalog selection, MP3 decoding, jitter buffering,
I2S initialization, animation, short keys and long-key settings navigation.
The plugin loads and initializes Wi-Fi and its UI; playback is **not yet a pass**:
the decoder's 8 KiB allocation exceeds the remaining largest free block.
Disabling the supported Wi-Fi IRAM optimizations and using six static RX buffers
raised manager free heap from about 47 to 55 KiB, but the hybrid-plugin decoder
still had only a 7,680-byte largest block despite about 15 KiB aggregate free RAM.
This remains an explicit failed audio acceptance case, not a package-size limit.
The Wi-Fi RAM settings trade throughput for memory; maximum download throughput
has not been benchmarked.

This candidate exposed foundation issues rather than business-source defects:

- Stream executable ELF sections through Flash one at a time, releasing their
  staging buffers. The measured peak staging block is 11,572 bytes, rather than
  retaining the complete 119,076-byte code image in RAM.
- Reduce 1,193 section-map entries from 52,492 to 14,316 bytes by reading raw
  headers from verified Flash on demand. No arbitrary package-size cap is added.
- Allocate only the metadata workspaces required by a commit (13,376 bytes),
  leaving room for LVGL instead of allocating all 23,624 bytes. NOR failure and
  recovery fixtures retain their durable-record ordering checks.
- Use buffered USB and bounded whole-record writes for installation ACKs. A
  failing transfer had received 515,328 bytes while the client had only seen the
  514,304-byte ACK. The first corrected transfer completed all 1,449,488 bytes.
- Support applications polling ADC buttons without a registered callback.

The experimental host factory is now 2.5 MiB (`0x10000..0x290000`). The identity
region remains `0x356000..0x35a000`. This is not a production 2 MiB updater path.
For large hybrid files whose trailer crosses identity, independent-mode tests
write verified native segments only; they do not raw-flash that trailer.

While the device owner is asleep, DeviceTest and standalone test components
force codec volume zero/mute and replace actual I2S output with zero PCM.
Decoding input and application buffers are unchanged. Host tests cover forced
mute, partial writes and error propagation. Normal releases do not link this
interceptor. No microphone/instrument measurement of acoustic output is claimed.
The independent and hybrid-independent device logs both contain
`CODEC_MUTED volume=0 mute=1` and `I2S_ZERO_OUTPUT bytes=1152`, alongside actual
22,050 Hz mono MP3 decode and jitter-buffer priming. Both independent routes pass
their automated key/settings/animation main flow under this silence profile.

Evidence: `diagnostics/community/octo-radio/`; external directory/MP3 peers are
deterministic local fixtures, while HTTP parsing, MP3 decode and I2S run on the
device. Real public station availability and subjective sound quality remain
unverified. Plugin-only retesting passed short/long keys, station selection,
pause/resume commands, menu navigation and animation; MP3 playback still failed.
An initial menu capture lost 8 of 320 rows when decoder-error logs interleaved;
the retained failed attempt was followed by whole-frame captures with retrying
playback paused. No missing rows were synthesized.

The exact local test bundle is `dist/community-link-validation/0cdcdf426178/`.
Host APP: 2,611,632 bytes; independent merged BIN: 2,804,528 bytes;
plugin: 1,449,488 bytes; hybrid BIN: 4,254,032 bytes.
All 16 allowlisted files were read back and hash-checked, and checked for the
private test Wi-Fi values. Both host and independent images are deliberately
silent test images, not normal release builds. The report explicitly retains
the failed plugin-audio result. The device is returned to the ZION list.

## Bathroom reader: HTTP configuration, HTTPS and persistent paging

[Community play 1427](https://ai-passport.folotoy.cn/plays/1427/),
[upstream](https://github.com/RadyWu/ai-passport/tree/feature/bathroom-short-read),
revision `f2142e3bbab18a1831e190b7a22c37986217ae36`. All 11 application/font C
units remain unchanged; the upstream checkout is Git clean. An external migration
header maps the IDF 6 GPIO deep-sleep wake API rename without editing business code.

The old copied DNS dependency assumes IPv6 types even for IPv4. The plugin instead
links to ZION's real AP DNS transport. Compile-time assertions compare the original
configuration ABI. The adapter supports one wildcard rule for `WIFI_AP_DEF` or its
explicit IP; other configurations return NULL. Host checks cover packet fuzzing,
scope validation and start/stop errors. Device logs confirm shared DNS start/stop
during APSTA-to-STA provisioning; phone-to-AP connectivity is not tested.

The host has 500 automatic imports plus manual services, including actual SDK
sleep/SNTP, NVS blob/i64, string and LVGL providers. Three SNTP slots match the app.
Missing BSP lifecycle APIs reuse the official BSP at
`3d685a90bdb96dd8526d84edde1e63fec25b460e`, preserving runtime display/button changes.
Official codec recovery and suspend/readback host tests exercise that implementation.
Actual deep sleep is not executed while the owner is unavailable: it can remove USB.

This cohort reserves `0x284000` factory bytes, ending at `0x294000`, only 16 KiB
above the earlier 2.5 MiB profile. APP, bootloader and table must agree; identity
is unchanged. Host tests cover the non-64-KiB factory end, app/data placement and
exact reclamation. Repeated replacement exposed a false-space rejection: the
1,703,936-byte gap `0x360000..0x500000` fits the actual 1,695,744-byte extent, while
the old worst-case padding requested 1,708,032. Carve starts already guarantee
64 KiB alignment, so the installer now reserves the exact extent and rechecks the
chosen offset. The old package stays live until payload verification and commit.
The same previously rejected gap subsequently accepted the complete update.

Independent and hybrid-independent routes pass HTTP configuration, persisted
article display, paging, brightness long presses and OK modals. Rebooted hybrid
body pixels exactly match the earlier saved independent article. The hybrid-plugin
route also passes original HTTP save, HTTPS/JSON/article persistence and keys.
Final-cohort pure-plugin repetition and archival are recorded separately below.

The external AI service is a local HTTPS fixture with a dummy key. Actual TLS 1.2,
certificate verification, receive callbacks, JSON, NVS and rendering execute on
the ESP32; the peer observes `ECDHE-RSA-AES256-GCM-SHA384`. Clock seeding and preflight
hostname substitution are fixtures, not actual SNTP or paid DeepSeek acceptance.
SDK/configuration differences mean free-heap figures are not a pure overhead delta.

An early harness used the PC fixture IP as the device URL; the tool now requires
the device IP explicitly. Failed LCD captures remain recorded, are retried only
as whole frames and never repaired with fabricated rows. Buffered whole-row USB
writes replace per-character output. These failures do not establish a UI defect.

Final reader cohort: `dist/community-link-validation/3cb3cb00b308/`. Host APP is
2,622,128 bytes; independent BIN 3,014,368; plugin 1,587,344; hybrid BIN 4,601,728.
All four execution routes pass their scoped main flows. The pure plugin also
reloads the saved article after reset. Article-body pixels match across native,
hybrid-native, hybrid-plugin, plugin-only and plugin cold boot. All 16 final
plugin frames contain 320 complete LCD rows, with no frame retries.

The same host subsequently ran the unchanged original printer plugin against a
local TLS/MQTT peer. A held RUNNING-to-FINISH transition exercised the reused
audio BSP: 16 kHz codec open, `CODEC_MUTED volume=0 mute=1`, then
`I2S_ZERO_OUTPUT bytes=512`. The earlier fixture overwrote FINISH with RUNNING
before the UI poll; that attempt is retained as a missing stimulus, not a driver
failure. The regression plugin/ELF and four reader screenshots are in the bundle.
All 23 allowlisted files were read back and hash-checked; private Wi-Fi values
and the actual fixture private key are absent. Public PEM parser delimiter
strings are not private keys. The device is returned to the manager list.

Build: PASS. Host tests: PASS (16 focused groups plus unsupported-import rejection).
Device tests: PASS within the reader and muted-audio scopes above. Complete gate:
FAIL on unchanged imported XiaoZhi bilingual-document checks. Unverified: actual
deep sleep, real SNTP/paid AI service, phone SoftAP/official-miniapp delivery,
physical switch contacts, acoustic quality and long-duration/max-throughput tests.

## DiDaaa: real-time UDP keys and MQTT signaling

[Community play 629](https://ai-passport.folotoy.cn/plays/629/),
[source](https://github.com/subooku/didaaa), revision
`502db4b9e254eabcbaeeabefb831cee9732e051f` (firmware 1.1.18). All seven original
C units, original arrow bitmap and CA remain unchanged; the upstream is Git clean.
The profile retains original signedness warnings and disables strict aliasing
for the original IP-address cast in `cw_net.c`, without editing business files.

This app needs raw PRESS/RELEASE and a fourth parallel-key level. The generated
entry extracts the original ADC table; the device logs four actual registrations.
Default three-key initialization, profile rollback and callback ownership have
host checks. The current host has 532 automatic imports plus manual services,
including pinned ESP WebSocket client 1.8.0 and original Montserrat 16. Host factory
is `0x289000` bytes, ending at `0x299000`; current APP is 2,654,016 bytes.

The independent build uses a sidecar compact dual-OTA table: original NVS/PHY
addresses and sizes stay fixed, OTA metadata occupies `0x10000..0x12000`, OTA 0
starts at `0x20000`, and OTA 1 stays at `0x400000`. This avoids megabytes of
trailing erased padding in the hybrid. The complete hybrid (1,957,304 bytes)
was actually written from `0x0`, preserving the identity region. Plugin SDK OTA
writes are explicitly unsupported; standalone still uses real SDK OTA APIs.

All four routes pass: standalone, hybrid-independent, hybrid-plugin and plugin-only.
A local peer performs actual UDP authentication and MQTT signaling, supplies a
roster/occupancy/time and sends a remote dot. Actual device timestamps measure
89–90 ms dots and 279–280 ms dashes. Frequency changes from 7,100,000 to 7,101,000 Hz
and back; the display decodes TX `E T` and RX `E`. Virtual combo toggles online/offline;
2.2-second key holds enter/leave the original 2-second settings menu. Actual codec
mute and zero-PCM I2S writes are observed. Physical contacts remain untested.

Early receipts retain two instrumentation problems: waiting for the desktop ACK
extended a requested press to 603 ms; timed device pulses remove that dependency.
Per-block codec mute writes also contended for I2C and an instrumented independent
run dropped key edges. The final profile forces mute at open/setters and zeros the
I2S sink without per-block I2C traffic. All final routes pass under this lower-load
profile; this does not establish behavior under every UI/network load.

The unchanged reader was rebuilt against this host and again passed HTTPS, article
storage, paging and keys after reset, exercising ordinary three-key operation.
19 focused host groups pass. The inherited complete gate still fails imported
XiaoZhi bilingual-document checks. Public service interoperability, WSS, standalone
OTA/rollback, physical contact/ADC noise, acoustic quality and long-duration timing
remain unverified. Evidence: `diagnostics/community/didaaa/` and
`diagnostics/community/reader-532-regression/`.

The matching final bundle is `dist/community-link-validation/eff4dc42ef7f/`:
host APP 2,654,016 bytes; independent DiDaaa BIN 1,747,296; plugin 209,992;
hybrid BIN 1,957,304. Actual installed plugin extent, including relocation cache
and alignment, is 323,584 bytes. It includes the same-host reader regression
plugin, corresponding ELF/MAP artifacts and five selected device screenshots.
The board is left at the ZION list with both external apps installed and no default.
