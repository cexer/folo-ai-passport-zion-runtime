**English** | [简体中文](README.zh_CN.md)

# ZION Mini-program Multi-firmware Manager

**Shared-runtime / ELF experiment:** use ESP-IDF **6.0.1** and `tools/build-runtime.ps1`. Only XiaoZhi is built in; other examples are externally installed. Read [current packages, services and validation](docs/runtime-applications.md), including ADPCM/DSP and Nofrendo CPU workloads. Native entry does not reset; leaving requires power off/on, with no long-OK restart. The overview below describes the inherited independent-firmware baseline, not the current plugin build instructions or layout.

A maintained multi-firmware launcher for ESP32-C3 AI Passport, derived from
alexwwang/meta-pass and FoloToy's official mini-program installer protocol.
The default build is the full Zion product UI, not the earlier research probe.

- Four scrolling game cards, trailing default marker, battery progress and space statistics.
- Persistent default startup with a cancellation window; safe return after software/crash resets.
- Game management: run once, set/clear default, confirmed deletion of APP and owned resources.
- Settings: Wi-Fi status, password-protected temporary setup hotspot and a local configuration page.
- Official community mini-program installation over BLE; the device downloads, verifies and relocates firmware.
- Drawn retro graphics and a compact Chinese font; only the official mini-program code uses an image.
- Failed/cancelled installs reclaim APP and owned resources; uncertain commits retain proven valid installs.

Source: [cexer/folo-ai-passport-zion](https://github.com/cexer/folo-ai-passport-zion).
This project preserves the upstream MIT license and copyright notices.

Read the [user guide](docs/zion-user-guide.md), [architecture and compatibility](docs/zion.md),
and [validation](docs/development/engineering/build-and-test.md). Activate ESP-IDF
5.5.3, then run `tools/build-zion.ps1` on Windows. Packages, matching ELF/MAP and
manifests are generated under `dist/`. Flashing is a separate explicit action
through `tools/flash_zion.py`. On Linux/macOS, run `tools/build-firmware.sh`, then
`python3 tools/package_zion.py --build-dir build`. Run `./tools/validate.sh` before delivery.
Build output, downloaded dependencies, private device configuration and `dist/` are excluded from Git.

Target: ESP32-C3, 8 MiB Flash, no PSRAM. The product uses a 2 MiB factory layout.
Migration from the old 1.44 MiB factory requires a fresh install; compatible
segmented updates preserve games/settings. Identity at `0x356000` is protected.
Zion replaces 592; two separate launchers are not kept in the Flash pool.

Build, host tests and device acceptance are reported separately. Test-only key
injection must remain disabled in production. Package identity is defined in
`zion-product.json`: MNAM keeps the exact display name; the ESP descriptor uses
a compact name within its 31-byte limit. Legacy web-deployment workflows are
manual; publishing source does not deploy a web service.
