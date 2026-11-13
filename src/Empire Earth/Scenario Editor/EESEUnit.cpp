// C:\EEX\Empire Earth\Scenario Editor\EESEUnit.cpp (file name inferred: no __FILE__; RTTI class EESEUnit). The scenario
// editor's unit tab: 0x199afd..0x19cc39. Its helper controls first, in definition order (EEUIFSELBUnitList,
// EEUIFSELBCargoList, EEUIFSEBUnitType, EEUIFSEBRotation, EEUIFSEBRemoveCargo: not alphabetical, so one unit), then
// the tab's ctor, the inline action ctors it refused, the members, and EEUIBuildWall's ctor COMDAT (first user: the
// ctor) with the build modes' ??_Gs. After EESEUITechListBox.cpp, before EEUIFScenarioEditor.cpp (0x19cc39). Unit
// flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"
#include "ee/EEPlayer.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EEUICCommandUnits.h"
#include "ee/EESEARust.h"
#include "ee/EESEUICCommandUnits.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/EEMiniMap.h"


// cdecl helpers of other units (provisional names; types inferred)
void EEPreviewWall(std::vector<EETileCoordinate>& inTiles, long inTechnology, long inPlayer);  // 0x23eb4c (wall ghosts)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType);  // 0x21176e: restores the colour of inPlayer's walls
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection);  // 0x2116a2: a gate fits on inTile's wall
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (the ghost at the mouse)
extern unsigned long& gTexturePixelDepth;

// The upgrade panel is a real 0x60-byte class (new at 0x59b976 in the tab's ctor); both its ctor counters
// are mutable
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFSEUpgradePanel.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"






