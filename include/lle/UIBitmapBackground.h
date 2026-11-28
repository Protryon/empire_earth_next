// C:\EEX\Low-Level Engine\User Interface\UIBitmapBackground.h (RECONSTRUCTION). A background drawn as one
// stretched bitmap. Vtable 0x10098c88 (UIBackground's 7 slots, then DrawStrectchedBackground, spelling as
// exported); the original's sizeof was 0x7c (??_E stride). Requires lle/UIBackground.h.
// Whole in Rust (bgobj.rs, R6 ui round 20): the bitmap is the record's mBitmap.
#ifndef LLE_UIBITMAPBACKGROUND_H
#define LLE_UIBITMAPBACKGROUND_H

#include "lle/UIBackground.h"

// C:\EEX\Low-Level Engine\User Interface\UITextureBackground.h (RECONSTRUCTION; port: here, with its subclass). A background drawn from a texture
// file. Vtable 0x10098bb4; the original's sizeof was 0x78 (??_E stride). Requires lle/UIBackground.h and the
// GETextureDescription definition.
//
// Whole in Rust (bgobj.rs, R6 ui round 20): the texture's rectangle, description and a copy of the file name are the
// record's (EeUiBackgroundFields); the name stays a UString here for GetTextureFileName and the game's map
// background.
class LLE_API UITextureBackground : public UIBackground {
public:
    UITextureBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                        const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                        const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, bool inVisible)
        : UIBackground(inBackgroundID, inLayout, inForm, inDrawPriority, inVisible), mTextureFileName(inTextureFileName)
    {
        ee_ui_bg_texture(this, reinterpret_cast<const float*>(&inTextureRectangle),
                         reinterpret_cast<const EeUiTextureDescription*>(&inDescription), inTextureFileName);
    }
    virtual ~UITextureBackground() {}
    const UString& GetTextureFileName() const { return this->mTextureFileName; }
    // the game's map background (EEUIBMapBackground::LoadBackground) describes the textures it cuts
    void SetTextureDescription(const GETextureDescription& inDescription)
        { this->mTextureDescription = *reinterpret_cast<const EeUiTextureDescription*>(&inDescription); }
    void SetTextureRectangle(const GERectangle& inRectangle) { this->mTextureRectangle = *reinterpret_cast<const EeUiRect*>(&inRectangle); }
    UString mTextureFileName;
};


class LLE_API UIBitmapBackground : public UITextureBackground {
public:
    UIBitmapBackground(unsigned long inBackgroundID, const UString& inTextureFileName, const GETextureDescription& inDescription, const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, bool inVisible) : UITextureBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm, inDrawPriority, inVisible) { this->Call(EeUiBgCall::BitmapInit); }
    virtual ~UIBitmapBackground() { this->Call(EeUiBgCall::BitmapRelease); }
    virtual long DrawBackground(const GERectangle& inRectangle) { return (long)this->CallRect(EeUiBgCall::BitmapDraw, inRectangle); }
    GEBitmap* GetBitmap() { return static_cast<GEBitmap*>(this->mBitmap); }
    virtual long LoadBackground() { return (long)this->Call(EeUiBgCall::BitmapLoad); }
    void SetBitmapTexture(GETexture* inTexture) { this->Call(EeUiBgCall::BitmapSetTexture, (intptr_t)inTexture); }
    virtual long UnloadBackground() { return (long)this->Call(EeUiBgCall::BitmapUnload); }
};

#endif
