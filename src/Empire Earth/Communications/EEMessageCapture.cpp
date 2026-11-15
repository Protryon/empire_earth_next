// New (port), TEST ONLY: the game messages' records of the simulation capture (ee/EEMessageCapture.h;
// tools/sim_capture.py; docs/RUST_PLAN.md R5b.4). Reads the messages, writes nothing to the world.
//
// NMIM: every game message the factory builds (EEMessage::BuildEEMessage: the lobby's, the start's and the players'
//       inputs inside each communications tick): its type, ExportSize, the bytes it was built from, the bytes it exports
//       again, then its members as Import filled them.
// NMEX: every game message the physics executes (EEPhysics::DrainInput): its type, ExportSize, the bytes it exports,
//       its members; then the message is built again from those bytes (an NMIM record). A single-player game hands
//       its messages to the physics unencoded, so this is where the games' messages meet the codec.
// NMSW: the same for the sweep: every game message type built from edge payloads (all 0x00, all 0xff, 0x7f, 0x80, a
//       counting pattern, random bytes; strings of 0..300 bytes, unit lists of 0..63 ids, upgrade lists).
// NMCU: the box selection's packing: the owner and the ids in, CompressUnits' bytes and DecompressUnits' ids out.
// NMNE: the engine messages (NEMessage.cpp, types 0x20..0x3c) over edge payloads, built by NEMessage::BuildNEMessage:
//       the bytes in, the export (ExportSize, the bytes), the members as Import filled them; the two communications
//       ticks with nested players' inputs (none, game and engine messages, unknown types, a tick in a tick, 255).
// NMDC: the Execute decisions (ee/EEMessageExecute.h): every call in game (its kind, inputs and outputs), the
//       decisions' tables over edge values (once), and every five game minutes the real players' market
//       prices, exchange steps and take tests and the disband refund of every unit.
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
#include "ee/EESimCapture.h"
#include "ee/EEMessageCapture.h"
#include "ee/EEMessageExecute.h"
#include "rust/ee_netmsg.h"
#include <initializer_list>

static bool sSweeping = false;

static void Wide(EESimCaptureRecord& ioRecord, const UWideString& inString)
{
    const char* theBytes = (const char*)inString;
    unsigned int theLength = (unsigned int)strlen(theBytes);
    ioRecord.U32(theLength);
    ioRecord.Bytes(theBytes, theLength);
    ioRecord.U32((unsigned int)inString.GetLength());
}

static void Longs(EESimCaptureRecord& ioRecord, const std::vector<long>& inValues)
{
    ioRecord.U32((unsigned int)inValues.size());
    for (size_t i = 0; i < inValues.size(); i++)
        ioRecord.I32((int)inValues[i]);
}

// the members Import filled, read through the codec's slots (EEMessage::GetCodecSlots, in wire order): a fixed
// member its bytes, a string its bytes and character count, records their count and bytes, a blob its bytes; then
// the box selection's ids decoded
static void Members(EESimCaptureRecord& ioRecord, NEMessage* inMessage)
{
    ioRecord.U32(inMessage->GetType());
    ioRecord.U32(inMessage->GetForSpecificSlot());
    ioRecord.U32(inMessage->GetOriginatingSlot());
    EEMessage* theMessage = EEGameMessageCast(inMessage);
    if (theMessage == NULL || ee_netmsg_class_of_type(inMessage->GetType()) == 0xffffffffu) {
        ioRecord.U32(0xffffffffu);   // no type the factory builds
        return;
    }
    EeNetMsgSlot theSlots[32];
    uint32_t theClass;
    theMessage->GetCodecSlots(theSlots, theClass);
    uint32_t theCount = ee_netmsg_import_fields(theClass);
    for (uint32_t i = 0; i < theCount; i++) {
        const EeNetMsgSlot& theSlot = theSlots[4 + i];
        switch (ee_netmsg_field_kind(theClass, i)) {
        case EE_NETMSG_FIELD_STRING:
            ioRecord.U32(theSlot.size);
            ioRecord.Bytes(theSlot.data, theSlot.size);
            ioRecord.U32((unsigned int)PlatMbsLen(theSlot.data));   // UWideString::GetLength
            break;
        case EE_NETMSG_FIELD_RECORDS:
            ioRecord.U32(theSlot.count);
            ioRecord.Bytes(theSlot.data, theSlot.count * theSlot.size);
            break;
        default:   // fixed, blob
            ioRecord.Bytes(theSlot.data, theSlot.size);
            break;
        }
    }
    if (theClass == EE_NETMSG_SELECT_UNITS) {
        std::vector<long> theUnitIDs;
        EESelectUnitsIDs(static_cast<EEMRecord*>(theMessage), theUnitIDs);
        Longs(ioRecord, theUnitIDs);
    }
}

static bool sEngineSweep = false;   // the engine sweep: the nested game inputs are in its NMNE records

void EEMessageCaptureImport(NEMessage* inMessage, const unsigned char* inData)
{
    if (inMessage == NULL || !EESimCaptureRecord::Active() || sEngineSweep)
        return;
    EESimCaptureRecord theRecord(sSweeping ? 0x57534d4eu /* NMSW */ : 0x4d494d4eu /* NMIM */);
    unsigned long theSize = inMessage->ExportSize();
    theRecord.U32(inMessage->GetType());
    theRecord.U32((unsigned int)theSize);
    theRecord.Bytes(inData, (unsigned int)theSize);
    unsigned char* theData = NULL;
    unsigned long theDataSize = 0;
    inMessage->ExportMessage(&theData, theDataSize);
    theRecord.U32((unsigned int)theDataSize);
    if (theData != NULL) {
        theRecord.Bytes(theData, (unsigned int)theDataSize);
        delete[] theData;
    }
    Members(theRecord, inMessage);
    theRecord.Commit();
}

