// C:\EEX\Empire Earth\Communications\EEMessage.cpp (__FILE__ 0x4bac84). The game's network messages: 0xb8cda..0xca90f.
// Unit flags: the exe's module set. Members per class in the original's order (mostly alphabetical: ctor, Execute,
// ExportMessage, ExportSize, GetMessageLabel, ImportMessage, IsResponse), classes alphabetical. The import ctors
// (`EEMxxx(unsigned char*)`) and some labels are inline in ee/EEMessage.h: their out-of-line copies follow the message
// factory (0xba41b..0xbadd0; SYNTHETIC markers at the end). ESVERIFY_AT carries the original's __LINE__.
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/U2DPoint.h"
#include "lle/UIEvent.h"
#include "lle/UResourceFile.h"
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
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMouse.h"
#include "ee/EEMiniMap.h"
#include "ee/EECheatCodes.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEAction.h"
#include "ee/EEAAddChatMessage.h"
#include "ee/EEUIEProcessTimerExpired.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEFormation.h"
#include "ee/EEPhysics.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGPatrol.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEActiveGroup.h"
#include "ee/EEUGExplore.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUCTransport.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGameSound.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
static const float kDisbandRefund = 0.75f;   // EEMDisband (its own .rdata word: a named constant)
static unsigned char CompressUnits(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer);
static void DecompressUnits(unsigned char* inBuffer, unsigned char inSize, std::vector<long>& outUnits);
#include "LostDebug.h"                      // NOT original source: the EE_LOST_DEBUG stand-in (0x4b8f8e)
// FUNCTION: EXE 0x4b8cda
// Registers the game's message factory for every game message type (0x41..0xfd) with the network engine
void EEMessage::RegisterKnownMessages()
{
    long i;
    for (i = 0x41; i < 0xfe; i++)
        NEMessage::sConstructors[i] = EEMessage::BuildEEMessage;
}

// FUNCTION: EXE 0x4b8cf7
// Executes a selection message under the fast-selection lock and records its selection id for the sending player;
// for the local player whose id is current, copies the result into the selected group; clears the player's JIT flag
void EEMessage::ExecuteSelectionMessage()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer) {
        if (gCommunications->IsServer()) {
            static_cast<EEServer*>(gCommunications)->mFastSelectionLock->Enter();
            if (thePlayer->mPlayerID == gWorld.GetCurrentPlayer()->mPlayerID) {
                this->Execute();
                if (this->mFastSelectionID == static_cast<EEServer*>(gCommunications)->mFastSelectionID) {
                    thePlayer->mSelectedGroup->CopySelection(thePlayer->mActiveGroup);
                    thePlayer->SetSelectedViewedUnit(thePlayer->mViewedUnit);
                }
            } else
                this->Execute();
            static_cast<EEServer*>(gCommunications)->mFastSelection[(unsigned char)thePlayer->mPlayerID] = this->mFastSelectionID;
            static_cast<EEServer*>(gCommunications)->mFastSelectionLock->Leave();
        } else {
            static_cast<EEClient*>(gCommunications)->mFastSelectionLock->Enter();
            if (thePlayer->mPlayerID == gWorld.GetCurrentPlayer()->mPlayerID) {
                gWorld.mExecutingLocalSelection = true;
                this->Execute();
                gWorld.mExecutingLocalSelection = false;
                if (this->mFastSelectionID == static_cast<EEClient*>(gCommunications)->mFastSelectionID) {
                    thePlayer->mSelectedGroup->CopySelection(thePlayer->mActiveGroup);
                    thePlayer->SetSelectedViewedUnit(thePlayer->mViewedUnit);
                }
            } else
                this->Execute();
            static_cast<EEClient*>(gCommunications)->mFastSelection[(unsigned char)thePlayer->mPlayerID] = this->mFastSelectionID;
            static_cast<EEClient*>(gCommunications)->mFastSelectionLock->Leave();
        }
        if (thePlayer->mJITManufacturing) {
            thePlayer->mJITManufacturing = false;
            if (thePlayer == gWorld.GetCurrentPlayer())
                gUserInterface->UpdateJITManufacturing();
        }
    }
}

// FUNCTION: EXE 0x4b8e86
// Executes a command message if its selection id matches the sender's last selection; otherwise resets the sender's
// active group to the selection (Game Restarting always runs; on clients Migration Slot Info too)
void EEMessage::ExecuteMessage()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer) {
        if (gCommunications->IsServer()) {
            static_cast<EEServer*>(gCommunications)->mFastSelectionLock->Enter();
            unsigned char theSelectionID = static_cast<EEServer*>(gCommunications)->mFastSelection[(unsigned char)thePlayer->mPlayerID];
            this->GetMessageLabel();
            if (this->mFastSelectionID != theSelectionID && this->mType != 0x91) {
                thePlayer->UnselectAllUnits();
                thePlayer->mSelectedGroup->CopySelection(thePlayer->mActiveGroup);
            } else
                this->Execute();
            static_cast<EEServer*>(gCommunications)->mFastSelectionLock->Leave();
        } else {
            static_cast<EEClient*>(gCommunications)->mFastSelectionLock->Enter();
            unsigned char theSelectionID = static_cast<EEClient*>(gCommunications)->mFastSelection[(unsigned char)thePlayer->mPlayerID];
            this->GetMessageLabel();
            if (this->mFastSelectionID != theSelectionID && this->mType != 0x91 && this->mType != 0x7c) {
                thePlayer->UnselectAllUnits();
                thePlayer->mSelectedGroup->CopySelection(thePlayer->mActiveGroup);
            } else
                this->Execute();
            static_cast<EEClient*>(gCommunications)->mFastSelectionLock->Leave();
        }
    }
}

// FUNCTION: EXE 0x4b8f8e
// Creates the game message object for a received buffer from its type byte (NULL and an assert for an unknown type)
NEMessage* EEMessage::BuildEEMessage(unsigned char* inMemory)
{
    unsigned char theType = inMemory[1];
    NEMessage* theMessage = NULL;
    ESVERIFY_AT(inMemory != NULL, 841);
    switch (theType) {
    case kMessageAddAndRemoveUnitsFromSelectedGroup:
        theMessage = new EEMAddAndRemoveUnitsFromSelectedGroup(inMemory);
        break;
    case kMessageAddPatrolPoint:
        theMessage = new EEMAddPatrolPoint(inMemory);
        break;
    case kMessageAddRemoveResources:
        theMessage = new EEMAddRemoveResources(inMemory);
        break;
    case kMessageAddUnitsToSelectedGroup:
        theMessage = new EEMAddUnitsToSelectedGroup(inMemory);
        break;
    case kMessageAddUnitToSelectedGroup:
        theMessage = new EEMAddUnitToSelectedGroup(inMemory);
        break;
    case kMessageAlwaysAddUnitToSelectedGroup:
        theMessage = new EEMAlwaysAddUnitToSelectedGroup(inMemory);
        break;
    case kMessageAttackGround:
        theMessage = new EEMAttackGround(inMemory);
        break;
    case kMessageAttackPlayers:
        theMessage = new EEMAttackPlayers(inMemory);
        break;
    case kMessageAttackUnit:
        theMessage = new EEMAttackUnit(inMemory);
        break;
    case kMessageBuildGate:
        theMessage = new EEMBuildGate(inMemory);
        break;
    case kMessageBuildUnit:
        theMessage = new EEMBuildUnit(inMemory);
        break;
    case kMessageBuildWall:
        theMessage = new EEMBuildWall(inMemory);
        break;
    case kMessageCameraLocationInfo:
        theMessage = new EEMCameraLocationInfo(inMemory);
        break;
    case kMessageCastCalamity:
        theMessage = new EEMCastCalamity(inMemory);
        break;
    case kMessageChangeSelectedUnitCollisionSphere:
        theMessage = new EEMChangeSelectedUnitCollisionSphere(inMemory);
        break;
    case kMessageChangeFormation:
        theMessage = new EEMChangeFormation(inMemory);
        break;
    case kMessageChat:
        theMessage = new EEMChat(inMemory);
        break;
    case kMessageCheatCode:
        theMessage = new EEMCheatCode(inMemory);
        break;
    case kMessageCivTimerExpired:
        theMessage = new EEMCivTimerExpired(inMemory);
        break;
    case kMessageCivUpgradesPurchased:
        theMessage = new EEMCivUpgradesPurchased(inMemory);
        break;
    case kMessageCivUpgradesRequested:
        theMessage = new EEMCivUpgradesRequested(inMemory);
        break;
    case kMessageClearAirportCommands:
        theMessage = new EEMClearAirportCommands(inMemory);
        break;
    case kMessageDisband:
        theMessage = new EEMDisband(inMemory);
        break;
    case kMessageDropoffResource:
        theMessage = new EEMDropoffResource(inMemory);
        break;
    case kMessageEdictProposal:
        theMessage = new EEMEdictProposal(inMemory);
        break;
    case kMessageEdictVote:
        theMessage = new EEMEdictVote(inMemory);
        break;
    case kMessageEdictPreProposal:
        theMessage = new EEMEdictPreProposal(inMemory);
        break;
    case kMessageEdictPreProposalResponse:
        theMessage = new EEMEdictPreProposalResponse(inMemory);
        break;
    case kMessageEnterShiftMode:
        theMessage = new EEMEnterShiftMode(inMemory);
        break;
    case kMessageExitShiftMode:
        theMessage = new EEMExitShiftMode(inMemory);
        break;
    case kMessageExplore:
        theMessage = new EEMExplore(inMemory);
        break;
    case kMessageFullGameOptions:
        theMessage = new EEMFullGameOptions(inMemory);
        break;
    case kMessageGameOptions:
        theMessage = new EEMGameOptions(inMemory);
        break;
    case kMessageGameRestarting:
        theMessage = new EEMGameRestarting(inMemory);
        break;
    case kMessageGameSpeed:
        theMessage = new EEMGameSpeed(inMemory);
        break;
    case kMessageGatherResource:
        theMessage = new EEMGatherResource(inMemory);
        break;
    case kMessageGoFanatic:
        theMessage = new EEMGoFanatic(inMemory);
        break;
    case kMessageHackANewUnit:
        theMessage = new EEMHackANewUnit(inMemory);
        break;
    case kMessageHaltSelectedGroup:
        theMessage = new EEMHaltSelectedGroup(inMemory);
        break;
    case kMessageHealUnit:
        theMessage = new EEMHealUnit(inMemory);
        break;
    case kMessageImmediateCargoUnload:
        theMessage = new EEMImmediateCargoUnload(inMemory);
        break;
    case kMessageIncrementEpic:
        theMessage = new EEMIncrementEpic(inMemory);
        break;
    case kMessageIncrementRallyTimer:
        theMessage = new EEMIncrementRallyTimer(inMemory);
        break;
    case kMessageInGameChat:
        theMessage = new EEMInGameChat(inMemory);
        break;
    case kMessageJITManufacturing:
        theMessage = new EEMJITManufacturing(inMemory);
        break;
    case kMessageKillUnit:
        theMessage = new EEMKillUnit(inMemory);
        break;
    case kMessageLagStatus:
        theMessage = new EEMLagStatus(inMemory);
        break;
    case kMessageLoadMainScreen:
        theMessage = new EEMLoadMainScreen(inMemory);
        break;
    case kMessageLoadMainScreenResponse:
        theMessage = new EEMLoadMainScreenResponse(inMemory);
        break;
    case kMessageLoadOrSaveGame:
        theMessage = new EEMLoadOrSaveGame(inMemory);
        break;
    case kMessageLoadOrSaveGameResponse:
        theMessage = new EEMLoadOrSaveGameResponse(inMemory);
        break;
    case kMessageLoadPercent:
        theMessage = new EEMLoadPercent(inMemory);
        break;
    case kMessageLoadUnitData:
        theMessage = new EEMLoadUnitData(inMemory);
        break;
    case kMessageLoadGarrison:
        theMessage = new EEMLoadGarrison(inMemory);
        break;
    case kMessageLoadTransport:
        theMessage = new EEMLoadTransport(inMemory);
        break;
    case kMessageLockUnlock:
        theMessage = new EEMLockUnlock(inMemory);
        break;
    case kMessagePackUnpack:
        theMessage = new EEMPackUnpack(inMemory);
        break;
    case kMessageUnloadTransport:
        theMessage = new EEMUnloadTransport(inMemory);
        break;
    case kMessageMakeWorldVisible:
        theMessage = new EEMMakeWorldVisible(inMemory);
        break;
    case kMessageMigrationSlotInfo:
        theMessage = new EEMMigrationSlotInfo(inMemory);
        break;
    case kMessageNewEdictEnacted:
        theMessage = new EEMNewEdictEnacted(inMemory);
        break;
    case kMessagePathTo:
        theMessage = new EEMPathTo(inMemory);
        break;
    case kMessagePatrol:
        theMessage = new EEMPatrol(inMemory);
        break;
    case kMessagePayTribute:
        theMessage = new EEMPayTribute(inMemory);
        break;
    case kMessageFullPlayerInformation:
        theMessage = new EEMFullPlayerInformation(inMemory);
        break;
    case kMessagePlayerInformation:
        theMessage = new EEMPlayerInformation(inMemory);
        break;
    case kMessagePlayerSlotInfo:
        theMessage = new EEMPlayerSlotInfo(inMemory);
        break;
    case kMessageRecordFrames:
        theMessage = new EEMRecordFrames(inMemory);
        break;
    case kMessageRemoveUnitFromSelectedGroup:
        theMessage = new EEMRemoveUnitFromSelectedGroup(inMemory);
        break;
    case kMessageRepairUnit:
        theMessage = new EEMRepairUnit(inMemory);
        break;
    case kMessageReplantFarms:
        theMessage = new EEMReplantFarms(inMemory);
        break;
    case kMessageRemoveTechnology:
        theMessage = new EEMRemoveTechnology(inMemory);
        break;
    case kMessageRemoveImprovement:
        theMessage = new EEMRemoveImprovement(inMemory);
        break;
    case kMessageResearchTechnology:
        theMessage = new EEMResearchTechnology(inMemory);
        break;
    case kMessageReportStatistics:
        theMessage = new EEMReportStatistics(inMemory);
        break;
    case kMessageResign:
        theMessage = new EEMResign(inMemory);
        break;
    case kMessageReturnToBase:
        theMessage = new EEMReturnToBase(inMemory);
        break;
    case kMessageSavedGroupAddSelection:
        theMessage = new EEMSavedGroupAddSelection(inMemory);
        break;
    case kMessageSavedGroupClear:
        theMessage = new EEMSavedGroupClear(inMemory);
        break;
    case kMessageSavedGroupCreate:
        theMessage = new EEMSavedGroupCreate(inMemory);
        break;
    case kMessageSavedGroupSelect:
        theMessage = new EEMSavedGroupSelect(inMemory);
        break;
    case kMessageScrambleAircraft:
        theMessage = new EEMScrambleAircraft(inMemory);
        break;
    case kMessageSelectCustomPlayerForChat:
        theMessage = new EEMSelectCustomPlayerForChat(inMemory);
        break;
    case kMessageSelectPatrolGroup:
        theMessage = new EEMSelectPatrolGroup(inMemory);
        break;
    case kMessageSelectPlayerChatType:
        theMessage = new EEMSelectPlayerChatType(inMemory);
        break;
    case kMessageSelectTheseUnits:
        theMessage = new EEMSelectTheseUnits(inMemory);
        break;
    case kMessageSelectUnit:
        theMessage = new EEMSelectUnit(inMemory);
        break;
    case kMessageSelectUnits:
        theMessage = new EEMSelectUnits(inMemory);
        break;
    case kMessageSelfDestruct:
        theMessage = new EEMSelfDestruct(inMemory);
        break;
    case kMessageSendFlare:
        theMessage = new EEMSendFlare(inMemory);
        break;
    case kMessageSetAirportCommand:
        theMessage = new EEMSetAirportCommand(inMemory);
        break;
    case kMessageSetDiplomaticStance:
        theMessage = new EEMSetDiplomaticStance(inMemory);
        break;
    case kMessageSetRallyGoal:
        theMessage = new EEMSetRallyGoal(inMemory);
        break;
    case kMessageSetWaterLevel:
        theMessage = new EEMSetWaterLevel(inMemory);
        break;
    case kMessageSmartFarmers:
        theMessage = new EEMSmartFarmers(inMemory);
        break;
    case kMessageStartLoading:
        theMessage = new EEMStartLoading(inMemory);
        break;
    case kMessageStartLoadingResponse:
        theMessage = new EEMStartLoadingResponse(inMemory);
        break;
    case kMessageStopActiveGroup:
        theMessage = new EEMStopActiveGroup(inMemory);
        break;
    case kMessageSnapToDaylight:
        theMessage = new EEMSnapToDaylight(inMemory);
        break;
    case kMessageToggleCollisionDetection:
        theMessage = new EEMToggleCollisionDetection(inMemory);
        break;
    case kMessageToggleDayNight:
        theMessage = new EEMToggleDayNight(inMemory);
        break;
    case kMessageUnselectAllUnits:
        theMessage = new EEMUnselectAllUnits(inMemory);
        break;
    case kMessageUpdatePingStat:
        theMessage = new EEMUpdatePingStat(inMemory);
        break;
    case kMessageImproveUnit:
        theMessage = new EEMImproveUnit(inMemory);
        break;
    case kMessageZoomInfo:
        theMessage = new EEMZoomInfo(inMemory);
        break;
    case kMessageSetUnitBehavior:
        theMessage = new EEMSetUnitBehavior(inMemory);
        break;
    case kMessageExchangeResources:
        theMessage = new EEMExchangeResources(inMemory);
        break;
    default:
        ESVERIFY_AT("Undefined message type" == NULL, 1275);
        break;
    }
    return theMessage;
    EE_LOST_DEBUG(1); }                     // LOST-DEBUG: none: exact only (tools lostdebug_search)

// FUNCTION: EXE 0x4badd0
// Toggles the listed units in the sender's selection (twice while a selection is being replayed)
void EEMAddAndRemoveUnitsFromSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    bool theTwice;
    if ((gCommunications->IsClient() && static_cast<EEClient*>(gCommunications)->mIsExecutingSelection)
        || (gCommunications->IsServer() && static_cast<EEServer*>(gCommunications)->mIsExecutingSelection))
        theTwice = true;
    else
        theTwice = false;
    thePlayer->AddAndRemoveUnitsFromSelectedGroup(this->mUnitIDs);
    if (theTwice)
        thePlayer->AddAndRemoveUnitsFromSelectedGroup(this->mUnitIDs);
}

// FUNCTION: EXE 0x4bae45
// Returns the Add And Remove Units From Selected Group message's debug label
char* EEMAddAndRemoveUnitsFromSelectedGroup::GetMessageLabel() const
{
    return "Remove Units From Selected Group";
}

// FUNCTION: EXE 0x4bae4b
// Adds a patrol point for the sender's selection: inserts it into a selected path point's route, extends the
// first unit's patrol, or starts a new patrol per unit category; with nothing selected, creates a lone patrol group
void EEMAddPatrolPoint::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEComplexUnit* theUnit = theGroup->GetFirstUnit();
    if (theUnit) {
        if (theUnit->mUnitClass->mUnitType == kUnitTypePathPoint)
            theUnit->mPatrolGroup->InsertPatrolPointAt(this->mPoint, thePlayer->mPlayerID, theUnit);
        else {
            EEUGPatrol* theGoal = dynamic_cast<EEUGPatrol*>(theUnit->GetGoal());
            if (theGoal)
                theGoal->mPatrolGroup->AddPoint(this->mPoint, thePlayer->mPlayerID);
            else {
                for (unsigned long i = 1; i < 6; i++) {
                    EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
                    theActiveGroup->Fill(theGroup, 0, i);
                    if (theActiveGroup->GetNumUnits() > 0) {
                        thePlayer->mPendingGroups.push_back(theActiveGroup);
                        theActiveGroup->KeepMobileUnits();
                        theActiveGroup->KeepNonCitizens();
                        EEUGPatrol* theNewGoal = new EEUGPatrol(true);
                        theNewGoal->mPatrolGroup->AddPoint(this->mPoint, thePlayer->mPlayerID);
                        thePlayer->AddPatrolGroup(theNewGoal->mPatrolGroup);
                        theActiveGroup->ClearGoal();
                        theActiveGroup->SetGoal(theNewGoal, false);
                        theActiveGroup->mAction = kActionMoveToNextPatrolPoint;
                        theActiveGroup->Plan();
                        if (theActiveGroup->GetNumUnits() > 0)
                            thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
                    } else
                        theActiveGroup->Dereference();
                }
            }
        }
    } else {
        EEPPatrolGroup* thePatrolGroup = new EEPPatrolGroup();
        thePatrolGroup->AddPoint(this->mPoint, thePlayer->mPlayerID);
        thePlayer->AddPatrolGroup(thePatrolGroup);
        thePatrolGroup->Dereference();
    }
}

// FUNCTION: EXE 0x4bb040
// Writes the Add Patrol Point message (selection id, point) to a new network buffer
void EEMAddPatrolPoint::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPoint, sizeof(this->mPoint));
        theLocation += sizeof(this->mPoint);
        ESVERIFY_AT(theLocation == theMessageSize, 1471);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bb0be
// Returns the Add Patrol Point message's wire size: 13 bytes
unsigned long EEMAddPatrolPoint::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4bb0c2
// Returns the Add Patrol Point message's debug label
char* EEMAddPatrolPoint::GetMessageLabel() const
{
    return "Add Patrol Point";
}

// FUNCTION: EXE 0x4bb0c8
// Reads the Add Patrol Point message (selection id, point) from a network buffer
void EEMAddPatrolPoint::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1520);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPoint, inPayloadPointer + theLocation, sizeof(this->mPoint));
    theLocation += sizeof(this->mPoint);
}

// FUNCTION: EXE 0x4bb118
// Returns false: an Add Patrol Point message answers no other message
bool EEMAddPatrolPoint::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bb11d
// Returns the Add/Remove Resources message's wire size: 18 bytes
unsigned long EEMAddRemoveResources::ExportSize()
{
    return 18;
}

// FUNCTION: EXE 0x4bb121
// Writes the Add/Remove Resources message (selection id, amount, player, resource, add flag) to a new network buffer
void EEMAddRemoveResources::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mAmount, sizeof(this->mAmount));
        theLocation += sizeof(this->mAmount);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mResourceType, sizeof(this->mResourceType));
        theLocation += sizeof(this->mResourceType);
        memcpy(*outPayloadPointer + theLocation, &this->mAdd, sizeof(this->mAdd));
        theLocation += sizeof(this->mAdd);
        ESVERIFY_AT(theLocation == theMessageSize, 1591);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bb1da
// Reads the Add/Remove Resources message (selection id, amount, player, resource, add flag) from a network buffer
void EEMAddRemoveResources::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1601);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mAmount, inPayloadPointer + theLocation, sizeof(this->mAmount));
    theLocation += sizeof(this->mAmount);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mResourceType, inPayloadPointer + theLocation, sizeof(this->mResourceType));
    theLocation += sizeof(this->mResourceType);
    memcpy(&this->mAdd, inPayloadPointer + theLocation, sizeof(this->mAdd));
    theLocation += sizeof(this->mAdd);
}

bool EEMAddRemoveResources::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bb264
// Gives a player an amount of one resource, or takes it if the amount is positive and no more than the stock
void EEMAddRemoveResources::Execute()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayerID];
    if (this->mAdd)
        thePlayer->AddResource(this->mResourceType, this->mAmount);
    else if (this->mAmount > 0 && this->mAmount <= thePlayer->mResourceCaps[this->mResourceType])
        thePlayer->TakeResource(this->mResourceType, this->mAmount);
}

// FUNCTION: EXE 0x4bb2a2
// Toggles one unit in or out of the sender's selection (twice while a selection is being replayed)
void EEMAddUnitToSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    bool theTwice;
    if (theUnit) {
        if ((gCommunications->IsClient() && static_cast<EEClient*>(gCommunications)->mIsExecutingSelection)
            || (gCommunications->IsServer() && static_cast<EEServer*>(gCommunications)->mIsExecutingSelection))
            theTwice = true;
        else
            theTwice = false;
        if (thePlayer->mActiveGroup->Contains(theUnit->GetComplexUnit()))
            thePlayer->RemoveUnitFromSelectedGroup(this->mUnitID);
        else
            thePlayer->AddUnitToSelectedGroup(this->mUnitID, true);
        if (theTwice) {
            if (thePlayer->mActiveGroup->Contains(theUnit->GetComplexUnit()))
                thePlayer->RemoveUnitFromSelectedGroup(this->mUnitID);
            else
                thePlayer->AddUnitToSelectedGroup(this->mUnitID, true);
        }
    }
}

// FUNCTION: EXE 0x4bb377
// Writes the Add Unit To Selected Group message (selection id, unit id) to a new network buffer
void EEMAddUnitToSelectedGroup::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        ESVERIFY_AT(theLocation == theMessageSize, 1730);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMAddUnitToSelectedGroup::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4bb3f5
// Returns the Add Unit To Selected Group message's debug label
char* EEMAddUnitToSelectedGroup::GetMessageLabel() const
{
    return "Add Unit To Selected Group";
}

// FUNCTION: EXE 0x4bb3fb
// Reads the Add Unit To Selected Group message (selection id, unit id) from a network buffer
void EEMAddUnitToSelectedGroup::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1779);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
}

bool EEMAddUnitToSelectedGroup::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bb44b
// Adds the listed units to the sender's selection
void EEMAddUnitsToSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->AddUnitsToSelectedGroup(this->mUnitIDs);
}

// FUNCTION: EXE 0x4bb473
// Returns the Add Units To Selected Group message's debug label
char* EEMAddUnitsToSelectedGroup::GetMessageLabel() const
{
    return "Add Units To Selected Group";
}

// FUNCTION: EXE 0x4bb479
// Adds one unit to the sender's selection if it still exists (never removes it)
void EEMAlwaysAddUnitToSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (gWorld.GetUnit(this->mUnitID))
        thePlayer->AddUnitToSelectedGroup(this->mUnitID, true);
}

// FUNCTION: EXE 0x4bb4b8
// Returns the Always Add Unit To Selected Group message's debug label
char* EEMAlwaysAddUnitToSelectedGroup::GetMessageLabel() const
{
    return "Always Add Units To Selected Group";
}

// FUNCTION: EXE 0x4bb4be
// Orders the sender's ground attackers, split by unit category, to attack a map location; sends each group's
// command and plays an acknowledgement
void EEMAttackGround::Execute()
{
    GE3DPoint theTarget;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEComplexUnit* theUnit = theGroup->GetFirstUnit();
    unsigned long thePlayerID = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mPlayerID;
    if (theUnit) {
        theTarget.SetX(this->mX);
        theTarget.SetY(this->mY);
        theTarget.SetZ(gWorld.GetMap()->GetHeight(theTarget));
        if (theTarget.GetZ() < 0.0f)
            theTarget.SetZ(0.0f);
        for (unsigned long i = 1; i < 6; i++) {
            EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
            theActiveGroup->Fill(theGroup, 0, i);
            if (theActiveGroup->GetNumUnits() > 0) {
                thePlayer->mPendingGroups.push_back(theActiveGroup);
                theActiveGroup->KeepGroundAttackers();
                if (theActiveGroup->GetNumUnits() > 0)
                    theUnit = theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1));
                else
                    theUnit = NULL;
                if (theUnit) {
                    theActiveGroup->ClearGoal();
                    theActiveGroup->SetGoal(new EEUGAttackGround(theTarget, true), false);
                    theActiveGroup->mAction = kActionAttackLocation;
                    theActiveGroup->Plan();
                    EESendGroupCommand(theActiveGroup, NULL, thePlayerID, 0xb9, 0);
                    thePlayer->PlayAcknowledgement(theUnit);
                }
            } else
                theActiveGroup->Dereference();
        }
    }
}

// FUNCTION: EXE 0x4bb666
// Writes the Attack Ground message (selection id, x, y) to a new network buffer
void EEMAttackGround::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mX, sizeof(this->mX));
        theLocation += sizeof(this->mX);
        memcpy(*outPayloadPointer + theLocation, &this->mY, sizeof(this->mY));
        theLocation += sizeof(this->mY);
        ESVERIFY_AT(theLocation == theMessageSize, 1989);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMAttackGround::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4bb6f7
// Reads the Attack Ground message (selection id, x, y) from a network buffer
void EEMAttackGround::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2025);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mX, inPayloadPointer + theLocation, sizeof(this->mX));
    theLocation += sizeof(this->mX);
    memcpy(&this->mY, inPayloadPointer + theLocation, sizeof(this->mY));
    theLocation += sizeof(this->mY);
}

bool EEMAttackGround::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bb75e
// Runs the attack-players order (EEAttackPlayers) for the message's player and allies flag
void EEMAttackPlayers::Execute()
{
    EEAttackPlayers(this->mPlayerID, this->mAllies);
}

// FUNCTION: EXE 0x4bb76f
// Writes the Attack Players message (selection id, player, allies flag) to a new network buffer
void EEMAttackPlayers::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mAllies, sizeof(this->mAllies));
        theLocation += sizeof(this->mAllies);
        ESVERIFY_AT(theLocation == theMessageSize, 2104);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bb7fe
// Returns the Attack Players message's wire size: 10 bytes
unsigned long EEMAttackPlayers::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4bb802
// Reads the Attack Players message (selection id, player, allies flag) from a network buffer
void EEMAttackPlayers::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2140);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mAllies, inPayloadPointer + theLocation, sizeof(this->mAllies));
    theLocation += sizeof(this->mAllies);
}

bool EEMAttackPlayers::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bb869
// Orders the sender's selection, split by unit category and spread around the target, to attack (or convert)
// a living unit, queued or not; sends each group's command and plays an acknowledgement
void EEMAttackUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEUnit* theTarget = gWorld.GetUnit(this->mTargetID);
    EEComplexUnit* theUnit = theGroup->GetFirstUnit();
    unsigned long thePlayerID = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mPlayerID;
    if (theUnit && theTarget && theTarget->mHitPoints > 0) {
        for (unsigned long i = 1; i < 6; i++) {
            EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
            theActiveGroup->Fill(theGroup, 1, i);
            theActiveGroup->mIsQueued = this->mIsQueued;
            if (theActiveGroup->GetNumUnits() > 0) {
                thePlayer->mPendingGroups.push_back(theActiveGroup);
                theActiveGroup->SpreadAroundUnit(theTarget, theTarget->mTileX, theTarget->mTileY);
                if (theActiveGroup->GetNumUnits() > 0)
                    theUnit = theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1));
                else
                    theUnit = NULL;
                if (theUnit) {
                    theActiveGroup->ClearGoal();
                    theActiveGroup->SetGoal(new EEUGAttackUnit(theGroup->GetFirstUnit(), theTarget, 1, this->mIsConvert), this->mIsQueued);
                    theActiveGroup->mAction = kActionAttackEnemyUnit;
                    theActiveGroup->Plan();
                    EESendGroupCommand(theActiveGroup, theTarget, thePlayerID, 0xb9, 0);
                    thePlayer->PlayAcknowledgement(theUnit);
                }
            } else
                theActiveGroup->Dereference();
        }
    }
}

// FUNCTION: EXE 0x4bba1f
// Writes the Attack Unit message (selection id, target, queued and convert flags) to a new network buffer
void EEMAttackUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        memcpy(*outPayloadPointer + theLocation, &this->mIsConvert, sizeof(this->mIsConvert));
        theLocation += sizeof(this->mIsConvert);
        ESVERIFY_AT(theLocation == theMessageSize, 2292);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bbabf
// Returns the Attack Unit message's wire size: 11 bytes
unsigned long EEMAttackUnit::ExportSize()
{
    return 11;
}

// FUNCTION: EXE 0x4bbac3
// Returns the Attack Unit message's debug label
char* EEMAttackUnit::GetMessageLabel() const
{
    return "Attack Unit";
}

// FUNCTION: EXE 0x4bbac9
// Reads the Attack Unit message (selection id, target, queued and convert flags) from a network buffer
void EEMAttackUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2343);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
    memcpy(&this->mIsConvert, inPayloadPointer + theLocation, sizeof(this->mIsConvert));
    theLocation += sizeof(this->mIsConvert);
}

bool EEMAttackUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bbb3f
// Builds a gate for a player into the wall at the message's tile
void EEMBuildGate::Execute()
{
    EETileCoordinate theTile;
    theTile.mX = (long)this->mPoint.GetX();
    theTile.mY = (long)this->mPoint.GetY();
    EEBuildGate(theTile, this->mPlayerID, this->mTechnology, this->mSide);
}

// FUNCTION: EXE 0x4bbb75
// Writes the Build Gate message (selection id, point, technology, side, player) to a new network buffer
void EEMBuildGate::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPoint, sizeof(this->mPoint));
        theLocation += sizeof(this->mPoint);
        memcpy(*outPayloadPointer + theLocation, &this->mTechnology, sizeof(this->mTechnology));
        theLocation += sizeof(this->mTechnology);
        memcpy(*outPayloadPointer + theLocation, &this->mSide, sizeof(this->mSide));
        theLocation += sizeof(this->mSide);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        ESVERIFY_AT(theLocation == theMessageSize, 2433);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bbc30
// Returns the Build Gate message's wire size: 25 bytes
unsigned long EEMBuildGate::ExportSize()
{
    return 25;
}

// FUNCTION: EXE 0x4bbc34
// Returns the Build Gate message's debug label
char* EEMBuildGate::GetMessageLabel() const
{
    return "Build Gate";
}

// FUNCTION: EXE 0x4bbc3a
// Reads the Build Gate message (selection id, point, technology, side, player) from a network buffer
void EEMBuildGate::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2485);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPoint, inPayloadPointer + theLocation, sizeof(this->mPoint));
    theLocation += sizeof(this->mPoint);
    memcpy(&this->mTechnology, inPayloadPointer + theLocation, sizeof(this->mTechnology));
    theLocation += sizeof(this->mTechnology);
    memcpy(&this->mSide, inPayloadPointer + theLocation, sizeof(this->mSide));
    theLocation += sizeof(this->mSide);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
}

bool EEMBuildGate::IsResponse(NEMessage* inMessage)
{
    return false;
}

