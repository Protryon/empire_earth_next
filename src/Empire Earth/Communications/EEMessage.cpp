// C:\EEX\Empire Earth\Communications\EEMessage.cpp (__FILE__ 0x4bac84). The game's network messages: 0xb8cda..0xca90f.
// Unit flags: the exe's module set. Members per class in the original's order (mostly alphabetical: ctor, Execute,
// ExportMessage, ExportSize, GetMessageLabel, ImportMessage, IsResponse), classes alphabetical. The import ctors
// (`EEMxxx(unsigned char*)`) and some labels are inline in ee/EEMessage.h: their out-of-line copies follow the message
// factory (0xba41b..0xbadd0; SYNTHETIC markers at the end). ESVERIFY_AT carries the original's __LINE__.
#include <windows.h>
#include "platform/PlatSim.h"
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
#include "ee/EEUnitKind.h"
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
#include "ee/EEGameLog.h"
#include "ee/EEMessage.h"
#include "ee/EECPEngine.h"
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
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"
#include "ee/EEActiveGroup.h"
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
#include "ee/EEMessageCapture.h"
#include "ee/EEMessageExecute.h"
#include "rust/ee_netmsg.h"   // ee_netmsg
#include "rust/ee_units.h"
#include "ee/EEUnitHost.h"   // EESortFarms
static unsigned char CompressUnits(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer);
static void DecompressUnits(unsigned char* inBuffer, unsigned char inSize, std::vector<long>& outUnits);

// ---- the codec (ee_netmsg, docs/RUST_PLAN.md R5b.4): each message hands its members to the Rust as slots in wire
// order, the header's four first; the layout (sizes, order, the strings' and lists' forms) is the Rust table's ----
static const size_t kNetMsgMaxSlots = 32;

template <class T> static inline EeNetMsgSlot EENetMsgField(T& inMember)
{
    EeNetMsgSlot theSlot = {(uint8_t*)&inMember, (uint32_t)sizeof(T), 0, 0};
    return theSlot;
}

// a string's bytes (UWideString::Export's strlen); on import the Rust reports where the string is
static inline EeNetMsgSlot EENetMsgString(const UWideString& inString)
{
    const char* theBytes = inString;
    EeNetMsgSlot theSlot = {(uint8_t*)theBytes, (uint32_t)strlen(theBytes), 0, 0};
    return theSlot;
}

// records: on export the first record, a record's size and the count
static inline EeNetMsgSlot EENetMsgRecords(void* inRecords, uint32_t inSize, size_t inCount)
{
    EeNetMsgSlot theSlot = {(uint8_t*)inRecords, inSize, (uint32_t)inCount, 0};
    return theSlot;
}

// a new buffer of the message's size, filled by the Rust (the original's ExportMessage shape)
static void EENetMsgExport(uint32_t inClass, EeNetMsgSlot* inSlots, size_t inCount, unsigned char** outPayloadPointer,
                           unsigned long& outMessageSize)
{
    unsigned long theMessageSize = ee_netmsg_export_size(inClass, inSlots, inCount);
    *outPayloadPointer = new unsigned char[theMessageSize];
    outMessageSize = ee_netmsg_export(inClass, inSlots, inCount, *outPayloadPointer, theMessageSize);
}

// New (game messages lane, round 1): the codec's side of every game message, once. A class gives its codec class
// and its members as slots in wire order (GetCodecSlots); these run the codec over them, as each class's own
// ExportMessage, ExportSize and ImportMessage did (a class whose members the slots cannot carry overrides them).
size_t EEMessage::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    // only a class that overrides the three (EEMSelectTheseUnits) has none: the codec refuses the class
    outClass = 0xffffffffu;
    return 0;
}

void EEMessage::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    EENetMsgExport(theClass, theSlots, theCount, outPayloadPointer, outMessageSize);
}

// a class of fixed members its constant, else the size of these members
unsigned long EEMessage::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    uint32_t theFixed = ee_netmsg_fixed_size(theClass);
    return theFixed != 0 ? theFixed : ee_netmsg_export_size(theClass, theSlots, theCount);
}

void EEMessage::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, __LINE__);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    ee_netmsg_import(theClass, inPayloadPointer, theSlots, theCount);
}

// EEMSelectTheseUnits' slot list (its id list is read and written around the codec)
static size_t Slots(EEMSelectTheseUnits* inMessage, EeNetMsgSlot* outSlots, std::vector<int32_t>& ioIDs);

// the Execute decisions (ee/EEMessageExecute.h), each call recorded by the capture's NMDC record (off by default)
static inline uint8_t DecideSelectionMarksLocal(uint8_t inServer, uint8_t inLocal)
{
    uint8_t theResult = ee_netmsg_selection_marks_local(inServer, inLocal);
    long long theValues[] = {inServer, inLocal, theResult};
    EEMessageCaptureDecision(kDecisionSelectionMarksLocal, theValues, 3);
    return theResult;
}

static inline uint8_t DecideSelectionCopies(uint8_t inLocal, uint8_t inID, uint8_t inCurrentID)
{
    uint8_t theResult = ee_netmsg_selection_copies(inLocal, inID, inCurrentID);
    long long theValues[] = {inLocal, inID, inCurrentID, theResult};
    EEMessageCaptureDecision(kDecisionSelectionCopies, theValues, 4);
    return theResult;
}

static inline uint8_t DecideCommandRuns(uint8_t inServer, uint8_t inType, uint8_t inID, uint8_t inLastID)
{
    uint8_t theResult = ee_netmsg_command_runs(inServer, inType, inID, inLastID);
    long long theValues[] = {inServer, inType, inID, inLastID, theResult};
    EEMessageCaptureDecision(kDecisionCommandRuns, theValues, 5);
    return theResult;
}

// the selection toggles' passes (the communications object replaying a selection)
static inline uint32_t DecideSelectionPasses()
{
    uint8_t theExecuting = (gCommunications->IsClient() && static_cast<EEClient*>(gCommunications)->mIsExecutingSelection)
        || (gCommunications->IsServer() && static_cast<EEServer*>(gCommunications)->mIsExecutingSelection);
    uint32_t theResult = ee_netmsg_selection_passes(theExecuting);
    long long theValues[] = {theExecuting, theResult};
    EEMessageCaptureDecision(kDecisionSelectionPasses, theValues, 2);
    return theResult;
}

static inline uint8_t DecideAddRemove(uint8_t inAdd, uint32_t inAmount, long inHave)
{
    uint8_t theResult = ee_netmsg_add_remove(inAdd, inAmount, inHave);
    long long theValues[] = {inAdd, inAmount, inHave, theResult};
    EEMessageCaptureDecision(kDecisionAddRemove, theValues, 4);
    return theResult;
}

static inline EeNetMsgExchangeStep DecideExchangeStep(uint8_t inSell, long inPrice, long inHave)
{
    EeNetMsgExchangeStep theStep = ee_netmsg_exchange_step(inSell, inPrice, inHave);
    long long theValues[] = {inSell, inPrice, inHave, theStep.apply, theStep.cost, theStep.amount};
    EEMessageCaptureDecision(kDecisionExchangeStep, theValues, 6);
    return theStep;
}

static inline uint32_t DecideTributeOrder(const uint32_t* inAmounts, uint8_t inText, uint8_t* outResources)
{
    uint32_t theCount = ee_netmsg_tribute_order(inAmounts, inText, outResources);
    long long theValues[] = {inAmounts[2], inAmounts[3], inAmounts[4], inAmounts[5], inAmounts[6], inText, theCount,
                             theCount > 0 ? outResources[0] : 0, theCount > 1 ? outResources[1] : 0,
                             theCount > 2 ? outResources[2] : 0, theCount > 3 ? outResources[3] : 0,
                             theCount > 4 ? outResources[4] : 0};
    EEMessageCaptureDecision(kDecisionTributeOrder, theValues, 12);
    return theCount;
}

static inline uint8_t DecideTributeViewer(uint8_t inFrom, uint8_t inTo)
{
    uint8_t theResult = ee_netmsg_tribute_viewer(inFrom, inTo);
    long long theValues[] = {inFrom, inTo, theResult};
    EEMessageCaptureDecision(kDecisionTributeViewer, theValues, 3);
    return theResult;
}

static inline uint8_t DecideDisbandRefund(long inHitPoints, long inMaxHitPoints, uint8_t inCanDisband, float* outFraction)
{
    float theFraction = 0.0f;
    uint8_t theResult = ee_netmsg_disband_refund(inHitPoints, inMaxHitPoints, inCanDisband, &theFraction);
    uint32_t theBits;
    memcpy(&theBits, &theFraction, 4);
    long long theValues[] = {inHitPoints, inMaxHitPoints, inCanDisband, theResult, theBits};
    EEMessageCaptureDecision(kDecisionDisbandRefund, theValues, 5);
    *outFraction = theFraction;
    return theResult;
}

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
        uint8_t theServer = gCommunications->IsServer() ? 1 : 0;
        uint8_t theLocal = thePlayer->mPlayerID == gWorld.GetCurrentPlayer()->mPlayerID ? 1 : 0;
        TSCriticalSection* theLock;
        unsigned char* theCurrentID;
        unsigned char* theLastIDs;
        if (theServer) {
            theLock = static_cast<EEServer*>(gCommunications)->mFastSelectionLock;
            theCurrentID = &static_cast<EEServer*>(gCommunications)->mFastSelectionID;
            theLastIDs = static_cast<EEServer*>(gCommunications)->mFastSelection;
        } else {
            theLock = static_cast<EEClient*>(gCommunications)->mFastSelectionLock;
            theCurrentID = &static_cast<EEClient*>(gCommunications)->mFastSelectionID;
            theLastIDs = static_cast<EEClient*>(gCommunications)->mFastSelection;
        }
        theLock->Enter();
        uint8_t theMarks = DecideSelectionMarksLocal(theServer, theLocal);
        if (theMarks)
            gWorld.mExecutingLocalSelection = true;
        this->Execute();
        if (theMarks)
            gWorld.mExecutingLocalSelection = false;
        if (DecideSelectionCopies(theLocal, this->mFastSelectionID, *theCurrentID)) {
            thePlayer->mSelectedGroup->CopySelection(thePlayer->mActiveGroup);
            thePlayer->SetSelectedViewedUnit(thePlayer->mViewedUnit);
        }
        theLastIDs[(unsigned char)thePlayer->mPlayerID] = this->mFastSelectionID;
        theLock->Leave();
        if (thePlayer->mJITManufacturing) {
            thePlayer->mJITManufacturing = false;
            if (thePlayer == gWorld.GetCurrentPlayer())
                gUserInterface->UpdateJITManufacturing();
        }
    }
}