void EEMessageCaptureExecuted(NEMessage* inMessage)
{
    if (inMessage == NULL || !EESimCaptureRecord::Active() || inMessage->GetType() < 0x41 || inMessage->GetType() > 0xfd)
        return;
    EESimCaptureRecord theRecord(0x58454d4eu);   // NMEX
    unsigned char* theData = NULL;
    unsigned long theDataSize = 0;
    theRecord.U32(inMessage->GetType());
    theRecord.U32((unsigned int)inMessage->ExportSize());
    inMessage->ExportMessage(&theData, theDataSize);
    theRecord.U32((unsigned int)theDataSize);
    theRecord.Bytes(theData, (unsigned int)theDataSize);
    Members(theRecord, inMessage);
    theRecord.Commit();
    std::vector<unsigned char> theCopy(theData, theData + theDataSize);
    delete[] theData;
    theCopy.resize(theCopy.size() + 64, 0);
    NEMessage* theMessage = EEMessage::BuildEEMessage(&theCopy[0]);   // its NMIM record
    if (theMessage != NULL)
        theMessage->Dereference();
}

enum { kSweepFixed, kSweepString, kSweepTheseUnits, kSweepCivUpgrades, kSweepSelectUnits };
struct EESweepRow {
    unsigned char mType;
    int mKind;
    unsigned long mPrefix;   // the fixed size (with the 4-byte header), or the bytes before the variable part
    unsigned long mSuffix;   // the fixed bytes after a string
};
static const EESweepRow kSweepRows[] = {
    {kMessageAddAndRemoveUnitsFromSelectedGroup, kSweepSelectUnits, 6, 0},   // EEMAddAndRemoveUnitsFromSelectedGroup
    {kMessageAddPatrolPoint, kSweepFixed, 13, 0},   // EEMAddPatrolPoint
    {kMessageAddRemoveResources, kSweepFixed, 18, 0},   // EEMAddRemoveResources
    {kMessageAddUnitsToSelectedGroup, kSweepSelectUnits, 6, 0},   // EEMAddUnitsToSelectedGroup
    {kMessageAddUnitToSelectedGroup, kSweepFixed, 9, 0},   // EEMAddUnitToSelectedGroup
    {kMessageAlwaysAddUnitToSelectedGroup, kSweepFixed, 9, 0},   // EEMAlwaysAddUnitToSelectedGroup
    {kMessageAttackGround, kSweepFixed, 13, 0},   // EEMAttackGround
    {kMessageAttackPlayers, kSweepFixed, 10, 0},   // EEMAttackPlayers
    {kMessageAttackUnit, kSweepFixed, 11, 0},   // EEMAttackUnit
    {kMessageBuildGate, kSweepFixed, 25, 0},   // EEMBuildGate
    {kMessageBuildUnit, kSweepFixed, 18, 0},   // EEMBuildUnit
    {kMessageBuildWall, kSweepFixed, 29, 0},   // EEMBuildWall
    {kMessageCameraLocationInfo, kSweepFixed, 17, 0},   // EEMCameraLocationInfo
    {kMessageCastCalamity, kSweepFixed, 25, 0},   // EEMCastCalamity
    {kMessageChangeSelectedUnitCollisionSphere, kSweepFixed, 6, 0},   // EEMChangeSelectedUnitCollisionSphere
    {kMessageChangeFormation, kSweepFixed, 10, 0},   // EEMChangeFormation
    {kMessageChat, kSweepString, 6, 0},   // EEMChat
    {kMessageCheatCode, kSweepFixed, 13, 0},   // EEMCheatCode
    {kMessageCivTimerExpired, kSweepFixed, 6, 0},   // EEMCivTimerExpired
    {kMessageCivUpgradesPurchased, kSweepCivUpgrades, 10, 0},   // EEMCivUpgradesPurchased
    {kMessageCivUpgradesRequested, kSweepCivUpgrades, 10, 0},   // EEMCivUpgradesRequested
    {kMessageClearAirportCommands, kSweepFixed, 5, 0},   // EEMClearAirportCommands
    {kMessageDisband, kSweepFixed, 5, 0},   // EEMDisband
    {kMessageDropoffResource, kSweepFixed, 9, 0},   // EEMDropoffResource
    {kMessageEdictProposal, kSweepFixed, 7, 0},   // EEMEdictProposal
    {kMessageEdictVote, kSweepFixed, 9, 0},   // EEMEdictVote
    {kMessageEdictPreProposal, kSweepFixed, 7, 0},   // EEMEdictPreProposal
    {kMessageEdictPreProposalResponse, kSweepFixed, 9, 0},   // EEMEdictPreProposalResponse
    {kMessageEnterShiftMode, kSweepFixed, 9, 0},   // EEMEnterShiftMode
    {kMessageExitShiftMode, kSweepFixed, 9, 0},   // EEMExitShiftMode
    {kMessageExplore, kSweepFixed, 5, 0},   // EEMExplore
    {kMessageFullGameOptions, kSweepString, 53, 0},   // EEMFullGameOptions
    {kMessageGameOptions, kSweepString, 13, 0},   // EEMGameOptions
    {kMessageGameRestarting, kSweepFixed, 5, 0},   // EEMGameRestarting
    {kMessageGameSpeed, kSweepFixed, 9, 0},   // EEMGameSpeed
    {kMessageGatherResource, kSweepFixed, 10, 0},   // EEMGatherResource
    {kMessageGoFanatic, kSweepFixed, 5, 0},   // EEMGoFanatic
    {kMessageHackANewUnit, kSweepFixed, 18, 0},   // EEMHackANewUnit
    {kMessageHaltSelectedGroup, kSweepFixed, 5, 0},   // EEMHaltSelectedGroup
    {kMessageHealUnit, kSweepFixed, 9, 0},   // EEMHealUnit
    {kMessageImmediateCargoUnload, kSweepFixed, 14, 0},   // EEMImmediateCargoUnload
    {kMessageIncrementEpic, kSweepFixed, 5, 0},   // EEMIncrementEpic
    {kMessageIncrementRallyTimer, kSweepFixed, 6, 0},   // EEMIncrementRallyTimer
    {kMessageInGameChat, kSweepString, 5, 0},   // EEMInGameChat
    {kMessageJITManufacturing, kSweepFixed, 5, 0},   // EEMJITManufacturing
    {kMessageKillUnit, kSweepFixed, 6, 0},   // EEMKillUnit
    {kMessageLagStatus, kSweepFixed, 8, 0},   // EEMLagStatus
    {kMessageLoadMainScreen, kSweepFixed, 6, 0},   // EEMLoadMainScreen
    {kMessageLoadMainScreenResponse, kSweepFixed, 7, 0},   // EEMLoadMainScreenResponse
    {kMessageLoadOrSaveGame, kSweepString, 20, 0},   // EEMLoadOrSaveGame
    {kMessageLoadOrSaveGameResponse, kSweepFixed, 9, 0},   // EEMLoadOrSaveGameResponse
    {kMessageLoadPercent, kSweepFixed, 13, 0},   // EEMLoadPercent
    {kMessageLoadUnitData, kSweepFixed, 10, 0},   // EEMLoadUnitData
    {kMessageLoadGarrison, kSweepFixed, 14, 0},   // EEMLoadGarrison
    {kMessageLoadTransport, kSweepFixed, 14, 0},   // EEMLoadTransport
    {kMessageLockUnlock, kSweepFixed, 6, 0},   // EEMLockUnlock
    {kMessagePackUnpack, kSweepFixed, 6, 0},   // EEMPackUnpack
    {kMessageUnloadTransport, kSweepFixed, 17, 0},   // EEMUnloadTransport
    {kMessageMakeWorldVisible, kSweepFixed, 5, 0},   // EEMMakeWorldVisible
    {kMessageMigrationSlotInfo, kSweepFixed, 7, 0},   // EEMMigrationSlotInfo
    {kMessageNewEdictEnacted, kSweepFixed, 12, 0},   // EEMNewEdictEnacted
    {kMessagePathTo, kSweepFixed, 21, 0},   // EEMPathTo
    {kMessagePatrol, kSweepFixed, 9, 0},   // EEMPatrol
    {kMessagePayTribute, kSweepFixed, 26, 0},   // EEMPayTribute
    {kMessageFullPlayerInformation, kSweepString, 17, 23},   // EEMFullPlayerInformation
    {kMessagePlayerInformation, kSweepFixed, 21, 0},   // EEMPlayerInformation
    {kMessagePlayerSlotInfo, kSweepString, 8, 0},   // EEMPlayerSlotInfo
    {kMessageRecordFrames, kSweepFixed, 6, 0},   // EEMRecordFrames
    {kMessageRemoveUnitFromSelectedGroup, kSweepFixed, 9, 0},   // EEMRemoveUnitFromSelectedGroup
    {kMessageRepairUnit, kSweepFixed, 10, 0},   // EEMRepairUnit
    {kMessageReplantFarms, kSweepFixed, 9, 0},   // EEMReplantFarms
    {kMessageRemoveTechnology, kSweepFixed, 15, 0},   // EEMRemoveTechnology
    {kMessageRemoveImprovement, kSweepFixed, 13, 0},   // EEMRemoveImprovement
    {kMessageResearchTechnology, kSweepFixed, 14, 0},   // EEMResearchTechnology
    {kMessageReportStatistics, kSweepFixed, 13, 0},   // EEMReportStatistics
    {kMessageResign, kSweepFixed, 5, 0},   // EEMResign
    {kMessageReturnToBase, kSweepFixed, 10, 0},   // EEMReturnToBase
    {kMessageSavedGroupAddSelection, kSweepFixed, 6, 0},   // EEMSavedGroupAddSelection
    {kMessageSavedGroupClear, kSweepFixed, 6, 0},   // EEMSavedGroupClear
    {kMessageSavedGroupCreate, kSweepFixed, 6, 0},   // EEMSavedGroupCreate
    {kMessageSavedGroupSelect, kSweepFixed, 7, 0},   // EEMSavedGroupSelect
    {kMessageScrambleAircraft, kSweepFixed, 17, 0},   // EEMScrambleAircraft
    {kMessageSelectCustomPlayerForChat, kSweepFixed, 10, 0},   // EEMSelectCustomPlayerForChat
    {kMessageSelectPatrolGroup, kSweepFixed, 9, 0},   // EEMSelectPatrolGroup
    {kMessageSelectPlayerChatType, kSweepFixed, 9, 0},   // EEMSelectPlayerChatType
    {kMessageSelectTheseUnits, kSweepTheseUnits, 9, 0},   // EEMSelectTheseUnits
    {kMessageSelectUnit, kSweepFixed, 9, 0},   // EEMSelectUnit
    {kMessageSelectUnits, kSweepSelectUnits, 6, 0},   // EEMSelectUnits
    {kMessageSelfDestruct, kSweepFixed, 5, 0},   // EEMSelfDestruct
    {kMessageSendFlare, kSweepFixed, 13, 0},   // EEMSendFlare
    {kMessageSetAirportCommand, kSweepFixed, 22, 0},   // EEMSetAirportCommand
    {kMessageSetDiplomaticStance, kSweepFixed, 13, 0},   // EEMSetDiplomaticStance
    {kMessageSetRallyGoal, kSweepFixed, 22, 0},   // EEMSetRallyGoal
    {kMessageSetWaterLevel, kSweepFixed, 6, 0},   // EEMSetWaterLevel
    {kMessageSmartFarmers, kSweepFixed, 10, 0},   // EEMSmartFarmers
    {kMessageStartLoading, kSweepFixed, 10, 0},   // EEMStartLoading
    {kMessageStartLoadingResponse, kSweepFixed, 8, 0},   // EEMStartLoadingResponse
    {kMessageStopActiveGroup, kSweepFixed, 5, 0},   // EEMStopActiveGroup
    {kMessageSnapToDaylight, kSweepFixed, 5, 0},   // EEMSnapToDaylight
    {kMessageToggleCollisionDetection, kSweepFixed, 13, 0},   // EEMToggleCollisionDetection
    {kMessageToggleDayNight, kSweepFixed, 5, 0},   // EEMToggleDayNight
    {kMessageUnselectAllUnits, kSweepFixed, 5, 0},   // EEMUnselectAllUnits
    {kMessageUpdatePingStat, kSweepFixed, 10, 0},   // EEMUpdatePingStat
    {kMessageImproveUnit, kSweepFixed, 13, 0},   // EEMImproveUnit
    {kMessageZoomInfo, kSweepFixed, 9, 0},   // EEMZoomInfo
    {kMessageSetUnitBehavior, kSweepFixed, 9, 0},   // EEMSetUnitBehavior
    {kMessageExchangeResources, kSweepFixed, 11, 0},   // EEMExchangeResources
};

