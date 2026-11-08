// C:\EEX\Empire Earth\User Interface Thread\EEUICUnitIconBar.cpp (file name inferred: no __FILE__; RTTI class
// EEUICUnitIconBar). The main game form's selected-units icon bar: 0x259f7f..0x25b2ce (ctor, ??_G, dtor, the members,
// the message ctors after their first users, vector<EEButtonObject*>::push_back last; EEUICSelectUnits ends at
// 0x259f7f, EEUICWonNet* follows). Unit flags: the exe module set.
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
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/GELine.h"
#include "lle/UIControlsTable.h"
#include "ee/EEUICUnitIconBar.h"

static unsigned long sIconBarUpSound;           // 0x56458c (sound 0x208)
static unsigned long sIconBarDownSound;         // 0x564590 (sound 0x207)

// FUNCTION: EXE 0x659f7f
// Creates the selected-units icon bar with its icon frame bitmap and click sounds
EEUICUnitIconBar::EEUICUnitIconBar(UIControlsTable& inTable, const GERectangle& inRectangle, UIForm* inForm)
    : UIControl(kControlTypeCustom, inTable.GetID(), inRectangle, inForm, inTable.GetFormIndex(), 0), mIcon(new GEBitmap)
{
    this->mButtons.clear();
    this->mCopiedButtons.clear();
    this->mHealth.clear();
    this->mCopiedHealth.clear();
    this->mIconRectangle.SetLeft(0.0f);
    this->mIconRectangle.SetRight((this->GetRectangle().GetWidth() - 25.0f + 1.0f) * 0.04f);
    this->mIconRectangle.SetTop(0.0f);
    this->mIconRectangle.SetBottom(this->GetRectangle().GetHeight());
    this->mBackground = inTable.CreateNormalBitmap(inForm->GetManager(), this->mIconRectangle);
    EESoundObject* theSound = EESoundObject::Get(kSoundButton1);
    if (theSound) {
        theSound->LoadMedia();
        sIconBarDownSound = theSound->GetSampleID();
    }
    theSound = EESoundObject::Get(kSoundButton2);
    if (theSound) {
        theSound->LoadMedia();
        sIconBarUpSound = theSound->GetSampleID();
    }
    this->mShowCargo = false;
    this->mSelecting = false;
    this->mSpacing = 0.0f;
    this->mCopiedSpacing = 0.0f;
}

// SYNTHETIC: EXE 0x65a178
// (the scalar deleting destructor, emitted with the vtable; slot EEUICUnitIconBar+0x0)

// FUNCTION: EXE 0x65a194
// Destroys the icon bar, unloading its sounds and releasing its bitmaps and buttons
EEUICUnitIconBar::~EEUICUnitIconBar()
{
    EESoundObject* theSound = EESoundObject::Get(kSoundButton1);
    if (theSound)
        theSound->UnloadMedia();
    theSound = EESoundObject::Get(kSoundButton2);
    if (theSound)
        theSound->UnloadMedia();
    this->mIcon->Dereference();
    this->mBackground->Dereference();
    for (std::vector<EEButtonObject*>::iterator theIterator = this->mCopiedButtons.begin();
         theIterator != this->mCopiedButtons.end(); theIterator++)
        (*theIterator)->Release();
}

// FUNCTION: EXE 0x65a258
// Snapshots the icons, health values, multi-select marks and spacing for drawing
long EEUICUnitIconBar::CopyData()
{
    std::vector<EEButtonObject*>::iterator theIterator;
    for (theIterator = this->mCopiedButtons.begin(); theIterator != this->mCopiedButtons.end(); theIterator++)
        (*theIterator)->Release();
    this->mCopiedButtons.clear();
    this->mCopiedHealth.clear();
    this->mCopiedSelectIcons.clear();
    for (theIterator = this->mButtons.begin(); theIterator != this->mButtons.end(); theIterator++) {
        this->mCopiedButtons.push_back(*theIterator);
        (*theIterator)->Acquire();
    }
    for (std::vector<float>::iterator theHealth = this->mHealth.begin(); theHealth != this->mHealth.end(); theHealth++)
        this->mCopiedHealth.push_back(*theHealth);
    for (std::vector<long>::iterator theIcon = this->mSelectIcons.begin(); theIcon != this->mSelectIcons.end(); theIcon++)
        this->mCopiedSelectIcons.push_back(*theIcon);
    this->mCopiedSpacing = this->mSpacing;
    this->mCopiedIconRectangle = this->mIconRectangle;
    return UIControl::CopyData();
}