// The unit tab's list of unit classes (RTTI EEUIFSELBUnitList > UILBString; vtable 0x444b30; sizeof 0x154):
// mUnitTypes holds the class index of each line, sorted by mCompare (by name, by epoch, by layer)
// (provisional names)
class EEUIFSELBUnitList : public UILBString {
public:
    EEUIFSELBUnitList(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont, bool inAutoSelect,
                      const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                      unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUIFSELBUnitList() {}
    virtual long UpdateKeyboard();          // +0x7c 0x199e81
    virtual void ButtonUp();                // +0xa4 0x199bc9
    void AddUnitType(long inUnitType);      // 0x199bf2
    void SetSortMode(long inMode);          // 0x199d49

    std::vector<long> mUnitTypes;           // +0x13c
    long mSortMode;                         // +0x14c 0 name, 1 epoch, 2 layer
    bool (*mCompare)(long inLeft, long inRight);   // +0x150
};

// The cargo of the selected units (RTTI EEUIFSELBCargoList > UILBString; vtable 0x444bf0; sizeof 0x14c)
class EEUIFSELBCargoList : public UILBString {
public:
    EEUIFSELBCargoList(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont, bool inAutoSelect,
                       const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                       unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUIFSELBCargoList() {}
    void AddCargo(EEUnit* inUnit);          // 0x199f74 (sorted by name)
    void RemoveSelected();                  // 0x19a05f

    std::vector<EEUnitHandle<EEUnit> > mCargo;   // +0x13c (the carried units shown; handles since R5a.4, the vector's order kept)
};

// One of the six unit type buttons (RTTI EEUIFSEBUnitType > UIRadioButton; vtable 0x444cb0; id 0x406..0x40b)
class EEUIFSEBUnitType : public UIRadioButton {
public:
    EEUIFSEBUnitType(unsigned long inID, GEBitmap* inUp, GEBitmap* inFocused, GEBitmap* inDown,
                     const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                     unsigned long inIndex, unsigned long inBorderThickness, bool inIsChecked);
    virtual ~EEUIFSEBUnitType() {}
    virtual void Check();                   // +0x94 0x19a1ae (sets the global unit type mode)
};

// The rotation button (RTTI EEUIFSEBRotation > UIButton; vtable 0x444d50; sizeof 0x11c; id 0x3f5): left click turns
// one way, right click the other; eight steps of 45 degrees
class EEUIFSEBRotation : public UIButton {
public:
    EEUIFSEBRotation(unsigned long inID, GEBitmap* inNormal, GEBitmap* inFocused, GEBitmap* inPressed,
                     const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                     unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUIFSEBRotation();            // 0x19a4d2 (??_G 0x19a4b6)
    virtual long UpdateKeyboard();          // +0x7c 0x19a728
    virtual void ButtonUp();                // +0x8c 0x19a515
    virtual long MouseLButtonDown();        // +0x34 0x19a5ba
    virtual long MouseRButtonDown();        // +0x44 0x19a5ca
    // +0x48 (ICF-folded into EEUIBUpgrade::MouseLButtonUp 0x256949)
    virtual long MouseRButtonUp() { return UIButton::MouseLButtonUp(); }
    float GetAngle();                       // 0x19a5d7
    void UpdateBitmaps();                   // 0x19a636
    void RotateGhosts();                    // 0x19a664
    void RotateSelection();                 // 0x19a7e5

    GEBitmap* mBitmaps[4];                  // +0x104 (straight, diagonal) x (normal, pressed), flat
    long mRotation;                         // +0x114 0..7
    long mDirection;                        // +0x118 +1/-1 while a button is down
};

// The remove cargo button (RTTI EEUIFSEBRemoveCargo > UIButton; vtable 0x444de8; id 0x3f4)
class EEUIFSEBRemoveCargo : public UIButton {
public:
    EEUIFSEBRemoveCargo(unsigned long inID, GEBitmap* inNormal, GEBitmap* inFocused, GEBitmap* inPressed,
                        const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                        unsigned long inIndex, unsigned long inBorderThickness);   // 0x19a8a2
    virtual ~EEUIFSEBRemoveCargo() {}
    virtual void ButtonUp();                // +0x8c 0x19a880
};

// The sort radio buttons' event (RTTI EEUIFSEESortUnitList > UIEvent; vtable 0x444e88; new 0xc): an EESEERust
// (ee_screens editor_events.rs)

// the unit type mode the six buttons select (EEUIFSEBUnitType::Check; 0x531ff0) (provisional name)
static long sUnitTypeMode;

static bool EESEUnitNameLess(long inLeft, long inRight);
static bool EESEUnitEpochLess(long inLeft, long inRight);
static bool EESEUnitLayerLess(long inLeft, long inRight);

// the unit tab's current player (inline; the original expands it in PlaceWall, PlaceGate, SelectUnitType and
// Virtual24). Our VC6 refuses it (inline budget: two locals); PlaceWall/PlaceGate spell it out instead (exact).
inline long EESEUnit::GetCurrentPlayer()
{
    long thePlayer = 0;
    if (!this->mWorldButton->GetIsChecked()) {
        unsigned long theSelection;
        this->mPlayerComboBox->GetCurrentSelection(theSelection);
        thePlayer = theSelection + 1;
    }
    return thePlayer;
}

// The six unit type buttons: the index, the normal and pressed cells in "textures\scenunit", the help string
struct EESEUnitTypeButton {
    long mIndex;
    float mNormalX;
    float mNormalY;
    float mPressedX;
    float mPressedY;
    unsigned long mHelpString;
};

static EESEUnitTypeButton sUnitTypeButtons[6] = {   // 0x8c4458
    { 0, 0.0f, 146.0f, 37.0f, 146.0f, 8501 },
    { 1, 74.0f, 146.0f, 111.0f, 146.0f, 8505 },
    { 2, 148.0f, 146.0f, 185.0f, 146.0f, 8502 },
    { 3, 0.0f, 171.0f, 37.0f, 171.0f, 8504 },
    { 4, 74.0f, 171.0f, 111.0f, 171.0f, 8503 },
    { 5, 148.0f, 171.0f, 185.0f, 171.0f, 8500 },
};

// FUNCTION: EXE 0x599afd
// Creates the unit tab's unit class list, sorted by name
EEUIFSELBUnitList::EEUIFSELBUnitList(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont,
                                     bool inAutoSelect, const GE2DPoint& inLocation, unsigned long inWidth,
                                     unsigned long inHeight, UIForm* inForm, unsigned long inIndex,
                                     unsigned long inBorderThickness)
    : UILBString(inID, inStrings, inFont, inAutoSelect, GERectangle(inLocation, (float)inWidth, (float)inHeight),
                 inForm, inIndex, inBorderThickness),
      mSortMode(kUnitSortAlpha)
{
    this->mCompare = EESEUnitNameLess;
}

// SYNTHETIC: EXE 0x599b99
// (EEUIFSELBUnitList's scalar deleting destructor)

// FUNCTION: EXE 0x599bc9
// Picks the clicked unit class for placement on the map, then fires the list's button-up event
void EEUIFSELBUnitList::ButtonUp()
{
    EESEUnit* theScreen = (static_cast<EEUIFScenarioEditor*>(this->mForm))->GetUnitScreen();
    theScreen->SelectUnitType(this->mForm->GetMouseLocation());
    this->FireTriggerEvent(kControlTriggerButtonUp);
}

// FUNCTION: EXE 0x599bf2
// Inserts a unit class at its sorted place, prefixed with its family or epoch number unless sorted by name
void EEUIFSELBUnitList::AddUnitType(long inUnitType)
{
    UWideString theText;
    UWideString theName;
    unsigned long theCount = this->mUnitTypes.size();
    unsigned long i;
    for (i = 0; i < theCount; i++) {
        if (this->mCompare(inUnitType, this->mUnitTypes[i]))
            break;
    }
    if (this->mSortMode == kUnitSortAlpha)
        gResource->GetString(gUnitClassTable[inUnitType]->mNameStringID, theText);
    else {
        long theValue;
        if (this->mSortMode == kUnitSortFamily)
            theValue = gUnitClassTable[inUnitType]->mFamily;
        else
            theValue = gUnitClassTable[inUnitType]->mEpoch + 1;
        sprintf(theText, "%ld: ", theValue);
        gResource->GetString(gUnitClassTable[inUnitType]->mNameStringID, theName);
        theText += theName;
    }
    UILBNoSort<UWideString> theTemp1;
    this->InsertLine(theText, i, theTemp1);
    this->mUnitTypes.insert(this->mUnitTypes.begin() + i, inUnitType);
}

// FUNCTION: EXE 0x599d49
// Picks the comparator for later inserts (by epoch, by family, else by name); lines already listed stay
void EEUIFSELBUnitList::SetSortMode(long inMode)
{
    if (this->mSortMode == inMode)
        return;
    switch (inMode) {
    case kUnitSortEpoch:
        this->mCompare = EESEUnitEpochLess;
        break;
    case kUnitSortFamily:
        this->mCompare = EESEUnitLayerLess;
        break;
    default:
        this->mCompare = EESEUnitNameLess;
        break;
    }
    this->mSortMode = inMode;
}

// FUNCTION: EXE 0x599d88
// Orders two unit classes by epoch, then by name
static bool EESEUnitEpochLess(long inLeft, long inRight)
{
    if (gUnitClassTable[inLeft]->mEpoch > gUnitClassTable[inRight]->mEpoch)
        return false;
    if (gUnitClassTable[inLeft]->mEpoch < gUnitClassTable[inRight]->mEpoch)
        return true;
    return EESEUnitNameLess(inLeft, inRight);
}

// FUNCTION: EXE 0x599dc4
// Orders two unit classes by their displayed names
static bool EESEUnitNameLess(long inLeft, long inRight)
{
    UWideString theLeft;
    UWideString theRight;
    gResource->GetString(gUnitClassTable[inLeft]->mNameStringID, theLeft);
    gResource->GetString(gUnitClassTable[inRight]->mNameStringID, theRight);
    return theLeft < theRight ? true : false;
}

// FUNCTION: EXE 0x599e45
// Orders two unit classes by family, then by name
static bool EESEUnitLayerLess(long inLeft, long inRight)
{
    if (gUnitClassTable[inLeft]->mFamily > gUnitClassTable[inRight]->mFamily)
        return false;
    if (gUnitClassTable[inLeft]->mFamily < gUnitClassTable[inRight]->mFamily)
        return true;
    return EESEUnitNameLess(inLeft, inRight);
}

// FUNCTION: EXE 0x599e81
// Handles the list's keys; Enter picks the selected unit class for placement
long EEUIFSELBUnitList::UpdateKeyboard()
{
    long theResult = UILBString::UpdateKeyboard();
    if (UIControl::sEnterKey->GetValue()) {
        EESEUnit* theScreen = (static_cast<EEUIFScenarioEditor*>(this->mForm))->GetUnitScreen();
        theScreen->SelectUnitType(this->mForm->GetMouseLocation());
    }
    return theResult;
}

// FUNCTION: EXE 0x599eb9
// Creates the unit tab's (empty) cargo list
EEUIFSELBCargoList::EEUIFSELBCargoList(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont,
                                       bool inAutoSelect, const GE2DPoint& inLocation, unsigned long inWidth,
                                       unsigned long inHeight, UIForm* inForm, unsigned long inIndex,
                                       unsigned long inBorderThickness)
    : UILBString(inID, inStrings, inFont, inAutoSelect, GERectangle(inLocation, (float)inWidth, (float)inHeight),
                 inForm, inIndex, inBorderThickness)
{
}

// SYNTHETIC: EXE 0x599f44
// (EEUIFSELBCargoList's scalar deleting destructor)

// FUNCTION: EXE 0x599f74
// Inserts a carried unit into the cargo list, keeping the lines sorted by unit name
void EEUIFSELBCargoList::AddCargo(EEUnit* inUnit)
{
    UWideString theName;
    gResource->GetString(inUnit->mUnitClass->mNameStringID, theName);
    unsigned long theIndex = 0;
    if (this->GetLineCount() == 0)
        this->mCargo.insert(this->mCargo.begin(), inUnit);
    else {
        std::vector<EEUnitHandle<EEUnit> >::iterator it;
        for (it = this->mCargo.begin(); it != this->mCargo.end(); it++) {
            if (theName < this->mData[theIndex])
                break;
            theIndex++;
        }
        this->mCargo.insert(it, inUnit);
    }
    UILBNoSort<UWideString> theTemp2;
    this->InsertLine(theName, theIndex, theTemp2);
}

// FUNCTION: EXE 0x59a05f
// Deletes the unit on the selected cargo line from the map (posts a delete-unit action)
void EEUIFSELBCargoList::RemoveSelected()
{
    unsigned long theSelection;
    if (this->GetLineCount() > 0 && this->GetCurrentSelection(theSelection) == kErrorOK) {
        EESEARust* theAction = new EESEARust(kSEADeleteUnit, NULL, this->mCargo[theSelection]->GetUnitID());
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x59a0b1
// Creates one of the six unit type buttons from its normal and pressed bitmaps
EEUIFSEBUnitType::EEUIFSEBUnitType(unsigned long inID, GEBitmap* inUp, GEBitmap* inFocused, GEBitmap* inDown,
                                   const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight,
                                   UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness,
                                   bool inIsChecked)
    : UIRadioButton(inID, UWideString(""), gU531930, GERectangle(inLocation, (float)inWidth, (float)inHeight),
                    inForm, inIndex, inBorderThickness, true, inIsChecked, false)
{
    this->SetUpCheckedBitmap(inDown);
    this->SetDownCheckedBitmap(inDown);
    this->SetDownUncheckedBitmap(inDown);
    this->SetUpUncheckedBitmap(inUp);
    this->SetFocusedBitmap(inFocused);
    this->mBitmapDrawOffset.SetXY(0.0f, 0.0f);
}

// SYNTHETIC: EXE 0x59a18b
// (EEUIFSEBUnitType's scalar deleting destructor)

// FUNCTION: EXE 0x59a1ae
// Sets the unit palette (land, air, naval, structures, heroes or resources) from the button's id, then checks it
void EEUIFSEBUnitType::Check()
{
    switch (this->mId) {
    case 0x406:
        sUnitTypeMode = kUnitPaletteLandUnits;
        break;
    case 0x407:
        sUnitTypeMode = kUnitPaletteAirUnits;
        break;
    case 0x408:
        sUnitTypeMode = kUnitPaletteNavalUnits;
        break;
    case 0x409:
        sUnitTypeMode = kUnitPaletteStructures;
        break;
    case 0x40a:
        sUnitTypeMode = kUnitPaletteHeroes;
        break;
    case 0x40b:
        sUnitTypeMode = kUnitPaletteResources;
        break;
    }
    UIRadioButton::Check();
}

// NEAR: differs in the texture coordinate block
// FUNCTION: EXE 0x59a213
// Creates the rotation button with its straight and diagonal arrow bitmaps (normal and pressed), at step 0
EEUIFSEBRotation::EEUIFSEBRotation(unsigned long inID, GEBitmap* inNormal, GEBitmap* inFocused, GEBitmap* inPressed,
                                   const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight,
                                   UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UIButton(inID, inNormal, inFocused, inPressed, GERectangle(inLocation, (float)inWidth, (float)inHeight),
               inForm, inIndex, inBorderThickness)
{
    GETexture* theTexture = static_cast<EEUIFScenarioEditor*>(inForm)->mButtonTexture;
    float theSize = (float)theTexture->GetWidth();
    unsigned long i;
    long j;

    this->mRotation = 0;
    this->mDirection = 0;
    for (j = 0; j < 4; j++)
        this->mBitmaps[j] = new GEBitmap;
    for (i = 0; i < 2; i++) {
        this->mBitmaps[i * 2]->SetTexture(theTexture, inForm->GetManager()->GetScreen());
        this->mBitmaps[i * 2 + 1]->SetTexture(theTexture, inForm->GetManager()->GetScreen());
        if (i & 1) {
            this->mBitmaps[i * 2]->SetTextureCoordinates(
                GETextureCoordinate(100.0f / theSize, 0.0f / theSize), GETextureCoordinate(150.0f / theSize, 0.0f / theSize),
                GETextureCoordinate(100.0f / theSize, 50.0f / theSize), GETextureCoordinate(150.0f / theSize, 50.0f / theSize));
            this->mBitmaps[i * 2 + 1]->SetTextureCoordinates(
                GETextureCoordinate(150.0f / theSize, 0.0f / theSize), GETextureCoordinate(200.0f / theSize, 0.0f / theSize),
                GETextureCoordinate(150.0f / theSize, 50.0f / theSize), GETextureCoordinate(200.0f / theSize, 50.0f / theSize));
        } else {
            this->mBitmaps[i * 2]->SetTextureCoordinates(
                GETextureCoordinate(0.0f / theSize, 0.0f / theSize), GETextureCoordinate(50.0f / theSize, 0.0f / theSize),
                GETextureCoordinate(0.0f / theSize, 50.0f / theSize), GETextureCoordinate(50.0f / theSize, 50.0f / theSize));
            this->mBitmaps[i * 2 + 1]->SetTextureCoordinates(
                GETextureCoordinate(50.0f / theSize, 0.0f / theSize), GETextureCoordinate(100.0f / theSize, 0.0f / theSize),
                GETextureCoordinate(50.0f / theSize, 50.0f / theSize), GETextureCoordinate(100.0f / theSize, 50.0f / theSize));
        }
        this->mBitmaps[i * 2]->SetXYScale(50.0f / theSize, 50.0f / theSize);
        this->mBitmaps[i * 2 + 1]->SetXYScale(50.0f / theSize, 50.0f / theSize);
    }
    this->UpdateBitmaps();
    this->mAlwaysDirty = true;
}

// SYNTHETIC: EXE 0x59a4b6
// (EEUIFSEBRotation's scalar deleting destructor)

// FUNCTION: EXE 0x59a4d2
// Destroys the rotation button, releasing its four arrow bitmaps
EEUIFSEBRotation::~EEUIFSEBRotation()
{
    this->mBitmaps[0]->Dereference();
    this->mBitmaps[1]->Dereference();
    this->mBitmaps[2]->Dereference();
    this->mBitmaps[3]->Dereference();
}

// FUNCTION: EXE 0x59a515
// Turns the placement one 45-degree step (wrapping 0..7) and rotates the ghosts if any, else the
// selected units; then turns the arrow bitmaps to match
void EEUIFSEBRotation::ButtonUp()
{
    this->mRotation += this->mDirection;
    if (this->mRotation > 7)
        this->mRotation = 0;
    else if (this->mRotation < 0)
        this->mRotation = 7;
    float theAngle = (float)(unsigned long)(this->mRotation >> 1) * 1.5707964f;
    for (long i = 0; i < 4; i++) {
        this->mBitmaps[i]->SetRotation(theAngle);
    }
    if (gWorld.GetNumGhostUnits() > 0)
        this->RotateGhosts();
    else
        this->RotateSelection();
    this->UpdateBitmaps();
    this->mDirection = 0;
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x59a5ba
// Presses the button so its release turns one step forward
long EEUIFSEBRotation::MouseLButtonDown()
{
    this->mDirection = 1;
    return UIButton::MouseLButtonDown();
}

// FUNCTION: EXE 0x59a5ca
// Presses the button (as a left press) so its release turns one step back
long EEUIFSEBRotation::MouseRButtonDown()
{
    this->mDirection = -1;
    return UIButton::MouseLButtonDown();
}

// FUNCTION: EXE 0x59a5d7
// Returns the placement yaw in radians: 45 degrees at step 0, 45 degrees less per step (step 1 is 0)
float EEUIFSEBRotation::GetAngle()
{
    float theAngle = 0.0f;
    switch (this->mRotation) {
    case 0:
        theAngle = 0.78539819f;
        break;
    case 2:
        theAngle = -0.78539819f;
        break;
    case 3:
        theAngle = -1.5707964f;
        break;
    case 4:
        theAngle = -2.3561945f;
        break;
    case 5:
        theAngle = 3.1415927f;
        break;
    case 6:
        theAngle = 2.3561945f;
        break;
    case 7:
        theAngle = 1.5707964f;
        break;
    }
    return theAngle;
}

// FUNCTION: EXE 0x59a636
// Shows the straight (even step) or diagonal (odd step) arrow bitmaps
void EEUIFSEBRotation::UpdateBitmaps()
{
    unsigned long theKind = (this->mRotation & 1) * 2;
    this->SetNormalBitmap(this->mBitmaps[theKind]);
    this->SetPressedBitmap(this->mBitmaps[theKind + 1]);
}

// FUNCTION: EXE 0x59a664
// Turns every placement ghost whose class can turn to the button's angle
void EEUIFSEBRotation::RotateGhosts()
{
    GETransformation theTransformation;
    float theAngle = this->GetAngle();
    gWorld.mGhostUnitsLock->Enter();
    unsigned long theCount = gWorld.GetNumGhostUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEGhostUnit* theGhost = static_cast<EEGhostUnit*>(gWorld.mGhostUnits[i]);
        if (!(gUnitClassTable[theGhost->mUnitClass->mUnitType]->mAngularAcceleration > 0.0f))
            continue;
        theTransformation = theGhost->mTransformation;
        theTransformation.SetOrientationYPR(theAngle, 0.0f, 0.0f);
        theGhost->mTransformation = theTransformation;
    }
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x59a728
// Lets the comma and period keys turn the placement back and forward like presses of the button
long EEUIFSEBRotation::UpdateKeyboard()
{
    ISEntity* theKey;
    gISKeyboard->GetEntity(kKeyComma, theKey);
    if (theKey->JustPressed()) {
        this->mDirection = -1;
        this->ButtonDown();
        this->CaptureKeyboard();
    } else if (theKey->JustUnpressed()) {
        this->ButtonUp();
        this->ReleaseCapturedKeyboard();
    }
    long theResult = gISKeyboard->GetEntity(kKeyPeriod, theKey);
    if (theKey->JustPressed()) {
        this->mDirection = 1;
        this->ButtonDown();
        this->CaptureKeyboard();
    } else if (theKey->JustUnpressed()) {
        this->ButtonUp();
        this->ReleaseCapturedKeyboard();
    }
    return theResult;
}

// FUNCTION: EXE 0x59a7e5
// Turns every selected unit whose class can turn to the button's angle
void EEUIFSEBRotation::RotateSelection()
{
    GETransformation theTransformation;
    float theAngle = this->GetAngle();
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mForm);
    unsigned long theCount = theEditor->GetSelectedUnitCount();
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnit* theUnit = theEditor->GetSelectedUnit(i);
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit == NULL || !(theUnit->mUnitClass->mAngularAcceleration > 0.0f))
            continue;
        theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
        theTransformation.SetOrientationYPR(theAngle, 0.0f, 0.0f);
        theComplexUnit->SetTransformation(theTransformation, true);
    }
}

// FUNCTION: EXE 0x59a880
// Deletes the unit on the cargo list's selected line, then releases the button
void EEUIFSEBRemoveCargo::ButtonUp()
{
    static_cast<EEUIFSELBCargoList*>(this->mForm->GetControlById(0x3f2))->RemoveSelected();
    UIButton::ButtonUp();
}

// FUNCTION: EXE 0x59a8a2
// Creates the remove cargo button
EEUIFSEBRemoveCargo::EEUIFSEBRemoveCargo(unsigned long inID, GEBitmap* inNormal, GEBitmap* inFocused,
    GEBitmap* inPressed, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
    unsigned long inIndex, unsigned long inBorderThickness)
    : UIButton(inID, inNormal, inFocused, inPressed, GERectangle(inLocation, (float)inWidth, (float)inHeight),
               inForm, inIndex, inBorderThickness)
{
}

// SYNTHETIC: EXE 0x59a91b
// (EEUIFSEBRemoveCargo's scalar deleting destructor)

// The unit type buttons' cell width: a plain (non-folded) .rdata float in the
// original (0x444e7c), read by the ctor only (provisional name)
static const float kSEUButtonWidth = 35.0f;
// (the sort controls are 67 wide: GERectangle folds 67.0f - 1.0f to the 66.0f at 0x444e80, x + 66 with no fsub)

// FUNCTION: EXE 0x59a93e
// Builds the unit tab: owner and sort radio buttons, the six unit type buttons, the unit and cargo lists,
// the remove and rotate buttons, the editor actions, the panel background and the upgrade panel
EESEUnit::EESEUnit(unsigned long& ioIndex, EEUIFScenarioEditor* inForm, GEFont* inFont, UIComboBox* inPlayers)
    : mL6C(0), mL70(0), mL74(0), mL78(0), mL7C(0), mL80(0), mL84(0), mPlacing(false)
{
    GETextureDescription theDescription = { 1, 0, gTexturePixelDepth, 0, false, false, true, true };
    UWideString theText;
    GE2DPoint thePosition;
    GETexture* theTexture = inForm->mButtonTexture;
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;
    float theSize = (float)theTexture->GetWidth();
    GEText* theGEText;
    long i;

    this->mSortEvent = new EESEERust(kSEESortUnitList);
    this->mPlayerComboBox = inPlayers;
    this->mRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f));

