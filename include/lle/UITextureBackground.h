// C:\EEX\Low-Level Engine\User Interface\UITextureBackground.h (RECONSTRUCTION). A background drawn from a texture
// file. Vtable 0x10098bb4; the original's sizeof was 0x78 (??_E stride). Requires lle/UIBackground.h and the
// GETextureDescription definition.
//
// Whole in Rust (bgobj.rs, R6 ui round 20): the texture's rectangle, description and a copy of the file name are the
// record's (EeUiBackgroundFields); the name stays a UString here for GetTextureFileName and the game's map
// background.
#ifndef LLE_UITEXTUREBACKGROUND_H
#define LLE_UITEXTUREBACKGROUND_H

#include "lle/UIBackground.h"

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
    void SetTextureRectangle(const GERectangle& inRectangle)
        { this->mTextureRectangle = *reinterpret_cast<const EeUiRect*>(&inRectangle); }
protected:
    UString mTextureFileName;
};

#endif