// Orders farms for EEMBuildUnit::Execute: built ones first, then paid-for ones, then the nearer to the tile. Inline
// (header-declared); its COMDAT 0x4c995c lands among this unit's sort COMDATs after its first user, the sort
// in EEMBuildUnit::Execute
inline bool EETileDistanceLess::operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
{
    if (inFirst->mIsBuilt) {
        if (!inSecond->mIsBuilt)
            return true;
        return this->DistanceSquared(inFirst->GetTile()) < this->DistanceSquared(EETileCoordinate(inSecond->mTileX, inSecond->mTileY));
    }
    if (inSecond->mIsBuilt)
        return false;
    if (inFirst->mIsPaidFor) {
        if (!inSecond->mIsPaidFor)
            return true;
        return this->DistanceSquared(inFirst->GetTile()) < this->DistanceSquared(inSecond->GetTile());
    }
    if (inSecond->mIsPaidFor)
        return false;
    return this->DistanceSquared(inFirst->GetTile()) < this->DistanceSquared(EETileCoordinate(inSecond->mTileX, inSecond->mTileY));
}

// FUNCTION: EXE 0x4bbcc4
// Places a building for the sender where allowed and orders the selected citizens to build it (queued or at once);
// for a granary, also hands them the free farms nearby, nearest first; plays an acknowledgement
void EEMBuildUnit::Execute()
{
    std::vector<EEComplexUnit*> theNearUnits;
    std::vector<EEUnit*> theMovedUnits;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    long theCount = 1;
    EEActiveGroup* theActiveGroup = NULL;
    EEComplexUnit* theUnit;
    if (theGroup->GetFirstUnit()) {
        theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 0, 0);
        thePlayer->mPendingGroups.push_back(theActiveGroup);
        theActiveGroup->KeepCitizens();
        if (theActiveGroup->GetFirstUnit()) {
            long theUnitType = thePlayer->mTechnologies->Get(this->mTechnology)->mData->mUnitClass;
            if (theUnitType != kUnitTypeCapitol || thePlayer->mNumCapitols == 0) {
                if (thePlayer->CanProduce(this->mTechnology, 0x20, 1, true, 3, NULL)) {
                    if (EECanPlaceOnTile(thePlayer, theUnitType, (long)this->mPoint.GetX(), (long)this->mPoint.GetY(), 0, 1)) {
                        EEUnit* theBuilding = EEMakeBuilding(thePlayer, theUnitType, (long)this->mPoint.GetX(), (long)this->mPoint.GetY());
                        if (theUnitType == kUnitTypeGranary || theUnitType == kUnitTypeGranaryImproved) {
                            do {
                                theUnit = EEFindFreeFarm(theBuilding->GetComplexUnit(), 1, &theCount);
                                if (theUnit)
                                    theNearUnits.push_back(theUnit);
                            } while (theUnit);
                            if (theNearUnits.size() > 0)
                                std::sort(theNearUnits.begin(), theNearUnits.end(),
                                          EETileDistanceLess(theActiveGroup->GetFirstUnit()->GetTile()));
                        }
                        unsigned long i = 0;
                        unsigned long j = 0;
                        for (; i < theActiveGroup->GetNumUnits(); i++, j++) {
                            unsigned long theNumUnits = theActiveGroup->GetNumUnits();
                            EEComplexUnit* theWorker = theActiveGroup->GetUnit(i);
                            if (this->mIsQueued) {
                                theWorker->AddBuildTarget(theBuilding);
                                if (theWorker->mGoalState && theWorker->mGoalState->mKind != kGoalBuildRepair) {
                                    theWorker->StartAction(kActionIdle);
                                    theWorker->mUnitClass->ActionChanged(theWorker);
                                }
                            } else
                                theWorker->OrderRepair(theBuilding);
                            if (theUnitType == kUnitTypeGranary || theUnitType == kUnitTypeGranaryImproved) {
                                if (j < theNearUnits.size())
                                    theWorker->AddBuildTarget(theNearUnits[j]);
                                else if (theNearUnits.size() > 0)
                                    theWorker->AddBuildTarget(theNearUnits[gPhysics->GetRandom(0L, (long)theNearUnits.size() - 1)]);
                            }
                            if (theActiveGroup->GetNumUnits() != theNumUnits) {
                                i--;
                                theMovedUnits.push_back(theWorker);
                            }
                        }
                        for (i = 0; i < theMovedUnits.size(); i++)
                            theActiveGroup->AddMember(theMovedUnits[i]->mComplexUnit, true);
                        if (theActiveGroup->GetNumUnits() > 0)
                            thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
                    }
                }
            }
        }
    }
}

// FUNCTION: EXE 0x4bc00c
// Writes the Build Unit message (selection id, point, technology, queued flag) to a new network buffer
void EEMBuildUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPoint, sizeof(this->mPoint));
        theLocation += sizeof(this->mPoint);
        memcpy(*outPayloadPointer + theLocation, &this->mTechnology, sizeof(this->mTechnology));
        theLocation += sizeof(this->mTechnology);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        ESVERIFY_AT(theLocation == theMessageSize, 2786);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMBuildUnit::ExportSize()
{
    return 18;
}

// FUNCTION: EXE 0x4bc0ae
// Returns the Build Unit message's debug label
char* EEMBuildUnit::GetMessageLabel() const
{
    return "Build Unit";
}

// FUNCTION: EXE 0x4bc0b4
// Reads the Build Unit message (selection id, point, technology, queued flag) from a network buffer
void EEMBuildUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2837);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPoint, inPayloadPointer + theLocation, sizeof(this->mPoint));
    theLocation += sizeof(this->mPoint);
    memcpy(&this->mTechnology, inPayloadPointer + theLocation, sizeof(this->mTechnology));
    theLocation += sizeof(this->mTechnology);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
}

bool EEMBuildUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bc12c
// Plans a wall from start to end for a player and orders the selected citizens to build its pieces
void EEMBuildWall::Execute()
{
    std::vector<EEUnit*> thePieces;
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayerID];
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEActiveGroup* theActiveGroup = NULL;
    if (theGroup->GetFirstUnit()) {
        theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 0, 0);
        thePlayer->mPendingGroups.push_back(theActiveGroup);
        theActiveGroup->KeepCitizens();
        if (theActiveGroup->GetFirstUnit()) {
            EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(this->mTechnology)->mData->mUnitClass];
            EEPlanWall(this->mStart, this->mEnd, theClass, thePlayer, 1, 0, thePieces);
            if (theActiveGroup->GetNumUnits() > 0)
                thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
        }
        if (thePieces.size() > 0)
            EEBuildWall(theActiveGroup, thePieces);
    }
}

// FUNCTION: EXE 0x4bc262
// Writes the Build Wall message (selection id, start, end, technology, player) to a new network buffer
void EEMBuildWall::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mStart, sizeof(this->mStart));
        theLocation += sizeof(this->mStart);
        memcpy(*outPayloadPointer + theLocation, &this->mEnd, sizeof(this->mEnd));
        theLocation += sizeof(this->mEnd);
        memcpy(*outPayloadPointer + theLocation, &this->mTechnology, sizeof(this->mTechnology));
        theLocation += sizeof(this->mTechnology);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        ESVERIFY_AT(theLocation == theMessageSize, 2969);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bc31d
// Returns the Build Wall message's wire size: 29 bytes
unsigned long EEMBuildWall::ExportSize()
{
    return 29;
}

// FUNCTION: EXE 0x4bc321
// Returns the Build Wall message's debug label
char* EEMBuildWall::GetMessageLabel() const
{
    return "Build Wall";
}

// FUNCTION: EXE 0x4bc327
// Reads the Build Wall message (selection id, start, end, technology, player) from a network buffer
void EEMBuildWall::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 3021);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mStart, inPayloadPointer + theLocation, sizeof(this->mStart));
    theLocation += sizeof(this->mStart);
    memcpy(&this->mEnd, inPayloadPointer + theLocation, sizeof(this->mEnd));
    theLocation += sizeof(this->mEnd);
    memcpy(&this->mTechnology, inPayloadPointer + theLocation, sizeof(this->mTechnology));
    theLocation += sizeof(this->mTechnology);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
}

bool EEMBuildWall::IsResponse(NEMessage* inMessage)
{
    return false;
}

void EEMCameraLocationInfo::Execute()
{
}

// FUNCTION: EXE 0x4bc3b1
// Writes the Camera Location Info message (selection id, camera location) to a new network buffer
void EEMCameraLocationInfo::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mLocation, sizeof(this->mLocation));
        theLocation += sizeof(this->mLocation);
        ESVERIFY_AT(theLocation == theMessageSize, 3118);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bc42f
// Returns the Camera Location Info message's wire size: 17 bytes
unsigned long EEMCameraLocationInfo::ExportSize()
{
    return 17;
}

// FUNCTION: EXE 0x4bc433
// Reads the Camera Location Info message (selection id, camera location) from a network buffer
void EEMCameraLocationInfo::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 3165);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mLocation, inPayloadPointer + theLocation, sizeof(this->mLocation));
    theLocation += sizeof(this->mLocation);
}

bool EEMCameraLocationInfo::IsResponse(NEMessage* inMessage)
{
    return false;
}

#include "lle/USingleton.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x4bc483
// Starts a calamity or power for the sender's selected casters: one able caster (or all for teleport and
// all-cast powers) gets the cast goal; calamity class 0x12 gives every selected caster a copy of the goal
void EEMCastCalamity::Execute()
{
    unsigned long i; GE3DPoint theLocation;
    EEUnit* theTarget = NULL;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    bool theCast = false;
    EESelectedGroup* theSelection = thePlayer->mActiveGroup;
    EEComplexUnit* theCaster = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(this->mCasterID));
    if (theCaster == NULL || theCaster->mHitPoints <= 0)
        return;
    EETechTreeNode* theNode = thePlayer->mTechnologies->Get(this->mTechnology);
    if (theNode == NULL)
        return;
    EECalamityClass* theClass =
        EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity);
    if (this->mTargetID != -1)
        theTarget = gWorld.GetUnit(this->mTargetID);
    if (theClass->mClassEnum == 0x12) {
        if (theClass->CanCastAt(theCaster, theLocation, this->mTileX, this->mTileY, true, theTarget) != 0)
            return;
        EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
        theGroup->Fill(theSelection, 0, 0);
        theGroup->SplitCalamityCasters(theClass, theNode, theTarget);
        if (theGroup->GetNumUnits() > 0) {
            std::vector<EEUnit*> theUnits;
            EEUGCastCalamity* theGoal = new EEUGCastCalamity(theCaster, theTarget, this->mTileX, this->mTileY, this->mTechnology, true);
            for (i = 0; i < theGroup->GetNumUnits(); i++)
                theUnits.push_back(theGroup->GetUnit(i));
            for (std::vector<EEUnit*>::iterator theIterator = theUnits.begin(); theIterator != theUnits.end();
                 theIterator++) {
                EEComplexUnit* theUnit = (*theIterator)->mComplexUnit;
                theGroup->RemoveUnit(theUnit);
                theUnit->ClearGoals(0);
                theUnit->SetGoal(theGoal->Clone(), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
            delete theGoal;
            theGroup->Dereference();
        }
        return;
    }
    EEActiveGroup* theGroup = new EEActiveGroup(theCaster->mPlayer);
    if (this->mTechnology != kTechTeleport && !theClass->mIsCastByAllSelected) {
        unsigned long theCount = theSelection->GetNumUnits();
        for (i = 0; i < theCount && !theCast; i++) {
            EEComplexUnit* theUnit = theSelection->GetUnit(i);
            long theResult = theClass->CanCastAt(theUnit, theLocation, this->mTileX, this->mTileY, true, theTarget);
            if (theUnit->mHitPoints > 0 && theResult == 0 && theNode->Contains(theUnit->mUnitClass->mUnitType)
                && theUnit->mPower >= theClass->GetCost()) {
                theUnit->SetGoal(new EEUGCastCalamity(theUnit, theTarget, this->mTileX, this->mTileY, this->mTechnology, true), 0, 0);
                if (theUnit->mUnitClass->mUnitType == kUnitTypeCapitol || this->mTechnology == kTechConquistadorsSpell) theUnit->StartAction(kActionCalamityPreCast); else theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
                theGroup->AddMember(theUnit, true);
                theCast = true;
            }
        }
    } else {
        for (i = 0; i < theSelection->GetNumUnits(); i++) {
            EEComplexUnit* theUnit = theSelection->GetUnit(i);
            if (theUnit->mHitPoints > 0 && theNode->Contains(theUnit->mUnitClass->mUnitType)
                && theClass->CanCastAt(theUnit, theLocation, this->mTileX, this->mTileY, true, theTarget) == 0) {
                theUnit->SetGoal(new EEUGCastCalamity(theUnit, theTarget, this->mTileX, this->mTileY, this->mTechnology, true), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
                theGroup->AddMember(theUnit, true);
            }
        }
    }
    if (theGroup != NULL)
        thePlayer->mPendingGroups.push_back(theGroup);
}

// FUNCTION: EXE 0x4bc8c2
// Writes the Cast Calamity message (selection id, caster, target, tile, technology) to a new network buffer
void EEMCastCalamity::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mCasterID, sizeof(this->mCasterID));
        theLocation += sizeof(this->mCasterID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mTileX, sizeof(this->mTileX));
        theLocation += sizeof(this->mTileX);
        memcpy(*outPayloadPointer + theLocation, &this->mTileY, sizeof(this->mTileY));
        theLocation += sizeof(this->mTileY);
        memcpy(*outPayloadPointer + theLocation, &this->mTechnology, sizeof(this->mTechnology));
        theLocation += sizeof(this->mTechnology);
        ESVERIFY_AT(theLocation == theMessageSize, 3445);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMCastCalamity::ExportSize()
{
    return 25;
}

// FUNCTION: EXE 0x4bc990
// Reads the Cast Calamity message (selection id, caster, target, tile, technology) from a network buffer
void EEMCastCalamity::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 3483);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mCasterID, inPayloadPointer + theLocation, sizeof(this->mCasterID));
    theLocation += sizeof(this->mCasterID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mTileX, inPayloadPointer + theLocation, sizeof(this->mTileX));
    theLocation += sizeof(this->mTileX);
    memcpy(&this->mTileY, inPayloadPointer + theLocation, sizeof(this->mTileY));
    theLocation += sizeof(this->mTileY);
    memcpy(&this->mTechnology, inPayloadPointer + theLocation, sizeof(this->mTechnology));
    theLocation += sizeof(this->mTechnology);
}

bool EEMCastCalamity::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bca28
// Gives the sender's selection (and its saved group) a new formation of the requested type
void EEMChangeFormation::Execute()
{
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    EEFormation* theFormation = EEFormation::Create(this->mFormationType);
    theFormation->mB5C = this->mB14;
    theGroup->SetFormation(theFormation);
    if (theGroup->mSavedGroup)
        theGroup->mSavedGroup->SetFormation(theFormation);
    theFormation->Dereference();
}

// FUNCTION: EXE 0x4bca7d
// Writes the Change Formation message (selection id, formation type, flag) to a new network buffer
void EEMChangeFormation::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mFormationType, sizeof(this->mFormationType));
        theLocation += sizeof(this->mFormationType);
        memcpy(*outPayloadPointer + theLocation, &this->mB14, sizeof(this->mB14));
        theLocation += sizeof(this->mB14);
        ESVERIFY_AT(theLocation == theMessageSize, 3584);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMChangeFormation::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4bcb0c
// Returns the Change Formation message's debug label
char* EEMChangeFormation::GetMessageLabel() const
{
    return "ChangeFormation";
}

// FUNCTION: EXE 0x4bcb12
// Reads the Change Formation message (selection id, formation type) from a buffer; the exported flag is never read
void EEMChangeFormation::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 3634);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mFormationType, inPayloadPointer + theLocation, sizeof(this->mFormationType));
    theLocation += sizeof(this->mFormationType);
}

bool EEMChangeFormation::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bcb62
// Debug: grows or shrinks the inner or outer collision sphere (by collision sphere mode 9/10) of the first
// selected unit's class by 0.05 and refreshes that unit's radius; the sea units it collects are never used
void EEMChangeSelectedUnitCollisionSphere::Execute()
{
    std::vector<RTSUnit*> theUnits;
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    if (theGroup) {
        EEComplexUnit* theUnit = theGroup->GetFirstUnit();
        if (theUnit) {
            if (theUnit->mUnitClass->mDomains & kMovementSea) {
                for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[i];
                    if (thePlayer) {
                        for (EEUnit* theOther = thePlayer->mFirstUnit; theOther; theOther = static_cast<EEUnit*>(theOther->mNext)) {
                            if (theOther->mComplexUnit && theOther->mUnitClass->mUnitType == theUnit->mUnitClass->mUnitType)
                                theUnits.push_back(theOther->mComplexUnit);
                        }
                    }
                }
            }
            if (this->mGrow) {
                switch (gCollisionSphereMode) {
                case 9:
                    theUnit->mUnitClass->mInnerCollisionSphere += 0.05f;
                    break;
                case 10:
                    theUnit->mUnitClass->mOuterCollisionSphere += 0.05f;
                    break;
                default:
                    return;
                }
            } else {
                switch (gCollisionSphereMode) {
                case 9:
                    theUnit->mUnitClass->mInnerCollisionSphere -= 0.05f;
                    break;
                case 10:
                    theUnit->mUnitClass->mOuterCollisionSphere -= 0.05f;
                    break;
                default:
                    return;
                }
            }
            theUnit->mCollisionRadius = (float)theUnit->mUnitClass->mInnerCollisionSphere;
        }
    }
}

// FUNCTION: EXE 0x4bcc91
// Writes the Change Selected Unit Collision Sphere message (selection id, grow flag) to a new network buffer
void EEMChangeSelectedUnitCollisionSphere::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGrow, sizeof(this->mGrow));
        theLocation += sizeof(this->mGrow);
        ESVERIFY_AT(theLocation == theMessageSize, 3772);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bcd0d
// Returns the Change Selected Unit Collision Sphere message's debug label
char* EEMChangeSelectedUnitCollisionSphere::GetMessageLabel() const
{
    return "ChangeSelectedUnitCollisionSphere";
}

// FUNCTION: EXE 0x4bcd13
// Reads the Change Selected Unit Collision Sphere message (selection id, grow flag) from a network buffer
void EEMChangeSelectedUnitCollisionSphere::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 3821);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGrow, inPayloadPointer + theLocation, sizeof(this->mGrow));
    theLocation += sizeof(this->mGrow);
}

bool EEMChangeSelectedUnitCollisionSphere::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bcd63
// Posts a non-empty chat text to the chat log, marked as own when it came from the local slot
void EEMChat::Execute()
{
    if (gCommunications && this->mText.GetLength() > 0) {
        unsigned char theLocalSlot;
        if (gCommunications->IsClient() == true)
            theLocalSlot = static_cast<EEClient*>(gCommunications)->GetMySendToSlot();
        else
            theLocalSlot = 0xfe;
        EEAction* theAction = new EEAAddChatMessage(this->mText, theLocalSlot == this->GetOriginatingSlot());
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x4bcdca
// Writes the Chat message (selection id, player, text) to a new network buffer
void EEMChat::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        this->mText.Export(*outPayloadPointer + theLocation);
        theLocation += this->mText.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 3905);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bce60
// Returns the Chat message's wire size: 6 bytes plus the text
unsigned long EEMChat::ExportSize()
{
    return this->mText.GetExportSize() + 6;
}

// FUNCTION: EXE 0x4bce6d
// Returns the Chat message's debug label
char* EEMChat::GetMessageLabel() const
{
    return "Chat";
}

// FUNCTION: EXE 0x4bce73
// Reads the Chat message (selection id, player, text) from a network buffer
void EEMChat::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 3949);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    this->mText.Import(inPayloadPointer + theLocation);
}

bool EEMChat::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bced6
// Applies a cheat code with its parameter for the sending player
void EEMCheatCode::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    gCheatCodes->ApplyCheatCode(this->mCode, thePlayer->mPlayerID, this->mParameter);
}

// FUNCTION: EXE 0x4bcf0a
// Writes the Cheat Code message (selection id, code, parameter) to a new network buffer
void EEMCheatCode::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mCode, sizeof(this->mCode));
        theLocation += sizeof(this->mCode);
        memcpy(*outPayloadPointer + theLocation, &this->mParameter, sizeof(this->mParameter));
        theLocation += sizeof(this->mParameter);
        ESVERIFY_AT(theLocation == theMessageSize, 4034);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMCheatCode::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4bcf9b
// Returns the Cheat Code message's debug label
char* EEMCheatCode::GetMessageLabel() const
{
    return "Cheat Code";
}

// FUNCTION: EXE 0x4bcfa1
// Reads the Cheat Code message (selection id, code, parameter) from a network buffer
void EEMCheatCode::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 4084);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mCode, inPayloadPointer + theLocation, sizeof(this->mCode));
    theLocation += sizeof(this->mCode);
    memcpy(&this->mParameter, inPayloadPointer + theLocation, sizeof(this->mParameter));
    theLocation += sizeof(this->mParameter);
}

bool EEMCheatCode::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bd008
// For a local player still choosing a civilization: warns that time is running out (clock tick), or ends
// the choice (posts the timer-expired event, logs it, locks every other player's civilization)
void EEMCivTimerExpired::Execute()
{
    UWideString theText;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    if (!thePlayer->mCivilizationChosen && !thePlayer->mObserverMode) {
        if (this->mIsWarning) {
            EESoundObject* theSound = EESoundObject::Get(kSoundClockTick);
            if (theSound)
                theSound->Play();
            gResource->GetString(IDS_WARNING_YOU_ARE_RUNNING_OUT, theText);
            gUserInterface->AddChatLogEntry(theText, 0);
        } else {
            UIEvent* theEvent = new EEUIEProcessTimerExpired;
            gUserInterface->PostEvent(theEvent, NULL);
            theEvent->Dereference();
            gResource->GetString(IDS_CIVILIZATION_SELECTION_TIME_HAS_EXPIRED, theText);
            gUserInterface->AddChatLogEntry(theText, 0);
            for (unsigned long i = 1; i < gWorld.mNumPlayers; i++)
                if (i != gWorld.mCurrentPlayer)
                    gWorld.mPlayers[i]->mCivilizationLocked = true;
        }
    }
}

// FUNCTION: EXE 0x4bd124
// Writes the Civ Timer Expired message (selection id, warning flag) to a new network buffer
void EEMCivTimerExpired::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsWarning, sizeof(this->mIsWarning));
        theLocation += sizeof(this->mIsWarning);
        ESVERIFY_AT(theLocation == theMessageSize, 4218);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMCivTimerExpired::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4bd1a0
// Returns the Civ Timer Expired message's debug label
char* EEMCivTimerExpired::GetMessageLabel() const
{
    return "Civ Timer Expired";
}

// FUNCTION: EXE 0x4bd1a6
// Reads the Civ Timer Expired message (selection id, warning flag) from a network buffer
void EEMCivTimerExpired::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 4267);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mIsWarning, inPayloadPointer + theLocation, sizeof(this->mIsWarning));
    theLocation += sizeof(this->mIsWarning);
}

bool EEMCivTimerExpired::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bd1f6
// Returns the civilization upgrades message's wire size: 11 bytes plus 2 per upgrade plus the name
unsigned long EEMCivUpgrades::ExportSize()
{
    return this->mUpgrades.size() * sizeof(unsigned short) + this->mCivName.GetExportSize() + 11;
}

// FUNCTION: EXE 0x4bd218
// Writes the civilization upgrades message (player, name string, upgrades, civ name) to a new network buffer
void EEMCivUpgrades::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    unsigned char theNumUpgrades = this->mUpgrades.size();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mNameString, sizeof(this->mNameString));
        theLocation += sizeof(this->mNameString);
        memcpy(*outPayloadPointer + theLocation, &theNumUpgrades, sizeof(theNumUpgrades));
        theLocation += sizeof(theNumUpgrades);
        for (std::vector<unsigned short>::iterator it = this->mUpgrades.begin(); it != this->mUpgrades.end(); ++it) {
            unsigned short theUpgrade = *it;
            memcpy(*outPayloadPointer + theLocation, &theUpgrade, sizeof(theUpgrade));
            theLocation += sizeof(theUpgrade);
        }
        this->mCivName.Export(*outPayloadPointer + theLocation);
        theLocation += this->mCivName.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 4372);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bd327
// Reads the civilization upgrades message (player, name string, upgrades, civ name) from a network buffer
void EEMCivUpgrades::ImportMessage(unsigned char* inPayloadPointer)
{
    unsigned char theNumUpgrades = 0;
    ESVERIFY_AT(inPayloadPointer != NULL, 4392);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mNameString, inPayloadPointer + theLocation, sizeof(this->mNameString));
    theLocation += sizeof(this->mNameString);
    memcpy(&theNumUpgrades, inPayloadPointer + theLocation, sizeof(theNumUpgrades));
    theLocation += sizeof(theNumUpgrades);
    for (unsigned long i = 0; i < theNumUpgrades; i++) {
        unsigned short theUpgrade;
        memcpy(&theUpgrade, inPayloadPointer + theLocation, sizeof(theUpgrade));
        theLocation += sizeof(theUpgrade);
        this->mUpgrades.push_back(theUpgrade);
    }
    this->mCivName.Import(inPayloadPointer + theLocation);
}

bool EEMCivUpgrades::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bd3f7
// Applies a player's civilization (name and upgrades) validated by the server, once only when civs are bought;
// locks the choice for players other than the local one
void EEMCivUpgradesPurchased::Execute()
{
    UWideString theName;
    unsigned long thePlayerIndex = this->mPlayerID;
    EEPlayer* thePlayer = gWorld.mPlayers[thePlayerIndex];
    bool theApply = gWorld.mGameOptions.CanBuyCivilization() ? !thePlayer->mCivUpgradesPurchased : true;
    if (theApply) {
        theName = this->mCivName;
        if (this->mNameString)
            gResource->GetString(this->mNameString, theName);
        thePlayer->mCivilization.SetName(theName);
        thePlayer->mCivilization.AddUpgrades(this->mUpgrades, thePlayerIndex, true);
        thePlayer->mCivUpgradesPurchased = true;
        if (thePlayer != gWorld.GetCurrentPlayer()) {
            thePlayer->mCivilizationChosen = true;
            thePlayer->mCivilizationLocked = true;
        }
    }
}

// FUNCTION: EXE 0x4bd4ad
// Returns the Civ Upgrades Purchased message's debug label
char* EEMCivUpgradesPurchased::GetMessageLabel() const
{
    return "Civilization Builder upgrades validated and purchased from server";
}

// FUNCTION: EXE 0x4bd4b3
// Has the server validate a player's requested civilization upgrades (EECBRequestUpgrades)
void EEMCivUpgradesRequested::Execute()
{
    EECBRequestUpgrades(this->mPlayerID, this->mCivName, this->mUpgrades);
}

// FUNCTION: EXE 0x4bd4ca
// Returns the Civ Upgrades Requested message's debug label
char* EEMCivUpgradesRequested::GetMessageLabel() const
{
    return "Civilization Builder requesting upgrades from server";
}

// FUNCTION: EXE 0x4bd4d0
// Returns the Clear Airport Commands message's wire size: 5 bytes
unsigned long EEMClearAirportCommands::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4bd4d4
// Writes the Clear Airport Commands message (selection id) to a new network buffer
void EEMClearAirportCommands::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 4561);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bd53f
// Returns the Clear Airport Commands message's debug label
char* EEMClearAirportCommands::GetMessageLabel() const
{
    return "Clear Airport Commands";
}

// FUNCTION: EXE 0x4bd545
// Reads the Clear Airport Commands message (selection id) from a network buffer
void EEMClearAirportCommands::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 4579);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMClearAirportCommands::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bd588
// Clears the queued orders of the sender's selected airports, carriers/space docks and navy yards (reopening
// them and emptying their air traffic queue) and the orders of selected aircraft
void EEMClearAirportCommands::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup != NULL) {
        for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
            EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(theGroup->GetUnit(i));
            EEUnitClass* theClass = theUnit->GetUnitClass();
            long theCategory = theClass->mCategory;
            if (theClass->mUnitType == kUnitTypeAirport) {
                theUnit->mGoalQueues[0].Clear();
                theUnit->mGoalQueues[1].Clear();
                theUnit->mGoalQueues[2].Clear();
                theUnit->mGoalQueues[3].Clear();
                theUnit->mGoalQueues[4].Clear();
                theUnit->StartAction(kActionAirportOpen);
                theClass->ActionChanged(theUnit);
                theUnit->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1]->ClearAirportQueue(theUnit, 0);
            } else if ((theCategory == kUnitCategoryNavalBuilding || theCategory == kUnitCategorySpaceBuilding) && theClass->mAirTrafficType != 0) {
                (theUnit->mGoalQueues.begin() + 0)->Clear();
                (theUnit->mGoalQueues.begin() + 1)->Clear();
                theUnit->StartAction(kActionAirportOpen);
                theClass->ActionChanged(theUnit);
                theUnit->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1]->ClearAirportQueue(theUnit, 0);
            } else if (theClass->mUnitType == kUnitTypeNavyYard) {
                theUnit->mGoalQueues[0].Clear();
                theUnit->mGoalQueues[2].Clear();
                theUnit->StartAction(kActionAirportOpen);
                theClass->ActionChanged(theUnit);
                theUnit->GetPlayer()->mAirTraffic[theClass->mAirTrafficType - 1]->ClearAirportQueue(theUnit, 0);
            } else if (theClass->mCategory == kUnitCategoryAirplane || theClass->mCategory == kUnitCategoryPlanetSpaceship) {
                theUnit->mGoalQueues[1].Clear();
            }
            theUnit->RefreshGoals();
            theUnit->RefreshActiveGoal();
        }
    }
}

// FUNCTION: EXE 0x4bd77c
// Writes the Disband message (selection id) to a new network buffer
void EEMDisband::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 4701);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMDisband::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4bd7e7
// Reads the Disband message (selection id) from a network buffer
void EEMDisband::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 4712);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMDisband::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bd82a
// Disbands the sender's living, disbandable selected units, refunding 75% of their cost scaled by health;
// plays the cash register sound for the local player
void EEMDisband::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup->GetFirstUnit()) {
        EESoundObject* theSound = EESoundObject::Get(kSoundCashRegister);
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theUnit->mHitPoints > 0 && theUnit->mUnitClass->mCanDisband) {
                thePlayer->RefundTechnology(thePlayer->mTechnologies->Get(theUnit->mUnitClass->mTechnology), theUnit->mUnitClass->mUnitType,
                                     (float)theUnit->mHitPoints / theUnit->mStats->mMaxHitPoints * kDisbandRefund);
                gEffectManager->CreateUnitEffect(theUnit, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), NULL, 1, 0, 0);
                theUnit->Kill(0, 0, 0);
            }
        }
        if (theSound && thePlayer == gWorld.GetCurrentPlayer())
            theSound->Play();
    }
}

// FUNCTION: EXE 0x4bd93c
// Orders the sender's selection to carry its resources to the given drop-off building
void EEMDropoffResource::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEComplexUnit* theUnit = theGroup->GetFirstUnit();
    unsigned long theID = this->mDropoffID;
    EEComplexUnit* theDropoff = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
    EEActiveGroup* theActiveGroup = NULL;
    if (theUnit && theDropoff) {
        theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 0, 0);
        thePlayer->mPendingGroups.push_back(theActiveGroup);
        theActiveGroup->SpreadAroundTile(theDropoff->mTileX, theDropoff->mTileY);
        if (theActiveGroup->GetNumUnits() > 0) {
            theActiveGroup->ClearGoal();
            theActiveGroup->SetGoal(new EEUGGather(NULL, theDropoff, true), false);
            theActiveGroup->mAction = kActionDropoffResource;
            theActiveGroup->Plan();
            if (theActiveGroup->GetNumUnits() > 0)
                thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
        }
    }
}

// FUNCTION: EXE 0x4bda6e
// Writes the Dropoff Resource message (selection id, drop-off unit) to a new network buffer
void EEMDropoffResource::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mDropoffID, sizeof(this->mDropoffID));
        theLocation += sizeof(this->mDropoffID);
        ESVERIFY_AT(theLocation == theMessageSize, 4907);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMDropoffResource::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4bdaec
// Returns the Dropoff Resource message's debug label
char* EEMDropoffResource::GetMessageLabel() const
{
    return "Dropoff Resource";
}

// FUNCTION: EXE 0x4bdaf2
// Reads the Dropoff Resource message (selection id, drop-off unit) from a network buffer
void EEMDropoffResource::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 4956);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mDropoffID, inPayloadPointer + theLocation, sizeof(this->mDropoffID));
    theLocation += sizeof(this->mDropoffID);
}

bool EEMDropoffResource::IsResponse(NEMessage* inMessage)
{
    return false;
}

void EEMEdictPreProposal::Execute()
{
}

// FUNCTION: EXE 0x4bdb82
// Writes the Edict Pre-Proposal message (selection id, edict) to a new network buffer
void EEMEdictPreProposal::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mEdict, sizeof(this->mEdict));
        theLocation += sizeof(this->mEdict);
        ESVERIFY_AT(theLocation == theMessageSize, 5015);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMEdictPreProposal::ExportSize()
{
    return 7;
}

// FUNCTION: EXE 0x4bdc33
// Reads the Edict Pre-Proposal message (selection id, edict) from a network buffer
void EEMEdictPreProposal::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5025);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mEdict, inPayloadPointer + theLocation, sizeof(this->mEdict));
    theLocation += sizeof(this->mEdict);
}

bool EEMEdictPreProposal::IsResponse(NEMessage* inMessage)
{
    return false;
}

void EEMEdictPreProposalResponse::Execute()
{
}

// FUNCTION: EXE 0x4bdc83
// Writes the Edict Pre-Proposal Response message (selection id, edict, answer) to a new network buffer
void EEMEdictPreProposalResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mEdict, sizeof(this->mEdict));
        theLocation += sizeof(this->mEdict);
        memcpy(*outPayloadPointer + theLocation, &this->mW12, sizeof(this->mW12));
        theLocation += sizeof(this->mW12);
        ESVERIFY_AT(theLocation == theMessageSize, 5084);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMEdictPreProposalResponse::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4bdd13
