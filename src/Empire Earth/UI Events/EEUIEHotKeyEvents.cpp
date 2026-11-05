// C:\EEX\Empire Earth\UI Events\EEUIEHotKeyEvents.cpp (file name inferred: no __FILE__; class prefix EEUIEHK,
// plus EEUIESelectClosestAircraft: the vector<EEUIEHKUnitEntry>::push_back COMDAT both units would need sits after
// SelectClosestAircraft's helper, so they are one unit). The hot-key events: 0x1e93df..0x1e9f40. Unit flags: the exe
// module set.
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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEAction.h"
#include "ee/EEAPlayerEvent.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIBSavedGroup.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUIEHotKeyEvents.h"

extern GEViewport gU4E8F08;                 // 0x4e8f08 (EERenderObject.cpp; the render viewport)

// one candidate of the unit selection hot keys (file-local; provisional name)
struct EEUIEHKUnitEntry {
    EEComplexUnit* mUnit;                   // +0x00
    bool mSelected;                         // +0x04 already in the selected group
};

static bool IsSelectableAircraft(EEUnit* inUnit, long inKind);

// FUNCTION: EXE 0x5e93df
// Adds the current selection to a saved control group (sends the add-selection message)
long EEUIEHKAddSelectionToGroup::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    thePlayer->UpdateSelection(NULL, true);
    thePlayer->mSavedGroups.Reset(0x14, true);
    gCommunications->ProcessLocalUserInput(new EEMSavedGroupAddSelection(this->mGroup), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e943e
// Creates the message that adds the selection to a saved group
// (EEMSavedGroupAddSelection's inline ctor, emitted after its first user)

// FUNCTION: EXE 0x5e946b
// Saves the current selection as a control group and shows its first unit on the group's button
long EEUIEHKCreateGroup::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    thePlayer->UpdateSelection(NULL, true);
    thePlayer->mSavedGroups.Reset(0x14, true);
    gCommunications->ProcessLocalUserInput(new EEMSavedGroupCreate(this->mGroup), kSlotNone);
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    static_cast<EEUIBSavedGroup*>(inForm->GetControl(this->mGroup + 0xd4))->SetUnit(theGroup->GetFirstUnit());
    return kErrorOK;
}

// FUNCTION: EXE 0x5e94f1
// Creates the message that saves the selection as a group
// (EEMSavedGroupCreate's inline ctor)

// FUNCTION: EXE 0x5e951e
// Posts a player-event action
long EEUIEHKPlayerEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEAction* theAction = new EEAPlayerEvent;
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e955c
// Creates the select-group hot key event for a group number and flag
EEUIEHKSelectGroup::EEUIEHKSelectGroup(long inGroup, bool inFlag) : mGroup(inGroup)
{
    this->mFlag = inFlag;
}

// FUNCTION: EXE 0x5e9580
// Destroys the select-group event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIEHKSelectGroup)

// FUNCTION: EXE 0x5e959c
// Destroys the select-group event (nothing to release)
EEUIEHKSelectGroup::~EEUIEHKSelectGroup()
{
}

