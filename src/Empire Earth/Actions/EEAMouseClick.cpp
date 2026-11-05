// C:\EEX\Empire Earth\Actions\EEAMouseClick.cpp (file name inferred: no __FILE__; RTTI class EEAMouseClick,
// vtable 0x438dc8). The mouse click action: 0x2fb0..0x4352 (EEAMiniMap's code ends at 0x2fb0, EEAPlayerEvent's Virtual4
// starts at 0x4352). ctor, ??_G, dtor, Virtual4 (sends the built message), CreateLoadTransportMessage (the load-transport message) with
// EEMLoadTransport's inline ctor and label, SelectAtMouse (a plain click) with the selection messages' inline ctors, Virtual0C
// (builds the message for a click) with the EEM*/EEUnitClass/EETile inline COMDATs after it (first user).
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
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
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "lle/GE3DPoint.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMouse.h"
#include "ee/EEMessage.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAction.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEDoppelganger.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEEffectManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAMouseClick.h"

// FUNCTION: EXE 0x402fb0
// Creates the mouse-click action with no message
EEAMouseClick::EEAMouseClick()
    : mMessage(NULL)
{
}

// SYNTHETIC: EXE 0x402fc8
// (the scalar deleting destructor)

// FUNCTION: EXE 0x402fe4
// Destroys the mouse-click action, releasing the message it has not sent
EEAMouseClick::~EEAMouseClick()
{
    if (this->mMessage)
        this->mMessage->Dereference();
}

// FUNCTION: EXE 0x403009
// Sends the built click message as local user input (resetting the player's saved group 0x15) and forgets it
void EEAMouseClick::Virtual4()
{
    if (!this->mMessage)
        return;
    ::gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x15, 1);
    gCommunications->ProcessLocalUserInput(this->mMessage, kSlotNone);
    this->mMessage = NULL;
}

// FUNCTION: EXE 0x403046
// Builds the message that loads a group into a transport of the clicked kind; NULL without a group
EEMessage* EEAMouseClick::CreateLoadTransportMessage(EEGroup* inGroup, long inKind, unsigned long inTargetID)
{
    EEMessage* theMessage = NULL;
    long theCategory = 0x23;
    if (inGroup) {
        switch (inKind) {
        case 0x80:
            theCategory = 0x11;
            break;
        case 0x100:
            theCategory = 0x10;
            break;
        case 0x200:
            theCategory = 0x12;
            break;
        case 0x4000:
            theCategory = 0x1d;
            break;
        case 0x10000:
            theCategory = 0x21;
            break;
        }
        EEComplexUnit* theTransport = inGroup->FindUnitOfCategory(theCategory);
        if (theTransport) {
            unsigned long theTransportID = theTransport->GetUnitID();
            if (theTransportID == inTargetID && theTransport->mIsBuilt) {
                if (inGroup->GetNumUnits() > 1)
                    theMessage = new EEMLoadTransport(theTransportID, -1, 0);
            } else {
                EEUnit* theTarget = static_cast<EEUnit*>(::gWorld.GetUnit(inTargetID));
                if (theTarget && theTarget->GetClass() == theTransport->GetClass() && inGroup->GetNumUnits() > 1 &&
                    theTarget->mTileNode && theTarget->GetComplexUnit()->mIsBuilt)
                    theMessage = new EEMLoadTransport(inTargetID, -1, 0);
                else if (theTransport->mIsBuilt)
                    theMessage = new EEMLoadTransport(theTransportID, inTargetID, 0);
            }
        }
    }
    return theMessage;
}

// FUNCTION: EXE 0x403192
// Creates the message that loads units into a transport
// (EEMLoadTransport's inline ctor, first user)

// FUNCTION: EXE 0x4031cd
// Returns the load-transport message's label
// (EEMLoadTransport::GetMessageLabel: inline, with the vtable)

