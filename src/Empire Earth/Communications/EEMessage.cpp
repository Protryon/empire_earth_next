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
#include "lle/NEMessage.h"
#include "lle/UResourceFile.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
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
#include "ee/EEARust.h"
#include "ee/EEUIEProcessTimerExpired.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEFormation.h"
#include "ee/EEPhysics.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUGRust.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
#include "ee/EEMessageCapture.h"
#include "rust/sim/netmsg.h"   // ee_netmsg
#include "rust/sim/units.h"
#include "rust/game/gamecpp.h"   // the bodies (gamecpp message.rs)

// Registers the game's message factory for every game message type (0x41..0xfd) with the network engine
void EEMessage::RegisterKnownMessages()
{
    NEMessage::RegisterBuilder(0x41, 0xfd, EEMessage::BuildEEMessage);
}

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

#include "ee/EEStringIDs.h"

// 0x23cc42 (cdecl): the exchange rate between a resource and the trade resource for inPlayer (provisional name)
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);

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

bool gLocalPlayerOut;                      // 0x4e6099 (.bss between this unit's Execute statics 0x4e6098/0x4e609a; ee/EEMessage.h)

#include "ee/EESaveLoad.h"
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);

extern long& gAutoSaveCount;                 // 0x5308b8 (the auto-save count LoadOrSave passes on; provisional name)
// (0x14cbc7 TrimAutoSaves: ee/EESaveLoad.h)

// Saves the game under the request's name (the old EEMLoadOrSave::LoadOrSave; Rust: gamecpp message.rs)
long EELoadOrSave(EEMRecord* m)
{
    return ee_gamecpp_msg_load_or_save(m);
}

// New (port), TEST ONLY: set by the UI fixture (EETestFixture.cpp, only with EE_TEST_UI) to see each box selection's
// decoded ids as its message is built on the UI thread; NULL otherwise
void (*gEESelectUnitsTestHook)(const std::vector<long>& inUnitIDs) = NULL;

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

// EEMSmartFarmers' sort predicate (provisional name): the farther farm first, with +0x34c, then +0x344 ordering
// the units; `this` is the EETileDistanceLess the distances are taken from.
struct EEFarmDistanceGreater : public EETileDistanceLess {
    EEFarmDistanceGreater(const EETileCoordinate& inCenter) : EETileDistanceLess(inCenter) {}
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const;
};

// NEAR: the original is built without optimization (ebp frame, `this` and every bool test through memory
// temporaries); `#pragma optimize("g", off)` alone does not reproduce it (59%), so it is not used

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

// Starts loading the game: seeds the random generator, sets the human player count and multiplayer flag (members:
// the humans, the seed). Not static: the legacy harness's seed pin breaks here and writes the seed into the record
// (tools/build.py HARNESS_HOOKS, tools/harness/game_session.py).
void EEStartLoading(EEMRecord* m)
{
    ee_gamecpp_msg_start_loading(m);
}