    gResource->GetString(IDS_CARGO, theText);
    thePosition.SetXY(260.0f, 613.0f);
    this->mTitleLabel = new UITextLabel(theText, inFont, 0x3f3, GERectangle(thePosition, 225.0f, (float)inFont->GetHeight()),
                                  inForm, 0, ioIndex, 0);
    this->mTitleLabel->SetAlignment(kTextAlignLeft);
    this->mTitleLabel->SetBgOpacity(0.4f);
    theGEText = this->mTitleLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTitleLabel, 6);
    ioIndex++;

    gResource->GetString(IDS_WORLD_PLAYER, theText);
    thePosition.SetXY(0.0f, 30.0f);
    this->mWorldButton = new UIRadioButton(1000, theText, inFont, GERectangle(thePosition, 140.0f, (float)inFont->GetHeight()),
                                     inForm, ioIndex, 0, false, false, false);
    this->mWorldButton->SetBgOpacity(0.4f);
    theGEText = this->mWorldButton->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mWorldButton, 6);
    ioIndex++;
    this->mWorldButton->SetTriggerEvent(kControlTriggerChecked, this->mSortEvent);

    gResource->GetString(IDS_SE_PLAYERS, theText);
    thePosition.SetY(this->mWorldButton->GetRectangle().GetBottom() + 2.0f);
    this->mSeaButton = new UIRadioButton(0x3ea, theText, inFont, GERectangle(thePosition, 140.0f, (float)inFont->GetHeight()),
                                   inForm, ioIndex, 0, false, true, false);
    this->mSeaButton->SetBgOpacity(0.4f);
    theGEText = this->mSeaButton->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSeaButton, 6);
    ioIndex++;
    this->mSeaButton->SetTriggerEvent(kControlTriggerChecked, this->mSortEvent);

    gResource->GetString(IDS_SORT, theText);
    thePosition.SetXY(145.0f, 30.0f);
    this->mSortLabel = new UITextLabel(theText, inFont, 0x3ed, GERectangle(thePosition, 67.0f, (float)inFont->GetHeight()),
                                 inForm, 0, ioIndex, 0);
    this->mSortLabel->SetAlignment(kTextAlignLeft);
    this->mSortLabel->SetBgOpacity(0.4f);
    theGEText = this->mSortLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSortLabel, 6);
    ioIndex++;

    for (i = 0; i < 6; i++)
        this->mUnitTypeButtons[i] = NULL;
    float theYScale = 25.0f / theSize;
    float theXScale = kSEUButtonWidth / theSize;
    unsigned long theX = 0;
    for (i = 0; i < 6; i++) {
        EESEUnitTypeButton* theButton = &sUnitTypeButtons[i];
        thePosition.SetXY((float)theX, 0.0f);
        GEBitmap* theNormal = new GEBitmap;
        long theIndex = theButton->mIndex;
        GEBitmap* thePressed = new GEBitmap;
        theNormal->SetTexture(theTexture, inForm->GetManager()->GetScreen());
        thePressed->SetTexture(theTexture, inForm->GetManager()->GetScreen());
        theTopLeft.SetU(theButton->mNormalX / theSize);
        theTopLeft.SetV(theButton->mNormalY / theSize);
        theBottomRight.SetU((theButton->mNormalX + kSEUButtonWidth) / theSize);
        theBottomRight.SetV((theButton->mNormalY + 25.0f) / theSize);
        theTopRight.SetU(theBottomRight.GetU());
        theTopRight.SetV(theTopLeft.GetV());
        theBottomLeft.SetU(theTopLeft.GetU());
        theBottomLeft.SetV(theBottomRight.GetV());
        theNormal->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
        theTopLeft.SetU(theButton->mPressedX / theSize);
        theTopLeft.SetV(theButton->mPressedY / theSize);
        theBottomRight.SetU((theButton->mPressedX + kSEUButtonWidth) / theSize);
        theBottomRight.SetV((theButton->mPressedY + 25.0f) / theSize);
        theTopRight.SetU(theBottomRight.GetU());
        theTopRight.SetV(theTopLeft.GetV());
        theBottomLeft.SetU(theTopLeft.GetU());
        theBottomLeft.SetV(theBottomRight.GetV());
        thePressed->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
        theNormal->SetXYScale(theXScale, theYScale);
        thePressed->SetXYScale(theXScale, theYScale);
        this->mUnitTypeBitmaps[theIndex * 2] = theNormal;
        this->mUnitTypeBitmaps[theIndex * 2 + 1] = thePressed;
        this->mUnitTypeButtons[theIndex] = new EEUIFSEBUnitType(theIndex + 0x406, theNormal, NULL, thePressed, thePosition,
                                                          0x23, 0x19, inForm, ioIndex, 0, theIndex == 0);
        this->mUnitTypeButtons[theIndex]->SetTriggerEvent(kControlTriggerButtonUp, this->mSortEvent);
        this->mUnitTypeButtons[theIndex]->SetStringTableID(theButton->mHelpString);
        inForm->AddControl(this->mUnitTypeButtons[theIndex], 6);
        ioIndex++;
        theX += 0x23;
    }

    thePosition.SetXY(0.0f, 125.0f);
    std::vector<UWideString> theTemp3;
    this->mUnitList = new EEUIFSELBUnitList(0x3f1, theTemp3, gU531930, false, thePosition, 0xd4, 0x1e1,
                                      inForm, ioIndex, 3);
    this->mUnitList->SetBgOpacity(0.4f);
    this->mUnitList->SetScrollBarBgOpacity(0.4f);
    theGEText = this->mUnitList->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mUnitList->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mUnitList, 6);
    ioIndex++;

    gResource->GetString(IDS_ALPHA, theText);
    thePosition.SetX(145.0f);
    thePosition.SetY(this->mSortLabel->GetRectangle().GetBottom() + 2.0f);
    this->mAlphaSortButton = new UIRadioButton(0x3ee, theText, inFont, GERectangle(thePosition, 67.0f, (float)inFont->GetHeight()),
                                      inForm, ioIndex, 0, false, true, false);
    this->mAlphaSortButton->SetBgOpacity(0.4f);
    theGEText = this->mAlphaSortButton->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAlphaSortButton, 6);
    ioIndex++;
    this->mAlphaSortButton->SetTriggerEvent(kControlTriggerChecked, this->mSortEvent);

    gResource->GetString(IDS_SE_EPOCH, theText);
    thePosition.SetXY(145.0f, this->mAlphaSortButton->GetRectangle().GetBottom() + 2.0f);
    this->mEpochSortButton = new UIRadioButton(0x3ef, theText, inFont, GERectangle(thePosition, 67.0f, (float)inFont->GetHeight()),
                                      inForm, ioIndex, 0, false, false, false);
    this->mEpochSortButton->SetBgOpacity(0.4f);
    theGEText = this->mEpochSortButton->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mEpochSortButton, 6);
    ioIndex++;
    this->mEpochSortButton->SetTriggerEvent(kControlTriggerChecked, this->mSortEvent);

    gResource->GetString(IDS_FAMILY, theText);
    thePosition.SetX(145.0f);
    thePosition.SetY(this->mEpochSortButton->GetRectangle().GetBottom() + 2.0f);
    this->mFamilySortButton = new UIRadioButton(0x3f0, theText, inFont, GERectangle(thePosition, 67.0f, (float)inFont->GetHeight()),
                                      inForm, ioIndex, 0, false, false, false);
    this->mFamilySortButton->SetBgOpacity(0.4f);
    theGEText = this->mFamilySortButton->GetTextLabel()->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mFamilySortButton, 6);
    ioIndex++;
    this->mFamilySortButton->SetTriggerEvent(kControlTriggerChecked, this->mSortEvent);

    thePosition.SetXY(260.0f, 633.0f);
    std::vector<UWideString> theTemp4;
    this->mCargoList = new EEUIFSELBCargoList(0x3f2, theTemp4, inFont, false, thePosition, 0xe1, 99, inForm,
                                        ioIndex, 3);
    this->mCargoList->SetBgOpacity(0.4f);
    theGEText = this->mCargoList->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mCargoList->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 1.0f));
    inForm->AddControl(this->mCargoList, 6);
    ioIndex++;

    gResource->GetString(IDS_SE_REMOVE, theText);
    thePosition.SetXY(this->mCargoList->GetRectangle().GetRight() + 2.0f, 707.0f);
    this->mRemoveCargo = new EEUIFSEBRemoveCargo(0x3f4, NULL, NULL, NULL, thePosition, 100, 0x19, inForm, ioIndex, 2);
    this->mRemoveCargo->SetText(theText, inFont);
    this->mRemoveCargo->SetBgOpacity(1.0f);
    theGEText = this->mRemoveCargo->GetGEText();
    theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mRemoveCargo, 6);
    ioIndex++;

    thePosition.SetXY(500.0f, 633.0f);
    this->mRotation = new EEUIFSEBRotation(0x3f5, NULL, NULL, NULL, thePosition, 0x32, 0x32, inForm, ioIndex, 0);
    this->mRotation->SetStringTableID(IDS_ROTATE);
    inForm->AddControl(this->mRotation, 6);
    ioIndex++;

    this->mMouseClick = new EESEAMouseClick;
    this->mLasso = new EESEALasso;
    this->mPlaceUnit = new EESEARust(kSEAPlaceUnit);
    this->mPlaceGate = new EESEARust(kSEAPlaceGate);
    this->mPlaceWall = new EESEARust(kSEAPlaceWall);
    this->mMoveUnit = new EESEARust(kSEAMoveUnit);
    this->mSelectUnits = new EEUICSelectUnits(inForm, 0x3f6, ioIndex, this->mMouseClick, this->mLasso);
    inForm->AddControl(this->mSelectUnits, 0);
    ioIndex++;
    this->mCommandUnits = new EESEUICCommandUnits(inForm, 0x3f7, ioIndex, this->mMouseClick);
    inForm->AddControl(this->mCommandUnits, 0);
    ioIndex++;

    theDescription.mCreateMIPMaps = false;
    theTexture = gTextureManager->LoadTexture("textures\\scenunit", theDescription);
    theTexture->SetFrame(0);
    theTexture->SetStageAlphaMode(kStageOpTexture, -1);
    theTexture->SetStageColorMode(kStageOpTexture, -1);
    theTexture->SetStageMinFilterMode(0, -1);
    theTexture->SetStageMagFilterMode(0, -1);
    theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
    theTexture->SetStageMipMapBias(-1.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    this->mBackground = new GEBitmap;
    this->mBackground->SetTexture(theTexture, gScreenIndex);

    this->mUnitUnderMouse = -1;
    this->mPlacingWall = false;
    this->mPlacing = false;
    this->mPlacingGate = false;
    this->mIsGateHeld = false;
    this->mLayer = 2;
    this->mSortMode = 3;
    this->mUnitTypeMode = 6;
    this->ShowCargo(false);
    unsigned long theID = 0x41a;
    this->mUpgradePanel = new EEUIFSEUpgradePanel(theID, ioIndex, inForm);
}