static unsigned long sRandom = 0x2545f491u;
static unsigned char NextRandom()
{
    sRandom ^= sRandom << 13;
    sRandom &= 0xffffffffu;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    sRandom &= 0xffffffffu;
    return (unsigned char)(sRandom >> 8);
}

// pattern p of n bytes: 0 all 0x00, 1 all 0xff, 2 all 0x7f, 3 all 0x80, 4 counting, 5.. random
static void Pattern(std::vector<unsigned char>& ioData, unsigned long inCount, int inPattern)
{
    for (unsigned long i = 0; i < inCount; i++) {
        unsigned char theByte;
        switch (inPattern) {
        case 0: theByte = 0x00; break;
        case 1: theByte = 0xff; break;
        case 2: theByte = 0x7f; break;
        case 3: theByte = 0x80; break;
        case 4: theByte = (unsigned char)(i * 37 + 11); break;
        default: theByte = NextRandom(); break;
        }
        ioData.push_back(theByte);
    }
}

static void Build(std::vector<unsigned char>& ioData)
{
    ioData.resize(ioData.size() + 64, 0);   // slack past the message
    sSweeping = true;
    NEMessage* theMessage = EEMessage::BuildEEMessage(&ioData[0]);
    sSweeping = false;
    if (theMessage != NULL)
        theMessage->Dereference();
}