// New (game messages lane, round 5): the record types' Execute bodies are Rust (ee_netmsg bodies.rs): it reads the
// members, makes the decisions and asks here for each world call (EE_NETMSG_OP_*: an object, a fact, an action); the
// unit orders go to the units lane's bodies. The bodies the C++ kept (the lobby's, the files', the strings' and the
// screens') and the world's, the units' and the players' actions are Rust too (game core round 8: gamecpp message.rs,
// over EEMessageOp below); the chat texts (EE_NETMSG_OP_CHAT_TEXT, EE_NETMSG_WACT_SHOW_CHAT):
static const long kNetMsgTexts[] = {IDS_WARNING_YOU_ARE_RUNNING_OUT, IDS_CIVILIZATION_SELECTION_TIME_HAS_EXPIRED,
                                    IDS_YOU_MAY_NOT_UNPACK_ALL_UNITS, IDS_SETUP_ALL, IDS_SETUP_ALLIES, IDS_SETUP_ENEMIES,
                                    IDS_SETUP_CUSTOM, IDS_OBSERVERS};
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
    case EE_NETMSG_OP_DECISION: ee_netmsg_capture_decision((uint32_t)i[0], &i[2], (uint32_t)i[1]); return 0;
    case EE_NETMSG_OP_CHAT_TEXT: ee_gamecpp_msg_chat_text(i[0], i[1], i[2]); return 0;
    case EE_NETMSG_OP_SOUND: ee_gamecpp_msg_sound(i[0]); return 0;
    case EE_NETMSG_OP_BODY: ee_gamecpp_msg_body(i[0], m); return 0;
    case EE_NETMSG_OP_SELECT_LIST: ee_gamecpp_msg_select_list(p, i[0], (const int32_t*)(uintptr_t)c->b, i[1]); return 0;
    case EE_NETMSG_OP_SELECT: ee_gamecpp_msg_select(p, i[0], i[1]); return 0;
    case EE_NETMSG_OP_RESOURCE: ee_gamecpp_msg_resource(p, i[0], i[1], i[2], i[3]); return 0;
    case EE_NETMSG_OP_KILL: u->Kill(i[0], 0, 0); return 0;
    case EE_NETMSG_OP_DISBAND: ee_gamecpp_msg_disband(p, u, c->f[0]); return 0;
    case EE_NETMSG_OP_DELETE_REFUND: ee_gamecpp_msg_delete_refund(u); return 0;
    case EE_NETMSG_OP_UNIT_ACT: ee_gamecpp_msg_unit_act(i, (void*)(uintptr_t)c->a, u); return 0;
    case EE_NETMSG_OP_PLAYER_ACT: ee_gamecpp_msg_player_act(i, p); return 0;
    case EE_NETMSG_OP_WORLD_ACT: ee_gamecpp_msg_world_act(m, i, c->f, p, (EEPlayer*)(uintptr_t)c->b); return 0;
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
    case EE_NETMSG_OP_SELECTION: return ee_gamecpp_msg_selection(i, p);
    case EE_NETMSG_OP_CAN_PLACE: return ee_gamecpp_msg_can_place(p, i[0], i[1], i[2], i[3], i[4]);
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

// (moved from EEClient.cpp in game messages round 6, unchanged: the command messages' local markers; EEClient.cpp
// keeps the client)
// Shows the local command markers of a group order (move, attack, target, unload) on the goal the units just
// queued, else on the first unit whose last goal fits
// NEAR: the original refuses vector::operator[]'s begin()/operator+ in the first loop (more inline pressure than
// ours), mL29C compares unsigned (jb; EEUnit.h has long)
// effects 0xb8 move, 0xb9 attack, 0xba target, 0xbb unload
void EESendGroupCommand(EEGroup* inGroup, EEUnit* inTarget, unsigned long inPlayerID, long inCommand, long inGoalQueue)
{
    ee_gamecpp_msg_group_command(inGroup, inTarget, inPlayerID, inCommand, inGoalQueue);
}

// ------------------------------------------------------------------------- the message bodies' C++ (gamecpp message.rs)