// FUNCTION: EXE 0x4b8e86
// Executes a command message if its selection id matches the sender's last selection; otherwise resets the sender's
// active group to the selection (Game Restarting always runs; on clients Migration Slot Info too: the Rust decision)
void EEMessage::ExecuteMessage()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer) {
        uint8_t theServer = gCommunications->IsServer() ? 1 : 0;
        TSCriticalSection* theLock;
        unsigned char* theLastIDs;
        if (theServer) {
            theLock = static_cast<EEServer*>(gCommunications)->mFastSelectionLock;
            theLastIDs = static_cast<EEServer*>(gCommunications)->mFastSelection;
        } else {
            theLock = static_cast<EEClient*>(gCommunications)->mFastSelectionLock;
            theLastIDs = static_cast<EEClient*>(gCommunications)->mFastSelection;
        }
        theLock->Enter();
        unsigned char theSelectionID = theLastIDs[(unsigned char)thePlayer->mPlayerID];
        this->GetMessageLabel();
        if (!DecideCommandRuns(theServer, this->mType, this->mFastSelectionID, theSelectionID)) {
            thePlayer->UnselectAllUnits();
            thePlayer->mSelectedGroup->CopySelection(thePlayer->mActiveGroup);
        } else
            this->Execute();
        theLock->Leave();
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
    EEMessageCaptureImport(theMessage, inMemory);   // TEST ONLY: the simulation capture's NMIM record (off by default)
    return theMessage;
     }

// FUNCTION: EXE 0x4badd0
// Toggles the listed units in the sender's selection (twice while a selection is being replayed)
void EEMAddAndRemoveUnitsFromSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    uint32_t thePasses = DecideSelectionPasses();
    for (uint32_t i = 0; i < thePasses; i++)
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
    // New (R5b.4): the body is Rust (ee_units group.rs add_patrol_point: a selected path point takes the point into its
    // route, a patrolling first unit adds it to its goal's route, else per formation slot 1..5 an active group of the
    // mobile non-citizens gets a new patrol with the point, planned, one unit drawn to acknowledge; nothing selected:
    // a lone patrol group) over the world calls of EEUnitHost.cpp
    ee_units_add_patrol_point(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), this->mPoint.GetX(),
                              this->mPoint.GetY());
}

// EEMAddPatrolPoint's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMAddPatrolPoint::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ADD_PATROL_POINT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPoint);
    return 6;
}

// FUNCTION: EXE 0x4bb0c2
// Returns the Add Patrol Point message's debug label
char* EEMAddPatrolPoint::GetMessageLabel() const
{
    return "Add Patrol Point";
}

// EEMAddRemoveResources's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMAddRemoveResources::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ADD_REMOVE_RESOURCES;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mAmount);
    outSlots[6] = EENetMsgField(this->mPlayerID);
    outSlots[7] = EENetMsgField(this->mResourceType);
    outSlots[8] = EENetMsgField(this->mAdd);
    return 9;
}

// FUNCTION: EXE 0x4bb264
// Gives a player an amount of one resource, or takes it if the amount is positive and no more than the stock
void EEMAddRemoveResources::Execute()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayerID];
    // the take's test compares unsigned, as on Windows (the decision); an add reads nothing
    uint8_t theDecision = DecideAddRemove(this->mAdd, this->mAmount, this->mAdd ? 0 : thePlayer->mResourceCaps[this->mResourceType]);
    if (theDecision == 1)
        thePlayer->AddResource(this->mResourceType, this->mAmount);
    else if (theDecision == 2)
        thePlayer->TakeResource(this->mResourceType, this->mAmount);
}

// FUNCTION: EXE 0x4bb2a2
// Toggles one unit in or out of the sender's selection (twice while a selection is being replayed)
void EEMAddUnitToSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    if (theUnit) {
        uint32_t thePasses = DecideSelectionPasses();
        for (uint32_t i = 0; i < thePasses; i++) {
            if (thePlayer->mActiveGroup->Contains(theUnit->GetComplexUnit()))
                thePlayer->RemoveUnitFromSelectedGroup(this->mUnitID);
            else
                thePlayer->AddUnitToSelectedGroup(this->mUnitID, true);
        }
    }
}

// EEMAddUnitToSelectedGroup's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMAddUnitToSelectedGroup::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ADD_UNIT_TO_SELECTED_GROUP;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    return 6;
}

// FUNCTION: EXE 0x4bb3f5
// Returns the Add Unit To Selected Group message's debug label
char* EEMAddUnitToSelectedGroup::GetMessageLabel() const
{
    return "Add Unit To Selected Group";
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
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs attack_ground: per formation slot 1..5 an active group filled
    // from the selection keeps its ground attackers, one drawn to acknowledge, an Attack Ground goal at the point on
    // the map's height, planned, the attack marker) over the world calls of EEUnitHost.cpp
    ee_units_attack_ground(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), this->mX, this->mY);
}

// EEMAttackGround's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMAttackGround::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ATTACK_GROUND;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mX);
    outSlots[6] = EENetMsgField(this->mY);
    return 7;
}

// FUNCTION: EXE 0x4bb75e
// Runs the attack-players order (EEAttackPlayers) for the message's player and allies flag
void EEMAttackPlayers::Execute()
{
    EEAttackPlayers(this->mPlayerID, this->mAllies);
}

// EEMAttackPlayers's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMAttackPlayers::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ATTACK_PLAYERS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgField(this->mAllies);
    return 7;
}

// FUNCTION: EXE 0x4bb869
// Orders the sender's selection, split by unit category and spread around the target, to attack (or convert)
// a living unit, queued or not; sends each group's command and plays an acknowledgement
void EEMAttackUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs attack_unit: with a selection and a living target, per formation
    // slot 1..5 an active group of the selection takes the queued flag, spreads around the target, one unit drawn, an
    // Attack Unit goal, planned, the attack marker, the drawn unit acknowledging) over the world calls of EEUnitHost.cpp
    ee_units_attack_unit(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mTargetID,
                         this->mIsQueued, this->mIsConvert);
}

// EEMAttackUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMAttackUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ATTACK_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTargetID);
    outSlots[6] = EENetMsgField(this->mIsQueued);
    outSlots[7] = EENetMsgField(this->mIsConvert);
    return 8;
}

// FUNCTION: EXE 0x4bbac3
// Returns the Attack Unit message's debug label
char* EEMAttackUnit::GetMessageLabel() const
{
    return "Attack Unit";
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

// EEMBuildGate's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMBuildGate::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_BUILD_GATE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPoint);
    outSlots[6] = EENetMsgField(this->mTechnology);
    outSlots[7] = EENetMsgField(this->mSide);
    outSlots[8] = EENetMsgField(this->mPlayerID);
    return 9;
}

// FUNCTION: EXE 0x4bbc34
// Returns the Build Gate message's debug label
char* EEMBuildGate::GetMessageLabel() const
{
    return "Build Gate";
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
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units commands.rs build_unit: with a selection, one active group of it keeps its
    // citizens; the technology's class is made where allowed, a granary's free farms sorted around the first unit;
    // each unit builds it, queued or at once, and takes a farm; one unit drawn acknowledges) over the world calls of
    // EEUnitHost.cpp; the tile is the point's (long) casts, as the original's
    ee_units_build_unit(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), this->mTechnology,
                        (int64_t)(long)this->mPoint.GetX(), (int64_t)(long)this->mPoint.GetY(), this->mIsQueued);
}

// EEMBuildUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMBuildUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_BUILD_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPoint);
    outSlots[6] = EENetMsgField(this->mTechnology);
    outSlots[7] = EENetMsgField(this->mIsQueued);
    return 8;
}

// FUNCTION: EXE 0x4bc0ae
// Returns the Build Unit message's debug label
char* EEMBuildUnit::GetMessageLabel() const
{
    return "Build Unit";
}

// FUNCTION: EXE 0x4bc12c
// Plans a wall from start to end for a player and orders the selected citizens to build its pieces
void EEMBuildWall::Execute()
{
    EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayerID];
    // New (R5b.4): the body is Rust (ee_units group.rs build_wall: with a selection, one active group of it keeps its
    // citizens; with some left the wall is planned as foundations and one unit drawn acknowledges; the group builds the
    // new pieces) over the world calls of EEUnitHost.cpp
    ee_units_build_wall(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), this->mTechnology, this->mStart.GetX(),
                        this->mStart.GetY(), this->mEnd.GetX(), this->mEnd.GetY());
}

// EEMBuildWall's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMBuildWall::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_BUILD_WALL;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mStart);
    outSlots[6] = EENetMsgField(this->mEnd);
    outSlots[7] = EENetMsgField(this->mTechnology);
    outSlots[8] = EENetMsgField(this->mPlayerID);
    return 9;
}

// FUNCTION: EXE 0x4bc321
// Returns the Build Wall message's debug label
char* EEMBuildWall::GetMessageLabel() const
{
    return "Build Wall";
}

void EEMCameraLocationInfo::Execute()
{
}

// EEMCameraLocationInfo's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMCameraLocationInfo::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CAMERA_LOCATION_INFO;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mLocation);
    return 6;
}

