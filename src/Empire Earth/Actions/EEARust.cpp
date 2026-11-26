// The host of the game's actions whole in Rust (ee/EEARust.h; ee_screens game_actions.rs; port addition, no original
// address): the world's reads and calls of the original Actions units (EEAAddChatMessage.cpp through
// EEAUserPauseGame.cpp) and of Scenario Editor Actions/EEAQuickSaveLoad.cpp and EESEAQuickSaveLoad.cpp, whose
// decisions are Rust now, and the COMDATs those units emitted out of line. Unit flags: the exe module set.
#include <windows.h>
#include <list>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/UIForm.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMouse.h"
#include "ee/EEMessage.h"
#include "ee/EEUserInterface.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEDoppelganger.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/EECalamityClass.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESELoadThread.h"
#include "ee/EESaveLoad.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEGameSound.h"
#include "ee/EESoundObject.h"
#include "ee/EEARust.h"
#include "ee/EECombatGoals.h"

// ------------------------------------------------------------------------------------------------ the host
// The game's actions' host (ee_screens game_actions.rs: EE_SCREENS_GA_OP_*; ee/EEARust.h). The original units'
// ranges: EEAAddChatMessage 0x1000..0x16c0, EEABootClientFromCanceledGame ..0x173b, EEABuild ..0x1a8d, EEACalamity
// ..0x1de1, EEACenterCamera ..0x1fc4, EEAJoinOrHostAGame 0x2086..0x289f, EEALasso ..0x2a84, EEAMiniMap ..0x2fb0,
// EEAMouseClick ..0x4352, EEAPlayerEvent ..0x4413, EEARemovePlayer ..0x4444, EEAResetAllPlayerInfo ..0x4613,
// EEASaveGame ..0x470f, EEAScroll ..0x4a0b, EEAServerMigration ..0x505b, EEAUpgradeImprovement ..0x5280,
// EEAUserPauseGame ..0x52f3, EEAQuickSaveLoad 0x1a5529..0x1a5881, EESEAQuickSaveLoad 0x1aa70e..0x1aa723.
// The C++ constants game_actions.rs names are checked against the headers by its game_actions_cpp_consts_tests.rs
// (they were static_asserts here); ee/EEARust.h's kinds are its constants.


GETransformation gMiniMapTransformation;

// the points and rectangles the cases write into f are the Rust records' floats
static_assert(sizeof(GE2DPoint) == 8 && sizeof(GE3DPoint) == 12 && sizeof(GERectangle) == 16 && sizeof(GE3DPlane) == 24, "f");

// A unit argument: an EEUnit*, or with inComplex an EEComplexUnit*
static EEUnit* AsUnit(void* p, int64_t inComplex)
{
    return inComplex ? static_cast<EEUnit*>(static_cast<EEComplexUnit*>(p)) : static_cast<EEUnit*>(p);
}

// Its complex unit (none for a NULL unit)
static EEComplexUnit* AsComplex(void* p, int64_t inComplex)
{
    return inComplex ? static_cast<EEComplexUnit*>(p) : (p != NULL ? AsUnit(p, 0)->GetComplexUnit() : NULL);
}

// The join's and the migration's connection request (the original's NEMClientConnectionRequest)
static NEMessage* NewConnectionRequest(EEARust* a, bool inBehindFirewall)
{
    return new NEMRecord(kNetMessageClientConnectionRequest, {inBehindFirewall, gBinaryChecksum}, {gGameVersion, a->mPlayerName});
}

// the objects game_actions_host.rs's sequences keep between cases (the UI thread's)
static UWideString sMessage, sFormat;      // a chat or message box line and its format
static UString sAddress;                    // the connection's address
static std::vector<EEUnit*> sFarmers;       // a farm's drop sites

#define GA_A (static_cast<EEARust*>(r))
#define GA_U AsUnit(p, i[1])
#define GA_C AsComplex(p, i[1])
#define GA_CLASS (AsUnit(p, i[1])->GetUnitClass())
#define GA_PL (gWorld.GetCurrentPlayer())
#define GA_G (gWorld.GetCurrentPlayer()->mSelectedGroup)
#define GA_TILE static_cast<EETile*>(p)
#define GA_CAM (gWorld.mCameraControl)
#define GA_K(op, field) (EE_SCREENS_GA_OP_##op << 8 | EE_SCREENS_GA_##field)
#define GA_F2 (*reinterpret_cast<GE2DPoint*>(f))
#define GA_F3 (*reinterpret_cast<GE3DPoint*>(f))
#define GA_LONG(k) (*reinterpret_cast<long*>(&i[k]))   // a long kept in slot k (its low bytes)
#define GA_SERVER static_cast<EEServer*>(gCommunications)
#define GA_CLIENT static_cast<EEClient*>(gCommunications)
#define GA_SYNC static_cast<NESynchronousClient*>(gCommunications)
#define GA_CHAT static_cast<UITextLabel*>(p)
#define GA_FORM gUIManager->GetScenarioEditorForm()
#define GA_ENTRY gWorld.mPlayers[GA_A->mPlayers[i[1]]]
#define GA_SPEC static_cast<FSFileSpec*>(p)

