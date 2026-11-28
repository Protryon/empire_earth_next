// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGAttribute.h (RECONSTRUCTION; file name inferred). The main
// game form's attribute (improvement) button: an EEUIBUpgrade that also draws its level as a
// number (RTTI EEUIFMGAttribute > EEUIBUpgrade; vtable 0x44aa78; sizeof 0x138). EEUIFMGAttribute.cpp is
// 0x26783d..0x267ad9 (ctor, ??_G, dtor, CopyData, DrawTextA, GetIconRectangle). The inline overrides are ICF-folded
// elsewhere (the tab slots 0x1720c6, UpdateKeyboard 0x1ad67e). Names are provisional. Requires ee/EEUIBUpgrade.h.
#ifndef EE_EEUIFMGATTRIBUTE_H
#define EE_EEUIFMGATTRIBUTE_H
#include "ee/EEUIBUpgrade.h"
#include "rust/game/records.h"


class EEUIFMGAttribute : public EEUIBUpgrade, public EeScreensAttributeFields {
public:
    // 0x26783d (callers 0x1a526d, 0x274868)
    // Creates the main game form's attribute upgrade button with a level text in its corner (hud_more.rs)
    EEUIFMGAttribute(GEFont* inFont, UIForm* inForm, const GERectangle& inRectangle, unsigned long inIndex,
                     unsigned long inId, const GEColor& inTextColor, bool inScenarioEditor,
                     unsigned long inBorderThickness)
        : EEUIBUpgrade(inId, inRectangle.GetTopLeft(), (unsigned long)inRectangle.GetWidth(),
                       (unsigned long)inRectangle.GetHeight(), inForm, inIndex, inScenarioEditor, inBorderThickness),
          EeScreensAttributeFields()
    {
        ee_screens_att_construct((uintptr_t)this, (uintptr_t)inFont, (uintptr_t)&inTextColor);
    }
    virtual ~EEUIFMGAttribute();
    // Snapshots the button; shows the number of queued upgrades as its level text (ee_screens hud_more.rs)
    virtual long CopyData() { return (long)ee_screens_att_copy_data((uintptr_t)this); }
    // Draws the button and, when shown, the level text in its corner (ee_screens hud_more.rs)
    virtual long DrawTextA() { return (long)ee_screens_att_draw_text((uintptr_t)this); }
    // 0x267a6b: the rectangle the status box stretches the button's bitmaps to (EEUIFMGStatusBox 0x26ae16)
    GERectangle GetIconRectangle() const;
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
    virtual long UpdateKeyboard() { return 0; }
};

#endif
