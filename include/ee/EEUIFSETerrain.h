// C:\EEX\Empire Earth\Scenario Editor\EEUIFSETerrain.h (RECONSTRUCTION; file name inferred). The
// scenario editor's terrain tab (EEUIFSETerrain.cpp 0x1aaad4..0x1ac48b, __FILE__ 0x4c561c in the ctor's ESVERIFYs) and
// its controls: the terrain type and colour list boxes, the paint control, the colour/type change events and the paint
// type radio button (its ctor/??_G live in EEUIFSETRBPaintType.cpp). Names are provisional (RTTI names where there is
// one).
// Requires lle/UIListBox.h, lle/UIRadioButton.h, lle/GEText.h, lle/GEBitmap.h,
// ee/EEUIFSEMainGroup.h, ee/EESEAPaint.h, <vector>.
#ifndef EE_EEUIFSETERRAIN_H
#define EE_EEUIFSETERRAIN_H

struct EeScreensSeGroup;

// one line of the terrain type list (0x1c bytes: UIListBox<EESEUITTData>, the dtor's 0x1c stride)
struct EESEUITTData {
    // user ctor (vector::resize(n)'s `_Ty()` 0x1ade81 zeroes +0x18 then +0x14)
    EESEUITTData() { this->mColorSet = 0; this->mTexture = NULL; }
    UWideString mName;                      // +0x00
    GETexture* mTexture;                    // +0x14 (referenced; the dtor dereferences it)
    long mColorSet;                         // +0x18 the colour list's line for this terrain
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
    // +0x14 0x1ac31d (inline COMDAT): the base CopyData, then the copied focus flag set
    virtual long CopyData()
    {
        long theResult = UIListBox<std::vector<GEColor> >::CopyData();
        this->SetCopiedHasFocus(true);
        return theResult;
    }
    virtual long DrawPolygons();            // +0x18 0x1aaad4 (each line's colours as rhombi)
};

// the terrain type list (RTTI EESEUITerrainTypeLB > UIListBox<EESEUITTData>; vtable 0x445c2c, sizeof 0x144)
class EESEUITerrainTypeLB : public UIListBox<EESEUITTData> {
public:
    // inline (COMDAT 0x1ac350, refused at its first user, EEUIFSETerrain's ctor)
    EESEUITerrainTypeLB(unsigned long inId, GEFont* inFont, std::vector<EESEUITTData>& inData,
                        const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                        unsigned long inBorderThickness)
        : UIListBox<EESEUITTData>(inId, inData, 0x10, false, inRectangle, inForm, inIndex, inBorderThickness)
    {
        this->mBitmap = new GEBitmap;
        this->mText = new GEText(inFont);
        this->mCopyText = new GEText(inFont);
        this->mIsSelectionFramed = false;
    }
    // inline (COMDAT 0x1ac40d; ??_G 0x1ac3f1)
    virtual ~EESEUITerrainTypeLB()
    {
        // the copied lines (the original 0x1ac40d walks +0xb0, mCopyLineArray)
        for (unsigned long i = 0; i < this->mCopyLineArray.size(); i++) {
            if (this->mCopyLineArray[i].mTexture != NULL)
                this->mCopyLineArray[i].mTexture->Dereference();
        }
        this->mBitmap->Dereference();
        delete this->mText;
        delete this->mCopyText;
    }
    virtual long CopyData();                // +0x14 0x1aacf2
    virtual long DrawPolygons();            // +0x18 0x1aade1 (each line's texture)
    virtual long DrawTextA();               // +0x1c 0x1ab1a2 (the names)
    void SetTexture(GETexture* inTexture, unsigned long inLine);

    GEText* mText;                          // +0x134
    GEText* mCopyText;                      // +0x138
    GEBitmap* mBitmap;                      // +0x13c
    bool mIsSelectionFramed;                // +0x140 (CopyData: the copied selection is drawn)
};

// the colour list's selection event (RTTI EEUIETerrainColorChange > UIEvent; vtable 0x445b10; ??_G 0x1ac55b)
class EEUIETerrainColorChange : public UIEvent {
public:
    EEUIETerrainColorChange(EeScreensSeGroup* inTerrain) : mTerrain(inTerrain) {}
    virtual ~EEUIETerrainColorChange() {}   // user-declared (??_G 0x1ac55b stores this vtable first)
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x1ab4bc

    EeScreensSeGroup* mTerrain;             // the tab's group (editor_terrain.rs reads the preview through it)
};

// the type list's selection event (RTTI EEUIETerrainTypeChange > UIEvent; vtable 0x445be4, sizeof 0x10; ??_G 0x1ac586)
class EEUIETerrainTypeChange : public UIEvent {
public:
    EEUIETerrainTypeChange(EeScreensSeGroup* inTerrain) : mTerrain(inTerrain) {}
    virtual ~EEUIETerrainTypeChange() {}    // user-declared (??_G 0x1ac586 stores this vtable first)
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x1ab6d7

    EeScreensSeGroup* mTerrain;             // +0x0c (the tab's group)
};

struct EeScreensSeTerrain;

// whole in Rust (ee_screens editor_terrain.rs): the class holds its object (the in-editor terrains and colour types,
// the controls ids 0xc8 .. 0xcf and 205, the selected terrain)
#endif
