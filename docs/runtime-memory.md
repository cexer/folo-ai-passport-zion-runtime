[简体中文](runtime-memory.zh_CN.md) · **English**

# Runtime memory measurements

## Shared-loader improvements (2026-10-27)

Original application sources remain unchanged. These changes apply generally,
without checking an application's name or identity:

- Relocate non-writable PROGBITS into a data-readable Flash cache. Writable
  data, BSS and initializer arrays remain in RAM. Other immutable data still
  borrows the verified input mapping. Older packages with insufficient cache
  capacity fall back to RAM; new packages reserve both cache ranges.
- Allocate persistent module state and mappings before temporary section
  metadata, reducing the chance of persistent allocations pinning holes.
- Allocate the explicit SDK frame workspace only on first frame use. Native
  LVGL apps do not pay its 2,716-byte static cost.
- Retain names/keys rather than duplicate complete package headers in the
  runtime catalog: another 912 bytes of static RAM saved.

For the matched Octo Radio build, relocation-dependent read-only data is 7,456
bytes; module RAM data falls from 29,732 to 22,276 bytes. The two static changes
save 3,628 bytes before allocator effects and added loader bookkeeping. Other
modules may save different amounts.

The final plugin passed MP3 decoding, station changes, pause/resume, animation
and settings on COM5. During a 25-second steady observation, the real silenced
I2S counter advanced 1,049,472 bytes without allocation failures. The unchanged
application first requests an 8 KiB jitter buffer and falls back to 6 KiB; that
expected startup allocation failure remains visible. It is separate from the
former MP3 allocation failure and intermediate Wi-Fi allocation failures.

Evidence: `diagnostics/community/octo-radio/memory-final-progress/`. The native
test uses ESP-IDF 5.5.3, whereas the host uses 6.0.1; cross-mode heap differences
are not a controlled measurement of framework overhead. Acoustic quality,
public radio services and long-duration operation remain unverified.

These ESP32-C3/8 MiB measurements supersede the older embedded-example report.
Only XiaoZhi is built into the experimental host. Gallery was installed from an
external ZPK1 package and its RV32IM ELF executed from RAM. The primary ZION
project still boots complete child firmware through reset; it does not retain
the launcher's heap inside that firmware.

## Gallery comparison

Both variants decoded the same 158,140-byte FGA1 animation, with a 240×320
RGB565 surface requiring 153,600 bytes. Measurements came from COM5, not a host
estimate. Original source: [PhoenixZHC/folotoy_gallery](https://github.com/PhoenixZHC/folotoy_gallery),
commit `00cbde9d2c9f7998f1690e44c7dcc8ed21cbb63f`.

| Stage | Free heap | Largest free block |
| --- | ---: | ---: |
| Independent gallery, animation playing with AP/HTTP/FAT | 24,400 B | 16,384 B |
| ZION external gallery, before ELF/surface allocation | 168,456 B | 65,536 B |
| ZION external gallery, animation playing | 9,732 B | 800 B |

The independent app reserves its 150 KiB frame statically. ZION allocates the
surface after launch. Comparing the second row directly with the first would
therefore be misleading. The 14,668-byte playing-state difference is not a pure
framework overhead measurement: SDK versions/configuration differ and the
independent application also has FAT, HTTP, audio-player and Wi-Fi facilities;
the current plugin ports only FGA parsing/scheduling and key interactions.

The independent measurement build adds heap logging and seeds the same media
through its existing media-storage API. Its test FAT partition was moved to
`0x360000..0x7F0000` to preserve identity and the installed plugin. It is not a
byte-identical official release BIN. Bootloader/table/APP were then restored
to ZION; no whole-chip erase or identity-region write was performed.

## Allocation and ownership

Entering an offline plugin deletes the old launcher screen, stops/releases BLE,
stops the installer/network worker, releases its queue and deinitializes Wi-Fi.
The 8 KiB USB installation task exits; a 2 KiB USB control task remains, reports
memory and explicitly rejects installation while an application is running.
The launcher still supports USB installation after the next power-on. ROM USB
flashing is independent of these tasks. Shared display/LVGL, input, battery,
storage services and the active plugin remain alive. XiaoZhi retains networking.

Before surface allocation, RAM is already used by static data, instruction RAM,
system/driver tasks, LVGL buffers and task stacks. In the measured host map,
RAM-resident code used 97,108 B, `.bss` 44,320 B and `.data` 20,985 B before the
later adaptive-surface arrays were added. These are historical measurements,
not the exact final layout. Most firmware text/rodata remains Flash mapped.

Fixed 15,360-, 7,680- and 3,840-byte image tiles each failed despite aggregate
free memory. The surface now adapts tile height to available blocks and uses a
single LVGL draw object. The successful real run used 35 tiles; a legacy module
log saying `tiles=5` was a stale literal, not an allocation measurement.
RGB565 and decoded pixels were not reduced in precision.

## Validation boundary

Build: PASS for the instrumented standalone and experimental DeviceTest builds.
Host tests: PASS for seven focused groups. Device tests: PASS for external ELF
load, FGA full/delta animation, serial-injected pause/resume, long OK without
reset, memory observations and continued USB control with installation rejection.
The current 800-byte largest-block margin is a serious remaining limitation.

Unverified: physical key feel, camera-observed panel output, audio fidelity,
network/voice concurrency, long-running fragmentation, production clean rebuild,
and the independent gallery's complete installation through primary ZION.
The inherited complete gate remains blocked by imported bilingual-document checks.
Do not describe these scoped passes as complete product acceptance.

Machine-readable evidence and matching firmware hashes are under
`diagnostics/gallery-memory-comparison/`; native execution logs are under
`diagnostics/device-external-test/`. Earlier `runtime-final-report.json` and
0.2.0 artifacts describe the superseded embedded examples only.