// Reads the Edict Pre-Proposal Response message (selection id, edict, answer) from a network buffer
void EEMEdictPreProposalResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5094);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mEdict, inPayloadPointer + theLocation, sizeof(this->mEdict));
    theLocation += sizeof(this->mEdict);
    memcpy(&this->mW12, inPayloadPointer + theLocation, sizeof(this->mW12));
    theLocation += sizeof(this->mW12);
}

bool EEMEdictPreProposalResponse::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bdd7a
// Writes the Edict Proposal message (selection id, edict) to a new network buffer
void EEMEdictProposal::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mEdict, sizeof(this->mEdict));
        theLocation += sizeof(this->mEdict);
        ESVERIFY_AT(theLocation == theMessageSize, 5152);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMEdictProposal::ExportSize()
{
    return 7;
}

// FUNCTION: EXE 0x4bddf8
// Reads the Edict Proposal message (selection id, edict) from a network buffer
void EEMEdictProposal::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5162);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mEdict, inPayloadPointer + theLocation, sizeof(this->mEdict));
    theLocation += sizeof(this->mEdict);
}

bool EEMEdictProposal::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bde48
// Proposes an edict to the vote (argument 15) and clears the physics value mF84
void EEMEdictProposal::Execute()
{
    gWorld.mVoting.Propose(this->mEdict, 15);
    gPhysics->mF84 = 0.0f;
}

void EEMEdictVote::Execute()
{
}

// FUNCTION: EXE 0x4bde69
// Writes the Edict Vote message (selection id, edict, yes flag, count) to a new network buffer
void EEMEdictVote::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mEdict, sizeof(this->mEdict));
        theLocation += sizeof(this->mEdict);
        memcpy(*outPayloadPointer + theLocation, &this->mIsYes, sizeof(this->mIsYes));
        theLocation += sizeof(this->mIsYes);
        memcpy(*outPayloadPointer + theLocation, &this->mCount, sizeof(this->mCount));
        theLocation += sizeof(this->mCount);
        ESVERIFY_AT(theLocation == theMessageSize, 5231);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMEdictVote::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4bdf08
// Reads the Edict Vote message (selection id, edict, yes flag, count) from a network buffer
void EEMEdictVote::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5241);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mEdict, inPayloadPointer + theLocation, sizeof(this->mEdict));
    theLocation += sizeof(this->mEdict);
    memcpy(&this->mIsYes, inPayloadPointer + theLocation, sizeof(this->mIsYes));
    theLocation += sizeof(this->mIsYes);
    memcpy(&this->mCount, inPayloadPointer + theLocation, sizeof(this->mCount));
    theLocation += sizeof(this->mCount);
}

bool EEMEdictVote::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bdf7d
// Ends the scenario's running cinematic
void EEMEndCinematic::Execute()
{
    UWideString theText;
    gWorld.mScenarioManager->EndCinematic();
}

// FUNCTION: EXE 0x4bdfa2
// Writes the End Cinematic message (selection id, value) to a new network buffer
void EEMEndCinematic::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mF10, sizeof(this->mF10));
        theLocation += sizeof(this->mF10);
        ESVERIFY_AT(theLocation == theMessageSize, 5320);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMEndCinematic::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4be020
// Returns the End Cinematic message's debug label
char* EEMEndCinematic::GetMessageLabel() const
{
    return "End Cinematic";
}

// FUNCTION: EXE 0x4be026
// Reads the End Cinematic message (selection id, value) from a network buffer
void EEMEndCinematic::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5369);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mF10, inPayloadPointer + theLocation, sizeof(this->mF10));
    theLocation += sizeof(this->mF10);
}

bool EEMEndCinematic::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4be076
// Puts the sender into shift (queue-orders) mode, and the mouse too for the local player
void EEMEnterShiftMode::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->mShiftMode = true;
    if (thePlayer == gWorld.GetCurrentPlayer())
        gMouse->mShiftMode = true;
}

// FUNCTION: EXE 0x4be0b1
// Writes the Enter Shift Mode message (selection id, game time) to a new network buffer
void EEMEnterShiftMode::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGameTime, sizeof(this->mGameTime));
        theLocation += sizeof(this->mGameTime);
        ESVERIFY_AT(theLocation == theMessageSize, 5445);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMEnterShiftMode::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4be12f
// Returns the Enter Shift Mode message's debug label
char* EEMEnterShiftMode::GetMessageLabel() const
{
    return "Enter Shift Mode";
}

// FUNCTION: EXE 0x4be135
// Reads the Enter Shift Mode message (selection id, game time) from a network buffer
void EEMEnterShiftMode::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5491);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGameTime, inPayloadPointer + theLocation, sizeof(this->mGameTime));
    theLocation += sizeof(this->mGameTime);
}

bool EEMEnterShiftMode::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4be185
// Takes the sender out of shift (queue-orders) mode, and the mouse too for the local player
void EEMExitShiftMode::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->mShiftMode = false;
    if (thePlayer == gWorld.GetCurrentPlayer())
        gMouse->mShiftMode = false;
}

// FUNCTION: EXE 0x4be1c0
// Writes the Exit Shift Mode message (selection id, game time) to a new network buffer
void EEMExitShiftMode::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGameTime, sizeof(this->mGameTime));
        theLocation += sizeof(this->mGameTime);
        ESVERIFY_AT(theLocation == theMessageSize, 5571);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMExitShiftMode::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4be23e
// Returns the Exit Shift Mode message's debug label
char* EEMExitShiftMode::GetMessageLabel() const
{
    return "Exit Shift Mode";
}

// FUNCTION: EXE 0x4be244
// Reads the Exit Shift Mode message (selection id, game time) from a network buffer
void EEMExitShiftMode::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5617);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGameTime, inPayloadPointer + theLocation, sizeof(this->mGameTime));
    theLocation += sizeof(this->mGameTime);
}

bool EEMExitShiftMode::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4be294
// Writes the Exchange Resources message (selection id, sell flag, resource, five-times flag) to a new network buffer
void EEMExchangeResources::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsSell, sizeof(this->mIsSell));
        theLocation += sizeof(this->mIsSell);
        memcpy(*outPayloadPointer + theLocation, &this->mResourceType, sizeof(this->mResourceType));
        theLocation += sizeof(this->mResourceType);
        memcpy(*outPayloadPointer + theLocation, &this->mFiveTimes, sizeof(this->mFiveTimes));
        theLocation += sizeof(this->mFiveTimes);
        ESVERIFY_AT(theLocation == theMessageSize, 5689);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMExchangeResources::ExportSize()
{
    return 11;
}

// FUNCTION: EXE 0x4be334
// Returns the Exchange Resources message's debug label
char* EEMExchangeResources::GetMessageLabel() const
{
    return "Exchange Resources";
}

// FUNCTION: EXE 0x4be33a
// Reads the Exchange Resources message (selection id, sell flag, resource, five-times flag) from a network buffer
void EEMExchangeResources::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5712);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mIsSell, inPayloadPointer + theLocation, sizeof(this->mIsSell));
    theLocation += sizeof(this->mIsSell);
    memcpy(&this->mResourceType, inPayloadPointer + theLocation, sizeof(this->mResourceType));
    theLocation += sizeof(this->mResourceType);
    memcpy(&this->mFiveTimes, inPayloadPointer + theLocation, sizeof(this->mFiveTimes));
    theLocation += sizeof(this->mFiveTimes);
}

bool EEMExchangeResources::IsResponse(NEMessage* inMessage)
{
    return false;
}

// 0x23cc42 (cdecl): the exchange rate between a resource and the trade resource for inPlayer (provisional name)
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);

// FUNCTION: EXE 0x4be3b1
// Trades 100 of a resource for gold or buys 100 with gold at the market price, once or five times (a partial
// trade when short), tracks the traded amount and refreshes the help text for the local player
void EEMExchangeResources::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    long theCount = 1;
    if (this->mFiveTimes)
        theCount = 5;
    while (theCount > 0) {
        long theAmount = EEGetTradePrice(thePlayer, this->mResourceType, this->mIsSell);
        long theCost = 100;
        if (this->mIsSell) {
            long theHave = thePlayer->mResourceCaps[this->mResourceType];
            if (theHave < theCost) {
                theAmount = UMFloatToLong(theHave * theAmount * 0.01f);
                theCost = thePlayer->mResourceCaps[this->mResourceType];
            }
            if (theAmount > 0) {
                thePlayer->TakeResource(this->mResourceType, theCost);
                thePlayer->AddResource(kResourceGold, theAmount);
                thePlayer->mResourceTrade[this->mResourceType - 2] -= theCost;
            }
        } else {
            long theHave = thePlayer->mResourceCaps[kResourceGold];
            if (theHave < theAmount) {
                theCost = UMFloatToLong(100.0f / theAmount * theHave);
                theAmount = thePlayer->mResourceCaps[kResourceGold];
                if (theCost <= 0)
                    goto next;
            }
            thePlayer->AddResource(this->mResourceType, theCost);
            thePlayer->TakeResource(kResourceGold, theAmount);
            thePlayer->mResourceTrade[this->mResourceType - 2] += theCost;
        }
next:
        theCount--;
    }
    if (gWorld.mPlayers[gWorld.mCurrentPlayer]->mPlayerID == thePlayer->mPlayerID)
        gUIManager->mMainGameForm->UpdateHelpString();
}

// FUNCTION: EXE 0x4be508
// Writes the Explore message (selection id) to a new network buffer
void EEMExplore::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 5846);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMExplore::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4be573
// Reads the Explore message (selection id) from a network buffer
void EEMExplore::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 5856);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMExplore::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4be5b6
// Sends the sender's mobile selected units, split by unit category, to explore unexplored areas
void EEMExplore::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    for (unsigned long i = 1; i < 6; i++) {
        EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 1, i);
        if (theActiveGroup->GetNumUnits() > 0) {
            thePlayer->mPendingGroups.push_back(theActiveGroup);
            theActiveGroup->KeepMobileUnits();
            theActiveGroup->ClearGoal();
            theActiveGroup->SetGoal(new EEUGExplore(false), false);
            theActiveGroup->mAction = kActionFindUnexploredArea;
            theActiveGroup->Plan();
            EEComplexUnit* theUnit;
            if (theActiveGroup->GetNumUnits() > 0)
                theUnit = theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1));
            else
                theUnit = NULL;
            if (theUnit)
                thePlayer->PlayAcknowledgement(theUnit);
        } else
            theActiveGroup->Dereference();
    }
}

// FUNCTION: EXE 0x4be6c6
// Creates a Full Game Options message holding a copy of every lobby game option
EEMFullGameOptions::EEMFullGameOptions(const EEGameOptions& inOptions)
    : EEMessage(kMessageFullGameOptions, false)
{
    this->mMapType = inOptions.GetMapType();
    this->mGameSpeed = inOptions.GetGameSpeed();
    this->mLoadType = inOptions.GetLoadType();
    this->mMapSize = inOptions.GetMapSize();
    this->mEpochStart = inOptions.GetEpochStart();
    this->mEpochEnd = inOptions.GetEpochEnd();
    this->mVariant = inOptions.GetVariant();
    this->mResources = inOptions.GetResources();
    this->mUnitLimit = inOptions.GetUnitLimit();
    this->mWondersForVictory = inOptions.GetWondersForVictory();
    this->mDifficulty = inOptions.GetDifficulty();
    this->mVictory = inOptions.GetVictory();
    this->mLockTeams = inOptions.GetLockTeams();
    this->mLockSpeed = inOptions.GetLockSpeed();
    this->mRevealMap = inOptions.GetRevealMap();
    this->mCheatCodes = inOptions.GetCheatCodes();
    this->mRecordGames = inOptions.GetRecordGames();
    this->mAllowCustomCivs = inOptions.GetAllowCustomCivs();
    this->mB53 = inOptions.GetB407();
}

// FUNCTION: EXE 0x4be7b7
// Copies the received lobby game options into the game's options (except the B53 flag) and updates the lobby
void EEMFullGameOptions::Execute()
{
    gGameOptions.mLock->WriteLock();
    gGameOptions.mLoadType = this->mLoadType;
    gGameOptions.mMapType = this->mMapType;
    gGameOptions.mMapSize = this->mMapSize;
    gGameOptions.mResources = this->mResources;
    gGameOptions.mEpochStart = this->mEpochStart;
    gGameOptions.mEpochEnd = this->mEpochEnd;
    gGameOptions.mUnitLimit = this->mUnitLimit;
    gGameOptions.mWondersForVictory = this->mWondersForVictory;
    gGameOptions.mVariant = this->mVariant;
    gGameOptions.mDifficulty = this->mDifficulty;
    gGameOptions.mGameSpeed = this->mGameSpeed;
    gGameOptions.mRecordGames = this->mRecordGames;
    gGameOptions.mAllowCustomCivs = this->mAllowCustomCivs;
    gGameOptions.mRevealMap = this->mRevealMap;
    gGameOptions.mLockTeams = this->mLockTeams;
    gGameOptions.mLockSpeed = this->mLockSpeed;
    gGameOptions.mCheatCodes = this->mCheatCodes;
    gGameOptions.mVictory = this->mVictory;
    gGameOptions.mLock->WriteUnlock();
    gUserInterface->PostLobbyUpdateGameOptions();
}

// FUNCTION: EXE 0x4be876
// Writes the Full Game Options message (selection id, every option, map type) to a new network buffer
void EEMFullGameOptions::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mAllowCustomCivs, sizeof(this->mAllowCustomCivs));
        theLocation += sizeof(this->mAllowCustomCivs);
        memcpy(*outPayloadPointer + theLocation, &this->mB53, sizeof(this->mB53));
        theLocation += sizeof(this->mB53);
        memcpy(*outPayloadPointer + theLocation, &this->mCheatCodes, sizeof(this->mCheatCodes));
        theLocation += sizeof(this->mCheatCodes);
        memcpy(*outPayloadPointer + theLocation, &this->mDifficulty, sizeof(this->mDifficulty));
        theLocation += sizeof(this->mDifficulty);
        memcpy(*outPayloadPointer + theLocation, &this->mEpochEnd, sizeof(this->mEpochEnd));
        theLocation += sizeof(this->mEpochEnd);
        memcpy(*outPayloadPointer + theLocation, &this->mGameSpeed, sizeof(this->mGameSpeed));
        theLocation += sizeof(this->mGameSpeed);
        memcpy(*outPayloadPointer + theLocation, &this->mLoadType, sizeof(this->mLoadType));
        theLocation += sizeof(this->mLoadType);
        memcpy(*outPayloadPointer + theLocation, &this->mVariant, sizeof(this->mVariant));
        theLocation += sizeof(this->mVariant);
        memcpy(*outPayloadPointer + theLocation, &this->mLockSpeed, sizeof(this->mLockSpeed));
        theLocation += sizeof(this->mLockSpeed);
        memcpy(*outPayloadPointer + theLocation, &this->mLockTeams, sizeof(this->mLockTeams));
        theLocation += sizeof(this->mLockTeams);
        memcpy(*outPayloadPointer + theLocation, &this->mMapSize, sizeof(this->mMapSize));
        theLocation += sizeof(this->mMapSize);
        memcpy(*outPayloadPointer + theLocation, &this->mRecordGames, sizeof(this->mRecordGames));
        theLocation += sizeof(this->mRecordGames);
        memcpy(*outPayloadPointer + theLocation, &this->mResources, sizeof(this->mResources));
        theLocation += sizeof(this->mResources);
        memcpy(*outPayloadPointer + theLocation, &this->mRevealMap, sizeof(this->mRevealMap));
        theLocation += sizeof(this->mRevealMap);
        memcpy(*outPayloadPointer + theLocation, &this->mEpochStart, sizeof(this->mEpochStart));
        theLocation += sizeof(this->mEpochStart);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitLimit, sizeof(this->mUnitLimit));
        theLocation += sizeof(this->mUnitLimit);
        memcpy(*outPayloadPointer + theLocation, &this->mVictory, sizeof(this->mVictory));
        theLocation += sizeof(this->mVictory);
        memcpy(*outPayloadPointer + theLocation, &this->mWondersForVictory, sizeof(this->mWondersForVictory));
        theLocation += sizeof(this->mWondersForVictory);
        this->mMapType.Export(*outPayloadPointer + theLocation);
        theLocation += this->mMapType.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 6095);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bea4e
// Returns the Full Game Options message's wire size: 53 bytes plus the map type
unsigned long EEMFullGameOptions::ExportSize()
{
    return this->mMapType.GetExportSize() + 53;
}

// FUNCTION: EXE 0x4bea5b
// Returns the Full Game Options message's debug label
char* EEMFullGameOptions::GetMessageLabel() const
{
    return "Full Game Options";
}

// FUNCTION: EXE 0x4bea61
// Reads the Full Game Options message (selection id, every option, map type) from a network buffer
void EEMFullGameOptions::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 6160);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mAllowCustomCivs, inPayloadPointer + theLocation, sizeof(this->mAllowCustomCivs));
    theLocation += sizeof(this->mAllowCustomCivs);
    memcpy(&this->mB53, inPayloadPointer + theLocation, sizeof(this->mB53));
    theLocation += sizeof(this->mB53);
    memcpy(&this->mCheatCodes, inPayloadPointer + theLocation, sizeof(this->mCheatCodes));
    theLocation += sizeof(this->mCheatCodes);
    memcpy(&this->mDifficulty, inPayloadPointer + theLocation, sizeof(this->mDifficulty));
    theLocation += sizeof(this->mDifficulty);
    memcpy(&this->mEpochEnd, inPayloadPointer + theLocation, sizeof(this->mEpochEnd));
    theLocation += sizeof(this->mEpochEnd);
    memcpy(&this->mGameSpeed, inPayloadPointer + theLocation, sizeof(this->mGameSpeed));
    theLocation += sizeof(this->mGameSpeed);
    memcpy(&this->mLoadType, inPayloadPointer + theLocation, sizeof(this->mLoadType));
    theLocation += sizeof(this->mLoadType);
    memcpy(&this->mVariant, inPayloadPointer + theLocation, sizeof(this->mVariant));
    theLocation += sizeof(this->mVariant);
    memcpy(&this->mLockSpeed, inPayloadPointer + theLocation, sizeof(this->mLockSpeed));
    theLocation += sizeof(this->mLockSpeed);
    memcpy(&this->mLockTeams, inPayloadPointer + theLocation, sizeof(this->mLockTeams));
    theLocation += sizeof(this->mLockTeams);
    memcpy(&this->mMapSize, inPayloadPointer + theLocation, sizeof(this->mMapSize));
    theLocation += sizeof(this->mMapSize);
    memcpy(&this->mRecordGames, inPayloadPointer + theLocation, sizeof(this->mRecordGames));
    theLocation += sizeof(this->mRecordGames);
    memcpy(&this->mResources, inPayloadPointer + theLocation, sizeof(this->mResources));
    theLocation += sizeof(this->mResources);
    memcpy(&this->mRevealMap, inPayloadPointer + theLocation, sizeof(this->mRevealMap));
    theLocation += sizeof(this->mRevealMap);
    memcpy(&this->mEpochStart, inPayloadPointer + theLocation, sizeof(this->mEpochStart));
    theLocation += sizeof(this->mEpochStart);
    memcpy(&this->mUnitLimit, inPayloadPointer + theLocation, sizeof(this->mUnitLimit));
    theLocation += sizeof(this->mUnitLimit);
    memcpy(&this->mVictory, inPayloadPointer + theLocation, sizeof(this->mVictory));
    theLocation += sizeof(this->mVictory);
    memcpy(&this->mWondersForVictory, inPayloadPointer + theLocation, sizeof(this->mWondersForVictory));
    theLocation += sizeof(this->mWondersForVictory);
    this->mMapType.Import(inPayloadPointer + theLocation);
}

bool EEMFullGameOptions::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bebe1
// Creates a Game Options message that changes one lobby option to a value (or the map type)
EEMGameOptions::EEMGameOptions(long inOption, long inValue, const UWideString& inS14)
    : EEMessage(kMessageGameOptions, false)
{
    this->mOption = inOption;
    this->mValue = inValue;
    this->mMapType = inS14;
}

// FUNCTION: EXE 0x4bec38
// Changes one lobby game option (0..17) to the message's value or map type and updates the lobby
void EEMGameOptions::Execute()
{
    gGameOptions.mLock->WriteLock();
    switch (this->mOption) {
    case 0:
        gGameOptions.mLoadType = this->mValue;
        break;
    case 1:
        gGameOptions.mMapSize = this->mValue;
        break;
    case 2:
        gGameOptions.mMapType = this->mMapType;
        break;
    case 3:
        gGameOptions.mResources = this->mValue;
        break;
    case 4:
        gGameOptions.mEpochStart = this->mValue;
        break;
    case 5:
        gGameOptions.mEpochEnd = this->mValue;
        break;
    case 6:
        gGameOptions.mUnitLimit = this->mValue;
        break;
    case 7:
        gGameOptions.mWondersForVictory = this->mValue;
        break;
    case 8:
        gGameOptions.mVariant = this->mValue;
        break;
    case 9:
        gGameOptions.mDifficulty = this->mValue;
        break;
    case 10:
        gGameOptions.mGameSpeed = this->mValue;
        break;
    case 11:
        gGameOptions.mRecordGames = this->mValue > 0;
        break;
    case 12:
        gGameOptions.mAllowCustomCivs = this->mValue > 0;
        break;
    case 13:
        gGameOptions.mRevealMap = this->mValue > 0;
        break;
    case 14:
        gGameOptions.mLockTeams = this->mValue > 0;
        break;
    case 15:
        gGameOptions.mLockSpeed = this->mValue > 0;
        break;
    case 16:
        gGameOptions.mCheatCodes = this->mValue > 0;
        break;
    case 17:
        gGameOptions.mVictory = this->mValue > 0;
        break;
    }
    gGameOptions.mLock->WriteUnlock();
    gUserInterface->PostLobbyUpdateGameOptions();
}

// FUNCTION: EXE 0x4beda5
// Writes the Game Options message (selection id, option, value, map type) to a new network buffer
void EEMGameOptions::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mOption, sizeof(this->mOption));
        theLocation += sizeof(this->mOption);
        memcpy(*outPayloadPointer + theLocation, &this->mValue, sizeof(this->mValue));
        theLocation += sizeof(this->mValue);
        this->mMapType.Export(*outPayloadPointer + theLocation);
        theLocation += this->mMapType.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 6373);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bee54
// Returns the Game Options message's wire size: 13 bytes plus the map type
unsigned long EEMGameOptions::ExportSize()
{
    return this->mMapType.GetExportSize() + 13;
}

// FUNCTION: EXE 0x4bee61
// Returns the Game Options message's debug label
char* EEMGameOptions::GetMessageLabel() const
{
    return "Game Options";
}

// FUNCTION: EXE 0x4bee67
// Reads the Game Options message (selection id, option, value, map type) from a network buffer
void EEMGameOptions::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 6423);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mOption, inPayloadPointer + theLocation, sizeof(this->mOption));
    theLocation += sizeof(this->mOption);
    memcpy(&this->mValue, inPayloadPointer + theLocation, sizeof(this->mValue));
    theLocation += sizeof(this->mValue);
    this->mMapType.Import(inPayloadPointer + theLocation);
}

bool EEMGameOptions::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4beedd
// Ends host migration on this machine (client or server) as the game restarts
void EEMGameRestarting::Execute()
{
    if (gCommunications) {
        if (gCommunications->IsClient())
            (static_cast<EEClient*>(gCommunications))->mIsMigrating = false;
        else
            (static_cast<EEServer*>(gCommunications))->mIsMigrating = false;
    }
}

// FUNCTION: EXE 0x4bef05
// Writes the Game Restarting message (selection id) to a new network buffer
void EEMGameRestarting::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 6507);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMGameRestarting::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4bef70
// Returns the Game Restarting message's debug label
char* EEMGameRestarting::GetMessageLabel() const
{
    return "Game Restarting";
}

// FUNCTION: EXE 0x4bef76
// Reads the Game Restarting message (selection id) from a network buffer
void EEMGameRestarting::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 6555);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMGameRestarting::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4befb9
// Sets the world's game speed to the requested speed
void EEMGameSpeed::Execute()
{
    gWorld.SetGameSpeed(this->mSpeed);
}

// FUNCTION: EXE 0x4befcf
// Packs the game-speed request (header, selection id, speed) into a new send buffer
void EEMGameSpeed::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSpeed, sizeof(this->mSpeed));
        theLocation += sizeof(this->mSpeed);
        ESVERIFY_AT(theLocation == theMessageSize, 6632);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMGameSpeed::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4bf04d
// Returns the message's debug label "Game Speed"
char* EEMGameSpeed::GetMessageLabel() const
{
    return "Game Speed";
}

// FUNCTION: EXE 0x4bf053
// Reads the game-speed request (header, selection id, speed) from a received payload
void EEMGameSpeed::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 6681);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSpeed, inPayloadPointer + theLocation, sizeof(this->mSpeed));
    theLocation += sizeof(this->mSpeed);
}

bool EEMGameSpeed::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bf0a3
// Sends the sender's selected units to gather from the target resource (if it has any left), spread around it,
// optionally queued; acknowledges with a random unit's voice
void EEMGatherResource::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEComplexUnit* theUnit = theGroup->GetFirstUnit();
    EEUnit* theTarget = gWorld.GetUnit(this->mTargetID);
    unsigned long thePlayerID = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mPlayerID;
    EEActiveGroup* theActiveGroup = NULL;
    if (theTarget && theUnit && theTarget->mResourceAmount > 0) {
        theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 0, 0);
        thePlayer->mPendingGroups.push_back(theActiveGroup);
        theActiveGroup->mIsQueued = this->mIsQueued;
        theActiveGroup->SpreadAroundResource(theTarget);
        if (theActiveGroup->GetNumUnits()) {
            theActiveGroup->ClearGoal();
            theActiveGroup->SetGoal(new EEUGGather(theTarget, NULL, true), this->mIsQueued);
            theActiveGroup->mAction = kActionGatherResource;
            theActiveGroup->Plan();
            EESendGroupCommand(theActiveGroup, theTarget, thePlayerID, 0xba, 0);
            if (theActiveGroup->GetNumUnits() > 0)
                thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
        }
    }
}

// FUNCTION: EXE 0x4bf20a
// Packs the gather order (header, selection id, target id, queued flag) into a new send buffer
void EEMGatherResource::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        ESVERIFY_AT(theLocation == theMessageSize, 6796);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMGatherResource::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4bf299
// Returns the message's debug label "Gather Resource"
char* EEMGatherResource::GetMessageLabel() const
{
    return "Gather Resource";
}

// FUNCTION: EXE 0x4bf29f
// Reads the gather order (header, selection id, target id, queued flag) from a received payload
void EEMGatherResource::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 6846);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
}

bool EEMGatherResource::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bf306
// Packs the go-fanatic order (header, selection id) into a new send buffer
void EEMGoFanatic::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 6908);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMGoFanatic::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4bf371
// Returns the message's debug label "Go Fanatic"
char* EEMGoFanatic::GetMessageLabel() const
{
    return "Go Fanatic";
}

// FUNCTION: EXE 0x4bf377
// Reads the go-fanatic order (header, selection id) from a received payload
void EEMGoFanatic::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 6922);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMGoFanatic::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bf3ba
// Makes every living unit of the sender's selection that is in unit set 0x2f go fanatic
void EEMGoFanatic::Execute()
{
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    EEUnitSet* theSet = gUnitSetManager.GetSet(0x2f);
    for (long i = theGroup->GetNumUnits() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = theGroup->GetUnit(i);
        if (theSet->Contains(theUnit->mUnitClass->mUnitType) && theUnit->mHitPoints > 0)
            theUnit->GoFanatic();
    }
}

// FUNCTION: EXE 0x4bf429
// Packs the create-unit cheat (header, selection id, unit type, position, owner id) into a new send buffer
void EEMHackANewUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitType, sizeof(this->mUnitType));
        theLocation += sizeof(this->mUnitType);
        memcpy(*outPayloadPointer + theLocation, &this->mPoint, sizeof(this->mPoint));
        theLocation += sizeof(this->mPoint);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        ESVERIFY_AT(theLocation == theMessageSize, 7007);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMHackANewUnit::ExportSize()
{
    return 18;
}

// FUNCTION: EXE 0x4bf4cb
// Reads the create-unit cheat (header, selection id, unit type, position, owner id) from a received payload
void EEMHackANewUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7017);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitType, inPayloadPointer + theLocation, sizeof(this->mUnitType));
    theLocation += sizeof(this->mUnitType);
    memcpy(&this->mPoint, inPayloadPointer + theLocation, sizeof(this->mPoint));
    theLocation += sizeof(this->mPoint);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
}

bool EEMHackANewUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bf543
// Cheat: creates a unit of the given type (or the sender's chosen cheat unit) at the point for the given player
// (ambient/resource units go to player 0); buildings must fit on the map and start fully built
void EEMHackANewUnit::Execute()
{
    GETransformation theTransformation;
    GE3DPoint thePoint;
    GE3DPoint theGround;
    GE2DPoint theTile = this->mPoint;
    EEPlayer* theSender = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    long thePlayerID = this->mPlayerID;
    gWorld.GetMap()->GetTile((long)this->mPoint.GetX(), (long)this->mPoint.GetY());
    EEPlayer* thePlayer = gWorld.mPlayers[thePlayerID];
    if (this->mUnitType == 0x367)
        this->mUnitType = theSender->mHackUnitClass;
    if (this->mUnitType == 0x367)
        return;
    EEUnitClass* theClass = gUnitClassTable[this->mUnitType];
    // a unit type without a class creates nothing (its category used to be read through NULL first)
    if (theClass == NULL)
        return;
    if (theClass->mCategory == kUnitCategoryAmbient || theClass->mCategory == kUnitCategoryResource) {
        thePlayer = gWorld.mPlayers[0];
        thePlayerID = 0;
    }
    if (!theClass->IsLandBuilding()) {
        thePoint.SetX(theTile.GetX());
        thePoint.SetY(theTile.GetY());
        gWorld.GetMap()->GetGroundPoint(thePoint, theGround);
        theTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f);
        theTransformation.SetPosition(theGround.GetX(), theGround.GetY(), theGround.GetZ());
        EEUnit* theUnit = theClass->CreateUnitAt(theTransformation, thePlayerID, false, NULL);
        if (theUnit->mTileNode != NULL) {
            theUnit->mComplexUnit->SetReloadTimer(0);
            theUnit->mComplexUnit->SetPower(theUnit->mComplexUnit->mStats->mPower);
        }
        if (thePlayerID == gWorld.GetCurrentPlayer()->mPlayerID && thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mCreationSound != NULL) {
            EESoundObject* theSound = thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mCreationSound;
            gSoundManager->Play(theSound->mSampleID, thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mCreationSound->mVolume, 0.0f,
                                1.0f, false, NULL);
        }
    } else {
        float theHalf = (float)((long)thePlayer->mUnitClassUpgrades[theClass->mUnitType]->mScale / 2);
        float theRight = theTile.GetX() + theHalf;
        float theTop = theTile.GetY() + theHalf;
        if (gWorld.GetMap()->GetTile((long)theRight, (long)theTop) == NULL)
            return;
        float theLeft = theTile.GetX() - theHalf;
        if (gWorld.GetMap()->GetTile((long)theLeft, (long)theTop) == NULL)
            return;
        float theBottom = theTile.GetY() - theHalf;
        if (gWorld.GetMap()->GetTile((long)theRight, (long)theBottom) == NULL)
            return;
        if (gWorld.GetMap()->GetTile((long)theLeft, (long)theBottom) == NULL)
            return;
        EEUnit* theUnit = theClass->CreateUnit((long)theLeft, (long)theBottom, thePlayerID, false, NULL);
        if (theUnit->mTileNode != NULL)
            theUnit->mComplexUnit->SetBuilt(1, 0, 1);
        if (thePlayerID == gWorld.GetCurrentPlayer()->mPlayerID) {
            EESoundObject* theSound = theUnit->mStats->mCreationSound;
            if (theSound != NULL)
                gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
        }
    }
}

// FUNCTION: EXE 0x4bf8c8
// Stops the sender's selected units: clears their goals and sets them idle
void EEMHaltSelectedGroup::Execute()
{
    EEGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    unsigned long theCount = theGroup->GetNumUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theGroup->GetUnit(i);
        theUnit->ClearGoals(0);
        theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
    }
}

// FUNCTION: EXE 0x4bf92b
// Packs the halt order (header, selection id) into a new send buffer
void EEMHaltSelectedGroup::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 7232);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMHaltSelectedGroup::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4bf996
// Returns the message's debug label "Halt Selected Group"
char* EEMHaltSelectedGroup::GetMessageLabel() const
{
    return "Halt Selected Group";
}

// FUNCTION: EXE 0x4bf99c
// Reads the halt order (header, selection id) from a received payload
void EEMHaltSelectedGroup::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7263);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMHaltSelectedGroup::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bf9df
// Orders the sender's selected healers, one group per formation slot, to heal the target unit; acknowledges by voice
void EEMHealUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEUnit* theTarget = gWorld.GetUnit(this->mTargetID);
    if (theTarget) {
        EEComplexUnit* theComplexUnit = theTarget->GetComplexUnit();
        for (unsigned long i = 1; i < 6; i++) {
            EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
            theActiveGroup->Fill(theGroup, 1, i);
            if (theActiveGroup->GetNumUnits() > 0) {
                thePlayer->mPendingGroups.push_back(theActiveGroup);
                theActiveGroup->ClearGoal();
                theActiveGroup->SetGoal(new EEUGHealUnit(theComplexUnit, true), false);
                theActiveGroup->mAction = kActionHealUnit;
                theActiveGroup->Plan();
                if (theActiveGroup->GetNumUnits() > 0)
                    thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
            } else
                theActiveGroup->Dereference();
        }
    }
}

// FUNCTION: EXE 0x4bfb0b
// Packs the heal order (header, selection id, target id) into a new send buffer
void EEMHealUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        ESVERIFY_AT(theLocation == theMessageSize, 7393);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMHealUnit::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4bfb89
// Returns the message's debug label "Heal Unit"
char* EEMHealUnit::GetMessageLabel() const
{
    return "Heal Unit";
}

// FUNCTION: EXE 0x4bfb8f
// Reads the heal order (header, selection id, target id) from a received payload
void EEMHealUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7424);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
}

