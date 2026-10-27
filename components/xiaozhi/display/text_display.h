#pragma once
#include "display.h"
class TextDisplay final : public Display {
public:
    TextDisplay() {width_=240;height_=320;}
    void SetupUI() override;
    void SetStatus(const char *status) override;
    void ShowNotification(const char *text,int duration_ms=3000) override;
    void SetChatMessage(const char *role,const char *text) override;
    void ClearChatMessages() override;
    void SetEmotion(const char *) override {}
    void UpdateStatusBar(bool=false) override {}
    void ShowActivationCode(const char *code,const char *message) override;
    void ClearActivationCode() override;
    bool AddTextGlyphs(const std::vector<TextGlyph> &glyphs,uint8_t bpp) override;
    void ClearTextGlyphs() override;
    bool IsMonochrome() const override {return true;}
protected:
    bool Lock(int=0) override {return true;} // Host service takes the real LVGL lock.
    void Unlock() override {}
};