// FUNCTION: EXE 0x65a32f
// Draws up to 35 unit icons, each with a health bar (green over red) and a red outline when marked
// NEAR: inline budget (the original calls _Ptrit::operator+ and vector<long>::begin/end out of line and expands GEBitmap::SetLocation) and the frame layout of the seven locals
long EEUICUnitIconBar::DrawPolygons()
{
    GERectangle theIconRectangle;
    GERectangle theRedBar;
    GERectangle theGreenBar;
    GERectangle theBlackBar;
    GE2DPoint theLocation;
    GERhombus theRhombus;
    GELine theLine;
    long theResult = UIControl::DrawPolygons();
    float theOffset = this->GetCopiedDrawRectangle().GetHeight() * 0.1f;
    float theStep = this->mCopiedIconRectangle.GetWidth() * this->mCopiedSpacing;
    theLocation.SetX(this->GetCopiedDrawRectangle().GetLeft());
    theLocation.SetY(this->GetCopiedDrawRectangle().GetTop());
    GE2DPoint theIconLocation(theLocation.GetX(),
                              this->GetCopiedDrawRectangle().GetHeight() * 0.18f + theLocation.GetY() + theOffset);
    theIconRectangle.SetLeft(0.0f);
    theIconRectangle.SetTop(theIconLocation.GetY());
    theIconRectangle.SetRight(this->mCopiedIconRectangle.GetWidth());
    theIconRectangle.SetBottom(this->mCopiedIconRectangle.GetHeight() + theIconRectangle.GetTop() -
                               (this->GetCopiedDrawRectangle().GetHeight() * 0.18f + theOffset));
    theBlackBar.SetLeft(theLocation.GetX());
    theBlackBar.SetTop(theLocation.GetY());
    theBlackBar.SetRight(this->mCopiedIconRectangle.GetWidth() + theBlackBar.GetLeft());
    theBlackBar.SetBottom(this->GetCopiedDrawRectangle().GetHeight() * 0.18f + theBlackBar.GetTop() + theOffset);
    theRedBar.SetTop(this->GetCopiedDrawRectangle().GetHeight() * 0.09f + theLocation.GetY());
    theRedBar.SetBottom(theRedBar.GetTop() + theOffset);
    theGreenBar.SetTop(this->GetCopiedDrawRectangle().GetHeight() * 0.09f + theLocation.GetY());
    theGreenBar.SetBottom(theGreenBar.GetTop() + theOffset);
    for (unsigned long i = 0; i < this->mCopiedButtons.size() && i < 35; i++) {
        theBlackBar.SetLeft(theLocation.GetX());
        theBlackBar.SetRight(this->mCopiedIconRectangle.GetWidth() + theBlackBar.GetLeft());
        float theLeft = this->mCopiedIconRectangle.GetWidth() * 0.1f + theLocation.GetX();
        theGreenBar.SetLeft(theLeft);
        theGreenBar.SetRight(this->mCopiedIconRectangle.GetWidth() * 0.8f * this->mCopiedHealth[i] + theLeft);
        theRedBar.SetLeft(theGreenBar.GetRight());
        theRedBar.SetRight(this->mCopiedIconRectangle.GetWidth() * 0.9f + theLocation.GetX());
        this->mIcon->SetTexture(this->mCopiedButtons[i]->GetTexture(), this->GetScreenIndex());
        this->mCopiedButtons[i]->SetFaceBitmap(this->mIcon, theIconRectangle, gScreenIndex);
        this->mIcon->SetLocation(theIconLocation);
        this->GetRasterizer()->Draw(this->mIcon, this->GetScreenIndex());
        theRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f));
        theRhombus.SetRectangle(theBlackBar);
        this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        theRhombus.SetColor(GEColor(1.0f, 0.0f, 0.0f));
        theRhombus.SetRectangle(theRedBar);
        this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        theRhombus.SetColor(GEColor(0.0f, 1.0f, 0.0f));
        theRhombus.SetRectangle(theGreenBar);
        this->GetRasterizer()->Draw(&theRhombus, this->GetScreenIndex());
        this->mBackground->SetLocation(theLocation);
        theResult = this->GetRasterizer()->Draw(this->mBackground, this->GetScreenIndex());
        if (std::find(this->mCopiedSelectIcons.begin(), this->mCopiedSelectIcons.end(), i) != this->mCopiedSelectIcons.end()) {
            theLine.SetStartPoint(theLocation);
            theLine.SetEndPoint(GE2DPoint(this->mCopiedIconRectangle.GetWidth() + theLocation.GetX(), theLocation.GetY()));
            theLine.SetColor(GEColor(1.0f, 0.0f, 0.0f));
            gRasterizer->Draw(&theLine, 1, this->GetScreenIndex());
            theLine.SetEndPoint(GE2DPoint(theLocation.GetX(), this->mCopiedIconRectangle.GetHeight() + theLocation.GetY()));
            gRasterizer->Draw(&theLine, 1, this->GetScreenIndex());
            theLine.SetStartPoint(GE2DPoint(this->mCopiedIconRectangle.GetWidth() + theLocation.GetX(),
                                            this->mCopiedIconRectangle.GetHeight() + theLocation.GetY()));
            gRasterizer->Draw(&theLine, 1, this->GetScreenIndex());
            theLine.SetEndPoint(GE2DPoint(this->mCopiedIconRectangle.GetWidth() + theLocation.GetX(), theLocation.GetY()));
            theResult = gRasterizer->Draw(&theLine, 1, this->GetScreenIndex());
        }
        theLocation.SetXY(theLocation.GetX() + theStep + 1.0f, theLocation.GetY());
        theIconLocation = GE2DPoint(theLocation.GetX(),
                                    this->GetCopiedDrawRectangle().GetHeight() * 0.18f + theLocation.GetY() + theOffset);
    }
    return theResult;
}

