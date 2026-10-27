[简体中文](runtime-optimization-log.zh_CN.md) · **English**

# Runtime optimization and compatibility provenance

## Passport OS integration checkpoint (2026-10-27, in progress)

Selection now follows the official cumulative `sort=hearts` endpoint (verified
descending public `hearts` counts); downloads are the fallback only if hearts
sorting is unavailable. Snapshot/queue: `diagnostics/community/popularity/`.
Candidate 438 is the separate complete `manchunx7-bit/passport-os` repository at
`b70879b`, not its historical voice-only repository. Business sources/assets are
unchanged; `examples/link-apps/passport-os/` records the exact component set.

The original source compile exposed these general requirements:

- Component-local headers must precede host application headers: otherwise its
  `ui_pixel.h` resolves to ZION's unrelated UI. The compile/link adapter now
  preserves that precedence. ADC-polling apps declare that input mode rather
  than being rejected for an unused callback enum ordering.
- Real PM/tickless idle, TJPGD and memory filesystem providers are enabled in
  the SDK configuration. SPIFFS has a required package capability bit, rejected
  by older hosts; FAT remains the default for existing packages. Storage reuse
  checks the requested subtype, never silently treating SPIFFS as FAT.
- The original Muse TLS and Opus wrappers need SDK-internal interposition, not
  only module-local `ld --wrap`. Typed shared dispatchers stage validated ELF
  function definitions and activate after executable-cache commit. `__real`
  imports bypass dispatch. Failed attempts discard pending addresses before
  another load. Tests cover delayed activation, no-recursion delegation and
  rejection without exporting functions. No business repair wrapper was added.
- The GCM include bridge exposes the actual IDF 6 TF-PSA header/layout, with
  private declarations explicitly enabled; it never copies IDF 5 contexts.
  Crypto headers are included in the exact-host contract.
- Original customized audio sessions and memory diagnostics remain private
  component code over shared SDK drivers. Shared display master brightness/
  power and synchronous battery poll/log APIs reuse the existing hardware owner.

The compile-only ELF has 4,237 sections, 437,692 code bytes and 101,542 writable
bytes; its largest original object is the contiguous 49,152-byte audio workspace.
These are ELF measurements, not evidence of device admission. The 200 missing
imports link against genuine providers; the initial APP overflowed its factory
by `0x6890`. Moving BLE controller code to Flash subsequently required more
space: factory is now `0x2D2000`; pool 0 starts at `0x2E2000`, size `0x74000`.
Identity at `0x356000` and pool 1 are unchanged. This is an
authorized fresh test layout, not a lossless upgrade of an old live catalog.
The original 2 MiB SPIFFS reservation failed installation at storage allocation;
the test profile reserves 512 KiB, keeping every original asset but reducing
user-upload capacity. Original-capacity evidence is archived separately.

Native reference: the stock 3.95 MB APP overlaps the protected identity at its
original offset, so the identical APP was executed at `0x360000`, with the same
2 MiB SPIFFS capacity below identity. Actual drawer, stopwatch/laps/pause/reset,
Tetris start/movement/rotation and settings passed; screenshots reviewed. Identity
bytes were read back identical. This is a relocated native reference, not a
claim that the stock prefix was safely flashed at `0x10000`.

The original inline diagnostic channel first lost a TX ACK, then a complete
panic showed `xIsrStack` bounds `0x3fc8f7b0..0x3fc8fdb0` (1536 bytes), while the
reported interrupted task was `ser_cmd`. That task name alone does not prove
its own stack overflow. Control replies now use atomic bounded USB writes;
stdin and raw USB share a broker; control parsing/capture executes on a 6 KiB
test-only worker instead of the app's reader. The identical flow then passed.
FIFO/context, queue saturation, timeout and rollback have host coverage.

**MEM-08 / 438:** a bit per section replaces repeated borrowability scans, and
the loader cooperates with the scheduler. The previous pre-allocation scan
triggered IDLE watchdog reports; the later device path reached metadata in
about 426 ms without a watchdog. USB's attached-host no-light-sleep lock is
enabled when children configure automatic PM.

