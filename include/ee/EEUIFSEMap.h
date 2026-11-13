// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEMap.h (RECONSTRUCTION; file name inferred).
// The scenario editor's map tab (RTTI EEUIFSEMap > EEUIFSEMainGroup; vtable 0x4454ac; ctor 0x1a1be6, dtor 0x1a3b27)
// and its helper controls: the "create map" button (RTTI UIBMap > UIButton; vtable 0x445368, sizeof 0x108), the
// three map-kind radio buttons (RTTI UIRBMap > UIRadioButton; vtable 0x445404, sizeof 0x114) and the elevation file
// combo box's event (RTTI EEUIFSEEMapType > UIEvent; vtable 0x445354, sizeof 0x10). Member names from the ctor's
// ESVERIFY texts where there are any ("this->mMapStyle", ...), the rest provisional (by control id).
// Requires lle/UIButton.h, lle/UIRadioButton.h, lle/UIEvent.h, ee/EEUIFSEMainGroup.h.
#ifndef EE_EEUIFSEMAP_H
#define EE_EEUIFSEMAP_H

class EETerrainTable;
class EEUIFileComboBox;
class EEUIFSEMap;
class GEFont;
class UIComboBox;
class UIEditBox;
class UITextLabel;

// the "create map" button (id 0x65): ButtonUp creates the map
class UIBMap : public UIButton {
public:
    UIBMap(unsigned long inId, const GE2DPoint& inLocation, float inWidth, unsigned long inHeight, UIForm* inForm,
           unsigned long inIndex, EEUIFSEMap* inMap)
        : UIButton(inId, NULL, NULL, NULL, GERectangle(inLocation, inWidth, (float)inHeight), inForm, inIndex, 2),
          mMap(inMap)
    {
    }
    virtual ~UIBMap() {}                    // ??_G 0x1a3a9a
protected:
    virtual void ButtonUp();                // slot 0x8c 0x1a18f1
public:
    EEUIFSEMap* mMap;                       // +0x104
};

// a map-kind radio button (ids 0x6e blank, 0x6f random, 0x6d random with a typed seed): Check shows the controls of
// its kind
class UIRBMap : public UIRadioButton {
public:
    UIRBMap(unsigned long inId, const UWideString& inText, GEFont* inFont, const GE2DPoint& inLocation, float inWidth,
            unsigned long inHeight, UIForm* inForm, unsigned long inIndex, bool inIsChecked)
        : UIRadioButton(inId, inText, inFont, GERectangle(inLocation, inWidth, (float)inHeight), inForm, inIndex, 0,
                        false, inIsChecked, false)
    {
    }
    virtual ~UIRBMap() {}                   // ??_G 0x1a3abd
    virtual void Check();                   // slot 0x94 0x1a1907
    virtual void HideControls();            // slot 0x9c 0x1a1a6b (provisional name)
};

// (the elevation file combo box's selection event (0x3eb), EEUIFSEEMapType, is an EESEERust: ee/EESEARust.h)

class EEUIFSEMap : public EEUIFSEMainGroup {
public:
    EEUIFSEMap(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont);   // 0x1a1be6
    virtual ~EEUIFSEMap();                  // +0x00 0x1a3b27 (??_G 0x1a3b0b)
    virtual void Enter();                   // +0x18 0x1a3ebf (re-reads the elevation files)
    virtual void MouseMove(const GE2DPoint& inLocation) {}   // +0x24 (ICF: 0x156dd8)
    void CreateMap();                       // 0x1a3c7d (UIBMap::ButtonUp)
    void ShowElevationControls(bool inShow);   // 0x1a3dec (provisional name)

    std::vector<EETerrainTable*> mTerrains; // +0x04 the loaded terrains with +0x85 set (the default terrain list)
    UIBMap* mCreateMap;                     // +0x14 (0x65)
    UITextLabel* mDefaultTerrainLabel;      // +0x18 (0x64)
    UITextLabel* mRandomMapSizeLabel;       // +0x1c (0x73)
    UITextLabel* mWidthLabel;               // +0x20 (0x66)
    UITextLabel* mHeightLabel;              // +0x24 (0x67)
    UITextLabel* mMapStyleLabel;            // +0x28 (0x68)
    UIComboBox* mDefaultTerrain;            // +0x2c (0x69)
    UIComboBox* mMapStyle;                  // +0x30 (0x6a)
    UIComboBox* mWidth;                     // +0x34 (0x6b)
    UIComboBox* mHeight;                    // +0x38 (0x6c)
    UIComboBox* mRandomMapSize;             // +0x3c (0x72)
    UIEditBox* mSeedEditBox;                // +0x40 (0x70)
    UITextLabel* mSeedLabel;                // +0x44 (0x71)
    EEUIFileComboBox* mElevationFile;       // +0x48 (0x75)
    UITextLabel* mElevationFileLabel;       // +0x4c (0x74)
    UITextLabel* mWaterDepthLabel;          // +0x50 (0x7b)
    UITextLabel* mWaterCutoffLabel;         // +0x54 (0x7a)
    UITextLabel* mLandHeightLabel;          // +0x58 (0x77)
    UITextLabel* mLandCutoffLabel;          // +0x5c (0x76)
    UIEditBox* mWaterDepth;                 // +0x60 (0x7c)
    UIEditBox* mWaterCutoff;                // +0x64 (0x7d)
    UIEditBox* mLandHeight;                 // +0x68 (0x79)
    UIEditBox* mLandCutoff;                 // +0x6c (0x78)
    UIRBMap* mMap[3];                       // +0x70 (0x6e, 0x6f, 0x6d)
};

#endif