#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x4bc483
// Starts a calamity or power for the sender's selected casters: one able caster (or all for teleport and
// all-cast powers) gets the cast goal; calamity class 0x12 gives every selected caster a copy of the goal
void EEMCastCalamity::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units commands.rs cast_calamity: a living caster, the technology's calamity
    // class; class 0x12 gives every selected caster a copy of the cast goal, else the first able caster, or every
    // one for teleport and all-cast powers, gets its own) over the world calls of EEUnitHost.cpp (CanCastAt, the cost,
    // the casters' split: the calamity classes' C++)
    ee_units_cast_calamity(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mCasterID,
                           this->mTargetID, this->mTileX, this->mTileY, this->mTechnology);
}

// EEMCastCalamity's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMCastCalamity::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CAST_CALAMITY;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mCasterID);
    outSlots[6] = EENetMsgField(this->mTargetID);
    outSlots[7] = EENetMsgField(this->mTileX);
    outSlots[8] = EENetMsgField(this->mTileY);
    outSlots[9] = EENetMsgField(this->mTechnology);
    return 10;
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

// EEMChangeFormation's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMChangeFormation::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CHANGE_FORMATION;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mFormationType);
    outSlots[6] = EENetMsgField(this->mB14);
    return 7;
}

// FUNCTION: EXE 0x4bcb0c
// Returns the Change Formation message's debug label
char* EEMChangeFormation::GetMessageLabel() const
{
    return "ChangeFormation";
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
            // the other units of the class (collected for sea units) take the new radius too
            for (unsigned long j = 0; j < theUnits.size(); j++)
                static_cast<EEComplexUnit*>(theUnits[j])->mCollisionRadius = (float)theUnit->mUnitClass->mInnerCollisionSphere;
        }
    }
}

// EEMChangeSelectedUnitCollisionSphere's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMChangeSelectedUnitCollisionSphere::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CHANGE_SELECTED_UNIT_COLLISION_SPHERE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGrow);
    return 6;
}

// FUNCTION: EXE 0x4bcd0d
// Returns the Change Selected Unit Collision Sphere message's debug label
char* EEMChangeSelectedUnitCollisionSphere::GetMessageLabel() const
{
    return "ChangeSelectedUnitCollisionSphere";
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

// EEMChat's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMChat::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CHAT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgString(this->mText);
    return 7;
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
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_CHAT, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mText.Import(inPayloadPointer + theSlots[6].offset);
}

// FUNCTION: EXE 0x4bced6
// Applies a cheat code with its parameter for the sending player
void EEMCheatCode::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    gCheatCodes->ApplyCheatCode(this->mCode, thePlayer->mPlayerID, this->mParameter);
}

// EEMCheatCode's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMCheatCode::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CHEAT_CODE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mCode);
    outSlots[6] = EENetMsgField(this->mParameter);
    return 7;
}

// FUNCTION: EXE 0x4bcf9b
// Returns the Cheat Code message's debug label
char* EEMCheatCode::GetMessageLabel() const
{
    return "Cheat Code";
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

// EEMCivTimerExpired's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMCivTimerExpired::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CIV_TIMER_EXPIRED;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mIsWarning);
    return 6;
}

// FUNCTION: EXE 0x4bd1a0
// Returns the Civ Timer Expired message's debug label
char* EEMCivTimerExpired::GetMessageLabel() const
{
    return "Civ Timer Expired";
}

// EEMCivUpgrades' members as the codec's slots
size_t EEMCivUpgrades::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CIV_UPGRADES;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgField(this->mNameString);
    outSlots[7] = EENetMsgRecords(this->mUpgrades.empty() ? NULL : &this->mUpgrades[0], sizeof(EECivUpgrade),
                                  this->mUpgrades.size());
    outSlots[8] = EENetMsgString(this->mCivName);
    return 9;
}

// FUNCTION: EXE 0x4bd327
// Reads the civilization upgrades message (player, name string, upgrades, civ name) from a network buffer
void EEMCivUpgrades::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 4392);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_CIV_UPGRADES, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    for (unsigned long i = 0; i < theSlots[7].count; i++) {
        EECivUpgrade theUpgrade;
        memcpy(&theUpgrade, inPayloadPointer + theSlots[7].offset + i * sizeof(theUpgrade), sizeof(theUpgrade));
        this->mUpgrades.push_back(theUpgrade);
    }
    this->mCivName.Import(inPayloadPointer + theSlots[8].offset);
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

// EEMClearAirportCommands's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMClearAirportCommands::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_CLEAR_AIRPORT_COMMANDS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4bd53f
// Returns the Clear Airport Commands message's debug label
char* EEMClearAirportCommands::GetMessageLabel() const
{
    return "Clear Airport Commands";
}

// FUNCTION: EXE 0x4bd588
// Clears the queued orders of the sender's selected airports, carriers/space docks and navy yards (reopening
// them and emptying their air traffic queue) and the orders of selected aircraft
void EEMClearAirportCommands::Execute()
{
    // New (game messages lane, round 1): the body is Rust (ee_units rally.rs clear_airport_commands: each selected
    // airport's, air base's and navy yard's queues cleared and opened, an aircraft's queue 1, the goals refreshed)
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    ee_units_clear_airport_commands(static_cast<EEGroup*>(thePlayer->mActiveGroup));
}

// EEMDisband's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMDisband::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_DISBAND;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
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
            float theFraction;
            if (DecideDisbandRefund(theUnit->mHitPoints, theUnit->mHitPoints > 0 ? theUnit->mStats->mMaxHitPoints : 1,
                                    theUnit->mHitPoints > 0 && theUnit->mUnitClass->mCanDisband, &theFraction)) {
                thePlayer->RefundTechnology(thePlayer->mTechnologies->Get(theUnit->mUnitClass->mTechnology), theUnit->mUnitClass->mUnitType,
                                     theFraction);
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
    // New (R5b.4): the body is Rust (ee_units group.rs dropoff_resource: with a selection and a complex drop-off, one
    // active group of the selection spreads around the drop-off's tile, gets a Gather goal with no resource and the
    // drop-off, the action Dropoff Resource, plans, one unit drawn to acknowledge) over the world calls of EEUnitHost.cpp
    ee_units_dropoff_resource(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mDropoffID);
}

// EEMDropoffResource's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMDropoffResource::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_DROPOFF_RESOURCE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mDropoffID);
    return 6;
}

// FUNCTION: EXE 0x4bdaec
// Returns the Dropoff Resource message's debug label
char* EEMDropoffResource::GetMessageLabel() const
{
    return "Dropoff Resource";
}

void EEMEdictPreProposal::Execute()
{
}

// EEMEdictPreProposal's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMEdictPreProposal::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EDICT_PRE_PROPOSAL;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mEdict);
    return 6;
}

void EEMEdictPreProposalResponse::Execute()
{
}

// EEMEdictPreProposalResponse's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMEdictPreProposalResponse::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EDICT_PRE_PROPOSAL_RESPONSE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mEdict);
    outSlots[6] = EENetMsgField(this->mW12);
    return 7;
}

// EEMEdictProposal's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMEdictProposal::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EDICT_PROPOSAL;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mEdict);
    return 6;
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

// EEMEdictVote's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMEdictVote::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EDICT_VOTE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mEdict);
    outSlots[6] = EENetMsgField(this->mIsYes);
    outSlots[7] = EENetMsgField(this->mCount);
    return 8;
}

// FUNCTION: EXE 0x4bdf7d
// Ends the scenario's running cinematic
void EEMEndCinematic::Execute()
{
    UWideString theText;
    gWorld.mScenarioManager->EndCinematic();
}

// EEMEndCinematic's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMEndCinematic::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_END_CINEMATIC;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mF10);
    return 6;
}

// FUNCTION: EXE 0x4be020
// Returns the End Cinematic message's debug label
char* EEMEndCinematic::GetMessageLabel() const
{
    return "End Cinematic";
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

// EEMEnterShiftMode's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMEnterShiftMode::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ENTER_SHIFT_MODE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGameTime);
    return 6;
}

// FUNCTION: EXE 0x4be12f
// Returns the Enter Shift Mode message's debug label
char* EEMEnterShiftMode::GetMessageLabel() const
{
    return "Enter Shift Mode";
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

// EEMExitShiftMode's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMExitShiftMode::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EXIT_SHIFT_MODE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGameTime);
    return 6;
}

// FUNCTION: EXE 0x4be23e
// Returns the Exit Shift Mode message's debug label
char* EEMExitShiftMode::GetMessageLabel() const
{
    return "Exit Shift Mode";
}

// EEMExchangeResources's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMExchangeResources::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EXCHANGE_RESOURCES;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mIsSell);
    outSlots[6] = EENetMsgField(this->mResourceType);
    outSlots[7] = EENetMsgField(this->mFiveTimes);
    return 8;
}

// FUNCTION: EXE 0x4be334
// Returns the Exchange Resources message's debug label
char* EEMExchangeResources::GetMessageLabel() const
{
    return "Exchange Resources";
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
        long thePrice = EEGetTradePrice(thePlayer, this->mResourceType, this->mIsSell);
        long theHave = thePlayer->mResourceCaps[this->mIsSell ? this->mResourceType : kResourceGold];
        EeNetMsgExchangeStep theStep = DecideExchangeStep(this->mIsSell, thePrice, theHave);
        if (theStep.apply) {
            if (this->mIsSell) {
                thePlayer->TakeResource(this->mResourceType, (long)theStep.cost);
                thePlayer->AddResource(kResourceGold, (long)theStep.amount);
                thePlayer->mResourceTrade[this->mResourceType - 2] -= (long)theStep.cost;
            } else {
                thePlayer->AddResource(this->mResourceType, (long)theStep.cost);
                thePlayer->TakeResource(kResourceGold, (long)theStep.amount);
                thePlayer->mResourceTrade[this->mResourceType - 2] += (long)theStep.cost;
            }
        }
        theCount--;
    }
    if (gWorld.mPlayers[gWorld.mCurrentPlayer]->mPlayerID == thePlayer->mPlayerID)
        gUIManager->mMainGameForm->UpdateHelpString();
}