// FUNCTION: EXE 0x5e95b1
// Selects a saved control group (sends the select-group message with the event's flag)
long EEUIEHKSelectGroup::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    thePlayer->UpdateSelection(NULL, true);
    thePlayer->mSavedGroups.Reset(0x14, true);
    gCommunications->ProcessLocalUserInput(new EEMSavedGroupSelect(this->mGroup, this->mFlag), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e9616
// Creates the message that selects a saved group
// (EEMSavedGroupSelect's inline ctor)

// FUNCTION: EXE 0x5e964a
// Selects units for a hot key (idle citizens, idle military, town centres/capitols, or one unit type): cycles to the
// next unit by id and centres the camera on it, or selects all; Shift adds to the selection
long EEUIEHKUnitSelection::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    GE3DPoint theCenter;
    std::vector<EEUIEHKUnitEntry> theUnits;
    std::vector<EEComplexUnit*> theSelection;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    bool theSpecial = false;
    EEComplexUnit* theNext = NULL;
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    EEComplexUnit* theHighestSelected;
    EEComplexUnit* theLowestSelected;
    EEComplexUnit* theLowestUnselected;
    unsigned long theCount;
    EEUIEHKUnitEntry theEntry;
    ISEntity* theKey;
    theLowestUnselected = theLowestSelected = theHighestSelected = 0;
    theCount = 0;
    thePlayer->UpdateSelection(NULL, true);
    if (this->mUnitType == kUnitTypeTownCenter)
        theSpecial = true;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mComplexUnit && theUnit->mHitPoints > 0 && theUnit->mTile != NULL) {
            long theType = theUnit->mUnitClass->mUnitType;
            long theCommand = theUnit->mComplexUnit->mCommand ? theUnit->mComplexUnit->mCommand->mType : 0;
            float theValue = theUnit->mStats->mMaxVelocity;
            if (this->mIdleWorkers) {
                if ((theUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen || theUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen) &&
                    (theCommand == kActionIdle || theCommand == kActionWaitForOpenPosition)) {
                    theEntry.mUnit = theUnit->mComplexUnit;
                    theEntry.mSelected = theGroup->Contains(theUnit->GetComplexUnit());
                    theUnits.push_back(theEntry);
                }
            }
            if (this->mIdleMilitary) {
                if (theUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen && theUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen &&
                    theCommand == kActionIdle && theValue > 0.0f) {
                    theEntry.mUnit = theUnit->mComplexUnit;
                    theEntry.mSelected = theGroup->Contains(theUnit->GetComplexUnit());
                    theUnits.push_back(theEntry);
                }
            }
            if (theSpecial) {
                if (theType == kUnitTypeCapitol || theType == kUnitTypeTownCenter) {
                    theEntry.mUnit = theUnit->mComplexUnit;
                    theEntry.mSelected = theGroup->Contains(theUnit->GetComplexUnit());
                    theUnits.push_back(theEntry);
                }
            }
            if (this->mUnitType == kUnitTypeGranary && theType == kUnitTypeGranaryImproved) {
                theEntry.mUnit = theUnit->mComplexUnit;
                theEntry.mSelected = theGroup->Contains(theUnit->GetComplexUnit());
                theUnits.push_back(theEntry);
            }
            if (!this->mIdleWorkers && !this->mIdleMilitary && !theSpecial && this->mUnitType == theType) {
                theEntry.mUnit = theUnit->mComplexUnit;
                theEntry.mSelected = theGroup->Contains(theUnit->GetComplexUnit());
                theUnits.push_back(theEntry);
            }
        }
    }
    if (theUnits.empty())
        return kErrorOK;
    thePlayer->mSavedGroups.Reset(0x14, true);
    if (!this->mSelectAll) {
        for (unsigned long i = 0; i < theUnits.size(); i++) {
            if (theUnits[i].mSelected) {
                if (theLowestSelected == NULL || theUnits[i].mUnit->mUnitID < theLowestSelected->mUnitID)
                    theLowestSelected = theUnits[i].mUnit;
                if (theHighestSelected == NULL || theUnits[i].mUnit->mUnitID > theHighestSelected->mUnitID)
                    theHighestSelected = theUnits[i].mUnit;
            } else if (theLowestUnselected == NULL || theUnits[i].mUnit->mUnitID < theLowestUnselected->mUnitID)
                theLowestUnselected = theUnits[i].mUnit;
        }
        if (theHighestSelected) {
            for (unsigned long j = 0; j < theUnits.size(); j++) {
                if (!(theUnits[j].mUnit->mUnitID > theHighestSelected->mUnitID))
                    continue;
                if (theNext == NULL || theUnits[j].mUnit->mUnitID < theNext->mUnitID) {
                    if (!theUnits[j].mSelected)
                        theNext = theUnits[j].mUnit;
                }
            }
            if (theNext == NULL) {
                if (theLowestUnselected == NULL)
                    theNext = theLowestSelected;
                else
                    theNext = theLowestUnselected;
            }
        } else
            theNext = theLowestUnselected;
        theSelection.push_back(theNext);
    } else {
        theSelection.reserve(theUnits.size());
        for (long k = theUnits.size() - 1; k >= 0; k--)
            theSelection.push_back(theUnits[k].mUnit);
    }
    gISKeyboard->GetEntity(kKeyShift, theKey);
    bool theAdd = theKey->GetValue() > 0;
    while (!theSelection.empty()) {
        theNext = theSelection.back();
        theSelection.pop_back();
        if (!theAdd && theCount == 0) {
            gCommunications->ProcessLocalUserInput(new EEMSelectUnit(theNext->GetUnitID()), kSlotNone);
            if (!this->mSelectAll) {
                const GETransformation& theTransformation = theNext->mPhysicalModel.GetLocation();
                theCenter.SetX(theTransformation.GetX());
                theCenter.SetY(theTransformation.GetY());
                theCenter.SetZ(theTransformation.GetZ());
                gWorld.mCameraControl.SetChanged();
                gWorld.mCameraControl.SetLocation(theCenter, false, true);
                gMiniMap->Refresh();
                gCameraFollowing = false;
            }
        } else
            gCommunications->ProcessLocalUserInput(new EEMAlwaysAddUnitToSelectedGroup(theNext->GetUnitID()), kSlotNone);
        theCount++;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e9bea
// Selects the matching aircraft closest to the point under the cursor (Shift adds one not yet selected); plays the
// hot-key alarm when there is none
long EEUIESelectClosestAircraft::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    GE2DPoint theCursor(gU4E8F08.GetXCenter(), gU4E8F08.GetYCenter());
    EEUnit* theClosest = NULL;
    GE3DPoint theTarget;
    GE3DPoint theLocation;
    float theBestDistance = 0.0f;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    ISEntity* theKey;
    gISKeyboard->GetEntity(kKeyShift, theKey);
    bool theReplace = !theKey->GetValue();
    if (gWorld.mMap->GetLocationAt(theCursor, theTarget, true) == NULL) {
        long theX;
        long theY;
        gWorld.mMap->GetTileAt(theCursor, theX, theY);
        theTarget.SetX(theX + 0.5f);
        theTarget.SetY(theY + 0.5f);
    }
    theTarget.SetZ(0.0f);
    theLocation.SetZ(0.0f);
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!IsSelectableAircraft(theUnit, this->mKind))
            continue;
        if (!theReplace && theGroup->Contains(theUnit->GetComplexUnit()))
            continue;
        if (theUnit->mComplexUnit->mContainer == NULL) {
            theLocation.SetX(theUnit->mComplexUnit->mPhysicalModel.GetLocation().GetX());
            theLocation.SetY(theUnit->mComplexUnit->mPhysicalModel.GetLocation().GetY());
        } else {
            theLocation.SetX(theUnit->mComplexUnit->mContainer->mPhysicalModel.GetLocation().GetX());
            theLocation.SetY(theUnit->mComplexUnit->mContainer->mPhysicalModel.GetLocation().GetY());
        }
        float theDistance = theLocation.DistanceToSquared(theTarget);
        if (theClosest == NULL || theDistance < theBestDistance) {
            theBestDistance = theDistance;
            theClosest = theUnit;
        }
    }
    if (theClosest) {
        NEMessage* theMessage;
        if (theReplace)
            theMessage = new EEMSelectUnit(theClosest->mUnitID);
        else
            theMessage = new EEMAlwaysAddUnitToSelectedGroup(theClosest->mUnitID);
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    } else if (EESoundObject::Get(kSoundHotKeyAlarm))
        gSoundManager->Play(EESoundObject::Get(kSoundHotKeyAlarm)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    return kErrorOK;
}