// the header with pattern p: reserved, type, target slot, originating slot
static void Header(std::vector<unsigned char>& ioData, unsigned char inType, int inPattern)
{
    Pattern(ioData, 4, inPattern);
    ioData[1] = inType;
}

static void PutU32(std::vector<unsigned char>& ioData, unsigned long inValue)
{
    for (int i = 0; i < 4; i++)
        ioData.push_back((unsigned char)(inValue >> (8 * i)));
}

// a wire string (UWideString::Export): u32 length + 2, the bytes, u16 0
static void PutString(std::vector<unsigned char>& ioData, unsigned long inLength, int inPattern)
{
    static const char kText[] = "Empire Earth \xc3\xa9\xc3\xa8 \xe2\x82\xac chat";   // ASCII and UTF-8
    PutU32(ioData, inLength + 2);
    for (unsigned long i = 0; i < inLength; i++) {
        unsigned char theByte = inPattern == 0 ? (unsigned char)kText[i % (sizeof(kText) - 1)]
                                               : (unsigned char)(0x21 + (NextRandom() % 0x5e));
        ioData.push_back(theByte);
    }
    ioData.push_back(0);
    ioData.push_back(0);
}

static const unsigned long kStringLengths[] = {0, 1, 2, 7, 31, 64, 255, 300};
static const unsigned long kIDs[] = {0, 1, 0xff, 0x100, 0x1ff, 0xffff, 0x10000, 0xffff00, 0xffffff, 0x1000000, 0x7fffffff,
                                     0x80000000, 0xffffffff, 0x123456, 0xabcdef};

static unsigned char Compress(const std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer,
                              std::vector<long>& outUnits)
{
    std::vector<uint32_t> theIDs(inUnits.begin(), inUnits.end());   // CompressUnits, then DecompressUnits over its bytes
    unsigned char theSize = ee_netmsg_compress_units(theIDs.empty() ? NULL : &theIDs[0], theIDs.size(), (uint32_t)inOwner, outBuffer);
    EEMRecord theMessage(kMessageSelectUnits);
    theMessage.mList.assign(outBuffer, outBuffer + theSize);
    EESelectUnitsIDs(&theMessage, outUnits);
    return theSize;
}