// FUNCTION: EXE 0x4031d3
// Builds the selection message for a plain click: select, add or toggle the unit under the mouse, or unselect all
void EEAMouseClick::SelectAtMouse(unsigned long inModifiers)
{
    EEMessage* theMessage;
    EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
    if (theUnit) {
        if (inModifiers & kModifierShift)
            theMessage = new EEMAlwaysAddUnitToSelectedGroup(theUnit->GetUnitID());
        else if (inModifiers & kModifierControl)
            theMessage = new EEMAddUnitToSelectedGroup(theUnit->GetUnitID());
        else
            theMessage = new EEMSelectUnit(theUnit->GetUnitID());
        theUnit->Dereference();
    } else {
        if (inModifiers != 0)
            return;
        theMessage = new EEMUnselectAllUnits;
    }
    if (theMessage) {
        this->mMessage = theMessage;
        gUserInterface->PostAction(this);
    }
}

// FUNCTION: EXE 0x4032a6
// Creates a base message with a type and the current player
// (EEMessage's inline ctor, refused in SelectAtMouse)

// FUNCTION: EXE 0x4032d2
// Creates the message that adds a unit to the selected group
// (EEMAddUnitToSelectedGroup's inline ctor)

// FUNCTION: EXE 0x4032ff
// Creates the message that selects a single unit
// (EEMSelectUnit's inline ctor)

// FUNCTION: EXE 0x40332c
// Creates the message that unselects every unit
// (EEMUnselectAllUnits's inline ctor)