// FUNCTION: EXE 0x65a954
// Returns the unit of an icon: a cargo unit of the selected transport, or a selected unit; NULL for 100
EEComplexUnit* EEUICUnitIconBar::GetUnit(unsigned long inIndex)
{
    EEComplexUnit* theResult = NULL;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    if (inIndex != 100 && theGroup && theGroup->GetNumUnits() > 0) {
        EEComplexUnit* theFirst = theGroup->GetUnit(0);
        if (this->mShowCargo && inIndex < theFirst->GetNumCargoUnits())
            theResult = theFirst->GetCargo(inIndex);
        else if (inIndex < theGroup->GetNumUnits())
            theResult = thePlayer->mSelectedGroup->GetUnit(inIndex);
    }
    return theResult;
}

// FUNCTION: EXE 0x65a9e2
// Returns the index of the icon under the mouse, or 100 when the bar is empty
// the mouse offset from the bar, x over the icon pitch
long EEUICUnitIconBar::GetIconIndex()
{
    long theResult = 100;
    if (this->mButtons.size() > 0) {
        GE2DPoint theOffset = this->GetMouseLocation();
        theOffset.SetX(theOffset.GetX() - this->GetRectangle().GetLeft());
        theOffset.SetY(this->GetMouseLocation().GetY() - this->GetRectangle().GetTop());
        theResult = theOffset.GetX() / (this->mIconRectangle.GetWidth() * this->mSpacing + 1.0f);
    }
    return theResult;
}

// FUNCTION: EXE 0x65aa43
// Collects the ids of the listed units of a type
void EEUICUnitIconBar::GetUnitIDs(unsigned long inType, std::vector<long>& outIDs)
{
    for (unsigned long i = 0; i < this->mButtons.size(); i++) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        if (theUnit && theUnit->GetUnitClass()->mUnitType == inType)
            outIDs.push_back(theUnit->GetUnitID());
    }
}