static void CompressCase(const std::vector<unsigned long>& inUnits, unsigned long inOwner)
{
    EESimCaptureRecord theRecord(0x55434d4eu);   // NMCU
    unsigned char theBuffer[0x100];
    std::vector<long> theOut;
    unsigned char theSize = Compress(inUnits, inOwner, theBuffer, theOut);
    theRecord.U32((unsigned int)inOwner);
    theRecord.U32((unsigned int)inUnits.size());
    for (size_t i = 0; i < inUnits.size(); i++)
        theRecord.U32((unsigned int)inUnits[i]);
    theRecord.U32(theSize);
    theRecord.Bytes(theBuffer, theSize);
    Longs(theRecord, theOut);
    theRecord.Commit();
    // and the selection message built from it
    std::vector<unsigned char> theData;
    Header(theData, kMessageSelectUnits, 4);
    theData.push_back(0x5a);
    theData.push_back(theSize);
    theData.insert(theData.end(), theBuffer, theBuffer + theSize);
    Build(theData);
}

static void UnitLists()
{
    std::vector<unsigned long> theUnits;
    for (unsigned long theOwner = 0; theOwner < 0x101; theOwner += 0x40) {
        theUnits.clear();
        CompressCase(theUnits, theOwner);   // the empty selection
        for (size_t i = 0; i < sizeof(kIDs) / sizeof(kIDs[0]); i++) {
            theUnits.assign(1, kIDs[i]);
            CompressCase(theUnits, theOwner);
        }
        theUnits.assign(kIDs, kIDs + sizeof(kIDs) / sizeof(kIDs[0]));
        CompressCase(theUnits, theOwner);
        for (unsigned long theCount = 2; theCount <= 80; theCount += 13) {
            theUnits.clear();
            for (unsigned long i = 0; i < theCount; i++) {   // ids spread over a few groups, some repeated
                unsigned long theID = ((unsigned long)NextRandom() << 16) % 0x30000;
                theID |= ((unsigned long)NextRandom() << 8) & 0x300;
                theID |= NextRandom();
                theUnits.push_back(theID | (theOwner << 24));
            }
            CompressCase(theUnits, theOwner);
        }
        theUnits.clear();
        for (unsigned long i = 0; i < 70; i++)   // one group over the 63-id limit
            theUnits.push_back(0x4200 + i);
        CompressCase(theUnits, theOwner);
        theUnits.clear();
        for (unsigned long i = 0; i < 70; i++)   // many groups over the limit
            theUnits.push_back((i << 8) | (i * 7));
        CompressCase(theUnits, theOwner);
    }
}

// ---- NMNE: the engine messages (NEMessage.cpp, 0x20..0x3c) over edge payloads, with the players' inputs nested in
// the two communications ticks (game and engine messages, unknown types, a tick in a tick, 255 inputs) ----

static void EngineMessage(EESimCaptureRecord& ioRecord, NEMessage* inMessage);

// a message's export: its type, ExportSize, the bytes ExportMessage writes
static void Exported(EESimCaptureRecord& ioRecord, NEMessage* inMessage)
{
    unsigned char* theData = NULL;
    unsigned long theDataSize = 0;
    ioRecord.U32(inMessage->GetType());
    ioRecord.U32((unsigned int)inMessage->ExportSize());
    inMessage->ExportMessage(&theData, theDataSize);
    ioRecord.U32((unsigned int)theDataSize);
    if (theData != NULL) {
        ioRecord.Bytes(theData, (unsigned int)theDataSize);
        delete[] theData;
    }
}

static void Inputs(EESimCaptureRecord& ioRecord, NEMRecord* inMessage)
{
    ioRecord.U32((unsigned int)inMessage->mPlayerInputs.size());
    for (size_t i = 0; i < inMessage->mPlayerInputs.size(); i++) {
        NEMessage* theInput = inMessage->mPlayerInputs[i];
        if (theInput->GetType() >= 0x20 && theInput->GetType() <= 0x3c) {
            EngineMessage(ioRecord, theInput);
        } else {
            Exported(ioRecord, theInput);
            Members(ioRecord, theInput);
        }
    }
}

// an engine message: its export, then its members as Import filled them (NEMRecord's wire members in wire order: a
// fixed member's bytes, a string, the players' inputs)
static void EngineMessage(EESimCaptureRecord& ioRecord, NEMessage* inMessage)
{
    Exported(ioRecord, inMessage);
    ioRecord.U32(inMessage->GetForSpecificSlot());
    ioRecord.U32(inMessage->GetOriginatingSlot());
    NEMRecord* m = static_cast<NEMRecord*>(inMessage);
    for (uint32_t i = 0; i < ee_netmsg_ne_type(m->GetType()).wire_fields; i++) {
        EeNetMsgNeField theField = ee_netmsg_ne_field(m->GetType(), i);
        if (theField.kind == EE_NETMSG_NE_FIELD_STRING)
            Wide(ioRecord, m->GetString(i));
        else if (theField.kind == EE_NETMSG_NE_FIELD_INPUTS)
            Inputs(ioRecord, m);
        else
            ioRecord.Bytes(m->mRecord + theField.offset, theField.size);
    }
}

static void BuildEngine(std::vector<unsigned char>& ioData)
{
    unsigned int theLength = (unsigned int)ioData.size();
    ioData.resize(ioData.size() + 64, 0);   // slack past the message
    EESimCaptureRecord theRecord(0x454e4d4eu);   // NMNE
    theRecord.U32(theLength);
    theRecord.Bytes(&ioData[0], theLength);
    sEngineSweep = true;
    NEMessage* theMessage = NEMessage::BuildNEMessage(&ioData[0]);
    sEngineSweep = false;
    if (theMessage != NULL) {
        EngineMessage(theRecord, theMessage);
        theMessage->Dereference();
    } else {
        theRecord.U32(0xffffffffu);
    }
    theRecord.Commit();
}