// FUNCTION: EXE 0x403350
// Builds the order for a right click at a world point for the selected group (move, attack, gather, load, repair ...)
void EEAMouseClick::Execute(float inX, float inY, bool inMove, unsigned long inModifiers,
                             bool inIsRotating, float inFacing, bool inForceMove)
{
    EEEffectHandle theHandle;
    std::vector<EETileCoordinate> theUnused;
    std::vector<EEUnit*> theFarmers;
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    GE2DPoint thePoint(0.0f, 0.0f);
    EECPASGFlags theFlags;
    EEPlayer* thePlayer = ::gWorld.GetCurrentPlayer();
    EEGroup* theGroup = thePlayer->mSelectedGroup;
    EEComplexUnit* theSelected = NULL;
    EETile* theTile = NULL;
    unsigned long theGroupFlags = theGroup->mKindFlags;
    EEMessage* theMessage = NULL;
    EEUnit* theTarget = NULL;
    unsigned long theCount = theGroup->GetNumUnits();
    bool theShift = (inModifiers & kModifierShift) != 0;
    bool theControl = (inModifiers & kModifierControl) != 0;
    bool theFlag = (theGroupFlags & kUnitKindCitizen) != 0;
    thePoint = gMouse->mPosition;
    bool theAllow = false;
    bool theCitizens = true;
    bool theMoveTo = false;
    long theTileX, theTileY;
    if (theCount > 0) {
        if (!inForceMove) {
            if (theGroup->CountUnitsOfKind(2) == theCount && (theGroup->mKindFlags & kUnitKindBuilding) != 0) {
                for (unsigned long i = 0; i < theGroup->GetNumUnits() && theCitizens; i++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(i);
                    long theType = theUnit != NULL ? theUnit->GetUnitClass()->GetUnitType() : 0;
                    theCitizens = theType == kUnitTypeSettlement || theType == kUnitTypeCapitol || theType == kUnitTypeTownCenter;
                }
                theTarget = gUnitPicker.PickUnit(thePoint, theFlag || theCitizens);
                if (theTarget != NULL &&
                    (theTarget->GetPlayer()->GetPlayerID() != 0 || theTarget->GetComplexUnit() == NULL || !EEHerd::IsAnimalOnMap(theTarget->GetComplexUnit())) &&
                    !EEIsSeenBy(theTarget, thePlayer, true)) {
                    if (EEIsFootprintVisibleTo(theTarget, thePlayer) && (theTile = static_cast<EETile*>(theTarget->GetRTSTile())) != 0) {
                        EEDoppelganger* theLast = NULL;
                        bool theFound = false;
                        unsigned long theNum = theTile->GetNumDoppelgangers();
                        for (unsigned long j = 0; j < theNum; j++) {
                            if (theTile->GetDoppelganger(j)->mPlayer != thePlayer)
                                continue;
                            theLast = theTile->GetDoppelganger(j);
                            if (theLast->mUnit == theTarget) {
                                theFound = true;
                                break;
                            }
                        }
                        if (!theFound) {
                            if (theLast != NULL) {
                                theTarget->Dereference();
                                theTarget = theLast->mUnit;
                                if (theTarget != NULL)
                                    theTarget->Reference();
                            } else {
                                theTarget->Dereference();
                                theTarget = NULL;
                            }
                        }
                    } else {
                        theTarget->Dereference();
                        theTarget = NULL;
                    }
                }
                if (theGroup->GetUnitTypeCount(0x24) > 0)
                    theAllow = true;
                if (EEPointUnitOrTileXY(thePoint, theTile, theTileX, theTileY, theAllow) &&
                    EEGetFirstSelectedUnit(theSelected) && theSelected->GetUnitClass()->GetCanSetRallyPoint()) {
                    EEUnit* theRallyUnit = theSelected->GetComplexUnit();
                    long theTargetID = theTarget != NULL ? theTarget->GetUnitID() : -1;
                    gCommunications->ProcessLocalUserInput(
                        new EEMSetRallyGoal(theRallyUnit->GetUnitID(), GE2DPoint(inX, inY), theTargetID, theShift), kSlotNone);
                }
            } else {
                bool theCanDrop = theGroup->GetUnit(0) != NULL &&
                    (theGroup->GetUnit(0)->GetUnitClass()->GetIsTower() || theGroup->GetUnit(0)->GetUnitClass()->GetUnitType() == kUnitTypeCpMissileBase);
                theTarget = gUnitPicker.PickUnit(thePoint, theFlag);
                if (theTarget != NULL &&
                    (theTarget->GetPlayer()->GetPlayerID() != 0 || theTarget->GetComplexUnit() == NULL || !EEHerd::IsAnimalOnMap(theTarget->GetComplexUnit())) &&
                    !EEIsSeenBy(theTarget, thePlayer, true)) {
                    if (EEIsFootprintVisibleTo(theTarget, thePlayer) && (theTile = static_cast<EETile*>(theTarget->GetRTSTile())) != 0) {
                        EEDoppelganger* theLast = NULL;
                        bool theFound = false;
                        unsigned long theNum = theTile->GetNumDoppelgangers();
                        for (unsigned long j = 0; j < theNum; j++) {
                            if (theTile->GetDoppelganger(j)->mPlayer != thePlayer)
                                continue;
                            theLast = theTile->GetDoppelganger(j);
                            if (theLast->mUnit == theTarget) {
                                theFound = true;
                                break;
                            }
                        }
                        if (!theFound) {
                            if (theLast != NULL) {
                                theTarget->Dereference();
                                theTarget = theLast->mUnit;
                                if (theTarget != NULL)
                                    theTarget->Reference();
                            } else {
                                theTarget->Dereference();
                                theTarget = NULL;
                            }
                        }
                    } else {
                        theTarget->Dereference();
                        theTarget = NULL;
                    }
                }
                if (theTarget == NULL && theFlag == true && inMove &&
                    EEPointUnitOrTileXY(thePoint, theTile, theTileX, theTileY, false) && theTile != NULL && theTile->IsVisibleTo(thePlayer)) {
                    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                        if (!theNode->mUnit->GetUnitClass()->GetIsTree())
                            continue;
                        theTarget = theNode->mUnit;
                        theTarget->Reference();
                        break;
                    }
                }
                if (theTarget != NULL && theTarget->GetUnitClass()->GetIsTree() && !theFlag) {
                    theFlags = theGroup->GetFlags();
                    if (theFlags.mFlags[kAITileForest] == true) {
                        theTarget->Dereference();
                        theTarget = NULL;
                    }
                }
                if (theTarget != NULL &&
                    !(theTarget->GetTileNode() != NULL && theTarget->GetComplexUnit()->GetIsDisabled() && !theTarget->GetComplexUnit()->IsProtected())) {
                    if (EEUUGetIsUnitClassTargetable(NULL, theTarget->GetUnitClass())) {
                        EEPlayer* theOwner = theTarget->GetPlayer();
                        EEUnitClass* theClass = theTarget->GetUnitClass();
                        EEComplexUnit* theWorker = theGroup->FindUnitWithFlags(kUnitKindCitizen);
                        EEComplexUnit* theOtherWorker = theGroup->FindUnitWithFlags(kUnitKindNavalCitizen);
                        if ((theWorker != NULL && theWorker->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), false, false)) ||
                            (theOtherWorker != NULL && theOtherWorker->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), false, false))) {
                            if (theOwner != thePlayer)
                                theMessage = new EEMGatherResource(theTarget->GetUnitID(), theShift);
                            else if (theFlag && theClass->IsLandBuilding() && theTarget->GetHitPoints() < theTarget->GetStats()->GetMaxHitPoints())
                                theMessage = new EEMRepairUnit(theTarget->GetUnitID(), theShift);
                            else if (theClass->IsLandBuilding()) {
                                if (theGroup->GetNumUnits() > 1 && theTarget->GetUnitClass()->GetUnitType() == kUnitTypeFarm)
                                    EEGetFarmDropSites(theTarget->GetComplexUnit(), theFarmers);
                                if (theFarmers.size() == 1)
                                {
                                    EEUnit* theFarmer = theFarmers[0];
                                    theMessage = new EEMSmartFarmers(theFarmer->GetUnitID(), theShift);
                                }
                                else
                                    theMessage = new EEMGatherResource(theTarget->GetUnitID(), theShift);
                            }
                        } else if (theOwner != thePlayer && thePlayer->GetEnemy(theOwner->GetPlayerID()) != 0) {
                            if (theTarget->GetResourceType() != kResourceNone && theTarget->GetPlayer()->GetPlayerID() == 0 && theTarget->GetResourceAmount() > 0 &&
                                (!theCanDrop || theTarget->GetUnitClass()->GetAnimalID() == 1000)) {
                                if (!theFlag && theTarget->GetUnitClass()->GetAnimalID() == 1000) {
                                    if (EEGetFirstSelectedUnit(theSelected)) {
                                        EEUnitClass* theSelectedClass = theSelected->GetUnitClass();
                                        if (theSelectedClass->GetUnitType() == kUnitTypeAirport ||
                                            (((theSelectedClass->GetCategory() == kUnitCategoryNavalBuilding || theSelectedClass->GetCategory() == kUnitCategorySpaceBuilding) &&
                                              theSelectedClass->GetAirTrafficType() != 0) && theTarget->GetUnitClass()->GetFamily() != kUnitFamilyFish))
                                            theMessage = new EEMSetRallyGoal(theSelected->GetUnitID(), GE2DPoint(inX, inY), theTarget->GetUnitID(), theShift);
                                        else
                                            theMessage = new EEMPathTo(inX, inY, inFacing, !theControl, inIsRotating, theShift, false);
                                    }
                                } else
                                    theMessage = new EEMGatherResource(theTarget->GetUnitID(), theShift);
                            } else if (theTarget->GetHitPoints() > 0)
                                theMessage = new EEMAttackUnit(theTarget->GetUnitID(), theShift, false);
                            else
                                theMessage = new EEMPathTo(inX, inY, inFacing, !theControl, inIsRotating, theShift, false);
                        } else if (EEGetFirstSelectedUnit(theSelected) && theSelected != NULL && theSelected->GetUnitClass()->GetIsMedic() &&
                                   theTarget->GetHitPoints() > 0 && theSelected->GetTileNode() != NULL && theTarget->GetTileNode() != NULL &&
                                   EECanHeal(theSelected->GetComplexUnit(), theTarget->GetComplexUnit())) {
                            theMessage = new EEMHealUnit(theTarget->GetUnitID());
                        } else if (theOwner == thePlayer) {
                            if (theFlag && theClass->IsLandBuilding() && theTarget->GetHitPoints() < theTarget->GetStats()->mMaxHitPoints)
                                theMessage = new EEMRepairUnit(theTarget->GetUnitID(), theShift);
                            else if (theClass->IsDropSite() ||
                                     ((theClass->GetUnitType() == kUnitTypeDock || theClass->GetUnitType() == kUnitTypeNavyYard) && theGroup->CountUnitsOfKind(3) < theCount)) {
                                if (theClass->GetUnitType() != kUnitTypeGranaryImproved && theClass->GetUnitType() != kUnitTypeGranary)
                                    theMessage = new EEMDropoffResource(theTarget->GetUnitID());
                                else
                                    theMessage = new EEMSmartFarmers(theTarget->GetUnitID(), theShift);
                            } else {
                                unsigned long theLoad = 0;
                                if ((theGroupFlags & kUnitKindHelicopterTransport) == kUnitKindHelicopterTransport)
                                    theLoad = 0x80;
                                else if ((theGroupFlags & kUnitKindLandTransport) == kUnitKindLandTransport)
                                    theLoad = 0x100;
                                else if ((theGroupFlags & kUnitKindNavalTransport) == kUnitKindNavalTransport)
                                    theLoad = 0x200;
                                else if ((theGroupFlags & kUnitKindSpaceTransport) == kUnitKindSpaceTransport)
                                    theLoad = 0x10000;
                                else if ((theGroupFlags & kUnitKindFortress) == kUnitKindFortress)
                                    theLoad = 0x4000;
                                if (theLoad != 0)
                                    theMessage = this->CreateLoadTransportMessage(theGroup, theLoad, theTarget->GetUnitID());
                                else if (theClass->IsTransport() && theTarget->GetTileNode() != NULL && theTarget->GetComplexUnit()->GetIsBuilt())
                                    theMessage = new EEMLoadTransport(theTarget->GetUnitID(), -1, 0);
                                else if (theClass->GetUnitType() == kUnitTypePathPoint)
                                    theMoveTo = true;
                                else if (theClass->IsAirBase())
                                    theMessage = new EEMReturnToBase(theTarget->GetUnitID(), theShift);
                                else if (theGroup->Contains(theTarget->GetComplexUnit()) && inMove)
                                    theMoveTo = true;
                                else {
                                    float x, y;
                                    if (theTarget->GetComplexUnit() != NULL) {
                                        x = theTarget->GetComplexUnit()->GetPhysicalModel().GetLocation().GetX();
                                        y = theTarget->GetComplexUnit()->GetPhysicalModel().GetLocation().GetY();
                                    } else {
                                        x = theTarget->GetTileX() + 0.5f;
                                        y = theTarget->GetTileY() + 0.5f;
                                    }
                                    if (!::gWorld.GetMap()->GetTerrainMesh().GetIsValidTile((unsigned long)(long)x, (unsigned long)(long)y)) {
                                        x = theTarget->GetTileX() + 0.5f;
                                        y = theTarget->GetTileY() + 0.5f;
                                    }
                                    theMessage = new EEMPathTo(x, y, inFacing, !theControl, inIsRotating, theShift, false);
                                }
                            }
                        } else {
                            float x, y;
                            if (theTarget->GetComplexUnit() != NULL) {
                                x = theTarget->GetComplexUnit()->GetPhysicalModel().GetLocation().GetX();
                                y = theTarget->GetComplexUnit()->GetPhysicalModel().GetLocation().GetY();
                            } else {
                                x = theTarget->GetTileX() + 0.5f;
                                y = theTarget->GetTileY() + 0.5f;
                            }
                            theMessage = new EEMPathTo(x, y, inFacing, !theControl, inIsRotating, theShift, false);
                        }
                    }
                } else if (inMove)
                    theMoveTo = true;
            }
            if (theMoveTo)
                theMessage = new EEMPathTo(inX, inY, inFacing, !theControl, inIsRotating, theShift, false);
        } else if (inMove)
            theMessage = new EEMPathTo(inX, inY, inFacing, !theControl, inIsRotating, theShift, inForceMove);
        EEGraphicEffect* theGraphicEffect;
        if (theTarget != NULL && (theTarget->GetHitPoints() > 0 ||
            (theTarget->GetResourceAmount() != 0 && theTarget->GetUnitClass()->GetCategory() != kUnitCategoryLandCitizen && theTarget->GetUnitClass()->GetCategory() != kUnitCategoryNavalCitizen)))
            theGraphicEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMouseClickUnit);
        else
            theGraphicEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMouseClickNoUnit);
        theLocation.SetX(inX);
        theLocation.SetY(inY);
        if (::gWorld.GetMap()->GetTerrainMesh().GetIsValidTile((unsigned long)(long)inX, (unsigned long)(long)inY))
            theLocation.SetZ(::gWorld.GetMap()->GetTerrainMesh().GetMeshZ(inX, inY));
        else
            theLocation.SetZ(0.0f);
        if (theLocation.GetZ() < 0.0f)
            theLocation.SetZ(0.0f);
        gEffectManager->CreateEffect(theTarget, theLocation, theGraphicEffect, &theHandle, 0);
        EEEffect* theEffect = gEffectManager->GetEffect(theHandle);
        theEffect->SetIsLocalOnly(true);
        theEffect->SetPlayer(thePlayer->GetPlayerID());
        theEffect->SetIsSaved(false);
        if (theTarget != NULL)
            theTarget->Dereference();
        if (theMessage != NULL) {
            this->SetMessage(theMessage);
            gUserInterface->PostAction(this);
        }
    }
}

