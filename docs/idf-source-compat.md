[简体中文](idf-source-compat.zh_CN.md) · **English**

# ESP-IDF source compatibility: experimental profile

The goal is to keep application business sources unchanged and select the
environment through compilation/linking. This is an additional path beside the
stable, explicit `zion_api_t` interface. It is not a converter for existing BINs.

The current profile supports C and C++17 translation units against ESP-IDF 6.0.1,
LVGL 9.5.0 and the matching host configuration. C++ constructors, local static
initialization and tested standard-library primitives are supported; exceptions
and RTTI are disabled. The import registry combines explicit ownership adapters
with generated exact SDK symbols. This is not arbitrary binary compatibility.

## Application changes

Keep normal `app_main()`, LVGL, FreeRTOS, NVS and supported BSP calls. Add a
`compat.json` build description alongside the existing project, for example:

```json
{
  "id": 2147483653,
  "key": "idf-compat",
  "name": "IDF Compat CPU",
  "sources": ["main/main.c", "../plugins/nescpu/upstream/nes6502.c"],
  "include_dirs": ["../plugins/nescpu", "../plugins/nescpu/upstream"],
  "cflags": ["-Wno-discarded-qualifiers"]
}
```

Paths are relative to that description. Allocate a unique stable high-bit ID
and storage key for each app; do not reuse the example identity. No ZION header,
API table, application-specific shim or conditional branch is needed inside the
fixture's business code. The build tool generates the entry wrapper separately.

An optional `link_wrap` list applies GNU link-time wrapping to extra profile
sources. This is not automatic compatibility or a shared-service requirement.
For example, the V-Pet profile retains an initialization context that the
upstream incorrectly places on `app_main`'s stack. Its candidate sources stay
unchanged, but the application-specific repair is explicitly recorded in the
build manifest. Report such repairs separately from generic SDK providers.

The compiler uses the host's actual SDK/BSP/LVGL headers and C-library specs.
It builds RV32IM/ILP32 `ET_REL` code, retaining supported system calls as imports
instead of linking complete library copies. The loader resolves those imports
to host implementations. BSP initialization calls reuse host-owned subsystems;
button registration attaches the app callback to the existing button dispatcher.
Callback work must remain non-blocking. Returning from `app_main` retires its
task; application-created workers remain alive until power off.

The compatibility registries are `main/zion_idf_exports.inc` and the generated
`main/zion_idf_auto_imports.inc`. Unsupported imports
fail packaging with explicit names. Whole-chip Flash erase and arbitrary symbol
lookup are not exposed. Exact SDK `esp_restart` is now imported for existing
application protocols, including the Agent's USB `reboot` command; ZION does not
reserve any button for restarting. These restrictions are a compatibility boundary, not a
security sandbox: native code and NVS namespaces remain shared and trusted.

## Build and package

From this checkout, activate the SDK and build the host first:

```powershell
. ../../../build-tools/Enter-ESP-IDF6.ps1
./tools/build-runtime.ps1 -DeviceTest -BuildDirectory build-layout-apps
python tools/build-idf-compat.py examples/idf-compat --host-build build-layout-apps --output build-idf-compat-device
```

For a dual BIN, also build the unchanged application through its ordinary IDF
project. Use absolute build/config paths with `idf.py -C`:

```powershell
$compatBuild = Join-Path (Get-Location) 'build-idf-compat-standalone'
New-Item -ItemType Directory -Force $compatBuild | Out-Null
idf.py -C examples/idf-compat -B $compatBuild -D "SDKCONFIG=$compatBuild/sdkconfig" build
idf.py -C examples/idf-compat -B $compatBuild merge-bin -o "$compatBuild/IDF-Compat-Standalone-full.bin"
python tools/build-idf-compat.py examples/idf-compat --host-build build-layout-apps --output build-idf-compat-device --standalone-full "$compatBuild/IDF-Compat-Standalone-full.bin"
```

Outputs: `app.elf`, `app.zion`, optional `app-dual.bin`, generated `entry.c` and
`manifest.json` containing source hashes, imported symbols and host identity.
The dual BIN appends a ZPK1 package and ZIONDUAL footer to the independently
runnable merged firmware. The current USB tool extracts the native package:

```powershell
python tools/zionctl.py build-idf-compat-device/app-dual.bin --port COM_PORT
```

The package is bound to its host build contract. Build production plugins against
the production host, not DeviceTest/SIM. A generated contract fingerprints SDK
configuration, C-library specs, relevant headers and the compatibility layer.
The wrapper imports that exact contract symbol. Stale local SDK inputs fail the
build; a mismatched host rejects loading before `app_main`. This first profile
is deliberately strict and is not a portable, cross-version dynamic ABI.

For production, use a fresh host build without `-DeviceTest` or `-Simulator`.
SIM explicitly disables radio/PCM. Official mini-program delivery and community
approval of the extended dual BIN have not been established.

## Evidence and remaining scope