static size_t RowOf(unsigned char inType)
{
    for (size_t r = 0; r < sizeof(kSweepRows) / sizeof(kSweepRows[0]); r++)
        if (kSweepRows[r].mType == inType)
            return r;
    return 0;
}

// a fixed game message of the sweep's rows with pattern p
static void PutGameMessage(std::vector<unsigned char>& ioData, unsigned char inType, int inPattern)
{
    std::vector<unsigned char> theMessage;
    Header(theMessage, inType, inPattern);
    Pattern(theMessage, kSweepRows[RowOf(inType)].mPrefix - 4, inPattern);
    ioData.insert(ioData.end(), theMessage.begin(), theMessage.end());
}

// a header of pattern p appended
static void PutHeader(std::vector<unsigned char>& ioData, unsigned char inType, int inPattern)
{
    std::vector<unsigned char> theHeader;
    Header(theHeader, inType, inPattern);
    ioData.insert(ioData.end(), theHeader.begin(), theHeader.end());
}

// the players' inputs list number l (a count byte, then the messages)
static void PutInputs(std::vector<unsigned char>& ioData, int inList, int inPattern)
{
    switch (inList) {
    case 0:   // none
        ioData.push_back(0);
        break;
    case 1:   // one game message
        ioData.push_back(1);
        PutGameMessage(ioData, kMessageAttackUnit, inPattern);
        break;
    case 2: {   // a chat, an engine message, a wall
        ioData.push_back(3);
        PutHeader(ioData, kMessageChat, inPattern);
        Pattern(ioData, 2, inPattern);
        PutString(ioData, kStringLengths[inPattern % 8], inPattern & 1);
        PutHeader(ioData, kNetMessagePauseGame, inPattern);
        Pattern(ioData, 8, inPattern);
        PutGameMessage(ioData, kMessageBuildWall, inPattern);
        break;
    }
    case 3:   // an unknown type first: the import does not advance, so every input is skipped
        ioData.push_back(2);
        PutHeader(ioData, 0x05, inPattern);
        PutGameMessage(ioData, kMessageAttackUnit, inPattern);
        break;
    case 4:   // a known input, then an unknown type, then one the import never reaches
        ioData.push_back(3);
        PutGameMessage(ioData, kMessageGameSpeed, inPattern);
        PutHeader(ioData, 0xfe, inPattern);
        PutGameMessage(ioData, kMessageAttackUnit, inPattern);
        break;
    case 5:   // a communications tick in the inputs, with its own input
        ioData.push_back(2);
        PutHeader(ioData, kNetMessageCommunicationsTick, inPattern);
        Pattern(ioData, 8, inPattern);
        PutInputs(ioData, 1, (inPattern + 1) % 9);
        PutGameMessage(ioData, kMessageExplore, inPattern);
        break;
    case 6:   // 255 inputs, the most the list holds
        ioData.push_back(255);
        for (int i = 0; i < 255; i++)
            PutHeader(ioData, kNetMessageServerIsAlive, (inPattern + i) % 5);
        break;
    }
}

struct EEEngineSweepRow {
    unsigned char mType;
    unsigned long mFixed;   // the fixed bytes after the header (before the strings, the inputs)
    int mStrings;           // the strings after them (a 4-byte member follows two)
    bool mInputs;
};
static const EEEngineSweepRow kEngineRows[] = {
    {kNetMessageAddPlayer, 1, 1, false},
    {kNetMessageAddPlayerResponse, 0, 0, false},
    {kNetMessageClientConnectionRequest, 1, 2, false},
    {kNetMessageClientConnectionRequestResponse, 10, 1, false},
    {kNetMessageClientDisconnectionNotification, 1, 0, false},
    {kNetMessageClientDisconnectionNotificationResponse, 4, 0, false},
    {kNetMessageCommunicationsTick, 8, 0, true},
    {kNetMessageCommunicationsTickResponse, 10, 0, true},
    {kNetMessagePhysicsTick, 7, 0, false},
    {kNetMessagePlayerConnectionCheck, 4, 0, false},
    {kNetMessagePlayerConnectionCheckResponse, 4, 0, false},
    {kNetMessageRemovePlayer, 4, 0, false},
    {kNetMessageStartGame, 4, 0, false},
    {kNetMessageStartGameResponse, 4, 0, false},
    {kNetMessageStopGame, 0, 0, false},
    {kNetMessageStopGameResponse, 0, 0, false},
    {kNetMessageWorldDatabaseChecksum, 5, 0, false},
    {kNetMessageWorldDatabaseChecksumResponse, 8, 0, false},
    {kNetMessagePauseGame, 8, 0, false},
    {kNetMessagePauseGameResponse, 4, 0, false},
    {kNetMessageUnPauseGame, 8, 0, false},
    {kNetMessageUnPauseGameResponse, 4, 0, false},
    {kNetMessageClientReconnectionRequest, 8, 0, false},
    {kNetMessageClientReconnectionRequestResponse, 9, 0, false},
    {kNetMessageClientRequestPause, 0, 0, false},
    {kNetMessageServerIsAlive, 0, 0, false},
    {kNetMessageXferMessage, 0x405, 0, false},
    {kNetMessageXferStatus, 7, 0, false},
};