// FUNCTION: EXE 0x59b9b4
// Creates the action that places the chosen unit (EESEAPlaceUnit ctor, inline; an EESEARust now)

// SYNTHETIC: EXE 0x59b9e4
// (EESEAPlaceUnit's scalar deleting destructor)

// FUNCTION: EXE 0x59ba16
// Creates the action that places a gate (EESEAPlaceGate ctor, inline; an EESEARust now)

// SYNTHETIC: EXE 0x59ba46
// (EESEAPlaceGate's scalar deleting destructor)

// FUNCTION: EXE 0x59ba78
// Creates the action that places a wall (EESEAPlaceWall ctor, inline; an EESEARust now)

// SYNTHETIC: EXE 0x59baa8
// (EESEAPlaceWall's scalar deleting destructor)

// FUNCTION: EXE 0x59bada
// Rebuilds the unit tab's class list after an owner, palette or sort button changed (EEUIFSEESortUnitList;
// ee_screens editor_events.rs)

// FUNCTION: EXE 0x59baf1
// Destroys and frees the sort event (EEUIFSEESortUnitList's scalar deleting destructor, with its vtable)

// SYNTHETIC: EXE 0x59bb1c
// (EESEUnit's scalar deleting destructor)

// FUNCTION: EXE 0x59bb38
// Destroys the unit tab: deletes its controls, releases its actions, bitmaps and sort event
EESEUnit::~EESEUnit()
{
    long i;
    delete this->mWorldButton;
    delete this->mSeaButton;
    delete this->mSortLabel;
    delete this->mTitleLabel;
    for (i = 0; i < 6; i++)
        delete this->mUnitTypeButtons[i];
    delete this->mAlphaSortButton;
    delete this->mEpochSortButton;
    delete this->mFamilySortButton;
    delete this->mUnitList;
    delete this->mCargoList;
    delete this->mRemoveCargo;
    delete this->mRotation;
    delete this->mSelectUnits;
    delete this->mCommandUnits;
    this->mMouseClick->Dereference();
    this->mLasso->Dereference();
    this->mPlaceUnit->Dereference();
    this->mPlaceGate->Dereference();
    this->mPlaceWall->Dereference();
    this->mMoveUnit->Dereference();
    for (i = 0; i < 12; i++)
        this->mUnitTypeBitmaps[i]->Dereference();
    this->mBackground->Dereference();
    this->mSortEvent->Dereference();
    delete this->mUpgradePanel;
}