// One case each (game_actions_host.rs composes them): inOp an op (EE_SCREENS_GA_OP_*) or, for the ops with fields,
// op << 8 | field (EE_SCREENS_GA_*, EE_SCREENS_GAH_*)
extern "C" int64_t ee_screenscpp_ga(uint32_t inOp, EeScreensGaFields* r, void* p, int64_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_GA_OP_POST: gUserInterface->PostAction(GA_A); return 0;
    case EE_SCREENS_GA_OP_SEND: gCommunications->ProcessLocalUserInput(static_cast<EEMessage*>(p), kSlotNone); return 0;
    case EE_SCREENS_GA_OP_MESSAGE_RELEASE: static_cast<EEMessage*>(p)->Dereference(); return 0;
    case EE_SCREENS_GA_OP_SELECT_UNITS:   // the box selection's record (0x4e, 0x4f, 0x50)
        return (intptr_t) static_cast<EEMessage*>(EENewSelectUnits(*static_cast<const GERectangle*>(p), i[1], (unsigned char)i[0]));
    case EE_SCREENS_GA_OP_CLASS_COUNT: return EEUnitClassCount();
    case EE_SCREENS_GA_OP_SAVED_GROUP_RESET: GA_PL->mSavedGroups.Reset(0x15, 1); return 0;
    case EE_SCREENS_GA_OP_MOUSE: GA_F2 = gMouse->mPosition; return 0;
    case EE_SCREENS_GA_OP_MOUSE_FROZEN: return gMouse->mIsFrozen != 0;
    case EE_SCREENS_GA_OP_PICK: return (intptr_t)gUnitPicker.PickUnit(GE2DPoint(f[0], f[1]), i[0] != 0);
    case EE_SCREENS_GA_OP_UNIT_REFERENCE: static_cast<EEUnit*>(p)->Reference(); return 0;
    case EE_SCREENS_GA_OP_UNIT_RELEASE: static_cast<EEUnit*>(p)->Dereference(); return 0;
    case EE_SCREENS_GA_OP_CAN_GATHER:
        return static_cast<EEComplexUnit*>(p)->CanGather(reinterpret_cast<EEUnit*>(i[0]), reinterpret_cast<EEUnit*>(i[0])->GetTileX(), reinterpret_cast<EEUnit*>(i[0])->GetTileY(), false, false);
    case EE_SCREENS_GA_OP_CAN_HEAL: return EECanHeal(static_cast<EEComplexUnit*>(p), reinterpret_cast<EEComplexUnit*>(i[0]));
    case EE_SCREENS_GA_OP_WORLD_UNIT: return (intptr_t) static_cast<EEUnit*>(gWorld.GetUnit(i[0]));
    case EE_SCREENS_GA_OP_ACTION: return (intptr_t) static_cast<EEAction*>(GA_A);
    // the units (i1 set: p an EEComplexUnit*)
    case GA_K(UNIT, U_ID): return GA_U->GetUnitID();
    case GA_K(UNIT, U_PLAYER_ID): return GA_U->GetPlayer()->GetPlayerID();
    case GA_K(UNIT, U_PLAYER): return (intptr_t)GA_U->GetPlayer();
    case GA_K(UNIT, U_COMPLEX): return (intptr_t)GA_U->GetComplexUnit();
    case GA_K(UNIT, U_TILE_NODE): return GA_U->GetTileNode() != NULL;
    case GA_K(UNIT, U_TILE_X): return GA_U->GetTileX();
    case GA_K(UNIT, U_TILE_Y): return GA_U->GetTileY();
    case GA_K(UNIT, U_HIT_POINTS): return GA_U->GetHitPoints();
    case GA_K(UNIT, U_MAX_HIT_POINTS): return GA_U->GetStats()->GetMaxHitPoints();
    case GA_K(UNIT, U_RESOURCE_TYPE): return GA_U->GetResourceType();
    case GA_K(UNIT, U_RESOURCE_AMOUNT): return GA_U->GetResourceAmount();
    case GA_K(UNIT, U_ANIMAL_ON_MAP): return EEHerd::IsAnimalOnMap(GA_U->GetComplexUnit());
    case GA_K(UNIT, U_DISABLED): return GA_C->GetIsDisabled();
    case GA_K(UNIT, U_PROTECTED): return GA_C->IsProtected();
    case GA_K(UNIT, U_BUILT): return GA_C->mIsBuilt;
    case GA_K(UNIT, U_SEEN): return EECombatIsSeenBy(GA_U, GA_PL, true);
    case GA_K(UNIT, U_FOOTPRINT_VISIBLE): return ee_units_uu_footprint_visible((uint64_t)(uintptr_t)GA_U, (uint64_t)(uintptr_t)GA_PL) != 0;
    case GA_K(UNIT, U_TILE): return (intptr_t)GA_U->GetRTSTile();
    case EE_SCREENS_GAH_U_LOC_X: f[0] = GA_C->GetPhysicalModel().GetLocation().GetX(); return 0;
    case EE_SCREENS_GAH_U_LOC_Y: f[0] = GA_C->GetPhysicalModel().GetLocation().GetY(); return 0;
    case GA_K(UNIT, U_POSITION): GA_C->mPhysicalModel.GetLocation().GetPosition(GA_F3); return 0;
    case GA_K(UNIT, U_CLASS_PTR): return (intptr_t)GA_U->GetClass();
    case GA_K(UNIT, U_CONTAINER): return (intptr_t)GA_C->mContainer;
    case GA_K(UNIT, U_TILE_HEIGHT): f[0] = GA_U->mTile->mHeight; return 0;
    case GA_K(UNIT, U_IMPROVEMENT_LEVEL): return GA_U->mStats->mImprovement.GetLevel(i[2]);
    case GA_K(UNIT, U_HANDLE): return EEUnitHandleID(GA_U);
    case GA_K(UNIT, U_RESOLVE): return (intptr_t)EEUnitHandle<EEUnit>::FromSavedID((uint32_t)i[2]).Get();
    // the unit classes (of the unit p)
    case GA_K(CLASS, C_TYPE): return GA_CLASS->GetUnitType();
    case GA_K(CLASS, C_CATEGORY): return GA_CLASS->GetCategory();
    case GA_K(CLASS, C_FAMILY): return GA_CLASS->GetFamily();
    case GA_K(CLASS, C_ANIMAL_ID): return GA_CLASS->GetAnimalID();
    case GA_K(CLASS, C_AIR_TRAFFIC): return GA_CLASS->GetAirTrafficType();
    case GA_K(CLASS, C_CAN_RALLY): return GA_CLASS->GetCanSetRallyPoint();
    case GA_K(CLASS, C_TOWER): return GA_CLASS->GetIsTower();
    case GA_K(CLASS, C_TREE): return GA_CLASS->GetIsTree();
    case GA_K(CLASS, C_LAND_BUILDING): return GA_CLASS->IsLandBuilding();
    case GA_K(CLASS, C_DROP_SITE): return GA_CLASS->IsDropSite();
    case GA_K(CLASS, C_TRANSPORT): return GA_CLASS->IsTransport();
    case GA_K(CLASS, C_AIR_BASE): return GA_CLASS->IsAirBase();
    case GA_K(CLASS, C_MEDIC): return GA_CLASS->GetIsMedic();
    case GA_K(CLASS, C_TARGETABLE): return ee_units_uu_targetable(0, (uint64_t)(uintptr_t)GA_CLASS, 0);
    // the current player's selected group
    case GA_K(GROUP, G_COUNT): return GA_G->GetNumUnits();
    case GA_K(GROUP, G_KIND_FLAGS): return (uint32_t)GA_G->mKindFlags;
    case GA_K(GROUP, G_COUNT_KIND): return GA_G->CountUnitsOfKind(i[1]);
    case GA_K(GROUP, G_UNIT): return (intptr_t)GA_G->GetUnit(i[1]);
    case GA_K(GROUP, G_TYPE_COUNT): return GA_G->GetUnitTypeCount(i[1]);
    case GA_K(GROUP, G_FIND_CATEGORY): return (intptr_t)GA_G->FindUnitOfCategory(i[1]);
    case GA_K(GROUP, G_FIND_FLAGS): return (intptr_t)GA_G->FindUnitWithFlags(i[1]);
    case GA_K(GROUP, G_CONTAINS): return GA_G->Contains(static_cast<EEComplexUnit*>(p));
    case GA_K(GROUP, G_FOREST): return GA_G->GetFlags().mFlags[kAITileForest] == true;
    case EE_SCREENS_GAH_G_BOUNDS: GA_G->GetBounds(*reinterpret_cast<GERectangle*>(f)); return 0;
    case GA_K(GROUP, G_FIRST_ID): return GA_G->GetFirstUnit()->GetUnitID();
    case GA_K(GROUP, G_ACTIVE_COUNT): return GA_PL->mActiveGroup->GetNumUnits();
    case GA_K(GROUP, G_ACTIVE_UNIT): return (intptr_t)GA_PL->mActiveGroup->GetUnit(i[1]);
    // the current player
    case GA_K(PLAYER, P_PTR): return (intptr_t)GA_PL;
    case GA_K(PLAYER, P_ID): return GA_PL->GetPlayerID();
    case GA_K(PLAYER, P_ENEMY): return GA_PL->GetEnemy(i[1]);
    case GA_K(PLAYER, P_CURRENT_ID): return gWorld.mPlayers[gWorld.mCurrentPlayer]->mPlayerID;
    case EE_SCREENS_GAH_P_EVENT: return GA_PL->mEvents.GetNextEventSource(*reinterpret_cast<EETileLocation*>(&i[1]));   // (ee_units player_events.rs)
    // EEGetFirstSelectedUnit, EEPointUnitOrTileXY over the call's slots (in and out: the unit in i1; the tile in i1,
    // x and y the longs at i2 and i3)
    case EE_SCREENS_GAH_FS_CALL: return EEGetFirstSelectedUnit(*reinterpret_cast<EEComplexUnit**>(&i[1]));
    case EE_SCREENS_GAH_PT_CALL: return EEPointUnitOrTileXY(GE2DPoint(f[0], f[1]), *reinterpret_cast<EETile**>(&i[1]), GA_LONG(2), GA_LONG(3), i[0] != 0);
    // the tile p
    case GA_K(TILE, T_DOPPEL_COUNT): return GA_TILE->GetNumDoppelgangers();
    case GA_K(TILE, T_DOPPEL_PLAYER): return (intptr_t)GA_TILE->GetDoppelganger(i[1])->mPlayer;
    case GA_K(TILE, T_DOPPEL_UNIT): return (intptr_t)GA_TILE->GetDoppelganger(i[1])->mUnit;
    case GA_K(TILE, T_VISIBLE): return GA_TILE->IsVisibleTo(GA_PL);
    case EE_SCREENS_GAH_T_FIRST: return (intptr_t)GA_TILE->mUnits;
    case EE_SCREENS_GAH_T_NEXT: return (intptr_t) static_cast<RTSTileUnitNode*>(p)->mNext;
    case EE_SCREENS_GAH_T_NODE_UNIT: return (intptr_t) static_cast<RTSTileUnitNode*>(p)->mUnit;
    // a farm's drop sites
    case EE_SCREENS_GAH_FD_CLEAR: sFarmers.clear(); return 0;
    case EE_SCREENS_GAH_FD_FILL: ee_units_uu_farm_drop_sites((uint64_t)(uintptr_t)static_cast<EEComplexUnit*>(p), &sFarmers); return 0;
    case EE_SCREENS_GAH_FD_SIZE: return sFarmers.size();
    case EE_SCREENS_GAH_FD_FIRST_ID: return sFarmers[0]->GetUnitID();
    // the map
    case GA_K(MAP, M_VALID_TILE): return gWorld.GetMap()->GetTerrainMesh().GetIsValidTile((unsigned long)(long)i[1], (unsigned long)(long)i[2]);
    case GA_K(MAP, M_MESH_Z): f[0] = gWorld.mMap->mTerrainMesh.GetMeshZ(f[0], f[1]); return 0;
    case EE_SCREENS_GAH_M_TILE_PTR: return (intptr_t)gWorld.mMap->GetTile(i[1], i[2]);
    case EE_SCREENS_GAH_M_TILE_HEIGHT: f[0] = static_cast<EEMapTile*>(p)->mHeight; return 0;
    case EE_SCREENS_GAH_M_CLOSEST: gWorld.mMap->GetClosestTile(&GA_F3, &GA_LONG(1), &GA_LONG(2)); return 0;
    case EE_SCREENS_GAH_M_PLANE:
        *reinterpret_cast<GE3DPlane*>(f) = i[1] == 0 ? gWorld.mMap->mTerrainMesh.GetLeftPlane() : i[1] == 1 ? gWorld.mMap->mTerrainMesh.GetRightPlane() : i[1] == 2 ? gWorld.mMap->mTerrainMesh.GetFrontPlane() : gWorld.mMap->mTerrainMesh.GetBackPlane();
        return 0;
    case GA_K(MAP, M_EXISTS): return gWorld.mMap != NULL;
    // the effects
    case EE_SCREENS_GAH_FX_CREATE: gWorld.mEffectManager->CreateEffect(static_cast<EEUnit*>(p), GA_F3, EEGraphicEffect::Get(i[0]), reinterpret_cast<EEEffectHandle*>(&i[1]), 0); return 0;
    case EE_SCREENS_GAH_FX_LOCAL: gWorld.mEffectManager->GetEffect(*reinterpret_cast<EEEffectHandle*>(&i[2]))->SetLocalTo(i[1]); return 0;
    // the current player's technologies
    case GA_K(TECH, K_CAN_PRODUCE): return GA_PL->CanProduce(i[1], 0x20, 1, true, 3, NULL);
    case EE_SCREENS_GAH_K_CLASS: return (intptr_t)gUnitClassTable[GA_PL->mTechnologies->Get(i[1])->mData->mUnitClass];
    case EE_SCREENS_GAH_K_CLASS_TILE_SIZE: return static_cast<EEUnitClass*>(p)->GetTileSize();
    case GA_K(TECH, K_NODE_CONTAINS): return GA_PL->mTechnologies->Get(i[1])->Contains(i[2]);
    // the calamity class p at the point f0..f2 (in and out)
    case EE_SCREENS_GAH_A_SCREEN_TO_WORLD: gWorld.ScreenToWorld(GE2DPoint(f[0], f[1]), *reinterpret_cast<GE3DPoint*>(f + 3)); return 0;
    case EE_SCREENS_GAH_A_TRANSFORM: GETransformation(gWorld.mTransformation).Apply(*reinterpret_cast<GE3DPoint*>(f + 3), GA_F3); return 0;
    case GA_K(CALAMITY, A_DEFAULT_TARGET): return static_cast<EECalamityClass*>(p)->GetDefaultTarget(GA_F3, i[1], i[2], i[3] != 0);
    case GA_K(CALAMITY, A_ORDER_AT_CURSOR): EEOrderAtCursor(GE2DPoint(f[0], f[1]), reinterpret_cast<EEUnit*>(i[1])); return 0;
    case GA_K(CALAMITY, A_CAN_CAST):
        return static_cast<EECalamityClass*>(p)->CanCastAt(reinterpret_cast<EEComplexUnit*>(i[1]), GA_F3, i[2], i[3], i[4] != 0, reinterpret_cast<EEUnit*>(i[5]));
    // the camera
    case GA_K(CAMERA, CAM_SET_LOCATION): GA_CAM.SetLocation(GE3DPoint(f[0], f[1], f[2]), false, true); return 0;
    case GA_K(CAMERA, CAM_KEEP_ZOOM): GA_CAM.SetZoom(GA_CAM.GetZoom()); return 0;
    case GA_K(CAMERA, CAM_CHANGED): GA_CAM.SetChanged(); return 0;
    case EE_SCREENS_GAH_CAM_SET_ZOOM: GA_CAM.SetZoom(f[0]); return 0;
    case EE_SCREENS_GAH_CAM_SET_MODE: GA_CAM.SetMode(i[1]); return 0;
    case EE_SCREENS_GAH_CAM_MOVE_TO:
        GA_CAM.MoveTo(static_cast<EEUnit*>(p)->mComplexUnit, GE3DPoint(-10000.0f, 0.0f, 0.0f), false, 0.0f, static_cast<EEUnit*>(p)->mComplexUnit, GE3DPoint(-10000.0f, 0.0f, 0.0f), false, 0.0f, 0.0f, 0.0f);
        return 0;
    case EE_SCREENS_GAH_CAM_UPDATE: GA_CAM.Update(f[0]); return 0;
    case GA_K(CAMERA, CAM_ZOOM): f[0] = GA_CAM.mZoom; return 0;
    case GA_K(CAMERA, CAM_SCROLL): GA_CAM.Scroll(f[0], f[1]); return 0;
    case GA_K(CAMERA, CAM_FOLLOWING_OFF): gCameraFollowing = false; return 0;
    // the mini map, the edge scroll
    case GA_K(MINI_MAP, MM_REFRESH): gMiniMap->Refresh(); return 0;
    case GA_K(MINI_MAP, MM_APPLY): gMiniMapTransformation.Apply(GA_A->mPoint, GA_F2); return 0;
    case GA_K(SCROLL, S_SPEED): f[0] = gGamePreferences.mSettings.GetSideScrollSpeed(); return 0;
    case GA_K(SCROLL, S_EDGES): *reinterpret_cast<GERectangle*>(f) = GA_A->mEdges; return 0;
    case GA_K(SCROLL, S_POLL_RUNNING): return GA_A->mPoll.GetIsRunning();
    case GA_K(SCROLL, S_POLL): return GA_A->mPoll.Poll();
    case GA_K(SCROLL, S_POLL_START): GA_A->mPoll.Start(); return 0;
    case GA_K(SCROLL, S_POLL_STOP): GA_A->mPoll.Stop(); return 0;
    // the screens', the lobby's and the network's (only a LAN or WON game reaches most)
    case GA_K(NET, N_LOBBY_CURRENT): return gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormLobby);
    case EE_SCREENS_GAH_N_CHAT: return (intptr_t) static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormLobby)->GetControl(kLobbyChatPreviousMessages));
    case EE_SCREENS_GAH_N_CHAT_APPEND: GA_CHAT->AppendText(GA_A->mText); return 0;
    case EE_SCREENS_GAH_N_CHAT_BAR: return (intptr_t)GA_CHAT->GetScrollBar();
    case EE_SCREENS_GAH_N_CHAT_MAX: return GA_CHAT->GetScrollBar()->GetSlider()->GetMaxRange();
    case EE_SCREENS_GAH_N_CHAT_CURRENT: return GA_CHAT->GetScrollBar()->GetSlider()->GetCurrentRangeValue();
    case EE_SCREENS_GAH_N_CHAT_START_LINE: GA_CHAT->SetStartingDisplayLine(i[1]); return 0;
    case EE_SCREENS_GAH_N_WON: return gWonNet != NULL;
    case EE_SCREENS_GAH_N_WON_CLEAR: gWonNet->ClearGameList(); return 0;
    case EE_SCREENS_GAH_N_WON_REJOIN: gWonNet->RejoinRoom(false); return 0;
    case GA_K(NET, N_POP): gUIManager->Pop(); return 0;
    case EE_SCREENS_GAH_N_STRING: gResource->GetString(i[1], sMessage); return 0;
    case EE_SCREENS_GAH_N_SHOW: gUIManager->DisplayMessage(sMessage, true, NULL, i[2] ? (void (*)(UIForm*))ee_screens_join_restart_client : NULL, true); return 0;
    case GA_K(NET, N_IS_SERVER): return gCommunications->IsServer() == true;
    case GA_K(NET, N_BOOT_PLAYER): static_cast<EEServer*>(gCommunications)->BootPlayer(GA_A->mPlayer, GA_A->mB0D, GA_A->mIsExiting); return 0;
    case GA_K(NET, N_CTS_PROCESSED): return gCommunications->GetNumberOfCTsProcessedInPhysics() > 0;
    case GA_K(NET, N_USER_PAUSED): return gWorld.mUserPaused == true;
    case EE_SCREENS_GAH_N_PAUSE: dynamic_cast<EEServer*>(gCommunications)->PauseGame(); return 0;
    case EE_SCREENS_GAH_N_UNPAUSE: dynamic_cast<EEServer*>(gCommunications)->UnpauseGame(); return 0;
    case EE_SCREENS_GAH_N_POST_PAUSE: gUserInterface->PostPauseInformation(i[1] != 0); return 0;
    case EE_SCREENS_GAH_N_SET_USER_PAUSED: gWorld.mUserPaused = i[1] != 0; return 0;
    case GA_K(NET, N_FIREWALL): return gWonNet ? gWonNet->GetIsBehindFirewall() : false;
    case GA_K(NET, N_VALID_NAME): return EEIsValidName(i[1] ? GA_A->mGameName : GA_A->mPlayerName);
    case GA_K(NET, N_COMMS): return gCommunications != NULL;
    case EE_SCREENS_GAH_N_DELETE: delete gCommunications; return 0;
    case EE_SCREENS_GAH_N_CLEAR: gCommunications = NULL; return 0;
    case EE_SCREENS_GAH_N_NEW_CLIENT: gCommunications = new EEClient; return 0;
    case EE_SCREENS_GAH_N_CLIENT_START: GA_CLIENT->StartThreads(); return 0;
    case EE_SCREENS_GAH_N_NEW_SERVER:
        gCommunications = new EEServer(GA_A->mGameName, gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, i[1] != 0);
        return 0;
    case EE_SCREENS_GAH_N_SERVER_START: GA_SERVER->StartThreads(); return 0;
    case GA_K(NET, N_PUBLISHED): return GA_SERVER->GetGameWasPublished();
    case EE_SCREENS_GAH_N_PLAYER_NAME: gPlayerName = GA_A->mPlayerName; return 0;
    case EE_SCREENS_GAH_N_HOST_NAME: gGamePreferences.mSettings.mHostGameName = GA_A->mGameName; return 0;
    case EE_SCREENS_GAH_N_OPT_LOCK: gWorld.mLobbyOptions.mLock->WriteLock(); return 0;
    case EE_SCREENS_GAH_N_OPT_COPY: gWorld.mLobbyOptions = gGamePreferences.GetLastGameOptions(); return 0;
    case EE_SCREENS_GAH_N_OPT_MULTI: gWorld.mLobbyOptions.mIsMultiplayer = true; return 0;
    case EE_SCREENS_GAH_N_OPT_NO_CHEATS: gWorld.mLobbyOptions.mCheatCodes = false; return 0;
    case EE_SCREENS_GAH_N_OPT_UNLOCK: gWorld.mLobbyOptions.mLock->WriteUnlock(); return 0;
    case EE_SCREENS_GAH_N_SLEEP: PlatSleep(i[1]); return 0;
    case EE_SCREENS_GAH_N_FORMAT: gResource->GetString(i[1], sFormat); return 0;
    case EE_SCREENS_GAH_N_PRINT_TEXT: sprintf(sMessage, sFormat, (const char*)GA_A->mText); return 0;
    case EE_SCREENS_GAH_N_PRINT_GAME: sprintf(sMessage, sFormat, (const char*)GA_A->mGameName); return 0;
    case EE_SCREENS_GAH_N_CHAT_LOG: gUserInterface->AddChatLogEntry(sMessage, 0); return 0;
    case EE_SCREENS_GAH_N_ADDRESS_GAME: sAddress = GA_A->mGameName; return 0;
    case EE_SCREENS_GAH_N_ADDRESS_NAME: sAddress = GA_A->mName; return 0;
    case EE_SCREENS_GAH_N_SEARCH_OFF: gCommunications->SearchForGame(false); return 0;
    case EE_SCREENS_GAH_N_CONNECT: return GA_SYNC->Connect(sAddress, UString("EEService"), UWideString(""));
    case EE_SCREENS_GAH_N_NEW_REQUEST: return (intptr_t)NewConnectionRequest(GA_A, i[1] != 0);
    case EE_SCREENS_GAH_N_SEND_START: GA_SYNC->SendStartConnectMessage(static_cast<NEMessage*>(p)); return 0;
    case EE_SCREENS_GAH_N_INPUT: gCommunications->ProcessLocalUserInput(static_cast<NEMessage*>(p), kSlotNone); return 0;
    case EE_SCREENS_GAH_N_EVENT: return (intptr_t)EECreateScreenEvent(i[1]);
    case EE_SCREENS_GAH_N_OPEN_EVENT: gUIManager->GetFormByID(kFormLobby)->SetFormOpenEvent(static_cast<UIEvent*>(p)); return 0;
    case EE_SCREENS_GAH_N_EVENT_RELEASE: static_cast<UIEvent*>(p)->Dereference(); return 0;
    case EE_SCREENS_GAH_N_SIERRA_ID: gGamePreferences.mSettings.mSierraUserID = GA_A->mPlayerName; return 0;
    case GA_K(NET, N_DETECTED_COUNT): return GA_SYNC->GetDetectedGameCount();
    case GA_K(NET, N_DETECTED): return GA_SYNC->GetDetectedGame(i[1], GA_A->mFileName, GA_A->mDescription, GA_A->mDetectedName);
    case GA_K(NET, N_DETECTED_MATCH): return GA_A->mDescription == "EEService" && GA_A->mDetectedName == GA_A->mGameName;
    case GA_K(NET, N_CONNECT_DETECTED): return GA_SYNC->Connect(GA_A->mFileName, GA_A->mDescription, GA_A->mDetectedName);
    case EE_SCREENS_GAH_N_CLEAR_TEXT: GA_A->mText = UWideString(); return 0;   // the dropped host's name, the new host's and its address
    case EE_SCREENS_GAH_N_CLEAR_GAME_NAME: GA_A->mGameName = UWideString(); return 0;
    case EE_SCREENS_GAH_N_CLEAR_NAME: GA_A->mName = UString(); return 0;
    case EE_SCREENS_GAH_N_RUN_OWED: gPhysics->RunOwedTicks(); return 0;
    case EE_SCREENS_GAH_N_CLIENT_MIGRATING: GA_CLIENT->mIsMigrating = true; return 0;
    case GA_K(NET, N_HAS_LEFT): return GA_CLIENT->mHasLeftGame != 0;
    case GA_K(NET, N_NUM_PLAYERS): return gWorld.mNumPlayers;
    case EE_SCREENS_GAH_N_PLAYER_SLOT: return gWorld.mPlayers[i[1]]->mSlot;
    case EE_SCREENS_GAH_N_PLAYER_FIREWALL: return gWorld.mPlayers[i[1]]->mBehindFirewall != 0;
    case EE_SCREENS_GAH_N_PLAYER_HUMAN: return gWorld.mPlayers[i[1]]->mHumanPlayer != 0;
    case EE_SCREENS_GAH_N_PLAYER_CURRENT: return GA_PL == gWorld.mPlayers[i[1]];
    case GA_K(NET, N_LOADING_SAVED): return gWorld.mLoadingSavedGame != 0;
    case EE_SCREENS_GAH_N_ACTIVE_CLEAR: gWorld.mPlayers[i[1]]->mActiveGroup->Clear(); return 0;
    case EE_SCREENS_GAH_N_COPY_SELECTION: GA_PL->mSelectedGroup->CopySelection(GA_PL->mActiveGroup); return 0;
    case EE_SCREENS_GAH_N_RESET_FAST: GA_CLIENT->ResetFastSelection(); return 0;
    case GA_K(NET, N_SET_SLOT): gWorld.mPlayers[i[1]]->mSlot = (unsigned char)i[2]; return 0;
    case EE_SCREENS_GAH_N_KEEP_TEXT: GA_A->mText = gWorld.mPlayers[i[1]]->mName; return 0;
    case EE_SCREENS_GAH_N_KEEP_HUMAN: GA_A->mName = static_cast<EEHumanPlayer*>(gWorld.mPlayers[i[1]])->GetName(); return 0;
    case EE_SCREENS_GAH_N_KEEP_GAME_NAME: GA_A->mGameName = gWorld.mPlayers[i[1]]->mName; return 0;
    case EE_SCREENS_GAH_N_CTS: return gCommunications->GetNumberOfCTsProcessedInPhysics();
    case EE_SCREENS_GAH_N_CURRENT_TICK: return gPhysics->mCurrentTick;
    case EE_SCREENS_GAH_N_FAST_ID: return GA_CLIENT->mFastSelectionID;
    case EE_SCREENS_GAH_N_NEW_MIGRATED:
        gCommunications = new EEServer(UWideString("Migrated Server"), gGamePreferences.mSettings.GetNetworkAdapter(), (unsigned long)i[1], (unsigned long)i[2], false, false);
        return 0;
    case EE_SCREENS_GAH_N_CLEAR_SERVER_MESSAGES: gPhysics->ClearServerMessages(); return 0;
    case EE_SCREENS_GAH_N_MULTI_SESSION: gCommunications->SetIsMultiplayerSession(true); return 0;
    case EE_SCREENS_GAH_N_SEND_TO_SERVER: GA_SERVER->SetSendToSlotForPlayer(gWorld.mCurrentPlayer, kSlotServer); return 0;
    case EE_SCREENS_GAH_N_SLOT_FE: GA_PL->mSlot = 0xfe; return 0;
    case EE_SCREENS_GAH_N_SERVER_FAST_ID: GA_SERVER->mFastSelectionID = (unsigned char)i[1]; return 0;
    case EE_SCREENS_GAH_N_SERVER_MIGRATING: GA_SERVER->mIsMigrating = true; return 0;
    case EE_SCREENS_GAH_N_WAIT_TIMED_OUT: GA_SERVER->HandleReconnectWaitTimedOut(); return 0;
    case EE_SCREENS_GAH_N_WAIT_START: GA_SERVER->StartWaitingForReconnects(i[1]); return 0;
    case EE_SCREENS_GAH_N_LAST_TICK: gCommunications->SetLastTickReceived(-2); return 0;
    case EE_SCREENS_GAH_N_MY_SLOT_NONE: GA_SYNC->SetMySendToSlot(kSlotNone); return 0;
    case EE_SCREENS_GAH_N_NEW_RECONNECT:   // the tick, the player
        return (intptr_t)static_cast<NEMessage*>(new NEMRecord(kNetMessageClientReconnectionRequest, {gCommunications->GetNumberOfCTsProcessedInPhysics(), gWorld.mCurrentPlayer}));
    case GA_K(NET, N_LAG_CLEAR): gWorld.mLagStatus = 0; return 0;
    // the scenario editor's and the saved games'
    case GA_K(EDITOR, E_JIT): gUserInterface->UpdateJITManufacturing(); return 0;
    case EE_SCREENS_GAH_E_APPLY_IMPROVEMENT:
        static_cast<EEUnit*>(p)->GetPlayer()->ApplyImprovement(static_cast<EEUnit*>(p)->GetUnitClass()->GetUnitType(), GA_A->mImprovement, i[1] != 0);
        return 0;
    case EE_SCREENS_GAH_E_SHOW_UNIT: GA_FORM->UnitCall(EE_SCREENS_SEUN_SHOW_UNIT, 0, static_cast<EEUnit*>(p)); return 0;
    case EE_SCREENS_GAH_E_HELP: GA_FORM->UpdateHelpString(); return 0;
    case GA_K(EDITOR, E_SOUND):
        gSoundManager->Play(EESoundObject::Get(i[1] == 1 ? kSoundButton1 : kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case GA_K(EDITOR, E_RESET_COUNT): return GA_A->mPlayers.size();
    case EE_SCREENS_GAH_E_APPLY_CIV: EEApplyCivilizationFile(GA_ENTRY->mCivilizationFile, GA_ENTRY); return 0;
    case EE_SCREENS_GAH_E_POST406:
        GA_FORM->GetLoadThread()->Post406(reinterpret_cast<void*>(GA_A->mPlayers[i[1]]), reinterpret_cast<void*>(GA_ENTRY->GetTechTree()->mStartEpoch));
        return 0;
    case GA_K(EDITOR, E_RESET_POST403): GA_FORM->GetLoadThread()->Post403(reinterpret_cast<void*>(GA_A->mPlayers[i[1]]), false); return 0;
    case GA_K(EDITOR, E_RESET_POST407):
        GA_FORM->GetLoadThread()->Post407(reinterpret_cast<void*>(GA_A->mPlayers[i[1]]), reinterpret_cast<void*>(GA_ENTRY->GetTechTree()->mStartEpoch));
        return 0;
    case GA_K(EDITOR, E_SAVE): return SaveGame(GA_A->mFileName, TMGetTime(), GA_A->mFlag, GA_A->mDescription);
    case GA_K(EDITOR, E_CLOSE_FORM): gUserInterface->CloseForm(NULL); return 0;
    case GA_K(EDITOR, E_SAVE_ERROR): gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true); return 0;
    case GA_K(EDITOR, E_PAUSED): return gCommunications->GetIsGamePaused();
    case EE_SCREENS_GAH_E_MASTER: f[0] = gSoundManager->GetMasterVolume(); return 0;
    case EE_SCREENS_GAH_E_MUSIC: f[0] = gConductor->GetConductor().GetVolume(); return 0;
    case EE_SCREENS_GAH_E_SET_MASTER: gSoundManager->SetMasterVolume(f[0]); return 0;
    case EE_SCREENS_GAH_E_SET_MUSIC: gConductor->GetConductor().SetVolume(f[0]); return 0;
    case EE_SCREENS_GAH_E_NO_OWED: gPhysics->mTicksOwed = 0; return 0;
    case EE_SCREENS_GAH_E_PAUSE: static_cast<NESynchronousServer*>(gCommunications)->PauseGame(); return 0;
    case EE_SCREENS_GAH_E_UNPAUSE: static_cast<NESynchronousServer*>(gCommunications)->UnpauseGame(); return 0;
    case EE_SCREENS_GAH_E_NEW_SPEC: return (intptr_t)(new FSFileSpec);   // the quick save's file in data\Scenarios
    case EE_SCREENS_GAH_E_SPEC_VOLUME: GA_SPEC->SetVolume(gStartupVolume); return 0;
    case EE_SCREENS_GAH_E_SPEC_DIRECTORY: GA_SPEC->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\"); return 0;
    case EE_SCREENS_GAH_E_SPEC_QUICK: GA_SPEC->SetFile(UString("EEQuickSave") + GA_A->mName); return 0;
    case EE_SCREENS_GAH_E_SPEC_FILE_NAME: GA_SPEC->SetFile(GA_A->mFileName); return 0;
    case EE_SCREENS_GAH_E_LOAD: return LoadSavedGameFile(*GA_SPEC, true, true, true) != 0;
    case EE_SCREENS_GAH_E_DELETE_SPEC: delete GA_SPEC; return 0;
    case EE_SCREENS_GAH_E_NEW_MEMORY: return (intptr_t)(new FSMemoryFile(0x400, true, 1000));
    case EE_SCREENS_GAH_E_TEXT_NAME: GA_A->mFileName = GetSaveTextName(UString("EEQuickSave") + GA_A->mName, true); return 0;
    case EE_SCREENS_GAH_E_WRITE: WriteSaveGame(GA_A->mFileName, TMGetTime(), *static_cast<FSMemoryFile*>(p), *static_cast<FSMemoryFile*>(p)); return 0;
    case EE_SCREENS_GAH_E_CREATE: GA_SPEC->Create(); return 0;
    case EE_SCREENS_GAH_E_OPEN: return GA_SPEC->Open(kFileAccessWrite) == 0;
    case EE_SCREENS_GAH_E_SAVE_INTO: static_cast<FSMemoryFile*>(p)->Save(*reinterpret_cast<FSFileSpec*>(i[1])); return 0;
    case EE_SCREENS_GAH_E_CLOSE: GA_SPEC->Close(); return 0;
    case GA_K(EDITOR, E_RESUME_GROUP): GA_FORM->ResumeCurrentGroup(); return 0;
    case GA_K(EDITOR, E_DELETE_FILE): delete static_cast<FSMemoryFile*>(p); return 0;
    case GA_K(EDITOR, E_OBJECT_SELECTION):   // the trigger tab's object screen's UpdateSelection (it was EESEARust.cpp's)
        GA_FORM->TriggerSubOp(EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT, EE_SCREENS_SEOB_UPDATE_SELECTION);
        return 0;
    case GA_K(EDITOR, E_LOAD_TEXT): GA_FORM->SetLoadText(GA_A->mText); return 0;
    }
    return 0;
}