static void EngineSweep()
{
    for (size_t r = 0; r < sizeof(kEngineRows) / sizeof(kEngineRows[0]); r++) {
        const EEEngineSweepRow& theRow = kEngineRows[r];
        for (int p = 0; p < 9; p++) {
            int theVariants = theRow.mInputs ? 7 : theRow.mStrings > 0 ? 8 : 1;
            for (int v = 0; v < theVariants; v++) {
                std::vector<unsigned char> theData;
                Header(theData, theRow.mType, p);
                Pattern(theData, theRow.mFixed, p);
                if (theRow.mInputs) {
                    PutInputs(theData, v, p);
                } else if (theRow.mStrings > 0) {
                    PutString(theData, kStringLengths[v], p & 1);
                    if (theRow.mStrings > 1) {
                        PutString(theData, kStringLengths[(v + 3) % 8], (p + 1) & 1);
                        Pattern(theData, 4, p);
                    }
                }
                BuildEngine(theData);
            }
        }
    }
}

void EEMessageCaptureSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    for (size_t r = 0; r < sizeof(kSweepRows) / sizeof(kSweepRows[0]); r++) {
        const EESweepRow& theRow = kSweepRows[r];
        for (int p = 0; p < 9; p++) {
            std::vector<unsigned char> theData;
            switch (theRow.mKind) {
            case kSweepFixed:
                Header(theData, theRow.mType, p);
                Pattern(theData, theRow.mPrefix - 4, p);
                Build(theData);
                break;
            case kSweepString:
                for (size_t l = 0; l < sizeof(kStringLengths) / sizeof(kStringLengths[0]); l++) {
                    theData.clear();
                    Header(theData, theRow.mType, p);
                    Pattern(theData, theRow.mPrefix - 4, p);
                    PutString(theData, kStringLengths[l], p & 1);
                    Pattern(theData, theRow.mSuffix, p);
                    Build(theData);
                }
                break;
            case kSweepTheseUnits:
                for (unsigned long n = 0; n <= 63; n += (n < 4 ? 1 : 19)) {
                    theData.clear();
                    Header(theData, theRow.mType, p);
                    Pattern(theData, 1, p);
                    PutU32(theData, n);
                    Pattern(theData, 4 * n, p);
                    Build(theData);
                }
                break;
            case kSweepCivUpgrades:
                for (unsigned long n = 0; n <= 40; n += (n < 3 ? 1 : 37)) {
                    theData.clear();
                    Header(theData, theRow.mType, p);
                    Pattern(theData, theRow.mPrefix - 4, p);
                    theData.push_back((unsigned char)n);
                    Pattern(theData, 4 * n, p);
                    PutString(theData, kStringLengths[(n + p) % 8], p & 1);
                    Build(theData);
                }
                break;
            case kSweepSelectUnits:
                break;   // in UnitLists: only CompressUnits' buffers decode (DecompressUnits trusts its input)
            }
        }
    }
    UnitLists();
    // the box selection's other two types over one buffer each
    std::vector<unsigned long> theUnits(kIDs, kIDs + 5);
    unsigned char theBuffer[0x100];
    std::vector<long> theOut;
    unsigned char theSize = Compress(theUnits, 3, theBuffer, theOut);
    const unsigned char kTypes[] = {kMessageAddUnitsToSelectedGroup, kMessageAddAndRemoveUnitsFromSelectedGroup};
    for (int t = 0; t < 2; t++) {
        std::vector<unsigned char> theData;
        Header(theData, kTypes[t], 5);
        theData.push_back(0x11);
        theData.push_back(theSize);
        theData.insert(theData.end(), theBuffer, theBuffer + theSize);
        Build(theData);
    }
    EngineSweep();
}

// ---- NMDC: the Execute decisions (ee/EEMessageExecute.h) ----
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);   // EEWheel.cpp

void EEMessageCaptureDecision(unsigned int inKind, const long long* inValues, unsigned int inCount)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(0x43444d4eu);   // NMDC
    theRecord.U32(inKind);
    theRecord.U32(inCount);
    theRecord.Bytes(inValues, inCount * 8);
    theRecord.Commit();
}

static void Decision(unsigned int inKind, std::initializer_list<long long> inValues)
{
    std::vector<long long> theValues(inValues);
    EEMessageCaptureDecision(inKind, &theValues[0], (unsigned int)theValues.size());
}

static long long FloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

static void Exchange(uint8_t inSell, long long inPrice, long long inHave)
{
    EeNetMsgExchangeStep s = ee_netmsg_exchange_step(inSell, inPrice, inHave);
    Decision(kDecisionExchangeStep, {inSell, inPrice, inHave, s.apply, s.cost, s.amount});
}

static void AddRemove(uint8_t inAdd, uint32_t inAmount, long long inHave)
{
    Decision(kDecisionAddRemove, {inAdd, inAmount, inHave, ee_netmsg_add_remove(inAdd, inAmount, inHave)});
}

static void Disband(long long inHitPoints, long long inMax, uint8_t inCan)
{
    float theFraction = -1.0f;
    uint8_t theResult = ee_netmsg_disband_refund(inHitPoints, inMax, inCan, &theFraction);
    Decision(kDecisionDisbandRefund, {inHitPoints, inMax, inCan, theResult, theResult ? FloatBits(theFraction) : 0});
}

