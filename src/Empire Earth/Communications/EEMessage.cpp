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
#include "ee/EECivUpgrade.h"
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

// a new buffer of the message's size, filled by the Rust (the original's ExportMessage shape)
static void EENetMsgExport(uint32_t inClass, EeNetMsgSlot* inSlots, size_t inCount, unsigned char** outPayloadPointer,
                           unsigned long& outMessageSize)
{
    unsigned long theMessageSize = ee_netmsg_export_size(inClass, inSlots, inCount);
    *outPayloadPointer = new unsigned char[theMessageSize];
    outMessageSize = ee_netmsg_export(inClass, inSlots, inCount, *outPayloadPointer, theMessageSize);
}

// New (game messages lane, round 1): the codec's side of every game message, once. The message gives its codec
// class and its members as slots in wire order (GetCodecSlots); these run the codec over them, as each class's own
// ExportMessage, ExportSize and ImportMessage did.
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

// ---- EEMRecord (game messages lane, round 3): the record types' messages (ee/EEMessage.h; ee_netmsg records.rs) ----
EEMRecordValue::EEMRecordValue(const GE2DPoint& inPoint) : mInteger(0), mIsFloat(true)
{
    this->mFloats[0] = inPoint.GetX();
    this->mFloats[1] = inPoint.GetY();
}

EEMRecordValue::EEMRecordValue(const GEVector& inVector) : mInteger(0), mIsFloat(true)
{
    this->mFloats[0] = inVector.GetX();
    this->mFloats[1] = inVector.GetY();
    this->mFloats[2] = inVector.GetZ();
}

// the import constructor: EEMessage's defaults (no send flag), as the old classes' import constructors
EEMRecord::EEMRecord(unsigned char* inData)
{
    memset(this->mRecord, 0, sizeof(this->mRecord));
    this->mType = inData[1];
    this->ImportMessage(inData);
}

// a sender's: the members in wire order, then the ones off the wire (fewer leave the rest 0)
EEMRecord::EEMRecord(unsigned char inType, std::initializer_list<EEMRecordValue> inValues)
    : EEMessage(inType, (ee_netmsg_record_flags(inType) & EE_NETMSG_RECORD_WHEN_OUT) != 0)
{
    memset(this->mRecord, 0, sizeof(this->mRecord));
    ESVERIFY_AT(ee_netmsg_record_class(inType) != 0xffffffffu, __LINE__);
    uint32_t theIndex = 1;
    for (const EEMRecordValue& theValue : inValues) {
        EeNetMsgRecordField theField = ee_netmsg_record_member(inType, theIndex);
        bool theBeside = theField.kind == EE_NETMSG_RECORD_FIELD_STRING || theField.kind == EE_NETMSG_RECORD_FIELD_LIST
                         || theField.kind == EE_NETMSG_RECORD_FIELD_BLOB;
        unsigned char* theTo = theBeside ? NULL : this->Field(theIndex, theField.size);
        theIndex++;
        float theFloat = theValue.mIsFloat ? theValue.mFloats[0] : (float)theValue.mInteger;
        long long theInteger = theValue.mIsFloat ? (long long)theValue.mFloats[0] : theValue.mInteger;
        bool theBool = theInteger != 0 || (theValue.mIsFloat && theValue.mFloats[0] != 0.0f);
        switch (theField.kind) {
        case EE_NETMSG_RECORD_FIELD_STRING:   // the one string member (not in mRecord)
            ESVERIFY_AT(theValue.mString != NULL, __LINE__);
            this->mString = *theValue.mString;
            break;
        case EE_NETMSG_RECORD_FIELD_LIST:   // the one list (not in mRecord): each element's low bytes, one record
            ESVERIFY_AT(theValue.mList != NULL && theValue.mListElement >= theField.size, __LINE__);
            for (size_t i = 0; i < theValue.mListCount; i++) {
                const unsigned char* theElement = (const unsigned char*)theValue.mList + i * theValue.mListElement;
                this->mList.insert(this->mList.end(), theElement, theElement + theField.size);
            }
            break;
        case EE_NETMSG_RECORD_FIELD_BLOB:   // the blob and a member off the wire as bytes: set by their sender
        case EE_NETMSG_RECORD_FIELD_RAW:
            break;
        case EE_NETMSG_RECORD_FIELD_INTEGER:
            memcpy(theTo, &theInteger, theField.size);   // little-endian: the low bytes
            break;
        case EE_NETMSG_RECORD_FIELD_BOOL:
            *theTo = theBool ? 1 : 0;
            break;
        case EE_NETMSG_RECORD_FIELD_FLOAT:
            memcpy(theTo, &theFloat, 4);
            break;
        default:   // a point or a vector
            memcpy(theTo, theValue.mFloats, theField.size);
            break;
        }
    }
}

unsigned char* EEMRecord::Field(uint32_t inIndex, size_t inSize) const
{
    EeNetMsgRecordField theField = ee_netmsg_record_member(this->mType, inIndex);
    ESVERIFY_AT(theField.kind != EE_NETMSG_RECORD_FIELD_NONE && theField.size == inSize, __LINE__);
    return const_cast<unsigned char*>(this->mRecord) + theField.offset;
}

bool EEMRecord::GetCanSendWhilePaused() const
{
    return (ee_netmsg_record_flags(this->mType) & EE_NETMSG_RECORD_PAUSED) != 0;
}

char* EEMRecord::GetMessageLabel() const
{
    return const_cast<char*>(ee_netmsg_record_label(this->mType));
}

bool EEMRecord::GetSendToAllAndWaitMessageID(unsigned long& outID) const
{
    EeNetMsgRecordRules theRules = ee_netmsg_record_rules(this->mType);
    if (theRules.wait_field != 0)
        outID = this->Get<uint32_t>(theRules.wait_field);
    return theRules.wait_field != 0;
}

void EEMRecord::SetSendToAllAndWaitMessageID(unsigned long inID)
{
    EeNetMsgRecordRules theRules = ee_netmsg_record_rules(this->mType);
    if (theRules.wait_field != 0)
        this->Set<uint32_t>(theRules.wait_field, (uint32_t)inID);
}

// FUNCTION: EXE 0x4c0e6a (EEMLoadOrSaveGame::IsResponse)
// Whether a message is this one's response, by the type's rule (Load or Save: the response for its tick)
bool EEMRecord::IsResponse(NEMessage* inMessage)
{
    EeNetMsgRecordRules theRules = ee_netmsg_record_rules(this->mType);
    return theRules.response_type != 0 && inMessage->GetType() == theRules.response_type
           && this->Get<int32_t>(theRules.response_mine) == static_cast<EEMRecord*>(inMessage)->Get<int32_t>(theRules.response_theirs);
}

size_t EEMRecord::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = ee_netmsg_record_class(this->mType);
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    size_t theCount = 5;
    for (EeNetMsgRecordField theField; (theField = ee_netmsg_record_field(outClass, (uint32_t)theCount - 4)).kind != 0; theCount++) {
        EeNetMsgSlot theSlot = {this->mRecord + theField.offset, theField.size, 0, 0};
        if (theField.kind == EE_NETMSG_RECORD_FIELD_STRING)
            theSlot = EENetMsgString(this->mString);
        else if (theField.kind == EE_NETMSG_RECORD_FIELD_LIST || theField.kind == EE_NETMSG_RECORD_FIELD_BLOB) {
            // the records: the first, a record's size, the count; the blob: its bytes (the room for an import)
            theSlot.data = this->mList.empty() ? NULL : &this->mList[0];
            theSlot.size = theField.kind == EE_NETMSG_RECORD_FIELD_LIST ? theField.size : (uint32_t)this->mList.size();
            theSlot.count = theField.kind == EE_NETMSG_RECORD_FIELD_LIST ? (uint32_t)(this->mList.size() / theField.size) : 0;
        }
        outSlots[theCount] = theSlot;
    }
    return theCount;
}

