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
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEARust.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
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
#include "rust/ee_screens.h"


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
    void AddUnitType(long inUnitType);
    void SetSortMode(long inMode);

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
    void RemoveSelected();

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
    virtual ~EEUIFSEBRotation();
    virtual long UpdateKeyboard();          // +0x7c 0x19a728
    virtual void ButtonUp();                // +0x8c 0x19a515
    virtual long MouseLButtonDown();        // +0x34 0x19a5ba
    virtual long MouseRButtonDown();        // +0x44 0x19a5ca
    // +0x48 (ICF-folded into EEUIBUpgrade::MouseLButtonUp 0x256949)
    virtual long MouseRButtonUp() { return UIButton::MouseLButtonUp(); }
    float GetAngle();
    void UpdateBitmaps();
    void RotateGhosts();
    void RotateSelection();

    GEBitmap* mBitmaps[4];                  // +0x104 (straight, diagonal) x (normal, pressed), flat
    long mRotation;                         // +0x114 0..7
    long mDirection;                        // +0x118 +1/-1 while a button is down
};

// The remove cargo button (RTTI EEUIFSEBRemoveCargo > UIButton; vtable 0x444de8; id 0x3f4)
class EEUIFSEBRemoveCargo : public UIButton {
public:
    EEUIFSEBRemoveCargo(unsigned long inID, GEBitmap* inNormal, GEBitmap* inFocused, GEBitmap* inPressed,
                        const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
                        unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUIFSEBRemoveCargo() {}
    virtual void ButtonUp();                // +0x8c 0x19a880
};

// The sort radio buttons' event (RTTI EEUIFSEESortUnitList > UIEvent; vtable 0x444e88; new 0xc): an EESEERust
// (ee_screens editor_events.rs)

// the unit type mode the six buttons select (EEUIFSEBUnitType::Check; 0x531ff0) (provisional name)
static long sUnitTypeMode;

extern GEFont* gSeplScreenFont;   // the screen's font while the tab makes its controls (EESERBUnitTypes.cpp)

static bool EESEUnitNameLess(long inLeft, long inRight);
static bool EESEUnitEpochLess(long inLeft, long inRight);
static bool EESEUnitLayerLess(long inLeft, long inRight);

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

// Picks the clicked unit class for placement on the map, then fires the list's button-up event
void EEUIFSELBUnitList::ButtonUp()
{
    EESEUnit* theScreen = (static_cast<EEUIFScenarioEditor*>(this->GetForm()))->GetUnitScreen();
    theScreen->SelectUnitType(this->GetForm()->GetMouseLocation());
    this->FireTriggerEvent(kControlTriggerButtonUp);
}

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

// Orders two unit classes by epoch, then by name
static bool EESEUnitEpochLess(long inLeft, long inRight)
{
    if (gUnitClassTable[inLeft]->mEpoch > gUnitClassTable[inRight]->mEpoch)
        return false;
    if (gUnitClassTable[inLeft]->mEpoch < gUnitClassTable[inRight]->mEpoch)
        return true;
    return EESEUnitNameLess(inLeft, inRight);
}

// Orders two unit classes by their displayed names
static bool EESEUnitNameLess(long inLeft, long inRight)
{
    UWideString theLeft;
    UWideString theRight;
    gResource->GetString(gUnitClassTable[inLeft]->mNameStringID, theLeft);
    gResource->GetString(gUnitClassTable[inRight]->mNameStringID, theRight);
    return theLeft < theRight ? true : false;
}

// Orders two unit classes by family, then by name
static bool EESEUnitLayerLess(long inLeft, long inRight)
{
    if (gUnitClassTable[inLeft]->mFamily > gUnitClassTable[inRight]->mFamily)
        return false;
    if (gUnitClassTable[inLeft]->mFamily < gUnitClassTable[inRight]->mFamily)
        return true;
    return EESEUnitNameLess(inLeft, inRight);
}

// Handles the list's keys; Enter picks the selected unit class for placement
long EEUIFSELBUnitList::UpdateKeyboard()
{
    long theResult = UILBString::UpdateKeyboard();
    if (UIControl::sEnterKey->GetValue()) {
        EESEUnit* theScreen = (static_cast<EEUIFScenarioEditor*>(this->GetForm()))->GetUnitScreen();
        theScreen->SelectUnitType(this->GetForm()->GetMouseLocation());
    }
    return theResult;
}

// Creates the unit tab's (empty) cargo list
EEUIFSELBCargoList::EEUIFSELBCargoList(unsigned long inID, std::vector<UWideString>& inStrings, GEFont* inFont,
                                       bool inAutoSelect, const GE2DPoint& inLocation, unsigned long inWidth,
                                       unsigned long inHeight, UIForm* inForm, unsigned long inIndex,
                                       unsigned long inBorderThickness)
    : UILBString(inID, inStrings, inFont, inAutoSelect, GERectangle(inLocation, (float)inWidth, (float)inHeight),
                 inForm, inIndex, inBorderThickness)
{
}

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

// Deletes the unit on the selected cargo line from the map (posts a delete-unit action)
void EEUIFSELBCargoList::RemoveSelected()
{
    unsigned long theSelection;
    if (this->GetLineCount() > 0 && this->GetCurrentSelection(theSelection) == kErrorOK) {
        EESEARust* theAction = new EESEARust(kSEADeleteUnit, NULL, this->mCargo[theSelection]->GetUnitID());
        theAction->Dereference();
    }
}

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
    this->SetBitmapDrawOffset(GE2DPoint(0.0f, 0.0f));
}