// EEMExplore's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMExplore::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_EXPLORE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4be5b6
// Sends the sender's mobile selected units, split by unit category, to explore unexplored areas
void EEMExplore::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs explore: per formation slot 1..5 an active group filled from
    // the selection, given an Explore goal and planned, one unit drawn to acknowledge) over the world calls of
    // EEUnitHost.cpp
    ee_units_explore(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup));
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

// EEMFullGameOptions's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMFullGameOptions::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_FULL_GAME_OPTIONS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mAllowCustomCivs);
    outSlots[6] = EENetMsgField(this->mB53);
    outSlots[7] = EENetMsgField(this->mCheatCodes);
    outSlots[8] = EENetMsgField(this->mDifficulty);
    outSlots[9] = EENetMsgField(this->mEpochEnd);
    outSlots[10] = EENetMsgField(this->mGameSpeed);
    outSlots[11] = EENetMsgField(this->mLoadType);
    outSlots[12] = EENetMsgField(this->mVariant);
    outSlots[13] = EENetMsgField(this->mLockSpeed);
    outSlots[14] = EENetMsgField(this->mLockTeams);
    outSlots[15] = EENetMsgField(this->mMapSize);
    outSlots[16] = EENetMsgField(this->mRecordGames);
    outSlots[17] = EENetMsgField(this->mResources);
    outSlots[18] = EENetMsgField(this->mRevealMap);
    outSlots[19] = EENetMsgField(this->mEpochStart);
    outSlots[20] = EENetMsgField(this->mUnitLimit);
    outSlots[21] = EENetMsgField(this->mVictory);
    outSlots[22] = EENetMsgField(this->mWondersForVictory);
    outSlots[23] = EENetMsgString(this->mMapType);
    return 24;
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
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_FULL_GAME_OPTIONS, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mMapType.Import(inPayloadPointer + theSlots[23].offset);
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

// EEMGameOptions's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMGameOptions::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_GAME_OPTIONS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mOption);
    outSlots[6] = EENetMsgField(this->mValue);
    outSlots[7] = EENetMsgString(this->mMapType);
    return 8;
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
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_GAME_OPTIONS, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mMapType.Import(inPayloadPointer + theSlots[7].offset);
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

// EEMGameRestarting's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMGameRestarting::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_GAME_RESTARTING;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4bef70
// Returns the Game Restarting message's debug label
char* EEMGameRestarting::GetMessageLabel() const
{
    return "Game Restarting";
}

// FUNCTION: EXE 0x4befb9
// Sets the world's game speed to the requested speed
void EEMGameSpeed::Execute()
{
    gWorld.SetGameSpeed(this->mSpeed);
}

// EEMGameSpeed's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMGameSpeed::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_GAME_SPEED;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSpeed);
    return 6;
}

// FUNCTION: EXE 0x4bf04d
// Returns the message's debug label "Game Speed"
char* EEMGameSpeed::GetMessageLabel() const
{
    return "Game Speed";
}

// FUNCTION: EXE 0x4bf0a3
// Sends the sender's selected units to gather from the target resource (if it has any left), spread around it,
// optionally queued; acknowledges with a random unit's voice
void EEMGatherResource::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs gather: with a selection and a resource with something left, one
    // active group of the selection takes the queued flag, spreads around the resource, gets a Gather goal, plans, the
    // target marker, one unit drawn to acknowledge) over the world calls of EEUnitHost.cpp
    ee_units_gather(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mTargetID, this->mIsQueued);
}

// EEMGatherResource's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMGatherResource::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_GATHER_RESOURCE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTargetID);
    outSlots[6] = EENetMsgField(this->mIsQueued);
    return 7;
}

// FUNCTION: EXE 0x4bf299
// Returns the message's debug label "Gather Resource"
char* EEMGatherResource::GetMessageLabel() const
{
    return "Gather Resource";
}

// EEMGoFanatic's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMGoFanatic::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_GO_FANATIC;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4bf371
// Returns the message's debug label "Go Fanatic"
char* EEMGoFanatic::GetMessageLabel() const
{
    return "Go Fanatic";
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

// EEMHackANewUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMHackANewUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_HACK_A_NEW_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitType);
    outSlots[6] = EENetMsgField(this->mPoint);
    outSlots[7] = EENetMsgField(this->mPlayerID);
    return 8;
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
    if ((unsigned long)this->mUnitType == kNoUnitClass)
        this->mUnitType = (int32_t)theSender->mHackUnitClass;
    if ((unsigned long)this->mUnitType >= EEUnitClassCount())
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

// EEMHaltSelectedGroup's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMHaltSelectedGroup::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_HALT_SELECTED_GROUP;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4bf996
// Returns the message's debug label "Halt Selected Group"
char* EEMHaltSelectedGroup::GetMessageLabel() const
{
    return "Halt Selected Group";
}

// FUNCTION: EXE 0x4bf9df
// Orders the sender's selected healers, one group per formation slot, to heal the target unit; acknowledges by voice
void EEMHealUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs heal_unit: with a target, per formation slot 1..5 an active group
    // of the selection gets a Heal Unit goal on the target's complex unit, planned, one unit drawn to acknowledge) over
    // the world calls of EEUnitHost.cpp
    ee_units_heal_unit(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mTargetID);
}

// EEMHealUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMHealUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_HEAL_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTargetID);
    return 6;
}

// FUNCTION: EXE 0x4bfb89
// Returns the message's debug label "Heal Unit"
char* EEMHealUnit::GetMessageLabel() const
{
    return "Heal Unit";
}

// EEMImmediateCargoUnload's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMImmediateCargoUnload::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_IMMEDIATE_CARGO_UNLOAD;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTransportID);
    outSlots[6] = EENetMsgField(this->mCargoID);
    outSlots[7] = EENetMsgField(this->mB18);
    return 8;
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
            theText = EEUnitClassCast<EEUCTransport>(theTransport->GetUnitClass())->GetUnloadFailedTextID();
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
    if (theSender)   // port: the game log (ee/EEGameLog.h), every in-game chat line (notes typed while playing)
        EEGameLogChat(theSender->mPlayerID, (const char*)this->mText);
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
                gUserInterface->AddChatLogEntry(theSender->mColor, theSender->mName, theType + this->mText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);

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

// EEMInGameChat's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMInGameChat::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_IN_GAME_CHAT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgString(this->mText);
    return 6;
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
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_IN_GAME_CHAT, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mText.Import(inPayloadPointer + theSlots[5].offset);
}

// EEMIncrementEpic's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMIncrementEpic::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_INCREMENT_EPIC;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
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

// EEMIncrementRallyTimer's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMIncrementRallyTimer::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_INCREMENT_RALLY_TIMER;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGoalQueue);
    return 6;
}

// FUNCTION: EXE 0x4c0265
// Returns the message's debug label "Increment Rally Timer"
char* EEMIncrementRallyTimer::GetMessageLabel() const
{
    return "Increment Rally Timer";
}

// EEMJITManufacturing's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMJITManufacturing::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_J_I_T_MANUFACTURING;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4c0326
// Returns the message's debug label "JIT Manufacturing"
char* EEMJITManufacturing::GetMessageLabel() const
{
    return "JIT Manufacturing";
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

// EEMKillUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMKillUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_KILL_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mAllUnits);
    return 6;
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
            EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(theGroup->GetFirstUnit());
            if (theUnit && (theUnit->mUnitClass->mUnitType == kUnitTypePathPoint || theUnit->mHitPoints > 0) && theUnit->CanDelete()) {
                if (IsRefundable(theUnit)) {
                    EETechnology* theTechnology = theUnit->GetPlayer()->GetTechTree()->Get(theUnit->GetUnitClass()->GetTechnology());
                    if (theTechnology)
                        theUnit->mPlayer->RefundTechnology(theTechnology, 0x20, gWorldTable.GetFloatValue(kWorldDeleteRefundFactor));
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
                        EETechnology* theTechnology = theUnit->GetPlayer()->GetTechTree()->Get(theUnit->GetUnitClass()->GetTechnology());
                        if (theTechnology)
                            theUnit->mPlayer->RefundTechnology(theTechnology, 0x20, gWorldTable.GetFloatValue(kWorldDeleteRefundFactor));
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

// EEMLagStatus's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLagStatus::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LAG_STATUS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mStatus);
    outSlots[6] = EENetMsgField(this->mLagWarning);
    return 7;
}

// FUNCTION: EXE 0x4c075b
// Returns the message's debug label "Lag Status"
char* EEMLagStatus::GetMessageLabel() const
{
    return "Lag Status";
}

// EEMLoadMainScreen's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadMainScreen::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_MAIN_SCREEN;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mB10);
    return 6;
}

// inline (header-declared; the COMDAT 0x4c0894 sits here): inPoint on the terrain, its own z off
// the mesh
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
void EEMap::GetGroundPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint)
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
            PlatSleep(1);
        }
    } else {
        gGraphics->PostPause();
        for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
            PlatSleep(1);
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

// EEMLoadMainScreenResponse's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadMainScreenResponse::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_MAIN_SCREEN_RESPONSE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSlot);
    outSlots[6] = EENetMsgField(this->mPlayerID);
    return 7;
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

// EEMLoadOrSaveGame's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadOrSaveGame::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_OR_SAVE_GAME;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mIsLoad);
    outSlots[6] = EENetMsgField(this->mIsQuiet);
    outSlots[7] = EENetMsgField(this->mTick);
    outSlots[8] = EENetMsgField(this->mTime);
    outSlots[9] = EENetMsgField(this->mIsAutoSave);
    outSlots[10] = EENetMsgField(this->mWaitMessageID);
    outSlots[11] = EENetMsgString(this->mFileName);
    return 12;
}