int64_t EEMessageOpNext(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEMessageOpNextF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the constants message.rs passes
static_assert(kGraphicEffectCommandConfirmationMove == 0xb8 && kGraphicEffectCommandConfirmationAttack == 0xb9 && kGraphicEffectCommandConfirmationGatherOther == 0xba && kGraphicEffectDisbandUnitEmitter == 0xbc && kAIKindAircraftCarrier == 0xf && kAIKindSpaceCarrier == 0x27 && kUnitTypeAirport == 0x42 && kGoalUnloadTransport == 8 && kUnitCategoryFortress == 0x1d && kActionIdle == 5000 && kSoundButton2 == 0x208 && kSoundIncomingChatMessage == 0x3ae && kSoundClockTick == 0x3a8 && kSoundCashRegister == 0xe && kEECPChoiceClassic == 0, "gamecpp message.rs");
static_assert(IDS_LOADING == 30663 && IDS_SAVING_GAME_AS == 30661 && IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME == 30612 && IDS_NEW_HOST_ESTABLISHED == 30634 && IDS_HOSTILE == 29514 && IDS_FRIENDLY == 29513 && IDS_CHANGES_DIPLOMATIC_STANCE_TO == 31601 && IDS_YOU_TRIBUTE == 31602 && IDS_TRIBUTES_YOU == 31600 && IDS_CARBON == 30121 && IDS_PLAYER_HAS_RESIGNED_FROM_GAME == 31808 && IDS_MSG_GAME == 31813 && IDS_RESOURCE_NAME_FOOD == 30116 && IDS_RESOURCE_NAME_IRON == 30120, "gamecpp message.rs");
static_assert(sizeof(EETileCoordinate) == 2 * sizeof(long) && sizeof(GE2DPoint) == 2 * sizeof(float), "gamecpp message.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define MSG ((EEMRecord*)p)
#define PL ((EEPlayer*)p)
#define UN ((EEUnit*)p)
#define CX ((EEComplexUnit*)p)
#define UC ((EEUnitClass*)p)
#define GOAL ((EEUnitGoal*)p)
#define SEL ((EESelectedGroup*)p)
#define WSTR (*(UWideString*)p)
#define WSTRA(i) (*(UWideString*)a[i])
#define SERVER static_cast<EEServer*>(gCommunications)
#define CLIENT static_cast<EEClient*>(gCommunications)
#define MAIN_FORM gUIManager->GetMainGameForm()

// The message bodies' C++ calls and members (EeGameCpp's message section; EEGameState.cpp's EEGameStateOp default):
// one statement each
int64_t EEMessageOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the records
    case O::MsgField: return P64(MSG->Field((uint32_t)a[0], (size_t)a[1]));
    case O::MsgSender: return P64(gWorld.GetPlayerBySlot(MSG->GetOriginatingSlot()));
    case O::MsgOrigin: return MSG->GetOriginatingSlot();
    case O::MsgString: return P64(&MSG->mString);
    case O::MsgSetOrigin: MSG->SetOriginatingSlot((unsigned char)a[0]);
        break;
    case O::MsgStartLoading: EEStartLoading(MSG);
        break;
    case O::MsgLoadSavedGame: return EELoadSavedGameFile(UString(MSG->mString), true, a[0] != 0, UString("data\\Saved Games\\"), 1);
    case O::MsgSaveGame: return SaveGame(UString(MSG->mString), (unsigned long)a[0], a[1] != 0, UString("data\\Saved Games\\"));
    case O::TrimAutoSaves: TrimAutoSaves(UString("AutoSave") + UString("*") + gSavedGameExtension, UString("data\\") + UString("Saved Games\\"), gAutoSaveCount);
        break;
    case O::NewLoadSaveResponse: return P64(new EEMRecord(kMessageLoadOrSaveGameResponse, {(int32_t)a[0]}));
    case O::NewStartGameRecord: return P64(static_cast<NEMessage*>(new NEMRecord(kNetMessageStartGame, {(unsigned long)a[0]})));
    case O::NewLoadMainScreenRecord: return P64(static_cast<NEMessage*>(new EEMRecord(kMessageLoadMainScreen, {0})));
    case O::NewChatAction: return P64(static_cast<EEAction*>(new EEAAddChatMessage(MSG->mString, a[0] != 0)));
    case O::NewTimerExpiredEvent: return P64(static_cast<UIEvent*>(new EEUIEProcessTimerExpired));
    case O::VecCivUpgradesOfMsg: return P64(new std::vector<EECivUpgrade>(MSG->GetList<EECivUpgrade>()));
    case O::VecCivUpgradesDelete: delete (std::vector<EECivUpgrade>*)p;
        break;
    case O::CivRequestUpgrades: EECBRequestUpgrades((unsigned char)a[0], MSG->mString, *(std::vector<EECivUpgrade>*)a[1]);
        break;
    case O::VecLongNew: return P64(new std::vector<long>((const int32_t*)a[0], (const int32_t*)a[0] + a[1]));
    case O::VecLongDelete: delete (std::vector<long>*)p;
        break;
    case O::VecPlayersAt: return P64((*(std::vector<EEPlayer*>*)p)[a[0]]);
    // the strings and the chat log
    case O::StrNew1K: return P64(new UWideString(0x400));
    case O::StrAssignMsg: WSTR = ((EEMRecord*)a[0])->mString;
        break;
    case O::StrLength: return WSTR.GetLength();
    case O::StrFormatS: sprintf(WSTR, WSTRA(0), (const char*)WSTRA(1));
        break;
    case O::StrFormatSS: sprintf(WSTR, "%s %s ", (const char*)WSTRA(0), (const char*)WSTRA(1));
        break;
    case O::StrFormatSSS: sprintf(WSTR, "%s %s %s", (const char*)WSTRA(0), (const char*)WSTRA(1), (const char*)WSTRA(2));
        break;
    case O::StrAppendSpace: WSTR = WSTR + " ";
        break;
    case O::StrAppendC: WSTR += (const char*)a[0];
        break;
    case O::StrAppendStr: WSTR += WSTRA(0);
        break;
    case O::ResChatText: return gResource->GetString(kNetMsgTexts[a[0]], WSTR);
    case O::UiAddChat: gUserInterface->AddChatLogEntry(WSTR, (long)a[0]);
        break;
    case O::UiAddChatGameLine: gUserInterface->AddChatLogEntry(GEColor(1.0f, 1.0f, 1.0f), WSTRA(0), WSTR, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        break;
    case O::UiAddChatPlayerLine: gUserInterface->AddChatLogEntry(((EEPlayer*)a[0])->mColor, ((EEPlayer*)a[0])->mName, WSTR, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
        break;
    case O::UiAddChatMsgLine: gUserInterface->AddChatLogEntry(((EEPlayer*)a[0])->mColor, ((EEPlayer*)a[0])->mName, WSTR + ((EEMRecord*)a[1])->mString, gWorldTable.GetLongValue(kWorldChatLogDisplayTime), 0);
        break;
    case O::GameLogChat: EEGameLogChat((unsigned long)a[0], (const char*)MSG->mString);
        break;
    // the players
    case O::PlName: return P64(&PL->mName);
    case O::PlGetSlot: return PL->GetSlot();
    case O::PlActiveGroup: return P64(PL->mActiveGroup);
    case O::PlSelectedGroup: return P64(PL->mSelectedGroup);
    case O::PlSavedGroupAt: return P64(PL->mGroups[a[0]]);
    case O::PlCopyAllies: *(std::vector<EEPlayer*>*)a[0] = PL->mAllies;
        break;
    case O::PlAddEvent: PL->mEvents.AddEvent(U2DPoint<long>((long)a[0], (long)a[1]), (long)a[2], (EEPlayer*)a[3], (long)a[4], a[5] != 0);
        break;
    case O::PlFlareReceived: PL->FlareReceived((EEPlayer*)a[0], U2DPoint<long>((long)a[1], (long)a[2]));
        break;
    case O::PlCivAddUpgrades: PL->mCivilization.AddUpgrades(*(std::vector<EECivUpgrade>*)a[0], (long)a[1], true);
        break;
    case O::PlSetHackUnitClass: PL->SetHackUnitClass((long)a[0]);
        break;
    case O::PlSetGameResult: PL->SetGameResult((long)a[0]);
        break;
    case O::PlCanBuy: return PL->CanBuy((long)a[0], (long)a[1], (UWideString*)a[2]);
    case O::PlQueueImprovement: PL->QueueImprovement((unsigned long)a[0], (unsigned long)a[1], 0, 0);
        break;
    case O::PlAddAndRemoveSelectedIds: PL->AddAndRemoveUnitsFromSelectedGroup(*(std::vector<long>*)a[0]);
        break;
    case O::PlAddSelectedIds: PL->AddUnitsToSelectedGroup(*(std::vector<long>*)a[0]);
        break;
    case O::PlSelectIds: PL->SelectUnits(*(std::vector<long>*)a[0]);
        break;
    case O::PlRemoveSelectedId: PL->RemoveUnitFromSelectedGroup(a[0]);
        break;
    case O::PlAddSelectedId: PL->AddUnitToSelectedGroup(a[0], true);
        break;
    case O::PlSelectId: PL->SelectUnit(a[0], true, true);
        break;
    case O::PlUnselectAll: PL->UnselectAllUnits();
        break;
    case O::PlAddResource: PL->AddResource((long)a[0], (long)a[1]);
        break;
    case O::PlTakeResource: PL->TakeResource((long)a[0], (long)a[1]);
        break;
    case O::PlAddTribute: PL->AddTribute((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::PlRefundTechnology: PL->RefundTechnology((EETechnology*)a[0], (long)a[1], f[0]);
        break;
    case O::PlRefundDelete: PL->RefundTechnology((EETechnology*)a[0], 0x20, gWorldTable.GetFloatValue(kWorldDeleteRefundFactor));
        break;
    case O::PlIncrementEpic: PL->IncrementEpic();
        break;
    case O::PlSavedGroupsSet: PL->mSavedGroups.Set((long)a[0], (long)a[1]);
        break;
    case O::PlClearSavedGroup: PL->ClearSavedGroup((unsigned char)a[0]);
        break;
    case O::PlAssignSavedGroup: PL->mGroups[a[0]]->Assign(PL->mActiveGroup);
        break;
    case O::PlPlayAcknowledgement: PL->PlayAcknowledgement((EEComplexUnit*)a[0]);
        break;
    case O::PlCountQueuedImprovements: return PL->CountQueuedImprovements((unsigned long)a[0], (long)a[1]);
    case O::PlRemoveQueuedImprovement: PL->RemoveQueuedImprovement((long)a[0], (long)a[1], a[2] != 0);
        break;
    case O::PlSelectedCopyActive: PL->mSelectedGroup->CopySelection(PL->mActiveGroup);
        break;
    case O::PlSetSelectedViewedUnit: PL->SetSelectedViewedUnit(PL->mViewedUnit);
        break;
    case O::PlayForLocalSelectionSound: EEPlayForLocalPlayer(PL, ((EEComplexUnit*)a[0])->mStats->GetSelectionSound(-1));
        break;
    case O::PatrolGroupSelect: ee_units_pg_select(CX->mPatrolGroup, (EEPlayer*)a[0], true);
        break;
    case O::CheatApply: gCheatCodes->ApplyCheatCode((long)a[0], (unsigned long)a[1], (long)a[2]);
        break;
    case O::WCanBuyCivilization: return ((EEWorld*)p)->mGameOptions.CanBuyCivilization();
    case O::WRemovePlayerFromGame: ((EEWorld*)p)->RemovePlayerFromGame(a[0] != 0, (unsigned long)a[1]);
        break;
    case O::WSetDiplomaticStanceCall: ((EEWorld*)p)->SetDiplomaticStance((unsigned long)a[0], (long)a[1], (long)a[2]);
        break;
    // the groups and formations
    case O::GroupCount: return ((EEGroup*)p)->GetNumUnits();
    case O::GroupUnit: return P64(((EEGroup*)p)->GetUnit((unsigned long)a[0]));
    case O::GroupFormationType: return ((EEGroup*)p)->mFormation->mType;
    case O::GroupSetFormation: ((EEGroup*)p)->SetFormation((EEFormation*)a[0]);
        break;
    case O::SelGroupSetFormation: SEL->SetFormation((EEFormation*)a[0]);
        break;
    case O::SelGroupSavedGroup: return P64(SEL->mSavedGroup);
    case O::SelGroupSelectSaved: SEL->SelectSavedGroup((EEGroup*)a[0], a[1] != 0);
        break;
    case O::SelGroupFirstUnit: return P64(SEL->GetFirstUnit());
    case O::FormationCreate: return P64(EEFormation::Create((long)a[0]));
    case O::FormationSetB5C: ((EEFormation*)p)->mB5C = a[0] != 0;
        break;
    case O::FormationDereference: ((EEFormation*)p)->Dereference();
        break;
    // the units, their classes and goals
    case O::CxNumCargoUnits: return CX->GetNumCargoUnits();
    case O::CxHasCargo: return CX->HasCargo((EEComplexUnit*)a[0]);
    case O::CxUnloadCargoNow: return CX->UnloadCargoNow((EEComplexUnit*)a[0]);
    case O::CxGoFanatic: CX->GoFanatic();
        break;
    case O::CxToggleJit: CX->ToggleJITManufacturing();
        break;
    case O::CxUpdateJit: CX->UpdateJITManufacturing();
        break;
    case O::CxRemoveResearch: CX->RemoveResearch((long)a[0], 0);
        break;
    case O::CxSetLocked: CX->SetLocked(a[0] != 0);
        break;
    case O::CxQueueSize: return (int64_t)CX->mGoalQueues[a[0]].mGoals.size();
    case O::CxQueueBack: return P64(CX->mGoalQueues[a[0]].mGoals.back());
    case O::CxQueueAddTime: CX->mGoalQueues[a[0]].AddTime((unsigned long)a[1]);
        break;
    case O::CxBehavior: return P64(CX->mBehavior);
    case O::BehaviorAt: return P64(EEUnitBehavior::sTable[a[0]]);
    case O::CxSetBehavior: CX->mBehavior = (EEUnitBehavior*)a[0];
        break;
    case O::CxSetHomeBase: CX->SetHomeBase((EEComplexUnit*)a[0]);
        break;
    case O::CxClearAllGoalQueues: CX->ClearAllGoalQueues();
        break;
    case O::CxQueueGoal: CX->QueueGoal((EEUnitGoal*)a[0], (long)a[1]);
        break;
    case O::CxSetIsPacked: CX->SetIsPacked(a[0] != 0);
        break;
    case O::NewLocationGoal2D: return P64(static_cast<EEUnitGoal*>(EEUGRust::NewLocation(GE2DPoint(long(a[0]), long(a[1])), false, false, false, 0.0f, false, false)));
    case O::GoalClone: return P64(GOAL->Clone());
    case O::GoalCreationTime: return GOAL->GetCreationTime();
    case O::GoalHasLocation: return EEGoalHasLocation(GOAL);
    case O::GoalLocation: GOAL->GetGoalLocation(*(GE3DPoint*)a[0]);
        break;
    case O::GoalIsAttackUnit: return EEAttackUnitCast(GOAL) != NULL;
    case O::GoalIsAttackGround: return EEAttackGroundCast(GOAL) != NULL;
    case O::GoalIsBuildRepair: return EEBuildRepairGoalCast(GOAL) != NULL;
    case O::GoalIsGather: return EEGatherGoalCast(GOAL) != NULL;
    case O::GoalIsLoadGarrison: return EELoadGarrisonGoalCast(GOAL) != NULL;
    case O::GoalAsUnload: return P64(EEUnloadGoalCast(GOAL));
    case O::UnloadGoalTransport: return P64(((EEUGRust*)p)->GetUnloadTransport());
    case O::UnTreeTechnology: return P64(UN->GetPlayer()->GetTechTree()->Get(UN->GetUnitClass()->GetTechnology()));
    case O::UcIsTransport: return UC->IsTransport();
    case O::UcUnloadFailedText: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(UC)->GetUnloadFailedTextID();
    case O::UcAsGate: return P64(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(UC));
    case O::GateSecondPart: return P64(UC->GetSecondPart((EEComplexUnit*)a[0]));
    case O::GateFourthPart: return P64(UC->GetFourthPart((EEComplexUnit*)a[0]));
    case O::UcReplantFarm: UC->ReplantFarm((EEComplexUnit*)a[0], (EEPlayer*)a[1]);
        break;
    case O::ReplaceUnit: return P64(EEReplaceUnit(CX, (unsigned long)a[0], (EEUnitClass*)a[1]));
    case O::PlaceUnit: EEPlaceUnit(CX, (EEUnitClass*)a[0], (EEPlayer*)a[1], U2DPoint<long>((long)a[2], (long)a[3]), true);
        break;
    case O::CanPlace: return EECanPlace(PL, (long)a[0], (GE3DPoint*)a[1], (long)a[2], (long)a[3], 0, 0);
    case O::MapGroundPoint: gWorld.mMap->GetGroundPoint(*(GE3DPoint*)a[0], *(GE3DPoint*)a[1]);
        break;
    case O::EffSetLocalTo: ((EEEffect*)p)->SetLocalTo((long)a[0]);
        break;
    case O::SoundObjectPlay: ((EESoundObject*)p)->Play();
        break;
    case O::PreloadUnloadClass: gDataPreLoading->PostUnloadUnitClass((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::PreloadLoadClass: gDataPreLoading->PostLoadUnitClass((unsigned long)a[0], (unsigned long)a[1]);
        break;
    // the world, the physics, the graphics, the communications
    case O::AttackPlayers: EEAttackPlayers((long)a[0], (unsigned char)a[1]);
        break;
    case O::BuildGate: EEBuildGate(*(EETileCoordinate*)a[0], (long)a[1], (long)a[2], (long)a[3]);
        break;
    case O::VotingPropose: gWorld.mVoting.Propose((unsigned short)a[0], 15);
        break;
    case O::PhysClearF84: gPhysics->mF84 = 0.0f;
        break;
    case O::PhysPost: gPhysics->mThread->Post((unsigned long)a[0], NULL, NULL, true);
        break;
    case O::PhysSetSlowPlayback: gPhysics->SetSlowPlayback(a[0] != 0);
        break;
    case O::PhysQueueMessage: gPhysics->QueueMessage(MSG);
        break;
    case O::PhysFlushWorld: gPhysics->FlushWorld();
        break;
    case O::PhysSetSeed: gPhysics->mRandom.SetSeed((long)a[0]);
        break;
    case O::SimSeedSet: return gPlatSimSeedSet;
    case O::SimSeed: return (int64_t)gPlatSimSeed;
    case O::ScenarioSetLastChat: gWorld.mScenarioManager->mLastChatText = MSG->mString;
        break;
    case O::GfxPostResume: gGraphics->PostResume();
        break;
    case O::GfxPostPause: gGraphics->PostPause();
        break;
    case O::GfxIsPaused: return gGraphics->mIsPaused;
    case O::GfxPostSetFlag5B: gGraphics->PostSetFlag5B(a[0] != 0);
        break;
    case O::Sleep: PlatSleep((unsigned long)a[0]);
        break;
    case O::TimeNow: return TMGetTime();
    case O::CameraUpdate: gWorld.mCameraControl.Update(f[0]);
        break;
    case O::CommPresent: return gCommunications != NULL;
    case O::CommIsClient: return gCommunications->IsClient();
    case O::CommIsGamePaused: return gCommunications->GetIsGamePaused();
    case O::CommSetHumans: gCommunications->SetNumHumanPlayers((unsigned char)a[0]);
        break;
    case O::CommSetMultiplayer: gCommunications->SetIsMultiplayerSession(a[0] != 0);
        break;
    case O::CommProcessLocal: gCommunications->ProcessLocalUserInput((NEMessage*)p, (unsigned char)a[0]);
        break;
    case O::CommSendLoadMainScreenResponse: gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadMainScreenResponse, {gWorld.GetCurrentPlayer()->GetSlot(), gWorld.mCurrentPlayer}), kSlotServer);
        break;
    case O::ClientSetMigrating: CLIENT->mIsMigrating = a[0] != 0;
        break;
    case O::ClientSetSlowPlayback: static_cast<NESynchronousClient*>(gCommunications)->SetSlowPlayback();
        break;
    case O::ClientSetNormalPlayback: static_cast<NESynchronousClient*>(gCommunications)->SetNormalPlayback();
        break;
    case O::ClientFastLockEnter: CLIENT->mFastSelectionLock->Enter();
        break;
    case O::ClientFastLockLeave: CLIENT->mFastSelectionLock->Leave();
        break;
    case O::ClientFastID: return CLIENT->mFastSelectionID;
    case O::ClientFastLast: return P64(CLIENT->mFastSelection);
    case O::ServerSetMigrating: SERVER->mIsMigrating = a[0] != 0;
        break;
    case O::ServerFastLockEnter: SERVER->mFastSelectionLock->Enter();
        break;
    case O::ServerFastLockLeave: SERVER->mFastSelectionLock->Leave();
        break;
    case O::ServerFastID: return SERVER->mFastSelectionID;
    case O::ServerFastLast: return P64(SERVER->mFastSelection);
    case O::ServerLogResponse: static_cast<NESynchronousServer*>(gCommunications)->LogResponse(MSG);
        break;
    case O::ServerSetLoadState: SERVER->mLoadState = (unsigned char)a[0];
        break;
    case O::ServerSetGameState: SERVER->SetGameState((long)a[0]);
        break;
    case O::ServerClearSendToSlots: SERVER->ClearSendToSlotArray();
        break;
    case O::ServerSetLoadResponses: SERVER->mNumLoadResponses = (unsigned char)a[0];
        break;
    case O::ServerGetSendToSlot: return SERVER->GetSendToSlot((unsigned char)a[0]);
    case O::ServerSetSendToSlotForPlayer: SERVER->SetSendToSlotForPlayer((unsigned char)a[0], (unsigned char)a[1]);
        break;
    case O::ServerSendPlayerSlotInfo: SERVER->SendPlayerSlotInfo();
        break;
    case O::CpEngineChoiceCount: return EECPEngineChoiceCount();
    case O::LobbyOptPlayer: return P64(&gGameOptions.mPlayers[a[0]]);
    case O::LobbyReadLock: gGameOptions.mLock->ReadLock();
        break;
    case O::LobbyReadUnlock: gGameOptions.mLock->ReadUnlock();
        break;
    // the user interface, the mouse
    case O::UiPostAction: gUserInterface->PostAction((EEAction*)p);
        break;
    case O::ActionDereference: ((EEAction*)p)->Dereference();
        break;
    case O::UiPostEvent: gUserInterface->PostEvent((UIEvent*)p, NULL);
        break;
    case O::EventDereference: ((UIEvent*)p)->Dereference();
        break;
    case O::UiSetLagStatus: gUserInterface->SetLagStatus((unsigned short)a[0], (unsigned char)a[1]);
        break;
    case O::UiSetLoadPercent: gUserInterface->SetLoadPercent((int32_t)a[0], (uint32_t)a[1]);
        break;
    case O::UiPostLobbyUpdateGameOptions: gUserInterface->PostLobbyUpdateGameOptions();
        break;
    case O::UiPostLobbyUpdatePlayer: gUserInterface->PostLobbyUpdatePlayer((long)a[0], (long)a[1]);
        break;
    case O::UiPostStartLoading: gUserInterface->PostStartLoading();
        break;
    case O::UiCloseForm: gUserInterface->CloseForm(NULL);
        break;
    case O::UiUpdateJit: gUserInterface->UpdateJITManufacturing();
        break;
    case O::UiShowLoadSaveError: gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
        break;
    case O::UiSwapToMainGame: gUIManager->SwapForm(gUIManager->GetMainGameForm());
        break;
    case O::UiSwapToScenarioEditor: gUIManager->SwapForm(gUIManager->GetScenarioEditorForm());
        break;
    case O::MainFormSetInfoBlinking: MAIN_FORM->mInfoBlinking = a[0] != 0;
        break;
    case O::MainFormSetButtonBlinking: MAIN_FORM->mIsButtonBlinking = a[0] != 0;
        break;
    case O::MainFormSetUiState: MAIN_FORM->SetUIState((long)a[0]);
        break;
    case O::MainFormSetCursorLocked: MAIN_FORM->mIsCursorLocked = a[0] != 0;
        break;
    case O::MainFormUpdateHelp: MAIN_FORM->UpdateHelpString();
        break;
    case O::MainFormGroup0: return P64(static_cast<EEUIFMGMainGroup*>(MAIN_FORM->mGroups[0]));
    case O::MainGroupReset: ((EEUIFMGMainGroup*)p)->Reset();
        break;
    case O::GameUiShowCommands: gEEGameUI.mShowCommands((EEUIFMGMainGroup*)p, ((EEUIFMGMainGroup*)p)->GetSelectedUnit());
        break;
    case O::MouseSetPointer: gMouse->mPointer = (long)a[0];
        break;
    case O::MouseSetShiftMode: gMouse->mShiftMode = a[0] != 0;
        break;
    case O::MouseGetPosition: *(GE2DPoint*)a[0] = gMouse->mPosition;
        break;
    case O::MouseSetPosition: gMouse->SetPosition(*(GE2DPoint*)a[0]);
        break;
    case O::MouseUpdate: gMouse->Update();
        break;
    case O::IsMouseBuffered: gISMouse->SetUseBufferedMovement(a[0] != 0);
        break;
    case O::IsMouseRefresh: gISMouse->Refresh();
        break;
    case O::CameraSetZoom: gWorld.mCameraControl.SetZoom(f[0]);
        break;
    default: return EEMessageOpNext(inOp, p, a, f);
    }
    return 0;
}

double EEMessageOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EEMessageOpNextF(inOp, p, a, f);
}

// the next bodies' cases (Empire Earth.cpp)
int64_t EEStartupOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEStartupOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
int64_t EEMessageOpNext(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EEStartupOp(inOp, p, a, f);   // the start-up bodies' (Empire Earth.cpp)
}

double EEMessageOpNextF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EEStartupOpF(inOp, p, a, f);
}
