// C:\EEX\Empire Earth\Scenario Editor\EEUIFSETerrain.h (RECONSTRUCTION; file name inferred). The
// scenario editor's terrain tab (EEUIFSETerrain.cpp 0x1aaad4..0x1ac48b, __FILE__ 0x4c561c in the ctor's ESVERIFYs) and
// its controls: the terrain type and colour list boxes, the paint control, the colour/type change events and the paint
// type radio button (its ctor/??_G live in EEUIFSETRBPaintType.cpp). Names are provisional (RTTI names where there is
// one).
// Requires lle/UIListBox.h, lle/UIRadioButton.h, lle/GEText.h, lle/GEBitmap.h,
// ee/EEUIFSEMainGroup.h, ee/EESEAPaint.h, <vector>.
#ifndef EE_EEUIFSETERRAIN_H
#define EE_EEUIFSETERRAIN_H

#include "rust/game/screens.h"   // ee_screens_setlb_call

// the terrain tab's statements (editor_setnx.rs: the list boxes' draws and copies are composed in Rust)
extern "C" int32_t ee_screenscpp_setnx(uint32_t inOp, void* const* p, int32_t* i, float* f);

// one line of the terrain type list (0x1c bytes: UIListBox<EESEUITTData>, the dtor's 0x1c stride)
struct EESEUITTData {
    // (the user ctor, vector::resize(n)'s `_Ty()` 0x1ade81, zeroes +0x18 then +0x14: the initializers)
    UWideString mName;                      // +0x00
    GETexture* mTexture = NULL;             // +0x14 (referenced; the dtor dereferences it)
    long mColorSet = 0;                     // +0x18 the colour list's line for this terrain
};

// the terrain colour list (RTTI EESEUITerrainColorLB > UIListBox<vector<GEColor> >; vtable 0x445b24, sizeof 0x134)
class EESEUITerrainColorLB : public UIListBox<std::vector<GEColor> > {
public:
    EESEUITerrainColorLB(unsigned long inId, std::vector<std::vector<GEColor> >& inData, unsigned long inLineHeight,
                         bool inAutoSelect, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                         unsigned long inBorderThickness)
        : UIListBox<std::vector<GEColor> >(inId, inData, inLineHeight, inAutoSelect, inRectangle, inForm, inIndex,
                                            inBorderThickness)
    {
    }
    virtual ~EESEUITerrainColorLB() {}      // +0x00 (??_G 0x1ac32e: its own vptr first)
    virtual long CopyData();                // +0x14 0x1ac31d (inline COMDAT): the base's, the copied focus set
    virtual long DrawPolygons();            // +0x18 0x1aaad4 (each line's colours as rhombi)
};

// the terrain type list (RTTI EESEUITerrainTypeLB > UIListBox<EESEUITTData>; vtable 0x445c2c, sizeof 0x144)
class EESEUITerrainTypeLB : public UIListBox<EESEUITTData> {
public:
    // inline (COMDAT 0x1ac350, refused at its first user, EEUIFSETerrain's ctor)
    EESEUITerrainTypeLB(unsigned long inId, GEFont* inFont, std::vector<EESEUITTData>& inData,
                        const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                        unsigned long inBorderThickness)
        : UIListBox<EESEUITTData>(inId, inData, 0x10, false, inRectangle, inForm, inIndex, inBorderThickness),
          mText(new GEText(inFont)), mCopyText(new GEText(inFont)), mBitmap(new GEBitmap), mIsSelectionFramed(false) {}
    // inline (COMDAT 0x1ac40d; ??_G 0x1ac3f1): the copied lines' textures released, the bitmap, the texts
    // (editor_setnx.rs type_free)
    virtual ~EESEUITerrainTypeLB() { ee_screens_setlb_call(EE_SCREENS_SETLB_TYPE_FREE, this); }
    virtual long CopyData();                // +0x14 0x1aacf2
    virtual long DrawPolygons();            // +0x18 0x1aade1 (each line's texture)
    virtual long DrawTextA();               // +0x1c 0x1ab1a2 (the names)

    GEText* mText;                          // +0x134
    GEText* mCopyText;                      // +0x138
    GEBitmap* mBitmap;                      // +0x13c
    bool mIsSelectionFramed;                // +0x140 (CopyData: the copied selection is drawn)
};

// (the colour and type lists' selection events, RTTI EEUIETerrainColorChange and EEUIETerrainTypeChange, are
// EESEERust kinds: ee/EESEARust.h, ee_screens editor_events.rs)

// whole in Rust (ee_screens editor_terrain.rs): the class holds its object (the in-editor terrains and colour types,
// the controls ids 0xc8 .. 0xcf and 205, the selected terrain)
#endif