// Sets the unit palette (land, air, naval, structures, heroes or resources) from the button's id, then checks it
void EEUIFSEBUnitType::Check()
{
    switch (this->GetId()) {
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
    this->SetAlwaysDirty(true);
}

// Destroys the rotation button, releasing its four arrow bitmaps
EEUIFSEBRotation::~EEUIFSEBRotation()
{
    this->mBitmaps[0]->Dereference();
    this->mBitmaps[1]->Dereference();
    this->mBitmaps[2]->Dereference();
    this->mBitmaps[3]->Dereference();
}

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

// Presses the button so its release turns one step forward
long EEUIFSEBRotation::MouseLButtonDown()
{
    this->mDirection = 1;
    return UIButton::MouseLButtonDown();
}

// Presses the button (as a left press) so its release turns one step back
long EEUIFSEBRotation::MouseRButtonDown()
{
    this->mDirection = -1;
    return UIButton::MouseLButtonDown();
}

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

// Shows the straight (even step) or diagonal (odd step) arrow bitmaps
void EEUIFSEBRotation::UpdateBitmaps()
{
    unsigned long theKind = (this->mRotation & 1) * 2;
    this->SetNormalBitmap(this->mBitmaps[theKind]);
    this->SetPressedBitmap(this->mBitmaps[theKind + 1]);
}

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

// Turns every selected unit whose class can turn to the button's angle
void EEUIFSEBRotation::RotateSelection()
{
    GETransformation theTransformation;
    float theAngle = this->GetAngle();
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->GetForm());
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

// Deletes the unit on the cargo list's selected line, then releases the button
void EEUIFSEBRemoveCargo::ButtonUp()
{
    static_cast<EEUIFSELBCargoList*>(this->GetForm()->GetControlById(0x3f2))->RemoveSelected();
    UIButton::ButtonUp();
}

// Creates the remove cargo button
EEUIFSEBRemoveCargo::EEUIFSEBRemoveCargo(unsigned long inID, GEBitmap* inNormal, GEBitmap* inFocused,
    GEBitmap* inPressed, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight, UIForm* inForm,
    unsigned long inIndex, unsigned long inBorderThickness)
    : UIButton(inID, inNormal, inFocused, inPressed, GERectangle(inLocation, (float)inWidth, (float)inHeight),
               inForm, inIndex, inBorderThickness)
{
}

// The unit type buttons' cell width: a plain (non-folded) .rdata float in the original (0x444e7c), read by the ctor
// only (provisional name)
static const float kSEUButtonWidth = 35.0f;

// Builds the unit tab (ee_screens editor_unit.rs: the owner and sort radio buttons, the six unit type buttons, the
// unit and cargo lists, the remove and rotate buttons, the editor actions, the panel background and the upgrade
// panel); the shell keeps the rhombus, the build tools and the panel rectangle
EESEUnit::EESEUnit(unsigned long& ioIndex, EEUIFScenarioEditor* inForm, GEFont* inFont, UIComboBox* inPlayers)
    : mForm(inForm)
{
    this->mRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f));
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_seun_new(this, inForm, inPlayers, &theIndex);
    ioIndex = theIndex;
}

// Destroys the unit tab: deletes its controls, releases its actions, bitmaps and sort event (editor_unit.rs)
EESEUnit::~EESEUnit()
{
    ee_screens_seun_free(this->mRust);
}

