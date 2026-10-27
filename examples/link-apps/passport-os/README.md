[简体中文](README.zh_CN.md) · **English**

# Passport OS original-source candidate

Upstream: https://github.com/manchunx7-bit/passport-os, revision `b70879b`.
This is the multi-application source repository, not the separate voice-only
repository. Compilation and device acceptance are recorded separately under
`diagnostics/community/passport-os/`; this profile alone is not a PASS.

The profile keeps the original main, Muse, Noise and Wi-Fi component translation
units, default Muse artwork and embedded web/city assets. The original memory
diagnostics and specialized capture/playback/feedback audio BSP are compiled
privately, using shared SDK driver imports; no second display/I2C owner is
compiled. Missing SDK fonts are private data, compiled
from the same host LVGL checkout. No business sources or assets are edited.

The four linker wraps come from the upstream main/Muse CMake files: Opus
caller-storage and TLS allocation ownership. They are original application build
semantics, not added repairs. They remain explicitly recorded in the manifest.
The runtime must dispatch SDK-internal calls into these wrappers too. A module's
own link wrapping cannot intercept calls inside already-linked shared libraries.
`muse_proxy_defaults.h` reproduces the upstream public-release configuration
with an empty host and zero port; no maintainer proxy is baked in.
The GCM include bridge uses the current IDF 6 TF-PSA declarations and layouts;
crypto headers participate in the exact-host ABI fingerprint.

Input is ADC polling with the original voltage table, not BSP event callbacks.
SPIFFS requests a private 512 KiB `storage` extent. The native layout's 2 MiB
reservation failed the runtime's contiguous Flash placement; that original
failure is archived. No assets were removed, but fewer uploads may fit.
The original full firmware puts identity at a different address; it must never
be raw-flashed over the runtime's protected `0x356000..0x35a000` identity region.