// FUNCTION: EXE 0x4c0dae
// Reads the load/save request (header, load, quiet, tick, time, auto-save, wait id, file name) from a received payload
void EEMLoadOrSaveGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 8602);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_LOAD_OR_SAVE_GAME, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mFileName.Import(inPayloadPointer + theSlots[11].offset);
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

extern long& gAutoSaveCount;                 // 0x5308b8 (the auto-save count LoadOrSave passes on; provisional name)
// (0x14cbc7 TrimAutoSaves: ee/EESaveLoad.h)

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
        TrimAutoSaves(theName, theDirectory, gAutoSaveCount);
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

// EEMLoadOrSaveGameResponse's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadOrSaveGameResponse::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_OR_SAVE_GAME_RESPONSE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTick);
    return 6;
}

// EEMLoadPercent's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadPercent::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_PERCENT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSlot);
    outSlots[6] = EENetMsgField(this->mPercent);
    return 7;
}

// FUNCTION: EXE 0x4c142a
// Shows a slot's loading progress in the interface, capped at 100 percent
void EEMLoadPercent::Execute()
{
    if (this->mPercent > 100)
        this->mPercent = 100;
    gUserInterface->SetLoadPercent(this->mSlot, this->mPercent);
}

// EEMLoadUnitData's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadUnitData::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_UNIT_DATA;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitType);
    outSlots[6] = EENetMsgField(this->mB14);
    return 7;
}

// FUNCTION: EXE 0x4c153d
// Changes the sender's cheat unit class: unloads the old class's data for every player and preloads the new one
void EEMLoadUnitData::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer->mHackUnitClass != this->mUnitType) {
        unsigned long i;
        for (i = 1; i < gWorld.mNumPlayers; i++)
            if (thePlayer->mHackUnitClass < EEUnitClassCount() && gUnitClassTable[thePlayer->mHackUnitClass])
                gDataPreLoading->PostUnloadUnitClass(gWorld.mPlayers[i]->mPlayerID, thePlayer->mHackUnitClass);
        thePlayer->SetHackUnitClass(this->mUnitType);
        for (i = 1; i < gWorld.mNumPlayers; i++)
            if (thePlayer->mHackUnitClass < EEUnitClassCount() && gUnitClassTable[thePlayer->mHackUnitClass])
                gDataPreLoading->PostLoadUnitClass(gWorld.mPlayers[i]->mPlayerID, thePlayer->mHackUnitClass);
    }
}

// EEMLoadGarrison's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadGarrison::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_GARRISON;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGarrisonID);
    outSlots[6] = EENetMsgField(this->mUnitID);
    outSlots[7] = EENetMsgField(this->mB18);
    return 8;
}

#include "ee/EEUGLocation.h"
#include "ee/EEUGRust.h"

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
        EEUGRust* theGoal;
        if (theOldGoal != NULL && theOldGoal->mKind == kGoalLoadGarrison) {
            theGoal = EELoadGarrisonGoalCast(theOldGoal);
            theGoal->AddLoadUnits(theUnits);
        } else {
            theGoal = EEUGRust::NewLoad(kGoalLoadGarrison, theGarrison, theUnits, false);
            if (theGoal->IsLoadGarrisonValid()) {
                theGarrison->SetGoal(theGoal, 0, 0);
                if (theGarrison->mStats->mMaxVelocity > 0.0f) theGarrison->StartAction(kActionPrepareToMove); else theGarrison->StartAction(kActionPickedUpLoadableUnits);
                theGarrison->mUnitClass->ActionChanged(theGarrison);
            }
        }
        if (theGoal->IsLoadGarrisonValid()) {
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

// EEMLoadTransport's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLoadTransport::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOAD_TRANSPORT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTransportID);
    outSlots[6] = EENetMsgField(this->mPassengerID);
    outSlots[7] = EENetMsgField(this->mB18);
    return 8;
}

#include "ee/EEUGLocation.h"
#include "ee/EEUGRust.h"

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
    } else {
        // the passenger may be gone by the time the order runs (it used to be read through NULL)
        EEUnit* thePassenger = gWorld.GetUnit(this->mPassengerID);
        EEComplexUnit* theUnit = thePassenger != NULL ? thePassenger->mComplexUnit : NULL;
        if (theUnit != NULL && theUnit->mHitPoints > 0)
            theUnits.push_back(theUnit);
    }
    EEUnitClassCast<EEUCTransport>(theTransport->mUnitClass)->FilterLoadable(&theUnits, theTransport, &theRejected);
    long theTileX;
    long theTileY;
    if (theUnits.size() != 0) {
        EEUGRust* theGoal;
        if (theOldGoal != NULL && theOldGoal->mKind == kGoalLoadTransport) {
            theGoal = EELoadTransportGoalCast(theOldGoal);
            theGoal->AddLoadUnits(theUnits);
        } else {
            theGoal = EEUGRust::NewLoad(kGoalLoadTransport, theTransport, theUnits, true);
            theTransport->SetGoal(theGoal, 0, 0);
            theTransport->StartAction(theTransport->mStats->mMaxVelocity > 0.0f ? 8000 : 0x2ee3);
            theTransport->mUnitClass->ActionChanged(theTransport);
        }
        theTileX = theGoal->GetLoadUnitTile().GetX();
        theTileY = theGoal->GetLoadUnitTile().GetY();
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

// EEMLockUnlock's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMLockUnlock::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_LOCK_UNLOCK;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mIsLocked);
    return 6;
}

// FUNCTION: EXE 0x4c1f60
// Returns the message's debug label "Lock Unlock"
char* EEMLockUnlock::GetMessageLabel() const
{
    return "Lock Unlock";
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
            EEUCGate* theGate = EEUnitClassCast<EEUCGate>(theUnit->GetUnitClass());
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

// EEMMakeWorldVisible's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMMakeWorldVisible::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_MAKE_WORLD_VISIBLE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
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

// EEMMigrationSlotInfo's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMMigrationSlotInfo::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_MIGRATION_SLOT_INFO;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgField(this->mSlot);
    return 7;
}

void EEMNewEdictEnacted::Execute()
{
}

// EEMNewEdictEnacted's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMNewEdictEnacted::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_NEW_EDICT_ENACTED;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mEdict);
    outSlots[6] = EENetMsgField(this->mYesVotes);
    outSlots[7] = EENetMsgField(this->mNoVotes);
    outSlots[8] = EENetMsgField(this->mIsEnacted);
    return 9;
}

// EEMPackUnpack's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMPackUnpack::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_PACK_UNPACK;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPack);
    return 6;
}

// FUNCTION: EXE 0x4c2547
// Returns the message's debug label "Pack Unpack"
char* EEMPackUnpack::GetMessageLabel() const
{
    return "Pack Unpack";
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
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs path_to: with a selection, the point adjusted to the map and
    // around a building the first unit cannot pass (the message keeps it), per formation slot 1..5 an active group of
    // the mobile units gets a Location goal, its paths reset, planned, one unit drawn to acknowledge, the move marker)
    // over the world calls of EEUnitHost.cpp
    ee_units_path_to(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), &this->mX, &this->mY, this->mFacing,
                     this->mIsForcedMove, this->mIsRotating, this->mIsQueued, this->mUseGroupMovement);
}

// EEMPathTo's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMPathTo::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_PATH_TO;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mFacing);
    outSlots[6] = EENetMsgField(this->mX);
    outSlots[7] = EENetMsgField(this->mY);
    outSlots[8] = EENetMsgField(this->mIsForcedMove);
    outSlots[9] = EENetMsgField(this->mIsRotating);
    outSlots[10] = EENetMsgField(this->mIsQueued);
    outSlots[11] = EENetMsgField(this->mUseGroupMovement);
    return 12;
}

// FUNCTION: EXE 0x4c2c30
// Returns the message's debug label "Path To"
char* EEMPathTo::GetMessageLabel() const
{
    return "Path To";
}

// FUNCTION: EXE 0x4c2cf2
// Patrol order: a selected path point gets the target's tile as a new point; other units patrol the target's
// route (target id -1: the nearest town's patrol route); acknowledges by voice
void EEMPatrol::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs patrol: the target by id or the nearest town's route point; a
    // selected path point takes the target's tile, else per formation slot 1..5 an active group of the mobile units
    // patrols the target's route, planned, the target marker, one unit drawn to acknowledge) over the world calls of
    // EEUnitHost.cpp
    ee_units_patrol(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), this->mTargetID);
}

// EEMPatrol's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMPatrol::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_PATROL;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTargetID);
    return 6;
}

// FUNCTION: EXE 0x4c2f73
// Returns the message's debug label "Patrol"
char* EEMPatrol::GetMessageLabel() const
{
    return "Patrol";
}