**MEM-09 / 438:** moving BLE to Flash let the 49,179-byte workspace allocation
succeed. The next measured failure was a 50,844-byte temporary mapping table:
74,592 bytes free, largest block 24,576. A flat-first mapping table now falls
back to 32-entry pages (384 bytes each on RV32), freed before entry. After
reserving staging memory, the largest remaining hole was only 2,688 bytes;
the earlier 256-entry page was still too large. Real-ELF relocations and partial
allocation rollback pass on the host. The previous device loaded all 4,237
sections through 133 pages and entered Passport OS. No application object is split.
The clean streak remains zero because this case changes the base.

**MEM-10 / 438:** an interleaved 4,804-byte staging request failed with a
4,608-byte largest hole. Reserve the maximum staged section (11,228 bytes)
before metadata allocation, reuse it across sections, and release it before
application entry. The previous device then reached Boot Complete and passed
the stopwatch flow; Radio still requested its own heap-defragmenting restart.

**MEM-11 / 438:** the fallback previously allocated 698 mutable sections
individually. Pack small objects into bounded arenas and allocate large objects
first; on failure split only between original objects. Real-ELF host tests and
allocation-failure rollback pass. On the newly connected C3/8MB device, the
allocation count fell to 16; startup free/largest were 30,696/12,800 bytes and
after the stopwatch, active Tetris and settings flows they were 44,476/17,408.
The previous device's pre-arena startup was 9,700/7,168; this comparison spans
devices, not a controlled same-device A/B. Actual LCD captures and UP/DOWN/OK/
long-OK flow passed. Evidence: `diagnostics/community/passport-os/report.json`;
matching cohort: `dist/community-link-validation/passportos-packed-arenas/7fc7dd1f5611`.
This is not an application-size limit.

Record the triggering application, actual failure, general remedy and evidence
for every base change. An application-specific repair is not a shared capability.
Measurements belong to the recorded build, not every future application.

## Memory changes

| ID | Trigger and observed problem | General change and evidence |
| --- | --- | --- |
| MEM-01 | Octo Radio: 1193 ELF sections; 52,492-byte temporary workspace exceeded a 45,056-byte block | Borrow verified section headers; compact mappings need 14,316 bytes. Stream executable sections to Flash, with a measured 11,572-byte peak stage instead of retaining 119,076 code bytes in RAM. `main/zion_elf.c`, `main/zion_elf_host.c`. |
| MEM-02 | Octo Radio: MP3 requested 8192 bytes with only a 7680-byte largest block | Relocate immutable PROGBITS to DROM cache; module RAM 29,732 to 22,276 bytes (7456 saved). Keep writable data and constructor arrays in RAM; legacy packages may fall back to RAM. Loader and package-builder tests cover cache sizing, relocation and failure cleanup. |
| MEM-03 | Octo Radio: Wi-Fi still failed a 1700-byte allocation after MEM-02 | Allocate persistent module state/mappings before temporary metadata to avoid pinning holes. Combined device evidence with MEM-04/05; no isolated causal saving is claimed. |
| MEM-04 | Octo Radio: resident RAM pressure despite unused explicit SDK frame surface | Allocate the 80-tile descriptor workspace on first use; 2716 static bytes saved on C3. Native LVGL device tests passed; explicit SDK frame rendering after this change remains unverified. |
| MEM-05 | Octo Radio: duplicate package headers occupied resident RAM | Catalog keeps owned key/name strings, saving 912 static bytes. Names remain valid throughout execution. |
| MEM-06 | Bathroom Reader: tight Flash pool falsely rejected a fitting package | Use actual installed extents instead of pessimistic padding: 1,695,744-byte package fits a 1,703,936-byte gap; former estimate was 1,708,032. This is Flash accounting, not RAM. |

MEM-01 through MEM-05: [measurements and limits](runtime-memory.md), evidence
`diagnostics/community/octo-radio/memory-final-progress/`, matched host cohort
`60cd2e31d2ab`. The final 25-second steady window advanced 1,049,472 silenced I2S
bytes with no allocation failure. The original startup jitter-buffer fallback
from 8 to 6 KiB remains; do not describe it as eliminated.