// FUNCTION: EXE 0x4041cf
// Creates the message that drops off carried resources at a building
// (EEMDropoffResource's inline ctor, first user Virtual0C)

// FUNCTION: EXE 0x4041fc
// Creates the message that heals a unit
// (EEMHealUnit's inline ctor, first user Virtual0C)

// FUNCTION: EXE 0x404229
// Returns whether the unit class is a building that handles aircraft (an air base or carrier)
// (EEUnitClass::IsAirBase: an out-of-line COMDAT after its first user Virtual0C) the aircraft carrier test
bool EEUnitClass::IsAirBase()
{
    return this->mAirTrafficType != 0 && (this->mCategory == kUnitCategoryBuilding || this->mCategory == kUnitCategoryNavalBuilding || this->mCategory == kUnitCategorySpaceBuilding);
}

// FUNCTION: EXE 0x404249
// Returns whether units can drop resources off at the unit class
// (an EEUnitClass predicate, out of line after its first user Virtual0C)
bool EEUnitClass::IsDropSite()
{
    return this->mUnitType == kUnitTypeMechZeusPrototype ||
           this->IsLandBuilding() && this->mUnitType != kUnitTypeFarm && this->mUnitType != kUnitTypeDock && this->mUnitType != kUnitTypeNavyYard && this->mResourceType > kResourceNone && this->mResourceType < 7;
}

