[简体中文](online-sync-051.zh_CN.md) · **English**

# Runtime 0.5.1: product synchronization

Source: [cexer/folo-ai-passport-zion-runtime](https://github.com/cexer/folo-ai-passport-zion-runtime),
branch `feature/shared-runtime`. Product commits for 0.4.5 and 0.4.6 are merged;
ESP-IDF 6.0.1, shared providers, external plugins and builtin XiaoZhi remain.

Installation progress is yellow, failures/reasons red, completion green. Short
OK launches the completed installation; long OK returns to the list. Native
and negative plugin slot IDs are both supported. The manager does not auto-launch,
including after a phone reboot request, so another install can follow.

All Zion fonts, including the complete Chinese fallback, are 4bpp. The full
fallback is losslessly compressed. Actual LVGL decoded all 6,873 glyphs to the
same A8 checksum as an independently generated uncompressed reference; maximum
single temporary allocation was 15 bytes. Full-font object bytes grew from
214,175 to 643,639, plus 32,812 bytes for UI subsets. The prior 2,930,128-byte
DeviceTest host and current release use different profiles; this comparison
does not establish device performance or application memory acceptance.

**New experimental layout:** factory at `0x10000`, size `0x342000`; pool_0 at
`0x352000`, size `0x4000`. Identity, store, pool_1 and otadata addresses remain
unchanged. APP: 3,387,632 bytes; factory headroom: 28,432 bytes. This is not a
lossless in-place update: old pool_0 content may overlap the enlarged factory.
Plan a fresh layout and reinstall; rebuild plugins for the exact host contract.
No device was flashed or migrated in this task.

Build/package verification, 30 focused host groups and actual LVGL font decode
pass. The full repository gate still fails inherited imported XiaoZhi Markdown
language/peer rules; these were not repaired. This synchronized firmware has
not been device-tested. Online 0.4.6 retains its original 2MiB factory layout.

The power button controls hardware supply and is not readable by firmware;
arbitrary standalone applications also run without Zion executing. A universal
power+UP soft restart cannot be supplied by this manager update. The existing
UP-held-at-power-on list entry remains.