## Shared capabilities and explicit boundaries

| Trigger | Capability / boundary | Evidence |
| --- | --- | --- |
| Space Pinball | Export `cosf`, `expf`, `fabsf`, `sqrtf`; no allocator or memory-layout change | `diagnostics/community/pinball/report.json`; cohort `11d86f83df99`; native/plugin main flows and hybrid routes. No unrelated old-app regression was required. |
| V-Pet | BLE observer role, GAP scan/advertisement control and missing LVGL timer/label exports | `diagnostics/community/vpet/report.json`; cohort `26b2b3920f2d`; real Windows-to-C3 and C3-to-Windows RF plus battle start. Static RAM section layout unchanged from Pinball. |
| Snoopy Passport | Add `__unordsf2`, `bsp_audio_wake`, `esp_netif_sntp_deinit`, `esp_pm_configure`, `lv_color_black`, `lv_color_white`, `lv_image_cache_drop`, `mktime`, `settimeofday` | Import plan exposed nine missing providers. Host APP 2,658,272 bytes fits the unchanged 2,658,304-byte partition. Plugin LCD/main-flow evidence: `diagnostics/community/snoopy/`; offline/manual-clock scope, not external NTP validation. This base change resets the clean streak. |
| Connect Four | Add `bsp_lvgl_set_landscape` and 33 drawing/GATT/math/OS exports. Enable NimBLE Central and GATT Client: five real GATT providers otherwise fail host linking | Host `5f67f4cdc172`: actual human-first/AI-first/two-player moves, landscape LCD, keys and BLE discovery start/stop PASS. BLE stopped-state heap recovers to 160,188 bytes. No second physical C3 was tested. Bundle `dist/community-link-validation/connect-four-validated/5f67f4cdc172/`. Hardware rotation adds no full-frame buffer. Streak stays zero because this case changed the base. |
| Liquid Glass UI | Original 25 translation units and wallpaper expose 44 missing shared drawing, motion, screenshot, audio-read and sleep/log providers; auto-import count rises from 587 to 631 | All real SDK providers link. Physical-target DeviceTest host `fe2972af1225`, APP 2,677,856 bytes. A separate radio-free simulator host `2adbf958` executes the external 332,712-byte ELF, all eight scenes, focus/actions, teardown and reentry; screenshots reviewed. Physical device run remains pending and does not count toward the streak. |
| Bathroom Reader | Shared DNS bridge prevents running copied DNS code against a mismatched SDK configuration | `docs/community-link-validation.md`; DNS bridge tests and actual Reader network main flow. |
| DiDaaa | SDK OTA facade rejects destructive native OTA calls in a plugin explicitly; configurable ADC ranges support its fourth ladder key | `docs/community-link-validation.md`; native behavior preserved, plugin rejection and key main flow tested. |
| Bambu Monitor | ES8311 clock/sleep recovery in BSP; do not repeatedly force I2C mute on every audio write | `docs/community-link-validation.md`; muted real I2S and focused codec recovery host tests. Acoustic quality was not tested. |

V-Pet also has **application defects**: retaining `app_main` stack model storage
after return, and an undersized native 24 KiB LVGL pool. Its explicit link wrapper
and native test pool of 64 KiB are recorded separately in
`examples/link-apps/vpet/` and the application report. They are not evidence that
arbitrary application lifetime bugs are automatically repaired by ZION.
Qiaopi was excluded before build because its audio blob alone exceeds the
largest install pool; it is not a loader/RAM failure.
Far Away was likewise excluded: its unchanged sprite blob is 6,937,968 bytes,
larger than the 4,841,472-byte maximum pool. Evidence:
`diagnostics/community/faraway/preflight.json`. No resource pruning was applied.
Snoopy's optional power-management call returns `ESP_ERR_NOT_SUPPORTED` with the
host's existing PM-disabled configuration; its offline main flow handles this.
Do not claim its independent firmware's power-saving behavior is preserved.