// FUNCTION: EXE 0x65aa9b
// On a click on an icon: selects that unit (Ctrl: all of its type, Shift: marks it for a multi-select), or
// unloads a cargo unit, or scrambles an idle aircraft from an airbase
// NEAR: inline budget (the original expands EEMSelectUnit/EEMImmediateCargoUnload with EEMessage's ctor refused; ours refuses EEMSelectUnit) and a 4-byte smaller frame
long EEUICUnitIconBar::MouseLButtonUp()
{
    std::vector<long> theIDs;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (this->GetRectangle().Contains(this->GetMouseLocation())) {
        EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
        long theIndex = this->GetIconIndex();
        EEComplexUnit* theUnit = this->GetUnit(theIndex);
        if (theUnit) {
            EEComplexUnit* theFirst = theGroup->GetUnit(0);
            EEMessage* theMessage;
            if (!this->mShowCargo) {
                if (UIControl::ControlKeyPressed()) {
                    this->GetUnitIDs(theUnit->GetUnitClass()->mUnitType, theIDs);
                    theMessage = new EEMSelectTheseUnits(theIDs);
                    gSoundManager->Play(sIconBarDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
                    gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
                } else if (!this->mSelecting && !UIControl::ShiftKeyPressed()) {
                    theMessage = new EEMSelectUnit(theUnit->GetUnitID());
                    gSoundManager->Play(sIconBarDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
                    gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
                } else {
                    this->mSelecting = true;
                    this->mSelectIDs.push_back(theUnit->GetUnitID());
                    this->mSelectIcons.push_back(theIndex);
                    gSoundManager->Play(sIconBarUpSound, 1.0f, 0.0f, 1.0f, false, NULL);
                    this->SetDirty(true);
                }
            } else if (theFirst->GetUnitClass()->IsTransport()) {
                theMessage = new EEMImmediateCargoUnload(theFirst->GetUnitID(), theUnit->GetUnitID(), false);
                gSoundManager->Play(sIconBarDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
                gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
            } else if (theFirst->GetUnitClass()->IsAirBase()) {
                EEUnitClass* theClass = theUnit->GetUnitClass();
                EEUnitGoal* theGoal = NULL;
                if (theClass->mFamily == kUnitFamilyAircraft || theClass->mFamily == kUnitFamilySpaceFighter)
                    theGoal = theFirst->GetQueuedGoal(1);
                else if (theClass->mFamily == kUnitFamilyBomber) {
                    if (!theClass->mIsParatrooperBomber)
                        theGoal = theFirst->GetQueuedGoal(2);
                } else if (theClass->mFamily == kUnitFamilyAtomicBomber)
                    theGoal = theFirst->GetQueuedGoal(3);
                if (theGoal == NULL &&
                    (theUnit->GetQueueGoals(1).size() == 0 || theUnit->mGoalQueues[1].GetFront() == NULL)) {
                    theMessage = new EEMScrambleAircraft(theUnit->GetUnitID(), theFirst->GetTileX(),
                                                         theFirst->GetTileY());
                    gSoundManager->Play(sIconBarDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
                    gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
                }
            }
        }
    }
    this->ReleaseCapturedMouse();
    return 0;
}

// FUNCTION: EXE 0x65ad92
// Creates a scramble-aircraft message for a unit and a tile
// (EEMScrambleAircraft::EEMScrambleAircraft(long, long, long), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65adcd
// Creates a select-these-units message from a list of ids
// (EEMSelectTheseUnits::EEMSelectTheseUnits(std::vector<long>&), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65ae3b
// Captures the mouse on a right press (also the left press, ICF-folded)
long EEUICUnitIconBar::MouseRButtonDown()
{
    this->CaptureMouse();
    return 0;
}

long EEUICUnitIconBar::MouseLButtonDown()
{
    this->CaptureMouse();
    return 0;
}

// FUNCTION: EXE 0x65ae44
// On a right click on an icon, removes the unit from the selection, or unloads a cargo unit
long EEUICUnitIconBar::MouseRButtonUp()
{
    if (this->GetRectangle().Contains(this->GetMouseLocation())) {
        EEComplexUnit* theUnit = this->GetUnit(this->GetIconIndex());
        if (theUnit && !this->mShowCargo) {
            gCommunications->ProcessLocalUserInput(new EEMRemoveUnitFromSelectedGroup(theUnit->GetUnitID()), kSlotNone);
            gSoundManager->Play(sIconBarDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
        } else if (this->mShowCargo && theUnit) {
            EEComplexUnit* theTransport = gWorld.GetCurrentPlayer()->mSelectedGroup->GetUnit(0);
            if (theTransport && theTransport->GetUnitClass()->IsTransport()) {
                gCommunications->ProcessLocalUserInput(
                    new EEMImmediateCargoUnload(theTransport->GetUnitID(), theUnit->GetUnitID(), false), kSlotNone);
                gSoundManager->Play(sIconBarDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
            }
        }
    }
    this->ReleaseCapturedMouse();
    return 0;
}

// FUNCTION: EXE 0x65af3d
// Creates an immediate cargo unload message for a transport and a cargo unit
// (EEMImmediateCargoUnload::EEMImmediateCargoUnload(long, long, bool), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65af78
// Creates a remove-from-selection message for a unit
// (EEMRemoveUnitFromSelectedGroup::EEMRemoveUnitFromSelectedGroup(long), inline in EEMessage.h; emitted after its first user)

// FUNCTION: EXE 0x65afa5
// Sends the Shift-marked units as the new selection once Shift is released
long EEUICUnitIconBar::MouseMove(const GE2DPoint& inLocation)
{
    if (!this->mSelecting)
        return 0;
    this->mSelecting = UIControl::ShiftKeyPressed();
    if (this->mSelecting)
        return 0;
    gCommunications->ProcessLocalUserInput(new EEMSelectTheseUnits(this->mSelectIDs), kSlotNone);
    this->mSelectIDs.clear();
    this->mSelectIcons.clear();
    return 0;
}

// FUNCTION: EXE 0x65b010
// Refills the bar from the selection (or the single selected transport's cargo): icons, health and spacing
// NEAR: the original calls mCargo.size() out of line (inline budget) and keeps the group in edi, the first unit in ebx
void EEUICUnitIconBar::Update()
{
    EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
    EEComplexUnit* theFirst;
    unsigned long i;
    EEComplexUnit* theUnit;
    unsigned long j;
    EEComplexUnit* theCargo;
    float theHealth;
    theFirst = theGroup->GetNumUnits() > 0 ? theGroup->GetUnit(0) : NULL;
    unsigned long theCount;
    if (theGroup->GetNumUnits() == 1 && theFirst && theFirst->mCargo.size() != 0)
        theCount = min(theFirst->mCargo.size(), 35);
    else
        theCount = min(theGroup->GetNumUnits(), 35);
    this->mSpacing = min(1.0f, ((this->GetRectangle().GetWidth() - this->mIconRectangle.GetWidth() - 25.0f + 1.0f) /
                          this->mIconRectangle.GetWidth() - 1.0f) / (theCount - 1.0f));
    this->mButtons.clear();
    this->mHealth.clear();
    this->mSelectIDs.clear();
    this->mSelectIcons.clear();
    this->mShowCargo = false;
    for (i = 0; i < theGroup->GetNumUnits(); i++) {
        theUnit = theGroup->GetUnit(i);
        if (theGroup->GetNumUnits() == 1 && theUnit->GetNumCargoUnits() != 0) {
            for (j = 0; j < theUnit->GetNumCargoUnits(); j++) {
                theCargo = theUnit->mCargo[j];
                this->mButtons.push_back(theCargo->mStats->mButton);
                theHealth = (float)theCargo->GetHitPoints() / theCargo->mStats->mMaxHitPoints;
                this->mHealth.push_back(max(min(theHealth, 1.0f), 0.0f));
                this->mShowCargo = true;
            }
        } else {
            this->mButtons.push_back(theUnit->mStats->mButton);
            this->mHealth.push_back((float)theUnit->GetHitPoints() / (float)theUnit->mStats->mMaxHitPoints);
        }
    }
    this->SetDirty(true);
}

// FUNCTION: EXE 0x65b2a2
// Appends a button to a vector (std::vector<EEButtonObject*>::push_back)
// (std::vector<EEButtonObject*>::push_back, a template COMDAT closing the unit)