// FUNCTION: EXE 0x59bcd5
// Places the wall from the build tool's start to its end for the tab's owner (0 when World is checked)
void EESEUnit::PlaceWall()
{
    GE2DPoint theStart(this->mBuildWall.mStart);
    GE2DPoint theEnd(this->mBuildWall.mEnd);
    // GetCurrentPlayer() spelled out (VC6 refuses the inline here; the original expands it)
    long thePlayer = 0;
    if (!this->mWorldButton->GetIsChecked()) {
        unsigned long theSelection;
        this->mPlayerComboBox->GetCurrentSelection(theSelection);
        thePlayer = theSelection + 1;
    }
    ee_screens_sew_wall(this->mPlaceWall->mAction, theStart.GetX(), theStart.GetY(), theEnd.GetX(), theEnd.GetY(),
                        thePlayer, this->mBuildWall.mTechnology);
}

// FUNCTION: EXE 0x59bd2d
// Ends wall placement and clears the ghosts
void EESEUnit::EndWall()
{
    gWorld.ClearGhostUnits();
    this->mPlacingWall = false;
    this->mPlacing = false;
}

// FUNCTION: EXE 0x59bd4a
// Previews placement each frame: the unit ghost at the mouse, the wall ghosts up to the mouse, or a gate on
// the wall under it (the early-out tests the move action pointer, never NULL, so it always runs)
void EESEUnit::Update()
{
    if (!this->mPlacing && this->mMoveUnit == NULL)
        return;
    if (!this->mPlacingWall && !this->mPlacingGate)
        EEPreviewPlacement(gMouse->mPosition);
    else if (this->mPlacingWall) {
        std::vector<EETileCoordinate> theTiles;
        std::vector<GE2DPoint> thePoints;
        EETile* theUnit;
        long theX;
        long theY;
        GE2DPoint theLocation;
        EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
        theLocation.SetXY((float)theX, (float)theY);
        if (this->mBuildWall.mState == 0) {
            theLocation.SetX(theLocation.GetX() + 0.5f);
            theLocation.SetY(theLocation.GetY() + 0.5f);
            theTiles.clear();
            theTiles.push_back(EETileCoordinate((long)theLocation.GetX(), (long)theLocation.GetY()));
            gWorld.mGhostUnitsLock->Enter();
            EEPreviewWall(theTiles, this->mBuildWall.mTechnology, this->mBuildWall.mPlayer);
            gWorld.mGhostUnitsLock->Leave();
        }
        if (this->mBuildWall.mState == 1) {
            theLocation.SetX(theLocation.GetX() + 0.5f);
            theLocation.SetY(theLocation.GetY() + 0.5f);
            theTiles.clear();
            this->mBuildWall.mStart.InterpolateTo(theLocation, 1.0f, thePoints);
            for (unsigned long i = 0; i < thePoints.size(); i++) {
                GE2DPoint& thePoint = thePoints[i];
                theTiles.push_back(EETileCoordinate((long)thePoint.GetX(), (long)thePoint.GetY()));

            }
            gWorld.mGhostUnitsLock->Enter();
            EEPreviewWall(theTiles, this->mBuildWall.mTechnology, this->mBuildWall.mPlayer);
            gWorld.mGhostUnitsLock->Leave();
        }
    } else if (this->mPlacingGate) {
        EETile* theUnit;
        long theX;
        long theY;
        EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
        long theDirection = this->mBuildGate.mDirection;
        EETileCoordinate theTile(theX, theY);
        if (EEFindGateWall(theTile, theDirection)) {
            EEGhostUnit::CreateGate(0.0f, this->mBuildGate.mUnitClass, gWorld.GetPlayer(this->mBuildGate.mPlayer));
            EEGhostUnit::PreviewGate(gMouse->mPosition, theDirection);
            this->mBuildGate.mDirection = theDirection;
        } else {
            gWorld.mGhostUnitsLock->Enter();
            gWorld.ClearGhostUnits();
            gWorld.mGhostUnitsLock->Leave();
        }
    }
}

