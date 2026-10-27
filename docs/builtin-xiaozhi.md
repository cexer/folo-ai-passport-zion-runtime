[简体中文](builtin-xiaozhi.zh_CN.md) · **English**

# ZION with built-in minimal XiaoZhi

Experimental branch `feature/builtin-xiaozhi`, based on the current local ZION 0.4.5 worktree and XiaoZhi commit `72da544d1b51678f5d88967adc299b3aec956946`. The original repositories are unchanged. This fork requires **ESP-IDF 6.0.1**, not the upstream ZION 5.5.3 environment.

## Measured result

The shared application is about **2.33 MiB**, and its bootable BIN is about **2.39 MiB**. Separate ZION 0.4.5 plus minimal XiaoZhi applications total about **3.44 MiB**: shared linking saves about **1.11 MiB** of APP bytes.

The factory partition is deliberately resized to **0x260000 / 2.375 MiB**, with cardid, store and OTA metadata addresses unchanged. Compared with ordinary ZION's 2 MiB factory plus the independently allocated minimal XiaoZhi, this leaves about **1.53 MiB more allocatable space**. Allocation alignment and contiguous gaps still matter; this is not a promise about a fixed number of arbitrary apps. The built-in app uses no external slot; up to eight external slot records remain available.

Exact sizes, hashes, partition tables and matching ELF identity are in `release/*.bin.json`; matching ELF/MAP/configuration files are under `dist/zion-with-xiaozhi/`. Only the trailing erased bytes of the verified 8 MiB merged image are omitted from the small bootable BIN. Initialized images, their checksums and partition MD5 are checked before packaging.

## Behavior

- The first list item is built-in XiaoZhi. It supports launch and default selection; its management page explicitly disables deletion.
- Entering XiaoZhi uses a persisted, consumed-once launch request and a software reset into the **same application image**. Returning with a long OK press resets to ZION. Hardware, display and network tasks from the two modes are never initialized together.
- Holding Up while powering on still bypasses the default app. Cold/fault boots do not consume a pending request as permission to enter XiaoZhi. Failed persistence never launches it.
- Default selection is exclusive: selecting built-in XiaoZhi clears the external default; cancelling it leaves no default. Selecting an external default clears the built-in preference.
- The built-in app reuses the network saved by ZION. It retains its original hotspot provisioning fallback when no valid network exists. Its interface has text only, 1bpp fonts, activation code text, battery and volume controls. Prerecorded prompts, external assets and XiaoZhi firmware self-update are disabled. ZION retains its official mini-program code for installation.

This is actual source-level linking, not two BIN files joined together. Wi-Fi, TLS/PSA crypto, RTOS, LVGL and device driver libraries are linked once. Board-specific initialization code remains separate per mode. ZION's 4bpp UI subsets are not linked in this experiment; its complete original monochrome font is retained for app names.

## Build

Activate ESP-IDF 6.0.1, then run from this checkout:

```powershell
.\tools\build-with-xiaozhi.ps1
```

The helper uses `sdkconfig.with-xiaozhi.defaults` and `partitions.with-xiaozhi.csv`, builds in `build-final`, merges, validates and archives the release. `tools/package_minimal.py` can repeat artifact checks. New MCU SDK requirements include explicit driver dependencies, the Flash structure header and checked PSA SHA-256 operations. The original installer still rejects failed package hashes.

Boot testing exposed a separate bootloader Kconfig range that silently restored the old factory size. Its range is fixed; the helper explicitly reconfigures a stale bootloader when necessary, and packaging rejects any APP/table/bootloader geometry mismatch. NVS data must be erased in release images.

**This is a different factory layout. Do not use the old ZION 2 MiB factory updater or install this manager as a child app.** A deliberate USB installation must consider any apps occupying the enlarged factory range and reset the OTA selection appropriately. No device Flash was written during this experiment.

## Validation

- Production build and verified package: **PASS**. Symbol inspection found neither 4bpp font objects nor embedded OGG prompts; the minimal XiaoZhi image decoder was also removed.
- Host tests of the actual built-in request implementation: **PASS** for consumed-once launch, cold/fault escape, persistence failure, default persistence and return.
- A test-only build writes one request using the real NVS helper, performs a real emulated software reset, enters the production entry and initializes built-in XiaoZhi. **PASS up to the unsupported Wi-Fi call**, with no early panic/layout rejection. The release helper forces this injection OFF; packaging rejects the injection marker before writing any release files.
- The same test build validates streaming PSA SHA-256 against the known `abc` digest, with input split across two updates. This exercises the new crypto API without requiring the unavailable network HLE.
- An IDF 5.5.3 harness directly reuses the unchanged ZION renderer with mock catalog/NVS/launch services: list, locked delete, default marker and launch dispatch pass. The same mock approach checks the minimal text renderer's standby, activation and volume pages. These results do not validate physical networking or voice.
- The real ZION C streaming parser/allocator admits the independent minimal XiaoZhi, and allocates the local play-138 sample alongside it. The integrated geometry can allocate the original local play-725 sample on its own, but adding it after the clock fails because a sufficient contiguous layout no longer exists. These are allocation tests, not Flash installations.
- PassportSim's current Wi-Fi HLE pins IDF 5.5.3 and rejects the IDF 6 production image. Complete production boot/network/voice testing remains blocked by emulator fidelity; no version string was altered to bypass the check.
- The inherited whole-repository static gate is **not passing**: imported upstream component documentation lacks this repository's bilingual peers; legacy checks also assume the original font/build profile. It has not been presented as a production release gate pass. The standalone upstream suite additionally has pre-existing removed-board fixture failures.
- **Device tests: NOT RUN.** The previously used target COM5 was absent. Remaining acceptance: cold/Up-key escape, actual built-in entry/long-OK return, shared Wi-Fi, activation, recording/TTS, glyph push, repeated mode switches, and real external-app installation/launch.

No Git commit, push or community publication was performed. Upstream licenses and notices remain with the copied component.
