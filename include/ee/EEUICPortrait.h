// C:\EEX\Empire Earth\User Interface Thread\EEUICPortrait.h (RECONSTRUCTION; file name inferred). The main game
// form's selected-unit portrait: the unit's button cell, a level text and three bars (green and yellow progress, red
// background) (RTTI EEUICPortrait > UIControl; vtable 0x449fac, 34 slots). EEUICPortrait.cpp is
// 0x25861c..0x25952f (ctor, ??_G, dtor, then the members in alphabetical order, with EEPlayer's two inline lookups
// emitted after their first users). The `return 0` slots are ICF-folded (0xfca98, 0x1ad67e). Names are provisional.
// Requires lle/UIControl.h.
#ifndef EE_EEUICPORTRAIT_H
#define EE_EEUICPORTRAIT_H

#include "ee/EEUnitHandle.h"
#include "ee/EEUIRustControl.h"
#include "rust/game/records.h"

class EEUnit;

class EEUICPortrait : public EEUIRustControl, public EeScreensPortraitFields {
public:
    // 0x25861c (the user-defined control factory 0x274965; the scenario editor 0x1a4f39)
    EEUICPortrait(UIForm* inForm, const GERectangle& inRectangle, unsigned long inId, unsigned long inIndex,
                  bool inShowLevel)
        : EEUIRustControl(inId, inRectangle, inForm, inIndex, 0), EeScreensPortraitFields()
    {
        ee_screens_portrait_init(this, inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(),
                                 inRectangle.GetBottom(), inShowLevel);
    }
    // Destroys the portrait, releasing its unit and button
    virtual ~EEUICPortrait() { ee_screens_portrait_free(this); }
    // The bars' rectangles, the drawing, the tooltip, the click and the update from the unit (the original's
    // GetBarRectangles, DrawPolygons, GetHelpString, MouseLButtonDown and Update): ee_screens portrait.rs over the host
    // below
    virtual long DrawPolygons() { return ee_screens_portrait_draw(this); }            // +0x18 0x258987
    // Draws the improvement level when it is above 0
    virtual long DrawTextA() { return ee_screens_portrait_draw_text(this); }               // +0x1c 0x258ad0
    virtual bool GetHelpString(UWideString& outString) const   // +0x20 0x258af7
    {
        return ee_screens_portrait_help(const_cast<EEUICPortrait*>(this), &outString);
    }
    // 0x258f82: shows inUnit (EEUIFMainGame 0x26bfe2, 0x26c38b)
    // Shows a unit's portrait (button face), taking references, and updates the bars (ee_screens portrait.rs)
    void SetUnit(EEUnit* inUnit) { ee_screens_portrait_set_unit(this, inUnit); }
    // 0x259028: updates the bars and the level text from the unit (EEUIFMainGame 0x26d0a0)
    void Update() { ee_screens_portrait_update(this); }
    // (the portrait's own ops are portrait_calls.rs': EEUIRustControl's RustOp; its shims read the members below)
    virtual long MouseLButtonDown() { return ee_screens_portrait_mouse_down(this); }        // +0x34 0x258d02
    // Updates the help text while the mouse moves over a portrait showing both bars
    virtual long MouseMove(const GE2DPoint& inLocation) { return ee_screens_portrait_mouse_move(this); }    // +0x40 0x258f5d
    virtual long MouseRButtonDown() { return this->MouseLButtonDown(); }  // +0x44 0x258f7d

    // (+0x0a8 mFirstBar, +0x0ac mBackground, +0x0b0 mSecondBar: the bars are Rust's, ee_screens hud_draw.rs)
    EEUnitHandle<EEUnit> mUnit;             // +0x0b4 (referenced; a handle since R5a.4: read on the UI thread and in CopyData)
};

#endif