## Test infrastructure and interpretation

- Connect Four's 320-pixel row needs 1280 hex characters. The SDK USB writer
  queues one complete ring item or returns zero; the existing 1024-byte TX ring
  cannot accept that row. `main/zion_test_capture.c` now emits bounded 256-byte
  pixel chunks, retaining the ring size. Strict reconstruction rejects missing,
  conflicting and overlapping chunks. A complete physical 320x240 capture passed
  with the `5f67f4cdc172` host, as did a raw RFC2217 simulator transfer. The CLI's
  serial *preview* truncates long lines and is unsuitable for image reconstruction.
- COM5 disappeared while examining Connect Four. Its source has a 60-second
  menu idle sleep, but the user also reported moving the USB connection and
  explicitly said the cause is uncertain. Neither cause is proven for the old
  disconnects. `tests/firmware/sleep_guard.c` changes the
  final deep-sleep call to reset **only in DeviceTest images**, for both native
  and runtime test builds. Production keeps real SDK sleep. The wrapper's actual
  C body passes a non-return/reset test, and the ELF exports the wrapper with no
  real `esp_deep_sleep_start` symbol. Hardware sleep current and wake acceptance
  are explicitly excluded. The guarded host is built but awaits physical wake
  before it can replace the currently sleeping device image.
- Liquid Glass's native simulator takes its custom GPIO0 recovery branch and
  finds no factory-installed recovery image at `0x700000`. That native simulator
  run is not a pass. Its unchanged application executes under the separate ZION
  simulator host; this does not prove native boot behavior on physical hardware.
- Build-only SDK 5-to-6 API renames and retained upstream compiler warnings are
  stored in profiles/manifests. They are distinct from application repairs.
- `tools/record-clean-streak.py` snapshots source/configuration, bootloader,
  partition and host identities. `tests/test_clean_streak.py` rejects simulation,
  unreviewed screens, source changes and application repair wrappers, and checks
  that a failure resets the counter. Source repositories remain unchanged.

## New unchanged-base streak

Started on 2026-10-27 at **0/3**; previous successes do not count retroactively.
Pull and test one complex community application at a time. Each qualifying case
must preserve business sources, use the same base identity and pass actual device
startup, principal workflow and keys. Record source/build hashes, imported
symbols, screenshots/logs and limitations. Any required base code, export or
configuration change resets the streak to zero. Application-specific wrappers
and preflight exclusions are recorded but do not qualify as clean passes.

Authoritative run state: `diagnostics/community/clean-streak/state.json`.

### 2026-10-27 continuation

- Liquid Glass physical acceptance now passes all eight scenes/actions and
  long-OK menu/reentry. Bounded adjacent USB ACK-fragment joining in
  `tools/device_protocol.py` fixes test transport, not application firmware.
- Word Notebook passed under `fe2972af1225` without new providers (then 1/3).
  All sources and its 850-word ADPCM blob remain unchanged. Build-only
  `RENAME_TO` reproduces IDF resource symbols. Its hybrid would be 8,632,312
  bytes, so no oversized file was emitted. Double-OK collection is unverified.
- Weiqi required five LVGL scroll/style exports and reset the count to 0/3.
  Host `0eb0c3a498c2`, APP 2,678,240 bytes: physical page scrolling, cursor,
  wrong-move feedback and UP/DOWN/OK/LONG pass. Double-click horizontal movement
  and winning/endgame branches are unverified. Evidence:
  `diagnostics/community/weiqi/plugin-only/mainflow/`; archived cohort:
  `dist/community-link-validation/weiqi-physical/0eb0c3a498c2/`.
- Weiqi's initial install failure was Flash fragmentation, not RAM: its aligned
  staging/cache extent needed 761,856 bytes versus 745,472 largest available.
  Deleting old test apps through management resolved it; identity was preserved.
- Tower preflight found raw SPI2/display ownership conflicts and a signed
  companion-image layout. No execution claimed; see
  `diagnostics/community/tower/preflight.json`.