bool EEMHealUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bfbdf
// Returns the immediate-unload message's payload size (14 bytes)
unsigned long EEMImmediateCargoUnload::ExportSize()
{
    return 14;
}

// FUNCTION: EXE 0x4bfbe3
// Packs the immediate-unload order (header, selection id, transport id, cargo id, flag) into a new send buffer
void EEMImmediateCargoUnload::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTransportID, sizeof(this->mTransportID));
        theLocation += sizeof(this->mTransportID);
        memcpy(*outPayloadPointer + theLocation, &this->mCargoID, sizeof(this->mCargoID));
        theLocation += sizeof(this->mCargoID);
        memcpy(*outPayloadPointer + theLocation, &this->mB18, sizeof(this->mB18));
        theLocation += sizeof(this->mB18);
        ESVERIFY_AT(theLocation == theMessageSize, 7493);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4bfc85
// Reads the immediate-unload order (header, selection id, transport id, cargo id, flag) from a received payload
void EEMImmediateCargoUnload::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7504);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTransportID, inPayloadPointer + theLocation, sizeof(this->mTransportID));
    theLocation += sizeof(this->mTransportID);
    memcpy(&this->mCargoID, inPayloadPointer + theLocation, sizeof(this->mCargoID));
    theLocation += sizeof(this->mCargoID);
    memcpy(&this->mB18, inPayloadPointer + theLocation, sizeof(this->mB18));
    theLocation += sizeof(this->mB18);
}

bool EEMImmediateCargoUnload::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4bfcfd
// Unloads one cargo unit from a living, enabled transport at once; if that fails, posts the transport class's
// unload-failed event to its owner
void EEMImmediateCargoUnload::Execute()
{
    EEUnit* theTransport = gWorld.GetUnit(this->mTransportID);
    EEUnit* theCargo = gWorld.GetUnit(this->mCargoID);
    long theText = 0;
    if (theTransport && theCargo && theTransport->mHitPoints > 0 && !theTransport->mComplexUnit->mIsDisabled
        && theTransport->mComplexUnit->GetNumCargoUnits() > 0 && theTransport->GetComplexUnit()->HasCargo(theCargo->GetComplexUnit())
        && !theTransport->GetComplexUnit()->UnloadCargoNow(theCargo->GetComplexUnit())) {
        if (theTransport->mUnitClass->IsTransport())
            theText = dynamic_cast<EEUCTransport*>(theTransport->GetUnitClass())->GetUnloadFailedTextID();
        U2DPoint<long> theTile(theTransport->mTileX, theTransport->mTileY);
        EEPlayer* theOwner = theTransport->mPlayer;
        theOwner->mEvents.AddEvent(theTile, 2, theOwner, theText, false);
    }
}

// FUNCTION: EXE 0x4bfdd2
// Shows a chat line in the local chat log if the sender's chat target (all, allies, enemies, chosen players)
// includes the local player and observers only reach observers; plays the incoming-chat sound; records the text
void EEMInGameChat::Execute()
{
    UWideString theType;
    EEPlayer* theSender = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEPlayer* theLocal = gWorld.GetCurrentPlayer();
    if (theSender && theLocal) {
        if (theLocal == theSender
            || (theSender->mSlot != kSlotNone
                && (theSender->mChatType == 0
                    || (theSender->mChatType == 1 && theSender->mEnemies[theLocal->mPlayerID] == 0)
                    || (theSender->mChatType == 2 && theSender->mEnemies[theLocal->mPlayerID] != 0)
                    || (theSender->mChatType == 3 && theSender->mSendChatMessage[theLocal->mPlayerID])))) {
            if (!theSender->mObserverMode || theLocal->mObserverMode) {
                switch (theSender->mChatType) {
                case 3:
                    gResource->GetString(IDS_SETUP_CUSTOM, theType);
                    break;
                case 2:
                    gResource->GetString(IDS_SETUP_ENEMIES, theType);
                    break;
                case 1:
                    gResource->GetString(IDS_SETUP_ALLIES, theType);
                    break;
                case 0:
                    gResource->GetString(IDS_SETUP_ALL, theType);
                    break;
                default:
                    theType = "";
                    break;
                }
                if (theSender->mObserverMode)
                    gResource->GetString(IDS_OBSERVERS, theType);
                if (theType.GetLength() != 0)
                    theType = theType + " ";
                gUserInterface->AddChatLogEntry(theSender->mColor, theSender->mName, theType + this->mText, gWorldTable.GetLongValue(25), 0);

                if (theSender != theLocal) {
                    EESoundObject* theSound = EESoundObject::Get(kSoundIncomingChatMessage);
                    if (theSound)
                        theSound->Play();
                }
            }
        }
        gWorld.mScenarioManager->mLastChatText = this->mText;
    }
}

// FUNCTION: EXE 0x4bffbc
// Packs the chat message (header, selection id, text) into a new send buffer
void EEMInGameChat::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        this->mText.Export(*outPayloadPointer + theLocation);
        theLocation += this->mText.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 7688);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c0041
// Returns the chat message's payload size: the text's export size plus 5 bytes
unsigned long EEMInGameChat::ExportSize()
{
    return this->mText.GetExportSize() + 5;
}

// FUNCTION: EXE 0x4c004e
// Returns the message's debug label "In Game Chat"
char* EEMInGameChat::GetMessageLabel() const
{
    return "In Game Chat";
}

// FUNCTION: EXE 0x4c0054
// Reads the chat message (header, selection id, text) from a received payload
void EEMInGameChat::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7731);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    this->mText.Import(inPayloadPointer + theLocation);
}

bool EEMInGameChat::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c00a2
// Packs the increment-epic message (header, selection id) into a new send buffer
void EEMIncrementEpic::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 7783);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMIncrementEpic::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c010d
// Reads the increment-epic message (header, selection id) from a received payload
void EEMIncrementEpic::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7793);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMIncrementEpic::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c0150
// Advances the sending player to the next epoch
void EEMIncrementEpic::Execute()
{
    gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->IncrementEpic();
}

// FUNCTION: EXE 0x4c0170
// Adds 60 seconds to the given goal queue's timer of every selected airport whose queue holds goals
void EEMIncrementRallyTimer::Execute()
{
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = theGroup->GetUnit(i);
        if (theUnit && theUnit->mUnitClass->mUnitType == kUnitTypeAirport) {
            EEUnitVectorItem& theItem = theUnit->mGoalQueues[this->mGoalQueue];
            if (theItem.mGoals.size() > 0)
                theItem.AddTime(60000);
        }
    }
}

// FUNCTION: EXE 0x4c01e9
// Packs the rally-timer message (header, selection id, goal queue) into a new send buffer
void EEMIncrementRallyTimer::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGoalQueue, sizeof(this->mGoalQueue));
        theLocation += sizeof(this->mGoalQueue);
        ESVERIFY_AT(theLocation == theMessageSize, 7881);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMIncrementRallyTimer::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c0265
// Returns the message's debug label "Increment Rally Timer"
char* EEMIncrementRallyTimer::GetMessageLabel() const
{
    return "Increment Rally Timer";
}

// FUNCTION: EXE 0x4c026b
// Reads the rally-timer message (header, selection id, goal queue) from a received payload
void EEMIncrementRallyTimer::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7927);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGoalQueue, inPayloadPointer + theLocation, sizeof(this->mGoalQueue));
    theLocation += sizeof(this->mGoalQueue);
}

bool EEMIncrementRallyTimer::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c02bb
// Packs the JIT-manufacturing toggle (header, selection id) into a new send buffer
void EEMJITManufacturing::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 7978);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMJITManufacturing::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c0326
// Returns the message's debug label "JIT Manufacturing"
char* EEMJITManufacturing::GetMessageLabel() const
{
    return "JIT Manufacturing";
}

// FUNCTION: EXE 0x4c032c
// Reads the JIT-manufacturing toggle (header, selection id) from a received payload
void EEMJITManufacturing::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 7992);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMJITManufacturing::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c036f
// Toggles just-in-time manufacturing on the sender's living selected units and on the player; refreshes the
// local interface
void EEMJITManufacturing::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEGroup* theGroup = thePlayer->mActiveGroup;
    unsigned long theCount = theGroup->GetNumUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theGroup->GetUnit(i);
        EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
        if (theUnit->mHitPoints > 0 && theComplexUnit) {
            theComplexUnit->ToggleJITManufacturing();
            theComplexUnit->UpdateJITManufacturing();
        }
    }
    thePlayer->mJITManufacturing = !thePlayer->mJITManufacturing;
    if (thePlayer == gWorld.GetCurrentPlayer())
        gUserInterface->UpdateJITManufacturing();
}

// FUNCTION: EXE 0x4c0404
// Packs the delete-unit order (header, selection id, all-units flag) into a new send buffer
void EEMKillUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mAllUnits, sizeof(this->mAllUnits));
        theLocation += sizeof(this->mAllUnits);
        ESVERIFY_AT(theLocation == theMessageSize, 8073);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMKillUnit::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c0480
// Reads the delete-unit order (header, selection id, all-units flag) from a received payload
void EEMKillUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8083);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mAllUnits, inPayloadPointer + theLocation, sizeof(this->mAllUnits));
    theLocation += sizeof(this->mAllUnits);
}

bool EEMKillUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c04d0
// Returns whether deleting the unit refunds its cost: a paid-for land building still under construction that is
// not a granary
// (file static; provisional name)
static bool IsRefundable(EEComplexUnit* inUnit)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    return theUnit != NULL && theUnit->mUnitClass->IsLandBuilding() && theUnit->mIsPaidFor && !theUnit->mComplexUnit->mIsBuilt
           && theUnit->mUnitClass->mUnitType != kUnitTypeGranaryImproved && theUnit->mUnitClass->mUnitType != kUnitTypeGranary;
}

// FUNCTION: EXE 0x4c051c
// Deletes the sender's first selected unit (or every selected unit) that may be deleted, refunding unfinished
// buildings' cost first
void EEMKillUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup) {
        if (!this->mAllUnits) {
            EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(theGroup->GetFirstUnit());
            if (theUnit && (theUnit->mUnitClass->mUnitType == kUnitTypePathPoint || theUnit->mHitPoints > 0) && theUnit->CanDelete()) {
                if (IsRefundable(theUnit)) {
                    EETechnology* theTechnology = theUnit->GetPlayer()->mTechnologies->Get(theUnit->GetUnitClass()->mTechnology);
                    if (theTechnology)
                        theUnit->mPlayer->RefundTechnology(theTechnology, 0x20, gWorldTable.GetFloatValue(45));
                }
                theUnit->Kill(1, 0, 0);
            }
        } else {
            unsigned long theNumUnits = theGroup->GetNumUnits();
            for (unsigned long i = 0; i < theNumUnits; i++) {
                EEComplexUnit* theUnit = theGroup->GetUnit(i);
                if (theUnit && (theUnit->GetUnitClass()->mUnitType == kUnitTypePathPoint || theUnit->GetHitPoints() > 0)
                    && theUnit->CanDelete()) {
                    if (IsRefundable(theUnit)) {
                        EETechnology* theTechnology = theUnit->GetPlayer()->mTechnologies->Get(theUnit->GetUnitClass()->mTechnology);
                        if (theTechnology)
                            theUnit->mPlayer->RefundTechnology(theTechnology, 0x20, gWorldTable.GetFloatValue(45));
                    }
                    theUnit->Kill(1, 0, 0);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x4c06aa
// Stores the network lag status and shows it (with the lag warning) in the interface
void EEMLagStatus::Execute()
{
    gWorld.mLagStatus = this->mStatus;
    gUserInterface->SetLagStatus(this->mStatus, this->mLagWarning);
}

// FUNCTION: EXE 0x4c06cd
// Packs the lag-status message (header, selection id, status, warning) into a new send buffer
void EEMLagStatus::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mStatus, sizeof(this->mStatus));
        theLocation += sizeof(this->mStatus);
        memcpy(*outPayloadPointer + theLocation, &this->mLagWarning, sizeof(this->mLagWarning));
        theLocation += sizeof(this->mLagWarning);
        ESVERIFY_AT(theLocation == theMessageSize, 8235);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c075b
// Returns the message's debug label "Lag Status"
char* EEMLagStatus::GetMessageLabel() const
{
    return "Lag Status";
}

// FUNCTION: EXE 0x4c0761
// Reads the lag-status message (header, selection id, status, warning) from a received payload
void EEMLagStatus::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8285);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mStatus, inPayloadPointer + theLocation, sizeof(this->mStatus));
    theLocation += sizeof(this->mStatus);
    memcpy(&this->mLagWarning, inPayloadPointer + theLocation, sizeof(this->mLagWarning));
    theLocation += sizeof(this->mLagWarning);
}

bool EEMLagStatus::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c07c8
// Packs the load-main-screen message (header, selection id, stop flag) into a new send buffer
void EEMLoadMainScreen::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mB10, sizeof(this->mB10));
        theLocation += sizeof(this->mB10);
        ESVERIFY_AT(theLocation == theMessageSize, 8346);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadMainScreen::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c0844
// Reads the load-main-screen message (header, selection id, stop flag) from a received payload
void EEMLoadMainScreen::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8356);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mB10, inPayloadPointer + theLocation, sizeof(this->mB10));
    theLocation += sizeof(this->mB10);
}

bool EEMLoadMainScreen::IsResponse(NEMessage* inMessage)
{
    return false;
}

// inline (header-declared; the COMDAT 0x4c0894 sits here): inPoint on the terrain, its own z off
// the mesh
inline void EEMap::GetGroundPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint)
{
    outPoint.SetX(inPoint.GetX());
    outPoint.SetY(inPoint.GetY());
    if (this->mTerrainMesh.GetIsValidTile(UMFloorUnsigned(inPoint.GetX()), UMFloorUnsigned(inPoint.GetY())))
        outPoint.SetZ(this->mTerrainMesh.GetMeshZ(inPoint.GetX(), inPoint.GetY()));
    else
        outPoint.SetZ(inPoint.GetZ());
}

// TEMPLATE: EXE 0x4c0894

// FUNCTION: EXE 0x4c0936
// With the flag set, stops the graphics thread and waits for it; otherwise restarts it, switches to the game (or
// scenario editor) screen and answers the server with a load-main-screen response
void EEMLoadMainScreen::Execute()
{
    if (this->mB10) {
        gGraphics->PostResume();
        while (true) {
            if (!gGraphics->mIsPaused)
                break;
            Sleep(1);
        }
    } else {
        gGraphics->PostPause();
        for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
            Sleep(1);
        if (gWorld.mGameScreenActive == true) {
            gUIManager->SwapForm(gUIManager->mMainGameForm);
            gUIManager->mMainGameForm->mInfoBlinking = false;
            gUIManager->mMainGameForm->mIsButtonBlinking = false;
            gUIManager->mMainGameForm->SetUIState(0x11);
        } else
            gUIManager->SwapForm(gUIManager->mScenarioEditorForm);
        gWorld.mCameraControl.Update(1.0f);
        gCommunications->ProcessLocalUserInput(
            new EEMLoadMainScreenResponse(gWorld.GetCurrentPlayer()->GetSlot(), gWorld.mCurrentPlayer), kSlotServer);
    }
}

// FUNCTION: EXE 0x4c0a3a
// Creates the response telling the server that this slot and player have reached the main screen
EEMLoadMainScreenResponse::EEMLoadMainScreenResponse(unsigned char inSlot, unsigned char inPlayerID)
    : EEMessage(kMessageLoadMainScreenResponse, true), mSlot(inSlot), mPlayerID(inPlayerID)
{
}

// FUNCTION: EXE 0x4c0a6e
// Packs the main-screen response (header, selection id, slot, player id) into a new send buffer
void EEMLoadMainScreenResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        ESVERIFY_AT(theLocation == theMessageSize, 8456);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadMainScreenResponse::ExportSize()
{
    return 7;
}

// FUNCTION: EXE 0x4c0afb
// Reads the main-screen response (header, selection id, slot, player id) from a received payload
void EEMLoadMainScreenResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8466);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
}

bool EEMLoadMainScreenResponse::IsResponse(NEMessage* inMessage)
{
    return false;
}

bool gLocalPlayerOut;                      // 0x4e6099 (.bss between this unit's Execute statics 0x4e6098/0x4e609a; ee/EEMessage.h)
// FUNCTION: EXE 0x4c0b60
// Server: counts each slot's first response; once every human (minus dropped loaders) has answered, starts the game
void EEMLoadMainScreenResponse::Execute()
{
    static std::vector<unsigned char> sResponses;
    static unsigned char sNumResponses;
    unsigned long theTime;
    if (gCommunications->IsServer()) {
        if (sNumResponses == 0)
            sResponses.clear();
        if (std::find(sResponses.begin(), sResponses.end(), this->mSlot) == sResponses.end()) {
            sResponses.push_back(this->mSlot);
            if (this->mPlayerID != 0xff)
                sNumResponses++;
            if (sNumResponses == static_cast<EEServer*>(gCommunications)->GetNumHumanPlayers() - static_cast<EEServer*>(gCommunications)->mNumDroppedLoaders) {
                theTime = TMGetTime();
                NEMStartGame* theMessage = new NEMStartGame(theTime);
                if (theMessage) {
                    static_cast<EEServer*>(gCommunications)->mLoadState = 0;
                    gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
                    static_cast<EEServer*>(gCommunications)->SetGameState(2);
                    sNumResponses = 0;
                }
            }
        }
    }
}

// FUNCTION: EXE 0x4c0ca9
// Returns the load/save message's payload size: the file name's export size plus 20
unsigned long EEMLoadOrSaveGame::ExportSize()
{
    return this->mFileName.GetExportSize() + 20;
}

// FUNCTION: EXE 0x4c0cb6
// Packs the load/save request (header, load, quiet, tick, time, auto-save, wait id, file name) into a new send buffer
void EEMLoadOrSaveGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsLoad, sizeof(this->mIsLoad));
        theLocation += sizeof(this->mIsLoad);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQuiet, sizeof(this->mIsQuiet));
        theLocation += sizeof(this->mIsQuiet);
        memcpy(*outPayloadPointer + theLocation, &this->mTick, sizeof(this->mTick));
        theLocation += sizeof(this->mTick);
        memcpy(*outPayloadPointer + theLocation, &this->mTime, sizeof(this->mTime));
        theLocation += sizeof(this->mTime);
        memcpy(*outPayloadPointer + theLocation, &this->mIsAutoSave, sizeof(this->mIsAutoSave));
        theLocation += sizeof(this->mIsAutoSave);
        memcpy(*outPayloadPointer + theLocation, &this->mWaitMessageID, sizeof(this->mWaitMessageID));
        theLocation += sizeof(this->mWaitMessageID);
        this->mFileName.Export(*outPayloadPointer + theLocation);
        theLocation += this->mFileName.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 8592);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c0dae
// Reads the load/save request (header, load, quiet, tick, time, auto-save, wait id, file name) from a received payload
void EEMLoadOrSaveGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8602);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mIsLoad, inPayloadPointer + theLocation, sizeof(this->mIsLoad));
    theLocation += sizeof(this->mIsLoad);
    memcpy(&this->mIsQuiet, inPayloadPointer + theLocation, sizeof(this->mIsQuiet));
    theLocation += sizeof(this->mIsQuiet);
    memcpy(&this->mTick, inPayloadPointer + theLocation, sizeof(this->mTick));
    theLocation += sizeof(this->mTick);
    memcpy(&this->mTime, inPayloadPointer + theLocation, sizeof(this->mTime));
    theLocation += sizeof(this->mTime);
    memcpy(&this->mIsAutoSave, inPayloadPointer + theLocation, sizeof(this->mIsAutoSave));
    theLocation += sizeof(this->mIsAutoSave);
    memcpy(&this->mWaitMessageID, inPayloadPointer + theLocation, sizeof(this->mWaitMessageID));
    theLocation += sizeof(this->mWaitMessageID);
    this->mFileName.Import(inPayloadPointer + theLocation);
}

// FUNCTION: EXE 0x4c0e6a
// Returns whether the message is the load/save response for this request's tick
bool EEMLoadOrSaveGame::IsResponse(NEMessage* inMessage)
{
    bool theResult = false;
    if (inMessage->GetType() == kMessageLoadOrSaveGameResponse && this->mTick == static_cast<EEMLoadOrSaveGameResponse*>(inMessage)->mTick)
        theResult = true;
    return theResult;
}

#include "ee/EESaveLoad.h"
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);  // 0x14cfb1

// FUNCTION: EXE 0x4c0e83
// Single-player server load: loads the saved game and logs a response; anything else queues the save with the
// physics thread; announces it in the chat log and shows the error box if loading failed
void EEMLoadOrSaveGame::Execute()
{
    UWideString theText;
    UWideString theFormat;
    UString theDirectory;
    long theResult;
    if (this->mIsLoad && gCommunications->IsServer() && !gCommunications->GetIsMultiplayerSession()) {
        gResource->GetString(IDS_LOADING, theFormat);
        sprintf(theText, theFormat, (const char*)this->mFileName);
        gUserInterface->AddChatLogEntry(theText, 0);
        theDirectory = "data\\";
        theDirectory += "Saved Games\\";
        theResult = EELoadSavedGameFile(UString(this->mFileName), true, this->mIsQuiet, theDirectory, 1);
        EEMLoadOrSaveGameResponse* theResponse = new EEMLoadOrSaveGameResponse(this->mTick);
        theResponse->SetOriginatingSlot(gWorld.GetCurrentPlayer()->mSlot);
        static_cast<NESynchronousServer*>(gCommunications)->LogResponse(theResponse);
    } else {
        gPhysics->QueueMessage(this);
        gResource->GetString(IDS_SAVING_GAME_AS, theFormat);
        sprintf(theText, theFormat, (const char*)this->mFileName);
        gUserInterface->AddChatLogEntry(theText, 0);
        theResult = 0;
    }
    if (theResult != 0)
        gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
}

// FUNCTION: EXE 0x4c1025
// Creates the response acknowledging the load/save request of the given tick
EEMLoadOrSaveGameResponse::EEMLoadOrSaveGameResponse(long inTick)
    : EEMessage(kMessageLoadOrSaveGameResponse, true), mTick(inTick)
{
}

void EEMLoadOrSaveGameResponse::Execute()
{
}

extern long gAutoSaveCount;                 // 0x5308b8 (the auto-save count LoadOrSave passes on; provisional name)
// (0x14cbc7 Fn14CBC7: ee/EESaveLoad.h)

// FUNCTION: EXE 0x4c1052
// Saves the game under the requested name, rotates the auto-saves for an auto-save, restores the mouse and reports
// an error box on failure; returns the request's tick
long EEMLoadOrSaveGame::LoadOrSave()
{
    GE2DPoint thePosition = gMouse->mPosition;
    UString theName;
    UString theDirectory;
    UWideString theText;
    UWideString theFormat;
    UString theFolder;
    theFolder = "data\\";
    theFolder += "Saved Games\\";
    gISMouse->SetUseBufferedMovement(false);
    long theResult = SaveGame(UString(this->mFileName), this->mTime, this->mIsQuiet, theFolder);
    if (this->mIsAutoSave == 1) {
        theDirectory = UString("data\\") + UString("Saved Games\\");
        theName = UString("AutoSave") + UString("*") + gSavedGameExtension;
        Fn14CBC7(theName, theDirectory, gAutoSaveCount);
    }
    if (gCommunications->IsServer() && !this->mIsQuiet && theResult == 0)
        gUserInterface->CloseForm(NULL);
    gISMouse->Refresh();
    gISMouse->SetUseBufferedMovement(true);
    gMouse->Update();
    gMouse->SetPosition(thePosition);
    if (theResult != 0)
        gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
    return this->mTick;
}

// FUNCTION: EXE 0x4c1264
// Packs the load/save response (header, selection id, tick) into a new send buffer

void EEMLoadOrSaveGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTick, sizeof(this->mTick));
        theLocation += sizeof(this->mTick);
        ESVERIFY_AT(theLocation == theMessageSize, 8793);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadOrSaveGameResponse::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c12e2
// Reads the load/save response (header, selection id, tick) from a received payload
void EEMLoadOrSaveGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8803);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTick, inPayloadPointer + theLocation, sizeof(this->mTick));
    theLocation += sizeof(this->mTick);
}

bool EEMLoadOrSaveGameResponse::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c1332
// Packs the load-progress message (header, selection id, slot, percent) into a new send buffer
void EEMLoadPercent::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        memcpy(*outPayloadPointer + theLocation, &this->mPercent, sizeof(this->mPercent));
        theLocation += sizeof(this->mPercent);
        ESVERIFY_AT(theLocation == theMessageSize, 8866);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadPercent::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c13c3
// Reads the load-progress message (header, selection id, slot, percent) from a received payload
void EEMLoadPercent::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8876);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
    memcpy(&this->mPercent, inPayloadPointer + theLocation, sizeof(this->mPercent));
    theLocation += sizeof(this->mPercent);
}

bool EEMLoadPercent::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c142a
// Shows a slot's loading progress in the interface, capped at 100 percent
void EEMLoadPercent::Execute()
{
    if (this->mPercent > 100)
        this->mPercent = 100;
    gUserInterface->SetLoadPercent(this->mSlot, this->mPercent);
}

// FUNCTION: EXE 0x4c1447
// Packs the load-unit-data message (header, selection id, unit type, flag) into a new send buffer
void EEMLoadUnitData::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitType, sizeof(this->mUnitType));
        theLocation += sizeof(this->mUnitType);
        memcpy(*outPayloadPointer + theLocation, &this->mB14, sizeof(this->mB14));
        theLocation += sizeof(this->mB14);
        ESVERIFY_AT(theLocation == theMessageSize, 8943);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadUnitData::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c14d6
// Reads the load-unit-data message (header, selection id, unit type, flag) from a received payload
void EEMLoadUnitData::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8953);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitType, inPayloadPointer + theLocation, sizeof(this->mUnitType));
    theLocation += sizeof(this->mUnitType);
    memcpy(&this->mB14, inPayloadPointer + theLocation, sizeof(this->mB14));
    theLocation += sizeof(this->mB14);
}

bool EEMLoadUnitData::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c153d
// Changes the sender's cheat unit class: unloads the old class's data for every player and preloads the new one
void EEMLoadUnitData::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer->mHackUnitClass != this->mUnitType) {
        unsigned long i;
        for (i = 1; i < gWorld.mNumPlayers; i++)
            if (thePlayer->mHackUnitClass < 0x367 && gUnitClassTable[thePlayer->mHackUnitClass])
                gDataPreLoading->PostUnloadUnitClass(gWorld.mPlayers[i]->mPlayerID, thePlayer->mHackUnitClass);
        thePlayer->SetHackUnitClass(this->mUnitType);
        for (i = 1; i < gWorld.mNumPlayers; i++)
            if (thePlayer->mHackUnitClass < 0x367 && gUnitClassTable[thePlayer->mHackUnitClass])
                gDataPreLoading->PostLoadUnitClass(gWorld.mPlayers[i]->mPlayerID, thePlayer->mHackUnitClass);
    }
}

// FUNCTION: EXE 0x4c1608
// Packs the garrison order (header, selection id, garrison id, unit id, flag) into a new send buffer
void EEMLoadGarrison::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGarrisonID, sizeof(this->mGarrisonID));
        theLocation += sizeof(this->mGarrisonID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        memcpy(*outPayloadPointer + theLocation, &this->mB18, sizeof(this->mB18));
        theLocation += sizeof(this->mB18);
        ESVERIFY_AT(theLocation == theMessageSize, 9046);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadGarrison::ExportSize()
{
    return 14;
}

// FUNCTION: EXE 0x4c16aa
// Reads the garrison order (header, selection id, garrison id, unit id, flag) from a received payload
void EEMLoadGarrison::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9056);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGarrisonID, inPayloadPointer + theLocation, sizeof(this->mGarrisonID));
    theLocation += sizeof(this->mGarrisonID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
    memcpy(&this->mB18, inPayloadPointer + theLocation, sizeof(this->mB18));
    theLocation += sizeof(this->mB18);
}

bool EEMLoadGarrison::IsResponse(NEMessage* inMessage)
{
    return false;
}

#include "ee/EEUGLocation.h"
#include "ee/EEUGLoadGarrison.h"

// FUNCTION: EXE 0x4c1722
// Sends the sender's selected units that can garrison into the target building (joining its load goal or starting
// one); the others move next to it; posts an event if the goal is invalid
void EEMLoadGarrison::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theSelection = thePlayer->mActiveGroup;
    EEUnit* theGarrisonUnit = gWorld.GetUnit(this->mGarrisonID);
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEComplexUnit*> theRejected;
    if (theGarrisonUnit == NULL || theGarrisonUnit->mTileNode == NULL || theGarrisonUnit->mHitPoints <= 0)
        return;
    EEComplexUnit* theGarrison = theGarrisonUnit->mComplexUnit;
    EEUnitGoal* theOldGoal = theGarrison->mGoalState;
    if (theSelection->GetFirstUnit() == NULL || theGarrison->mUnitClass->mGarrisonCapacity <= 0)
        return;
    unsigned long theCount = theSelection->GetNumUnits();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theSelection->GetUnit(i);
        if (theUnit != theGarrison && theUnit->mHitPoints > 0)
            (theUnit->mUnitClass->mCanGarrison ? theUnits : theRejected).push_back(theUnit);
    }
    if (theUnits.size() > 0) {
        EEUGLoadGarrison* theGoal;
        if (theOldGoal != NULL && theOldGoal->mKind == kGoalLoadGarrison) {
            theGoal = dynamic_cast<EEUGLoadGarrison*>(theOldGoal);
            theGoal->AddUnits(theUnits);
        } else {
            theGoal = new EEUGLoadGarrison(theGarrison, theUnits, false);
            if (theGoal->mIsValid) {
                theGarrison->SetGoal(theGoal, 0, 0);
                if (theGarrison->mStats->mMaxVelocity > 0.0f) theGarrison->StartAction(kActionPrepareToMove); else theGarrison->StartAction(kActionPickedUpLoadableUnits);
                theGarrison->mUnitClass->ActionChanged(theGarrison);
            }
        }
        if (theGoal->mIsValid) {
            for (std::vector<EEComplexUnit*>::iterator theIterator = theUnits.begin(); theIterator != theUnits.end();
                 theIterator++) {
                EEComplexUnit* theUnit = *theIterator;
                theUnit->SetGoal(theGoal->Clone(), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
                theUnit->mUnitClass->ActionChanged(theUnit);
            }
        } else {
            U2DPoint<long> theTile(theGarrisonUnit->mTileX, theGarrisonUnit->mTileY);
            thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, 0x7c3a, false);
        }
    }
    if (theRejected.size() > 0) {
        for (unsigned long i = 1; i < 6; i++) {
            EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
            for (std::vector<EEComplexUnit*>::iterator theIterator = theRejected.begin();
                 theIterator != theRejected.end(); theIterator++) {
                if (theGroup->IsInFormationSlot(*theIterator, i))
                    theGroup->AddMember(*theIterator, true);
            }
            theGroup->KeepMobileUnits();
            if (theGroup->GetNumUnits() > 0) {
                theGroup->SetGoal(new EEUGLocation(GE2DPoint((float)theGarrison->mTileX, (float)theGarrison->mTileY),
                                                   true, true, false, 0.0f, false, 0), false);
                theGroup->mAction = kActionPrepareToMove;
                thePlayer->mPendingGroups.push_back(theGroup);
                theGroup->Plan();
            } else
                theGroup->Dereference();
        }
    }
}

// FUNCTION: EXE 0x4c1a5f
// Packs the load-transport order (header, selection id, transport id, passenger id, flag) into a new send buffer
void EEMLoadTransport::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTransportID, sizeof(this->mTransportID));
        theLocation += sizeof(this->mTransportID);
        memcpy(*outPayloadPointer + theLocation, &this->mPassengerID, sizeof(this->mPassengerID));
        theLocation += sizeof(this->mPassengerID);
        memcpy(*outPayloadPointer + theLocation, &this->mB18, sizeof(this->mB18));
        theLocation += sizeof(this->mB18);
        ESVERIFY_AT(theLocation == theMessageSize, 9265);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLoadTransport::ExportSize()
{
    return 14;
}

// FUNCTION: EXE 0x4c1b01
// Reads the load-transport order (header, selection id, transport id, passenger id, flag) from a received payload
void EEMLoadTransport::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9275);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTransportID, inPayloadPointer + theLocation, sizeof(this->mTransportID));
    theLocation += sizeof(this->mTransportID);
    memcpy(&this->mPassengerID, inPayloadPointer + theLocation, sizeof(this->mPassengerID));
    theLocation += sizeof(this->mPassengerID);
    memcpy(&this->mB18, inPayloadPointer + theLocation, sizeof(this->mB18));
    theLocation += sizeof(this->mB18);
}

bool EEMLoadTransport::IsResponse(NEMessage* inMessage)
{
    return false;
}

#include "ee/EEUGLocation.h"
#include "ee/EEUGLoadTransport.h"