// FUNCTION: EXE 0x59bfdc
// Places the gate on the build tool's wall tile and direction for the tab's owner
void EESEUnit::PlaceGate()
{
    // GetCurrentPlayer() spelled out (VC6 refuses the inline here; the original expands it)
    long thePlayer = 0;
    if (!this->mWorldButton->GetIsChecked()) {
        unsigned long theSelection;
        this->mPlayerComboBox->GetCurrentSelection(theSelection);
        thePlayer = theSelection + 1;
    }
    ee_screens_sew_gate(this->mPlaceGate->mAction, this->mBuildGate.mTileX, this->mBuildGate.mTileY, thePlayer,
                        this->mBuildGate.mTechnology, this->mBuildGate.mDirection);
}

// FUNCTION: EXE 0x59c026
// Ends gate placement, clears the ghosts and restores the colour of the player's walls
void EESEUnit::EndGate()
{
    gWorld.ClearGhostUnits();
    this->mIsGateHeld = this->mPlacing = this->mPlacingGate = false;
    EEClearGateHighlights(gWorld.GetPlayer(this->mBuildGate.mPlayer), this->mBuildGate.mWallType);
}

// FUNCTION: EXE 0x59c05d
// Picks the selected unit class again while a placement is in progress
void EESEUnit::ReselectUnitType()
{
    if (this->mPlacing)
        this->SelectUnitType(this->mRotation->GetForm()->GetMouseLocation());
}

// FUNCTION: EXE 0x59c07b
// Leaves the unit tab: restores the full game view, hides the player list, clears the selection and
// cancels a move, the placement and its ghosts
void EESEUnit::Leave()
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mPlayerComboBox->GetForm());
    EESetGameViewRectangle(GERectangle(0.0f, 0.0f, 1024.0f, 606.0f));
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (gMiniMap != NULL)
        gMiniMap->RefreshView();
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
    this->mPlayerComboBox->SetDisplayMode(kDisplayModeHidden);
    theEditor->GetManager()->RedrawAll();
    theEditor->ClearSelection();
    this->mUnitUnderMouse = -1;
    if (ee_screens_sew_moving(this->mMoveUnit->mAction))
        ee_screens_sew_cancel(this->mMoveUnit->mAction);
    this->mPlacing = false;
    gWorld.ClearGhostUnits();
}