// EEMPayTribute's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMPayTribute::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_PAY_TRIBUTE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mFood);
    outSlots[6] = EENetMsgField(this->mGold);
    outSlots[7] = EENetMsgField(this->mIron);
    outSlots[8] = EENetMsgField(this->mStone);
    outSlots[9] = EENetMsgField(this->mWood);
    outSlots[10] = EENetMsgField(this->mReceiver);
    return 11;
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
    uint32_t theAmounts[7] = {0, 0, this->mFood, this->mWood, this->mStone, this->mGold, this->mIron};
    uint8_t theOrder[5];
    uint32_t theCount = DecideTributeOrder(theAmounts, 0, theOrder);
    for (uint32_t i = 0; i < theCount; i++) {
        theAmount = theAmounts[theOrder[i]];
        theFrom->AddTribute(0, theOrder[i], theAmount);
        theTo->AddResource(theOrder[i], theAmount);
        theTo->AddTribute(1, theOrder[i], theAmount);
    }
    gResource->GetString(IDS_RESOURCE_NAME_STONE, theFood);
    gResource->GetString(IDS_RESOURCE_NAME_GOLD, theWood);
    if (gWorld.mUsingCarbon)
        gResource->GetString(IDS_CARBON, theStone);
    else
        gResource->GetString(IDS_RESOURCE_NAME_WOOD, theStone);
    gResource->GetString(IDS_RESOURCE_NAME_FOOD, theGold);
    gResource->GetString(IDS_RESOURCE_NAME_IRON, theIron);
    uint8_t theViewer = DecideTributeViewer(gWorld.GetCurrentPlayer() == theFrom, gWorld.GetCurrentPlayer() == theTo);
    if (theViewer == 1) {
        gResource->GetString(IDS_YOU_TRIBUTE, theYouGave);
        sprintf(theText, "%s %s ", (const char*)theYouGave, (const char*)theTo->mName);
    } else if (theViewer == 2) {
        gResource->GetString(IDS_TRIBUTES_YOU, theYouGave);
        sprintf(theText, "%s %s ", (const char*)theFrom->mName, (const char*)theYouGave);
    } else
        return;
    // the chat line's names (the original's locals hold them crosswise: theGold is food's, theFood stone's, ...)
    const char* theNames[7] = {NULL, NULL, (const char*)theGold, (const char*)theStone, (const char*)theFood,
                               (const char*)theWood, (const char*)theIron};
    theCount = DecideTributeOrder(theAmounts, 1, theOrder);
    for (uint32_t i = 0; i < theCount; i++) {
        PlatIToA(theAmounts[theOrder[i]], theNumber, 10);
        theText += theNumber;
        theText += theSpace;
        theText += theNames[theOrder[i]];
        theText += theSpace;
    }
    gUserInterface->AddChatLogEntry(theFrom->mColor, theFrom->mName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
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
    case kPlayerInfoEngine:   // port: a computer slot's engine (ee/EECPEngine.h)
        thePlayer.mEngine = this->mValue < EECPEngineChoiceCount() ? this->mValue : (unsigned long)kEECPChoiceClassic;
        break;
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

// EEMPlayerInformation's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMPlayerInformation::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_PLAYER_INFORMATION;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mValue);
    outSlots[6] = EENetMsgField(this->mField);
    outSlots[7] = EENetMsgField(this->mPing);
    outSlots[8] = EENetMsgField(this->mSlot);
    return 9;
}

// FUNCTION: EXE 0x4c3700
// Returns the message's debug label "Player Information"
char* EEMPlayerInformation::GetMessageLabel() const
{
    return "Player Information";
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
    this->mEngine = (int32_t)inPlayer.mEngine;   // port
}

void EEMFullPlayerInformation::Execute()
{
}

// EEMFullPlayerInformation's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMFullPlayerInformation::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_FULL_PLAYER_INFORMATION;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSlot);
    outSlots[6] = EENetMsgField(this->mSlotType);
    outSlots[7] = EENetMsgField(this->mLoadedType);
    outSlots[8] = EENetMsgString(this->mName);
    outSlots[9] = EENetMsgField(this->mPing);
    outSlots[10] = EENetMsgField(this->mColor);
    outSlots[11] = EENetMsgField(this->mTeam);
    outSlots[12] = EENetMsgField(this->mCitizenBonus);
    outSlots[13] = EENetMsgField(this->mIsReady);
    outSlots[14] = EENetMsgField(this->mIsLocal);
    outSlots[15] = EENetMsgField(this->mB42);
    outSlots[16] = EENetMsgField(this->mEngine);
    return 17;
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
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_FULL_PLAYER_INFORMATION, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mName.Import(inPayloadPointer + theSlots[8].offset);
}

void EEMPlayerSlotInfo::Execute()
{
}

// EEMPlayerSlotInfo's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMPlayerSlotInfo::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_PLAYER_SLOT_INFO;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgField(this->mSlot);
    outSlots[7] = EENetMsgField(this->mIsBehindFirewall);
    outSlots[8] = EENetMsgString(this->mAddress);
    return 9;
}

// FUNCTION: EXE 0x4c3b39
// Reads the player slot info (header, selection id, player id, slot, firewall flag, address) from a received payload
void EEMPlayerSlotInfo::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 11370);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_PLAYER_SLOT_INFO, inPayloadPointer, theSlots, this->CodecSlots(theSlots));
    this->mAddress.Import(inPayloadPointer + theSlots[8].offset);
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

// EEMRecordFrames's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMRecordFrames::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_RECORD_FRAMES;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSlowPlayback);
    return 6;
}

// FUNCTION: EXE 0x4c3d36
// Removes the unit from the sending player's selection
void EEMRemoveUnitFromSelectedGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->RemoveUnitFromSelectedGroup(this->mUnitID);
}

// EEMRemoveUnitFromSelectedGroup's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMRemoveUnitFromSelectedGroup::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_REMOVE_UNIT_FROM_SELECTED_GROUP;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    return 6;
}

// FUNCTION: EXE 0x4c3ddb
// Returns the message's debug label "Remove Unit From Selected Group"
char* EEMRemoveUnitFromSelectedGroup::GetMessageLabel() const
{
    return "Remove Unit From Selected Group";
}

// FUNCTION: EXE 0x4c3e31
// Orders the sender's selected units, spread around the target, to repair it (queued: adds it as a build target);
// acknowledges by voice
void EEMRepairUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs repair: one active group of the selection spreads around the
    // target's tile; queued, its units add the target as a build target, else each orders the repair and all are
    // added back; one unit drawn to acknowledge) over the world calls of EEUnitHost.cpp
    ee_units_repair(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mTargetID, this->mIsQueued);
}

// EEMRepairUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMRepairUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_REPAIR_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTargetID);
    outSlots[6] = EENetMsgField(this->mIsQueued);
    return 7;
}

// FUNCTION: EXE 0x4c4099
// Returns the message's debug label "Repair Unit"
char* EEMRepairUnit::GetMessageLabel() const
{
    return "Repair Unit";
}

// EEMReplantFarms's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMReplantFarms::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_REPLANT_FARMS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mFarmID);
    return 6;
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

// EEMReportStatistics's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMReportStatistics::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_REPORT_STATISTICS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mL10);
    outSlots[6] = EENetMsgField(this->mL14);
    return 7;
}

// FUNCTION: EXE 0x4c439a
// Returns the message's debug label "Report Statistics"
char* EEMReportStatistics::GetMessageLabel() const
{
    return "Report Statistics";
}

// EEMResign's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMResign::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_RESIGN;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
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
            gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
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

// EEMRemoveImprovement's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMRemoveImprovement::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_REMOVE_IMPROVEMENT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    outSlots[6] = EENetMsgField(this->mImprovement);
    return 7;
}

// FUNCTION: EXE 0x4c46f9
// Returns the message's debug label "Remove Improvement"
char* EEMRemoveImprovement::GetMessageLabel() const
{
    return "Remove Improvement";
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

// EEMRemoveTechnology's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMRemoveTechnology::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_REMOVE_TECHNOLOGY;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    outSlots[6] = EENetMsgField(this->mTechnology);
    outSlots[7] = EENetMsgField(this->mToggleJITManufacturing);
    outSlots[8] = EENetMsgField(this->mFiveTimes);
    return 9;
}

// FUNCTION: EXE 0x4c4929
// Returns the message's debug label "Remove Technology"
char* EEMRemoveTechnology::GetMessageLabel() const
{
    return "Remove Technology";
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
                        unsigned long theQueue = theOther->mComplexUnit->GetBuildQueueSize() + (theOther->mComplexUnit->mCurrentResearch != 100000);   // queue + the one in progress (precedence)
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
                        theOther->mComplexUnit->QueueResearch(this->mTechnology, (EEIsEpochID(this->mTechnology)) || thePower2);
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
                theBest->mComplexUnit->QueueResearch(this->mTechnology, (EEIsEpochID(this->mTechnology)) || thePower2);
            }
        }
    }
}

// EEMResearchTechnology's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMResearchTechnology::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_RESEARCH_TECHNOLOGY;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mBuildingID);
    outSlots[6] = EENetMsgField(this->mTechnology);
    outSlots[7] = EENetMsgField(this->mFiveTimes);
    return 8;
}

// FUNCTION: EXE 0x4c4d84
// Returns the message's debug label "Research Technology"
char* EEMResearchTechnology::GetMessageLabel() const
{
    return "Research Technology";
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

// EEMReturnToBase's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMReturnToBase::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_RETURN_TO_BASE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mBaseID);
    outSlots[6] = EENetMsgField(this->mKeepFlightTime);
    return 7;
}

// FUNCTION: EXE 0x4c4f64
// Returns the message's debug label "Return To Base"
char* EEMReturnToBase::GetMessageLabel() const
{
    return "Return To Base";
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

// EEMSavedGroupAddSelection's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSavedGroupAddSelection::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SAVED_GROUP_ADD_SELECTION;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGroup);
    return 6;
}

// FUNCTION: EXE 0x4c50c1
// Returns the message's debug label "Add Selection To Saved Group"
char* EEMSavedGroupAddSelection::GetMessageLabel() const
{
    return "Add Selection To Saved Group";
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

// EEMSavedGroupClear's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSavedGroupClear::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SAVED_GROUP_CLEAR;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGroup);
    return 6;
}

// FUNCTION: EXE 0x4c51d8
// Returns the message's debug label "Clear Saved Group"
char* EEMSavedGroupClear::GetMessageLabel() const
{
    return "Clear Saved Group";
}

// FUNCTION: EXE 0x4c522e
// Stores the sender's current selection as a saved group (when anything is selected)
void EEMSavedGroupCreate::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer->mActiveGroup->GetNumUnits() > 0)
        thePlayer->mGroups[this->mGroup]->Assign(thePlayer->mActiveGroup);
}

// EEMSavedGroupCreate's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSavedGroupCreate::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SAVED_GROUP_CREATE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGroup);
    return 6;
}

// FUNCTION: EXE 0x4c52f4
// Returns the message's debug label "Create Saved Group"
char* EEMSavedGroupCreate::GetMessageLabel() const
{
    return "Create Saved Group";
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

// EEMSavedGroupSelect's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSavedGroupSelect::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SAVED_GROUP_SELECT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mGroup);
    outSlots[6] = EENetMsgField(this->mFlag);
    return 7;
}