The versioned production host is
`release/ZION-Runtime-0.4.0-runtime-afa71a9888b0.bin` (2,378,304 bytes),
with its matching debug bundle under `dist/zion-runtime/afa71a9888b0`.
The host does not embed this fixture. Matching example files are under
`release/idf-compat-0.4.0/`: plugin 76,540 bytes, dual BIN 776,028 bytes,
standalone BIN 699,472 bytes. Read that directory's `manifest.json` before
mixing packages with a different host profile. The non-test production pair
was built/verified; device automation used the corresponding DeviceTest pair.

`examples/idf-compat/main/main.c` is an ordinary IDF compatibility fixture. The
same file is used for standalone and plugin builds, without a source rewrite.
The external Nofrendo `nes6502.c` also remains byte-identical to the candidate
repository (SHA-256 `e8f3fe711aa8d5674df230d06a63a574d4f6bd2764f88c1b3a860cf289106ae0`).
This proves the mechanism with a real third-party CPU core; it is **not** a
completed port of the full original NES application's UI, PPU, APU or Bluetooth.

The aggregate [validation report](../diagnostics/idf-compat/report.json) binds
the package identities and per-profile results. Device and simulator receipts
are in `diagnostics/idf-compat/`. They cover the
6502 reference, real LVGL objects, FreeRTOS worker/queue after `app_main` returns,
buttons, NVS commit/persistence, dual-package extraction and bad-contract rejection.
Simulator run caveats and the continuous-redraw boot marker timeout are retained
in the receipts; they do not establish radio or acoustic behavior. Host tests
relocate the real compatibility ELF and reject wrong contracts without leaking
allocations. A real compiler test rejects the unsupported `esp_flash_erase_chip`
import and removes stale output.

A separate device panic during package update was decoded against matching ELF
`264cf4f6ebc1c287b765bab8c0b4df4237b66b737ea3cb88a6b22bf9a03ccfe2`:
the list read the default app from NVS while materialization replaced partition
cache pointers. Default identity is now loaded once, cached atomically, and
updated after a successful setting commit. Three subsequent package updates
completed without a reset. The full trace is retained in `install-device.log`;
the earlier pre-ready timeout is not independently attributed by that trace.

Keep one app per boot. Entering from the list does not reset; leaving or switching
apps requires power off/on, and Up-held power-on bypasses the default. No long-OK
restart or hot-unload behavior was added. Full untouched third-party application
compatibility and unsupported driver/resource lifecycles remain future work.
The later C/C++ acceptance below supersedes this earlier fixture's scope.

## Runtime resource admission

Plugin files have no configured byte quota, and the loader has no fixed code/data
allocation budget or section/symbol count quota. Installation is constrained by
actual available Flash; loading requests the required memory from the allocator.
If allocation fails, the device displays `insufficient memory` and releases
partial loader allocations. File-format, integer-overflow, ABI and relocation
checks remain mandatory. A valid large file can still fail to run because RAM,
contiguous executable memory, Flash mapping or a required import is unavailable.
This does not catch arbitrary allocation failures inside application code after entry.

## Current device acceptance

The unchanged upstream UNO C/C++ sources start on ESP32-C3, open the audio codec,
initialize Wi-Fi/ESP-NOW, create a room and process Up/Down/OK/long-press input.
A C/C++ probe separately verifies global constructors, local static guards,
strings, mutexes, C linkage and FreeRTOS tasks/queues. This checks main flows and
platform integration, not every UNO rule or a second physical radio peer.

An intentionally padded UNO test package (300,000-byte ELF, 1,100,256-byte total)
installed and completed the same main flow. A separate allocation fixture requested
more executable memory than the board has: the actual allocation failed, partial
allocations were released and the loader reported `elf:out-of-memory`.
These synthetic sizes are regression inputs, not the original UNO footprint.

## Optional input profile and standalone OTA semantics

For an application with a different ADC classification table, a profile may set
`button_config_header` to its original `bsp_pins.h`. The generated entry reads
`BSP_BTN_MV_TABLE` and asks the shared BSP to reconfigure before `app_main`.
Business C/C++ files remain unchanged. This supports the three board keys and
one distinguishable parallel-key voltage; pins and ADC ownership stay in the BSP.
Validation rejects overlapping/invalid ranges and initialization errors attempt
to restore the old profile. Restart restores the manager's default three ranges.
An arbitrary modified BSP is not automatically interchangeable solely by name.

Original standalone-image OTA calls are not module updates. Native-plugin imports
return no next OTA slot and `ESP_ERR_NOT_SUPPORTED` for begin/write/end/abort,
boot-selection and rollback-confirmation changes. Use ZION package management
for a plugin update. A separately built independent application still links to
the real ESP-IDF OTA implementation. Factory and OTA-only merged images are
accepted only when their partition table contains one unambiguous verified APP.

DeviceTest exposes timed raw key pulses for acceptance without a physical user.
These run on a device task and exclude USB acknowledgement time from the hold;
they are callback injections, not a physical-contact or ADC-noise measurement.