// FUNCTION: EXE 0x59c141
// Shows or hides the cargo title, list and remove button (only when that changes their state)
void EESEUnit::ShowCargo(bool inShow)
{
    if (inShow) {
        if (this->mCargoList->GetDisplayMode() != kDisplayModeHidden)
            return;
        this->mCargoList->SetDisplayMode(kDisplayModeShown);
        this->mTitleLabel->SetDisplayMode(kDisplayModeShown);
        this->mRemoveCargo->SetDisplayMode(kDisplayModeShown);
    } else {
        if (this->mCargoList->GetDisplayMode() != kDisplayModeShown)
            return;
        this->mCargoList->SetDisplayMode(kDisplayModeHidden);
        this->mTitleLabel->SetDisplayMode(kDisplayModeHidden);
        this->mRemoveCargo->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x59c1a1
// Shows a unit in the upgrade panel
void EESEUnit::ShowUnitStatus(EEUnit* inUnit)
{
    this->mUpgradePanel->ShowUnit(inUnit);
}

// FUNCTION: EXE 0x59c1b3
// Draws the tab's panel background where the panel overlaps the given rectangle
void EESEUnit::Draw(const GERectangle& inRectangle)
{
    GERectangle theRectangle;
    if (this->mPanelRectangle.And(inRectangle, theRectangle))
        this->mCargoList->GetForm()->DrawBackground(this->mBackground, theRectangle, GERectangle(0.0f, 0.0f, 255.0f, 255.0f),
                                         this->mPanelRectangle);
}

// FUNCTION: EXE 0x59c221
// Returns the owner choice: 0 World (gaia), 1 Players, 2 when neither radio button is checked
long EESEUnit::GetLayer()
{
    long theLayer = 0;
    if (!this->mWorldButton->GetIsChecked())
        theLayer = !this->mSeaButton->GetIsChecked() + 1;
    return theLayer;
}

// FUNCTION: EXE 0x59c246
// Returns the rotation button's placement yaw
float EESEUnit::GetRotationAngle()
{
    return this->mRotation->GetAngle();
}

// FUNCTION: EXE 0x59c251
// Returns the checked sort button: 0 name, 1 epoch, 2 family, 3 none
long EESEUnit::GetSortMode()
{
    long theMode = 0;
    if (!this->mAlphaSortButton->GetIsChecked()) {
        if (this->mEpochSortButton->GetIsChecked())
            theMode = 1;
        else
            theMode = this->mFamilySortButton->GetIsChecked() ? 2 : 3;
    }
    return theMode;
}

// FUNCTION: EXE 0x59c287
// Returns the unit palette the type buttons last selected
long EESEUnit::GetUnitTypeMode()
{
    return sUnitTypeMode;
}

// FUNCTION: EXE 0x59c28d
// Enters the unit tab: narrows the game view to the right of the panel, shows the player list when
// Players is checked and rebuilds the class list
void EESEUnit::Enter()
{
    GE2DPoint theLocation(0.0f, this->mSeaButton->GetRectangle().GetBottom() + 2.0f);
    GERectangle theRectangle(225.0f, 0.0f, 1024.0f, 606.0f);
    EESetGameViewRectangle(theRectangle);
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (gMiniMap != NULL)
        gMiniMap->RefreshView();
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
    this->mPlayerComboBox->SetLocation(theLocation);
    if (this->mSeaButton->GetIsChecked())
        this->mPlayerComboBox->SetDisplayMode(kDisplayModeShown);
    this->mPanelRectangle.SetLeft(0.0f);
    this->mPanelRectangle.SetTop(0.0f);
    this->mPanelRectangle.SetRight(224.0f);
    this->mPanelRectangle.SetBottom(605.0f);
    this->mPlayerComboBox->GetForm()->GetManager()->AddDirtyRectangle(this->mPanelRectangle, this->mPlayerComboBox->GetForm());
    this->RefreshUnitList();
}

// FUNCTION: EXE 0x59c37b
// Starts placing the selected unit class for the tab's owner if gaia/players may place it (walls and gates
// use the build tools), then posts the class and player to the editor's load thread
void EESEUnit::SelectUnitType(const GE2DPoint& inLocation)
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mTitleLabel->GetForm());
    unsigned long theSelection;
    if (this->mUnitList->GetCurrentSelection(theSelection) != kErrorOK)
        return;
    if (this->mPlacingGate)
        this->mBuildGate.Cancel();
    if (this->mPlacingWall)
        this->mBuildWall.Cancel();
    gWorld.ClearGhostUnits();
    EEPlayer* thePlayer = gWorld.GetPlayer(this->GetCurrentPlayer());
    EEUnitClass* theTypeClass = gUnitClassTable[this->mUnitList->mUnitTypes[theSelection]];
    long theTech;
    if (theTypeClass != NULL)
        theTech = theTypeClass->mTechnology;
    else
        theTech = 100000;
    EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(theTech);
    EEUnitClass* theClass = gUnitClassTable[this->mUnitList->mUnitTypes[theSelection]];
    if ((thePlayer->GetPlayerID() != 0 || !theClass->mIsGaiaPlaceable) && (!(thePlayer->GetPlayerID() > 0) || !theClass->mIsPlayerPlaceable))
        return;
    this->mPlacingWall = false;
    this->mPlacingGate = false;
    this->mPlacing = true;
    if (theNode != NULL && theNode->mData->mKind == 6) {
        this->mPlacingWall = true;
        this->mPlacing = true;
        this->mBuildWall.mTechnology = theNode->mData->mID;
        this->mBuildWall.Initialize(thePlayer->GetPlayerID());
    } else if (theClass != NULL && theClass->mCategory == kUnitCategoryGate) {
        this->mIsGateHeld = false;
        this->mPlacingGate = true;
        this->mPlacing = true;
        this->mBuildGate.mTechnology = theNode->mData->mID;
        this->mBuildGate.mWallType = theNode->GetUnitClass(0);
        this->mBuildGate.mUnitClass = theClass;
        this->mBuildGate.Initialize(thePlayer->GetPlayerID());
    }
    theEditor->mLoadThread->Post402(reinterpret_cast<void*>(this->mUnitList->mUnitTypes[theSelection]), reinterpret_cast<void*>(thePlayer->GetPlayerID()));
}

// FUNCTION: EXE 0x59c535
// Refills the cargo list from the selected units' cargo, keeping the selected line where possible
void EESEUnit::UpdateSelection()
{
    unsigned long theSelection;
    if (this->mCargoList->GetCurrentSelection(theSelection) != kErrorOK)
        theSelection = 0;
    this->mCargoList->mCargo.clear();
    this->mCargoList->DeleteAllEntries();
    unsigned long theCount = static_cast<EEUIFScenarioEditor*>(this->mCargoList->GetForm())->GetSelectedUnitCount();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = static_cast<EEUIFScenarioEditor*>(this->mCargoList->GetForm())->GetSelectedUnit(i)->mComplexUnit;
        if (theUnit != NULL && theUnit->GetNumCargoUnits() > 0) {
            for (unsigned long j = 0; j < theUnit->GetNumCargoUnits(); j++)
                this->mCargoList->AddCargo(theUnit->mCargo[j]);
        }
    }
    if (!(this->mCargoList->GetLineCount() > 0))
        return;
    if (theSelection < this->mCargoList->GetLineCount())
        this->mCargoList->ChangeSelectionIndex(theSelection, true, false);
    else
        this->mCargoList->ChangeSelectionIndex(this->mCargoList->GetLineCount() - 1, true, false);
}

