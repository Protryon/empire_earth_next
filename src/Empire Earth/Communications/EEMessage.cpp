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
#include "lle/GETerrainMesh.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
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
#include "ee/EEARust.h"
#include "ee/EEScreenEvent.h"
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
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
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

// Orders farms for EEMBuildUnit::Execute: built ones first, then paid-for ones, then the nearer to the tile. Inline
// (header-declared); its COMDAT 0x4c995c lands among this unit's sort COMDATs after its first user, the sort
// in EEMBuildUnit::Execute
inline bool EETileDistanceLess::operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
{
    return ee_gamecpp_farm_order(this->mCenter.mX, this->mCenter.mY, inFirst, inSecond, false);   // gamecpp msg_objs.rs
}

#include "ee/EEStringIDs.h"

// 0x23cc42 (cdecl): the exchange rate between a resource and the trade resource for inPlayer (provisional name)
long EEGetTradePrice(EEPlayer* inPlayer, long inResource, unsigned char inBuy);

// inline (header-declared; the COMDAT 0x4c0894 sits here): inPoint on the terrain, its own z off the mesh (gamecpp
// map.rs); not inline: other units call it
void EEMap::GetGroundPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint)
{
    ee_gamecpp_map_ground_point(this->mTerrainMesh.GetRust(), reinterpret_cast<const EeGe3DPoint*>(&inPoint), reinterpret_cast<EeGe3DPoint*>(&outPoint));
}

// gLocalPlayerOut (0x4e6099; ee/EEMessage.h): gamecpp globals.rs

#include "ee/EESaveLoad.h"
#include <vector>
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);

extern long& gAutoSaveCount;                 // 0x5308b8 (the auto-save count LoadOrSave passes on; provisional name)
// (0x14cbc7 TrimAutoSaves: ee/EESaveLoad.h)

// Saves the game under the request's name (the old EEMLoadOrSave::LoadOrSave; Rust: gamecpp message.rs)
long EELoadOrSave(EEMRecord* m)
{
    return ee_gamecpp_msg_load_or_save(m);
}

// Creates a box selection: picks the local player's units of the given type in the rectangle and stores them
// compressed (the count, the blob), the rectangle and the type beside (members 3 and 4, off the wire). The selection
// on the wire: byte 0 is the owner (the ids' top byte), then runs of a count byte (low 6 bits; the top 2 bits: 0, 1
// or 2 bytes of the ids' middle 16 bits follow) and the ids' low bytes; at most 63 ids (CompressUnits 0x4c5e67,
// ee_netmsg units.rs).
EEMRecord* EENewSelectUnits(const GERectangle& inRectangle, long inUnitType, unsigned char inType)
{
    return (EEMRecord*)ee_gamecpp_msg_select_units(&inRectangle, inUnitType, inType);   // gamecpp msg_objs.rs
}

