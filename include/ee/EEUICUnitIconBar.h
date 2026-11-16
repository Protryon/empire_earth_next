// C:\EEX\Empire Earth\User Interface Thread\EEUICUnitIconBar.h (RECONSTRUCTION; file name inferred). The main game
// form's selected-units icon bar: one button icon per unit of the selected group (or per cargo unit of a lone
// transport), each with a black/red/green health bar; a click selects the unit, ctrl selects its
// type, shift collects a selection, a right click drops it (RTTI EEUICUnitIconBar > UIControl; vtable 0x44a0c8, 34
// slots). EEUICUnitIconBar.cpp is 0x259f7f..0x25b2ce (ctor, ??_G, dtor, then the members, the message ctors after their
// first users, vector<EEButtonObject*>::push_back last). The `return 0` slot (0x1ad67e) and the empty tab slots
// (0x1720c6) are ICF-folded inline overrides. Layout: the ctor's stores; provisional names. Requires lle/UIControl.h,
// lle/UIControlsTable.h.
#ifndef EE_EEUICUNITICONBAR_H
#define EE_EEUICUNITICONBAR_H

#include "ee/EEUIRustControl.h"

class GEBitmap;
class UIControlsTable;

class EEUICUnitIconBar : public EEUIRustControl {
public:
    // 0x259f7f (the user-defined control factory 0x274b82)
    EEUICUnitIconBar(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm);
    virtual ~EEUICUnitIconBar();
    virtual long CopyData();                // +0x14 0x25a258
    virtual long DrawPolygons();            // +0x18 0x25a32f
    // 0x25b010: refills the icons from the local player's selected group (EEUIFMainGame 0x26814f) (provisional name)
    void Update();
protected:
    virtual long MouseLButtonDown() { return this->MouseRButtonDown(); }   // +0x34 (ICF-folded with 0x25ae3b)
    virtual long MouseLButtonUp();          // +0x38 0x25aa9b
    virtual long MouseMove(const GE2DPoint& inLocation);    // +0x40 0x25afa5
    virtual long MouseRButtonDown();        // +0x44 0x25ae3b
    virtual long MouseRButtonUp();          // +0x48 0x25ae44
    // the bar's own ops (EEUIRustControl's host the toolkit's)
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF);

    // (+0x0a0..+0x141: the icons' buttons, the Shift-collected ids and icons, the health values, the icon rectangle,
    // the spacing and the cargo and collecting flags, each with its snapshot, are ee_screens icon_bar.rs' IconBar)
protected:
    void* mState;                           // the Rust state (ee_screens_icon_bar_new)
    GEBitmap* mIcon;                        // +0x130 (new GEBitmap)
    GEBitmap* mBackground;                  // +0x134 the table's normal bitmap
};

#endif
