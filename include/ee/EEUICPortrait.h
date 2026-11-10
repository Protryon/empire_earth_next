// C:\EEX\Empire Earth\User Interface Thread\EEUICPortrait.h (RECONSTRUCTION; file name inferred). The main game
// form's selected-unit portrait: the unit's button cell, a level text and three bars (green and yellow progress, red
// background) (RTTI EEUICPortrait > UIControl; vtable 0x449fac, 34 slots). EEUICPortrait.cpp is
// 0x25861c..0x25952f (ctor, ??_G, dtor, then the members in alphabetical order, with EEPlayer's two inline lookups
// emitted after their first users). The `return 0` slots are ICF-folded (0xfca98, 0x1ad67e). Names are provisional.
// Requires lle/UIControl.h.
#ifndef EE_EEUICPORTRAIT_H
#define EE_EEUICPORTRAIT_H

#include "ee/EEUnitHandle.h"

class EEButtonObject;
class EEUnit;
class GEBitmap;
class GERhombus;
class GEText;

class EEUICPortrait : public UIControl {
public:
    // 0x25861c (the user-defined control factory 0x274965; the scenario editor 0x1a4f39)
    EEUICPortrait(UIForm* inForm, const GERectangle& inRectangle, unsigned long inId, unsigned long inIndex,
                  bool inShowLevel);
    virtual ~EEUICPortrait();
    virtual long DrawPolygons();            // +0x18 0x258987
    virtual long DrawTextA();               // +0x1c 0x258ad0
    // 0x25891c: the two bar rectangles (the second shortened when both bars show)
    void GetBarRectangles(GERectangle& outFirst, GERectangle& outSecond) const;
    virtual bool GetHelpString(UWideString& outString) const;   // +0x20 0x258af7
    // 0x258f82: shows inUnit (EEUIFMainGame 0x26bfe2, 0x26c38b)
    void SetUnit(EEUnit* inUnit);
    // 0x259028: updates the bars and the level text from the unit (EEUIFMainGame 0x26d0a0)
    void Update();
protected:
    virtual long MouseLButtonDown();        // +0x34 0x258d02
    virtual long MouseLButtonUp() { return 0; }
    virtual long MouseMove(const GE2DPoint& inLocation);    // +0x40 0x258f5d
    virtual long MouseRButtonDown() { return this->MouseLButtonDown(); }  // +0x44 0x258f7d
    virtual long MouseRButtonUp() { return 0; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
    virtual long UpdateKeyboard() { return 0; }

    GEText* mText;                          // +0x0a0 the level number
    GEBitmap* mPortrait;                    // +0x0a4 (new GEBitmap)
    GERhombus* mFirstBar;                   // +0x0a8 green
    GERhombus* mBackground;                 // +0x0ac red
    GERhombus* mSecondBar;                  // +0x0b0 yellow
    EEUnitHandle<EEUnit> mUnit;             // +0x0b4 (referenced; a handle since R5a.4: read on the UI thread and in CopyData)
    EEButtonObject* mButton;                // +0x0b8 the unit class's button (loaded while shown)
    unsigned long mLevel;                   // +0x0bc
    bool mShowFirstBar;                     // +0x0c0
    bool mShowSecondBar;                    // +0x0c1
    bool mShowLevel;                        // +0x0c2
};

#endif