// FUNCTION: EXE 0x59c64b
// When the palette, owner or sort changed: cancels any placement and relists the scenario-selectable
// unit classes that this owner may place and that belong to the palette
void EESEUnit::RefreshUnitList()
{
    UWideString theText;
    long theMode = this->GetUnitTypeMode();
    long theSort = this->GetSortMode();
    long theLayer = this->GetLayer();
    if (theMode == this->mUnitTypeMode && theSort == this->mSortMode && theLayer == this->mLayer)
        return;
    if (this->mPlacingGate)
        this->mBuildGate.Cancel();
    if (this->mPlacingWall)
        this->mBuildWall.Cancel();
    gWorld.mGhostUnitsLock->Enter();
    gWorld.ClearGhostUnits();
    gWorld.mGhostUnitsLock->Leave();
    if (theLayer == 0)
        this->mPlayerComboBox->SetDisplayMode(kDisplayModeHidden);
    else
        this->mPlayerComboBox->SetDisplayMode(kDisplayModeShown);
    this->mUnitList->SetSortMode(theSort);
    this->mUnitList->mUnitTypes.clear();
    this->mUnitList->DeleteAllEntries();
    for (long i = 0; i < EEUnitClassCount(); i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass == NULL || !theClass->mIsScenarioSelectable ||
            ((theLayer != 0 || !theClass->mIsGaiaPlaceable) && (theLayer != 1 || !theClass->mIsPlayerPlaceable)))
            continue;
        bool theShow;
        if (theClass->mAIKind == kAIKindHero || theClass->mCategory == kUnitCategoryLandMilitaryHero)
            theShow = theMode == kUnitPaletteHeroes;
        else if (theClass->mDomains & kMovementAir)
            theShow = theMode == kUnitPaletteAirUnits;
        else if (theClass->IsLandBuilding())
            theShow = theMode == kUnitPaletteStructures;
        else if (theClass->mDomains & kMovementLand)
            theShow = theMode == kUnitPaletteLandUnits;
        else if (theClass->mDomains & kMovementSea)
            theShow = theMode == kUnitPaletteNavalUnits;
        else if (theClass->mDomains & kMovementSpace)
            theShow = theMode == kUnitPaletteAirUnits;
        else
            theShow = theMode == kUnitPaletteResources;
        if (theShow)
            this->mUnitList->AddUnitType(theClass->mUnitType);
    }
    if (this->mUnitList->GetLineCount() != 0) {
        this->mUnitList->SetScrollBarCurrentRange(0);
        this->mUnitList->ChangeSelectionIndex(0, true, false);
    }
    this->mLayer = theLayer;
    this->mSortMode = theSort;
    this->mUnitTypeMode = theMode;
}

// FUNCTION: EXE 0x59c84b
// Handles the mouse on the map: selects, commands or picks up units (a double-clicked gate is held for
// re-placing); while placing, previews and places units, walls and gates (right button cancels)
void EESEUnit::MouseMove(const GE2DPoint& inLocation)
{
    if (!this->mPlacing && !ee_screens_sew_moving(this->mMoveUnit->mAction)) {
        if (!gViewport.GetViewRectangle().Contains(inLocation))
            return;
        if (UIManager::GetLMouseButton()->JustDoublePressed()) {
            EEUnit* theUnit = gUnitPicker.PickUnit(inLocation, 0);
            if (theUnit == NULL)
                return;
            if (theUnit->mUnitClass->mCategory != kUnitCategoryGate)
                ee_screens_sew_pick_up(this->mMoveUnit->mAction, theUnit->mUnitID);
            else {
                this->mPlacingGate = true;
                this->mPlacing = true;
                this->mIsGateHeld = true;
                this->mBuildGate.mTechnology = theUnit->mUnitClass->mTechnology;
                this->mBuildGate.mWallType = theUnit->GetTechTreeNode()->GetUnitClass(0);
                this->mBuildGate.mUnitClass = theUnit->mUnitClass;
                theUnit->Kill(0, 0, 0);
                this->mBuildGate.Initialize(theUnit->mPlayer->mPlayerID);
            }
            theUnit->Dereference();
            return;
        }
        if (UIManager::GetLMouseButton()->JustPressed()) {
            this->mSelectUnits->StartSelection(inLocation);
            return;
        }
        if (UIManager::GetRMouseButton()->JustPressed())
            this->mCommandUnits->StartCommand(inLocation);
        return;
    }
    if (!this->mPlacingWall && !this->mPlacingGate) {
        gWorld.mGhostUnitsLock->Enter();
        EEUnit* theUnit = gUnitPicker.PickUnit(inLocation, 0);
        if (theUnit != NULL) {
            this->mUnitUnderMouse = theUnit->mUnitID;
            theUnit->Dereference();
        } else
            this->mUnitUnderMouse = -1;
        if (gViewport.GetViewRectangle().Contains(inLocation)) {
            if (UIManager::GetLMouseButton()->JustPressed()) {
                // GetCurrentPlayer() spelled out as if/else (the inline is refused here; the original expands it: xor eax,eax arm first)
                long thePlayer;
                if (this->mWorldButton->GetIsChecked())
                    thePlayer = 0;
                else {
                    unsigned long theSelection;
                    this->mPlayerComboBox->GetCurrentSelection(theSelection);
                    thePlayer = theSelection + 1;
                }
                if (this->mPlacing)
                    ee_screens_sew_place_unit(this->mPlaceUnit->mAction, thePlayer);
                else {
                    ee_screens_sew_put_down(this->mMoveUnit->mAction);
                    this->mUnitUnderMouse = -1;
                }
            } else if (UIManager::GetRMouseButton()->JustUnpressed()) {
                gWorld.ClearGhostUnits();
                this->mUnitUnderMouse = -1;
                this->mPlacing = false;
                ee_screens_sew_cancel(this->mMoveUnit->mAction);
            }
        }
        gWorld.mGhostUnitsLock->Leave();
    } else if (this->mPlacingWall)
        this->mBuildWall.Update(inLocation);
    else if (this->mPlacingGate) {
        if (!this->mIsGateHeld)
            this->mBuildGate.Update(inLocation);
        else if (!UIManager::GetLMouseButton()->JustUnpressed() && UIManager::GetLMouseButton()->GetValue() == 0
                 && !UIManager::GetLMouseButton()->JustPressed())
            this->mIsGateHeld = false;
    }
}

// FUNCTION: EXE 0x59cada
// On Delete, deletes the selected units (Shift sets the action's group flag); then handles the rotation keys
void EESEUnit::UpdateKeys()
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mRotation->GetForm());
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    ISEntity* theKey;
    if (theEditor->GetSelectedUnitCount() > 0) {
        gISKeyboard->GetEntity(kKeyDelete, theKey);
        if (theKey->JustPressed()) {
            gISKeyboard->GetEntity(kKeyShift, theKey);
            EESEARust* theAction = new EESEARust(kSEADeleteUnit, NULL, -1, thePlayer->GetPlayerID(), theKey->GetValue() != 0);
            theAction->Dereference();
        }
    }
    this->mRotation->UpdateKeyboard();
}

// FUNCTION: EXE 0x59cb8b
// Allocates storage for a list of placed units (vector<EESEAPlacedUnit>::_Buy)

// FUNCTION: EXE 0x59cbc5
// Creates an idle wall build tool: start and end at (-99, -99), no technology (100000)
// (EEUIBuildWall ctor, inline in ee/EEUIBuildWall.h; emitted here, its first user)

// SYNTHETIC: EXE 0x59cbff
// (EESEUIBuildWall's scalar deleting destructor, ICF-shared with EEUIBuildWall's)

// SYNTHETIC: EXE 0x59cc1c
// (EESEUIBuildGate's scalar deleting destructor, ICF-shared with EEUIBuildGate's)