// the codec over the slots, then the string member's text from where the codec found it (UWideString::Import), the
// list's records from where it found them, the blob's bytes as many as it found
void EEMRecord::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, __LINE__);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    this->mList.clear();   // the slots carry no room: the list and the blob are copied from where the codec found them
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    ee_netmsg_import(theClass, inPayloadPointer, theSlots, theCount);
    for (size_t i = 5; i < theCount; i++) {
        EeNetMsgRecordField theField = ee_netmsg_record_field(theClass, (uint32_t)i - 4);
        unsigned char* theFrom = inPayloadPointer + theSlots[i].offset;
        if (theField.kind == EE_NETMSG_RECORD_FIELD_STRING)
            this->mString.Import(theFrom);
        else if (theField.kind == EE_NETMSG_RECORD_FIELD_LIST)
            this->mList.assign(theFrom, theFrom + theSlots[i].count * theField.size);
        else if (theField.kind == EE_NETMSG_RECORD_FIELD_BLOB)
            this->mList.assign(theFrom, theFrom + theSlots[i].count);
    }
}

// FUNCTION: EXE 0x4be6c6 (the EEMFullGameOptions constructor)
// The lobby's game options, whole (the server sends gGameOptions); the members in wire order
EEMRecord* EENewFullGameOptions(const EEGameOptions& inOptions)
{
    return new EEMRecord(kMessageFullGameOptions,
                         {inOptions.GetAllowCustomCivs(), inOptions.GetB407(), inOptions.GetCheatCodes(),
                          inOptions.GetDifficulty(), inOptions.GetEpochEnd(), inOptions.GetGameSpeed(),
                          inOptions.GetLoadType(), inOptions.GetVariant(), inOptions.GetLockSpeed(),
                          inOptions.GetLockTeams(), inOptions.GetMapSize(), inOptions.GetRecordGames(),
                          inOptions.GetResources(), inOptions.GetRevealMap(), inOptions.GetEpochStart(),
                          inOptions.GetUnitLimit(), inOptions.GetVictory(), inOptions.GetWondersForVictory(),
                          inOptions.GetMapType()});
}

// FUNCTION: EXE 0x4c378e (the EEMFullPlayerInformation constructor)
// One lobby slot, whole (the server sends it to the players); the members in wire order
EEMRecord* EENewFullPlayerInformation(const EEGameOptionsPlayer& inPlayer)
{
    return new EEMRecord(kMessageFullPlayerInformation,
                         {inPlayer.mSlot, inPlayer.mType, inPlayer.mLoadedType, inPlayer.mName, inPlayer.mPing,
                          inPlayer.mColor, inPlayer.mTeam, inPlayer.mStartingCitizens, inPlayer.mIsReady,
                          inPlayer.mIsLocal, inPlayer.mB36, (int32_t)inPlayer.mEngine});   // the engine: port
}

// FUNCTION: EXE 0x4b8cda
// Registers the game's message factory for every game message type (0x41..0xfd) with the network engine
void EEMessage::RegisterKnownMessages()
{
    NEMessage::RegisterBuilder(0x41, 0xfd, EEMessage::BuildEEMessage);
}

// FUNCTION: EXE 0x4b8cf7 (ExecuteSelectionMessage), 0x4b8e86 (ExecuteMessage)
// The physics' two ways to run a message (the selection id's framing and decisions: ee_netmsg bodies.rs
// execute_framed, over the host at the end of this file)
static void EENetMsgExecute(EEMRecord* inMessage, uint8_t inFraming);
void EEMessage::ExecuteSelectionMessage()
{
    EENetMsgExecute(static_cast<EEMRecord*>(this), 1);
}

void EEMessage::ExecuteMessage()
{
    EENetMsgExecute(static_cast<EEMRecord*>(this), 2);
}

// FUNCTION: EXE 0x4b8f8e
// Creates the game message object for a received buffer from its type byte (NULL and an assert for an unknown type)
NEMessage* EEMessage::BuildEEMessage(unsigned char* inMemory)
{
    unsigned char theType = inMemory[1];
    NEMessage* theMessage = NULL;
    ESVERIFY_AT(inMemory != NULL, 841);
    // every type the factory builds is a record (EEMRecord, ee_netmsg records.rs; not EEMEndCinematic's 0x9d: no game
    // type)
    if (ee_netmsg_record_class(theType) != 0xffffffffu && ee_netmsg_class_of_type(theType) != 0xffffffffu)
        theMessage = new EEMRecord(inMemory);
    else
        ESVERIFY_AT("Undefined message type" == NULL, 1275);
    EEMessageCaptureImport(theMessage, inMemory);   // TEST ONLY: the simulation capture's NMIM record (off by default)
    return theMessage;
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

#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEStringIDs.h"

// FUNCTION: EXE 0x4bca28
// Gives the sender's selection (and its saved group) a new formation of the requested type
static void ExecuteChangeFormation(EEMRecord* m)
{
    EESelectedGroup* theGroup = gWorld.GetPlayerBySlot(m->GetOriginatingSlot())->mActiveGroup;
    EEFormation* theFormation = EEFormation::Create(m->Get<int32_t>(1));
    theFormation->mB5C = m->Get<bool>(2);
    theGroup->SetFormation(theFormation);
    if (theGroup->mSavedGroup)
        theGroup->mSavedGroup->SetFormation(theFormation);
    theFormation->Dereference();
}

// FUNCTION: EXE 0x4bd3f7
// Applies a player's civilization (name and upgrades) validated by the server, once only when civs are bought;
// locks the choice for players other than the local one (members: the player, the name string, the upgrades, the name)
static void ExecuteCivUpgradesPurchased(EEMRecord* m)
{
    UWideString theName;
    unsigned long thePlayerIndex = m->Get<unsigned char>(1);
    EEPlayer* thePlayer = gWorld.mPlayers[thePlayerIndex];
    bool theApply = gWorld.mGameOptions.CanBuyCivilization() ? !thePlayer->mCivUpgradesPurchased : true;
    if (theApply) {
        theName = m->mString;
        if (m->Get<int32_t>(2))
            gResource->GetString(m->Get<int32_t>(2), theName);
        thePlayer->mCivilization.SetName(theName);
        std::vector<EECivUpgrade> theUpgrades = m->GetList<EECivUpgrade>();
        thePlayer->mCivilization.AddUpgrades(theUpgrades, thePlayerIndex, true);
        thePlayer->mCivUpgradesPurchased = true;
        if (thePlayer != gWorld.GetCurrentPlayer()) {
            thePlayer->mCivilizationChosen = true;
            thePlayer->mCivilizationLocked = true;
        }
    }
}

// FUNCTION: EXE 0x4bd4b3
// Has the server validate a player's requested civilization upgrades (EECBRequestUpgrades)
static void ExecuteCivUpgradesRequested(EEMRecord* m)
{
    std::vector<EECivUpgrade> theUpgrades = m->GetList<EECivUpgrade>();
    EECBRequestUpgrades(m->Get<unsigned char>(1), m->mString, theUpgrades);
}

// 0x23cc42 (cdecl): the exchange rate between a resource and the trade resource for inPlayer (provisional name)
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);

// FUNCTION: EXE 0x4bfcfd
// Unloads one cargo unit from a living, enabled transport at once; if that fails, posts the transport class's
// unload-failed event to its owner
static void ExecuteImmediateCargoUnload(EEMRecord* m)
{
    EEUnit* theTransport = gWorld.GetUnit(m->Get<int32_t>(1));
    EEUnit* theCargo = gWorld.GetUnit(m->Get<int32_t>(2));
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
static void ExecuteLoadMainScreen(EEMRecord* m)
{
    if (m->Get<unsigned char>(1)) {
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
            new EEMRecord(kMessageLoadMainScreenResponse, {gWorld.GetCurrentPlayer()->GetSlot(), gWorld.mCurrentPlayer}), kSlotServer);
    }
}

bool gLocalPlayerOut;                      // 0x4e6099 (.bss between this unit's Execute statics 0x4e6098/0x4e609a; ee/EEMessage.h)

#include "ee/EESaveLoad.h"
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);  // 0x14cfb1