// The members (editor_unit.rs): 0x59bcd5 PlaceWall, 0x59bd2d EndWall, 0x59bd4a Update, 0x59bfdc PlaceGate, 0x59c026
// EndGate, 0x59c05d ReselectUnitType, 0x59c07b Leave, 0x59c141 ShowCargo, 0x59c1a1 ShowUnitStatus, 0x59c1b3 Draw,
// 0x59c221 GetLayer, 0x59c246 GetRotationAngle, 0x59c251 GetSortMode, 0x59c287 GetUnitTypeMode (the host's
// EE_SCREENS_SEUN_OP_TYPE_MODE), 0x59c28d Enter, 0x59c37b SelectUnitType (inLocation unused, as the original),
// 0x59c535 UpdateSelection, 0x59c64b RefreshUnitList, 0x59c84b MouseMove, 0x59cada UpdateKeys
void EESEUnit::PlaceWall() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_PLACE_WALL, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::EndWall() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_END_WALL, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::Update() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_UPDATE, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::PlaceGate() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_PLACE_GATE, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::EndGate() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_END_GATE, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::ReselectUnitType() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_RESELECT, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::Leave() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_LEAVE, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::ShowCargo(bool inShow)
{
    ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_SHOW_CARGO, inShow, 0.0f, 0.0f, NULL);
}
void EESEUnit::ShowUnitStatus(EEUnit* inUnit)
{
    ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_SHOW_UNIT, 0, 0.0f, 0.0f, inUnit);
}
void EESEUnit::Draw(const GERectangle& inRectangle)
{
    ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_DRAW, 0, 0.0f, 0.0f, const_cast<GERectangle*>(&inRectangle));
}
long EESEUnit::GetLayer() { return ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_LAYER); }
float EESEUnit::GetRotationAngle() { return ee_screens_seun_angle(this->mRust); }
long EESEUnit::GetSortMode() { return ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_SORT); }
void EESEUnit::Enter() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_ENTER, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::SelectUnitType(const GE2DPoint& inLocation)
{
    ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_SELECT, 0, 0.0f, 0.0f, NULL);
}
void EESEUnit::UpdateSelection()
{
    ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_UPDATE_SELECTION, 0, 0.0f, 0.0f, NULL);
}
void EESEUnit::RefreshUnitList() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_REFRESH, 0, 0.0f, 0.0f, NULL); }
void EESEUnit::MouseMove(const GE2DPoint& inLocation)
{
    ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_MOUSE_MOVE, 0, inLocation.GetX(), inLocation.GetY(), NULL);
}
void EESEUnit::UpdateKeys() { ee_screens_seun_call(this->mRust, EE_SCREENS_SEUN_UPDATE_KEYS, 0, 0.0f, 0.0f, NULL); }
long EESEUnit::GetUnitUnderMouse() const { return ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_UNDER_MOUSE); }
void EESEUnit::SetUnitUnderMouse(long inUnit) { ee_screens_seun_set_under_mouse(this->mRust, (int32_t)inUnit); }
EEUICSelectUnits* EESEUnit::GetSelectUnits() const
{
    return static_cast<EEUICSelectUnits*>(ee_screens_seun_handle(this->mRust, EE_SCREENS_SEUN_GET_SELECT_UNITS));
}
long EESEUnit::GetPlacingFlags() const { return ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_FLAGS); }
void EESEUnit::GetListState(long& outMode, long& outSort, long& outLayer) const
{
    outMode = ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_LISTED_MODE);
    outSort = ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_LISTED_SORT);
    outLayer = ee_screens_seun_get(this->mRust, EE_SCREENS_SEUN_GET_LISTED_LAYER);
}

