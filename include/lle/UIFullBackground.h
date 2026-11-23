// C:\EEX\Low-Level Engine\User Interface\UIFullBackground.h (RECONSTRUCTION). A full-screen background cut into a
// grid of bitmaps, one texture file per cell (<name>_<row>_<column><ext>), scaled from a designed size to the screen.
// A root class (RTTI: no base). Vtable 0x100999e8: ??_E, LoadBackground, DrawBackground, UnloadBackground; the
// original's sizeof was 0x2c (??_E stride).
// Whole in Rust (bgobj.rs, R6 ui round 20): its members are the backgrounds' record (the file name, form, grid and
// designed size); UIBackground.cpp forwards.
#ifndef LLE_UIFULLBACKGROUND_H
#define LLE_UIFULLBACKGROUND_H

#include "rust/ui/bgobj.h"
#include "rust/ui/lle_hosts.h"
#include "lle/UIObjHost.h"


class LLE_API UIFullBackground : public EeUiBackgroundFields {
public:
    UIFullBackground(const UString& inFileName, unsigned long inColumns, unsigned long inRows, UIForm* inForm, unsigned long inDesignedWidth, unsigned long inDesignedHeight) { ee_ui_bg_full_init(this, static_cast<EeUiBackgroundFields*>(this), ee_ui_background_full_host, inFileName, inColumns, inRows, inForm, inDesignedWidth, inDesignedHeight); }
    UIFullBackground(const UIFullBackground&) = delete;
    UIFullBackground& operator=(const UIFullBackground&) = delete;
    virtual ~UIFullBackground() { ee_ui_bg_full_free(this); }
    virtual long LoadBackground(const GETextureDescription& inDescription) { return (long)ee_ui_bg_call(this, EeUiBgCall::FullLoad, (intptr_t)&inDescription, 0, NULL); }
    virtual long DrawBackground(const GERectangle& inRectangle) { return (long)ee_ui_bg_call(this, EeUiBgCall::FullDraw, 0, 0, UIF4(inRectangle)); }
    virtual long UnloadBackground() { return (long)ee_ui_bg_call(this, EeUiBgCall::FullUnload, 0, 0, NULL); }
};

#endif