// FUNCTION: EXE 0x4c0e83
// Single-player server load: loads the saved game and logs a response; anything else queues the save with the
// physics thread; announces it in the chat log and shows the error box if loading failed (members: load, quiet,
// tick, time, auto-save, the wait id, the file name)
static void ExecuteLoadOrSaveGame(EEMRecord* m)
{
    UWideString theText;
    UWideString theFormat;
    UString theDirectory;
    long theResult;
    if (m->Get<unsigned char>(1) && gCommunications->IsServer() && !gCommunications->GetIsMultiplayerSession()) {
        gResource->GetString(IDS_LOADING, theFormat);
        sprintf(theText, theFormat, (const char*)m->mString);
        gUserInterface->AddChatLogEntry(theText, 0);
        theDirectory = "data\\";
        theDirectory += "Saved Games\\";
        theResult = EELoadSavedGameFile(UString(m->mString), true, m->Get<bool>(2), theDirectory, 1);
        EEMRecord* theResponse = new EEMRecord(kMessageLoadOrSaveGameResponse, {m->Get<int32_t>(3)});
        theResponse->SetOriginatingSlot(gWorld.GetCurrentPlayer()->mSlot);
        static_cast<NESynchronousServer*>(gCommunications)->LogResponse(theResponse);
    } else {
        gPhysics->QueueMessage(m);
        gResource->GetString(IDS_SAVING_GAME_AS, theFormat);
        sprintf(theText, theFormat, (const char*)m->mString);
        gUserInterface->AddChatLogEntry(theText, 0);
        theResult = 0;
    }
    if (theResult != 0)
        gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
}

extern long& gAutoSaveCount;                 // 0x5308b8 (the auto-save count LoadOrSave passes on; provisional name)
// (0x14cbc7 TrimAutoSaves: ee/EESaveLoad.h)

// FUNCTION: EXE 0x4c1052
// Saves the game under the requested name, rotates the auto-saves for an auto-save, restores the mouse and reports
// an error box on failure; returns the request's tick
long EELoadOrSave(EEMRecord* m)
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
    long theResult = SaveGame(UString(m->mString), m->Get<int32_t>(4), m->Get<bool>(2), theFolder);
    if (m->Get<unsigned char>(5) == 1) {
        theDirectory = UString("data\\") + UString("Saved Games\\");
        theName = UString("AutoSave") + UString("*") + gSavedGameExtension;
        TrimAutoSaves(theName, theDirectory, gAutoSaveCount);
    }
    if (gCommunications->IsServer() && !m->Get<bool>(2) && theResult == 0)
        gUserInterface->CloseForm(NULL);
    gISMouse->Refresh();
    gISMouse->SetUseBufferedMovement(true);
    gMouse->Update();
    gMouse->SetPosition(thePosition);
    if (theResult != 0)
        gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
    return m->Get<int32_t>(3);
}

// FUNCTION: EXE 0x4c1264
// Packs the load/save response (header, selection id, tick) into a new send buffer

// FUNCTION: EXE 0x4c153d
// Changes the sender's cheat unit class: unloads the old class's data for every player and preloads the new one
static void ExecuteLoadUnitData(EEMRecord* m)
{
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(m->GetOriginatingSlot());
    if (thePlayer->mHackUnitClass != m->Get<int32_t>(1)) {
        unsigned long i;
        for (i = 1; i < gWorld.mNumPlayers; i++)
            if (thePlayer->mHackUnitClass < EEUnitClassCount() && gUnitClassTable[thePlayer->mHackUnitClass])
                gDataPreLoading->PostUnloadUnitClass(gWorld.mPlayers[i]->mPlayerID, thePlayer->mHackUnitClass);
        thePlayer->SetHackUnitClass(m->Get<int32_t>(1));
        for (i = 1; i < gWorld.mNumPlayers; i++)
            if (thePlayer->mHackUnitClass < EEUnitClassCount() && gUnitClassTable[thePlayer->mHackUnitClass])
                gDataPreLoading->PostLoadUnitClass(gWorld.mPlayers[i]->mPlayerID, thePlayer->mHackUnitClass);
    }
}

// FUNCTION: EXE 0x4c2170
// After a host migration, moves a player to the new slot; a player left without a slot is announced as dropped and
// (unless observing) removed from the game as defeated; tells the local client a new host is set
static void ExecuteMigrationSlotInfo(EEMRecord* m)
{
    UWideString theText;
    UWideString theFormat;
    if (m->Get<unsigned char>(2) == kSlotNone) {
        unsigned char theSlot = gWorld.mPlayers[m->Get<unsigned char>(1)]->mSlot;
        if (theSlot != kSlotNone && theSlot != kSlotServer) {
            gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
            sprintf(theText, theFormat, (const char*)gWorld.mPlayers[m->Get<unsigned char>(1)]->mName);
            gUserInterface->AddChatLogEntry(theText, 1);
        }
        if (!gWorld.mPlayers[m->Get<unsigned char>(1)]->mObserverMode) {
            gWorld.RemovePlayerFromGame(0, m->Get<unsigned char>(1));
            gWorld.mPlayers[m->Get<unsigned char>(1)]->SetGameResult(2);
        }
    }
    gWorld.mPlayers[m->Get<unsigned char>(1)]->mSlot = m->Get<unsigned char>(2);
    if (m->Get<unsigned char>(1) == gWorld.GetCurrentPlayer()->mPlayerID && gCommunications->IsClient()) {
        gResource->GetString(IDS_NEW_HOST_ESTABLISHED, theText);
        gUserInterface->AddChatLogEntry(theText, 0);
    }
}

#include "lle/GE3DLine.h"

