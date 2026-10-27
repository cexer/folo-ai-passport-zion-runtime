**English** · [简体中文](zion-user-guide.zh_CN.md)

# Zion user guide

Zion's own labels use a small 4-bpp antialiased font; battery text uses a matching
small font. The row number/size/version line uses the same compact 10px/4-bpp font.
Installed game names outside the UI subset fall back
to the losslessly compressed 4-bpp Chinese font, retaining all 6,873 characters.

Installation progress is yellow; failures and their reasons are red. Completion
is green and stays visible: short OK starts the just-installed game, long OK
returns to the list. No automatic launch occurs, including after a phone reboot
request, so another game can be installed through the same connection.

The power button controls hardware supply and is not readable by firmware.
Power-on while holding UP still enters the list; a universal power+UP soft
restart while arbitrary standalone firmware is running is not supported.

## Everyday controls

The list contains installed games, Install and Settings. Up/Down moves the
selection and scrolls through up to eight records; storage limits the actual
number of games. Short OK starts a game once. Long OK opens management. A star
**after** the title marks the default. The battery has a progress background;
unknown battery readings show a question mark.
The list keeps its short-OK launch hint on the left and long-OK management hint
on the right. Status messages do not replace these list instructions.

Card metadata shows the device slot number, APP slot capacity and firmware
version. It is not the community play ID or published download size. Owned
resources also consume the free-space total. Some official packages contain no
title; their firmware project name is shown. Free and largest-contiguous space
are different because alignment and occupied resources can fragment the pools.

## Default startup and returning to Zion

In management, select Set default or Clear default. The selection is saved on
the device and bound to the actual image, not just its list position. On a cold
start the valid default starts after an approximately 1.8-second cancellation
window following initialization. Hold Up while switching on until the list is
visible, or press any key during that window, to stay in Zion.

Software returns and crash/watchdog recovery do not immediately relaunch the
default. Arbitrary third-party firmware cannot universally return on long OK;
use its own return command if supplied, or restart while holding Up. A child
built with `zion_hook.h` can return and skip default startup once.
Switching starts another firmware through reboot, not simultaneous execution.

Starting a game also ends Zion's Bluetooth connection and installer service.
The Mini Program may show disconnected and cannot reconnect to the Zion service
while that game runs. Return to Zion by holding Up while switching on/restarting,
wait for the launcher list, then reconnect in the Mini Program. The game may
provide a different Bluetooth service of its own. Removing the explicit
disconnect does not preserve a Bluetooth connection across the reboot.

## Deleting a game

Long OK on the game, select Delete, then choose Confirm delete. The confirmation
defaults to Back. Short OK confirms the highlighted choice; long OK returns.
Deletion clears a matching default, durably removes that APP and its owned DATA,
then erases those extents and returns the space. Other games are retained.
Shared NVS is not per-game storage and is not indiscriminately erased.

An incomplete install left by interrupted/failed cleanup can be deleted from the
list; it cannot be started or made default. If metadata removal succeeds but erasing fails, the UI reports pending
cleanup rather than claiming the game is still installed or rolling back to
possibly erased bytes. A later allocation erases its destination before reuse.

## Wi-Fi setup

Select Settings, then Wi-Fi status/setup. The device opens a temporary
open hotspot (no password). Connect the phone to the displayed name,
keep the connection when the phone reports no Internet. A captive portal asks
the phone to open the setup page automatically; if it does not, open
`http://192.168.4.1` manually. Auto-opening depends on the phone's system settings.

Nearby 2.4 GHz Wi-Fi networks are scanned automatically. Select a network;
use Rescan to refresh the list. Enter the router password for an encrypted
network; open networks need no password. The web page first acknowledges the
settings, then triggers connection and polls the real result. Only a successful
connection is saved. On success the page attempts to open the official WeChat
Mini Program, with an Open button and an offline Mini Program code as fallbacks.
The hotspot closes about 10 seconds after the page receives success (90 seconds
if the page never acknowledges it); let the phone return to its normal network.
Failed setup keeps the hotspot available for retry. Mini Program launch depends
on the phone/browser/WeChat and has not been accepted on all phones.
Change Wi-Fi clears Zion's saved network
and starts a new setup session. Long OK returns to the list. Official
mini-program BLE provisioning is also supported. Child firmware may have its
own independent setup procedure.

## Installing from the official community

The list uses `+ Install` and `> Settings` prefixes to separate launcher actions
from game titles. Selecting Install shows the official WeChat Mini Program code
and asks the user to scan with WeChat. It is an issued Mini Program code image,
not a standard QR containing the community website URL. Returning
from a failed installation and selecting Install again opens the QR for a retry;
the failure remains visible until leaving its result page.