// FUNCTION: EXE 0x4c551c
// Returns the message's debug label "Select Saved Group"
char* EEMSavedGroupSelect::GetMessageLabel() const
{
    return "Select Saved Group";
}

// FUNCTION: EXE 0x4c5587
// Sends an airplane to the point: clears its goal queues and queues the move in queues 0 and 1 (1 s timer)
void EEMScrambleAircraft::Execute()
{
    EEUnit* theUnit = gWorld.GetUnit(this->mUnitID);
    if (theUnit) {
        EEComplexUnit* theAircraft = theUnit->GetComplexUnit();
        if (theAircraft && (theAircraft->mUnitClass->mCategory == kUnitCategoryAirplane || theAircraft->mUnitClass->mCategory == kUnitCategoryPlanetSpaceship)) {
            EEUGLocation* theGoal = new EEUGLocation(GE2DPoint(long(this->mX), long(this->mY)), false, false, false, 0.0f, false, false);
            theAircraft->ClearAllGoalQueues();
            theAircraft->QueueGoal(theGoal, 0);
            theAircraft->QueueGoal(theGoal->Clone(), 1);
            theAircraft->mGoalQueues[1].AddTime(1000);
        }
    }
}

// EEMScrambleAircraft's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMScrambleAircraft::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SCRAMBLE_AIRCRAFT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mX);
    outSlots[6] = EENetMsgField(this->mY);
    outSlots[7] = EENetMsgField(this->mUnitID);
    return 8;
}

// FUNCTION: EXE 0x4c56d7
// Returns the message's debug label "Scramble Aircraft"
char* EEMScrambleAircraft::GetMessageLabel() const
{
    return "Scramble Aircraft";
}

// FUNCTION: EXE 0x4c5755
// Sets whether the sender's chat goes to the given player
void EEMSelectCustomPlayerForChat::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer)
        thePlayer->mSendChatMessage[this->mPlayerID] = this->mIsSelected;
}

// EEMSelectCustomPlayerForChat's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSelectCustomPlayerForChat::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SELECT_CUSTOM_PLAYER_FOR_CHAT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgField(this->mIsSelected);
    return 7;
}

// FUNCTION: EXE 0x4c5812
// Returns the message's debug label "Select Custom Player For Chat"
char* EEMSelectCustomPlayerForChat::GetMessageLabel() const
{
    return "Select Custom Player For Chat";
}

// FUNCTION: EXE 0x4c587f
// Selects the patrol group of the given unit for the sender
void EEMSelectPatrolGroup::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    unsigned long theID = this->mUnitID;
    EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(theID));
    if (theUnit)
        theUnit->mPatrolGroup->Select(thePlayer, true);
}

// EEMSelectPatrolGroup's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSelectPatrolGroup::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SELECT_PATROL_GROUP;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    return 6;
}

// FUNCTION: EXE 0x4c5956
// Returns the message's debug label "Select Patrol Group"
char* EEMSelectPatrolGroup::GetMessageLabel() const
{
    return "Select Patrol Group";
}

// FUNCTION: EXE 0x4c59ac
// Sets the sender's chat recipient type
void EEMSelectPlayerChatType::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    if (thePlayer)
        thePlayer->mChatType = this->mChatType;
}

// EEMSelectPlayerChatType's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSelectPlayerChatType::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SELECT_PLAYER_CHAT_TYPE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mChatType);
    return 6;
}

// FUNCTION: EXE 0x4c5a54
// Returns the message's debug label "Select Player Chat Type"
char* EEMSelectPlayerChatType::GetMessageLabel() const
{
    return "Select Player Chat Type";
}

// FUNCTION: EXE 0x4c5aaa
// Replaces the sender's selection with the listed units that still exist
void EEMSelectTheseUnits::Execute()
{
    EEGroup* theGroup = gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup;
    theGroup->Clear();
    for (std::vector<long>::iterator it = this->mUnitIDs.begin(); it != this->mUnitIDs.end(); ++it) {
        // a listed unit may have died between sending and executing (it used to be read through NULL)
        EEUnit* theUnit = gWorld.GetUnit(*it);
        if (theUnit != NULL)
            theGroup->Add(theUnit->GetComplexUnit());
    }
}

// EEMSelectTheseUnits' members as the codec's slots; the ids go through ioIDs as int32 (the wire's width)
static size_t Slots(EEMSelectTheseUnits* inMessage, EeNetMsgSlot* outSlots, std::vector<int32_t>& ioIDs)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(inMessage->mFastSelectionID);
    outSlots[5] = EENetMsgRecords(ioIDs.empty() ? NULL : &ioIDs[0], sizeof(int32_t), ioIDs.size());
    return 6;
}

// FUNCTION: EXE 0x4c5afb
// Packs the unit list (header, selection id, count, unit ids) into a new send buffer
void EEMSelectTheseUnits::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    std::vector<int32_t> theIDs(this->mUnitIDs.begin(), this->mUnitIDs.end());   // 4 bytes on the wire (LP64: long is 8)
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    EENetMsgExport(EE_NETMSG_SELECT_THESE_UNITS, theSlots, Slots(this, theSlots, theIDs), outPayloadPointer, outMessageSize);
}

// FUNCTION: EXE 0x4c5bc8
// Returns the unit list's payload size: 4 bytes per unit plus 9
unsigned long EEMSelectTheseUnits::ExportSize()
{
    std::vector<int32_t> theIDs(this->mUnitIDs.size());
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    return ee_netmsg_export_size(EE_NETMSG_SELECT_THESE_UNITS, theSlots, Slots(this, theSlots, theIDs));
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
    std::vector<int32_t> theIDs;
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_SELECT_THESE_UNITS, inPayloadPointer, theSlots, Slots(this, theSlots, theIDs));
    for (unsigned long i = 0; i < theSlots[5].count; i++) {
        int32_t theUnitID;
        memcpy(&theUnitID, inPayloadPointer + theSlots[5].offset + i * sizeof(theUnitID), sizeof(theUnitID));
        this->mUnitIDs.push_back(theUnitID);
    }
}

// FUNCTION: EXE 0x4c5c80
// Selects one unit for the sender
void EEMSelectUnit::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->SelectUnit(this->mUnitID, true, true);
}

// EEMSelectUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSelectUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SELECT_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    return 6;
}

// FUNCTION: EXE 0x4c5d29
// Returns the message's debug label "Select Unit"
char* EEMSelectUnit::GetMessageLabel() const
{
    return "Select Unit";
}

// New (port), TEST ONLY: set by the UI fixture (EETestFixture.cpp, only with EE_TEST_UI) to see each box selection's
// decoded ids as its message is built on the UI thread; NULL otherwise
void (*gEESelectUnitsTestHook)(const std::vector<long>& inUnitIDs) = NULL;

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
    if (gEESelectUnitsTestHook != NULL)   // TEST ONLY: the UI fixture's box selection (EETestFixture.cpp)
        gEESelectUnitsTestHook(this->mUnitIDs);
}

// The selection on the wire: byte 0 is the owner (the ids' top byte), then runs of a count byte (low 6 bits; the top 2 bits: 0, 1 or 2 bytes of the ids' middle 16 bits follow) and the ids' low bytes; at most 63 ids.
// FUNCTION: EXE 0x4c5e67
// Packs unit ids into the compact selection format above (at most 63 ids); returns the byte count
static unsigned char CompressUnits(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer)
{
    std::vector<uint32_t> theIDs(inUnits.begin(), inUnits.end());   // only the low 24 bits count
    return ee_netmsg_compress_units(theIDs.empty() ? NULL : &theIDs[0], theIDs.size(), (uint32_t)inOwner, outBuffer);
}

// FUNCTION: EXE 0x4c5fd1
// Decodes a compact selection buffer into unit ids, replacing the list
static void DecompressUnits(unsigned char* inBuffer, unsigned char inSize, std::vector<long>& outUnits)
{
    int32_t theIDs[0x100];
    size_t theCount = ee_netmsg_decompress_units(inBuffer, inSize, theIDs, 0x100);
    outUnits.clear();
    for (size_t i = 0; i < theCount && i < 0x100; i++)
        outUnits.push_back(theIDs[i]);   // the original's long: the 32-bit value, sign-extended on LP64
}

// New (port), TEST ONLY: the two helpers above for the simulation capture's NMCU record (ee/EEMessageCapture.h)
unsigned char EEMessageCaptureCompress(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer,
                                       std::vector<long>& outUnits)
{
    unsigned char theSize = CompressUnits(inUnits, inOwner, outBuffer);
    DecompressUnits(outBuffer, theSize, outUnits);
    return theSize;
}

// FUNCTION: EXE 0x4c606b
// Selects the decoded units for the sender
void EEMSelectUnits::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    thePlayer->SelectUnits(this->mUnitIDs);
}

// EEMSelectUnits' members as the codec's slots (the buffer: mBufferSize of its bytes)
size_t EEMSelectUnits::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SELECT_UNITS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mBufferSize);
    EeNetMsgSlot theBuffer = {this->mBuffer, this->mBufferSize, 0, 0};
    outSlots[6] = theBuffer;
    return 7;
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
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    this->CodecSlots(theSlots);
    theSlots[6].size = sizeof(this->mBuffer);   // room for the bytes the Rust copies in
    ee_netmsg_import(EE_NETMSG_SELECT_UNITS, inPayloadPointer, theSlots, 7);
    DecompressUnits(this->mBuffer, this->mBufferSize, this->mUnitIDs);
}

// EEMSelfDestruct's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSelfDestruct::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SELF_DESTRUCT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
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

// EEMSendFlare's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSendFlare::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SEND_FLARE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mX);
    outSlots[6] = EENetMsgField(this->mY);
    return 7;
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

// EEMSetAirportCommand's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSetAirportCommand::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SET_AIRPORT_COMMAND;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPoint);
    outSlots[6] = EENetMsgField(this->mCommand);
    outSlots[7] = EENetMsgField(this->mTargetID);
    outSlots[8] = EENetMsgField(this->mIsQueued);
    return 9;
}

