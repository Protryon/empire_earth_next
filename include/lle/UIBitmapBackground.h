// C:\EEX\Low-Level Engine\User Interface\UIBitmapBackground.h (RECONSTRUCTION). A background drawn as one
// stretched bitmap. Vtable 0x10098c88 (UIBackground's 7 slots, then DrawStrectchedBackground, spelling as
// exported); the original's sizeof was 0x7c (??_E stride). Requires lle/UITextureBackground.h.
// Whole in Rust (bgobj.rs, R6 ui round 20): the bitmap is the record's mBitmap.
#ifndef LLE_UIBITMAPBACKGROUND_H
#define LLE_UIBITMAPBACKGROUND_H

#include "lle/UITextureBackground.h"

class GEBitmap;
class GETexture;

class LLE_API UIBitmapBackground : public UITextureBackground {
public:
    UIBitmapBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                       const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                       const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, bool inVisible);
    virtual ~UIBitmapBackground();
    virtual long DrawBackground(const GERectangle& inRectangle);
    virtual long DrawStrectchedBackground(const GERectangle& inSource, const GERectangle& inDestination);  // 0x1c
    GEBitmap* GetBitmap() { return static_cast<GEBitmap*>(this->mBitmap); }
    virtual long LoadBackground();
    void SetBitmapTexture(GETexture* inTexture);
    virtual long UnloadBackground();
};

#endif
