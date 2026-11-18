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
// NMDC: the Execute decisions (ee_netmsg execute.rs): every call in game (its kind, inputs and outputs), the
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
#include "ee/EEUnitClassKinds.h"
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
#include "rust/sim/netmsg.h"
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

static void Build(std::vector<unsigned char>& ioData)
{
    ioData.resize(ioData.size() + 64, 0);   // slack past the message
    sSweeping = true;
    NEMessage* theMessage = EEMessage::BuildEEMessage(&ioData[0]);
    sSweeping = false;
    if (theMessage != NULL)
        theMessage->Dereference();
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

// one payload of the sweep (ee_netmsg capture.rs, in its order): a game message built, an engine message built and
// recorded, a whole NMCU record committed
static void SweepItem(uint8_t inKind, const uint8_t* inData, size_t inSize)
{
    std::vector<unsigned char> theData(inData, inData + inSize);
    if (inKind == EE_NETMSG_SWEEP_ITEM_GAME)
        Build(theData);
    else if (inKind == EE_NETMSG_SWEEP_ITEM_ENGINE)
        BuildEngine(theData);
    else {
        EESimCaptureRecord theRecord(EE_NETMSG_SWEEP_NMCU);
        if (inSize != 0)
            theRecord.Bytes(inData, (unsigned int)inSize);
        theRecord.Commit();
    }
}

// NMSW, NMCU and NMNE: the sweep's payloads (ee_netmsg capture.rs: the game types by their codec shapes over edge
// patterns, the box selections' lists, the engine types and the communications ticks' nested inputs)
void EEMessageCaptureSweep()
{
    if (!EESimCaptureRecord::Active())
        return;
    ee_netmsg_capture_sweep(SweepItem);
}

// ---- NMDC: the Execute decisions (ee_netmsg execute.rs) ----
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