// FUNCTION: EXE 0x4c65d2
// Gives the sender's selected airports an order for their aircraft: attack the target (a seen enemy) or go to
// the point, in the queue(s) the command picks (12-15 add a 60 s timer; 15 shares one attack goal)
void EEMSetAirportCommand::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (game messages lane, round 1): the body is Rust (ee_units rally.rs set_airport_command: the attack or move
    // goal, each selected airport's queues and timers by the command) over the store and EEUnitHost.cpp's calls
    ee_units_set_airport_command(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), this->mPoint.GetX(),
                                 this->mPoint.GetY(), this->mCommand, this->mTargetID, this->mIsQueued);
}

// EEMSetDiplomaticStance's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSetDiplomaticStance::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SET_DIPLOMATIC_STANCE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPlayerID);
    outSlots[6] = EENetMsgField(this->mStance);
    return 7;
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
    gUserInterface->AddChatLogEntry(thePlayer->mColor, thePlayer->mName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
}

// EEMSetRallyGoal's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSetRallyGoal::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SET_RALLY_GOAL;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mPoint);
    outSlots[6] = EENetMsgField(this->mTargetID);
    outSlots[7] = EENetMsgField(this->mBuildingID);
    outSlots[8] = EENetMsgField(this->mIsQueued);
    return 9;
}

void EEMSetWaterLevel::Execute()
{
}

// inline (header-declared; its COMDAT 0x4c7486 follows its first user EEMSetRallyGoal::Execute): the unit can
// attack inClass: its stats' +0xd8 is positive, and the damage table entry of inClass against this class
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
bool EEComplexUnit::CanAttackClass(EEUnitClass* inClass)
{
    long theTarget;
    unsigned long theAttacker;
    return this->mStats->mAttack > 0.0f && (theTarget = this->mUnitClass->mAmmoType, theAttacker = inClass->mFamily, inClass->CanBeAttacked())
           && EEDamageFactor(theAttacker, theTarget) > 0.0f;
}


// FUNCTION: EXE 0x4c6dca
// Sets the rally goal of the sender's selected buildings of this type: gather, attack, repair, patrol, garrison,
// clear (target is the building) or move; airports, docks and aircraft fill their own queues
void EEMSetRallyGoal::Execute()
{
    // New (game messages lane, round 1): the body is Rust (ee_units rally.rs set_rally_goal: the goal the target gives,
    // the building owner's selected units' rally queues by the building's type, the marker and the acknowledgement)
    // over the store and EEUnitHost.cpp's calls
    ee_units_set_rally_goal(gWorld.GetPlayerBySlot(this->GetOriginatingSlot()), (uint32_t)this->mBuildingID,
                            this->mPoint.GetX(), this->mPoint.GetY(), this->mTargetID, this->mIsQueued);
}

// FUNCTION: EXE 0x4c74da
// Packs the water-level message (header, selection id, level byte) into a new send buffer

// EEMSetWaterLevel's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSetWaterLevel::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SET_WATER_LEVEL;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mB10);
    return 6;
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
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units commands.rs smart_farmers: with a selection and a complex target, one
    // active group of it keeps its citizens; each drops its load at the target, keeps its farm, takes the nearest free
    // farm, drops what it carries or moves to the target; one unit drawn acknowledges; the group finishes) over the
    // world calls of EEUnitHost.cpp
    ee_units_smart_farmers(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (uint32_t)this->mTargetID);
}

// New (R5b.4): the two farm sorts for the host (ee/EEUnitHost.h), here with their comparators
void EESortFarms(std::vector<EEComplexUnit*>& ioFarms, long inX, long inY, bool inGreater)
{
    if (inGreater)
        std::sort(ioFarms.begin(), ioFarms.end(), EEFarmDistanceGreater(EETileCoordinate(inX, inY)));
    else
        std::sort(ioFarms.begin(), ioFarms.end(), EETileDistanceLess(EETileCoordinate(inX, inY)));
}

// EEMSmartFarmers's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSmartFarmers::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SMART_FARMERS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mTargetID);
    outSlots[6] = EENetMsgField(this->mIsQueued);
    return 7;
}

// FUNCTION: EXE 0x4c7b7b
// Returns the message's debug label "Smart Farmers"
char* EEMSmartFarmers::GetMessageLabel() const
{
    return "Smart Farmers";
}

// EEMStartLoading's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMStartLoading::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_START_LOADING;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mNumHumanPlayers);
    outSlots[6] = EENetMsgField(this->mSeed);
    return 7;
}

// FUNCTION: EXE 0x4c7cdc
// Starts loading the game: seeds the random generator, sets the human player count and multiplayer flag
void EEMStartLoading::Execute()
{
    FSFileSpec theFile;
    UString theName;
    if (gPlatSimSeedSet)   // TEST ONLY: the headless simulation's seed (platform/PlatSim.h; the harness's --seed pin)
        this->mSeed = (long)gPlatSimSeed;
    gGameOptions.mLock->ReadLock();
    gGameOptions.Fn130272();
    gGameOptions.mLock->ReadUnlock();
    gPhysics->mRandom.SetSeed(this->mSeed);
    gUserInterface->mRandomSeed = this->mSeed;
    gCommunications->SetNumHumanPlayers(this->mNumHumanPlayers);
    gCommunications->SetIsMultiplayerSession(this->mNumHumanPlayers > 1);
    gUserInterface->PostStartLoading();
}

// EEMStartLoadingResponse's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMStartLoadingResponse::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_START_LOADING_RESPONSE;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSlot);
    outSlots[6] = EENetMsgField(this->mPlayerID);
    outSlots[7] = EENetMsgField(this->mLobbySlot);
    return 8;
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

// EEMSnapToDaylight's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSnapToDaylight::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SNAP_TO_DAYLIGHT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4c80bf
// Asks the physics thread to snap the day/night cycle to daylight
void EEMSnapToDaylight::Execute()
{
    gPhysics->mThread->Post(0x503, NULL, NULL, true);
}

// EEMStopActiveGroup's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMStopActiveGroup::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_STOP_ACTIVE_GROUP;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4c8144
// Returns the message's debug label "Stop Active Group"
char* EEMStopActiveGroup::GetMessageLabel() const
{
    return "Stop Active Group";
}

// FUNCTION: EXE 0x4c818d
// Stops the sender's living selected units: aircraft drop their orders and, unless taking off, flying a
// pattern or landing, hold at their tile; other units halt
void EEMStopActiveGroup::Execute()
{
    // New (game messages lane, round 1): the body is Rust (ee_units rally.rs stop_active_group: aircraft drop their
    // orders and, unless airborne, move to their tile; the other living units halt)
    ee_units_stop_active_group(static_cast<EEGroup*>(gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->mActiveGroup));
}

// EEMToggleCollisionDetection's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMToggleCollisionDetection::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_TOGGLE_COLLISION_DETECTION;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mDivisor);
    outSlots[6] = EENetMsgField(this->mL14);
    return 7;
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

// EEMToggleDayNight's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMToggleDayNight::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_TOGGLE_DAY_NIGHT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4c84df
// Asks the physics thread to toggle the day/night cycle
void EEMToggleDayNight::Execute()
{
    gPhysics->mThread->Post(0x502, NULL, NULL, true);
}

// EEMUnloadTransport's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMUnloadTransport::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_UNLOAD_TRANSPORT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    outSlots[6] = EENetMsgField(this->mX);
    outSlots[7] = EENetMsgField(this->mY);
    return 8;
}

#include "ee/EEUGLocation.h"

// FUNCTION: EXE 0x4c8615
// Splits the sender's selection by kind; parts with a loaded transport unload at the point, the rest move
// there (a loaded transport that cannot unload there raises a warning event)
void EEMUnloadTransport::Execute()
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(this->GetOriginatingSlot());
    // New (R5b.4): the body is Rust (ee_units group.rs unload_transport: per formation slot a group of the selection
    // spreads for the unload; a loaded transport that can unload there gets the unload goal, else the group moves to
    // the tile, with the warning event for a loaded one) over the world calls of EEUnitHost.cpp
    ee_units_unload_transport(thePlayer, static_cast<EEGroup*>(thePlayer->mActiveGroup), (int64_t)this->GetX(),
                              (int64_t)this->GetY());
}

// FUNCTION: EXE 0x4c889e
// Clears the sender's selection
void EEMUnselectAllUnits::Execute()
{
    gWorld.GetPlayerBySlot(this->GetOriginatingSlot())->UnselectAllUnits();
}

// EEMUnselectAllUnits's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMUnselectAllUnits::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_UNSELECT_ALL_UNITS;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    return 5;
}

// FUNCTION: EXE 0x4c8929
// Returns the message's debug label "Unselect All Units"
char* EEMUnselectAllUnits::GetMessageLabel() const
{
    return "Unselect All Units";
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

// EEMUpdatePingStat's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMUpdatePingStat::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_UPDATE_PING_STAT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mSlot);
    outSlots[6] = EENetMsgField(this->mPing);
    return 7;
}

// FUNCTION: EXE 0x4c8a79
// Returns the message's debug label "Update Ping Stat"
char* EEMUpdatePingStat::GetMessageLabel() const
{
    return "Update Ping Stat";
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

// EEMImproveUnit's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMImproveUnit::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_IMPROVE_UNIT;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mUnitID);
    outSlots[6] = EENetMsgField(this->mImprovement);
    return 7;
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

// EEMZoomInfo's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMZoomInfo::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_ZOOM_INFO;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mZoom);
    return 6;
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

// EEMSetUnitBehavior's codec class and members as the codec's slots, in wire order (ee_netmsg classes.rs)
size_t EEMSetUnitBehavior::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = EE_NETMSG_SET_UNIT_BEHAVIOR;
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    outSlots[5] = EENetMsgField(this->mBehavior);
    return 6;
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
