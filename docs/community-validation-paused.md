[简体中文](community-validation-paused.zh_CN.md) · **English**

# Community validation handoff — paused 2026-10-28

Paused at the user's request. Do not fetch another candidate or resume device
testing until requested. No commit, push or publication was performed.

## Validated checkpoint

- New device: COM7, ESP32-C3, 8MB. Silent DeviceTest host ELF
  `7fc7dd1f56116e0d53a73df8f4d5cf981988e45dff643850d1146923058e2fa0`.
- Passport OS / 438: unchanged `b70879b`; pure plugin and hybrid BIN extracted
  plugin passed actual startup, UP/DOWN/short/long OK, stopwatch, active Tetris
  and settings. Reviewed LCD images are in `diagnostics/community/passport-os`.
  Native core flow previously passed on the older device using identical APP
  bytes with an identity-preserving relocated partition layout. Stock merged
  native/hybrid images were not flashed over the protected identity range.
- Identity `0x356000..0x35a000`: new-device before/after readbacks identical.
- MEM-08 through MEM-11 are retained, including 32-entry metadata pages,
  early reusable staging reservation and packed mutable arenas. 438 uses
  16 arenas instead of 698 separate allocations. Startup free/largest:
  30,696/12,800 bytes; after pure-plugin core flow: 44,476/17,408.
- Bound artifact/debug cohort:
  `dist/community-link-validation/passportos-packed-arenas/7fc7dd1f5611`.
  DeviceTest artifacts are local validation builds, not a public release.
- Full gate still fails inherited XiaoZhi Markdown language/peer checks;
  not repaired per user scope. Focused host tests cover 30 groups.
- Clean streak remains **0/3**: this candidate required base changes.

## Next candidate, not accepted

PokeWalk / 234 was selected from the actual hearts-descending API. Repository:
`../candidates/pokewalk`, revision `dfc9098465b32a68f5218eb33fcd648ad1a7207a`.
Business sources remain clean. `examples/link-apps/pokewalk` is a build-only
preparation profile: 90 original translation units compiled into a 2,628,468-byte
ELF import plan; 22 unresolved providers remain. No runnable package, native
build or device PASS is claimed.

Important blockers: explicit RTC placement for the 7,792-byte legacy-save
scratch, multiple persistent partitions, and checks for original partition
addresses. Do not omit storage or silently redirect RTC state into DRAM to
force acceptance. Largest original mutable object: 39,976 bytes; total mutable
section sizes: 142,621 bytes. These are ELF measurements, not device headroom.

The incomplete RTC loader experiment was removed from active core files and
saved as `../diagnostics/pokewalk-paused/rtc-experiment-unverified.patch`.
It is **not validated**: focused tests failed on the existing Passport OS ELF.
Core loader/header/host files were restored byte-for-byte to the validated
checkpoint. Evidence: `restored-verified-source.json` in that folder.
Import plan: `build-link-pokewalk/import-plan.json`; full compilation log:
`../diagnostics/pokewalk-import-plan-build.log`.

## Resuming

Re-identify the device and port; do not assume COM7 remains current. Read the
runtime AGENTS and this checkpoint, keep the existing dirty work, then resolve
RTC and partition contracts on the host before flashing PokeWalk. Continue one
candidate at a time, by likes, until three new candidates pass without changing
the base. Retain silence and the protected identity range. Do not count mocked
network flows or compile success as physical application acceptance.