// FUNCTION: EXE 0x4c1b79
// Boards the selected units (or one passenger) onto the transport via its load goal; units it can't carry move to
// the pickup tile; acknowledges with the transport's voice
void EEMLoadTransport::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theSelection = thePlayer->mActiveGroup;
    EEUnit* theTransportUnit = gWorld.GetUnit(this->mTransportID);
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEComplexUnit*> theRejected;
    if (theTransportUnit == NULL || theTransportUnit->mTileNode == NULL || theTransportUnit->mHitPoints <= 0)
        return;
    EEComplexUnit* theTransport = theTransportUnit->mComplexUnit;
    EEUnitGoal* theOldGoal = theTransport->GetLoadGoal();
    if (theSelection->GetFirstUnit() == NULL || !theTransport->mUnitClass->IsTransport())
        return;
    if (this->mPassengerID == -1) {
        unsigned long theCount = theSelection->GetNumUnits();
        for (unsigned long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = theSelection->GetUnit(i);
            if (theUnit != theTransport && theUnit->mHitPoints > 0)
                theUnits.push_back(theUnit);
        }
    } else if (gWorld.GetUnit(this->mPassengerID)->mComplexUnit->mHitPoints > 0) {
        EEComplexUnit* theUnit = gWorld.GetUnit(this->mPassengerID)->mComplexUnit;
        theUnits.push_back(theUnit);
    }
    dynamic_cast<EEUCTransport*>(theTransport->mUnitClass)->FilterLoadable(&theUnits, theTransport, &theRejected);
    long theTileX;
    long theTileY;
    if (theUnits.size() != 0) {
        EEUGLoadTransport* theGoal;
        if (theOldGoal != NULL && theOldGoal->mKind == kGoalLoadTransport) {
            theGoal = dynamic_cast<EEUGLoadTransport*>(theOldGoal);
            theGoal->AddUnits(theUnits);
        } else {
            theGoal = new EEUGLoadTransport(theTransport, theUnits, true);
            theTransport->SetGoal(theGoal, 0, 0);
            theTransport->StartAction(theTransport->mStats->mMaxVelocity > 0.0f ? 8000 : 0x2ee3);
            theTransport->mUnitClass->ActionChanged(theTransport);
        }
        theTileX = theGoal->mUnitTile.GetX();
        theTileY = theGoal->mUnitTile.GetY();
        for (std::vector<EEComplexUnit*>::iterator theIterator = theUnits.begin(); theIterator != theUnits.end();
             theIterator++) {
            EEComplexUnit* theUnit = *theIterator;
            theUnit->SetGoal(theGoal->Clone(), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->mUnitClass->ActionChanged(theUnit);
        }
    } else {
        theTileX = theTransport->mTileX;
        theTileY = theTransport->mTileY;
    }
    if (theRejected.size() != 0) {
        for (unsigned long i = 1; i < 6; i++) {
            EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
            for (std::vector<EEComplexUnit*>::iterator theIterator = theRejected.begin();
                 theIterator != theRejected.end(); theIterator++) {
                if (theGroup->IsInFormationSlot(*theIterator, i))
                    theGroup->AddMember(*theIterator, true);
            }
            theGroup->KeepMobileUnits();
            if (theGroup->GetNumUnits() > 0) {
                theGroup->SetGoal(new EEUGLocation(GE2DPoint((float)theTileX, (float)theTileY), true, true, false, 0.0f,
                                                   false, 0), false);
                theGroup->mAction = kActionPrepareToMove;
                thePlayer->mPendingGroups.push_back(theGroup);
                theGroup->Plan();
            } else
                theGroup->Dereference();
        }
    }
    thePlayer->PlayAcknowledgement(theTransport);
}

// FUNCTION: EXE 0x4c1ee0
// Returns the message's payload size (6 bytes)
unsigned long EEMChangeSelectedUnitCollisionSphere::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c1ee4
// Packs the gate lock order (header, selection id, locked flag) into a new send buffer
void EEMLockUnlock::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsLocked, sizeof(this->mIsLocked));
        theLocation += sizeof(this->mIsLocked);
        ESVERIFY_AT(theLocation == theMessageSize, 9491);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMLockUnlock::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c1f60
// Returns the message's debug label "Lock Unlock"
char* EEMLockUnlock::GetMessageLabel() const
{
    return "Lock Unlock";
}

// FUNCTION: EXE 0x4c1f66
// Reads the gate lock order (header, selection id, locked flag) from a received payload
void EEMLockUnlock::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9515);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mIsLocked, inPayloadPointer + theLocation, sizeof(this->mIsLocked));
    theLocation += sizeof(this->mIsLocked);
}

bool EEMLockUnlock::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c1fb6
// Locks or unlocks every selected gate together with its second and fourth parts
void EEMLockUnlock::Execute()
{
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    if (theGroup->GetFirstUnit()) {
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            EEUCGate* theGate = dynamic_cast<EEUCGate*>(theUnit->GetUnitClass());
            theUnit->SetLocked(this->mIsLocked);
            // a gate built as a single piece (scenario Create effect) has no second or fourth part
            EEComplexUnit* thePart = theGate->GetSecondPart(theUnit);
            if (thePart != NULL)
                thePart->SetLocked(this->mIsLocked);
            thePart = theGate->GetFourthPart(theUnit);
            if (thePart != NULL)
                thePart->SetLocked(this->mIsLocked);
        }
    }
}

// FUNCTION: EXE 0x4c207d
// Packs the reveal-map cheat (header, selection id) into a new send buffer
void EEMMakeWorldVisible::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 9594);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMMakeWorldVisible::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c20e8
// Reads the reveal-map cheat (header, selection id) from a received payload
void EEMMakeWorldVisible::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9604);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMMakeWorldVisible::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c212b
// Toggles whether the sending player sees the whole map; redraws the mini-map for the local player
void EEMMakeWorldVisible::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->mSeesAll = !thePlayer->mSeesAll;
    if (thePlayer == gWorld.GetCurrentPlayer())
        gMiniMap->Redraw();
}

// FUNCTION: EXE 0x4c2170
// After a host migration, moves a player to the new slot; a player left without a slot is announced as dropped and
// (unless observing) removed from the game as defeated; tells the local client a new host is set
void EEMMigrationSlotInfo::Execute()
{
    UWideString theText;
    UWideString theFormat;
    if (this->mSlot == kSlotNone) {
        unsigned char theSlot = gWorld.mPlayers[this->mPlayerID]->mSlot;
        if (theSlot != kSlotNone && theSlot != kSlotServer) {
            gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
            sprintf(theText, theFormat, (const char*)gWorld.mPlayers[this->mPlayerID]->mName);
            gUserInterface->AddChatLogEntry(theText, 1);
        }
        if (!gWorld.mPlayers[this->mPlayerID]->mObserverMode) {
            gWorld.RemovePlayerFromGame(0, this->mPlayerID);
            gWorld.mPlayers[this->mPlayerID]->SetGameResult(2);
        }
    }
    gWorld.mPlayers[this->mPlayerID]->mSlot = this->mSlot;
    if (this->mPlayerID == gWorld.GetCurrentPlayer()->mPlayerID && gCommunications->IsClient()) {
        gResource->GetString(IDS_NEW_HOST_ESTABLISHED, theText);
        gUserInterface->AddChatLogEntry(theText, 0);
    }
}

// FUNCTION: EXE 0x4c2297
// Packs the migration slot message (header, selection id, player id, slot) into a new send buffer
void EEMMigrationSlotInfo::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        ESVERIFY_AT(theLocation == theMessageSize, 9730);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMMigrationSlotInfo::ExportSize()
{
    return 7;
}

// FUNCTION: EXE 0x4c2324
// Reads the migration slot message (header, selection id, player id, slot) from a received payload
void EEMMigrationSlotInfo::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9746);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
}

bool EEMMigrationSlotInfo::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c2389
// Returns the edict message's payload size (12 bytes)
unsigned long EEMNewEdictEnacted::ExportSize()
{
    return 12;
}

void EEMNewEdictEnacted::Execute()
{
}

// FUNCTION: EXE 0x4c238d
// Packs the edict vote result (header, selection id, edict, yes and no votes, enacted flag) into a new send buffer
void EEMNewEdictEnacted::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mEdict, sizeof(this->mEdict));
        theLocation += sizeof(this->mEdict);
        memcpy(*outPayloadPointer + theLocation, &this->mYesVotes, sizeof(this->mYesVotes));
        theLocation += sizeof(this->mYesVotes);
        memcpy(*outPayloadPointer + theLocation, &this->mNoVotes, sizeof(this->mNoVotes));
        theLocation += sizeof(this->mNoVotes);
        memcpy(*outPayloadPointer + theLocation, &this->mIsEnacted, sizeof(this->mIsEnacted));
        theLocation += sizeof(this->mIsEnacted);
        ESVERIFY_AT(theLocation == theMessageSize, 9819);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c2443
// Reads the edict vote result (header, selection id, edict, yes and no votes, enacted flag) from a received payload
void EEMNewEdictEnacted::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9829);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mEdict, inPayloadPointer + theLocation, sizeof(this->mEdict));
    theLocation += sizeof(this->mEdict);
    memcpy(&this->mYesVotes, inPayloadPointer + theLocation, sizeof(this->mYesVotes));
    theLocation += sizeof(this->mYesVotes);
    memcpy(&this->mNoVotes, inPayloadPointer + theLocation, sizeof(this->mNoVotes));
    theLocation += sizeof(this->mNoVotes);
    memcpy(&this->mIsEnacted, inPayloadPointer + theLocation, sizeof(this->mIsEnacted));
    theLocation += sizeof(this->mIsEnacted);
}

bool EEMNewEdictEnacted::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c24cb
// Packs the pack/unpack order (header, selection id, pack flag) into a new send buffer
void EEMPackUnpack::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPack, sizeof(this->mPack));
        theLocation += sizeof(this->mPack);
        ESVERIFY_AT(theLocation == theMessageSize, 9894);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMPackUnpack::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c2547
// Returns the message's debug label "Pack Unpack"
char* EEMPackUnpack::GetMessageLabel() const
{
    return "Pack Unpack";
}

// FUNCTION: EXE 0x4c254d
// Reads the pack/unpack order (header, selection id, pack flag) from a received payload
void EEMPackUnpack::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 9918);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPack, inPayloadPointer + theLocation, sizeof(this->mPack));
    theLocation += sizeof(this->mPack);
}

bool EEMPackUnpack::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c259d
// Returns whether the unpacked unit class can be placed for the player on every tile of its footprint at x, y
static bool CanUnpack(EEUnitClass* inClass, EEPlayer* inPlayer, long inX, long inY)
{
    EEMap* theMap = gWorld.mMap;
    GE3DPoint theLocation((float)inX, (float)inY, 0.0f);
    long theType = inClass->mUnitType;
    long theSize = inClass->mTileSize;
    bool theResult = true;
    long theStartX;
    long theStartY;
    if (theSize <= 1) {
        theStartX = inX;
        theStartY = inY;
        theSize = 1;
    } else {
        long theHalf = (long)(theSize * 0.5f);
        theStartX = inX - theHalf;
        theStartY = inY - theHalf;
    }
    long theEndX = theStartX + theSize;
    long theEndY = theStartY + theSize;
    for (long x = theStartX; x <= theEndX; x++)
        for (long y = theStartY; y <= theEndY; y++) {
            bool theOK;
            if (theMap->GetTile(x, y))
                theOK = EECanPlace(inPlayer, theType, &theLocation, x, y, 0, 0);
            else
                theOK = false;
            if (x < theEndX && y < theEndY && !theOK)
                theResult = false;
        }
    return theResult;
}

// FUNCTION: EXE 0x4c2683
// Packs or unpacks the sender's living selected units by replacing them with their packed/unpacked class; units
// with no room to unpack stay packed and the local player gets a chat warning
void EEMPackUnpack::Execute()
{
    GETransformation theTransformation;
    UWideString theText;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup->GetFirstUnit()) {
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theUnit->mHitPoints > 0) {
                if (this->mPack && !theUnit->mIsPacked) {
                    EEUnitClass* thePacked = theUnit->mUnitClass->mPackedClass;
                    if (thePacked)
                        EEReplaceUnit(theUnit, thePlayer->GetPlayerID(), thePacked)->SetIsPacked(true);
                } else if (!this->mPack && theUnit->mIsPacked) {
                    EEUnitClass* theUnpacked = theUnit->mUnitClass->mPackedClass;
                    if (theUnpacked) {
                        if (CanUnpack(theUnpacked, thePlayer, theUnit->GetTileX(), theUnit->GetTileY())) {
                            EEComplexUnit* theNew = EEReplaceUnit(theUnit, thePlayer->GetPlayerID(), theUnpacked);
                            EEPlaceUnit(theNew, theNew->mUnitClass, thePlayer, U2DPoint<long>(theUnit->mTileX, theUnit->mTileY),
                                        true);
                            theNew->SetIsPacked(false);
                        } else if (theText.GetLength() == 0)
                            gResource->GetString(IDS_YOU_MAY_NOT_UNPACK_ALL_UNITS, theText);
                    }
                }
            }
        }
    }
    if (theText.GetLength() > 0 && thePlayer == gWorld.GetCurrentPlayer())
        gUserInterface->AddChatLogEntry(theText, 0);
}

#include "lle/GE3DLine.h"

// FUNCTION: EXE 0x4c281c
// Moves the sender's selection to the target point (snapped onto the map and off blocking large units), one
// group per formation slot, with facing, forced, queued and group-movement options; acknowledges by voice
void EEMPathTo::Execute()
{
    GE3DLine theLine;
    GE3DPoint theLocation;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theSelection = thePlayer->mActiveGroup;
    unsigned long thePlayerID = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mPlayerID;
    long theX = (long)this->mX;
    long theY = (long)this->mY;
    EEComplexUnit* theFirstUnit = theSelection->GetFirstUnit();
    if (theFirstUnit == NULL)
        return;
    EETile* theTile = gWorld.GetMap()->GetTile(theX, theY);
    if (theTile == NULL) {
        GE3DPoint thePoint((float)theX + 0.5f, (float)theY + 0.5f, 0.0f);
        theTile = gWorld.GetMap()->GetClosestTile(&thePoint, &theX, &theY);
        this->mX = (float)theX + 0.5f;
        this->mY = (float)theY + 0.5f;
    }
    for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
        if (theNode->mUnit->mUnitClass->mTileSize > 1 && !theNode->mUnit->IsPassableBy(theFirstUnit->GetComplexUnit())) {
            theFirstUnit->GetNearestFootprintTile(theNode->mUnit, &theX, &theY);
            theTile = gWorld.GetMap()->GetTile(theX, theY);
            if (theTile != NULL) {
                this->mX = (float)theX + 0.5f;
                this->mY = (float)theY + 0.5f;
                break;
            }
            theX = (long)this->mX;
            theY = (long)this->mY;
            theTile = gWorld.GetMap()->GetTile(theX, theY);
        }
    }
    theLocation.SetX(this->mX);
    theLocation.SetY(this->mY);
    theLocation.SetZ(theTile->mHeight >= 0.0f ? theTile->mHeight : 0.0f);
    for (unsigned long i = 1; i < 6; i++) {
        EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
        theGroup->Fill(theSelection, 0, i);
        if (theGroup->GetNumUnits() > 0) {
            thePlayer->mPendingGroups.push_back(theGroup);
            theGroup->KeepMobileUnits();
            EEUGLocation* theGoal = new EEUGLocation(&theLocation, true, this->mIsForcedMove, this->mIsRotating, this->mFacing, false, 0);
            theGoal->mUseGroupMovement = this->mUseGroupMovement;
            theGroup->ResetUnitsPaths();
            theGroup->ClearGoal();
            theGroup->SetGoal(theGoal, this->mIsQueued);
            theGroup->mAction = kActionPrepareToMove;
            theGroup->Plan();
            if (theGroup->GetNumUnits() > 0)
                thePlayer->PlayAcknowledgement(theGroup->GetUnit(gPhysics->GetRandom(0L, (long)theGroup->GetNumUnits() - 1)));
            EESendGroupCommand(theGroup, NULL, thePlayerID, 0xb8, 0);
        } else
            theGroup->Dereference();
    }
    EE_LOST_DEBUG(12); }                    // LOST-DEBUG: none: exact only (lostdebug_search, end)

// FUNCTION: EXE 0x4c2b41
// Packs the move order (header, facing, x, y, forced, rotating, queued, group-movement flags) into a new send buffer
void EEMPathTo::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mFacing, sizeof(this->mFacing));
        theLocation += sizeof(this->mFacing);
        memcpy(*outPayloadPointer + theLocation, &this->mX, sizeof(this->mX));
        theLocation += sizeof(this->mX);
        memcpy(*outPayloadPointer + theLocation, &this->mY, sizeof(this->mY));
        theLocation += sizeof(this->mY);
        memcpy(*outPayloadPointer + theLocation, &this->mIsForcedMove, sizeof(this->mIsForcedMove));
        theLocation += sizeof(this->mIsForcedMove);
        memcpy(*outPayloadPointer + theLocation, &this->mIsRotating, sizeof(this->mIsRotating));
        theLocation += sizeof(this->mIsRotating);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        memcpy(*outPayloadPointer + theLocation, &this->mUseGroupMovement, sizeof(this->mUseGroupMovement));
        theLocation += sizeof(this->mUseGroupMovement);
        ESVERIFY_AT(theLocation == theMessageSize, 10271);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c2c30
// Returns the message's debug label "Path To"
char* EEMPathTo::GetMessageLabel() const
{
    return "Path To";
}

// FUNCTION: EXE 0x4c2c36
// Reads the move order (header, facing, x, y, forced, rotating, queued, group-movement flags) from a received payload
void EEMPathTo::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 10326);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mFacing, inPayloadPointer + theLocation, sizeof(this->mFacing));
    theLocation += sizeof(this->mFacing);
    memcpy(&this->mX, inPayloadPointer + theLocation, sizeof(this->mX));
    theLocation += sizeof(this->mX);
    memcpy(&this->mY, inPayloadPointer + theLocation, sizeof(this->mY));
    theLocation += sizeof(this->mY);
    memcpy(&this->mIsForcedMove, inPayloadPointer + theLocation, sizeof(this->mIsForcedMove));
    theLocation += sizeof(this->mIsForcedMove);
    memcpy(&this->mIsRotating, inPayloadPointer + theLocation, sizeof(this->mIsRotating));
    theLocation += sizeof(this->mIsRotating);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
    memcpy(&this->mUseGroupMovement, inPayloadPointer + theLocation, sizeof(this->mUseGroupMovement));
    theLocation += sizeof(this->mUseGroupMovement);
}

bool EEMPathTo::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c2cf2
// Patrol order: a selected path point gets the target's tile as a new point; other units patrol the target's
// route (target id -1: the nearest town's patrol route); acknowledges by voice
void EEMPatrol::Execute()
{
    GE2DPoint thePoint;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEComplexUnit* theUnit = theGroup->GetFirstUnit();
    unsigned long theTargetID = this->mTargetID;
    EEComplexUnit* theTarget = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theTargetID));
    unsigned long thePlayerID = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mPlayerID;
    if (theUnit && this->mTargetID == -1) {
        EETown* theSite = theUnit->FindNearestTown();
        if (theSite && theSite->mPatrolGroup)
            theTarget = theSite->mPatrolGroup->GetNearest(theUnit->GetTileX(), theUnit->GetTileY());
    }
    if (theTarget && theUnit) {
        if (theUnit->mUnitClass->mUnitType == kUnitTypePathPoint) {
            EEPPatrolGroup* theUnitPatrolGroup = theUnit->mPatrolGroup;
            thePoint.SetX((float)theTarget->mTileX);
            thePoint.SetY((float)theTarget->mTileY);
            theUnitPatrolGroup->AddPoint(thePoint, thePlayer->mPlayerID);
        } else {
            EEPPatrolGroup* thePatrolGroup = theTarget->mPatrolGroup;
            for (unsigned long i = 1; i < 6; i++) {
                EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
                theActiveGroup->Fill(theGroup, 0, i);
                if (theActiveGroup->GetNumUnits() > 0) {
                    thePlayer->mPendingGroups.push_back(theActiveGroup);
                    theActiveGroup->KeepMobileUnits();
                    EEUGPatrol* theGoal = new EEUGPatrol(thePatrolGroup, theTarget, true);
                    theActiveGroup->ClearGoal();
                    theActiveGroup->SetGoal(theGoal, false);
                    theActiveGroup->mAction = kActionMoveToNextPatrolPoint;
                    theActiveGroup->Plan();
                    EESendGroupCommand(theActiveGroup, NULL, thePlayerID, 0xba, 0);
                    if (theActiveGroup->GetNumUnits() > 0)
                        thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
                } else
                    theActiveGroup->Dereference();
            }
        }
    }
}

// FUNCTION: EXE 0x4c2ef5
// Packs the patrol order (header, selection id, target id) into a new send buffer
void EEMPatrol::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        ESVERIFY_AT(theLocation == theMessageSize, 10497);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMPatrol::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c2f73
// Returns the message's debug label "Patrol"
char* EEMPatrol::GetMessageLabel() const
{
    return "Patrol";
}

// FUNCTION: EXE 0x4c2f79
// Reads the patrol order (header, selection id, target id) from a received payload
void EEMPatrol::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 10546);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
}

bool EEMPatrol::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c2fc9
// Returns the tribute message's payload size (26 bytes)
unsigned long EEMPayTribute::ExportSize()
{
    return 26;
}

// FUNCTION: EXE 0x4c2fcd
// Packs the tribute (header, selection id, food, gold, iron, stone, wood, receiver) into a new send buffer
void EEMPayTribute::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mFood, sizeof(this->mFood));
        theLocation += sizeof(this->mFood);
        memcpy(*outPayloadPointer + theLocation, &this->mGold, sizeof(this->mGold));
        theLocation += sizeof(this->mGold);
        memcpy(*outPayloadPointer + theLocation, &this->mIron, sizeof(this->mIron));
        theLocation += sizeof(this->mIron);
        memcpy(*outPayloadPointer + theLocation, &this->mStone, sizeof(this->mStone));
        theLocation += sizeof(this->mStone);
        memcpy(*outPayloadPointer + theLocation, &this->mWood, sizeof(this->mWood));
        theLocation += sizeof(this->mWood);
        memcpy(*outPayloadPointer + theLocation, &this->mReceiver, sizeof(this->mReceiver));
        theLocation += sizeof(this->mReceiver);
        ESVERIFY_AT(theLocation == theMessageSize, 10624);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c30af
// Reads the tribute (header, selection id, food, gold, iron, stone, wood, receiver) from a received payload
void EEMPayTribute::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 10634);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mFood, inPayloadPointer + theLocation, sizeof(this->mFood));
    theLocation += sizeof(this->mFood);
    memcpy(&this->mGold, inPayloadPointer + theLocation, sizeof(this->mGold));
    theLocation += sizeof(this->mGold);
    memcpy(&this->mIron, inPayloadPointer + theLocation, sizeof(this->mIron));
    theLocation += sizeof(this->mIron);
    memcpy(&this->mStone, inPayloadPointer + theLocation, sizeof(this->mStone));
    theLocation += sizeof(this->mStone);
    memcpy(&this->mWood, inPayloadPointer + theLocation, sizeof(this->mWood));
    theLocation += sizeof(this->mWood);
    memcpy(&this->mReceiver, inPayloadPointer + theLocation, sizeof(this->mReceiver));
    theLocation += sizeof(this->mReceiver);
}

bool EEMPayTribute::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c315b
// Gives the tributed resources to the receiver and records them for both players; the giver and receiver get a
// chat line listing the amounts
void EEMPayTribute::Execute()
{
    UWideString theYouGave;
    UWideString theFood;
    UWideString theWood;
    UWideString theStone;
    UWideString theGold;
    UWideString theIron;
    UWideString theText(0x400);
    UWideString theSpace(" ");
    EEPlayer* theFrom = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEPlayer* theTo = gWorld.mPlayers[this->mReceiver];
    unsigned long theAmount;
    char theNumber[8];
    theAmount = this->mFood;
    if (theAmount > 0) {
        theFrom->AddTribute(0, 2, theAmount);
        theTo->AddResource(kResourceFood, theAmount);
        theTo->AddTribute(1, 2, theAmount);
    }
    theAmount = this->mGold;
    if (theAmount > 0) {
        theFrom->AddTribute(0, 5, theAmount);
        theTo->AddResource(kResourceGold, theAmount);
        theTo->AddTribute(1, 5, theAmount);
    }
    theAmount = this->mIron;
    if (theAmount > 0) {
        theFrom->AddTribute(0, 6, theAmount);
        theTo->AddResource(kResourceIron, theAmount);
        theTo->AddTribute(1, 6, theAmount);
    }
    theAmount = this->mStone;
    if (theAmount > 0) {
        theFrom->AddTribute(0, 4, theAmount);
        theTo->AddResource(kResourceStone, theAmount);
        theTo->AddTribute(1, 4, theAmount);
    }
    theAmount = this->mWood;
    if (theAmount > 0) {
        theFrom->AddTribute(0, 3, theAmount);
        theTo->AddResource(kResourceWood, theAmount);
        theTo->AddTribute(1, 3, theAmount);
    }
    gResource->GetString(IDS_RESOURCE_NAME_STONE, theFood);
    gResource->GetString(IDS_RESOURCE_NAME_GOLD, theWood);
    if (gWorld.mUsingCarbon)
        gResource->GetString(IDS_CARBON, theStone);
    else
        gResource->GetString(IDS_RESOURCE_NAME_WOOD, theStone);
    gResource->GetString(IDS_RESOURCE_NAME_FOOD, theGold);
    gResource->GetString(IDS_RESOURCE_NAME_IRON, theIron);
    if (gWorld.GetCurrentPlayer() == theFrom) {
        gResource->GetString(IDS_YOU_TRIBUTE, theYouGave);
        sprintf(theText, "%s %s ", (const char*)theYouGave, (const char*)theTo->mName);
    } else if (gWorld.GetCurrentPlayer() == theTo) {
        gResource->GetString(IDS_TRIBUTES_YOU, theYouGave);
        sprintf(theText, "%s %s ", (const char*)theFrom->mName, (const char*)theYouGave);
    } else
        return;
    if (this->mFood > 0) {
        _itoa(this->mFood, theNumber, 10);
        theText += theNumber;
        theText += theSpace;
        theText += (const char*)theGold;
        theText += theSpace;
    }
    if (this->mWood > 0) {
        _itoa(this->mWood, theNumber, 10);
        theText += theNumber;
        theText += theSpace;
        theText += (const char*)theStone;
        theText += theSpace;
    }
    if (this->mStone > 0) {
        _itoa(this->mStone, theNumber, 10);
        theText += theNumber;
        theText += theSpace;
        theText += (const char*)theFood;
        theText += theSpace;
    }
    if (this->mGold > 0) {
        _itoa(this->mGold, theNumber, 10);
        theText += theNumber;
        theText += theSpace;
        theText += (const char*)theWood;
        theText += theSpace;
    }
    if (this->mIron > 0) {
        _itoa(this->mIron, theNumber, 10);
        theText += theNumber;
        theText += theSpace;
        theText += (const char*)theIron;
        theText += theSpace;
    }
    gUserInterface->AddChatLogEntry(theFrom->mColor, theFrom->mName, theText, gWorldTable.GetLongValue(25), 0);
}

// FUNCTION: EXE 0x4c3547
// Creates a lobby update of one field (type, ping, color, team, citizens or ready) for a slot
EEMPlayerInformation::EEMPlayerInformation(long inField, long inSlot, long inValue, float inPing)
    : EEMessage(kMessagePlayerInformation, false)
{
    this->mPing = inPing;
    this->mValue = inValue;
    this->mField = inField;
    this->mSlot = inSlot;
}

// FUNCTION: EXE 0x4c3589
// Applies the lobby field update to the slot's game options under the write lock and refreshes the lobby row
void EEMPlayerInformation::Execute()
{
    EEGameOptionsPlayer thePlayer;
    gGameOptions.mLock->WriteLock();
    thePlayer = gGameOptions.mPlayers[this->mSlot];
    switch (this->mField) {
    case 5:
        thePlayer.mIsReady = this->mValue > 0;
        break;
    case 4:
        thePlayer.mStartingCitizens = this->mValue;
        break;
    case 3:
        thePlayer.mTeam = this->mValue;
        break;
    case 2:
        thePlayer.mColor = this->mValue;
        break;
    case 1:
        thePlayer.mPing = this->mPing;
        break;
    case 0:
        thePlayer.mType = this->mValue;
        break;
    }
    gGameOptions.mPlayers[this->mSlot] = thePlayer;
    gGameOptions.mLock->WriteUnlock();
    gUserInterface->PostLobbyUpdatePlayer(this->mSlot, this->mField);
}

// FUNCTION: EXE 0x4c3641
// Packs the lobby player update (header, selection id, value, field, ping, slot) into a new send buffer
void EEMPlayerInformation::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mValue, sizeof(this->mValue));
        theLocation += sizeof(this->mValue);
        memcpy(*outPayloadPointer + theLocation, &this->mField, sizeof(this->mField));
        theLocation += sizeof(this->mField);
        memcpy(*outPayloadPointer + theLocation, &this->mPing, sizeof(this->mPing));
        theLocation += sizeof(this->mPing);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        ESVERIFY_AT(theLocation == theMessageSize, 10998);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMPlayerInformation::ExportSize()
{
    return 21;
}

// FUNCTION: EXE 0x4c36fc
// Returns the move order's payload size (21 bytes)
unsigned long EEMPathTo::ExportSize()
{
    return 21;
}

// FUNCTION: EXE 0x4c3700
// Returns the message's debug label "Player Information"
char* EEMPlayerInformation::GetMessageLabel() const
{
    return "Player Information";
}

// FUNCTION: EXE 0x4c3706
// Reads the lobby player update (header, selection id, value, field, ping, slot) from a received payload
void EEMPlayerInformation::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11046);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mValue, inPayloadPointer + theLocation, sizeof(this->mValue));
    theLocation += sizeof(this->mValue);
    memcpy(&this->mField, inPayloadPointer + theLocation, sizeof(this->mField));
    theLocation += sizeof(this->mField);
    memcpy(&this->mPing, inPayloadPointer + theLocation, sizeof(this->mPing));
    theLocation += sizeof(this->mPing);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
}

bool EEMPlayerInformation::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c378e
// Creates a full lobby record of one player slot from its game options (slot, type, name, ping, color, team ...)
EEMFullPlayerInformation::EEMFullPlayerInformation(const EEGameOptionsPlayer& inPlayer)
    : EEMessage(kMessageFullPlayerInformation, false)
{
    this->mSlot = inPlayer.mSlot;
    this->mSlotType = inPlayer.mType;
    this->mName = inPlayer.mName;
    this->mPing = inPlayer.mPing;
    this->mColor = inPlayer.mColor;
    this->mTeam = inPlayer.mTeam;
    this->mCitizenBonus = inPlayer.mStartingCitizens;
    this->mIsReady = inPlayer.mIsReady;
    this->mIsLocal = inPlayer.mIsLocal;
    this->mLoadedType = inPlayer.mLoadedType;
    this->mB42 = inPlayer.mB36;
}

void EEMFullPlayerInformation::Execute()
{
}

// FUNCTION: EXE 0x4c3818
// Packs the full lobby player record (header, slot, types, name, ping, color, team, flags) into a new send buffer
void EEMFullPlayerInformation::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        memcpy(*outPayloadPointer + theLocation, &this->mSlotType, sizeof(this->mSlotType));
        theLocation += sizeof(this->mSlotType);
        memcpy(*outPayloadPointer + theLocation, &this->mLoadedType, sizeof(this->mLoadedType));
        theLocation += sizeof(this->mLoadedType);
        this->mName.Export(*outPayloadPointer + theLocation);
        theLocation += this->mName.GetExportSize();
        memcpy(*outPayloadPointer + theLocation, &this->mPing, sizeof(this->mPing));
        theLocation += sizeof(this->mPing);
        memcpy(*outPayloadPointer + theLocation, &this->mColor, sizeof(this->mColor));
        theLocation += sizeof(this->mColor);
        memcpy(*outPayloadPointer + theLocation, &this->mTeam, sizeof(this->mTeam));
        theLocation += sizeof(this->mTeam);
        memcpy(*outPayloadPointer + theLocation, &this->mCitizenBonus, sizeof(this->mCitizenBonus));
        theLocation += sizeof(this->mCitizenBonus);
        memcpy(*outPayloadPointer + theLocation, &this->mIsReady, sizeof(this->mIsReady));
        theLocation += sizeof(this->mIsReady);
        memcpy(*outPayloadPointer + theLocation, &this->mIsLocal, sizeof(this->mIsLocal));
        theLocation += sizeof(this->mIsLocal);
        memcpy(*outPayloadPointer + theLocation, &this->mB42, sizeof(this->mB42));
        theLocation += sizeof(this->mB42);
        ESVERIFY_AT(theLocation == theMessageSize, 11172);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c395e
// Returns the full player record's payload size: the name's export size plus 36
unsigned long EEMFullPlayerInformation::ExportSize()
{
    return this->mName.GetExportSize() + 36;
}

// FUNCTION: EXE 0x4c396b
// Returns the message's debug label "Full Player Information"
char* EEMFullPlayerInformation::GetMessageLabel() const
{
    return "Full Player Information";
}

// FUNCTION: EXE 0x4c3971
// Reads the full lobby player record (header, slot, types, name, ping, color, team, flags) from a received payload
void EEMFullPlayerInformation::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11227);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
    memcpy(&this->mSlotType, inPayloadPointer + theLocation, sizeof(this->mSlotType));
    theLocation += sizeof(this->mSlotType);
    memcpy(&this->mLoadedType, inPayloadPointer + theLocation, sizeof(this->mLoadedType));
    theLocation += sizeof(this->mLoadedType);
    theLocation += this->mName.Import(inPayloadPointer + theLocation);
    memcpy(&this->mPing, inPayloadPointer + theLocation, sizeof(this->mPing));
    theLocation += sizeof(this->mPing);
    memcpy(&this->mColor, inPayloadPointer + theLocation, sizeof(this->mColor));
    theLocation += sizeof(this->mColor);
    memcpy(&this->mTeam, inPayloadPointer + theLocation, sizeof(this->mTeam));
    theLocation += sizeof(this->mTeam);
    memcpy(&this->mCitizenBonus, inPayloadPointer + theLocation, sizeof(this->mCitizenBonus));
    theLocation += sizeof(this->mCitizenBonus);
    memcpy(&this->mIsReady, inPayloadPointer + theLocation, sizeof(this->mIsReady));
    theLocation += sizeof(this->mIsReady);
    memcpy(&this->mIsLocal, inPayloadPointer + theLocation, sizeof(this->mIsLocal));
    theLocation += sizeof(this->mIsLocal);
    memcpy(&this->mB42, inPayloadPointer + theLocation, sizeof(this->mB42));
    theLocation += sizeof(this->mB42);
}

bool EEMFullPlayerInformation::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c3a70
// Returns the slot-info message's payload size: the address's export size plus 8
unsigned long EEMPlayerSlotInfo::ExportSize()
{
    return this->mAddress.GetExportSize() + 8;
}

void EEMPlayerSlotInfo::Execute()
{
}

// FUNCTION: EXE 0x4c3a7d
// Packs the player slot info (header, selection id, player id, slot, firewall flag, address) into a new send buffer
void EEMPlayerSlotInfo::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        memcpy(*outPayloadPointer + theLocation, &this->mIsBehindFirewall, sizeof(this->mIsBehindFirewall));
        theLocation += sizeof(this->mIsBehindFirewall);
        this->mAddress.Export(*outPayloadPointer + theLocation);
        theLocation += this->mAddress.GetExportSize();
        ESVERIFY_AT(theLocation == theMessageSize, 11354);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c3b39
