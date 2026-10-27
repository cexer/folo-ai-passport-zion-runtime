#include "text_display.h"
#include "zion_sdk.h"
#include <cstdio>
extern const zion_api_t *xiaozhi_host_api;
void TextDisplay::SetupUI() {setup_ui_called_=true;xiaozhi_host_api->text_set(ZION_TEXT_TITLE,"内置小智");}
void TextDisplay::SetStatus(const char *s) {xiaozhi_host_api->text_set(ZION_TEXT_STATUS,s?s:"");}
void TextDisplay::ShowNotification(const char *s,int) {SetStatus(s);}
void TextDisplay::SetChatMessage(const char *,const char *s) {xiaozhi_host_api->text_set(ZION_TEXT_BODY,s?s:"");}
void TextDisplay::ClearChatMessages() {SetChatMessage(nullptr,"");}
void TextDisplay::ShowActivationCode(const char *code,const char *message) {
    char text[384];snprintf(text,sizeof(text),"xiaozhi.me\n%s\n%s",code?code:"",message?message:"");SetChatMessage(nullptr,text);
}
void TextDisplay::ClearActivationCode() {ClearChatMessages();}
bool TextDisplay::AddTextGlyphs(const std::vector<TextGlyph> &glyphs,uint8_t bpp) {
    std::vector<zion_glyph_t> data;
    for(const auto &g:glyphs) data.push_back({g.codepoint,g.adv_w,g.box_w,g.box_h,g.ofs_x,g.ofs_y,g.bitmap.data(),(uint32_t)g.bitmap.size()});
    return xiaozhi_host_api->glyph_add(data.data(),data.size(),bpp)==0;
}
void TextDisplay::ClearTextGlyphs() {xiaozhi_host_api->glyph_clear();}
