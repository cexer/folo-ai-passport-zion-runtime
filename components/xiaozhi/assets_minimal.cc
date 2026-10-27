#include <noto_font_bundle.h>
#include "assets.h"
#include "display.h"

// No partition lookup, image decoder, font download or asset-upgrade pipeline.
// Keep the protocol's real basic-font descriptor for on-demand subtitle glyphs.
Assets::Assets() { text_font_capability_ = {true, NOTO_FONT_BUNDLE_ID, "basic", 14, 1}; }
Assets::~Assets() = default;
bool Assets::Download(std::string, std::function<void(int, size_t)>) { return false; }
bool Assets::Apply(bool) { return false; }
bool Assets::GetAssetData(const std::string&, void*& ptr, size_t& size) {
    ptr = nullptr;
    size = 0;
    return false;
}