// Reads the player slot info (header, selection id, player id, slot, firewall flag, address) from a received payload
void EEMPlayerSlotInfo::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11370);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
    memcpy(&this->mIsBehindFirewall, inPayloadPointer + theLocation, sizeof(this->mIsBehindFirewall));
    theLocation += sizeof(this->mIsBehindFirewall);
    this->mAddress.Import(inPayloadPointer + theLocation);
}

bool EEMPlayerSlotInfo::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c3bbc
// During recorded-game playback, switches graphics, physics and the player between slow and normal speed when the
// local player sent it
void EEMRecordFrames::Execute()
{
    if ((gCommunications->IsClient() == true
         && static_cast<NESynchronousClient*>(gCommunications)->GetRecordPlayback()->GetMode() == kRecordModePlaying)
        || (gCommunications->IsServer() == true
            && static_cast<NESynchronousServer*>(gCommunications)->GetRecordPlayback()->GetMode() == kRecordModePlaying)) {
        if (this->GetOriginatingSlot() == gWorld.GetCurrentPlayer()->mSlot) {
            gGraphics->PostSetFlag5B(this->mSlowPlayback);
            gPhysics->SetSlowPlayback(this->mSlowPlayback);
            if (gCommunications->IsClient()) {
                if (this->mSlowPlayback)
                    (static_cast<NESynchronousClient*>(gCommunications))->GetRecordPlayback()->SetSlowPlayback();
                else
                    (static_cast<NESynchronousClient*>(gCommunications))->GetRecordPlayback()->SetNormalPlayback();
            }
        }
    }
}

// FUNCTION: EXE 0x4c3c6a
// Packs the playback-speed message (header, selection id, slow flag) into a new send buffer
void EEMRecordFrames::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlowPlayback, sizeof(this->mSlowPlayback));
        theLocation += sizeof(this->mSlowPlayback);
        ESVERIFY_AT(theLocation == theMessageSize, 11464);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMRecordFrames::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c3ce6
// Reads the playback-speed message (header, selection id, slow flag) from a received payload
void EEMRecordFrames::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11499);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSlowPlayback, inPayloadPointer + theLocation, sizeof(this->mSlowPlayback));
    theLocation += sizeof(this->mSlowPlayback);
}

bool EEMRecordFrames::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c3d36
// Removes the unit from the sending player's selection
void EEMRemoveUnitFromSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->RemoveUnitFromSelectedGroup(this->mUnitID);
}

// FUNCTION: EXE 0x4c3d5d
// Packs the deselect message (header, selection id, unit id) into a new send buffer
void EEMRemoveUnitFromSelectedGroup::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        ESVERIFY_AT(theLocation == theMessageSize, 11572);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMRemoveUnitFromSelectedGroup::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c3ddb
// Returns the message's debug label "Remove Unit From Selected Group"
char* EEMRemoveUnitFromSelectedGroup::GetMessageLabel() const
{
    return "Remove Unit From Selected Group";
}

// FUNCTION: EXE 0x4c3de1
// Reads the deselect message (header, selection id, unit id) from a received payload
void EEMRemoveUnitFromSelectedGroup::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11621);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
}

bool EEMRemoveUnitFromSelectedGroup::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c3e31
// Orders the sender's selected units, spread around the target, to repair it (queued: adds it as a build target);
// acknowledges by voice
void EEMRepairUnit::Execute()
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theRepairers;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEUnit* theTarget = gWorld.GetUnit(this->mTargetID);
    if (theGroup->GetFirstUnit() && theTarget) {
        EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 0, 0);
        thePlayer->mPendingGroups.push_back(theActiveGroup);
        theActiveGroup->SpreadAroundTile(theTarget->mTileX, theTarget->mTileY);
        if (theActiveGroup->GetNumUnits() > 0) {
            unsigned long i;
            for (i = 0; i < theActiveGroup->GetNumUnits(); i++)
                theUnits.push_back(theActiveGroup->GetUnit(i));
            for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); ) {
                EEComplexUnit* theUnit = (*it)->mComplexUnit;
                if (this->mIsQueued) {
                    theUnit->AddBuildTarget(theTarget);
                    ++it;
                } else {
                    theUnit->OrderRepair(theTarget);
                    theUnit->StartAction(kActionIdle);
                    theUnit->mUnitClass->ActionChanged(theUnit);
                    theRepairers.push_back(theUnit);
                    it = theUnits.erase(it);
                }
            }
            for (i = 0; i < theRepairers.size(); i++)
                theActiveGroup->AddMember(theRepairers[i]->mComplexUnit, true);
            if (theActiveGroup->GetNumUnits() > 0)
                thePlayer->PlayAcknowledgement(theActiveGroup->GetUnit(gPhysics->GetRandom(0L, (long)theActiveGroup->GetNumUnits() - 1)));
        }
    }
}

// FUNCTION: EXE 0x4c400a
// Packs the repair order (header, selection id, target id, queued flag) into a new send buffer
void EEMRepairUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        ESVERIFY_AT(theLocation == theMessageSize, 11773);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMRepairUnit::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c4099
// Returns the message's debug label "Repair Unit"
char* EEMRepairUnit::GetMessageLabel() const
{
    return "Repair Unit";
}

// FUNCTION: EXE 0x4c409f
// Reads the repair order (header, selection id, target id, queued flag) from a received payload
void EEMRepairUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11823);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
}

bool EEMRepairUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c4106
// Packs the replant order (header, selection id, farm id) into a new send buffer
void EEMReplantFarms::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mFarmID, sizeof(this->mFarmID));
        theLocation += sizeof(this->mFarmID);
        ESVERIFY_AT(theLocation == theMessageSize, 11884);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMReplantFarms::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c4184
// Reads the replant order (header, selection id, farm id) from a received payload
void EEMReplantFarms::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11894);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mFarmID, inPayloadPointer + theLocation, sizeof(this->mFarmID));
    theLocation += sizeof(this->mFarmID);
}

bool EEMReplantFarms::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c41d4
// Returns the message's debug label "Replant Farms"
char* EEMReplantFarms::GetMessageLabel() const
{
    return "Replant Farms";
}

// FUNCTION: EXE 0x4c41da
// Replants the given granary (farm id -1: every selected granary, if the first selected unit is one)
void EEMReplantFarms::Execute()
{
    if (this->mFarmID == -1) {
        EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
        EESelectedGroup* theGroup = thePlayer->mActiveGroup;
        if (theGroup->GetFirstUnit()) {
            long theType = theGroup->GetFirstUnit()->mUnitClass->mUnitType;
            if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary) {
                unsigned long theNumUnits = theGroup->GetNumUnits();
                for (unsigned long i = 0; i < theNumUnits; i++) {
                    EEComplexUnit* theUnit = theGroup->GetUnit(i);
                    long theUnitType = theUnit->mUnitClass->mUnitType;
                    if (theUnitType == kUnitTypeGranaryImproved || theUnitType == kUnitTypeGranary)
                        theUnit->mUnitClass->ReplantFarm(theUnit, thePlayer);
                }
                gPhysics->FlushWorld();
            }
        }
    } else {
        EEUnit* theUnit = gWorld.GetUnit(this->mFarmID);
        if (theUnit && theUnit->mTileNode) {
            EEComplexUnit* theFarm = theUnit->GetComplexUnit();
            long theType = theFarm->mUnitClass->mUnitType;
            if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary) {
                theFarm->mUnitClass->ReplantFarm(theFarm, theFarm->mPlayer);
                gPhysics->FlushWorld();
            }
        }
    }
}

// FUNCTION: EXE 0x4c42c8
// Stores the two reported values in the sending player's statistics entries 0x14 and 0x15
void EEMReportStatistics::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer) {
        thePlayer->mSavedGroups.Set(0x14, this->mL10);
        thePlayer->mSavedGroups.Set(0x15, this->mL14);
    }
}

// FUNCTION: EXE 0x4c4309
// Packs the statistics report (header, selection id, two values) into a new send buffer
void EEMReportStatistics::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mL10, sizeof(this->mL10));
        theLocation += sizeof(this->mL10);
        memcpy(*outPayloadPointer + theLocation, &this->mL14, sizeof(this->mL14));
        theLocation += sizeof(this->mL14);
        ESVERIFY_AT(theLocation == theMessageSize, 12049);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMReportStatistics::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c439a
// Returns the message's debug label "Report Statistics"
char* EEMReportStatistics::GetMessageLabel() const
{
    return "Report Statistics";
}

// FUNCTION: EXE 0x4c43a0
// Reads the statistics report (header, selection id, two values) from a received payload
void EEMReportStatistics::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 12099);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mL10, inPayloadPointer + theLocation, sizeof(this->mL10));
    theLocation += sizeof(this->mL10);
    memcpy(&this->mL14, inPayloadPointer + theLocation, sizeof(this->mL14));
    theLocation += sizeof(this->mL14);
}

bool EEMReportStatistics::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c4407
// Packs the resignation (header, selection id) into a new send buffer
void EEMResign::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 12158);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMResign::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c4472
// Reads the resignation (header, selection id) from a received payload
void EEMResign::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 12168);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMResign::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c44b5
// Returns the message's debug label "Resign"
char* EEMResign::GetMessageLabel() const
{
    return "Resign";
}

// FUNCTION: EXE 0x4c44bb
// Removes the sending player from the game as resigned (observers excepted); the other machines post a
// "has resigned" chat line
void EEMResign::Execute()
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theUnused;
    UWideString theText;
    UWideString theFormat;
    UWideString theName;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (!thePlayer->mObserverMode) {
        gWorld.RemovePlayerFromGame(0, thePlayer->mPlayerID);
        thePlayer->SetGameResult(2);
        if (thePlayer != gWorld.GetCurrentPlayer()) {
            gResource->GetString(IDS_PLAYER_HAS_RESIGNED_FROM_GAME, theFormat);
            sprintf(theText, theFormat, (const char*)thePlayer->mName);
            gResource->GetString(IDS_MSG_GAME, theName);
            gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(25) * 2, 0);
        }
    }
}

// FUNCTION: EXE 0x4c45b8
// Removes the improvement from the queue of every living selected building of the clicked unit's type
void EEMRemoveImprovement::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    if (theUnit && theUnit->mHitPoints > 0) {
        long theType = theUnit->mUnitClass->mUnitType;
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theOther = theGroup->GetUnit(i);
            long theOtherType = theOther->mUnitClass->mUnitType;
            if (theOther->mHitPoints > 0 && theOtherType == theType && theOther->mComplexUnit) {
                if (thePlayer->CountQueuedImprovements(theOtherType, this->mImprovement) == 0)
                    thePlayer->RemoveQueuedImprovement(theOtherType, this->mImprovement, false);
                else
                    thePlayer->RemoveQueuedImprovement(theOtherType, this->mImprovement, true);
            }
        }
    }
}

// FUNCTION: EXE 0x4c4668
// Packs the improvement cancel order (header, selection id, unit id, improvement) into a new send buffer
void EEMRemoveImprovement::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        memcpy(*outPayloadPointer + theLocation, &this->mImprovement, sizeof(this->mImprovement));
        theLocation += sizeof(this->mImprovement);
        ESVERIFY_AT(theLocation == theMessageSize, 12323);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMRemoveImprovement::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c46f9
// Returns the message's debug label "Remove Improvement"
char* EEMRemoveImprovement::GetMessageLabel() const
{
    return "Remove Improvement";
}

// FUNCTION: EXE 0x4c46ff
// Reads the improvement cancel order (header, selection id, unit id, improvement) from a received payload
void EEMRemoveImprovement::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 12354);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
    memcpy(&this->mImprovement, inPayloadPointer + theLocation, sizeof(this->mImprovement));
    theLocation += sizeof(this->mImprovement);
}

bool EEMRemoveImprovement::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c4766
// Cancels the research (five times if asked) or toggles just-in-time manufacturing at each living selected
// building of the clicked building's type; refreshes the JIT button for the local player
void EEMRemoveTechnology::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    unsigned long theCount = this->mFiveTimes ? 5 : 1;
    if (theUnit && theUnit->mHitPoints > 0) {
        long theType = theUnit->mUnitClass->mUnitType;
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theOther = theGroup->GetUnit(i);
            EEComplexUnit* theComplexUnit = theOther->mComplexUnit;
            if (theOther->mHitPoints > 0 && theOther->mUnitClass->mUnitType == theType && theComplexUnit) {
                if (this->mFiveTimes) {
                    for (unsigned long j = 0; j < theCount; j++)
                        theComplexUnit->RemoveResearch(this->mTechnology, 0);
                } else if (this->mToggleJITManufacturing)
                    theComplexUnit->ToggleJITManufacturing();
                else
                    theComplexUnit->RemoveResearch(this->mTechnology, 0);
                if (thePlayer == gWorld.GetCurrentPlayer())
                    gUserInterface->UpdateJITManufacturing();
            }
        }
    }
}

// FUNCTION: EXE 0x4c486e
// Returns the research cancel order's payload size (15 bytes)
unsigned long EEMRemoveTechnology::ExportSize()
{
    return 15;
}

// FUNCTION: EXE 0x4c4872
// Packs the research cancel (header, selection id, building, tech, JIT toggle, five-times flag) into a new send buffer
void EEMRemoveTechnology::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        memcpy(*outPayloadPointer + theLocation, &this->mTechnology, sizeof(this->mTechnology));
        theLocation += sizeof(this->mTechnology);
        memcpy(*outPayloadPointer + theLocation, &this->mToggleJITManufacturing, sizeof(this->mToggleJITManufacturing));
        theLocation += sizeof(this->mToggleJITManufacturing);
        memcpy(*outPayloadPointer + theLocation, &this->mFiveTimes, sizeof(this->mFiveTimes));
        theLocation += sizeof(this->mFiveTimes);
        ESVERIFY_AT(theLocation == theMessageSize, 12511);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c4929
// Returns the message's debug label "Remove Technology"
char* EEMRemoveTechnology::GetMessageLabel() const
{
    return "Remove Technology";
}

// FUNCTION: EXE 0x4c492f
// Reads the research cancel (header, selection id, building, tech, JIT toggle, five-times flag) from a received payload
void EEMRemoveTechnology::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 12542);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
    memcpy(&this->mTechnology, inPayloadPointer + theLocation, sizeof(this->mTechnology));
    theLocation += sizeof(this->mTechnology);
    memcpy(&this->mToggleJITManufacturing, inPayloadPointer + theLocation, sizeof(this->mToggleJITManufacturing));
    theLocation += sizeof(this->mToggleJITManufacturing);
    memcpy(&this->mFiveTimes, inPayloadPointer + theLocation, sizeof(this->mFiveTimes));
    theLocation += sizeof(this->mFiveTimes);
}

bool EEMRemoveTechnology::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c49b7
// Queues the research at each built selected building sharing the clicked one's research (five times if asked);
// kinds 0/2/7/8 and hero techs go to one building: one with an empty queue and no research, else the first
void EEMResearchTechnology::Execute()
{
    EECivPower thePower;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEUnit* theUnit = gWorld.GetUnit(this->mBuildingID);
    EEComplexUnit* theBest = NULL;
    unsigned long theBestQueue = 10000;
    bool theWonder = false;
    bool thePower2 = false;
    unsigned long theCount = this->mFiveTimes ? 5 : 1;
    if (theUnit && theUnit->mHitPoints > 0) {
        long theType = theUnit->mUnitClass->mUnitType;
        EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(this->mTechnology);
        bool theAll;
        if (theNode->mData->mKind == 0 || theNode->mData->mKind == 2 || theNode->mData->mKind == 7 || theNode->mData->mKind == 8)
            theAll = true;
        else
            theAll = false;
        if (theNode->mData->mUnitClass > 0 && gUnitClassTable[theNode->mData->mUnitClass] && gUnitClassTable[theNode->mData->mUnitClass]->mAIKind == kAIKindHero)
            theWonder = true;
        else if (theNode->mData->mKind == 3 && gCivPowersManager.GetPower(kCivPowerBundeswehr, thePower)
                 && thePower.mTechnology == theNode->mData->mPrerequisite1)
            thePower2 = true;
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theOther = theGroup->GetUnit(i);
            EEComplexUnit* theComplexUnit = theOther->mComplexUnit;
            if (theOther->mHitPoints > 0
                && (theOther->mUnitClass->mUnitType == theType || theOther->mUnitClass->SharesResearch(theType))
                && theComplexUnit && theComplexUnit->mIsBuilt && theCount > 0) {
                for (unsigned long j = 0; j < theCount; j++) {
                    if (theAll || (theWonder && !gWorld.mGameOptions.mIsScenarioGame)) {
                        unsigned long theQueue = theOther->mComplexUnit->GetBuildQueueSize() + theOther->mComplexUnit->mCurrentResearch != 100000;
                        if (theQueue < theBestQueue) {
                            theBestQueue = theQueue;
                            theBest = theOther;
                        }
                    } else {
                        if (thePower2 && theOther->mComplexUnit && theOther->mComplexUnit->mCurrentResearch != kTechNone
                            && theOther->mHitPoints > 0 && !theOther->mComplexUnit->mIsDisabled
                            && thePlayer->GetTechTree()->Get(theOther->mComplexUnit->mCurrentResearch)
                            && thePlayer->mTechnologies->Get(theOther->mComplexUnit->mCurrentResearch)->mData->mMakesUnit) {
                            theOther->mComplexUnit->StartAction(kActionIdle);
                            theOther->mUnitClass->ActionChanged(theOther->mComplexUnit);
                        }
                        theOther->mComplexUnit->QueueResearch(this->mTechnology, (this->mTechnology >= kTechEpochPrehistoric && this->mTechnology < kTechEpochAll) || thePower2);
                    }
                }
            }
        }
        if (theAll || (theWonder && !gWorld.mGameOptions.mIsScenarioGame)) {
            if (theBest) {
                if (thePower2 && theBest->mComplexUnit && theBest->mComplexUnit->mCurrentResearch != kTechNone && theBest->mHitPoints > 0
                    && !theBest->mComplexUnit->mIsDisabled && thePlayer->GetTechTree()->Get(theBest->mComplexUnit->mCurrentResearch)
                    && thePlayer->GetTechTree()->Get(theBest->mComplexUnit->mCurrentResearch)->mData->mMakesUnit) {
                    theBest->mComplexUnit->StartAction(kActionIdle);
                    theBest->mUnitClass->ActionChanged(theBest->mComplexUnit);
                }
                theBest->mComplexUnit->QueueResearch(this->mTechnology, (this->mTechnology >= kTechEpochPrehistoric && this->mTechnology < kTechEpochAll) || thePower2);
            }
        }
    }
}

// FUNCTION: EXE 0x4c4ce2
// Packs the research order (header, selection id, building id, technology, five-times flag) into a new send buffer
void EEMResearchTechnology::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mBuildingID, sizeof(this->mBuildingID));
        theLocation += sizeof(this->mBuildingID);
        memcpy(*outPayloadPointer + theLocation, &this->mTechnology, sizeof(this->mTechnology));
        theLocation += sizeof(this->mTechnology);
        memcpy(*outPayloadPointer + theLocation, &this->mFiveTimes, sizeof(this->mFiveTimes));
        theLocation += sizeof(this->mFiveTimes);
        ESVERIFY_AT(theLocation == theMessageSize, 12776);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMResearchTechnology::ExportSize()
{
    return 14;
}

// FUNCTION: EXE 0x4c4d84
// Returns the message's debug label "Research Technology"
char* EEMResearchTechnology::GetMessageLabel() const
{
    return "Research Technology";
}

// FUNCTION: EXE 0x4c4d8a
// Reads the research order (header, selection id, building id, technology, five-times flag) from a received payload
void EEMResearchTechnology::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 12807);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mBuildingID, inPayloadPointer + theLocation, sizeof(this->mBuildingID));
    theLocation += sizeof(this->mBuildingID);
    memcpy(&this->mTechnology, inPayloadPointer + theLocation, sizeof(this->mTechnology));
    theLocation += sizeof(this->mTechnology);
    memcpy(&this->mFiveTimes, inPayloadPointer + theLocation, sizeof(this->mFiveTimes));
    theLocation += sizeof(this->mFiveTimes);
}

bool EEMResearchTechnology::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c4e02
// Makes the target the home base of the selected aircraft in flight (resetting their flight time unless kept);
// acknowledges by voice
void EEMReturnToBase::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    EEComplexUnit* theLastUnit = NULL;
    EEUnit* theBase = gWorld.GetUnit(this->mBaseID);
    if (theBase) {
        EEComplexUnit* theBaseUnit = theBase->GetComplexUnit();
        if (theBaseUnit) {
            for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
                EEComplexUnit* theUnit = theGroup->GetUnit(i);
                if (theUnit) {
                    long theCategory = theUnit->GetUnitClass()->mCategory;
                    if ((theCategory == kUnitCategoryAirplane || theCategory == kUnitCategoryPlanetSpaceship) && theUnit->mFlightTime > 0) {
                        theUnit->SetHomeBase(theBaseUnit);
                        if (!this->mKeepFlightTime)
                            theUnit->SetFlightTime(0);
                        theLastUnit = theUnit;
                    }
                }
            }
            if (theLastUnit)
                thePlayer->PlayAcknowledgement(theLastUnit);
        }
    }
}

// FUNCTION: EXE 0x4c4ed5
// Packs the return-to-base order (header, selection id, base id, keep-flight-time flag) into a new send buffer
void EEMReturnToBase::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mBaseID, sizeof(this->mBaseID));
        theLocation += sizeof(this->mBaseID);
        memcpy(*outPayloadPointer + theLocation, &this->mKeepFlightTime, sizeof(this->mKeepFlightTime));
        theLocation += sizeof(this->mKeepFlightTime);
        ESVERIFY_AT(theLocation == theMessageSize, 12937);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMReturnToBase::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c4f64
// Returns the message's debug label "Return To Base"
char* EEMReturnToBase::GetMessageLabel() const
{
    return "Return To Base";
}

// FUNCTION: EXE 0x4c4f6a
// Reads the return-to-base order (header, selection id, base id, keep-flight-time flag) from a received payload
void EEMReturnToBase::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 12987);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mBaseID, inPayloadPointer + theLocation, sizeof(this->mBaseID));
    theLocation += sizeof(this->mBaseID);
    memcpy(&this->mKeepFlightTime, inPayloadPointer + theLocation, sizeof(this->mKeepFlightTime));
    theLocation += sizeof(this->mKeepFlightTime);
}

bool EEMReturnToBase::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c4fd1
// Adds the units of a saved group to the sender's selection, skipping ones already selected
void EEMSavedGroupAddSelection::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEGroup* theSavedGroup = thePlayer->mGroups[this->mGroup];
    EEGroup* theActiveGroup = thePlayer->mActiveGroup;
    for (unsigned long i = 0; i < theSavedGroup->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = theSavedGroup->GetUnit(i);
        if (theUnit && !theActiveGroup->Contains(theUnit))
            theActiveGroup->Add(theUnit);
    }
}

// FUNCTION: EXE 0x4c5045
// Packs the saved-group add message (header, selection id, group) into a new send buffer
void EEMSavedGroupAddSelection::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGroup, sizeof(this->mGroup));
        theLocation += sizeof(this->mGroup);
        ESVERIFY_AT(theLocation == theMessageSize, 13085);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSavedGroupAddSelection::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c50c1
// Returns the message's debug label "Add Selection To Saved Group"
char* EEMSavedGroupAddSelection::GetMessageLabel() const
{
    return "Add Selection To Saved Group";
}

// FUNCTION: EXE 0x4c50c7
// Reads the saved-group add message (header, selection id, group) from a received payload
void EEMSavedGroupAddSelection::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13131);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGroup, inPayloadPointer + theLocation, sizeof(this->mGroup));
    theLocation += sizeof(this->mGroup);
}

bool EEMSavedGroupAddSelection::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c5117
// Clears the sender's saved group if it holds any units
void EEMSavedGroupClear::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEGroup* theGroup = thePlayer->mGroups[this->mGroup];
    if (theGroup && theGroup->GetNumUnits() > 0)
        thePlayer->ClearSavedGroup(this->mGroup);
}

// FUNCTION: EXE 0x4c515c
// Packs the saved-group clear message (header, selection id, group) into a new send buffer
void EEMSavedGroupClear::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGroup, sizeof(this->mGroup));
        theLocation += sizeof(this->mGroup);
        ESVERIFY_AT(theLocation == theMessageSize, 13210);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSavedGroupClear::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c51d8
// Returns the message's debug label "Clear Saved Group"
char* EEMSavedGroupClear::GetMessageLabel() const
{
    return "Clear Saved Group";
}

// FUNCTION: EXE 0x4c51de
// Reads the saved-group clear message (header, selection id, group) from a received payload
void EEMSavedGroupClear::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13256);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGroup, inPayloadPointer + theLocation, sizeof(this->mGroup));
    theLocation += sizeof(this->mGroup);
}

bool EEMSavedGroupClear::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c522e
// Stores the sender's current selection as a saved group (when anything is selected)
void EEMSavedGroupCreate::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer->mActiveGroup->GetNumUnits() > 0)
        thePlayer->mGroups[this->mGroup]->Assign(thePlayer->mActiveGroup);
}

// FUNCTION: EXE 0x4c5278
// Packs the saved-group create message (header, selection id, group) into a new send buffer
void EEMSavedGroupCreate::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGroup, sizeof(this->mGroup));
        theLocation += sizeof(this->mGroup);
        ESVERIFY_AT(theLocation == theMessageSize, 13329);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSavedGroupCreate::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c52f4
// Returns the message's debug label "Create Saved Group"
char* EEMSavedGroupCreate::GetMessageLabel() const
{
    return "Create Saved Group";
}

// FUNCTION: EXE 0x4c52fa
// Reads the saved-group create message (header, selection id, group) from a received payload
void EEMSavedGroupCreate::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13375);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGroup, inPayloadPointer + theLocation, sizeof(this->mGroup));
    theLocation += sizeof(this->mGroup);
}

bool EEMSavedGroupCreate::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c534a
// Selects the sender's saved group; for the local player resets the command panel, centres the camera on it
// when asked and plays its selection sound
void EEMSavedGroupSelect::Execute()
{
    GE3DPoint theLocation;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEUIFMGMainGroup* theMainGroup = static_cast<EEUIFMGMainGroup*>(gUIManager->mMainGameForm->mGroups[0]);
    unsigned char theSlot = this->GetOriginatingSlot();
    unsigned char theLocalSlot = gWorld.GetCurrentPlayer()->GetSlot();
    thePlayer->mActiveGroup->SelectSavedGroup(thePlayer->mGroups[this->mGroup], true);
    if (theSlot == theLocalSlot && gWorld.mExecutingLocalSelection == true) {
        gMouse->mPointer = 0;
        theMainGroup->ShowCommands(theMainGroup->GetSelectedUnit());
        theMainGroup->Reset();
        gUIManager->mMainGameForm->SetUIState(0);
        gUIManager->mMainGameForm->mIsCursorLocked = false;
        EEComplexUnit* theUnit = thePlayer->mSelectedGroup->GetFirstUnit();
        if (theUnit) {
            if (this->mFlag) {
                theUnit->GetPosition(theLocation);
                gWorld.mCameraControl.SetChanged();
                gWorld.mCameraControl.SetLocation(theLocation, false, true);
                gMiniMap->Refresh();
                gCameraFollowing = false;
            }
            if (gWorld.mExecutingLocalSelection)
                EEPlayForLocalPlayer(thePlayer, theUnit->mStats->GetSelectionSound(-1));
        }
    }
}

// FUNCTION: EXE 0x4c548f
// Packs the saved-group select message (header, selection id, group, centre flag) into a new send buffer
void EEMSavedGroupSelect::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mGroup, sizeof(this->mGroup));
        theLocation += sizeof(this->mGroup);
        memcpy(*outPayloadPointer + theLocation, &this->mFlag, sizeof(this->mFlag));
        theLocation += sizeof(this->mFlag);
        ESVERIFY_AT(theLocation == theMessageSize, 13500);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSavedGroupSelect::ExportSize()
{
    return 7;
}

// FUNCTION: EXE 0x4c551c
// Returns the message's debug label "Select Saved Group"
char* EEMSavedGroupSelect::GetMessageLabel() const
{
    return "Select Saved Group";
}

// FUNCTION: EXE 0x4c5522
// Reads the saved-group select message (header, selection id, group, centre flag) from a received payload
void EEMSavedGroupSelect::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13547);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mGroup, inPayloadPointer + theLocation, sizeof(this->mGroup));
    theLocation += sizeof(this->mGroup);
    memcpy(&this->mFlag, inPayloadPointer + theLocation, sizeof(this->mFlag));
    theLocation += sizeof(this->mFlag);
}

bool EEMSavedGroupSelect::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c5587
// Sends an airplane to the point: clears its goal queues and queues the move in queues 0 and 1 (1 s timer)
void EEMScrambleAircraft::Execute()
{
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    if (theUnit) {
        EEComplexUnit* theAircraft = theUnit->GetComplexUnit();
        if (theAircraft && (theAircraft->mUnitClass->mCategory == kUnitCategoryAirplane || theAircraft->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship)) {
            EEUGLocation* theGoal = new EEUGLocation(GE2DPoint(this->mX, this->mY), false, false, false, 0.0f, false, false);
            theAircraft->ClearAllGoalQueues();
            theAircraft->QueueGoal(theGoal, 0);
            theAircraft->QueueGoal(theGoal->Clone(), 1);
            theAircraft->mGoalQueues[1].AddTime(1000);
        }
    }
}

// FUNCTION: EXE 0x4c5633
// Packs the scramble order (header, selection id, x, y, unit id) into a new send buffer
void EEMScrambleAircraft::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mX, sizeof(this->mX));
        theLocation += sizeof(this->mX);
        memcpy(*outPayloadPointer + theLocation, &this->mY, sizeof(this->mY));
        theLocation += sizeof(this->mY);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        ESVERIFY_AT(theLocation == theMessageSize, 13656);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMScrambleAircraft::ExportSize()
{
    return 17;
}

// FUNCTION: EXE 0x4c56d7
// Returns the message's debug label "Scramble Aircraft"
char* EEMScrambleAircraft::GetMessageLabel() const
{
    return "Scramble Aircraft";
}

// FUNCTION: EXE 0x4c56dd
// Reads the scramble order (header, selection id, x, y, unit id) from a received payload
void EEMScrambleAircraft::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13704);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mX, inPayloadPointer + theLocation, sizeof(this->mX));
    theLocation += sizeof(this->mX);
    memcpy(&this->mY, inPayloadPointer + theLocation, sizeof(this->mY));
    theLocation += sizeof(this->mY);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
}

bool EEMScrambleAircraft::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c5755
// Sets whether the sender's chat goes to the given player
void EEMSelectCustomPlayerForChat::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer)
        thePlayer->mSendChatMessage[this->mPlayerID] = this->mIsSelected;
}

// FUNCTION: EXE 0x4c5783
// Packs the chat recipient toggle (header, selection id, player id, selected flag) into a new send buffer
void EEMSelectCustomPlayerForChat::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsSelected, sizeof(this->mIsSelected));
        theLocation += sizeof(this->mIsSelected);
        ESVERIFY_AT(theLocation == theMessageSize, 13787);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSelectCustomPlayerForChat::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c5812
// Returns the message's debug label "Select Custom Player For Chat"
char* EEMSelectCustomPlayerForChat::GetMessageLabel() const
{
    return "Select Custom Player For Chat";
}

// FUNCTION: EXE 0x4c5818
// Reads the chat recipient toggle (header, selection id, player id, selected flag) from a received payload
void EEMSelectCustomPlayerForChat::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13834);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mIsSelected, inPayloadPointer + theLocation, sizeof(this->mIsSelected));
    theLocation += sizeof(this->mIsSelected);
}

bool EEMSelectCustomPlayerForChat::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c587f
// Selects the patrol group of the given unit for the sender
void EEMSelectPatrolGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    unsigned long theID = this->mUnitID;
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
    if (theUnit)
        theUnit->mPatrolGroup->Select(thePlayer, true);
}

// FUNCTION: EXE 0x4c58d8
// Packs the patrol group selection (header, selection id, unit id) into a new send buffer
void EEMSelectPatrolGroup::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        ESVERIFY_AT(theLocation == theMessageSize, 13922);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSelectPatrolGroup::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c5956
// Returns the message's debug label "Select Patrol Group"
char* EEMSelectPatrolGroup::GetMessageLabel() const
{
    return "Select Patrol Group";
}

// FUNCTION: EXE 0x4c595c
// Reads the patrol group selection (header, selection id, unit id) from a received payload
void EEMSelectPatrolGroup::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 13971);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
}

bool EEMSelectPatrolGroup::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c59ac
// Sets the sender's chat recipient type
void EEMSelectPlayerChatType::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer)
        thePlayer->mChatType = this->mChatType;
}

// FUNCTION: EXE 0x4c59d6
// Packs the chat type selection (header, selection id, chat type) into a new send buffer
void EEMSelectPlayerChatType::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mChatType, sizeof(this->mChatType));
        theLocation += sizeof(this->mChatType);
        ESVERIFY_AT(theLocation == theMessageSize, 14045);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSelectPlayerChatType::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c5a54
// Returns the message's debug label "Select Player Chat Type"
char* EEMSelectPlayerChatType::GetMessageLabel() const
{
    return "Select Player Chat Type";
}

// FUNCTION: EXE 0x4c5a5a
// Reads the chat type selection (header, selection id, chat type) from a received payload
void EEMSelectPlayerChatType::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14091);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mChatType, inPayloadPointer + theLocation, sizeof(this->mChatType));
    theLocation += sizeof(this->mChatType);
}

bool EEMSelectPlayerChatType::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c5aaa
// Replaces the sender's selection with the listed units (no check that each unit still exists)
void EEMSelectTheseUnits::Execute()
{
    EEGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    theGroup->Clear();
    for (std::vector<long>::iterator it = this->mUnitIDs.begin(); it != this->mUnitIDs.end(); ++it)
        theGroup->Add(gWorld.GetUnit(*it)->GetComplexUnit());
}