// The shared control maker's unit tab lists (EESERBUnitTypes.cpp SeplMake)
UILBString* EESEUnitNewList(long inKind, unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                            unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
{
    std::vector<UWideString> theNone;
    if (inKind == EE_SCREENS_SEPL_LIST_UNIT_TYPES)
        return new EEUIFSELBUnitList(inID, theNone, inFont, false, inLocation, inWidth, inHeight, inForm, inIndex, 3);
    return new EEUIFSELBCargoList(inID, theNone, inFont, false, inLocation, inWidth, inHeight, inForm, inIndex, 3);
}


// ---- the unit tab's host (editor_unit.rs; EE_SCREENS_SEUN_OP_*): the tab's own controls, the actions, the world,
// the mouse, the build tools and the unit classes

static_assert(EE_SCREENS_SEUN_IDS_ALPHA == IDS_ALPHA && EE_SCREENS_SEUN_IDS_ALPHA + 1 == IDS_SE_EPOCH &&
                  EE_SCREENS_SEUN_IDS_ALPHA + 2 == IDS_FAMILY && EE_SCREENS_SEUN_IDS_CARGO == IDS_CARGO &&
                  EE_SCREENS_SEUN_IDS_SORT == IDS_SORT && EE_SCREENS_SEUN_IDS_WORLD_PLAYER == IDS_WORLD_PLAYER,
              "the strings");
static_assert(EE_SCREENS_SEUN_PALETTE_LAND == kUnitPaletteLandUnits && EE_SCREENS_SEUN_PALETTE_AIR == kUnitPaletteAirUnits &&
                  EE_SCREENS_SEUN_PALETTE_NAVAL == kUnitPaletteNavalUnits &&
                  EE_SCREENS_SEUN_PALETTE_STRUCTURES == kUnitPaletteStructures &&
                  EE_SCREENS_SEUN_PALETTE_HEROES == kUnitPaletteHeroes &&
                  EE_SCREENS_SEUN_PALETTE_RESOURCES == kUnitPaletteResources,
              "the palettes");

namespace {

EESEARust* SeunAction(void* inAction)
{
    return static_cast<EESEARust*>(static_cast<EEAction*>(inAction));
}

// one unit type button with its normal and pressed bitmaps (the tab's ctor's loop body)
void SeunTypeButton(void** outMade, EEUIFScenarioEditor* inForm, UIEvent* inEvent, const int32_t* i, const float* f)
{
    GETexture* theTexture = inForm->mButtonTexture;
    float theSize = (float)theTexture->GetWidth();
    GETextureCoordinate theTopLeft, theTopRight, theBottomLeft, theBottomRight;
    float theYScale = 25.0f / theSize;
    float theXScale = kSEUButtonWidth / theSize;
    GE2DPoint thePosition((float)(unsigned long)i[3], 0.0f);
    GEBitmap* theNormal = new GEBitmap;
    GEBitmap* thePressed = new GEBitmap;
    GEBitmap* theBitmaps[2] = {theNormal, thePressed};
    theNormal->SetTexture(theTexture, inForm->GetManager()->GetScreen());
    thePressed->SetTexture(theTexture, inForm->GetManager()->GetScreen());
    for (int k = 0; k < 2; k++) {
        float theX = f[k * 2];
        float theY = f[k * 2 + 1];
        theTopLeft.SetU(theX / theSize);
        theTopLeft.SetV(theY / theSize);
        theBottomRight.SetU((theX + kSEUButtonWidth) / theSize);
        theBottomRight.SetV((theY + 25.0f) / theSize);
        theTopRight.SetU(theBottomRight.GetU());
        theTopRight.SetV(theTopLeft.GetV());
        theBottomLeft.SetU(theTopLeft.GetU());
        theBottomLeft.SetV(theBottomRight.GetV());
        theBitmaps[k]->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    }
    theNormal->SetXYScale(theXScale, theYScale);
    thePressed->SetXYScale(theXScale, theYScale);
    long theIndex = i[1];
    EEUIFSEBUnitType* theButton = new EEUIFSEBUnitType(theIndex + 0x406, theNormal, NULL, thePressed, thePosition, 0x23,
                                                       0x19, inForm, (unsigned long)i[0], 0, theIndex == 0);
    theButton->SetTriggerEvent(kControlTriggerButtonUp, inEvent);
    theButton->SetStringTableID((unsigned long)i[2]);
    inForm->AddControl(theButton, 6);
    outMade[0] = theButton;
    outMade[1] = theNormal;
    outMade[2] = thePressed;
}

// Update's wall ghosts: the wall tool's first tile, or the tiles from its start to the mouse
void SeunPreviewWall(EESEUnit* inTab)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<GE2DPoint> thePoints;
    EETile* theUnit;
    long theX;
    long theY;
    GE2DPoint theLocation;
    EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
    theLocation.SetXY((float)theX, (float)theY);
    if (inTab->mBuildWall.mState == 0) {
        theLocation.SetX(theLocation.GetX() + 0.5f);
        theLocation.SetY(theLocation.GetY() + 0.5f);
        theTiles.clear();
        theTiles.push_back(EETileCoordinate((long)theLocation.GetX(), (long)theLocation.GetY()));
        gWorld.mGhostUnitsLock->Enter();
        EEPreviewWall(theTiles, inTab->mBuildWall.mTechnology, inTab->mBuildWall.mPlayer);
        gWorld.mGhostUnitsLock->Leave();
    }
    if (inTab->mBuildWall.mState == 1) {
        theLocation.SetX(theLocation.GetX() + 0.5f);
        theLocation.SetY(theLocation.GetY() + 0.5f);
        theTiles.clear();
        inTab->mBuildWall.mStart.InterpolateTo(theLocation, 1.0f, thePoints);
        for (unsigned long i = 0; i < thePoints.size(); i++) {
            GE2DPoint& thePoint = thePoints[i];
            theTiles.push_back(EETileCoordinate((long)thePoint.GetX(), (long)thePoint.GetY()));
        }
        gWorld.mGhostUnitsLock->Enter();
        EEPreviewWall(theTiles, inTab->mBuildWall.mTechnology, inTab->mBuildWall.mPlayer);
        gWorld.mGhostUnitsLock->Leave();
    }
}

// Update's gate: a gate ghost on the wall under the mouse, else no ghost
void SeunPreviewGate(EESEUnit* inTab)
{
    EETile* theUnit;
    long theX;
    long theY;
    EEPointUnitOrTileXY(gMouse->mPosition, theUnit, theX, theY, true);
    long theDirection = inTab->mBuildGate.mDirection;
    EETileCoordinate theTile(theX, theY);
    if (EEFindGateWall(theTile, theDirection)) {
        EEGhostUnit::CreateGate(0.0f, inTab->mBuildGate.mUnitClass, gWorld.GetPlayer(inTab->mBuildGate.mPlayer));
        EEGhostUnit::PreviewGate(gMouse->mPosition, theDirection);
        inTab->mBuildGate.mDirection = theDirection;
    } else {
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
    }
}

// SelectUnitType's look-up: the player, the list line's class and its technology's node (the original's order)
struct SeunPick {
    EEPlayer* mPlayer;
    EEUnitClass* mClass;
    EETechTreeNode* mNode;
};

SeunPick SeunPickClass(EEUIFSELBUnitList* inList, long inPlayer, unsigned long inLine)
{
    SeunPick thePick;
    thePick.mPlayer = gWorld.GetPlayer(inPlayer);
    EEUnitClass* theTypeClass = gUnitClassTable[inList->mUnitTypes[inLine]];
    long theTech;
    if (theTypeClass != NULL)
        theTech = theTypeClass->mTechnology;
    else
        theTech = 100000;
    thePick.mNode = thePick.mPlayer->GetTechTree()->Get(theTech);
    thePick.mClass = gUnitClassTable[inList->mUnitTypes[inLine]];
    return thePick;
}

EEUIFScenarioEditor* SeunEditor(void* inControl)
{
    return static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(inControl)->GetForm());
}

}   // namespace