// FUNCTION: EXE 0x404292
// Returns whether the unit class is a land, helicopter or naval transport
// (EEUnitClass::IsTransport: out of line after its first user Virtual0C) the transport categories
bool EEUnitClass::IsTransport()
{
    return this->mCategory == kUnitCategoryLandTransport || this->mCategory == kUnitCategoryHelicopterTransport || this->mCategory == kUnitCategoryNavalTransport || this->mCategory == kUnitCategorySpaceTransport || this->mCategory == kUnitCategoryFortress;
}

// FUNCTION: EXE 0x4042b8
// Looks up a key in a long-to-pointer map
// (a map<long, long*> lookup accessor: inline, first user Virtual0C)

// FUNCTION: EXE 0x4042e2
// Returns whether the tile is visible to a player (or the player sees everything)
// (EETile::IsVisibleTo: out of line after its first user Virtual0C) the player's visibility count byte, or the
// world made visible to it
bool EETile::IsVisibleTo(EEPlayer* inPlayer)
{
    return (this->mVisibility[inPlayer->mPlayerID >> 1] >> ((inPlayer->mPlayerID & 1) << 4) & 0xff) > 0 ||
           inPlayer->mSeesAll;
}

// FUNCTION: EXE 0x40431c
// Returns the tile's number of doppelganger units
// (EETile::GetNumDoppelgangers: inline, first user Virtual0C)

// FUNCTION: EXE 0x40432f
// Returns the graphic effect at an index, or NULL past the end of the table
// (EEGraphicEffect::Get: out of line after its first user Virtual0C) 0 past the end of the table
EEGraphicEffect* EEGraphicEffect::Get(unsigned long inIndex)
{
    if (inIndex < EEGraphicEffect::sEffects.size())
        return EEGraphicEffect::sEffects[inIndex];
    return NULL;
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAMouseClick::Virtual8()
{
}
