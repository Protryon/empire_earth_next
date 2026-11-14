// C:\EEX\Empire Earth\User Interface Thread\EEUICSelectUnits.h (RECONSTRUCTION; file name inferred; first sketched
// by for EESEUICTriggerSelectUnits). The main game form's
// left-drag selection control (RTTI EEUICSelectUnits > UIControl; vtable 0x44a038; EEUICSelectUnits.cpp
// 0x25952f..0x259f7f: two GEColor `$E` chains, ctor, ??_G, dtor, then the members in alphabetical order): a drag draws
// a box (eight GELines, +0xc0: four black shadow lines offset by one pixel, four white) and hands it to the lasso action
// (+0xcc), a click to the mouse click action (+0xc8). The `return 0` slots (0x1ad67e) and the empty tab slots
// (0x1720c6) are ICF-folded inline overrides. Layout: the ctor's stores; provisional names. Requires lle/UIControl.h,
// ee/EEAction.h, ee/EEAMouseClick.h, ee/EEALasso.h.
#ifndef EE_EEUICSELECTUNITS_H
#define EE_EEUICSELECTUNITS_H

class GELine;

class EEUICSelectUnits : public UIControl {
public:
    virtual bool GetTabable() const { return false; }   // +0x28 datacmp: slot 10 = 0x4ac9f3 (xor al, al)
    // 0x25958f (the user-defined control factory 0x2748f7; EESEUICTriggerSelectUnits 0x1973a8): UIControl(0, inId, a
    // 1x1 rectangle at gViewport's view rectangle top-left, inForm, inIndex, 0); references inClick (+0xc8) and inLasso
    // (+0xcc)
    EEUICSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                     EEALasso* inLasso);
    virtual ~EEUICSelectUnits();            // 0x259739
    virtual long CopyData();                // +0x14 0x259775
    virtual long DrawPolygons();            // +0x18 0x2597e2
    // 0x259ec5: a left press at inLocation starts a selection (EEUIFMainGame 0x270e45, 0x271134; the scenario editor
    // 0x19300b, 0x19c945; provisional name)
    void StartSelection(const GE2DPoint& inLocation);
    // inline accessors (EERenderObject::FillRenderLists picks the units in the drawn box; provisional names)
    bool GetIsDrawn() const { return this->mD1; }
    const GERectangle& GetSelectionRectangle() const { return this->mRA0; }
    // New (port), TEST ONLY readers for the UI fixture's box selection (EETestFixture.cpp): the drag state the UI
    // thread last reached
    bool GetIsDragging() const { return this->mDragging; }
    const GE2DPoint& GetDragEnd() const { return this->mDragEnd; }
protected:
    virtual long MouseLButtonDown() { return 0; }
    virtual long MouseLButtonUp();          // +0x38 0x259bda
    virtual long MouseLButtonDoublePressed();   // +0x3c 0x259af1
    virtual long MouseMove(const GE2DPoint& inLocation);    // +0x40 0x259cfc
    virtual long MouseRButtonDown() { return 0; }
    virtual long MouseRButtonUp() { return 0; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
    virtual long UpdateKeyboard() { return 0; }

    GERectangle mRA0;                       // +0xa0 the box as of the last CopyData, clipped to gViewport's
    GE2DPoint mDragStart;                   // +0xb0 (rounded)
    GE2DPoint mDragEnd;                     // +0xb8 (rounded, clamped to gViewport's view rectangle)
    GELine* mLines;                         // +0xc0 (new GELine[8])
    long mLC4;                              // +0xc4 (not touched by this unit)
    EEAMouseClick* mClick;                  // +0xc8 (referenced; the mouse click action)
    EEALasso* mLasso;                       // +0xcc (referenced)
    bool mDragging;                         // +0xd0 the drag went 5 pixels past the press
    bool mD1;                               // +0xd1 the box is drawn
};

#endif