- Liuyao's standby rings exposed disabled arc widgets and eleven missing real
  SDK providers (`__fixdfsi`, arc/layer/style, `strtok`). Enable LVGL arc and
  export providers, preserving original business sources. With only 544 bytes
  spare previously, the experimental factory grows 16 KiB to `0x292000`;
  only pool_0 shrinks. Identity and pool_1 remain fixed. Acceptance pending.

The DeviceTest sleep guard has been physically flashed and the above apps ran;
its idle-trigger branch and hardware sleep/wake remain unverified. Earlier
paragraphs describing physical runs as pending are historical preflight status.

Liuyao also exposed an **event ABI surface mismatch**: its BSP enum ends at
LONG and its default event branch maps unknown values to CLICK. Forwarding the
host's extra RELEASE made one DOWN toggle twice; actual LCD before/after was
identical. The builder now inspects the application's own BSP declaration,
asserts the common integer enum prefix, and configures a runtime event mask
before `app_main`. Legacy callbacks receive phases 0..3; release-aware callbacks
retain 0..4. No application-name exceptions or business edits. The real click
sequence is covered by `tests/test_button_events.c`; this changes the base and
resets the clean streak. Physical revalidation passed: six animated casts,
interpretation pages, date entry, dressing result, original 60-second standby
arc and wake, all four key actions and return to the app home. Host
`e7fb28e9b13d` APP is 2,684,912 bytes; plugin is 708,356 bytes. Actual I2S zero
output was observed. 25 focused host groups pass; the complete gate still fails
inherited XiaoZhi bilingual docs. Bundle:
`dist/community-link-validation/liuyao-physical/e7fb28e9b13d/`. Streak is 0/3.

Basti's unchanged 18-source virtual pet exposed `bsp_audio_acquire/release`
and `lv_event_get_target`. The former are real task-owned BSP sessions, not
no-op exports: a mutex covers format/playback, release drains the original
6x240-sample DMA bound, closes the codec through the tracked recovery path and
stops I2S. Subsequent set_format reopens it. Non-owner/unpaired release does
nothing. `tests/test_bsp_audio_recovery.c` exercises contention, repeated
release, stopped channels and reopen alongside existing recovery failures.
Provenance comments are next to the implementation. This resets the streak;
the application's own model, drawing, NVS and queued audio remain unchanged.
Physical Basti acceptance passed original one-minute hatch, five care actions,
sleep/wake, menu/reentry and saved kitten after MCU reset. Repeated codec opens
and actual zero I2S output are logged. Host `bde27c44e907`, APP 2,685,424 bytes,
plugin 95,948 bytes. Bundle:
`dist/community-link-validation/basti-physical/bde27c44e907/`.

Pocket Chess's original AI/rules/drawing/NVS program exposed eight providers:
`__ashldi3`, `aligned_alloc`, `heap_caps_aligned_alloc`, `esp_log_default_level`,
`esp_rom_printf`, `lv_color_mix`, and two `bsp_power_*` functions. The SDK
providers are linked directly. The BSP power implementation is retained from
the candidate with only SDK-6 wake API renames; it performs actual button-pad,
LVGL, battery, I2C and display sleep preparation. DeviceTest still intercepts
the final deep-sleep call; real sleep current/wake are outside this acceptance.
The native AI's optional TT allocation policy is preserved, including its
own memory-dependent fallback, rather than changing the application to fit.
This is a base change; clean streak resets. Physical acceptance pending.

### MEM-07: writable ELF sections need not be one contiguous allocation