// The ops (editor_unit.rs; p: three handles, i: four integers, f: four floats)
extern "C" int32_t ee_screenscpp_seun(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESEUnit* theTab = static_cast<EESEUnit*>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    UILBString* theList = static_cast<UILBString*>(p[0]);
    EEUIFSELBUnitList* theUnitList = static_cast<EEUIFSELBUnitList*>(p[0]);
    EEUIFSELBCargoList* theCargo = static_cast<EEUIFSELBCargoList*>(p[0]);
    void** theOut = static_cast<void**>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEUN_OP_SORT_EVENT: *theOut = static_cast<UIEvent*>(new EESEERust(kSEESortUnitList)); return 0;
    case EE_SCREENS_SEUN_OP_SET_EVENT:
        static_cast<UIControl*>(p[0])->SetTriggerEvent((unsigned long)i[0], static_cast<UIEvent*>(p[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_TYPE_BUTTON:
        SeunTypeButton(theOut, static_cast<EEUIFScenarioEditor*>(theForm), static_cast<UIEvent*>(p[2]), i, f);
        return 0;
    case EE_SCREENS_SEUN_OP_REMOVE_CARGO: {
        UWideString theText;
        gResource->GetString(IDS_SE_REMOVE, theText);
        EEUIFSEBRemoveCargo* t = new EEUIFSEBRemoveCargo(0x3f4, NULL, NULL, NULL, GE2DPoint(f[0], 707.0f), 100, 0x19,
                                                         theForm, (unsigned long)i[0], 2);
        t->SetText(theText, gSeplScreenFont);
        t->SetBgOpacity(1.0f);
        GEText* theGEText = t->GetGEText();
        theGEText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
        theGEText->SetIsTransparent(true);
        theForm->AddControl(t, 6);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ROTATION: {
        EEUIFSEBRotation* t = new EEUIFSEBRotation(0x3f5, NULL, NULL, NULL, GE2DPoint(500.0f, 633.0f), 0x32, 0x32,
                                                   theForm, (unsigned long)i[0], 0);
        t->SetStringTableID(IDS_ROTATE);
        theForm->AddControl(t, 6);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ACTION: {
        EEAction* theAction = NULL;
        switch (i[0]) {
        case EE_SCREENS_SEUN_ACTION_CLICK: theAction = new EESEAMouseClick; break;
        case EE_SCREENS_SEUN_ACTION_LASSO: theAction = new EESEALasso; break;
        case EE_SCREENS_SEUN_ACTION_PLACE_UNIT: theAction = new EESEARust(kSEAPlaceUnit); break;
        case EE_SCREENS_SEUN_ACTION_PLACE_GATE: theAction = new EESEARust(kSEAPlaceGate); break;
        case EE_SCREENS_SEUN_ACTION_PLACE_WALL: theAction = new EESEARust(kSEAPlaceWall); break;
        default: theAction = new EESEARust(kSEAMoveUnit); break;
        }
        *theOut = theAction;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_SELECT_UNITS: {
        void* const* theTools = static_cast<void* const*>(p[2]);
        EEUICSelectUnits* t = new EEUICSelectUnits(
            theForm, 0x3f6, (unsigned long)i[0], static_cast<EESEAMouseClick*>(static_cast<EEAction*>(theTools[0])),
            static_cast<EESEALasso*>(static_cast<EEAction*>(theTools[1])));
        theForm->AddControl(t, 0);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_COMMAND_UNITS: {
        EESEUICCommandUnits* t = new EESEUICCommandUnits(
            theForm, 0x3f7, (unsigned long)i[0], static_cast<EESEAMouseClick*>(static_cast<EEAction*>(p[2])));
        theForm->AddControl(t, 0);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_BACKGROUND: {
        GETextureDescription theDescription = {1, 0, gTexturePixelDepth, 0, false, false, true, true};
        theDescription.mCreateMIPMaps = false;
        GETexture* theTexture = gTextureManager->LoadTexture("textures\\scenunit", theDescription);
        theTexture->SetFrame(0);
        theTexture->SetStageAlphaMode(kStageOpTexture, -1);
        theTexture->SetStageColorMode(kStageOpTexture, -1);
        theTexture->SetStageMinFilterMode(0, -1);
        theTexture->SetStageMagFilterMode(0, -1);
        theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
        theTexture->SetStageMipMapBias(-1.0f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
        GEBitmap* theBackground = new GEBitmap;
        theBackground->SetTexture(theTexture, gScreenIndex);
        *theOut = theBackground;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_UPGRADE_PANEL: {
        unsigned long theID = 0x41a;
        unsigned long theIndex = (unsigned long)i[0];
        *theOut = new EEUIFSEUpgradePanel(theID, theIndex, static_cast<EEUIFScenarioEditor*>(theForm));
        i[0] = (int32_t)theIndex;
        return 0;
    }
    case EE_SCREENS_SEUN_OP_RELEASE:
        switch (i[0]) {
        case 0: static_cast<EEAction*>(p[0])->Dereference(); break;
        case 1: static_cast<GEBitmap*>(p[0])->Dereference(); break;
        case 2: static_cast<UIEvent*>(p[0])->Dereference(); break;
        default: delete static_cast<EEUIFSEUpgradePanel*>(p[0]); break;
        }
        return 0;
    case EE_SCREENS_SEUN_OP_PLACE_WALL: {
        GE2DPoint theStart(theTab->mBuildWall.mStart);
        GE2DPoint theEnd(theTab->mBuildWall.mEnd);
        ee_screens_sew_wall(SeunAction(p[1])->mAction, theStart.GetX(), theStart.GetY(), theEnd.GetX(), theEnd.GetY(),
                            i[0], theTab->mBuildWall.mTechnology);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_PLACE_GATE:
        ee_screens_sew_gate(SeunAction(p[1])->mAction, theTab->mBuildGate.mTileX, theTab->mBuildGate.mTileY, i[0],
                            theTab->mBuildGate.mTechnology, theTab->mBuildGate.mDirection);
        return 0;
    case EE_SCREENS_SEUN_OP_CLEAR_GHOSTS:
        if (i[0])
            gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        if (i[0])
            gWorld.mGhostUnitsLock->Leave();
        return 0;
    case EE_SCREENS_SEUN_OP_GATE_HIGHLIGHTS:
        EEClearGateHighlights(gWorld.GetPlayer(theTab->mBuildGate.mPlayer), theTab->mBuildGate.mWallType);
        return 0;
    case EE_SCREENS_SEUN_OP_PREVIEW:
        if (i[0] == 0)
            EEPreviewPlacement(gMouse->mPosition);
        else if (i[0] == 1)
            SeunPreviewWall(theTab);
        else
            SeunPreviewGate(theTab);
        return 0;
    case EE_SCREENS_SEUN_OP_GAME_VIEW:
        if (i[0])
            EESetGameViewRectangle(GERectangle(225.0f, 0.0f, 1024.0f, 606.0f));
        else
            EESetGameViewRectangle(GERectangle(0.0f, 0.0f, 1024.0f, 606.0f));
        gB529066 = false;
        gCameraFollowing = false;
        gB529068 = false;
        if (gMiniMap != NULL)
            gMiniMap->RefreshView();
        gUIManager->RedrawAll();
        gUIManager->GetCurrentForm()->SetAllControlsDirty();
        return 0;
    case EE_SCREENS_SEUN_OP_REDRAW: static_cast<UIControl*>(p[0])->GetForm()->GetManager()->RedrawAll(); return 0;
    case EE_SCREENS_SEUN_OP_CLEAR_SELECTION: SeunEditor(p[0])->ClearSelection(); return 0;
    case EE_SCREENS_SEUN_OP_MOVING: return ee_screens_sew_moving(SeunAction(p[0])->mAction) ? 1 : 0;
    case EE_SCREENS_SEUN_OP_CANCEL_MOVE: ee_screens_sew_cancel(SeunAction(p[0])->mAction); return 0;
    case EE_SCREENS_SEUN_OP_PICK_UP: ee_screens_sew_pick_up(SeunAction(p[0])->mAction, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_PUT_DOWN: ee_screens_sew_put_down(SeunAction(p[0])->mAction); return 0;
    case EE_SCREENS_SEUN_OP_PLACE_UNIT: ee_screens_sew_place_unit(SeunAction(p[0])->mAction, i[0]); return 0;
    case EE_SCREENS_SEUN_OP_SHOW_UNIT:
        static_cast<EEUIFSEUpgradePanel*>(p[0])->ShowUnit(static_cast<EEUnit*>(p[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_DRAW: {
        GERectangle theRectangle;
        if (theTab->mPanelRectangle.And(*static_cast<const GERectangle*>(p[2]), theRectangle))
            theTab->mForm->DrawBackground(static_cast<GEBitmap*>(p[1]), theRectangle,
                                          GERectangle(0.0f, 0.0f, 255.0f, 255.0f), theTab->mPanelRectangle);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ANGLE: f[0] = static_cast<EEUIFSEBRotation*>(p[0])->GetAngle(); return 0;
    case EE_SCREENS_SEUN_OP_TYPE_MODE: return (int32_t)sUnitTypeMode;
    case EE_SCREENS_SEUN_OP_DIRTY: {
        EESEUnit* theShell = static_cast<EESEUnit*>(p[1]);
        theShell->mPanelRectangle.SetLeft(0.0f);
        theShell->mPanelRectangle.SetTop(0.0f);
        theShell->mPanelRectangle.SetRight(224.0f);
        theShell->mPanelRectangle.SetBottom(605.0f);
        UIForm* theComboForm = static_cast<UIControl*>(p[0])->GetForm();
        theComboForm->GetManager()->AddDirtyRectangle(theShell->mPanelRectangle, theComboForm);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_LIST_SELECTION: {
        unsigned long theSelection;
        return theList->GetCurrentSelection(theSelection) == kErrorOK ? (int32_t)theSelection : -1;
    }
    case EE_SCREENS_SEUN_OP_LIST_COUNT: return (int32_t)theList->GetLineCount();
    case EE_SCREENS_SEUN_OP_LIST_SELECT: theList->ChangeSelectionIndex((unsigned long)i[0], true, false); return 0;
    case EE_SCREENS_SEUN_OP_LIST_TOP: theList->SetScrollBarCurrentRange(0); return 0;
    case EE_SCREENS_SEUN_OP_CANCEL_BUILD:
        if (i[0])
            theTab->mBuildGate.Cancel();
        else
            theTab->mBuildWall.Cancel();
        return 0;
    case EE_SCREENS_SEUN_OP_PLACE_INFO: {
        SeunPick thePick = SeunPickClass(theUnitList, i[0], (unsigned long)i[1]);
        long theID = thePick.mPlayer->GetPlayerID();
        i[1] = !((theID != 0 || !thePick.mClass->mIsGaiaPlaceable) &&
                 (!(theID > 0) || !thePick.mClass->mIsPlayerPlaceable));
        if (thePick.mNode != NULL && thePick.mNode->mData->mKind == 6)
            i[2] = EE_SCREENS_SEUN_KIND_WALL;
        else if (thePick.mClass != NULL && thePick.mClass->mCategory == kUnitCategoryGate)
            i[2] = EE_SCREENS_SEUN_KIND_GATE;
        else
            i[2] = EE_SCREENS_SEUN_KIND_UNIT;
        return (int32_t)theID;
    }
    case EE_SCREENS_SEUN_OP_START_BUILD: {
        SeunPick thePick = SeunPickClass(static_cast<EEUIFSELBUnitList*>(p[1]), i[0], (unsigned long)i[1]);
        if (i[2] == 0) {
            theTab->mBuildWall.mTechnology = thePick.mNode->mData->mID;
            theTab->mBuildWall.Initialize(thePick.mPlayer->GetPlayerID());
        } else {
            theTab->mBuildGate.mTechnology = thePick.mNode->mData->mID;
            theTab->mBuildGate.mWallType = thePick.mNode->GetUnitClass(0);
            theTab->mBuildGate.mUnitClass = thePick.mClass;
            theTab->mBuildGate.Initialize(thePick.mPlayer->GetPlayerID());
        }
        return 0;
    }
    case EE_SCREENS_SEUN_OP_POST_GHOST: {
        EEUIFSELBUnitList* theTypes = static_cast<EEUIFSELBUnitList*>(p[1]);
        SeunEditor(p[0])->mLoadThread->Post402(reinterpret_cast<void*>(theTypes->mUnitTypes[(unsigned long)i[0]]),
                                               reinterpret_cast<void*>((long)i[1]));
        return 0;
    }
    case EE_SCREENS_SEUN_OP_CARGO_CLEAR:
        theCargo->mCargo.clear();
        theCargo->DeleteAllEntries();
        return 0;
    case EE_SCREENS_SEUN_OP_SELECTED_COUNT: return (int32_t)SeunEditor(p[1])->GetSelectedUnitCount();
    case EE_SCREENS_SEUN_OP_CARGO_COUNT: {
        EEComplexUnit* theUnit = SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit;
        return theUnit != NULL ? (int32_t)theUnit->GetNumCargoUnits() : 0;
    }
    case EE_SCREENS_SEUN_OP_CARGO_ADD:
        theCargo->AddCargo(SeunEditor(p[1])->GetSelectedUnit((unsigned long)i[0])->mComplexUnit->mCargo[(unsigned long)i[1]]);
        return 0;
    case EE_SCREENS_SEUN_OP_LIST_SORT: theUnitList->SetSortMode(i[0]); return 0;
    case EE_SCREENS_SEUN_OP_LIST_EMPTY:
        theUnitList->mUnitTypes.clear();
        theUnitList->DeleteAllEntries();
        return 0;
    case EE_SCREENS_SEUN_OP_LIST_ADD: theUnitList->AddUnitType(i[0]); return 0;
    case EE_SCREENS_SEUN_OP_CLASS_COUNT: return (int32_t)EEUnitClassCount();
    case EE_SCREENS_SEUN_OP_CLASS: {
        EEUnitClass* theClass = gUnitClassTable[i[0]];
        if (theClass == NULL)
            return 0;
        i[1] = (int32_t)theClass->mUnitType;
        int32_t theFlags = 0;
        if (theClass->mIsScenarioSelectable) theFlags |= EE_SCREENS_SEUN_CLASS_SELECTABLE;
        if (theClass->mIsGaiaPlaceable) theFlags |= EE_SCREENS_SEUN_CLASS_GAIA;
        if (theClass->mIsPlayerPlaceable) theFlags |= EE_SCREENS_SEUN_CLASS_PLAYER;
        if (theClass->mAIKind == kAIKindHero || theClass->mCategory == kUnitCategoryLandMilitaryHero)
            theFlags |= EE_SCREENS_SEUN_CLASS_HERO;
        if (theClass->mDomains & kMovementAir) theFlags |= EE_SCREENS_SEUN_CLASS_AIR;
        if (theClass->IsLandBuilding()) theFlags |= EE_SCREENS_SEUN_CLASS_BUILDING;
        if (theClass->mDomains & kMovementLand) theFlags |= EE_SCREENS_SEUN_CLASS_LAND;
        if (theClass->mDomains & kMovementSea) theFlags |= EE_SCREENS_SEUN_CLASS_SEA;
        if (theClass->mDomains & kMovementSpace) theFlags |= EE_SCREENS_SEUN_CLASS_SPACE;
        return theFlags;
    }
    case EE_SCREENS_SEUN_OP_IN_VIEW: return gViewport.GetViewRectangle().Contains(GE2DPoint(f[0], f[1])) ? 1 : 0;
    case EE_SCREENS_SEUN_OP_MOUSE: {
        int32_t theButtons = 0;
        if (UIManager::GetLMouseButton()->JustDoublePressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_L_DOUBLE;
        if (UIManager::GetLMouseButton()->JustPressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_L_PRESSED;
        if (UIManager::GetLMouseButton()->JustUnpressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_L_RELEASED;
        if (UIManager::GetLMouseButton()->GetValue() != 0) theButtons |= EE_SCREENS_SEUN_MOUSE_L_DOWN;
        if (UIManager::GetRMouseButton()->JustPressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_R_PRESSED;
        if (UIManager::GetRMouseButton()->JustUnpressed()) theButtons |= EE_SCREENS_SEUN_MOUSE_R_RELEASED;
        return theButtons;
    }
    case EE_SCREENS_SEUN_OP_PICK: {
        EEUnit* theUnit = gUnitPicker.PickUnit(GE2DPoint(f[0], f[1]), 0);
        if (theUnit == NULL)
            return -1;
        int32_t theID = (int32_t)theUnit->mUnitID;
        i[1] = theUnit->mUnitClass->mCategory == kUnitCategoryGate;
        theUnit->Dereference();
        return theID;
    }
    case EE_SCREENS_SEUN_OP_HOLD_GATE: {
        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)(uint32_t)i[0]));
        theTab->mBuildGate.mTechnology = theUnit->mUnitClass->mTechnology;
        theTab->mBuildGate.mWallType = theUnit->GetTechTreeNode()->GetUnitClass(0);
        theTab->mBuildGate.mUnitClass = theUnit->mUnitClass;
        theUnit->Kill(0, 0, 0);
        theTab->mBuildGate.Initialize(theUnit->mPlayer->mPlayerID);
        return 0;
    }
    case EE_SCREENS_SEUN_OP_BUILD_UPDATE:
        if (i[0])
            theTab->mBuildGate.Update(GE2DPoint(f[0], f[1]));
        else
            theTab->mBuildWall.Update(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_START_SELECTION:
        static_cast<EEUICSelectUnits*>(p[0])->StartSelection(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_START_COMMAND:
        static_cast<EESEUICCommandUnits*>(p[0])->StartCommand(GE2DPoint(f[0], f[1]));
        return 0;
    case EE_SCREENS_SEUN_OP_LOCK:
        if (i[0])
            gWorld.mGhostUnitsLock->Enter();
        else
            gWorld.mGhostUnitsLock->Leave();
        return 0;
    case EE_SCREENS_SEUN_OP_KEYS: {
        ISEntity* theKey;
        int32_t theKeys = 0;
        gISKeyboard->GetEntity(kKeyDelete, theKey);
        if (theKey->JustPressed()) {
            theKeys |= 1;
            gISKeyboard->GetEntity(kKeyShift, theKey);
            if (theKey->GetValue() != 0)
                theKeys |= 2;
        }
        return theKeys;
    }
    case EE_SCREENS_SEUN_OP_DELETE_SELECTION: {
        EESEARust* theAction = new EESEARust(kSEADeleteUnit, NULL, -1, gWorld.GetCurrentPlayer()->GetPlayerID(), i[0] != 0);
        theAction->Dereference();
        return 0;
    }
    case EE_SCREENS_SEUN_OP_ROTATION_KEYS: static_cast<EEUIFSEBRotation*>(p[0])->UpdateKeyboard(); return 0;
    }
    return 0;
}