// Decodes a box selection's compact buffer into unit ids, replacing the list
void EESelectUnitsIDs(const EEMRecord* inMessage, std::vector<long>& outUnits)
{
    ee_gamecpp_msg_select_ids(inMessage, &outUnits);   // gamecpp msg_objs.rs
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

// Orders farms for smart farmers: unpaid before paid-for before built farms, the farther farm first in a tier
bool EEFarmDistanceGreater::operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const
{
    return ee_gamecpp_farm_order(this->mCenter.mX, this->mCenter.mY, inFirst, inSecond, true);   // gamecpp msg_objs.rs
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
// over EEMessageOp below), and so is the host (gamecpp msg_host.rs, over EECore11OpC4's C11Nm cases: game core round
// 11); the chat texts (EE_NETMSG_OP_CHAT_TEXT, EE_NETMSG_WACT_SHOW_CHAT):
static const long kNetMsgTexts[] = {IDS_WARNING_YOU_ARE_RUNNING_OUT, IDS_CIVILIZATION_SELECTION_TIME_HAS_EXPIRED,
                                    IDS_YOU_MAY_NOT_UNPACK_ALL_UNITS, IDS_SETUP_ALL, IDS_SETUP_ALLIES, IDS_SETUP_ENEMIES,
                                    IDS_SETUP_CUSTOM, IDS_OBSERVERS};

// the type's body (ee_netmsg bodies.rs) over the record and its list, with the host above: alone (Execute) or in the
// physics' framing (1: a selection, 2: a command)
static void EENetMsgExecute(EEMRecord* inMessage, uint8_t inFraming)
{
    ee_netmsg_execute((EeNetMsgHostFn)ee_gamecpp_netmsg_host, inMessage, inMessage->GetType(), inMessage->GetOriginatingSlot(), inMessage->mFastSelectionID,
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


// the next bodies' cases (Empire Earth.cpp)
int64_t EEStartupOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
// ------------------------------------------------------------------------- round 11, lane C: the messages' C++


#define NC ((EeNetMsgCall*)p)
#define NM_M ((EEMRecord*)(uintptr_t)NC->message)
#define NM_P ((EEPlayer*)(uintptr_t)NC->a)
#define NM_G ((EEGroup*)(uintptr_t)NC->a)
#define NM_U ((EEComplexUnit*)(uintptr_t)NC->b)   // an EEUnit* when a world lookup gave it
#define NM_I (NC->i)

#define MCMSG ((NEMessage*)p)
#define MCNE (static_cast<NEMRecord*>(MCMSG))
#define MCMR (static_cast<EEMRecord*>(MCMSG))
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
    case O::NewStartGameRecord: return P64(static_cast<NEMessage*>(new NEMRecord(kNetMessageStartGame, {(unsigned long)a[0]})));
    case O::NewChatAction: return P64(static_cast<EEAction*>(new EEAAddChatMessage(MSG->mString, a[0] != 0)));
    case O::NewTimerExpiredEvent: return P64(static_cast<UIEvent*>(new EEScreenEvent(EE_SCREENS_EV_TIMER_EXPIRED)));
    case O::VecCivUpgradesOfMsg: return P64(new std::vector<EECivUpgrade>(MSG->GetList<EECivUpgrade>()));
    case O::VecCivUpgradesDelete: delete (std::vector<EECivUpgrade>*)p;
        break;
    case O::CivRequestUpgrades: EECBRequestUpgrades((unsigned char)a[0], MSG->mString, *(std::vector<EECivUpgrade>*)a[1]);
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
    case O::PlCanBuy: return PL->CanBuy((long)a[0], (long)a[1], (UWideString*)a[2]);
    case O::PlQueueImprovement: PL->QueueImprovement((unsigned long)a[0], (unsigned long)a[1], 0, 0);
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
    case O::GroupFormationType: return ((EEGroup*)p)->GetFormation()->mType;
    case O::GroupSetFormation: ((EEGroup*)p)->SetFormation((EEFormation*)a[0]);
        break;
    case O::SelGroupSetFormation: SEL->SetFormation((EEFormation*)a[0]);
        break;
    case O::SelGroupSavedGroup: return P64(SEL->GetSavedGroup());
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
    case O::EffSetLocalTo: ((EEEffect*)p)->SetLocalTo((long)a[0]);
        break;
    case O::SoundObjectPlay: ((EESoundObject*)p)->Play();
        break;
    case O::PreloadUnloadClass: gDataPreLoading->PostUnloadUnitClass((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::PreloadLoadClass: gDataPreLoading->PostLoadUnitClass((unsigned long)a[0], (unsigned long)a[1]);
        break;
    // the world, the physics, the graphics, the communications
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
    case O::PhysSetSeed: gPhysics->mRandom.SetSeed((long)a[0]);
        break;
    case O::SimSeedSet: return gPlatSimSeedSet;
    case O::SimSeed: return (int64_t)gPlatSimSeed;
    case O::ScenarioSetLastChat: gWorld.mScenarioManager->mLastChatText = MSG->mString;
        break;
    case O::GfxIsPaused: return gGraphics->mIsPaused;
    case O::GfxPostSetFlag5B: gGraphics->PostSetFlag5B(a[0] != 0);
        break;
    case O::CommSendLoadMainScreenResponse: gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageLoadMainScreenResponse, {gWorld.GetCurrentPlayer()->GetSlot(), gWorld.mCurrentPlayer}), kSlotServer);
        break;
    case O::ClientFastLockEnter: CLIENT->mFastSelectionLock->Enter();
        break;
    case O::ClientFastLockLeave: CLIENT->mFastSelectionLock->Leave();
        break;
    case O::ServerFastLockEnter: SERVER->mFastSelectionLock->Enter();
        break;
    case O::ServerFastLockLeave: SERVER->mFastSelectionLock->Leave();
        break;
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
    case O::MainFormGroup0: return P64(MAIN_FORM->mMainGroup);
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
    // (EECore11OpC4) the C++ calls and members of this file's bodies in Rust (gamecpp; EEClient.cpp's EECore11OpC3 default): one
    // statement each
    // the net message host (gamecpp msg_host.rs; p the host's call, EeNetMsgCall)
    case O::C11NmSender: return P64(gWorld.GetPlayerBySlot(NM_M->GetOriginatingSlot()));
    case O::C11NmPlayer: return P64(gWorld.mPlayers[NM_I[0]]);
    case O::C11NmCurrent: return P64(gWorld.GetCurrentPlayer());
    case O::C11NmGroup: return P64(static_cast<EEGroup*>(NM_P->mActiveGroup));
    case O::C11NmSavedGroup: return P64(static_cast<EEGroup*>(NM_P->mGroups[NM_I[0]]));
    case O::C11NmGroupCount: return NM_G->GetNumUnits();
    case O::C11NmGroupUnit: return P64(NM_G->GetUnit(NM_I[0]));
    case O::C11NmGroupFirst: return P64(NM_G->GetFirstUnit());
    case O::C11NmGroupContains: return NM_G->Contains(NM_U);
    case O::C11NmGroupAdd: NM_G->Add(NM_U);
        break;
    case O::C11NmGroupClear: NM_G->Clear();
        break;
    case O::C11NmUnit: return P64(gWorld.GetUnit(NM_I[0]));
    case O::C11NmKill: NM_U->Kill(NM_I[0], 0, 0);
        break;
    case O::C11NmClassFact: return NM_I[0] == EE_NETMSG_CLASS_TYPE ? ((EEUnitClass*)(uintptr_t)NC->a)->mUnitType : ((EEUnitClass*)(uintptr_t)NC->a)->mTileSize;
    case O::C11NmUnitPackedClass: return P64(NM_U->mUnitClass->mPackedClass);
    case O::C11NmUnitDeathSpawn: return NM_U->mUnitClass->mDeathSpawnClass != NULL;
    case O::C11NmUnitTileNode: return NM_U->mTileNode != NULL;
    case O::C11NmUnitOwner: return P64(NM_U->mPlayer);
    case O::C11NmUnitLandBuilding: return NM_U->mUnitClass->IsLandBuilding();
    case O::C11NmUnitCanDelete: return NM_U->CanDelete();
    case O::C11NmUnitTileX: return NM_U->GetTileX();
    case O::C11NmUnitTileY: return NM_U->GetTileY();
    case O::C11NmUnitNext: return P64(NM_U->mNext);
    case O::C11NmUnitAsComplex: return P64(EEUnitCast<EEComplexUnit>(static_cast<EEUnit*>(NM_U)));
    case O::C11NmUnitInSet: return gUnitSetManager.GetSet(NM_I[1])->Contains(NM_U->mUnitClass->mUnitType);
    case O::C11NmPlayerFirstUnit: return P64(NM_P->mFirstUnit);
    case O::C11NmPlayerTradePrice: return EEGetTradePrice(NM_P, (long)NM_I[1], (unsigned char)NM_I[2]);
    case O::C11NmWorldNumPlayers: return gWorld.mNumPlayers;
    case O::C11NmWorldCurrentIndex: return gWorld.mCurrentPlayer;
    case O::C11NmWorldStringLength: return NM_M->mString.GetLength();
    case O::C11NmWorldCollisionSphereMode: return gCollisionSphereMode;
    case O::C11NmMapType: gGameOptions.mMapType = NM_M->mString;
        break;
    // the factory (p the received buffer)
    case O::C11MsgBuildVerify: ESVERIFY_AT(p != 0, 841);
        break;
    case O::C11MsgNewRecord: return P64(static_cast<NEMessage*>(new EEMRecord((unsigned char*)p)));
    case O::C11MsgBuildUndefined: ESVERIFY_AT("Undefined message type" == NULL, 1275);
        break;
    // a box selection (p the record; the picked ids a C++ vector)
    case O::C11UlongsNew: return P64(new std::vector<unsigned long>);
    case O::C11UlongsDelete: delete (std::vector<unsigned long>*)p;
        break;
    case O::C11UlongsCount: return (int64_t)((std::vector<unsigned long>*)p)->size();
    case O::C11UlongsAt: return (int64_t)(*(std::vector<unsigned long>*)p)[a[0]];
    case O::C11PickUnits: gUnitPicker.PickUnits(*(const GERectangle*)a[0], gWorld.GetCurrentPlayer(), (long)a[1], *(std::vector<unsigned long>*)p);
        break;
    case O::C11CurrentPlayerId: return (int64_t)(uint32_t)gWorld.GetCurrentPlayer()->mPlayerID;
    case O::C11MsgListAssign: MSG->mList.assign((const unsigned char*)a[0], (const unsigned char*)a[0] + a[1]);
        break;
    case O::C11MsgListData: return P64(MSG->mList.empty() ? NULL : &MSG->mList[0]);
    case O::C11MsgListSize: return (int64_t)MSG->mList.size();
    case O::C11MsgSetRectangle: MSG->Set<GERectangle>(3, *(const GERectangle*)a[0]);
        break;
    case O::C11FarmKey: *(EeGamecppFarmKey*)a[0] = EeGamecppFarmKey{CX->mIsBuilt, CX->mIsPaidFor, CX->mTileX, CX->mTileY};
        break;
    case O::C11LongsAssign: ((std::vector<long>*)p)->assign((const int32_t*)a[0], (const int32_t*)a[0] + a[1]);
        break;
    // (EECore10OpC) lane C's chain link (round 10): the C++ calls and members of gamecpp msg_capture.rs and mrecord.rs (p a message),
    // then EECivCapture.cpp's, then lane D's link: one statement each
    case O::LcMsgType: return MCMSG->GetType();
    case O::LcMsgForSlot: return MCMSG->GetForSpecificSlot();
    case O::LcMsgOrigSlot: return MCMSG->GetOriginatingSlot();
    case O::LcMsgIsGame: return EEGameMessageCast(MCMSG) != NULL;
    case O::LcMsgExportSize: return (int64_t)MCMSG->ExportSize();
    case O::LcMsgExport: MCMSG->ExportMessage((unsigned char**)a[0], *(unsigned long*)a[1]);
        break;
    case O::LcBytesDelete: delete[] (unsigned char*)p;
        break;
    case O::LcMsgCodecSlots: return (int64_t)EEGameMessageCast(MCMSG)->GetCodecSlots((EeNetMsgSlot*)a[0], *(uint32_t*)a[1]);
    case O::LcMsgSelectIDs: EESelectUnitsIDs(static_cast<EEMRecord*>(EEGameMessageCast(MCMSG)), *(std::vector<long>*)a[0]);
        break;
    case O::LcMsgBuildGame: return P64(EEMessage::BuildEEMessage((unsigned char*)a[0]));
    case O::LcMsgBuildEngine: return P64(NEMessage::BuildNEMessage((unsigned char*)a[0]));
    case O::LcMsgDereference: MCMSG->Dereference();
        break;
    case O::LcNeInputs: return (int64_t)MCNE->mPlayerInputs.size();
    case O::LcNeInputAt: return P64(MCNE->mPlayerInputs[a[0]]);
    case O::LcNeString: return P64(&MCNE->GetString((uint32_t)a[0]));
    case O::LcNeRecord: return P64(MCNE->mRecord);
    case O::LcWideLength: return (int64_t)((const UWideString*)p)->GetLength();
    // the messages' codec and the records (gamecpp mrecord.rs; p the message)
    case O::LcEmCodecSlots: return (int64_t)static_cast<EEMessage*>(MCMSG)->GetCodecSlots((EeNetMsgSlot*)a[0], *(uint32_t*)a[1]);
    case O::LcBytesNew: return P64(new unsigned char[a[0]]);
    case O::LcMrRecord: return P64(MCMR->mRecord);
    case O::LcMrFastId: return P64(&MCMR->mFastSelectionID);
    case O::LcMrHeaderSlots: MCMR->GetHeaderSlots((EeNetMsgSlot*)a[0]);
        break;
    case O::LcMrSetString: MCMR->mString = *(const UWideString*)a[0];
        break;
    case O::LcMrStringText: return P64((const char*)MCMR->mString);
    case O::LcMrStringImport: MCMR->mString.Import((unsigned char*)a[0]);
        break;
    case O::LcMrListAppend: MCMR->mList.insert(MCMR->mList.end(), (const unsigned char*)a[0], (const unsigned char*)a[0] + a[1]);
        break;
    case O::LcMrListAssign: MCMR->mList.assign((const unsigned char*)a[0], (const unsigned char*)a[0] + a[1]);
        break;
    case O::LcMrListClear: MCMR->mList.clear();
        break;
    case O::LcMrListData: return P64(MCMR->mList.empty() ? NULL : &MCMR->mList[0]);
    case O::LcMrListSize: return (int64_t)MCMR->mList.size();
    case O::LcWideText: return (int64_t)(intptr_t)(const char*)*(const UWideString*)p;
    case O::LcLongVecNew: return (int64_t)(intptr_t)(new std::vector<long>);
    case O::LcLongVecDelete: delete (std::vector<long>*)p;
        break;
    case O::LcLongVecSize: return (int64_t)((std::vector<long>*)p)->size();
    case O::LcLongVecAt: return (*(std::vector<long>*)p)[a[0]];
    default: return EEStartupOp(inOp, p, a, f);
    }
    return 0;
}