// the decisions over plain edge values: their whole tables where they are small
static void DecisionTables()
{
    const uint8_t kIDs[] = {0, 1, 2, 0x7f, 0x80, 0xfe, 0xff};
    const uint8_t kLastIDs[] = {0, 1, 0xff};
    for (uint8_t s = 0; s < 2; s++)
        for (uint8_t l = 0; l < 2; l++)
            Decision(kDecisionSelectionMarksLocal, {s, l, ee_netmsg_selection_marks_local(s, l)});
    for (uint8_t l = 0; l < 2; l++)
        for (uint8_t a : kIDs)
            for (uint8_t b : kIDs)
                Decision(kDecisionSelectionCopies, {l, a, b, ee_netmsg_selection_copies(l, a, b)});
    for (uint8_t s = 0; s < 2; s++)
        for (unsigned int t = 0; t < 256; t++)
            for (uint8_t a : kIDs)
                for (uint8_t b : kLastIDs)
                    Decision(kDecisionCommandRuns, {s, t, a, b, ee_netmsg_command_runs(s, (uint8_t)t, a, b)});
    for (uint8_t e = 0; e < 2; e++)
        Decision(kDecisionSelectionPasses, {e, ee_netmsg_selection_passes(e)});
    const uint32_t kAmounts[] = {0, 1, 2, 99, 100, 0x7fffffffu, 0x80000000u, 0xffffffffu};
    const long long kHaves[] = {-1, 0, 1, 99, 100, 0x7fffffffLL, -0x80000000LL, 0x100000000LL, 0x1ffffffffLL};
    for (uint8_t a = 0; a < 2; a++)
        for (uint32_t m : kAmounts)
            for (long long h : kHaves)
                AddRemove(a, m, h);
    const float kRates[] = {0.0f, 0.25f, 0.5f, 0.666666687f, 1.0f, 1.5f, 2.0f, 3.0f, 10.0f, 1000.0f, -1.0f};
    const long long kTrades[] = {-1000000, -100000, -1000, -101, -100, -1, 0, 1, 100, 101, 1000, 7500, 100000, 1000000};
    for (float r : kRates)
        for (long long t : kTrades)
            for (uint8_t b = 0; b < 2; b++)
                Decision(kDecisionTradePrice, {FloatBits(r), t, b, ee_netmsg_trade_price(r, t, b)});
    const long long kPrices[] = {1, 2, 3, 50, 99, 100, 101, 333, 999, 1000};
    const long long kExchangeHaves[] = {-5000, -50, -1, 0, 1, 2, 3, 49, 50, 99, 100, 101, 333, 999, 1000, 1001, 5000};
    for (uint8_t s = 0; s < 2; s++)
        for (long long p : kPrices)
            for (long long h : kExchangeHaves)
                Exchange(s, p, h);
    for (unsigned int m = 0; m < 32; m++)
        for (uint8_t t = 0; t < 2; t++) {
            uint32_t theAmounts[7] = {0, 0, 0, 0, 0, 0, 0};
            for (int i = 0; i < 5; i++)
                if (m & (1u << i))
                    theAmounts[2 + i] = i == 4 ? 0xffffffffu : 1 + i * 37;
            uint8_t theOrder[5] = {0, 0, 0, 0, 0};
            uint32_t n = ee_netmsg_tribute_order(theAmounts, t, theOrder);
            Decision(kDecisionTributeOrder, {theAmounts[2], theAmounts[3], theAmounts[4], theAmounts[5], theAmounts[6], t, n,
                                             theOrder[0], theOrder[1], theOrder[2], theOrder[3], theOrder[4]});
        }
    for (uint8_t f = 0; f < 2; f++)
        for (uint8_t t = 0; t < 2; t++)
            Decision(kDecisionTributeViewer, {f, t, ee_netmsg_tribute_viewer(f, t)});
    const long long kHitPoints[] = {-1, 0, 1, 3, 50, 99, 100, 250, 1001};
    const long long kMaxHitPoints[] = {0, 1, 3, 100, 250, 7777};
    for (long long h : kHitPoints)
        for (long long m : kMaxHitPoints)
            for (uint8_t c = 0; c < 2; c++)
                Disband(h, m, c);
}

void EEMessageCaptureDecisionSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    static bool sTablesDone = false;
    if (!sTablesDone) {
        sTablesDone = true;
        DecisionTables();
    }
    // the real players: every resource's market price both ways (EEGetTradePrice, the world table's rates), the
    // exchange's steps and the take's test from what they have; the disband refund of every unit
    for (unsigned long p = 1; p < gWorld.GetNumPlayers(); p++) {
        EEPlayer* thePlayer = gWorld.GetPlayer(p);
        if (thePlayer == NULL)
            continue;
        for (long r = kResourceFood; r <= kResourceIron; r++) {
            for (uint8_t b = 0; b < 2; b++) {
                long thePrice = EEGetTradePrice(thePlayer, r, b);
                Decision(kDecisionTradePrice, {(long long)p, r, b, (long long)thePlayer->mResourceTrade[r - 2], thePrice});
                long theHave = thePlayer->mResourceCaps[b ? r : kResourceGold];
                Exchange(b, thePrice, theHave);
                Exchange(b, thePrice, theHave / 3);
                Exchange(b, thePrice, thePrice - 1);
            }
            long theHave = thePlayer->mResourceCaps[r];
            AddRemove(0, 1, theHave);
            AddRemove(0, (uint32_t)theHave, theHave);
            AddRemove(0, (uint32_t)theHave + 1, theHave);
            AddRemove(0, 100, theHave);
        }
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex == NULL || theComplex->mStats == NULL || theComplex->mUnitClass == NULL)
                continue;
            Disband(theComplex->mHitPoints, theComplex->mStats->mMaxHitPoints, theComplex->mUnitClass->mCanDisband);
        }
    }
}