// FUNCTION: EXE 0x4c534a
// Selects the sender's saved group; for the local player resets the command panel, centres the camera on it
// when asked and plays its selection sound
static void ExecuteSavedGroupSelect(EEMRecord* m)
{
    GE3DPoint theLocation;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(m->GetOriginatingSlot());
    EEUIFMGMainGroup* theMainGroup = static_cast<EEUIFMGMainGroup*>(gUIManager->mMainGameForm->mGroups[0]);
    unsigned char theSlot = m->GetOriginatingSlot();
    unsigned char theLocalSlot = gWorld.GetCurrentPlayer()->GetSlot();
    thePlayer->mActiveGroup->SelectSavedGroup(thePlayer->mGroups[m->Get<unsigned char>(1)], true);
    if (theSlot == theLocalSlot && gWorld.mExecutingLocalSelection == true) {
        gMouse->mPointer = 0;
        theMainGroup->ShowCommands(theMainGroup->GetSelectedUnit());
        theMainGroup->Reset();
        gUIManager->mMainGameForm->SetUIState(0);
        gUIManager->mMainGameForm->mIsCursorLocked = false;
        EEComplexUnit* theUnit = thePlayer->mSelectedGroup->GetFirstUnit();
        if (theUnit) {
            if (m->Get<bool>(2)) {
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

// New (port), TEST ONLY: set by the UI fixture (EETestFixture.cpp, only with EE_TEST_UI) to see each box selection's
// decoded ids as its message is built on the UI thread; NULL otherwise
void (*gEESelectUnitsTestHook)(const std::vector<long>& inUnitIDs) = NULL;

// FUNCTION: EXE 0x4c5d7f (the EEMSelectUnits constructor)
// Creates a box selection: picks the local player's units of the given type in the rectangle and stores them
// compressed (the count, the blob), the rectangle and the type beside (members 3 and 4, off the wire). The selection
// on the wire: byte 0 is the owner (the ids' top byte), then runs of a count byte (low 6 bits; the top 2 bits: 0, 1
// or 2 bytes of the ids' middle 16 bits follow) and the ids' low bytes; at most 63 ids (CompressUnits 0x4c5e67,
// ee_netmsg units.rs).
EEMRecord* EENewSelectUnits(const GERectangle& inRectangle, long inUnitType, unsigned char inType)
{
    std::vector<unsigned long> theUnits;
    gUnitPicker.PickUnits(inRectangle, gWorld.GetCurrentPlayer(), inUnitType, theUnits);
    std::vector<uint32_t> theIDs(theUnits.begin(), theUnits.end());   // only the low 24 bits count
    unsigned char theBuffer[0x100];
    unsigned char theSize = ee_netmsg_compress_units(theIDs.empty() ? NULL : &theIDs[0], theIDs.size(),
                                                     (uint32_t)gWorld.GetCurrentPlayer()->mPlayerID, theBuffer);
    EEMRecord* theMessage = new EEMRecord(inType, {theSize, 0, 0, inUnitType});
    theMessage->mList.assign(theBuffer, theBuffer + theSize);
    theMessage->Set<GERectangle>(3, inRectangle);
    if (gEESelectUnitsTestHook != NULL) {   // TEST ONLY: the UI fixture's box selection (EETestFixture.cpp)
        std::vector<long> theUnitIDs;
        EESelectUnitsIDs(theMessage, theUnitIDs);
        gEESelectUnitsTestHook(theUnitIDs);
    }
    return theMessage;
}

// FUNCTION: EXE 0x4c5fd1 (DecompressUnits)
// Decodes a box selection's compact buffer into unit ids, replacing the list
void EESelectUnitsIDs(const EEMRecord* inMessage, std::vector<long>& outUnits)
{
    int32_t theIDs[0x100];
    size_t theCount = ee_netmsg_decompress_units(inMessage->mList.empty() ? NULL : &inMessage->mList[0],
                                                 (unsigned char)inMessage->mList.size(), theIDs, 0x100);
    outUnits.clear();
    for (size_t i = 0; i < theCount && i < 0x100; i++)
        outUnits.push_back(theIDs[i]);   // the original's long: the 32-bit value, sign-extended on LP64
}

// FUNCTION: EXE 0x4c63e8
// Shows a flare at the tile to the sender and each ally, and tells each ally it received one
static void ExecuteSendFlare(EEMRecord* m)
{
    std::vector<EEPlayer*> theAllies;
    U2DPoint<long> theTile;
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(m->GetOriginatingSlot());
    theAllies = thePlayer->mAllies;
    theTile.mX = m->Get<int32_t>(1);
    theTile.mY = m->Get<int32_t>(2);
    thePlayer->mEvents.AddEvent(theTile, 3, thePlayer, -1, true);
    for (std::vector<EEPlayer*>::iterator it = theAllies.begin(); it != theAllies.end(); ++it) {
        (*it)->mEvents.AddEvent(theTile, 3, *it, -1, true);
        (*it)->FlareReceived(thePlayer, theTile);
    }
}

// FUNCTION: EXE 0x4c6b85
// Sets the sender's stance toward a player and posts "<name> changes diplomatic stance to" friendly
// (stance 0) or hostile to the chat log
static void ExecuteSetDiplomaticStance(EEMRecord* m)
{
    UWideString theAlly;
    UWideString theEnemy;
    UWideString theIsNow;
    UWideString theText(0x400);
    EEPlayer* thePlayer = gWorld.GetPlayerBySlot(m->GetOriginatingSlot());
    gWorld.SetDiplomaticStance(thePlayer->mPlayerID, m->Get<int32_t>(1), m->Get<int32_t>(2));
    gResource->GetString(IDS_HOSTILE, theAlly);
    gResource->GetString(IDS_FRIENDLY, theEnemy);
    gResource->GetString(IDS_CHANGES_DIPLOMATIC_STANCE_TO, theIsNow);
    sprintf(theText, "%s %s %s", (const char*)thePlayer->mName, (const char*)theIsNow,
            (const char*)(m->Get<int32_t>(2) == 0 ? theEnemy : theAlly));
    gUserInterface->AddChatLogEntry(thePlayer->mColor, thePlayer->mName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
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

// FUNCTION: EXE 0x4c74da
// Packs the water-level message (header, selection id, level byte) into a new send buffer

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

// New (R5b.4): the two farm sorts for the host (ee/EEUnitHost.h), here with their comparators
void EESortFarms(std::vector<EEComplexUnit*>& ioFarms, long inX, long inY, bool inGreater)
{
    if (inGreater)
        std::sort(ioFarms.begin(), ioFarms.end(), EEFarmDistanceGreater(EETileCoordinate(inX, inY)));
    else
        std::sort(ioFarms.begin(), ioFarms.end(), EETileDistanceLess(EETileCoordinate(inX, inY)));
}

// FUNCTION: EXE 0x4c7cdc (EEMStartLoading::Execute)
// Starts loading the game: seeds the random generator, sets the human player count and multiplayer flag (members:
// the humans, the seed). Not static: the legacy harness's seed pin breaks here and writes the seed into the record
// (tools/build.py HARNESS_HOOKS, tools/harness/game_session.py).
void EEStartLoading(EEMRecord* m)
{
    if (gPlatSimSeedSet)   // TEST ONLY: the headless simulation's seed (platform/PlatSim.h; the harness's --seed pin)
        m->Set<int32_t>(2, (int32_t)gPlatSimSeed);
    gGameOptions.mLock->ReadLock();
    gGameOptions.Fn130272();
    gGameOptions.mLock->ReadUnlock();
    gPhysics->mRandom.SetSeed(m->Get<int32_t>(2));
    gUserInterface->mRandomSeed = m->Get<int32_t>(2);
    gCommunications->SetNumHumanPlayers(m->Get<unsigned char>(1));
    gCommunications->SetIsMultiplayerSession(m->Get<unsigned char>(1) > 1);
    gUserInterface->PostStartLoading();
}

// FUNCTION: EXE 0x4c8ae4
// Queues the improvement for the unit's type if its owner can buy it; otherwise the local owner hears an error
// click and sees the reason in the chat log
static void ExecuteImproveUnit(EEMRecord* m)
{
    UWideString theText;
    EEUnit* theUnit = gWorld.GetUnit(m->Get<int32_t>(1));
    if (theUnit && theUnit->mHitPoints > 0) {
        EEPlayer* theOwner = theUnit->mPlayer;
        if (theOwner->CanBuy(theUnit->mUnitClass->GetUnitType(), m->Get<int32_t>(2), &theText))
            theOwner->QueueImprovement(theUnit->mUnitClass->mUnitType, m->Get<int32_t>(2), 0, 0);
        else {
            if (theOwner == gWorld.GetCurrentPlayer())
                gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
            if (theText.GetLength() > 0 && theOwner == gWorld.GetCurrentPlayer())
                gUserInterface->AddChatLogEntry(theText, 0);
        }
    }
}

// Inline members of other headers whose first user is in this unit (the original's message classes' import ctors,
// labels and ??_G at 0xba41b..0xbadd0 have no counterpart: every game message is an EEMRecord)
// SYNTHETIC: EXE 0x4ba41b   ??0EEMSelectUnits@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba47b   ??0EEMAddPatrolPoint@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba4be   ??0EEMAddUnitToSelectedGroup@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba4fd   ??0EEMBuildGate@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba53a   ??0EEMBuildUnit@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba577   ??0EEMBuildWall@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba5be   ??0EEMCameraLocationInfo@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba606   ??0EEMChat@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba64a   ??_GEEMChat@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba684   ??0EEMCivUpgrades@@QAE@XZ
// SYNTHETIC: EXE 0x4ba6e1   ??_GEEMCivUpgrades@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba6fd   ??1EEMCivUpgrades@@UAE@XZ
// SYNTHETIC: EXE 0x4ba727   ??_GEEMCivUpgradesPurchased@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba749   ??_GEEMCivUpgradesRequested@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba789   ??0EEMExchangeResources@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba7c2   ??0EEMGameOptions@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba806   ??_GEEMGameOptions@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba83a   ??0EEMFullGameOptions@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba87e   ??0EEMHackANewUnit@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba8cd   ??0EEMInGameChat@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba911   ??_GEEMInGameChat@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4ba957   ??0EEMLoadOrSaveGame@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4ba9b9   ??_GEEMLoadOrSaveGame@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4baa2f   ??0EEMFullPlayerInformation@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4baa73   ??0EEMPlayerSlotInfo@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4baabd   ??_GEEMFullGameOptions@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4baaf1   ??0EEMSelectTheseUnits@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bab3d   ??_GEEMSelectTheseUnits@@UAEPAXI@Z
// SYNTHETIC: EXE 0x4bab76   ??0EEMSetAirportCommand@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4babbf   ??0EEMSetRallyGoal@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bac1a   ??0EEMSnapToDaylight@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bac53   ??0EEMToggleCollisionDetection@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bac8c   ??0EEMToggleDayNight@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bacc5   ??0EEMUnselectAllUnits@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bacf8   ??0EEMUpdatePingStat@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bad2b   ??0EEMImproveUnit@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bad5e   ??0EEMZoomInfo@@QAE@PAE@Z
// SYNTHETIC: EXE 0x4bad97   ??0EEMSetUnitBehavior@@QAE@PAE@Z
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

// New (game messages lane, round 5): the record types' Execute bodies are Rust (ee_netmsg bodies.rs): it reads the
// members, makes the decisions and asks here for each world call (EE_NETMSG_OP_*: an object, a fact, an action); the
// unit orders go to the units lane's bodies. The bodies that stay here are the lobby's, the files', the strings' and
// the screens' (kNetMsgBodies).
// the bodies the C++ keeps (EE_NETMSG_BODY_*, in its order)
static void (*const kNetMsgBodies[])(EEMRecord* m) = {
    ExecuteCivUpgradesPurchased, ExecuteCivUpgradesRequested, ExecuteImmediateCargoUnload, ExecuteImproveUnit,
    ExecuteLoadMainScreen, ExecuteLoadOrSaveGame, ExecuteLoadUnitData, ExecuteMigrationSlotInfo, ExecuteSavedGroupSelect,
    ExecuteSendFlare, ExecuteSetDiplomaticStance, EEStartLoading, ExecuteChangeFormation};
static const long kNetMsgTexts[] = {IDS_WARNING_YOU_ARE_RUNNING_OUT, IDS_CIVILIZATION_SELECTION_TIME_HAS_EXPIRED,
                                    IDS_YOU_MAY_NOT_UNPACK_ALL_UNITS, IDS_SETUP_ALL, IDS_SETUP_ALLIES, IDS_SETUP_ENEMIES,
                                    IDS_SETUP_CUSTOM, IDS_OBSERVERS};
static const long kNetMsgSounds[] = {kSoundClockTick, kSoundCashRegister};

// the world's actions (EE_NETMSG_OP_WORLD_ACT): the lobby's, the loading's, the chat's, the world's one calls
static void EENetMsgWorldAct(EEMRecord* m, const int64_t* i, const float* f, EEPlayer* a, EEPlayer* b)
{
    EEServer* theServer = static_cast<EEServer*>(gCommunications);
    switch (i[0]) {
    case EE_NETMSG_WACT_ATTACK_PLAYERS: EEAttackPlayers(i[1], (unsigned char)i[2]); break;
    case EE_NETMSG_WACT_BUILD_GATE: {
        EETileCoordinate theTile;
        theTile.mX = (long)f[0];
        theTile.mY = (long)f[1];
        EEBuildGate(theTile, i[1], i[2], i[3]);
        break;
    }
    case EE_NETMSG_WACT_EDICT: gWorld.mVoting.Propose((unsigned short)i[1], 15); gPhysics->mF84 = 0.0f; break;
    case EE_NETMSG_WACT_END_CINEMATIC: gWorld.mScenarioManager->EndCinematic(); break;
    case EE_NETMSG_WACT_GAME_SPEED: gWorld.SetGameSpeed(f[0]); break;
    case EE_NETMSG_WACT_MIGRATING_OFF:   // the host migration over as the game restarts
        if (gCommunications && gCommunications->IsClient())
            static_cast<EEClient*>(gCommunications)->mIsMigrating = false;
        else if (gCommunications)
            theServer->mIsMigrating = false;
        break;
    case EE_NETMSG_WACT_LAG: gWorld.mLagStatus = (unsigned short)i[1]; gUserInterface->SetLagStatus((unsigned short)i[1], (unsigned char)i[2]); break;
    case EE_NETMSG_WACT_LOAD_PERCENT: gUserInterface->SetLoadPercent((int32_t)i[1], (uint32_t)i[2]); break;
    case EE_NETMSG_WACT_POST_PHYSICS: gPhysics->mThread->Post(i[1], NULL, NULL, true); break;
    case EE_NETMSG_WACT_COLLISION_UNIT: gCollisionDetectionUnit = i[1]; break;
    case EE_NETMSG_WACT_COLLISION_FLAGS: gWorld.mCollisionDetectionFlags = (gWorld.mCollisionDetectionFlags + 1) % 16; break;
    case EE_NETMSG_WACT_ZOOM: gWorld.mCameraControl.SetZoom(f[0]); break;
    case EE_NETMSG_WACT_RECORD_FRAMES:   // slow or normal playback
        gGraphics->PostSetFlag5B(i[1] != 0);
        gPhysics->SetSlowPlayback(i[1] != 0);
        if (gCommunications->IsClient() && i[1])
            static_cast<NESynchronousClient*>(gCommunications)->SetSlowPlayback();
        else if (gCommunications->IsClient())
            static_cast<NESynchronousClient*>(gCommunications)->SetNormalPlayback();
        break;
    case EE_NETMSG_WACT_OPTIONS_LOCK:
        if (i[1])
            gGameOptions.mLock->WriteLock();
        else {
            gGameOptions.mLock->WriteUnlock();
            gUserInterface->PostLobbyUpdateGameOptions();
        }
        break;
    case EE_NETMSG_WACT_PLAYER_INFO: {   // lobby slot i1's field i2 (port: 6 a computer's engine) set to i3, the ping f0
        EEGameOptionsPlayer thePlayer;
        uint32_t theValue = (uint32_t)i[3];
        gGameOptions.mLock->WriteLock();
        thePlayer = gGameOptions.mPlayers[i[1]];
        switch (i[2]) {
        case kPlayerInfoEngine: thePlayer.mEngine = theValue < EECPEngineChoiceCount() ? theValue : (unsigned long)kEECPChoiceClassic; break;
        case 5: thePlayer.mIsReady = theValue > 0; break;
        case 4: thePlayer.mStartingCitizens = theValue; break;
        case 3: thePlayer.mTeam = theValue; break;
        case 2: thePlayer.mColor = theValue; break;
        case 1: thePlayer.mPing = f[0]; break;
        case 0: thePlayer.mType = theValue; break;
        }
        gGameOptions.mPlayers[i[1]] = thePlayer;
        gGameOptions.mLock->WriteUnlock();
        gUserInterface->PostLobbyUpdatePlayer(i[1], i[2]);
        break;
    }
    case EE_NETMSG_WACT_POST_CHAT: {
        EEAction* theAction = new EEAAddChatMessage(m->mString, i[1] != 0);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_NETMSG_WACT_TIMER_EXPIRED: {
        UIEvent* theEvent = new EEUIEProcessTimerExpired;
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_NETMSG_WACT_START_GAME: {   // every human has shown the main screen: the start at the offset time
        unsigned long theTime = TMGetTime();
        NEMessage* theMessage = new NEMRecord(kNetMessageStartGame, {theTime});
        theServer->mLoadState = 0;
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        theServer->SetGameState(2);
        break;
    }
    case EE_NETMSG_WACT_CLEAR_SEND_TO_SLOTS: theServer->ClearSendToSlotArray(); break;
    case EE_NETMSG_WACT_LOAD_RESPONSES: theServer->mNumLoadResponses = (unsigned char)i[1]; break;
    case EE_NETMSG_WACT_SEND_TO_SLOT: theServer->SetSendToSlotForPlayer((unsigned char)i[1], theServer->GetSendToSlot((unsigned char)i[2])); break;
    case EE_NETMSG_WACT_ALL_LOADED:   // every human has loaded: the slots sent, the main screen shown everywhere
        theServer->SendPlayerSlotInfo();
        theServer->mLoadState = 2;
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadMainScreen, {0}), kSlotNone);
        break;
    case EE_NETMSG_WACT_CHAT_LOG: EEGameLogChat(a->mPlayerID, (const char*)m->mString); break;   // port: the game log
    case EE_NETMSG_WACT_SHOW_CHAT: {   // the sender a's chat target's text (an observer's: the observers'), the line
        UWideString theType;
        for (int k = 1; k <= 2; k++)
            if (i[k] >= 0)
                gResource->GetString(kNetMsgTexts[i[k]], theType);
        if (theType.GetLength() != 0)
            theType = theType + " ";
        gUserInterface->AddChatLogEntry(a->mColor, a->mName, theType + m->mString, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
        EESoundObject* theSound = a != b ? EESoundObject::Get(kSoundIncomingChatMessage) : NULL;
        if (theSound)
            theSound->Play();
        break;
    }
    case EE_NETMSG_WACT_LAST_CHAT: gWorld.mScenarioManager->mLastChatText = m->mString; break;
    case EE_NETMSG_WACT_TRIBUTE_TEXT: {   // "<you tribute> <to> " (viewer 1) or "<from> <tributes you> ", the amounts
        static const long kNames[7] = {0, 0, IDS_RESOURCE_NAME_FOOD, IDS_RESOURCE_NAME_WOOD, IDS_RESOURCE_NAME_STONE,
                                       IDS_RESOURCE_NAME_GOLD, IDS_RESOURCE_NAME_IRON};
        UWideString theYou;
        UWideString theName;
        UWideString theText(0x400);
        char theNumber[8];
        gResource->GetString(i[1] == 1 ? IDS_YOU_TRIBUTE : IDS_TRIBUTES_YOU, theYou);
        if (i[1] == 1)
            sprintf(theText, "%s %s ", (const char*)theYou, (const char*)b->mName);
        else
            sprintf(theText, "%s %s ", (const char*)a->mName, (const char*)theYou);
        for (long k = 0; k < i[2]; k++) {
            long r = (long)((i[3] >> (8 * k)) & 0xff);   // wood's name is carbon's on the carbon maps
            gResource->GetString(r == 3 && gWorld.mUsingCarbon ? IDS_CARBON : kNames[r], theName);
            PlatIToA((uint32_t)i[4 + r - 2], theNumber, 10);
            theText += theNumber;
            theText += " ";
            theText += theName;
            theText += " ";
        }
        gUserInterface->AddChatLogEntry(a->mColor, a->mName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
        break;
    }
    case EE_NETMSG_WACT_RESIGN_TEXT: {   // "<name> has resigned" for the others
        UWideString theText;
        UWideString theFormat;
        UWideString theName;
        gResource->GetString(IDS_PLAYER_HAS_RESIGNED_FROM_GAME, theFormat);
        sprintf(theText, theFormat, (const char*)a->mName);
        gResource->GetString(IDS_MSG_GAME, theName);
        gUserInterface->AddChatLogEntry(GEColor(1.0f, 1.0f, 1.0f), theName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        break;
    }
    case EE_NETMSG_WACT_FLUSH: gPhysics->FlushWorld(); break;
    }
}

static int64_t EENetMsgHost(uint32_t inOp, EeNetMsgCall* c)
{
    EEMRecord* m = (EEMRecord*)(uintptr_t)c->message;
    EEPlayer* p = (EEPlayer*)(uintptr_t)c->a;
    EEGroup* g = (EEGroup*)(uintptr_t)c->a;
    EEComplexUnit* u = (EEComplexUnit*)(uintptr_t)c->b;   // an EEUnit* when a world lookup gave it
    int64_t* i = c->i;
    switch (inOp) {
    case EE_NETMSG_OP_SENDER: return (int64_t)(uintptr_t)gWorld.GetPlayerBySlot(m->GetOriginatingSlot());
    case EE_NETMSG_OP_PLAYER: return (int64_t)(uintptr_t)gWorld.mPlayers[i[0]];
    case EE_NETMSG_OP_CURRENT: return (int64_t)(uintptr_t)gWorld.GetCurrentPlayer();
    case EE_NETMSG_OP_GROUP: return (int64_t)(uintptr_t)static_cast<EEGroup*>(p->mActiveGroup);
    case EE_NETMSG_OP_SAVED_GROUP: return (int64_t)(uintptr_t)static_cast<EEGroup*>(p->mGroups[i[0]]);
    case EE_NETMSG_OP_GROUP_COUNT: return g->GetNumUnits();
    case EE_NETMSG_OP_GROUP_UNIT: return (int64_t)(uintptr_t)g->GetUnit(i[0]);
    case EE_NETMSG_OP_GROUP_FIRST: return (int64_t)(uintptr_t)g->GetFirstUnit();
    case EE_NETMSG_OP_GROUP_CONTAINS: return g->Contains(u);
    case EE_NETMSG_OP_GROUP_ADD: g->Add(u); return 0;
    case EE_NETMSG_OP_GROUP_CLEAR: g->Clear(); return 0;
    case EE_NETMSG_OP_UNIT: return (int64_t)(uintptr_t)gWorld.GetUnit(i[0]);
    case EE_NETMSG_OP_UNIT_FACT:
        switch (i[0]) {
        case EE_NETMSG_UNIT_HIT_POINTS: return u->mHitPoints;
        case EE_NETMSG_UNIT_TYPE: return u->mUnitClass->mUnitType;
        case EE_NETMSG_UNIT_CATEGORY: return u->GetUnitClass()->mCategory;
        case EE_NETMSG_UNIT_MAX_HIT_POINTS: return u->mStats->mMaxHitPoints;
        case EE_NETMSG_UNIT_CAN_DISBAND: return u->mUnitClass->mCanDisband;
        case EE_NETMSG_UNIT_IS_PACKED: return u->mIsPacked;
        case EE_NETMSG_UNIT_PACKED_CLASS: return (int64_t)(uintptr_t)u->mUnitClass->mPackedClass;
        case EE_NETMSG_UNIT_FLIGHT_TIME: return u->mFlightTime;
        case EE_NETMSG_UNIT_DEATH_SPAWN: return u->mUnitClass->mDeathSpawnClass != NULL;
        case EE_NETMSG_UNIT_CAN_CHANGE_BEHAVIOR: return u->mUnitClass->mCanChangeBehavior;
        case EE_NETMSG_UNIT_COMPLEX: return (int64_t)(uintptr_t)u->mComplexUnit;
        case EE_NETMSG_UNIT_TILE_NODE: return u->mTileNode != NULL;
        case EE_NETMSG_UNIT_OWNER: return (int64_t)(uintptr_t)u->mPlayer;
        case EE_NETMSG_UNIT_LAND_BUILDING: return u->mUnitClass->IsLandBuilding();
        case EE_NETMSG_UNIT_PAID_FOR: return u->mIsPaidFor;
        case EE_NETMSG_UNIT_BUILT: return u->mIsBuilt;
        case EE_NETMSG_UNIT_CAN_DELETE: return u->CanDelete();
        case EE_NETMSG_UNIT_TILE_X: return u->GetTileX();
        case EE_NETMSG_UNIT_TILE_Y: return u->GetTileY();
        case EE_NETMSG_UNIT_DOMAINS: return u->mUnitClass->mDomains;
        case EE_NETMSG_UNIT_NEXT: return (int64_t)(uintptr_t)u->mNext;
        case EE_NETMSG_UNIT_AS_COMPLEX: return (int64_t)(uintptr_t)EEUnitCast<EEComplexUnit>(static_cast<EEUnit*>(u));
        case EE_NETMSG_UNIT_IN_SET: return gUnitSetManager.GetSet(i[1])->Contains(u->mUnitClass->mUnitType);
        }
        return 0;
    case EE_NETMSG_OP_PLAYER_FACT:
        switch (i[0]) {
        case EE_NETMSG_PLAYER_OBSERVER: return p->mObserverMode;
        case EE_NETMSG_PLAYER_CIV_CHOSEN: return p->mCivilizationChosen;
        case EE_NETMSG_PLAYER_SLOT: return p->mSlot;
        case EE_NETMSG_PLAYER_CHAT_TYPE: return p->mChatType;
        case EE_NETMSG_PLAYER_ENEMY: return p->mEnemies[i[1]];
        case EE_NETMSG_PLAYER_SENDS_CHAT: return p->mSendChatMessage[i[1]];
        case EE_NETMSG_PLAYER_ID: return p->mPlayerID;
        case EE_NETMSG_PLAYER_RESOURCE: return p->mResourceCaps[i[1]];
        case EE_NETMSG_PLAYER_FIRST_UNIT: return (int64_t)(uintptr_t)p->mFirstUnit;
        case EE_NETMSG_PLAYER_TRADE_PRICE: return EEGetTradePrice(p, (long)i[1], (unsigned char)i[2]);
        }
        return 0;
    case EE_NETMSG_OP_WORLD_FACT:
        switch (i[0]) {
        case EE_NETMSG_WORLD_NUM_PLAYERS: return gWorld.mNumPlayers;
        case EE_NETMSG_WORLD_CURRENT_INDEX: return gWorld.mCurrentPlayer;
        case EE_NETMSG_WORLD_EXECUTING_SELECTION:
            return (gCommunications->IsClient() && static_cast<EEClient*>(gCommunications)->mIsExecutingSelection)
                   || (gCommunications->IsServer() && static_cast<EEServer*>(gCommunications)->mIsExecutingSelection);
        case EE_NETMSG_WORLD_IS_SERVER: return gCommunications->IsServer();
        case EE_NETMSG_WORLD_HAS_COMMUNICATIONS: return gCommunications != NULL;
        case EE_NETMSG_WORLD_LOCAL_SEND_SLOT:
            return gCommunications->IsClient() == true ? static_cast<EEClient*>(gCommunications)->GetMySendToSlot() : 0xfe;
        case EE_NETMSG_WORLD_STRING_LENGTH: return m->mString.GetLength();
        case EE_NETMSG_WORLD_PLAYBACK:
            return (gCommunications->IsClient() == true
                    && static_cast<NESynchronousClient*>(gCommunications)->GetRecordPlaybackMode() == kRecordModePlaying)
                   || (gCommunications->IsServer() == true
                       && static_cast<NESynchronousServer*>(gCommunications)->GetRecordPlaybackMode() == kRecordModePlaying);
        case EE_NETMSG_WORLD_COLLISION_SPHERE_MODE: return gCollisionSphereMode;
        case EE_NETMSG_WORLD_LOADED_HUMANS:
            return static_cast<EEServer*>(gCommunications)->GetNumHumanPlayers() - static_cast<EEServer*>(gCommunications)->mNumDroppedLoaders;
        case EE_NETMSG_WORLD_LOAD_RESPONSES: return static_cast<EEServer*>(gCommunications)->mNumLoadResponses;
        case EE_NETMSG_WORLD_HUMANS: return static_cast<EEServer*>(gCommunications)->GetNumHumanPlayers();
        }
        return 0;
    case EE_NETMSG_OP_DECISION: EEMessageCaptureDecision((unsigned int)i[0], (const long long*)&i[2], (unsigned int)i[1]); return 0;
    case EE_NETMSG_OP_CHAT_TEXT: {
        UWideString theText;
        gResource->GetString(kNetMsgTexts[i[0]], theText);
        if (!i[2] || theText.GetLength() > 0)
            gUserInterface->AddChatLogEntry(theText, i[1]);
        return 0;
    }
    case EE_NETMSG_OP_SOUND:
        if (EESoundObject* theSound = EESoundObject::Get(kNetMsgSounds[i[0]]))
            theSound->Play();
        return 0;
    case EE_NETMSG_OP_BODY: kNetMsgBodies[i[0]](m); return 0;
    case EE_NETMSG_OP_SELECT_LIST: {
        std::vector<long> theIDs((const int32_t*)(uintptr_t)c->b, (const int32_t*)(uintptr_t)c->b + i[1]);
        if (i[0] == 0)
            p->AddAndRemoveUnitsFromSelectedGroup(theIDs);
        else if (i[0] == 1)
            p->AddUnitsToSelectedGroup(theIDs);
        else
            p->SelectUnits(theIDs);
        return 0;
    }
    case EE_NETMSG_OP_SELECT:
        switch (i[0]) {
        case 0: p->RemoveUnitFromSelectedGroup(i[1]); break;
        case 1: p->AddUnitToSelectedGroup(i[1], true); break;
        case 2: p->SelectUnit(i[1], true, true); break;
        default: p->UnselectAllUnits(); break;
        }
        return 0;
    case EE_NETMSG_OP_RESOURCE:
        switch (i[0]) {
        case 0: p->AddResource(i[1], i[2]); break;
        case 1: p->TakeResource(i[1], i[2]); break;
        case 2: p->mResourceTrade[i[1] - 2] += (long)i[2]; break;
        default: p->AddTribute(i[3], i[1], i[2]); break;
        }
        return 0;
    case EE_NETMSG_OP_KILL: u->Kill(i[0], 0, 0); return 0;
    case EE_NETMSG_OP_DISBAND:
        p->RefundTechnology(p->mTechnologies->Get(u->mUnitClass->mTechnology), u->mUnitClass->mUnitType, c->f[0]);
        gEffectManager->CreateUnitEffect(u, EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter), NULL, 1, 0, 0);
        u->Kill(0, 0, 0);
        return 0;
    case EE_NETMSG_OP_DELETE_REFUND:
        if (EETechnology* theTechnology = u->GetPlayer()->GetTechTree()->Get(u->GetUnitClass()->GetTechnology()))
            u->mPlayer->RefundTechnology(theTechnology, 0x20, gWorldTable.GetFloatValue(kWorldDeleteRefundFactor));
        return 0;
    case EE_NETMSG_OP_UNIT_ACT: {
        EEUnitClass* theClass = (EEUnitClass*)(uintptr_t)c->a;
        switch (i[0]) {
        case EE_NETMSG_ACT_HALT: u->ClearGoals(0); u->StartAction(kActionIdle); u->mUnitClass->ActionChanged(u); break;
        case EE_NETMSG_ACT_GO_FANATIC: u->GoFanatic(); break;
        case EE_NETMSG_ACT_JIT: u->ToggleJITManufacturing(); u->UpdateJITManufacturing(); break;
        case EE_NETMSG_ACT_JIT_TOGGLE: u->ToggleJITManufacturing(); break;
        case EE_NETMSG_ACT_REMOVE_RESEARCH: u->RemoveResearch(i[1], 0); break;
        case EE_NETMSG_ACT_LOCK_GATE: {   // a gate built as one piece (scenario Create effect) has no second or fourth part
            EEUCGate* theGate = EEUnitClassCast<EEUCGate>(u->GetUnitClass());
            u->SetLocked(i[1] != 0);
            if (EEComplexUnit* thePart = theGate->GetSecondPart(u))
                thePart->SetLocked(i[1] != 0);
            if (EEComplexUnit* thePart = theGate->GetFourthPart(u))
                thePart->SetLocked(i[1] != 0);
            break;
        }
        case EE_NETMSG_ACT_RALLY_TIME:
            if (u->mGoalQueues[i[1]].mGoals.size() > 0)
                u->mGoalQueues[i[1]].AddTime(60000);
            break;
        case EE_NETMSG_ACT_BEHAVIOR:
            if (u->mBehavior != EEUnitBehavior::sTable[i[1]])
                u->mBehavior = EEUnitBehavior::sTable[i[1]];
            break;
        case EE_NETMSG_ACT_REPLANT: u->mUnitClass->ReplantFarm(u, (EEPlayer*)(uintptr_t)c->a); break;
        case EE_NETMSG_ACT_HOME_BASE: u->SetHomeBase((EEComplexUnit*)(uintptr_t)c->a); break;
        case EE_NETMSG_ACT_LAND: u->SetFlightTime(0); break;
        case EE_NETMSG_ACT_SCRAMBLE: {
            EEUGRust* theGoal = EEUGRust::NewLocation(GE2DPoint(long(i[1]), long(i[2])), false, false, false, 0.0f, false, false);
            u->ClearAllGoalQueues();
            u->QueueGoal(theGoal, 0);
            u->QueueGoal(theGoal->Clone(), 1);
            u->mGoalQueues[1].AddTime(1000);
            break;
        }
        case EE_NETMSG_ACT_PACK: EEReplaceUnit(u, i[1], theClass)->SetIsPacked(true); break;
        case EE_NETMSG_ACT_UNPACK: {
            EEComplexUnit* theNew = EEReplaceUnit(u, i[1], theClass);
            EEPlaceUnit(theNew, theNew->mUnitClass, (EEPlayer*)(uintptr_t)i[2], U2DPoint<long>(u->mTileX, u->mTileY), true);
            theNew->SetIsPacked(false);
            break;
        }
        case EE_NETMSG_ACT_SPHERE:   // grown or shrunk by 0.05, the inner (mode 9) or the outer (10)
            if (i[1] == 9)
                u->mUnitClass->mInnerCollisionSphere += i[2] ? 0.05f : -0.05f;
            else
                u->mUnitClass->mOuterCollisionSphere += i[2] ? 0.05f : -0.05f;
            u->mCollisionRadius = (float)u->mUnitClass->mInnerCollisionSphere;
            break;
        case EE_NETMSG_ACT_RADIUS: u->mCollisionRadius = (float)((EEComplexUnit*)(uintptr_t)c->a)->mUnitClass->mInnerCollisionSphere; break;
        }
        return 0;
    }
    case EE_NETMSG_OP_PLAYER_ACT:
        switch (i[0]) {
        case EE_NETMSG_PACT_SHIFT_MODE:
            p->mShiftMode = i[1] != 0;
            if (p == gWorld.GetCurrentPlayer())
                gMouse->mShiftMode = i[1] != 0;
            break;
        case EE_NETMSG_PACT_SEES_ALL:
            p->mSeesAll = !p->mSeesAll;
            if (p == gWorld.GetCurrentPlayer())
                gMiniMap->Redraw();
            break;
        case EE_NETMSG_PACT_INCREMENT_EPIC: p->IncrementEpic(); break;
        case EE_NETMSG_PACT_JIT:
            p->mJITManufacturing = !p->mJITManufacturing;
        case EE_NETMSG_PACT_UPDATE_JIT:   // (JIT falls through)
            if (p == gWorld.GetCurrentPlayer())
                gUserInterface->UpdateJITManufacturing();
            break;
        case EE_NETMSG_PACT_CHEAT: gCheatCodes->ApplyCheatCode(i[1], p->mPlayerID, i[2]); break;
        case EE_NETMSG_PACT_CHAT_TYPE: p->mChatType = i[1]; break;
        case EE_NETMSG_PACT_SEND_CHAT: p->mSendChatMessage[i[1]] = i[2] != 0; break;
        case EE_NETMSG_PACT_REPORT: p->mSavedGroups.Set(0x14, i[1]); p->mSavedGroups.Set(0x15, i[2]); break;
        case EE_NETMSG_PACT_CLEAR_SAVED: p->ClearSavedGroup((unsigned char)i[1]); break;
        case EE_NETMSG_PACT_ASSIGN_SAVED: p->mGroups[i[1]]->Assign(p->mActiveGroup); break;
        case EE_NETMSG_PACT_ACKNOWLEDGE: p->PlayAcknowledgement((EEComplexUnit*)(uintptr_t)i[1]); break;
        case EE_NETMSG_PACT_LOCK_CIV: p->mCivilizationLocked = true; break;
        case EE_NETMSG_PACT_REMOVE: gWorld.RemovePlayerFromGame(0, p->mPlayerID); p->SetGameResult(2); break;
        case EE_NETMSG_PACT_REMOVE_IMPROVEMENT: p->RemoveQueuedImprovement(i[1], i[2], p->CountQueuedImprovements(i[1], i[2]) != 0); break;
        case EE_NETMSG_PACT_UPDATE_HELP:
            if (gWorld.mPlayers[gWorld.mCurrentPlayer]->mPlayerID == p->mPlayerID)
                gUIManager->mMainGameForm->UpdateHelpString();
            break;
        case EE_NETMSG_PACT_PATROL_GROUP:
            if (EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(i[1])))
                theUnit->mPatrolGroup->Select(p, true);
            break;
        }
        return 0;
    case EE_NETMSG_OP_WORLD_ACT: EENetMsgWorldAct(m, i, c->f, p, (EEPlayer*)(uintptr_t)c->b); return 0;
    case EE_NETMSG_OP_OPTION:
        switch (i[0]) {
        case 0: gGameOptions.mLoadType = i[1]; break;
        case 1: gGameOptions.mMapSize = i[1]; break;
        case 2: gGameOptions.mMapType = m->mString; break;
        case 3: gGameOptions.mResources = i[1]; break;
        case 4: gGameOptions.mEpochStart = i[1]; break;
        case 5: gGameOptions.mEpochEnd = i[1]; break;
        case 6: gGameOptions.mUnitLimit = i[1]; break;
        case 7: gGameOptions.mWondersForVictory = i[1]; break;
        case 8: gGameOptions.mVariant = i[1]; break;
        case 9: gGameOptions.mDifficulty = i[1]; break;
        case 10: gGameOptions.mGameSpeed = i[1]; break;
        case 11: gGameOptions.mRecordGames = i[1] != 0; break;
        case 12: gGameOptions.mAllowCustomCivs = i[1] != 0; break;
        case 13: gGameOptions.mRevealMap = i[1] != 0; break;
        case 14: gGameOptions.mLockTeams = i[1] != 0; break;
        case 15: gGameOptions.mLockSpeed = i[1] != 0; break;
        case 16: gGameOptions.mCheatCodes = i[1] != 0; break;
        case 17: gGameOptions.mVictory = i[1] != 0; break;
        }
        return 0;
    case EE_NETMSG_OP_CLASS_FACT: return i[0] == EE_NETMSG_CLASS_TYPE ? ((EEUnitClass*)(uintptr_t)c->a)->mUnitType : ((EEUnitClass*)(uintptr_t)c->a)->mTileSize;
    case EE_NETMSG_OP_SELECTION: {   // the communications object's fast-selection state (the server's or the client's)
        EEServer* theServer = gCommunications->IsServer() ? static_cast<EEServer*>(gCommunications) : NULL;
        EEClient* theClient = static_cast<EEClient*>(gCommunications);
        unsigned char* theLastIDs = theServer ? theServer->mFastSelection : theClient->mFastSelection;
        TSCriticalSection* theLock = theServer ? theServer->mFastSelectionLock : theClient->mFastSelectionLock;
        switch (i[0]) {
        case EE_NETMSG_SEL_LOCK: i[1] ? theLock->Enter() : theLock->Leave(); break;
        case EE_NETMSG_SEL_MARK_LOCAL: gWorld.mExecutingLocalSelection = i[1] != 0; break;
        case EE_NETMSG_SEL_CURRENT_ID: return theServer ? theServer->mFastSelectionID : theClient->mFastSelectionID;
        case EE_NETMSG_SEL_LAST_ID: return theLastIDs[(unsigned char)p->mPlayerID];
        case EE_NETMSG_SEL_SET_LAST_ID: theLastIDs[(unsigned char)p->mPlayerID] = (unsigned char)i[1]; break;
        case EE_NETMSG_SEL_COPY: p->mSelectedGroup->CopySelection(p->mActiveGroup); p->SetSelectedViewedUnit(p->mViewedUnit); break;
        case EE_NETMSG_SEL_RESET: p->UnselectAllUnits(); p->mSelectedGroup->CopySelection(p->mActiveGroup); break;
        case EE_NETMSG_SEL_JIT_OFF:
            if (p->mJITManufacturing) {
                p->mJITManufacturing = false;
                if (p == gWorld.GetCurrentPlayer())
                    gUserInterface->UpdateJITManufacturing();
            }
            break;
        }
        return 0;
    }
    case EE_NETMSG_OP_CAN_PLACE: {
        GE3DPoint theLocation((float)i[1], (float)i[2], 0.0f);
        return gWorld.mMap->GetTile(i[3], i[4]) ? EECanPlace(p, i[0], &theLocation, i[3], i[4], 0, 0) : false;
    }
    }
    return 0;
}

// the type's body (ee_netmsg bodies.rs) over the record and its list, with the host above: alone (Execute) or in the
// physics' framing (1: a selection, 2: a command)
static void EENetMsgExecute(EEMRecord* inMessage, uint8_t inFraming)
{
    ee_netmsg_execute(EENetMsgHost, inMessage, inMessage->GetType(), inMessage->GetOriginatingSlot(), inMessage->mFastSelectionID,
                      inMessage->mRecord, inMessage->mList.empty() ? NULL : &inMessage->mList[0], inMessage->mList.size(),
                      inFraming);
}

void EEMRecord::Execute()
{
    EENetMsgExecute(this, 0);
}