// A record value (EEMRecordValue's conversions)
static EEMRecordValue GaValue(const EeScreensGaValue& inValue)
{
    return inValue.kind == EE_SCREENS_GA_VALUE_POINT   ? EEMRecordValue(GE2DPoint(inValue.f[0], inValue.f[1]))
           : inValue.kind == EE_SCREENS_GA_VALUE_FLOAT ? EEMRecordValue((double)inValue.f[0])
                                                        : EEMRecordValue((long long)inValue.i);
}

#define GA_V(k) GaValue(inValues[k])

// A record message: its members in wire order (seven at most)
extern "C" void* ee_screenscpp_ga_record(uint32_t inKind, const EeScreensGaValue* inValues, uint32_t inCount)
{
    switch (inCount) {
    case 0: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind));
    case 1: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0)}));
    case 2: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0), GA_V(1)}));
    case 3: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0), GA_V(1), GA_V(2)}));
    case 4: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0), GA_V(1), GA_V(2), GA_V(3)}));
    case 5: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0), GA_V(1), GA_V(2), GA_V(3), GA_V(4)}));
    case 6: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0), GA_V(1), GA_V(2), GA_V(3), GA_V(4), GA_V(5)}));
    default: return static_cast<EEMessage*>(new EEMRecord((unsigned char)inKind, {GA_V(0), GA_V(1), GA_V(2), GA_V(3), GA_V(4), GA_V(5), GA_V(6)}));
    }
}

// ------------------------------------------------------------------------------------------------ the originals' COMDATs
// (out of line in these units after their first users)

// Returns whether a player name is non-empty and free of control, quote, backslash and @ characters
bool EEIsValidName(const UWideString& inName)
{
    return ee_screens_valid_name(inName, inName.GetLength());   // (ee_screens single_player.rs valid_name)
}

// Returns whether the tile is visible to a player (or the player sees everything)
bool EETile::IsVisibleTo(EEPlayer* inPlayer)
{
    return (this->mVisibility[inPlayer->mPlayerID >> 1] >> ((inPlayer->mPlayerID & 1) << 4) & 0xff) > 0 ||
           inPlayer->mSeesAll;
}