// FUNCTION: EXE 0x5e9df2
// Returns whether a unit is a living, commandable, free aircraft of the hot key's kind (bombers, atomic bombers, two
// fighter groups, space or planetary fighters); one inside a container only at full hit points
static bool IsSelectableAircraft(EEUnit* inUnit, long inKind)
{
    EEComplexUnit* theUnit = inUnit ? inUnit->mComplexUnit : 0;
    bool theResult = false;
    if (theUnit) {
        long theCategory = theUnit->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryPlanetSpaceship) {
            long theHitPoints = theUnit->mHitPoints;
            if (theHitPoints > 0 && theUnit->mIsCommandable && (theUnit->mTileX != -10000 || theUnit->mContainer) &&
                !theUnit->mIsDisabled && theUnit->mPreviousPart == NULL && (!theUnit->mGoalState || theUnit->mGoalState->mKind == kGoalLocation)) {
                long theKind1 = theUnit->mUnitClass->mFamily;
                long theKind2 = theUnit->mUnitClass->mAmmoType;
                switch (inKind) {
                case 0:
                    theResult = theKind1 == kUnitFamilyBomber && theKind2 == 0x1f;
                    break;
                case 1:
                    theResult = theKind1 == kUnitFamilyAtomicBomber && theKind2 == 0x1f;
                    break;
                case 2:
                    theResult = theKind1 == kUnitFamilyAircraft && theKind2 == 0xc;
                    break;
                case 3:
                    theResult = theKind1 == kUnitFamilyAircraft && (theKind2 == 0x28 || theKind2 == 0x1e || theKind2 == 0x29);
                    break;
                case 4:
                    theResult = theUnit->mUnitClass->mUnitType == kUnitTypeSp15SpaceFighter;
                    break;
                case 5:
                    theResult = theUnit->mUnitClass->mUnitType == kUnitTypeFighter15PlanetaryFighter;
                    break;
                default:
                    theResult = false;
                    break;
                }
                if (theResult) {
                    if (theUnit->mContainer == NULL || theHitPoints == theUnit->mStats->mMaxHitPoints)
                        return true;
                    theResult = false;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5e9f14
// Appends a hot-key candidate to the list (vector push_back)
// (std::vector<EEUIEHKUnitEntry>::push_back, the unit's template COMDAT)