// FUNCTION: EXE 0x4c5afb
// Packs the unit list (header, selection id, count, unit ids) into a new send buffer
void EEMSelectTheseUnits::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    unsigned long theNumUnits = this->mUnitIDs.size();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &theNumUnits, sizeof(theNumUnits));
        theLocation += sizeof(theNumUnits);
        for (unsigned long i = 0; i < theNumUnits; i++) {
            memcpy(*outPayloadPointer + theLocation, &this->mUnitIDs[i], sizeof(long));
            theLocation += sizeof(long);
        }
        ESVERIFY_AT(theLocation == theMessageSize, 14191);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c5bc8
// Returns the unit list's payload size: 4 bytes per unit plus 9
unsigned long EEMSelectTheseUnits::ExportSize()
{
    return this->mUnitIDs.size() * sizeof(long) + 9;
}

// FUNCTION: EXE 0x4c5be3
// Returns the message's debug label "Select These Units"
char* EEMSelectTheseUnits::GetMessageLabel() const
{
    return "Select These Units";
}

// FUNCTION: EXE 0x4c5be9
// Reads the unit list (header, selection id, count, unit ids) from a received payload, appending the ids
void EEMSelectTheseUnits::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14244);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    unsigned long theNumUnits;
    memcpy(&theNumUnits, inPayloadPointer + theLocation, sizeof(theNumUnits));
    theLocation += sizeof(theNumUnits);
    for (unsigned long i = 0; i < theNumUnits; i++) {
        long theUnitID;
        memcpy(&theUnitID, inPayloadPointer + theLocation, sizeof(theUnitID));
        theLocation += sizeof(theUnitID);
        this->mUnitIDs.push_back(theUnitID);
    }
}

bool EEMSelectTheseUnits::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c5c80
// Selects one unit for the sender
void EEMSelectUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->SelectUnit(this->mUnitID, true, true);
}

// FUNCTION: EXE 0x4c5cab
// Packs the single selection (header, selection id, unit id) into a new send buffer
void EEMSelectUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        ESVERIFY_AT(theLocation == theMessageSize, 14324);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSelectUnit::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c5d29
// Returns the message's debug label "Select Unit"
char* EEMSelectUnit::GetMessageLabel() const
{
    return "Select Unit";
}

// FUNCTION: EXE 0x4c5d2f
// Reads the single selection (header, selection id, unit id) from a received payload
void EEMSelectUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14373);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
}

bool EEMSelectUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c5d7f
// Creates a box selection: picks the local player's units of the given type in the rectangle and stores
// them compressed (and decoded)
EEMSelectUnits::EEMSelectUnits(const GERectangle& inRectangle, long inUnitType, unsigned char inType)
    : EEMessage(inType, true)
{
    std::vector<unsigned long> theUnits;
    this->mRectangle = inRectangle;
    this->mL30 = inUnitType;
    gUnitPicker.PickUnits(this->mRectangle, gWorld.GetCurrentPlayer(), inUnitType, theUnits);
    long thePlayerID = gWorld.GetCurrentPlayer()->mPlayerID;
    this->mBufferSize = CompressUnits(theUnits, thePlayerID, this->mBuffer);
    DecompressUnits(this->mBuffer, this->mBufferSize, this->mUnitIDs);
}

// The selection on the wire: byte 0 is the owner (the ids' top byte), then runs of a count byte (low 6 bits; the top 2 bits: 0, 1 or 2 bytes of the ids' middle 16 bits follow) and the ids' low bytes; at most 63 ids.
// FUNCTION: EXE 0x4c5e67
// Packs unit ids into the compact selection format above (at most 63 ids); returns the byte count
static unsigned char CompressUnits(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer)
{
    std::map<unsigned long, std::vector<unsigned long> > theGroups;
    unsigned long theGroup, theLow;
    std::map<unsigned long, std::vector<unsigned long> >::iterator it;
    unsigned long theRemaining = 63;
    unsigned long i;
    *outBuffer = (unsigned char)inOwner;
    unsigned char* theStart = outBuffer + 1;
    unsigned char* theCursor = theStart;
    unsigned long theSize = 1;
    for (i = 0; i < inUnits.size(); i++) {
        theGroup = (inUnits[i] >> 8) & 0xffff;
        theLow = inUnits[i] & 0xff;
        theGroups[theGroup].push_back(theLow);
    }
    for (it = theGroups.begin(); it != theGroups.end(); it++) {
        if (theRemaining == 0)
            break;
        unsigned long theCount = it->second.size();
        if (theCount != 0) {
            if (theCount > theRemaining)
                theCount = theRemaining;
            theGroup = it->first;
            if (theGroup == 0) {
                *theCursor++ = (unsigned char)theCount; theSize++;
            } else if (theGroup <= 0xff) {
                *theCursor++ = (unsigned char)(theCount + 0x40);
                *theCursor++ = (unsigned char)theGroup; theSize += 2;
            } else {
                *theCursor++ = (unsigned char)(theCount + 0x80);
                *theCursor++ = (unsigned char)(theGroup >> 8);
                *theCursor++ = (unsigned char)theGroup; theSize += 3;
            }
            for (unsigned long j = 0; j < theCount; j++) {
                *theCursor++ = (unsigned char)it->second[j]; theSize++; theRemaining--;
            }
        }
    }
    if (theCursor == theStart) { *theCursor = 0xff; theSize++; }
    return (unsigned char)theSize;
}

// FUNCTION: EXE 0x4c5fd1
// Decodes a compact selection buffer into unit ids, replacing the list
static void DecompressUnits(unsigned char* inBuffer, unsigned char inSize, std::vector<long>& outUnits)
{
    unsigned long theBase = 0;
    unsigned long i = 1;
    outUnits.clear();
    unsigned long theOwner = inBuffer[0] << 24;
    do {
        unsigned long theCount = inBuffer[i] & 0x3f;
        unsigned long theGroup;
        switch (inBuffer[i] >> 6) {
        case 0: theBase = theOwner; break;
        case 1:
            theBase = (inBuffer[++i] << 8) | theOwner;
            break;
        case 2:
            theGroup = inBuffer[++i] << 8;
            theGroup += inBuffer[++i];
            theBase = (theGroup << 8) & 0x00ffff00 | theOwner;
            break;
        case 3: theCount = 0; break;
        }
        i++;
        for (unsigned long j = 0; j < theCount; j++) {
            long theUnit = inBuffer[i] | theBase;
            outUnits.push_back(theUnit);
            i++;
        }
    } while (i != inSize);
}

// FUNCTION: EXE 0x4c606b
// Selects the decoded units for the sender
void EEMSelectUnits::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->SelectUnits(this->mUnitIDs);
}

// FUNCTION: EXE 0x4c6093
// Packs the box selection (header, selection id, buffer size, compressed ids) into a new send buffer
void EEMSelectUnits::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mBufferSize, sizeof(this->mBufferSize));
        theLocation += sizeof(this->mBufferSize);
        memcpy(*outPayloadPointer + theLocation, this->mBuffer, this->mBufferSize);
        theLocation += this->mBufferSize;
        ESVERIFY_AT(theLocation == theMessageSize, 14485);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c6132
// Returns the box selection's payload size: the compressed buffer plus 6 bytes
unsigned long EEMSelectUnits::ExportSize()
{
    return this->mBufferSize + 6;
}

// FUNCTION: EXE 0x4c613d
// Returns the message's debug label "Select Units"
char* EEMSelectUnits::GetMessageLabel() const
{
    return "Select Units";
}

// FUNCTION: EXE 0x4c6143
// Reads the box selection (header, selection id, buffer size, compressed ids) from a received payload and
// decodes its unit ids
void EEMSelectUnits::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14536);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mBufferSize, inPayloadPointer + theLocation, sizeof(this->mBufferSize));
    theLocation += sizeof(this->mBufferSize);
    memcpy(this->mBuffer, inPayloadPointer + theLocation, this->mBufferSize);
    theLocation += this->mBufferSize;
    DecompressUnits(this->mBuffer, this->mBufferSize, this->mUnitIDs);
}

bool EEMSelectUnits::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c61be
// Packs the self-destruct order (header, selection id) into a new send buffer
void EEMSelfDestruct::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 14602);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSelfDestruct::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c6229
// Reads the self-destruct order (header, selection id) from a received payload
void EEMSelfDestruct::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14613);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMSelfDestruct::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c626c
// Kills every living selected unit whose class has a death-spawn class; acknowledges by voice
void EEMSelfDestruct::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup->GetFirstUnit()) {
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theUnit->mHitPoints > 0 && theUnit->mUnitClass->mDeathSpawnClass != NULL)
                theUnit->Kill(1, 0, 0);
        }
        thePlayer->PlayAcknowledgement(theGroup->GetFirstUnit());
    }
}

// FUNCTION: EXE 0x4c62f0
// Packs the flare (header, selection id, x, y) into a new send buffer
void EEMSendFlare::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mX, sizeof(this->mX));
        theLocation += sizeof(this->mX);
        memcpy(*outPayloadPointer + theLocation, &this->mY, sizeof(this->mY));
        theLocation += sizeof(this->mY);
        ESVERIFY_AT(theLocation == theMessageSize, 14698);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSendFlare::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c6381
// Reads the flare (header, selection id, x, y) from a received payload
void EEMSendFlare::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14708);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mX, inPayloadPointer + theLocation, sizeof(this->mX));
    theLocation += sizeof(this->mX);
    memcpy(&this->mY, inPayloadPointer + theLocation, sizeof(this->mY));
    theLocation += sizeof(this->mY);
}

bool EEMSendFlare::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c63e8
// Shows a flare at the tile to the sender and each ally, and tells each ally it received one
void EEMSendFlare::Execute()
{
    std::vector<EEPlayer*> theAllies;
    U2DPoint<long> theTile;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    theAllies = thePlayer->mAllies;
    theTile.mX = this->mX;
    theTile.mY = this->mY;
    thePlayer->mEvents.AddEvent(theTile, 3, thePlayer, -1, true);
    for (std::vector<EEPlayer*>::iterator it = theAllies.begin(); it != theAllies.end(); ++it) {
        (*it)->mEvents.AddEvent(theTile, 3, *it, -1, true);
        (*it)->FlareReceived(thePlayer, theTile);
    }
}

// FUNCTION: EXE 0x4c648f
// Packs the airport order (header, selection id, point, command, target id, queued flag) into a new send buffer
void EEMSetAirportCommand::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPoint, sizeof(this->mPoint));
        theLocation += sizeof(this->mPoint);
        memcpy(*outPayloadPointer + theLocation, &this->mCommand, sizeof(this->mCommand));
        theLocation += sizeof(this->mCommand);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        ESVERIFY_AT(theLocation == theMessageSize, 14805);
        outMessageSize = theMessageSize;
    }
}

// FUNCTION: EXE 0x4c6548
// Reads the airport order (header, selection id, point, command, target id, queued flag) from a received payload
void EEMSetAirportCommand::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 14815);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPoint, inPayloadPointer + theLocation, sizeof(this->mPoint));
    theLocation += sizeof(this->mPoint);
    memcpy(&this->mCommand, inPayloadPointer + theLocation, sizeof(this->mCommand));
    theLocation += sizeof(this->mCommand);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
}

bool EEMSetAirportCommand::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c65d2
// Gives the sender's selected airports an order for their aircraft: attack the target (a seen enemy) or go to
// the point, in the queue(s) the command picks (12-15 add a 60 s timer; 15 shares one attack goal)
void EEMSetAirportCommand::Execute()
{
    EEComplexUnit* theFirstUnit = NULL;
    EEUnitGoal* theGoal = NULL;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer->mActiveGroup != NULL)
        theFirstUnit = thePlayer->mActiveGroup->GetFirstUnit();
    long theDamageIndex;
    switch (this->mCommand) {
    case 8:
    case 0xe:
        theDamageIndex = 0xc;
        break;
    default:
        theDamageIndex = 0;
        break;
    }
    bool theAttack = false;
    if (this->mTargetID != -1) {
        EEUnit* theTarget = gWorld.GetUnit(this->mTargetID);
        if (theTarget == NULL)
            goto issue;
        if (thePlayer != theTarget->mPlayer && theTarget->mPlayer->mEnemies[thePlayer->mPlayerID] != 0
            && EEIsSeenBy(theTarget, thePlayer, true)) {
            unsigned long theClass = theTarget->mUnitClass->mFamily;
            if ((theTarget->mUnitClass->CanBeAttacked() && gU4E8708[theClass][theDamageIndex] > 0.0f) || this->mCommand == 0xf) {
                theGoal = new EEUGAttackUnit(theFirstUnit != NULL ? theFirstUnit : theTarget->mComplexUnit, theTarget,
                                             true, false);
                theAttack = true;
            }
        }
    }
    if (!theAttack)
        theGoal = new EEUGLocation(this->mPoint, true, true, false, 0.0f, false, 0);
issue:
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup != NULL) {
        for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theUnit->mUnitClass->mUnitType != kUnitTypeAirport || theGoal == NULL)
                continue;
            EEUnitGoal* theCopy = NULL;
            if (dynamic_cast<EEUGAttackUnit*>(theGoal) != NULL)
                theCopy = dynamic_cast<EEUGAttackUnit*>(theGoal->Clone());
            switch (this->mCommand) {
            case 10:
            case 12:
                if (!this->mIsQueued)
                    theUnit->ClearGoalQueue(3);
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 3);
                if (this->mCommand == 12) {
                    theUnit->mGoalQueues[3].ResetTimer();
                    theUnit->mGoalQueues[3].AddTime(60000);
                }
                break;
            case 9:
            case 13:
                if (!this->mIsQueued)
                    theUnit->ClearGoalQueue(2);
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 2);
                if (this->mCommand == 13) {
                    theUnit->mGoalQueues[2].ResetTimer();
                    theUnit->mGoalQueues[2].AddTime(60000);
                }
                break;
            case 8:
            case 14:
                if (!this->mIsQueued)
                    theUnit->ClearGoalQueue(1);
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 1);
                if (this->mCommand == 14) {
                    theUnit->mGoalQueues[1].ResetTimer();
                    theUnit->mGoalQueues[1].AddTime(60000);
                }
                break;
            case 15:
                if (!this->mIsQueued)
                    theUnit->ClearAllGoalQueues();
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 3);
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 2);
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 1);
                theUnit->QueueGoal(theCopy != NULL ? theCopy : theGoal->Clone(), 4);
                theUnit->mGoalQueues[4].ResetTimer();
                theUnit->mGoalQueues[4].AddTime(60000);
                theUnit->mGoalQueues[3].ResetTimer();
                theUnit->mGoalQueues[3].AddTime(60000);
                theUnit->mGoalQueues[2].ResetTimer();
                theUnit->mGoalQueues[2].AddTime(60000);
                theUnit->mGoalQueues[1].ResetTimer();
                theUnit->mGoalQueues[1].AddTime(60000);
                break;
            case 20:
                if (!this->mIsQueued)
                    theUnit->ClearGoalQueue(4);
                theUnit->QueueGoal(new EEUGLocation(this->mPoint, true, true, false, 0.0f, false, 0), 4);
                break;
            }
        }
    }
    delete theGoal;
}

// FUNCTION: EXE 0x4c6a8d
// Packs the stance change (header, selection id, player id, stance) into a new send buffer
void EEMSetDiplomaticStance::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mStance, sizeof(this->mStance));
        theLocation += sizeof(this->mStance);
        ESVERIFY_AT(theLocation == theMessageSize, 15122);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSetDiplomaticStance::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c6b1e
// Reads the stance change (header, selection id, player id, stance) from a received payload
void EEMSetDiplomaticStance::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 15132);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mStance, inPayloadPointer + theLocation, sizeof(this->mStance));
    theLocation += sizeof(this->mStance);
}

bool EEMSetDiplomaticStance::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c6b85
// Sets the sender's stance toward a player and posts "<name> changes diplomatic stance to" friendly
// (stance 0) or hostile to the chat log
void EEMSetDiplomaticStance::Execute()
{
    UWideString theAlly;
    UWideString theEnemy;
    UWideString theIsNow;
    UWideString theText(0x400);
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    gWorld.SetDiplomaticStance(thePlayer->mPlayerID, this->mPlayerID, this->mStance);
    gResource->GetString(IDS_HOSTILE, theAlly);
    gResource->GetString(IDS_FRIENDLY, theEnemy);
    gResource->GetString(IDS_CHANGES_DIPLOMATIC_STANCE_TO, theIsNow);
    sprintf(theText, "%s %s %s", (const char*)thePlayer->mName, (const char*)theIsNow,
            (const char*)(this->mStance == 0 ? theEnemy : theAlly));
    gUserInterface->AddChatLogEntry(thePlayer->mColor, thePlayer->mName, theText, gWorldTable.GetLongValue(25), 0);
}

// FUNCTION: EXE 0x4c6c83
// Returns the airport order's payload size (22 bytes)
unsigned long EEMSetAirportCommand::ExportSize()
{
    return 22;
}

// FUNCTION: EXE 0x4c6c87
// Packs the rally order (header, selection id, point, target id, building id, queued flag) into a new send buffer
void EEMSetRallyGoal::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mPoint, sizeof(this->mPoint));
        theLocation += sizeof(this->mPoint);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mBuildingID, sizeof(this->mBuildingID));
        theLocation += sizeof(this->mBuildingID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        ESVERIFY_AT(theLocation == theMessageSize, 15229);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSetRallyGoal::ExportSize()
{
    return 22;
}

// FUNCTION: EXE 0x4c6d40
// Reads the rally order (header, selection id, point, target id, building id, queued flag) from a received payload
void EEMSetRallyGoal::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 15239);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mPoint, inPayloadPointer + theLocation, sizeof(this->mPoint));
    theLocation += sizeof(this->mPoint);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mBuildingID, inPayloadPointer + theLocation, sizeof(this->mBuildingID));
    theLocation += sizeof(this->mBuildingID);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
}

bool EEMSetRallyGoal::IsResponse(NEMessage* inMessage)
{
    return false;
}

void EEMSetWaterLevel::Execute()
{
}

// inline (header-declared; its COMDAT 0x4c7486 follows its first user EEMSetRallyGoal::Execute): the unit can
// attack inClass: its stats' +0xd8 is positive, and the damage table entry of inClass against this class
inline bool EEComplexUnit::CanAttackClass(EEUnitClass* inClass)
{
    long theTarget;
    unsigned long theAttacker;
    return this->mStats->mAttack > 0.0f && (theTarget = this->mUnitClass->mAmmoType, theAttacker = inClass->mFamily, inClass->CanBeAttacked())
           && gU4E8708[theAttacker][theTarget] > 0.0f;
}

#include "ee/EEUGBuildRepair.h"
#include "ee/EEDoppelganger.h"

// FUNCTION: EXE 0x4c6dca
// Sets the rally goal of the sender's selected buildings of this type: gather, attack, repair, patrol, garrison,
// clear (target is the building) or move; airports, docks and aircraft fill their own queues
void EEMSetRallyGoal::Execute()
{
    std::vector<EEComplexUnit*> theGarrisonUnits;
    std::vector<EEUnitGoal*> theGoals;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    unsigned long thePlayerID = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mPlayerID;
    EEUnitGoal* theGoal = NULL;
    EEUnit* theCommandTarget = NULL;
    long theCommand = 0;
    bool theClear = false;
    EEUnit* theBuildingUnit = gWorld.GetUnit(this->mBuildingID);
    if (theBuildingUnit == NULL)
        return;
    EEComplexUnit* theBuilding = theBuildingUnit->mComplexUnit;
    if (this->mTargetID != -1) {
        EEUnit* theTarget = gWorld.GetUnit(this->mTargetID);
        if (theTarget == NULL)
            goto issue;
        if (theTarget->mResourceAmount > 0
            && !((theTarget->mUnitClass->mCategory == kUnitCategoryLandCitizen || theTarget->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
                 && theTarget->mHitPoints > 0)
            && (theTarget->mUnitClass->mFamily == kUnitFamilyFish || theTarget->mTileNode == NULL
                || (theTarget->mComplexUnit->mIsBuilt && theTarget->mPlayer == theBuilding->mPlayer))) {
            theGoal = new EEUGGather(theTarget, NULL, true);
            theGoals.push_back(theGoal);
            theCommand = 0xba;
            theCommandTarget = theTarget;
            goto issue;
        }
        EEPlayer* theOwner = theBuilding->mPlayer;
        if (theTarget->mPlayer != theOwner) {
            if (theTarget->mPlayer->mEnemies[theOwner->mPlayerID] != 0) {
                bool theVisible = EEIsSeenBy(theTarget, theOwner, true);
                if (!theVisible) {
                    EETile* theTile = static_cast<EETile*>(theTarget->mTile);
                    for (unsigned long i = 0; i < theTile->GetNumDoppelgangers() && !theVisible; i++)
                        theVisible = theTile->mDoppelgangers[i]->mUnit == theTarget;
                }
                if (theVisible && EEUUGetIsUnitClassTargetable(NULL, theTarget->mUnitClass)
                    && ((theBuilding->mUnitClass->mCategory != kUnitCategoryAirplane && theBuilding->mUnitClass->mCategory != kUnitCategoryPlanetSpaceship)
                        || theBuilding->CanAttackClass(theTarget->mUnitClass))) {
                    theGoal = new EEUGAttackUnit(theBuilding, theTarget, true, false);
                    theGoals.push_back(theGoal);
                    theCommand = 0xb9;
                    theCommandTarget = theTarget;
                    goto issue;
                }
                theGoal = new EEUGLocation(this->mPoint, true, true, false, 0.0f, false, 0);
                theGoals.push_back(theGoal);
                theCommand = 0xb8;
                goto issue;
            }
        } else {
            if (theTarget->mUnitClass->mUnitType == kUnitTypeFarm
                || (theTarget->mUnitClass->IsLandBuilding() && theTarget->mHitPoints < theTarget->mStats->mMaxHitPoints)) {
                theGoal = new EEUGBuildRepair(theTarget->mPlayer, theTarget, true);
            } else if (theTarget->mUnitClass->mUnitType == kUnitTypePathPoint) {
                theGoal = new EEUGPatrol(theTarget->mComplexUnit->mPatrolGroup, theTarget->mComplexUnit, true);
            } else if (theTarget->mUnitClass->mGarrisonCapacity > 0 && theTarget->mTileNode != NULL && theTarget->mHitPoints > 0) {
                EEComplexUnit* theGarrison = theTarget->mComplexUnit;
                if (theGarrison->mGoalState != NULL && theGarrison->mGoalState->mKind == kGoalLoadGarrison) {
                    theGoal = theGarrison->mGoalState->Clone();
                } else {
                    theGarrisonUnits.clear();
                    EEUGLoadGarrison* theLoad = new EEUGLoadGarrison(theGarrison, theGarrisonUnits, true);
                    if (theLoad->mIsValid) {
                        theGarrison->SetGoal(theLoad, 0, 0);
                        if (theGarrison->mStats->mMaxVelocity > 0.0f) theGarrison->StartAction(kActionPrepareToMove); else theGarrison->StartAction(kActionPickedUpLoadableUnits);
                        theGarrison->mUnitClass->ActionChanged(theGarrison);
                        theGoal = theLoad->Clone();
                        theGoals.push_back(theGoal);
                        theCommand = 0xba;
                        theCommandTarget = theTarget;
                    } else {
                        delete theLoad;
                        theGoal = NULL;
                    }
                    goto issue;
                }
            } else if (theTarget == theBuilding) {
                theGoals.clear();
                theClear = true;
                theCommand = 0xba;
                theCommandTarget = theTarget;
                goto issue;
            } else
                goto location;
            theGoals.push_back(theGoal);
            theCommand = 0xba;
            theCommandTarget = theTarget;
            goto issue;
        }
    }
location:
    theGoal = new EEUGLocation(this->mPoint, true, true, false, 0.0f, false, 0);
    theGoals.push_back(theGoal);
    theCommand = 0xb8;
issue:
    if (theGoals.size() == 0 && !theClear)
        return;
    {
        unsigned long theType = theBuilding->mUnitClass->mUnitType;
        EESelectedGroup* theSelection = theBuilding->mPlayer->mActiveGroup;
        if (theSelection != NULL) {
            for (unsigned long i = 0; i < theSelection->GetNumUnits(); i++) {
                EEComplexUnit* theUnit = theSelection->GetUnit(i);
                if (theUnit->mUnitClass->mUnitType != theType && !theUnit->mUnitClass->SharesRallyPoint(theType))
                    continue;
                std::vector<EEUnitGoal*>::iterator theIterator;
                if (theType == kUnitTypeAirport) {
                    if (!this->mIsQueued)
                        theUnit->ClearAllGoalQueues();
                    for (theIterator = theGoals.begin(); theIterator != theGoals.end(); theIterator++) {
                        theUnit->QueueGoal((*theIterator)->Clone(), 3);
                        theUnit->QueueGoal((*theIterator)->Clone(), 2);
                        theUnit->QueueGoal((*theIterator)->Clone(), 1);
                        theUnit->QueueGoal((*theIterator)->Clone(), 4);
                    }
                } else if (theType == kUnitTypeNavyYard) {
                    if (!this->mIsQueued)
                        theUnit->ClearAllGoalQueues();
                    for (theIterator = theGoals.begin(); theIterator != theGoals.end(); theIterator++) {
                        theUnit->QueueGoal((*theIterator)->Clone(), 1);
                        theUnit->QueueGoal((*theIterator)->Clone(), 0);
                        theUnit->QueueGoal((*theIterator)->Clone(), 2);
                    }
                } else if (theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship) {
                    if (!this->mIsQueued) {
                        theUnit->ClearGoalQueue(0);
                        theUnit->ClearGoalQueue(1);
                    }
                    for (theIterator = theGoals.begin(); theIterator != theGoals.end(); theIterator++) {
                        EEUnitGoal* theRally = *theIterator;
                        theUnit->QueueGoal(theRally->Clone(), 0);
                        theUnit->QueueGoal(theRally->Clone(), 1);
                    }
                    if (theGoals.size() > 0) {
                        if (theUnit->GetCommandType() == kActionIdle) theUnit->StartAction(kActionTakeoff); else theUnit->StartAction(kActionPrepareToMove);
                        theUnit->mUnitClass->ActionChanged(theUnit);
                    }
                } else {
                    if (!this->mIsQueued || theClear)
                        theUnit->ClearGoalQueue(1);
                    for (theIterator = theGoals.begin(); theIterator != theGoals.end(); theIterator++)
                        theUnit->QueueGoal((*theIterator)->Clone(), 1);
                }
            }
            if (theCommand != 0) {
                EESendGroupCommand(theSelection, theCommandTarget, thePlayerID, theCommand, 1);
                thePlayer->PlayAcknowledgement(theSelection->GetFirstUnit());
            }
        }
    }
    delete theGoal;
}

// FUNCTION: EXE 0x4c74da
// Packs the water-level message (header, selection id, level byte) into a new send buffer

void EEMSetWaterLevel::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mB10, sizeof(this->mB10));
        theLocation += sizeof(this->mB10);
        ESVERIFY_AT(theLocation == theMessageSize, 15647);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSetWaterLevel::ExportSize()
{
    return 6;
}

// FUNCTION: EXE 0x4c7556
// Reads the water-level message (header, selection id, level byte) from a received payload
void EEMSetWaterLevel::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 15657);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mB10, inPayloadPointer + theLocation, sizeof(this->mB10));
    theLocation += sizeof(this->mB10);
}

bool EEMSetWaterLevel::IsResponse(NEMessage* inMessage)
{
    return false;
}

// EEMSmartFarmers' sort predicate (provisional name): the farther farm first, with +0x34c, then +0x344 ordering
// the units; `this` is the EETileDistanceLess the distances are taken from.
struct EEFarmDistanceGreater : public EETileDistanceLess {
    EEFarmDistanceGreater(const EETileCoordinate& inCenter) : EETileDistanceLess(inCenter) {}
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const;
};

// NEAR: the original is built without optimization (ebp frame, `this` and every bool test through memory
// temporaries); `#pragma optimize("g", off)` alone does not reproduce it (59%), so it is not used

// FUNCTION: EXE 0x4c75a6
// Orders farms for smart farmers: unpaid before paid-for before built farms, the farther farm first in a tier
bool EEFarmDistanceGreater::operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
{
    bool theResult = false;
    if (inFirst->mIsBuilt) {
        if (inSecond->mIsBuilt)
            theResult = this->DistanceSquared(inFirst->GetTile())
                        > this->DistanceSquared(EETileCoordinate(inSecond->mTileX, inSecond->mTileY));
        else
            theResult = false;
    } else if (inSecond->mIsBuilt) {
        theResult = true;
    } else if (inFirst->mIsPaidFor) {
        if (inSecond->mIsPaidFor)
            theResult = this->DistanceSquared(inFirst->GetTile()) > this->DistanceSquared(inSecond->GetTile());
        else
            theResult = false;
    } else if (inSecond->mIsPaidFor) {
        theResult = true;
    } else {
        theResult = this->DistanceSquared(inFirst->GetTile())
                    > this->DistanceSquared(EETileCoordinate(inSecond->mTileX, inSecond->mTileY));
    }
    return theResult;
}

// FUNCTION: EXE 0x4c7739
// Sends the selected citizens to farm near the target: farmers whose farm drops here deposit, others take the
// nearest free farm, else gather to or move to the target
void EEMSmartFarmers::Execute()
{
    std::vector<EEComplexUnit*> theFarms;
    std::vector<EEUnit*> theOccupants;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EESelectedGroup* theSelection = thePlayer->mActiveGroup;
    EEComplexUnit* theFirstUnit = theSelection->GetFirstUnit();
    EEComplexUnit* theTarget = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(this->mTargetID));
    long theIndex = 1;
    if (theFirstUnit == NULL || theTarget == NULL)
        return;
    EEActiveGroup* theGroup = new EEActiveGroup(thePlayer);
    theGroup->Fill(theSelection, 0, 0);
    theGroup->KeepCitizens();
    long theCount = theGroup->GetNumUnits();
    if (theCount > 0) {
        EEComplexUnit* theFarm;
        do {
            theFarm = EEFindFreeFarm(theTarget, 1, &theIndex);
            if (theFarm != NULL)
                theFarms.push_back(theFarm);
        } while (theFarm != NULL);
        for (long i = 0; i < theCount; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            EEUGGather* theGather = dynamic_cast<EEUGGather*>(theUnit->GetLoadGoal());
            if (theGather != NULL && theGather->mResource != NULL && theGather->mResource->mUnitClass->mUnitType == kUnitTypeFarm) {
                theFarm = theGather->mResource->mComplexUnit;
                theOccupants.clear();
                if (EEGetFarmDropSites(theFarm, theOccupants)) {
                    EEUnit* theTargetUnit = theTarget;
                    if (std::find(theOccupants.begin(), theOccupants.end(), theTargetUnit) != theOccupants.end()
                        && theTarget->CanTake(theUnit, theUnit->mResourceType)) {
                        theUnit->SetGatheredResource(NULL);
                        theUnit->StartAction(kActionMoveToStorage);
                        theUnit->mUnitClass->ActionChanged(theUnit);
                        continue;
                    }
                } else
                    theFarm = NULL;
            } else
                theFarm = NULL;
            if (theFarm == NULL && theFarms.size() > 0) {
                std::sort(theFarms.begin(), theFarms.end(), EEFarmDistanceGreater(theUnit->GetTile()));
                theFarm = theFarms.back();
                theFarms.pop_back();
            }
            if (theFarm != NULL) {
                EEWorkFarm(theFarm, theUnit);
                continue;
            }
            if (theUnit->mResourceAmount > 0 && theTarget->CanTake(theUnit, theUnit->mResourceType)) {
                theUnit->SetGoal(new EEUGGather(NULL, theTarget, true), 0, 0);
                theUnit->StartAction(kActionMoveToStorage);
            } else {
                theUnit->SetGoal(new EEUGLocation(GE2DPoint((float)theTarget->mTileX, (float)theTarget->mTileY), true,
                                                  true, false, 0.0f, false, 0), 0, 0);
                theUnit->StartAction(kActionPrepareToMove);
            }
            theUnit->mUnitClass->ActionChanged(theUnit);
        }
    }
    if (theGroup->GetNumUnits() > 0)
        thePlayer->PlayAcknowledgement(theGroup->GetUnit(gPhysics->GetRandom(0L, (long)theGroup->GetNumUnits() - 1)));
    if (theGroup != NULL) {
        thePlayer->mPendingGroups.push_back(theGroup);
        theGroup->Finish();
    }
}

// FUNCTION: EXE 0x4c7aec
// Packs the smart-farmers order (header, selection id, target id, queued flag) into a new send buffer
void EEMSmartFarmers::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mTargetID, sizeof(this->mTargetID));
        theLocation += sizeof(this->mTargetID);
        memcpy(*outPayloadPointer + theLocation, &this->mIsQueued, sizeof(this->mIsQueued));
        theLocation += sizeof(this->mIsQueued);
        ESVERIFY_AT(theLocation == theMessageSize, 15969);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSmartFarmers::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c7b7b
// Returns the message's debug label "Smart Farmers"
char* EEMSmartFarmers::GetMessageLabel() const
{
    return "Smart Farmers";
}

// FUNCTION: EXE 0x4c7b81
// Reads the smart-farmers order (header, selection id, target id, queued flag) from a received payload
void EEMSmartFarmers::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16019);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mTargetID, inPayloadPointer + theLocation, sizeof(this->mTargetID));
    theLocation += sizeof(this->mTargetID);
    memcpy(&this->mIsQueued, inPayloadPointer + theLocation, sizeof(this->mIsQueued));
    theLocation += sizeof(this->mIsQueued);
}

bool EEMSmartFarmers::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c7be8
// Packs the start-loading message (header, selection id, human player count, random seed) into a new send buffer
void EEMStartLoading::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mNumHumanPlayers, sizeof(this->mNumHumanPlayers));
        theLocation += sizeof(this->mNumHumanPlayers);
        memcpy(*outPayloadPointer + theLocation, &this->mSeed, sizeof(this->mSeed));
        theLocation += sizeof(this->mSeed);
        ESVERIFY_AT(theLocation == theMessageSize, 16086);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMStartLoading::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c7c77