After opening the official Mini Program, choose a play,
select mini-program installation and the paired device. The phone sends the
request over BLE; Zion downloads directly over its configured Wi-Fi. The phone
does not upload the whole firmware file through this BLE connection.

Zion probes only the prefix through the partition table and first valid APP header
(about 64 KiB for play 590), then closes that connection and reserves storage.
It transfers the complete BIN once while parsing, hashing and writing APP/resources.
This works even when the official server ignores HTTP Range. A sparse layout
whose APP header is later in the source needs a larger prefix. Success is reported before it
starts the installed game. Short OK returns when idle; long OK during an install
opens a cancel confirmation. Temporary read timeouts have bounded retries.

An installation failure does not clear saved Wi-Fi credentials. A new
mini-program handshake reports the actual network readiness instead of the
previous installation error; the device keeps that error on its result page.
The phone mini-program may still require reconnecting its own session.

Installation starts automatically; there is no extra approval prompt. The device
shows the community title when the official download URL can be matched to public
metadata with the same size and SHA-256. Otherwise it explicitly labels the
fallback project name as such. It also shows published download size, reserved
APP-plus-resource capacity, and overall progress through check/write/verify.
Reserved capacity includes slot alignment and metadata, not just initialized
resource bytes. Until parsing finishes, capacity reads "Parsing"; missing names
are explicitly unknown. The phone request does not contain a community title;
the optional metadata query is separate from the one complete BIN transfer.
Progress includes layout/reservation, transfer and final verification; the speed
field uses actual received bytes and elapsed time, including stalls.

The 24-byte executable header remains erased until the entire BIN's expected
SHA-256 matches. Image verification, readback and the durable VALID record follow.
After a failed transfer, verification failure or confirmed cancellation, Zion
automatically erases this installation's APP and owned DATA and removes its space
reservation. The original failure remains visible with a space-reclaimed notice.
If cleanup fails or is interrupted, an incomplete reservation can remain; use
management to delete it or retry installation. An uncertain final commit is first
reloaded from both A/B sectors. A proven valid installation is retained and its
runtime table repaired; unreadable records never authorize destructive cleanup.
Reinstalling the same URL replaces its prior
APP/resources; failures do not preserve that old version. Other games and system
partitions are outside the allocation. Wi-Fi modem sleep is temporarily disabled
during installation and restored afterward. The current official download route
does not support Range resume, so a broken transfer is not silently appended to a
fresh `200` response.

Unsupported/unallocatable packages show "Cannot install"; failures after storage
preparation show "Installation failed". Both retain known information, the last
progress, and a Chinese layer-specific reason. Unknown capacity is not shown as
zero. A failure stays visible until the user returns or a new installation starts;
the community QR does not cover the result. Long OK still opens cancellation.

A complete merged BIN is parsed: source `0x10000`/`0x20000` addresses are not
blindly used as destination addresses. Supported resources are relocated and
exposed only to their owning child. A large published package can therefore have
a different installed size. The inherited legacy USB web allocator rejects this
2 MiB layout; do not use it as the product installation path.

## Building and updating the launcher

The primary directory is `folo-ai-passport-zion-main`. Activate ESP-IDF 5.5.3 and
use the native Windows build:

```powershell
./tools/build-zion.ps1
```

Packages appear under `dist/<version>/<ELF-prefix>/`. The manifest records actual
hashes, target, layout and test-control status, with matching ELF/MAP. `zion-full.bin`
is a bootable merged image for `0x0`; `app.bin` is not an image for `0x0`.
A separate explicit flash command preserves compatible games/settings:

```powershell
python tools/flash_zion.py --port COM5 --package dist/<version>/<ELF-prefix>
```

Replace the port and package directory with the actual values. An incompatible
old 1.44 MiB layout is rejected. `--fresh` explicitly clears games/settings for a
new layout and preserves identity; it is not an ordinary update option. No
whole-chip erase is needed. New configuration defaults need a fresh build
configuration or deliberate regeneration; a stale cached configuration is not
a verified product build. Test-only key injection is never enabled in the
normal package.

## Acceptance boundaries

Build, host tests, replayed real-device installation and official phone UI
acceptance are separate results. Synthetic key events traverse the actual queue,
menu and LVGL widgets but do not test the physical ADC buttons. A configuration
HTTP test through STA does not test a phone associating with the setup AP.
Physical display/font/QR scanning, the official phone click path and individual
child cloud features need their own acceptance. Exact current results and
remaining checks are recorded in the delivery verification report.

Writing the merged image directly at `0x0` can reset settings inside its byte range. Use the segmented flash tool when preserving existing settings.