Chess then failed on hardware at `elf:out-of-memory`: 68,439 raw writable
section bytes (largest original object 11,408 bytes) were combined into one
allocation, exceeding the available largest heap block. Its real ELF also
reproduced rejection under the host test's 45,056-byte allocation bound.
`main/zion_elf.c` now tries the original contiguous allocation first, then
allocates each writable ELF section separately with 16-byte alignment. Each
object stays contiguous and ordinary symbol relocations use the per-section
addresses already maintained by the loader. No data structure/source changes,
fixed app limit, or whole-package RAM copy. One oversized individual section
still fails honestly. All partial allocations are retained in a cleanup list.
The real ELF host test passes 65 data blocks / 69,032 logical bytes on the
non-RO-cache path and exhaustively injects failure at every allocation to prove
rollback. Actual hardware acceptance remains to be recorded below. Earlier
normal-layout loader tests still run; this memory change warrants those tests.
Physical Chess now loads: 59 writable blocks / 68,504 bytes, free heap 87,924,
largest 18,432. Two local moves, AI playing White, an actual off-book search
(`outcome=0 callbacks=71 stack_free=5276`), reply and Chinese home render pass.
The original optional 49,152-byte TT is skipped by its own headroom policy.
An existing app policy suppresses the first CLICK after LONG even when there
was no post-LONG CLICK; follow-up navigation and its source are recorded, no
app-specific fix applied. Do not claim completely identical input behavior.
Host `34734f807c96`; bundle:
`dist/community-link-validation/chess-physical/34734f807c96/`. Streak stays 0/3.

Bagua Compass's unchanged nine-source image/timer/Chinese-scroll app passed on
`34734f807c96` with no providers added, temporarily 1/3. Its one unused-function
warning is retained, not patched in source. Bundle:
`dist/community-link-validation/bagua-clean/34734f807c96/`.
The next Pinyin candidate breaks that consecutive run: it unconditionally starts
a debug console whose single static snapshot array is 153,600 bytes. Current
host also disables LVGL snapshot. No package/device pass claimed, no removal of
the console to manufacture success; `diagnostics/community/pinyin/preflight.json`
distinguishes source evidence from measured heap in other apps. This is the same
single-object boundary as the gallery, not solved by splitting unrelated sections.

Countdown Reminder exposed `lv_obj_set_style_outline_width` (real LVGL export).
Its original 48px Montserrat font is compiled as private plugin data from the
same LVGL SDK, matching its native config, instead of expanding host fonts or
changing the UI. All four business translation units stay unchanged. This is
a new base export and resets the streak; five-minute physical expiry pending.
Countdown four physical routes passed: plugin, native, hybrid-native and
hybrid-plugin. Only pure plugin repeated original five-minute expiry/TIME UP/
actual zero-I2S alarm; other routes tested presets/start/pause/resume/cancel.
Native prefix 854,272 bytes, plugin 123,592, hybrid 977,880. Bundle:
`dist/community-link-validation/timer-four-routes/278737aee1a0/`.

Xiao Liu Ren exposed 22 providers plus DPP configuration and an event-value ABI
difference: its RELEASE is 5 because HOLD occupies 4. The generated entry now
passes its actual compiled RELEASE constant to the runtime mapping; common
PRESS/CLICK/DOUBLE/LONG values are compile-asserted. HOLD delivery itself is
not added (this app does not consume it). SDK5's deprecated NULL DPP callback
argument is adapted at build time to real SDK6 WIFI_EVENT_DPP_* behavior;
non-NULL callbacks return NOT_SUPPORTED explicitly. The header is hashed in
the manifest and package/streak verifiers. Source remains unchanged.

Linking LVGL QR also exposed two global Nayuki implementations (LVGL and
esp_emote_gfx). Their common 13 APIs and enum/segment layouts match (the one
int/enum-mask parameter has the same scalar ABI). Configure now verifies this
and selects LVGL's superset provider, excluding only the duplicate source via
target SOURCES; managed dependencies are not edited. Forty real-source cases
have identical fixed-mask matrices; two auto-mask choices differ. All eighty
old/new QR images decode back to exact payload bytes with an independent ZXing
decoder. Receipt: `diagnostics/qrcode-shared-provider.json`. No permissive
multiple-definition flags. Real DPP support and other providers grow APP to
2,730,928 bytes; factory 0x2A2000, only pool_0 shrinks, identity/pool_1 fixed.
26 focused host groups pass. Physical Xiao Liu Ren acceptance is pending.