// Reads the start-loading message (header, selection id, human player count, random seed) from a received payload
void EEMStartLoading::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16096);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mNumHumanPlayers, inPayloadPointer + theLocation, sizeof(this->mNumHumanPlayers));
    theLocation += sizeof(this->mNumHumanPlayers);
    memcpy(&this->mSeed, inPayloadPointer + theLocation, sizeof(this->mSeed));
    theLocation += sizeof(this->mSeed);
}

bool EEMStartLoading::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c7cdc
// Starts loading the game: seeds the random generator, sets the human player count and multiplayer flag
void EEMStartLoading::Execute()
{
    FSFileSpec theFile;
    UString theName;
    gGameOptions.mLock->ReadLock();
    gGameOptions.Fn130272();
    gGameOptions.mLock->ReadUnlock();
    gPhysics->mRandom.SetSeed(this->mSeed);
    gUserInterface->mRandomSeed = this->mSeed;
    gCommunications->SetNumHumanPlayers(this->mNumHumanPlayers);
    gCommunications->SetIsMultiplayerSession(this->mNumHumanPlayers > 1);
    gUserInterface->PostStartLoading();
}

// FUNCTION: EXE 0x4c7d6d
// Returns the lag status message's payload size (8 bytes)
unsigned long EEMLagStatus::ExportSize()
{
    return 8;
}

// FUNCTION: EXE 0x4c7d71
// Packs the loading reply (header, selection id, slot, player id, lobby slot) into a new send buffer
void EEMStartLoadingResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        memcpy(*outPayloadPointer + theLocation, &this->mPlayerID, sizeof(this->mPlayerID));
        theLocation += sizeof(this->mPlayerID);
        memcpy(*outPayloadPointer + theLocation, &this->mLobbySlot, sizeof(this->mLobbySlot));
        theLocation += sizeof(this->mLobbySlot);
        ESVERIFY_AT(theLocation == theMessageSize, 16187);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMStartLoadingResponse::ExportSize()
{
    return 8;
}

// FUNCTION: EXE 0x4c7e0f
// Reads the loading reply (header, selection id, slot, player id, lobby slot) from a received payload
void EEMStartLoadingResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16197);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
    memcpy(&this->mPlayerID, inPayloadPointer + theLocation, sizeof(this->mPlayerID));
    theLocation += sizeof(this->mPlayerID);
    memcpy(&this->mLobbySlot, inPayloadPointer + theLocation, sizeof(this->mLobbySlot));
    theLocation += sizeof(this->mLobbySlot);
}

bool EEMStartLoadingResponse::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c7e84
// On the server, counts each slot's loading reply once; when every human has replied, sends the slot table
// and tells everyone to show the main screen
void EEMStartLoadingResponse::Execute()
{
    static std::vector<unsigned char> sResponses;
    if (gCommunications->IsServer()) {
        EEServer* theServer = static_cast<EEServer*>(gCommunications);
        unsigned char theNumResponses = theServer->mNumLoadResponses;
        if (!theNumResponses) {
            sResponses.clear();
            theServer->ClearSendToSlotArray();
        }
        if (std::find(sResponses.begin(), sResponses.end(), this->mSlot) == sResponses.end()) {
            sResponses.push_back(this->mSlot);
            theServer->mNumLoadResponses = theNumResponses + 1;
            if (this->mPlayerID != 0xff)
                theServer->SetSendToSlotForPlayer(this->mPlayerID, theServer->GetSendToSlot(this->mLobbySlot));
            if (theNumResponses + 1 == theServer->GetNumHumanPlayers()) {
                theServer->SendPlayerSlotInfo();
                theServer->mLoadState = 2;
                gCommunications->ProcessLocalUserInput(new EEMLoadMainScreen((unsigned char)0), kSlotNone);
            }
        }
    }
}

// FUNCTION: EXE 0x4c7fda
// Creates a load-main-screen message with the given flag byte
EEMLoadMainScreen::EEMLoadMainScreen(unsigned char inB10)
    : EEMessage(kMessageLoadMainScreen, true), mB10(inB10)
{
}

// FUNCTION: EXE 0x4c8011
// Packs the snap-to-daylight message (header, selection id) into a new send buffer
void EEMSnapToDaylight::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 16311);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSnapToDaylight::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c807c
// Reads the snap-to-daylight message (header, selection id) from a received payload
void EEMSnapToDaylight::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16321);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMSnapToDaylight::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c80bf
// Asks the physics thread to snap the day/night cycle to daylight
void EEMSnapToDaylight::Execute()
{
    gPhysics->mThread->Post(0x503, NULL, NULL, true);
}

// FUNCTION: EXE 0x4c80d9
// Packs the stop order (header, selection id) into a new send buffer
void EEMStopActiveGroup::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 16372);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMStopActiveGroup::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c8144
// Returns the message's debug label "Stop Active Group"
char* EEMStopActiveGroup::GetMessageLabel() const
{
    return "Stop Active Group";
}

// FUNCTION: EXE 0x4c814a
// Reads the stop order (header, selection id) from a received payload
void EEMStopActiveGroup::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16396);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMStopActiveGroup::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c818d
// Stops the sender's living selected units: aircraft drop their orders and, unless taking off, flying a
// pattern or landing, hold at their tile; other units halt
void EEMStopActiveGroup::Execute()
{
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    if (theGroup->GetFirstUnit()) {
        unsigned long theNumUnits = theGroup->GetNumUnits();
        for (unsigned long i = 0; i < theNumUnits; i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theUnit->mHitPoints > 0) {
                if (theUnit->mUnitClass->mCategory == kUnitCategoryAirplane || theUnit->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship) {
                    theUnit->ClearBuildTargets();
                    theUnit->ClearGoalQueue(0);
                    theUnit->ClearGoals(0);
                    unsigned long theCommand = theUnit->mCommand ? theUnit->mCommand->mType : 0;
                    switch (theCommand) {
                    case kActionTakeoff:
                    case kActionTaxi:
                    case kActionContinueToTaxi:
                    case kActionClimb:
                    case kActionContinueToClimb:
                    case kActionContinueToFly:
                    case kActionCircle:
                    case kActionWaitForLandingClearance:
                    case kActionHoldingPattern:
                    case kActionLand:
                    case kActionDescend:
                    case kActionContinueToDescend:
                    case kActionBrake:
                    case kActionContinueToBrake:
                    case kActionEmergencyTakeoff:
                    case kActionSearchForSecondaryTarget:
                    case kActionBoardTransport:
                    case kActionRecalculateLanding:
                    case kActionContinueToWaitForLandingClearance:
                    case kActionRequestLanding:
                    case kActionCircleAirtrafficController:
                        break;
                    case kActionFly:
                        theUnit->SetGoal(new EEUGLocation((float)theUnit->GetTileX(), (float)theUnit->GetTileY(), 0.0f, false, true,
                                                          false, 0.0f, false, false), 0, 0);
                        theUnit->StartAction(kActionPrepareToMove);
                        break;
                    default:
                        theUnit->SetGoal(new EEUGLocation((float)theUnit->GetTileX(), (float)theUnit->GetTileY(), 0.0f, false, true,
                                                          false, 0.0f, false, false), 0, 0);
                        theUnit->StartAction(kActionPrepareToMove);
                        break;
                    }
                    theUnit->mUnitClass->ActionChanged(theUnit);
                } else
                    theUnit->Halt();
            }
        }
    }
}

// FUNCTION: EXE 0x4c8312
// Packs the collision debug message (header, selection id, cell size, mode value) into a new send buffer
void EEMToggleCollisionDetection::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mDivisor, sizeof(this->mDivisor));
        theLocation += sizeof(this->mDivisor);
        memcpy(*outPayloadPointer + theLocation, &this->mL14, sizeof(this->mL14));
        theLocation += sizeof(this->mL14);
        ESVERIFY_AT(theLocation == theMessageSize, 16529);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMToggleCollisionDetection::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c83a3
// Reads the collision debug message (header, selection id, cell size, mode value) from a received payload
void EEMToggleCollisionDetection::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16539);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mDivisor, inPayloadPointer + theLocation, sizeof(this->mDivisor));
    theLocation += sizeof(this->mDivisor);
    memcpy(&this->mL14, inPayloadPointer + theLocation, sizeof(this->mL14));
    theLocation += sizeof(this->mL14);
}

bool EEMToggleCollisionDetection::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c840a
// Debug: when the mode value is negative, sets the collision cell size (if positive) or cycles the
// collision flags 0..15
void EEMToggleCollisionDetection::Execute()
{
    if (this->mL14 < 0) {
        if (this->mDivisor > 0)
            gCollisionDetectionUnit = this->mDivisor;
        else
            gWorld.mCollisionDetectionFlags = (gWorld.mCollisionDetectionFlags + 1) % 16;
    }
}

// FUNCTION: EXE 0x4c8431
// Packs the day/night toggle (header, selection id) into a new send buffer
void EEMToggleDayNight::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 16606);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMToggleDayNight::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c849c
// Reads the day/night toggle (header, selection id) from a received payload
void EEMToggleDayNight::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16616);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMToggleDayNight::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c84df
// Asks the physics thread to toggle the day/night cycle
void EEMToggleDayNight::Execute()
{
    gPhysics->mThread->Post(0x502, NULL, NULL, true);
}

// FUNCTION: EXE 0x4c84f9
// Packs the unload order (header, selection id, unit id, x, y) into a new send buffer
void EEMUnloadTransport::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        memcpy(*outPayloadPointer + theLocation, &this->mX, sizeof(this->mX));
        theLocation += sizeof(this->mX);
        memcpy(*outPayloadPointer + theLocation, &this->mY, sizeof(this->mY));
        theLocation += sizeof(this->mY);
        ESVERIFY_AT(theLocation == theMessageSize, 16680);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMUnloadTransport::ExportSize()
{
    return 17;
}

// FUNCTION: EXE 0x4c859d
// Reads the unload order (header, selection id, unit id, x, y) from a received payload
void EEMUnloadTransport::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16691);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
    memcpy(&this->mX, inPayloadPointer + theLocation, sizeof(this->mX));
    theLocation += sizeof(this->mX);
    memcpy(&this->mY, inPayloadPointer + theLocation, sizeof(this->mY));
    theLocation += sizeof(this->mY);
}

bool EEMUnloadTransport::IsResponse(NEMessage* inMessage)
{
    return false;
}

#include "ee/EEUGLocation.h"
#include "ee/EEUGUnloadTransport.h"

// FUNCTION: EXE 0x4c8615
// Splits the sender's selection by kind; parts with a loaded transport unload at the point, the rest move
// there (a loaded transport that cannot unload there raises a warning event)
void EEMUnloadTransport::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    bool theCanUnload = false;
    EESelectedGroup* theGroup = thePlayer->mActiveGroup;
    if (theGroup->GetFirstUnit() == NULL)
        return;
    for (unsigned long i = 1; i < 6; i++) {
        EEActiveGroup* theActiveGroup = new EEActiveGroup(thePlayer);
        theActiveGroup->Fill(theGroup, 1, i);
        if (theActiveGroup->GetNumUnits() > 0) {
            thePlayer->mPendingGroups.push_back(theActiveGroup);
            theActiveGroup->SpreadForUnload(this->GetX(), this->GetY());
            EEComplexUnit* theUnit = theActiveGroup->GetFirstUnit();
            if (theUnit == NULL)
                continue;
            EEUGUnloadTransport* theGoal = new EEUGUnloadTransport(theUnit->GetComplexUnit(), this->GetX(), this->GetY(), false);
            unsigned long theCount = theActiveGroup->GetNumUnits();
            for (unsigned long j = 0; j < theCount && !theCanUnload; j++) {
                theUnit = theActiveGroup->GetUnit(j);
                if ((theUnit->GetUnitClass()->IsTransport() || theUnit->GetUnitClass()->mIsParatrooperBomber)
                    && theUnit->mComplexUnit->GetNumCargoUnits() > 0)
                    theCanUnload = true;
                else
                    theCanUnload = false;
            }
            if (theCanUnload) {
                theGoal->SetTransport(theUnit->mComplexUnit);
                if (theCanUnload && theGoal->CanUnloadAtDestination()) {
                    theActiveGroup->ClearGoal();
                    theActiveGroup->SetGoal(theGoal, false);
                    theActiveGroup->mAction = kActionUnloadTransport;
                    theActiveGroup->Plan();
                    thePlayer->PlayAcknowledgement(theUnit);
                    continue;
                }
            }
            delete theGoal;
            if ((theUnit->mUnitClass->IsTransport() || theUnit->mUnitClass->mIsParatrooperBomber)
                && theUnit->mComplexUnit->GetNumCargoUnits() > 0) {
                U2DPoint<long> theTile(this->GetX(), this->GetY());
                thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, 0x7c3b, false);
            }
            theActiveGroup->ClearGoal();
            theActiveGroup->SetGoal(new EEUGLocation((float)this->GetX(), (float)this->GetY(), 0.0f, false, true, false, 0.0f, false, 0),
                                    false);
            theActiveGroup->mAction = kActionPrepareToMove;
            theActiveGroup->Plan();
            EESendGroupCommand(theActiveGroup, NULL, thePlayer->mPlayerID, 0xb8, 0);
            thePlayer->PlayAcknowledgement(theUnit);
        } else
            theActiveGroup->Dereference();
    }
}

// FUNCTION: EXE 0x4c889e
// Clears the sender's selection
void EEMUnselectAllUnits::Execute()
{
    gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->UnselectAllUnits();
}

// FUNCTION: EXE 0x4c88be
// Packs the deselect-all message (header, selection id) into a new send buffer
void EEMUnselectAllUnits::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        ESVERIFY_AT(theLocation == theMessageSize, 16886);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMUnselectAllUnits::ExportSize()
{
    return 5;
}

// FUNCTION: EXE 0x4c8929
// Returns the message's debug label "Unselect All Units"
char* EEMUnselectAllUnits::GetMessageLabel() const
{
    return "Unselect All Units";
}

// FUNCTION: EXE 0x4c892f
// Reads the deselect-all message (header, selection id) from a received payload
void EEMUnselectAllUnits::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 16934);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
}

bool EEMUnselectAllUnits::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c8972
// Stores a slot's ping in the game options under the write lock and refreshes that lobby row
void EEMUpdatePingStat::Execute()
{
    EEGameOptionsPlayer thePlayer;
    gGameOptions.mLock->WriteLock();
    thePlayer = gGameOptions.mPlayers[this->mSlot];
    thePlayer.mPing = this->mPing;
    gGameOptions.mPlayers[this->mSlot] = thePlayer;
    gGameOptions.mLock->WriteUnlock();
    gUserInterface->PostLobbyUpdatePlayer(this->mSlot, 1);
}

// FUNCTION: EXE 0x4c89ea
// Packs the ping update (header, selection id, slot, ping) into a new send buffer
void EEMUpdatePingStat::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mSlot, sizeof(this->mSlot));
        theLocation += sizeof(this->mSlot);
        memcpy(*outPayloadPointer + theLocation, &this->mPing, sizeof(this->mPing));
        theLocation += sizeof(this->mPing);
        ESVERIFY_AT(theLocation == theMessageSize, 17019);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMUpdatePingStat::ExportSize()
{
    return 10;
}

// FUNCTION: EXE 0x4c8a79
// Returns the message's debug label "Update Ping Stat"
char* EEMUpdatePingStat::GetMessageLabel() const
{
    return "Update Ping Stat";
}

// FUNCTION: EXE 0x4c8a7f
// Reads the ping update (header, selection id, slot, ping) from a received payload
void EEMUpdatePingStat::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 17069);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mSlot, inPayloadPointer + theLocation, sizeof(this->mSlot));
    theLocation += sizeof(this->mSlot);
    memcpy(&this->mPing, inPayloadPointer + theLocation, sizeof(this->mPing));
    theLocation += sizeof(this->mPing);
}

bool EEMUpdatePingStat::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c8ae4
// Queues the improvement for the unit's type if its owner can buy it; otherwise the local owner hears an error
// click and sees the reason in the chat log
void EEMImproveUnit::Execute()
{
    UWideString theText;
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    if (theUnit && theUnit->mHitPoints > 0) {
        EEPlayer* theOwner = theUnit->mPlayer;
        if (theOwner->CanBuy(theUnit->mUnitClass->GetUnitType(), this->mImprovement, &theText))
            theOwner->QueueImprovement(theUnit->mUnitClass->mUnitType, this->mImprovement, 0, 0);
        else {
            if (theOwner == gWorld.GetCurrentPlayer())
                gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
            if (theText.GetLength() > 0 && theOwner == gWorld.GetCurrentPlayer())
                gUserInterface->AddChatLogEntry(theText, 0);
        }
    }
}

// FUNCTION: EXE 0x4c8bc3
// Packs the improvement order (header, selection id, unit id, improvement) into a new send buffer
void EEMImproveUnit::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mUnitID, sizeof(this->mUnitID));
        theLocation += sizeof(this->mUnitID);
        memcpy(*outPayloadPointer + theLocation, &this->mImprovement, sizeof(this->mImprovement));
        theLocation += sizeof(this->mImprovement);
        ESVERIFY_AT(theLocation == theMessageSize, 17182);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMImproveUnit::ExportSize()
{
    return 13;
}

// FUNCTION: EXE 0x4c8c54
// Reads the improvement order (header, selection id, unit id, improvement) from a received payload
void EEMImproveUnit::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 17217);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mUnitID, inPayloadPointer + theLocation, sizeof(this->mUnitID));
    theLocation += sizeof(this->mUnitID);
    memcpy(&this->mImprovement, inPayloadPointer + theLocation, sizeof(this->mImprovement));
    theLocation += sizeof(this->mImprovement);
}

bool EEMImproveUnit::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c8cbb
// During recorded-game playback, applies the sender's camera zoom when the sender is the local player
void EEMZoomInfo::Execute()
{
    if ((gCommunications->IsClient() == true
         && static_cast<NESynchronousClient*>(gCommunications)->GetRecordPlayback()->GetMode() == kRecordModePlaying)
        || (gCommunications->IsServer() == true
            && static_cast<NESynchronousServer*>(gCommunications)->GetRecordPlayback()->GetMode() == kRecordModePlaying)) {
        if (this->GetOriginatingSlot() == gWorld.GetCurrentPlayer()->mSlot)
            gWorld.mCameraControl.SetZoom(this->mZoom);
    }
}

// FUNCTION: EXE 0x4c8d23
// Packs the zoom message (header, selection id, zoom) into a new send buffer
void EEMZoomInfo::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mZoom, sizeof(this->mZoom));
        theLocation += sizeof(this->mZoom);
        ESVERIFY_AT(theLocation == theMessageSize, 17295);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMZoomInfo::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c8da1
// Reads the zoom message (header, selection id, zoom) from a received payload
void EEMZoomInfo::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 17329);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mZoom, inPayloadPointer + theLocation, sizeof(this->mZoom));
    theLocation += sizeof(this->mZoom);
}

bool EEMZoomInfo::IsResponse(NEMessage* inMessage)
{
    return false;
}

// FUNCTION: EXE 0x4c8df1
// Sets the behaviour (stance) of the sender's selected units that can change it
void EEMSetUnitBehavior::Execute()
{
    EEGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    EEUnitBehavior* theBehavior = EEUnitBehavior::sTable[this->mBehavior];
    for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
        if (theGroup->GetUnit(i)->mUnitClass->mCanChangeBehavior) {
            EEUnitBehavior*& theUnitBehavior = theGroup->GetUnit(i)->mBehavior;
            if (theUnitBehavior != theBehavior)
                theUnitBehavior = theBehavior;
        }
    }
}

// FUNCTION: EXE 0x4c8e64
// Packs the behaviour order (header, selection id, behaviour) into a new send buffer
void EEMSetUnitBehavior::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer != NULL) {
        unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
        memcpy(*outPayloadPointer + theLocation, &this->mFastSelectionID, sizeof(this->mFastSelectionID));
        theLocation += sizeof(this->mFastSelectionID);
        memcpy(*outPayloadPointer + theLocation, &this->mBehavior, sizeof(this->mBehavior));
        theLocation += sizeof(this->mBehavior);
        ESVERIFY_AT(theLocation == theMessageSize, 17413);
        outMessageSize = theMessageSize;
    }
}

unsigned long EEMSetUnitBehavior::ExportSize()
{
    return 9;
}

// FUNCTION: EXE 0x4c8ee2
// Reads the behaviour order (header, selection id, behaviour) from a received payload
void EEMSetUnitBehavior::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 17448);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mFastSelectionID, inPayloadPointer + theLocation, sizeof(this->mFastSelectionID));
    theLocation += sizeof(this->mFastSelectionID);
    memcpy(&this->mBehavior, inPayloadPointer + theLocation, sizeof(this->mBehavior));
    theLocation += sizeof(this->mBehavior);
}

bool EEMSetUnitBehavior::IsResponse(NEMessage* inMessage)
{
    return false;
}

// Inline members of ee/EEMessage.h whose first user is the message factory (0xba41b..0xbadd0): the import
// ctors VC6 refused to expand there, the labels and other virtuals pulled in with the vtables, ??_G
// SYNTHETIC: EXE 0x4ba41b   ??0EEMSelectUnits@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba47b   ??0EEMAddPatrolPoint@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba4b8   ?GetMessageLabel@EEMAddRemoveResources@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba4be   ??0EEMAddUnitToSelectedGroup@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba4f1   ?GetMessageLabel@EEMAttackGround@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba4f7   ?GetMessageLabel@EEMAttackPlayers@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba4fd   ??0EEMBuildGate@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba53a   ??0EEMBuildUnit@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba577   ??0EEMBuildWall@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba5be   ??0EEMCameraLocationInfo@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba600   ?GetMessageLabel@EEMCameraLocationInfo@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba606   ??0EEMChat@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba64a   ??_GEEMChat@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba684   ??0EEMCivUpgrades@@QAE@XZ
// SYNTHETIC: EXE 0x4ba6d1   ?SetName@EEMCivUpgrades@@UAEXABVUWideString@@@Z
// SYNTHETIC: EXE 0x4ba6e1   ??_GEEMCivUpgrades@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba6fd   ??1EEMCivUpgrades@@UAE@XZ
// SYNTHETIC: EXE 0x4ba727   ??_GEEMCivUpgradesPurchased@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba749   ??_GEEMCivUpgradesRequested@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba76b   ?GetMessageLabel@EEMDisband@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba771   ?GetMessageLabel@EEMEdictPreProposal@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba777   ?GetMessageLabel@EEMEdictPreProposalResponse@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba77d   ?GetMessageLabel@EEMEdictProposal@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba783   ?GetMessageLabel@EEMEdictVote@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba789   ??0EEMExchangeResources@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba7bc   ?GetMessageLabel@EEMExplore@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba7c2   ??0EEMGameOptions@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba806   ??_GEEMGameOptions@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba83a   ??0EEMFullGameOptions@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba87e   ??0EEMHackANewUnit@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba8bb   ?GetMessageLabel@EEMHackANewUnit@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba8c1   ?GetMessageLabel@EEMImmediateCargoUnload@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba8c7   ?GetMessageLabel@EEMIncrementEpic@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba8cd   ??0EEMInGameChat@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba911   ??_GEEMInGameChat@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba94b   ?GetMessageLabel@EEMLoadMainScreen@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba951   ?GetMessageLabel@EEMLoadMainScreenResponse@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba957   ??0EEMLoadOrSaveGame@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba99b   ?GetMessageLabel@EEMLoadOrSaveGame@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba9a1   ?GetSendToAllAndWaitMessageID@EEMLoadOrSaveGame@@UBE_NAAK@Z
// SYNTHETIC: EXE 0x4ba9af   ?SetSendToAllAndWaitMessageID@EEMLoadOrSaveGame@@UAEXK@Z
// SYNTHETIC: EXE 0x4ba9b9   ??_GEEMLoadOrSaveGame@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba9ed   ?GetMessageLabel@EEMLoadOrSaveGameResponse@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba9f3   ?GetMessageLabel@EEMLoadPercent@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba9f9   ?GetMessageLabel@EEMLoadUnitData@@UBEPADXZ
// SYNTHETIC: EXE 0x4ba9ff   ?GetMessageLabel@EEMLoadGarrison@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa05   ?GetMessageLabel@EEMSelfDestruct@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa0b   ?GetMessageLabel@EEMUnloadTransport@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa11   ?GetMessageLabel@EEMKillUnit@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa17   ?GetMessageLabel@EEMMakeWorldVisible@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa1d   ?GetMessageLabel@EEMMigrationSlotInfo@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa23   ?GetMessageLabel@EEMNewEdictEnacted@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa29   ?GetMessageLabel@EEMPayTribute@@UBEPADXZ
// SYNTHETIC: EXE 0x4baa2f   ??0EEMFullPlayerInformation@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4baa73   ??0EEMPlayerSlotInfo@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4baab7   ?GetMessageLabel@EEMPlayerSlotInfo@@UBEPADXZ
// SYNTHETIC: EXE 0x4baabd   ??_GEEMFullGameOptions@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4baaf1   ??0EEMSelectTheseUnits@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bab3d   ??_GEEMSelectTheseUnits@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4bab76   ??0EEMSetAirportCommand@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4babb3   ?GetMessageLabel@EEMSetAirportCommand@@UBEPADXZ
// SYNTHETIC: EXE 0x4babb9   ?GetMessageLabel@EEMSetDiplomaticStance@@UBEPADXZ
// SYNTHETIC: EXE 0x4babbf   ??0EEMSetRallyGoal@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4babfc   ?GetMessageLabel@EEMSendFlare@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac02   ?GetMessageLabel@EEMRecordFrames@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac08   ?GetMessageLabel@EEMStartLoading@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac0e   ?GetMessageLabel@EEMStartLoadingResponse@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac14   ?GetMessageLabel@EEMSetWaterLevel@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac1a   ??0EEMSnapToDaylight@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bac4d   ?GetMessageLabel@EEMSnapToDaylight@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac53   ??0EEMToggleCollisionDetection@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bac86   ?GetMessageLabel@EEMToggleCollisionDetection@@UBEPADXZ
// SYNTHETIC: EXE 0x4bac8c   ??0EEMToggleDayNight@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bacbf   ?GetMessageLabel@EEMToggleDayNight@@UBEPADXZ
// SYNTHETIC: EXE 0x4bacc5   ??0EEMUnselectAllUnits@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bacf8   ??0EEMUpdatePingStat@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bad2b   ??0EEMImproveUnit@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bad5e   ??0EEMZoomInfo@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bad91   ?GetMessageLabel@EEMZoomInfo@@UBEPADXZ
// SYNTHETIC: EXE 0x4bad97   ??0EEMSetUnitBehavior@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4badca   ?GetMessageLabel@EEMSetUnitBehavior@@UBEPADXZ
// SYNTHETIC: EXE 0x4bb02a   ?ClearGoal@EEActiveGroup@@QAEXXZ
// SYNTHETIC: EXE 0x4bd11c   ?ProcessEvent@EEUIEProcessTimerExpired@@UAEJPAVUIForm@@PAVUIEntity@@@Z
// SYNTHETIC: EXE 0x4bdb42   ??0EEAAddChatMessage@@QAE@ABVUWideString@@_N@Z
// SYNTHETIC: EXE 0x4bdc00   ??_GEEAAddChatMessage@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4c0c9f   $E60
// SYNTHETIC: EXE 0x4c8007   $E62
// SYNTHETIC: EXE 0x4c350f   ?AddTribute@EEPlayer@@QAEXJJJ@Z
// SYNTHETIC: EXE 0x4c8f32   ?push_back@?$vector@EV?$allocator@E@std@@@std@@QAEXABE@Z
// SYNTHETIC: EXE 0x4c8f5e   ?clear@?$vector@EV?$allocator@E@std@@@std@@QAEXXZ
// SYNTHETIC: EXE 0x4c8f99   ?push_back@?$vector@PAVEEGroup@@V?$allocator@PAVEEGroup@@@std@@@std@@QAEXABQAVEEGroup@@@Z
// SYNTHETIC: EXE 0x4c948f   ?find@std@@YA?AV?$_Ptrit@EHPAEAAEPAEAAE@1@V21@0ABE@Z
// SYNTHETIC: EXE 0x4c8fc5   ??A?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@std@@QAEAAV?$vector@KV?$allocator@K@std@@@1@ABK@Z
// SYNTHETIC: EXE 0x4c901b   ??1?$_Tree@KU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@U_Kfn@?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@2@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@$0A@@std@@QAE@XZ
// SYNTHETIC: EXE 0x4ca2d4   ?clear@?$_Tree@EU?$pair@$$CBEE@std@@U_Kfn@?$map@EEU?$less@E@std@@V?$allocator@U?$pair@$$CBEE@std@@@2@@2@U?$less@E@2@V?$allocator@U?$pair@$$CBEE@std@@@2@$0A@@std@@QAEXXZ
// SYNTHETIC: EXE 0x4c9072   ?insert@?$vector@EV?$allocator@E@std@@@std@@QAE?AV?$_Ptrit@EHPAEAAEPAEAAE@2@V32@ABE@Z
// SYNTHETIC: EXE 0x4c90b6   ?insert@?$_Tree@KU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@U_Kfn@?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@2@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@$0A@@std@@QAE?AU?$pair@Viterator@?$_Tree@KU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@U_Kfn@?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@2@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@$0A@@std@@_N@2@ABU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@2@@Z
// SYNTHETIC: EXE 0x4c916c   ?erase@?$_Tree@KU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@U_Kfn@?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@2@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@0@Z
// SYNTHETIC: EXE 0x4ca54f   ?erase@?$_Tree@EU?$pair@$$CBEE@std@@U_Kfn@?$map@EEU?$less@E@std@@V?$allocator@U?$pair@$$CBEE@std@@@2@@2@U?$less@E@2@V?$allocator@U?$pair@$$CBEE@std@@@2@$0A@@std@@QAE?AViterator@12@V312@0@Z
// SYNTHETIC: EXE 0x4ca2e8   ?insert@?$vector@EV?$allocator@E@std@@@std@@QAEXV?$_Ptrit@EHPAEAAEPAEAAE@2@IABE@Z
// SYNTHETIC: EXE 0x4c91dd   ?_Erase@?$_Tree@KU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@U_Kfn@?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@2@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z
// SYNTHETIC: EXE 0x4c9217   ?_Insert@?$_Tree@KU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@U_Kfn@?$map@KV?$vector@KV?$allocator@K@std@@@std@@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@@2@U?$less@K@2@V?$allocator@U?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@2@@Z
// SYNTHETIC: EXE 0x4ca5c0   ?erase@?$_Tree@EU?$pair@$$CBEE@std@@U_Kfn@?$map@EEU?$less@E@std@@V?$allocator@U?$pair@$$CBEE@std@@@2@@2@U?$less@E@2@V?$allocator@U?$pair@$$CBEE@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z
// SYNTHETIC: EXE 0x4ca86e   ?_Erase@?$_Tree@EU?$pair@$$CBEE@std@@U_Kfn@?$map@EEU?$less@E@std@@V?$allocator@U?$pair@$$CBEE@std@@@2@@2@U?$less@E@2@V?$allocator@U?$pair@$$CBEE@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z
// SYNTHETIC: EXE 0x4ca8a0   ?_Inc@const_iterator@?$_Tree@EU?$pair@$$CBEE@std@@U_Kfn@?$map@EEU?$less@E@std@@V?$allocator@U?$pair@$$CBEE@std@@@2@@2@U?$less@E@2@V?$allocator@U?$pair@$$CBEE@std@@@2@$0A@@std@@QAEXXZ
// SYNTHETIC: EXE 0x4ca8d8   ?fill@std@@YAXV?$_Ptrit@EHPAEAAEPAEAAE@1@0ABE@Z
// SYNTHETIC: EXE 0x4ca8ee   ?copy_backward@std@@YA?AV?$_Ptrit@EHPAEAAEPAEAAE@1@V21@00@Z
// SYNTHETIC: EXE 0x4c9594   ?_Construct@std@@YAXPAU?$pair@$$CBKV?$vector@KV?$allocator@K@std@@@std@@@1@ABU21@@Z

// EEMBuildUnit::Execute's std::sort over vector<EEComplexUnit*> with EETileDistanceLess (Dinkum 2.33, deferred
// template COMDATs; the predicate 0xc995c and the second tree 0xc94b0.., from 0xc7739, are open):
// TEMPLATE: EXE 0x4c93ab
// TEMPLATE: EXE 0x4c95b1
// TEMPLATE: EXE 0x4c95e4
// TEMPLATE: EXE 0x4c96ec
// TEMPLATE: EXE 0x4c98ab
// TEMPLATE: EXE 0x4c9a43
// TEMPLATE: EXE 0x4c9a87
// TEMPLATE: EXE 0x4c9ad4
// TEMPLATE: EXE 0x4c9bf4
// TEMPLATE: EXE 0x4c9eb7
// TEMPLATE: EXE 0x4c9f14
// TEMPLATE: EXE 0x4c9fd7
// TEMPLATE: EXE 0x4ca051
// TEMPLATE: EXE 0x4ca1ad
// TEMPLATE: EXE 0x4ca1fc
// TEMPLATE: EXE 0x4c7486   EEComplexUnit::CanAttackClass
// TEMPLATE: EXE 0x4c995c   EETileDistanceLess::operator()(EEComplexUnit*, EEComplexUnit*)
// TEMPLATE: EXE 0x4c9046   vector<EEUnitGoal*>::push_back (EEMSetRallyGoal)
// TEMPLATE: EXE 0x4c94b0   std::sort<EEComplexUnit*, EEFarmDistanceGreater> (EEMSmartFarmers) and its helpers
// TEMPLATE: EXE 0x4c972e
// TEMPLATE: EXE 0x4c9761
// TEMPLATE: EXE 0x4c9869
// TEMPLATE: EXE 0x4c9c27
// TEMPLATE: EXE 0x4c9cd8
// TEMPLATE: EXE 0x4c9d1c
// TEMPLATE: EXE 0x4c9d69
// TEMPLATE: EXE 0x4c9e84
// TEMPLATE: EXE 0x4c9f47
// TEMPLATE: EXE 0x4c9fa4
// TEMPLATE: EXE 0x4ca0c1
// TEMPLATE: EXE 0x4ca13d
// TEMPLATE: EXE 0x4ca240
// TEMPLATE: EXE 0x4ca290
