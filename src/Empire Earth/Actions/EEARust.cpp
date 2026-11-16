// The host of the game's actions whole in Rust (ee/EEARust.h; ee_screens game_actions.rs; port addition, no original
// address): the world's reads and calls of the original Actions units (EEAAddChatMessage.cpp through
// EEAUserPauseGame.cpp) and of Scenario Editor Actions/EEAQuickSaveLoad.cpp and EESEAQuickSaveLoad.cpp, whose
// decisions are Rust now, and the COMDATs those units emitted out of line. Unit flags: the exe module set.
#include <windows.h>
#include <list>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
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
#include "ee/EEAction.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEDoppelganger.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/EECalamityClass.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEStringIDs.h"
#include "ee/EEPPlayerEvent.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESELoadThread.h"
#include "ee/EESaveLoad.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEGameSound.h"
#include "ee/EESoundObject.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEGraphics.h"
#include "ee/EEARust.h"

// ------------------------------------------------------------------------------------------------ the host
// The game's actions' host (ee_screens game_actions.rs: EE_SCREENS_GA_OP_*; ee/EEARust.h). The original units'
// ranges: EEAAddChatMessage 0x1000..0x16c0, EEABootClientFromCanceledGame ..0x173b, EEABuild ..0x1a8d, EEACalamity
// ..0x1de1, EEACenterCamera ..0x1fc4, EEAJoinOrHostAGame 0x2086..0x289f, EEALasso ..0x2a84, EEAMiniMap ..0x2fb0,
// EEAMouseClick ..0x4352, EEAPlayerEvent ..0x4413, EEARemovePlayer ..0x4444, EEAResetAllPlayerInfo ..0x4613,
// EEASaveGame ..0x470f, EEAScroll ..0x4a0b, EEAServerMigration ..0x505b, EEAUpgradeImprovement ..0x5280,
// EEAUserPauseGame ..0x52f3, EEAQuickSaveLoad 0x1a5529..0x1a5881, EESEAQuickSaveLoad 0x1aa70e..0x1aa723.

static_assert(kEEAChat == EE_SCREENS_GA_CHAT && kEEABoot == EE_SCREENS_GA_BOOT && kEEABuild == EE_SCREENS_GA_BUILD &&
                  kEEACalamity == EE_SCREENS_GA_CALAMITY && kEEACenterCamera == EE_SCREENS_GA_CENTER_CAMERA &&
                  kEEAJoin == EE_SCREENS_GA_JOIN && kEEALasso == EE_SCREENS_GA_LASSO &&
                  kEEAMiniMap == EE_SCREENS_GA_MINI_MAP && kEEAMouseClick == EE_SCREENS_GA_MOUSE_CLICK &&
                  kEEAPlayerEvent == EE_SCREENS_GA_PLAYER_EVENT && kEEARemovePlayer == EE_SCREENS_GA_REMOVE_PLAYER &&
                  kEEAResetPlayers == EE_SCREENS_GA_RESET_PLAYERS && kEEASaveGame == EE_SCREENS_GA_SAVE_GAME &&
                  kEEAScroll == EE_SCREENS_GA_SCROLL && kEEAMigration == EE_SCREENS_GA_MIGRATION &&
                  kEEAUpgrade == EE_SCREENS_GA_UPGRADE && kEEAPause == EE_SCREENS_GA_PAUSE &&
                  kEEAQuick == EE_SCREENS_GA_QUICK && kEEAEditorQuick == EE_SCREENS_GA_EDITOR_QUICK &&
                  kEEAEditorClick == EE_SCREENS_GA_EDITOR_CLICK && kEEAEditorLasso == EE_SCREENS_GA_EDITOR_LASSO &&
                  kEEATriggerClick == EE_SCREENS_GA_TRIGGER_CLICK && kEEATriggerLasso == EE_SCREENS_GA_TRIGGER_LASSO &&
                  kEEALoadText == EE_SCREENS_GA_LOAD_TEXT && kEEAEditorPaint == EE_SCREENS_GA_EDITOR_PAINT &&
                  kEEAEditorWhole == EE_SCREENS_GA_EDITOR_WHOLE,
              "ee/EEARust.h's kinds are game_actions.rs's");
static_assert(kMessageSelectUnit == EE_SCREENS_GA_MSG_SELECT_UNIT && kMessageAddUnitToSelectedGroup == EE_SCREENS_GA_MSG_ADD_UNIT &&
                  kMessageUnselectAllUnits == EE_SCREENS_GA_MSG_UNSELECT_ALL && kMessageAttackUnit == EE_SCREENS_GA_MSG_ATTACK &&
                  kMessagePathTo == EE_SCREENS_GA_MSG_PATH_TO && kMessageSelectUnits == EE_SCREENS_GA_MSG_SELECT_UNITS &&
                  kMessageAddUnitsToSelectedGroup == EE_SCREENS_GA_MSG_ADD_UNITS &&
                  kMessageAddAndRemoveUnitsFromSelectedGroup == EE_SCREENS_GA_MSG_ADD_REMOVE_UNITS &&
                  kMessageGatherResource == EE_SCREENS_GA_MSG_GATHER && kMessageBuildUnit == EE_SCREENS_GA_MSG_BUILD_UNIT &&
                  kMessageRepairUnit == EE_SCREENS_GA_MSG_REPAIR && kMessageBuildWall == EE_SCREENS_GA_MSG_BUILD_WALL &&
                  kMessageDropoffResource == EE_SCREENS_GA_MSG_DROPOFF && kMessageSetRallyGoal == EE_SCREENS_GA_MSG_RALLY &&
                  kMessageLoadTransport == EE_SCREENS_GA_MSG_LOAD_TRANSPORT &&
                  kMessageSelectPatrolGroup == EE_SCREENS_GA_MSG_PATROL_GROUP &&
                  kMessageCastCalamity == EE_SCREENS_GA_MSG_CAST_CALAMITY &&
                  kMessageAlwaysAddUnitToSelectedGroup == EE_SCREENS_GA_MSG_ALWAYS_ADD_UNIT &&
                  kMessageImproveUnit == EE_SCREENS_GA_MSG_IMPROVE && kMessageBuildGate == EE_SCREENS_GA_MSG_BUILD_GATE &&
                  kMessageRemoveImprovement == EE_SCREENS_GA_MSG_REMOVE_IMPROVEMENT &&
                  kMessageSmartFarmers == EE_SCREENS_GA_MSG_SMART_FARMERS &&
                  kMessageReturnToBase == EE_SCREENS_GA_MSG_RETURN_TO_BASE && kMessageHealUnit == EE_SCREENS_GA_MSG_HEAL,
              "game_actions.rs's message types");
static_assert(kUnitTypeTownCenter == EE_SCREENS_GA_TYPE_TOWN_CENTER && kUnitTypeDock == EE_SCREENS_GA_TYPE_DOCK &&
                  kUnitTypeCapitol == EE_SCREENS_GA_TYPE_CAPITOL && kUnitTypeFarm == EE_SCREENS_GA_TYPE_FARM &&
                  kUnitTypeSettlement == EE_SCREENS_GA_TYPE_SETTLEMENT && kUnitTypeGranary == EE_SCREENS_GA_TYPE_GRANARY &&
                  kUnitTypeNavyYard == EE_SCREENS_GA_TYPE_NAVY_YARD && kUnitTypeAirport == EE_SCREENS_GA_TYPE_AIRPORT &&
                  kUnitTypeCameraObject == EE_SCREENS_GA_TYPE_CAMERA_OBJECT &&
                  kUnitTypePathPoint == EE_SCREENS_GA_TYPE_PATH_POINT &&
                  kUnitTypeGranaryImproved == EE_SCREENS_GA_TYPE_GRANARY_IMPROVED &&
                  kUnitTypeCpMissileBase == EE_SCREENS_GA_TYPE_CP_MISSILE_BASE &&
                  kUnitCategoryLandCitizen == EE_SCREENS_GA_CATEGORY_LAND_CITIZEN &&
                  kUnitCategoryNavalBuilding == EE_SCREENS_GA_CATEGORY_NAVAL_BUILDING &&
                  kUnitCategoryNavalCitizen == EE_SCREENS_GA_CATEGORY_NAVAL_CITIZEN &&
                  kUnitCategorySpaceBuilding == EE_SCREENS_GA_CATEGORY_SPACE_BUILDING &&
                  kUnitFamilyFish == EE_SCREENS_GA_FAMILY_FISH && kUnitKindCitizen == EE_SCREENS_GA_KIND_CITIZEN &&
                  kUnitKindBuilding == EE_SCREENS_GA_KIND_BUILDING && kUnitKindNavalCitizen == EE_SCREENS_GA_KIND_NAVAL_CITIZEN &&
                  kUnitKindHelicopterTransport == EE_SCREENS_GA_KIND_HELICOPTER_TRANSPORT &&
                  kUnitKindLandTransport == EE_SCREENS_GA_KIND_LAND_TRANSPORT &&
                  kUnitKindNavalTransport == EE_SCREENS_GA_KIND_NAVAL_TRANSPORT &&
                  kUnitKindFortress == EE_SCREENS_GA_KIND_FORTRESS &&
                  kUnitKindSpaceTransport == EE_SCREENS_GA_KIND_SPACE_TRANSPORT &&
                  kGraphicEffectCommandConfirmationMouseClickUnit == EE_SCREENS_GA_EFFECT_CLICK_UNIT &&
                  kGraphicEffectCommandConfirmationMouseClickNoUnit == EE_SCREENS_GA_EFFECT_CLICK_NO_UNIT &&
                  kTechConquistadorsSpell == EE_SCREENS_GA_TECH_CONQUISTADORS_SPELL && kTechNone == EE_SCREENS_GA_TECH_NONE &&
                  kSlotServer == EE_SCREENS_GA_SLOT_SERVER && kSlotNone == EE_SCREENS_GA_SLOT_NONE &&
                  kModifierShift == 1 && kModifierControl == 2 && kResourceNone == 0 &&
                  IDS_PLAYER_NAME_IS_INVALID == EE_SCREENS_GA_IDS_PLAYER_NAME_IS_INVALID &&
                  IDS_GAME_NAME_IS_INVALID == EE_SCREENS_GA_IDS_GAME_NAME_IS_INVALID &&
                  IDS_GAME_NAME_ALREADY_EXISTS_ON_SUBNET == EE_SCREENS_GA_IDS_GAME_NAME_EXISTS,
              "game_actions.rs's world constants");

extern unsigned short& gLagStatus;           // 0x5318d4 (cleared after a migration; provisional name)

GETransformation gMiniMapTransformation;

// A unit argument: an EEUnit*, or with inComplex an EEComplexUnit*
static EEUnit* AsUnit(void* p, int64_t inComplex)
{
    return inComplex ? static_cast<EEUnit*>(static_cast<EEComplexUnit*>(p)) : static_cast<EEUnit*>(p);
}

static int64_t UnitOp(void* p, int64_t* i, float* f)
{
    EEUnit* u = AsUnit(p, i[1]);
    EEComplexUnit* c = i[1] ? static_cast<EEComplexUnit*>(p) : (u != NULL ? u->GetComplexUnit() : NULL);
    switch (i[0]) {
    case EE_SCREENS_GA_U_ID: return u->GetUnitID();
    case EE_SCREENS_GA_U_PLAYER_ID: return u->GetPlayer()->GetPlayerID();
    case EE_SCREENS_GA_U_PLAYER: return (intptr_t)u->GetPlayer();
    case EE_SCREENS_GA_U_COMPLEX: return (intptr_t)u->GetComplexUnit();
    case EE_SCREENS_GA_U_TILE_NODE: return u->GetTileNode() != NULL;
    case EE_SCREENS_GA_U_TILE_X: return u->GetTileX();
    case EE_SCREENS_GA_U_TILE_Y: return u->GetTileY();
    case EE_SCREENS_GA_U_HIT_POINTS: return u->GetHitPoints();
    case EE_SCREENS_GA_U_MAX_HIT_POINTS: return u->GetStats()->GetMaxHitPoints();
    case EE_SCREENS_GA_U_RESOURCE_TYPE: return u->GetResourceType();
    case EE_SCREENS_GA_U_RESOURCE_AMOUNT: return u->GetResourceAmount();
    case EE_SCREENS_GA_U_ANIMAL_ON_MAP: return EEHerd::IsAnimalOnMap(u->GetComplexUnit());
    case EE_SCREENS_GA_U_DISABLED: return c->GetIsDisabled();
    case EE_SCREENS_GA_U_PROTECTED: return c->IsProtected();
    case EE_SCREENS_GA_U_BUILT: return c->mIsBuilt;
    case EE_SCREENS_GA_U_SEEN: return EEIsSeenBy(u, gWorld.GetCurrentPlayer(), true);
    case EE_SCREENS_GA_U_FOOTPRINT_VISIBLE: return EEIsFootprintVisibleTo(u, gWorld.GetCurrentPlayer());
    case EE_SCREENS_GA_U_TILE: return (intptr_t)u->GetRTSTile();
    case EE_SCREENS_GA_U_LOCATION:
        f[0] = c->GetPhysicalModel().GetLocation().GetX();
        f[1] = c->GetPhysicalModel().GetLocation().GetY();
        return 0;
    case EE_SCREENS_GA_U_POSITION: {
        GE3DPoint thePosition;
        c->mPhysicalModel.GetLocation().GetPosition(thePosition);
        f[0] = thePosition.GetX();
        f[1] = thePosition.GetY();
        f[2] = thePosition.GetZ();
        return 0;
    }
    case EE_SCREENS_GA_U_CLASS_PTR: return (intptr_t)u->GetClass();
    case EE_SCREENS_GA_U_CONTAINER: return (intptr_t)c->mContainer;
    case EE_SCREENS_GA_U_TILE_HEIGHT: f[0] = u->mTile->mHeight; return 0;
    case EE_SCREENS_GA_U_IMPROVEMENT_LEVEL: return u->mStats->mImprovement.GetLevel(i[2]);
    case EE_SCREENS_GA_U_HANDLE: return EEUnitHandleID(u);
    case EE_SCREENS_GA_U_RESOLVE: return (intptr_t)EEUnitHandle<EEUnit>::FromSavedID((uint32_t)i[2]).Get();
    }
    return 0;
}

static int64_t ClassOp(void* p, int64_t* i)
{
    EEUnitClass* c = AsUnit(p, i[1])->GetUnitClass();
    switch (i[0]) {
    case EE_SCREENS_GA_C_TYPE: return c->GetUnitType();
    case EE_SCREENS_GA_C_CATEGORY: return c->GetCategory();
    case EE_SCREENS_GA_C_FAMILY: return c->GetFamily();
    case EE_SCREENS_GA_C_ANIMAL_ID: return c->GetAnimalID();
    case EE_SCREENS_GA_C_AIR_TRAFFIC: return c->GetAirTrafficType();
    case EE_SCREENS_GA_C_CAN_RALLY: return c->GetCanSetRallyPoint();
    case EE_SCREENS_GA_C_TOWER: return c->GetIsTower();
    case EE_SCREENS_GA_C_TREE: return c->GetIsTree();
    case EE_SCREENS_GA_C_LAND_BUILDING: return c->IsLandBuilding();
    case EE_SCREENS_GA_C_DROP_SITE: return c->IsDropSite();
    case EE_SCREENS_GA_C_TRANSPORT: return c->IsTransport();
    case EE_SCREENS_GA_C_AIR_BASE: return c->IsAirBase();
    case EE_SCREENS_GA_C_MEDIC: return c->GetIsMedic();
    case EE_SCREENS_GA_C_TARGETABLE: return EEUUGetIsUnitClassTargetable(NULL, c);
    }
    return 0;
}

static int64_t GroupOp(void* p, int64_t* i, float* f)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EESelectedGroup* g = thePlayer->mSelectedGroup;
    switch (i[0]) {
    case EE_SCREENS_GA_G_COUNT: return g->GetNumUnits();
    case EE_SCREENS_GA_G_KIND_FLAGS: return (uint32_t)g->mKindFlags;
    case EE_SCREENS_GA_G_COUNT_KIND: return g->CountUnitsOfKind(i[1]);
    case EE_SCREENS_GA_G_UNIT: return (intptr_t)g->GetUnit(i[1]);
    case EE_SCREENS_GA_G_TYPE_COUNT: return g->GetUnitTypeCount(i[1]);
    case EE_SCREENS_GA_G_FIND_CATEGORY: return (intptr_t)g->FindUnitOfCategory(i[1]);
    case EE_SCREENS_GA_G_FIND_FLAGS: return (intptr_t)g->FindUnitWithFlags(i[1]);
    case EE_SCREENS_GA_G_CONTAINS: return g->Contains(static_cast<EEComplexUnit*>(p));
    case EE_SCREENS_GA_G_FOREST: {
        EECPASGFlags theFlags = g->GetFlags();
        return theFlags.mFlags[kAITileForest] == true;
    }
    case EE_SCREENS_GA_G_CENTER: {
        GERectangle theBounds;
        GE2DPoint theCenter;
        g->GetBounds(theBounds);
        theBounds.GetCenter(theCenter);
        f[0] = theCenter.GetX();
        f[1] = theCenter.GetY();
        return 0;
    }
    case EE_SCREENS_GA_G_FIRST_ID: return g->GetFirstUnit()->GetUnitID();
    case EE_SCREENS_GA_G_ACTIVE_COUNT: return thePlayer->mActiveGroup->GetNumUnits();
    case EE_SCREENS_GA_G_ACTIVE_UNIT: return (intptr_t)thePlayer->mActiveGroup->GetUnit(i[1]);
    }
    return 0;
}

static int64_t MapOp(int64_t* i, float* f)
{
    EEMap* theMap = gWorld.mMap;
    switch (i[0]) {
    case EE_SCREENS_GA_M_VALID_TILE:
        return gWorld.GetMap()->GetTerrainMesh().GetIsValidTile((unsigned long)(long)i[1], (unsigned long)(long)i[2]);
    case EE_SCREENS_GA_M_MESH_Z: f[0] = theMap->mTerrainMesh.GetMeshZ(f[0], f[1]); return 0;
    case EE_SCREENS_GA_M_TILE: {
        EEMapTile* theTile = theMap->GetTile(i[1], i[2]);
        if (theTile == NULL)
            return 0;
        f[0] = theTile->mHeight;
        return 1;
    }
    case EE_SCREENS_GA_M_CLOSEST: {
        GE3DPoint thePoint(f[0], f[1], 0.0f);
        long theX = 0, theY = 0;
        theMap->GetClosestTile(&thePoint, &theX, &theY);
        i[0] = theX;
        i[1] = theY;
        return 0;
    }
    case EE_SCREENS_GA_M_PLANE: {
        const GE3DPlane& thePlane = i[1] == 0   ? theMap->mTerrainMesh.GetLeftPlane()
                                    : i[1] == 1 ? theMap->mTerrainMesh.GetRightPlane()
                                    : i[1] == 2 ? theMap->mTerrainMesh.GetFrontPlane()
                                                : theMap->mTerrainMesh.GetBackPlane();
        f[0] = thePlane.GetPoint().GetX();
        f[1] = thePlane.GetPoint().GetY();
        f[2] = thePlane.GetPoint().GetZ();
        f[3] = thePlane.GetNormal().GetX();
        f[4] = thePlane.GetNormal().GetY();
        f[5] = thePlane.GetNormal().GetZ();
        return 0;
    }
    case EE_SCREENS_GA_M_EXISTS: return theMap != NULL;
    }
    return 0;
}

// The join's and the migration's connection request (the original's NEMClientConnectionRequest)
static NEMessage* NewConnectionRequest(EEARust* a, bool inBehindFirewall)
{
    return new NEMRecord(kNetMessageClientConnectionRequest, {inBehindFirewall, gBinaryChecksum}, {gGameVersion, a->mPlayerName});
}

static int64_t NetOp(EEARust* a, int64_t* i)
{
    UWideString theMessage;
    UWideString theFormat;
    switch (i[0]) {
    case EE_SCREENS_GA_N_LOBBY_CURRENT: return gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormLobby);
    case EE_SCREENS_GA_N_CHAT_SCROLL:
    case EE_SCREENS_GA_N_CHAT_APPEND:
    case EE_SCREENS_GA_N_CHAT_TO_END: {
        UITextLabel* theChat = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormLobby)->GetControl(kLobbyChatPreviousMessages));
        if (i[0] == EE_SCREENS_GA_N_CHAT_APPEND)
            theChat->AppendText(a->mText);
        else if (theChat->GetScrollBar() == NULL)
            return 0;
        else if (i[0] == EE_SCREENS_GA_N_CHAT_TO_END)
            theChat->SetStartingDisplayLine(theChat->GetScrollBar()->GetSlider()->GetMaxRange());
        else {
            i[0] = theChat->GetScrollBar()->GetSlider()->GetMaxRange();
            i[1] = theChat->GetScrollBar()->GetSlider()->GetCurrentRangeValue();
            return 1;
        }
        return 0;
    }
    case EE_SCREENS_GA_N_WON_REJOIN:
        if (gWonNet) {
            gWonNet->ClearGameList();
            gWonNet->RejoinRoom(false);
        }
        return 0;
    case EE_SCREENS_GA_N_POP: gUIManager->Pop(); return 0;
    case EE_SCREENS_GA_N_SHOW_STRING:
        gResource->GetString(i[1], theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, i[2] ? EEAJoinOrHostAGameRestartClient : NULL, true);
        return 0;
    case EE_SCREENS_GA_N_IS_SERVER: return gCommunications->IsServer() == true;
    case EE_SCREENS_GA_N_BOOT_PLAYER:
        static_cast<NESynchronousServer*>(gCommunications)->BootPlayer(a->mPlayer, a->mB0D, a->mIsExiting);
        return 0;
    case EE_SCREENS_GA_N_CTS_PROCESSED: return gCommunications->GetNumberOfCTsProcessedInPhysics() > 0;
    case EE_SCREENS_GA_N_USER_PAUSED: return gWorld.mUserPaused == true;
    case EE_SCREENS_GA_N_USER_PAUSE: {
        EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
        if (i[1])
            theServer->PauseGame();
        else
            theServer->UnpauseGame();
        gUserInterface->PostPauseInformation(i[1] != 0);
        gWorld.mUserPaused = i[1] != 0;
        return 0;
    }
    case EE_SCREENS_GA_N_FIREWALL: return gWonNet ? gWonNet->mIsBehindFirewall == 1 : false;
    case EE_SCREENS_GA_N_VALID_NAME: return EEIsValidName(i[1] ? a->mGameName : a->mPlayerName);
    case EE_SCREENS_GA_N_COMMS: return gCommunications != NULL;
    case EE_SCREENS_GA_N_DELETE_COMMS:
        if (gCommunications) {
            delete gCommunications;
            gCommunications = NULL;
        }
        return 0;
    case EE_SCREENS_GA_N_NEW_CLIENT:
        gCommunications = new EEClient;
        if (i[1])
            dynamic_cast<EEClient*>(gCommunications)->StartThreads();
        else
            static_cast<EEClient*>(gCommunications)->StartThreads();
        return 0;
    case EE_SCREENS_GA_N_NEW_SERVER:
        gCommunications = new EEServer(a->mGameName, gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, i[1] != 0);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        return 0;
    case EE_SCREENS_GA_N_PUBLISHED: return static_cast<EEServer*>(gCommunications)->GetGameWasPublished();
    case EE_SCREENS_GA_N_REMEMBER_HOST:
        gPlayerName = a->mPlayerName;
        gGamePreferences.mSettings.mHostGameName = a->mGameName;
        return 0;
    case EE_SCREENS_GA_N_OPTIONS:
        gGameOptions.mLock->WriteLock();
        gGameOptions = gGamePreferences.GetLastGameOptions();
        gGameOptions.mIsMultiplayer = true;
        gGameOptions.mCheatCodes = false;
        gGameOptions.mLock->WriteUnlock();
        return 0;
    case EE_SCREENS_GA_N_CONNECT_ADDRESS:
    case EE_SCREENS_GA_N_RECONNECT: {
        if (i[0] == EE_SCREENS_GA_N_RECONNECT) {
            PlatSleep(7000);
            gResource->GetString(IDS_HOST_IS_GONE, theFormat);
            sprintf(theMessage, theFormat, (const char*)a->mText);
            gUserInterface->AddChatLogEntry(theMessage, 0);
            gResource->GetString(IDS_ATTEMPTING_TO_CONNECT_TO_NEW_HOST, theFormat);
            sprintf(theMessage, theFormat, (const char*)a->mGameName);
            gUserInterface->AddChatLogEntry(theMessage, 0);
        }
        UString theAddress;
        UString theService;
        UWideString theGameName;
        if (i[0] == EE_SCREENS_GA_N_CONNECT_ADDRESS)
            theAddress = a->mGameName;
        else
            theAddress = a->mName;
        gCommunications->SearchForGame(false);
        theService = "EEService";
        theGameName = "";
        return static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName);
    }
    case EE_SCREENS_GA_N_REQUEST: {
        NEMessage* theRequest = NewConnectionRequest(a, i[1] != 0);
        if (i[2])
            static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(theRequest);
        gCommunications->ProcessLocalUserInput(theRequest, kSlotNone);
        return 0;
    }
    case EE_SCREENS_GA_N_LOBBY_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(i[1] ? EE_SCREENS_LE_ACTIVATE_CLIENT : EE_SCREENS_LE_ACTIVATE_SERVER);
        gUIManager->GetFormByID(kFormLobby)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_GA_N_SET_NAME:
        if (i[1])
            gGamePreferences.mSettings.mSierraUserID = a->mPlayerName;
        else
            gPlayerName = a->mPlayerName;
        return 0;
    case EE_SCREENS_GA_N_DETECTED_COUNT: return static_cast<NESynchronousClient*>(gCommunications)->GetDetectedGameCount();
    case EE_SCREENS_GA_N_DETECTED:
        return static_cast<NESynchronousClient*>(gCommunications)->GetDetectedGame(i[1], a->mFileName, a->mDescription, a->mDetectedName);
    case EE_SCREENS_GA_N_DETECTED_MATCH: return a->mDescription == "EEService" && a->mDetectedName == a->mGameName;
    case EE_SCREENS_GA_N_CONNECT_DETECTED:
        return static_cast<NESynchronousClient*>(gCommunications)->Connect(a->mFileName, a->mDescription, a->mDetectedName);
    case EE_SCREENS_GA_N_MIGRATE_BEGIN:
        a->mText = UWideString();       // the dropped host's name, the new host's and its address: empty
        a->mGameName = UWideString();
        a->mName = UString();
        gPhysics->RunOwedTicks();
        static_cast<EEClient*>(gCommunications)->mIsMigrating = true;
        return 0;
    case EE_SCREENS_GA_N_HAS_LEFT: return static_cast<EEClient*>(gCommunications)->mHasLeftGame != 0;
    case EE_SCREENS_GA_N_NUM_PLAYERS: return gWorld.mNumPlayers;
    case EE_SCREENS_GA_N_PLAYER: {
        EEPlayer* thePlayer = gWorld.mPlayers[i[1]];
        i[0] = thePlayer->mSlot;
        i[1] = thePlayer->mBehindFirewall != 0;
        i[2] = thePlayer->mHumanPlayer != 0;
        i[3] = gWorld.GetCurrentPlayer() == thePlayer;
        return 0;
    }
    case EE_SCREENS_GA_N_LOADING_SAVED: return gLoadingSavedGame != 0;
    case EE_SCREENS_GA_N_CLEAR_SELECTIONS:
        for (unsigned long k = 0; k < gWorld.mNumPlayers; k++)
            gWorld.mPlayers[k]->mActiveGroup->Clear();
        gWorld.GetCurrentPlayer()->mSelectedGroup->CopySelection(gWorld.GetCurrentPlayer()->mActiveGroup);
        static_cast<EEClient*>(gCommunications)->ResetFastSelection();
        return 0;
    case EE_SCREENS_GA_N_SET_SLOT: gWorld.mPlayers[i[1]]->mSlot = (unsigned char)i[2]; return 0;
    case EE_SCREENS_GA_N_KEEP_NAME: {
        EEPlayer* thePlayer = gWorld.mPlayers[i[1]];
        if (i[2] == 0) {
            a->mText = thePlayer->mName;
        } else {
            a->mName = static_cast<EEHumanPlayer*>(thePlayer)->GetName();
            a->mGameName = thePlayer->mName;
        }
        return 0;
    }
    case EE_SCREENS_GA_N_PROMOTE: {
        gResource->GetString(IDS_HOST_IS_GONE, theFormat);
        sprintf(theMessage, theFormat, (const char*)a->mText);
        gUserInterface->AddChatLogEntry(theMessage, 0);
        gResource->GetString(IDS_YOU_ARE_BEING_PROMOTED_TO_HOST, theMessage);
        gUserInterface->AddChatLogEntry(theMessage, 0);
        unsigned long theTick = gCommunications->GetNumberOfCTsProcessedInPhysics();
        unsigned long theCurrentTick = gPhysics->mCurrentTick;
        unsigned char theSelectionID = static_cast<EEClient*>(gCommunications)->mFastSelectionID;
        if (gCommunications) {
            delete gCommunications;
            gCommunications = NULL;
        }
        gCommunications = new EEServer(UWideString("Migrated Server"), gGamePreferences.mSettings.GetNetworkAdapter(), theTick, theCurrentTick, false, false);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        gPhysics->ClearServerMessages();
        gCommunications->SetIsMultiplayerSession(true);
        static_cast<EEServer*>(gCommunications)->SetSendToSlotForPlayer(gWorld.mCurrentPlayer, kSlotServer);
        gWorld.GetCurrentPlayer()->mSlot = 0xfe;
        static_cast<EEServer*>(gCommunications)->mFastSelectionID = theSelectionID;
        static_cast<EEServer*>(gCommunications)->mIsMigrating = true;
        return 0;
    }
    case EE_SCREENS_GA_N_RECONNECT_WAIT:
        if (i[1] == 0)
            static_cast<EEServer*>(gCommunications)->HandleReconnectWaitTimedOut();
        else
            static_cast<EEServer*>(gCommunications)->StartWaitingForReconnects(i[1]);
        return 0;
    case EE_SCREENS_GA_N_RECONNECT_REQUEST: {
        gCommunications->SetLastTickReceived(-2);
        static_cast<NESynchronousClient*>(gCommunications)->SetMySendToSlot(kSlotNone);
        NEMessage* theRequest = new NEMRecord(kNetMessageClientReconnectionRequest,   // the tick, the player
            {gCommunications->GetNumberOfCTsProcessedInPhysics(), gWorld.mCurrentPlayer});
        gCommunications->ProcessLocalUserInput(theRequest, kSlotNone);
        return 0;
    }
    case EE_SCREENS_GA_N_LAG_CLEAR: gLagStatus = 0; return 0;
    }
    return 0;
}

// the quick save's file in data\Scenarios
static void QuickSaveSpec(FSFileSpec& outFile, const UString& inName)
{
    outFile.SetVolume(gStartupVolume);
    outFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    outFile.SetFile(inName);
}

// the trigger tab's object screen's UpdateSelection (Scenario Editor Actions/EESEARust.cpp)
void EESEAObjectSelection();

static int64_t EditorOp(EEARust* a, void* p, int64_t* i, float* f)
{
    switch (i[0]) {
    case EE_SCREENS_GA_E_JIT: gUserInterface->UpdateJITManufacturing(); return 0;
    case EE_SCREENS_GA_E_APPLY_IMPROVEMENT: {
        EEUnit* theUnit = static_cast<EEUnit*>(p);
        theUnit->GetPlayer()->ApplyImprovement(theUnit->GetUnitClass()->GetUnitType(), a->mImprovement, i[1] != 0);
        if (i[1] == 0)
            gUIManager->mScenarioEditorForm->GetUnitScreen()->ShowUnitStatus(theUnit);
        else
            static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->ShowUnitStatus(theUnit);
        gUIManager->mScenarioEditorForm->UpdateHelpString();
        return 0;
    }
    case EE_SCREENS_GA_E_SOUND:
        gSoundManager->Play(EESoundObject::Get(i[1] == 1 ? kSoundButton1 : kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_SCREENS_GA_E_RESET_COUNT: return a->mPlayers.size();
    case EE_SCREENS_GA_E_RESET_APPLY: {
        EEPlayer* thePlayer = gWorld.mPlayers[a->mPlayers[i[1]]];
        EEApplyCivilizationFile(gWorld.mPlayers[a->mPlayers[i[1]]]->mCivilizationFile, thePlayer);
        long theCivilization = gWorld.mPlayers[a->mPlayers[i[1]]]->GetTechTree()->mStartEpoch;
        gUIManager->mScenarioEditorForm->mLoadThread->Post406(reinterpret_cast<void*>(a->mPlayers[i[1]]), reinterpret_cast<void*>(theCivilization));
        return 0;
    }
    case EE_SCREENS_GA_E_RESET_POST403:
        gUIManager->GetScenarioEditorForm()->mLoadThread->Post403(reinterpret_cast<void*>(a->mPlayers[i[1]]), false);
        return 0;
    case EE_SCREENS_GA_E_RESET_POST407:
        gUIManager->GetScenarioEditorForm()->mLoadThread->Post407(
            reinterpret_cast<void*>(a->mPlayers[i[1]]), reinterpret_cast<void*>(gWorld.mPlayers[a->mPlayers[i[1]]]->GetTechTree()->mStartEpoch));
        return 0;
    case EE_SCREENS_GA_E_SAVE: return SaveGame(a->mFileName, TMGetTime(), a->mFlag, a->mDescription);
    case EE_SCREENS_GA_E_CLOSE_FORM: gUserInterface->CloseForm(NULL); return 0;
    case EE_SCREENS_GA_E_SAVE_ERROR: gUIManager->DisplayMessage(gLoadSaveErrorMessage, true, NULL, NULL, true); return 0;
    case EE_SCREENS_GA_E_PAUSED: return gCommunications->GetIsGamePaused();
    case EE_SCREENS_GA_E_MUTE:
        f[0] = gSoundManager->GetMasterVolume();
        f[1] = gConductor->GetConductor().GetVolume();
        gSoundManager->SetMasterVolume(0.0f);
        gConductor->GetConductor().SetVolume(0.0f);
        return 0;
    case EE_SCREENS_GA_E_SERVER_PAUSE:
        if (i[1])
            static_cast<NESynchronousServer*>(gCommunications)->PauseGame();
        else
            static_cast<NESynchronousServer*>(gCommunications)->UnpauseGame();
        gUserInterface->PostPauseInformation(i[1] != 0);
        return 0;
    case EE_SCREENS_GA_E_QUICK_LOAD: {
        FSFileSpec theFile;
        QuickSaveSpec(theFile, UString("EEQuickSave") + a->mName);
        if (LoadSavedGameFile(theFile, true, true, true) != 0)
            gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
        return 0;
    }
    case EE_SCREENS_GA_E_QUICK_SAVE: {
        FSMemoryFile* theFile = new FSMemoryFile(0x400, true, 1000);
        a->mFileName = GetSaveTextName(UString("EEQuickSave") + a->mName, true);
        WriteSaveGame(a->mFileName, TMGetTime(), *theFile, *theFile);
        return (intptr_t)theFile;
    }
    case EE_SCREENS_GA_E_QUICK_WRITE: {
        FSFileSpec theFile;
        QuickSaveSpec(theFile, a->mFileName);
        theFile.Create();
        if (theFile.Open(kFileAccessWrite) == 0) {
            static_cast<FSMemoryFile*>(p)->Save(theFile);
            theFile.Close();
        }
        return 0;
    }
    case EE_SCREENS_GA_E_UNMUTE:
        gSoundManager->SetMasterVolume(f[0]);
        gConductor->GetConductor().SetVolume(f[1]);
        gPhysics->mTicksOwed = 0;
        return 0;
    case EE_SCREENS_GA_E_RESUME_GROUP: gUIManager->mScenarioEditorForm->ResumeCurrentGroup(); return 0;
    case EE_SCREENS_GA_E_DELETE_FILE: delete static_cast<FSMemoryFile*>(p); return 0;
    case EE_SCREENS_GA_E_OBJECT_SELECTION: EESEAObjectSelection(); return 0;
    case EE_SCREENS_GA_E_LOAD_TEXT: gUIManager->mScenarioEditorForm->SetLoadText(a->mText); return 0;
    }
    return 0;
}

extern "C" int64_t ee_screenscpp_ga(uint32_t inOp, EeScreensGaFields* r, void* p, int64_t* i, float* f)
{
    EEARust* a = static_cast<EEARust*>(r);
    switch (inOp) {
    case EE_SCREENS_GA_OP_POST: gUserInterface->PostAction(a); return 0;
    case EE_SCREENS_GA_OP_SEND: gCommunications->ProcessLocalUserInput(static_cast<EEMessage*>(p), kSlotNone); return 0;
    case EE_SCREENS_GA_OP_MESSAGE_RELEASE: static_cast<EEMessage*>(p)->Dereference(); return 0;
    case EE_SCREENS_GA_OP_SELECT_UNITS:   // the box selection's record (0x4e, 0x4f, 0x50)
        return (intptr_t) static_cast<EEMessage*>(EENewSelectUnits(*static_cast<const GERectangle*>(p), i[1], (unsigned char)i[0]));
    case EE_SCREENS_GA_OP_CLASS_COUNT: return EEUnitClassCount();
    case EE_SCREENS_GA_OP_SAVED_GROUP_RESET: gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x15, 1); return 0;
    case EE_SCREENS_GA_OP_MOUSE:
        f[0] = gMouse->mPosition.GetX();
        f[1] = gMouse->mPosition.GetY();
        return 0;
    case EE_SCREENS_GA_OP_MOUSE_FROZEN: return gMouse->mIsFrozen != 0;
    case EE_SCREENS_GA_OP_PICK: return (intptr_t)gUnitPicker.PickUnit(GE2DPoint(f[0], f[1]), i[0] != 0);
    case EE_SCREENS_GA_OP_UNIT_REFERENCE: static_cast<EEUnit*>(p)->Reference(); return 0;
    case EE_SCREENS_GA_OP_UNIT_RELEASE: static_cast<EEUnit*>(p)->Dereference(); return 0;
    case EE_SCREENS_GA_OP_UNIT: return UnitOp(p, i, f);
    case EE_SCREENS_GA_OP_CLASS: return ClassOp(p, i);
    case EE_SCREENS_GA_OP_GROUP: return GroupOp(p, i, f);
    case EE_SCREENS_GA_OP_PLAYER: {
        EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
        switch (i[0]) {
        case EE_SCREENS_GA_P_PTR: return (intptr_t)thePlayer;
        case EE_SCREENS_GA_P_ID: return thePlayer->GetPlayerID();
        case EE_SCREENS_GA_P_ENEMY: return thePlayer->GetEnemy(i[1]);
        case EE_SCREENS_GA_P_CURRENT_ID: return gWorld.mPlayers[gWorld.mCurrentPlayer]->mPlayerID;
        case EE_SCREENS_GA_P_NEXT_EVENT: {
            EEPPlayerEvent* theEvent = thePlayer->mEvents.GetNextEvent();
            if (!theEvent)
                return 0;
            EETileLocation theSource = theEvent->mSource;
            i[0] = theSource.GetX();
            i[1] = theSource.GetY();
            return 1;
        }
        }
        return 0;
    }
    case EE_SCREENS_GA_OP_FIRST_SELECTED: {
        EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(i[0]);
        bool theFound = EEGetFirstSelectedUnit(theUnit);
        i[0] = (intptr_t)theUnit;
        return theFound;
    }
    case EE_SCREENS_GA_OP_POINT_TILE: {
        EETile* theTile = reinterpret_cast<EETile*>(i[1]);
        long theX = (long)i[2], theY = (long)i[3];
        bool theFound = EEPointUnitOrTileXY(GE2DPoint(f[0], f[1]), theTile, theX, theY, i[0] != 0);
        i[1] = (intptr_t)theTile;
        i[2] = theX;
        i[3] = theY;
        return theFound;
    }
    case EE_SCREENS_GA_OP_TILE: {
        EETile* theTile = static_cast<EETile*>(p);
        switch (i[0]) {
        case EE_SCREENS_GA_T_DOPPEL_COUNT: return theTile->GetNumDoppelgangers();
        case EE_SCREENS_GA_T_DOPPEL_PLAYER: return (intptr_t)theTile->GetDoppelganger(i[1])->mPlayer;
        case EE_SCREENS_GA_T_DOPPEL_UNIT: return (intptr_t)theTile->GetDoppelganger(i[1])->mUnit;
        case EE_SCREENS_GA_T_VISIBLE: return theTile->IsVisibleTo(gWorld.GetCurrentPlayer());
        case EE_SCREENS_GA_T_UNIT_AT: {
            RTSTileUnitNode* theNode = theTile->mUnits;
            for (int64_t k = 0; theNode != NULL && k < i[1]; k++)
                theNode = theNode->mNext;
            return theNode != NULL ? (intptr_t)theNode->mUnit : 0;
        }
        }
        return 0;
    }
    case EE_SCREENS_GA_OP_CAN_GATHER: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(i[0]);
        return static_cast<EEComplexUnit*>(p)->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), false, false);
    }
    case EE_SCREENS_GA_OP_FARM_DROP_SITES: {
        std::vector<EEUnit*> theFarmers;
        EEGetFarmDropSites(static_cast<EEComplexUnit*>(p), theFarmers);
        if (theFarmers.size() == 1)
            i[0] = theFarmers[0]->GetUnitID();
        return theFarmers.size();
    }
    case EE_SCREENS_GA_OP_CAN_HEAL: return EECanHeal(static_cast<EEComplexUnit*>(p), reinterpret_cast<EEComplexUnit*>(i[0]));
    case EE_SCREENS_GA_OP_MAP: return MapOp(i, f);
    case EE_SCREENS_GA_OP_EFFECT: {
        EEEffectHandle theHandle;
        EEGraphicEffect* theGraphicEffect = EEGraphicEffect::Get(i[0]);
        GE3DPoint theLocation(f[0], f[1], f[2]);
        gEffectManager->CreateEffect(static_cast<EEUnit*>(p), theLocation, theGraphicEffect, &theHandle, 0);
        EEEffect* theEffect = gEffectManager->GetEffect(theHandle);
        theEffect->SetIsLocalOnly(true);
        theEffect->SetPlayer(i[1]);
        theEffect->SetIsSaved(false);
        return 0;
    }
    case EE_SCREENS_GA_OP_WORLD_UNIT: return (intptr_t) static_cast<EEUnit*>(gWorld.GetUnit(i[0]));
    case EE_SCREENS_GA_OP_TECH: {
        EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
        switch (i[0]) {
        case EE_SCREENS_GA_K_CAN_PRODUCE: return thePlayer->CanProduce(i[1], 0x20, 1, true, 3, NULL);
        case EE_SCREENS_GA_K_TILE_SIZE: {
            EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(i[1])->mData->mUnitClass];
            return theClass ? theClass->GetTileSize() : -1;
        }
        case EE_SCREENS_GA_K_NODE_CONTAINS: return thePlayer->mTechnologies->Get(i[1])->Contains(i[2]);
        }
        return 0;
    }
    case EE_SCREENS_GA_OP_CALAMITY: {
        EECalamityClass* theClass = static_cast<EECalamityClass*>(p);
        GE3DPoint thePoint(f[0], f[1], f[2]);
        int64_t theResult = 0;
        switch (i[0]) {
        case EE_SCREENS_GA_A_WORLD_POINT: {
            GETransformation theTransformation(gWorld.mTransformation);
            GE3DPoint theWorldPoint;
            gWorld.ScreenToWorld(GE2DPoint(f[0], f[1]), theWorldPoint);
            theTransformation.Apply(theWorldPoint, thePoint);
            break;
        }
        case EE_SCREENS_GA_A_DEFAULT_TARGET: theResult = theClass->GetDefaultTarget(thePoint, i[1], i[2], i[3] != 0); break;
        case EE_SCREENS_GA_A_ORDER_AT_CURSOR: EEOrderAtCursor(GE2DPoint(f[0], f[1]), reinterpret_cast<EEUnit*>(i[1])); return 0;
        case EE_SCREENS_GA_A_CAN_CAST:
            theResult = theClass->CanCastAt(reinterpret_cast<EEComplexUnit*>(i[1]), thePoint, i[2], i[3], i[4] != 0, reinterpret_cast<EEUnit*>(i[5]));
            break;
        }
        f[0] = thePoint.GetX();
        f[1] = thePoint.GetY();
        f[2] = thePoint.GetZ();
        return theResult;
    }
    case EE_SCREENS_GA_OP_CAMERA: {
        EECameraControl& theCamera = gWorld.mCameraControl;
        switch (i[0]) {
        case EE_SCREENS_GA_CAM_SET_LOCATION: theCamera.SetLocation(GE3DPoint(f[0], f[1], f[2]), false, true); return 0;
        case EE_SCREENS_GA_CAM_KEEP_ZOOM: theCamera.SetZoom(theCamera.GetZoom()); return 0;
        case EE_SCREENS_GA_CAM_CHANGED: theCamera.SetChanged(); return 0;
        case EE_SCREENS_GA_CAM_FOLLOW: {
            EEUnit* theUnit = static_cast<EEUnit*>(p);
            GE3DPoint theNoLocation(-10000.0f, 0.0f, 0.0f);
            theCamera.SetZoom(0.0f);
            theCamera.SetMode(3);
            theCamera.MoveTo(theUnit->mComplexUnit, theNoLocation, false, 0.0f, theUnit->mComplexUnit, theNoLocation, false, 0.0f,
                             0.0f, 0.0f);
            theCamera.SetChanged();
            theCamera.Update(0.0f);
            theCamera.SetMode(2);
            return 0;
        }
        case EE_SCREENS_GA_CAM_ZOOM: f[0] = theCamera.mZoom; return 0;
        case EE_SCREENS_GA_CAM_SCROLL: theCamera.Scroll(f[0], f[1]); return 0;
        case EE_SCREENS_GA_CAM_FOLLOWING_OFF: gCameraFollowing = false; return 0;
        }
        return 0;
    }
    case EE_SCREENS_GA_OP_MINI_MAP:
        if (i[0] == EE_SCREENS_GA_MM_REFRESH) {
            gMiniMap->Refresh();
        } else {
            GE2DPoint theLocation;
            gMiniMapTransformation.Apply(a->mPoint, theLocation);
            f[0] = theLocation.GetX();
            f[1] = theLocation.GetY();
        }
        return 0;
    case EE_SCREENS_GA_OP_SCROLL:
        switch (i[0]) {
        case EE_SCREENS_GA_S_SPEED: f[0] = gGamePreferences.mSettings.GetSideScrollSpeed(); return 0;
        case EE_SCREENS_GA_S_EDGES:
            f[0] = a->mEdges.GetLeft();
            f[1] = a->mEdges.GetTop();
            f[2] = a->mEdges.GetRight();
            f[3] = a->mEdges.GetBottom();
            return 0;
        case EE_SCREENS_GA_S_POLL_RUNNING: return a->mPoll.GetIsRunning();
        case EE_SCREENS_GA_S_POLL: return a->mPoll.Poll();
        case EE_SCREENS_GA_S_POLL_START: a->mPoll.Start(); return 0;
        case EE_SCREENS_GA_S_POLL_STOP: a->mPoll.Stop(); return 0;
        }
        return 0;
    case EE_SCREENS_GA_OP_NET: return NetOp(a, i);
    case EE_SCREENS_GA_OP_EDITOR: return EditorOp(a, p, i, f);
    case EE_SCREENS_GA_OP_ACTION: return (intptr_t) static_cast<EEAction*>(a);
    }
    return 0;
}

// A record message: its members in wire order (EEMRecordValue's conversions)
extern "C" void* ee_screenscpp_ga_record(uint32_t inKind, const EeScreensGaValue* inValues, uint32_t inCount)
{
    EEMRecordValue v[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (uint32_t k = 0; k < inCount && k < 8; k++) {
        const EeScreensGaValue& theValue = inValues[k];
        if (theValue.kind == EE_SCREENS_GA_VALUE_POINT)
            v[k] = EEMRecordValue(GE2DPoint(theValue.f[0], theValue.f[1]));
        else if (theValue.kind == EE_SCREENS_GA_VALUE_FLOAT)
            v[k] = EEMRecordValue((double)theValue.f[0]);
        else
            v[k] = EEMRecordValue((long long)theValue.i);
    }
    unsigned char theType = (unsigned char)inKind;
    EEMessage* theMessage;
    switch (inCount) {
    case 0: theMessage = new EEMRecord(theType); break;
    case 1: theMessage = new EEMRecord(theType, {v[0]}); break;
    case 2: theMessage = new EEMRecord(theType, {v[0], v[1]}); break;
    case 3: theMessage = new EEMRecord(theType, {v[0], v[1], v[2]}); break;
    case 4: theMessage = new EEMRecord(theType, {v[0], v[1], v[2], v[3]}); break;
    case 5: theMessage = new EEMRecord(theType, {v[0], v[1], v[2], v[3], v[4]}); break;
    case 6: theMessage = new EEMRecord(theType, {v[0], v[1], v[2], v[3], v[4], v[5]}); break;
    default: theMessage = new EEMRecord(theType, {v[0], v[1], v[2], v[3], v[4], v[5], v[6]}); break;
    }
    return theMessage;
}

// ------------------------------------------------------------------------------------------------ the originals' COMDATs
// (out of line in these units after their first users)

// Message box callback: replaces the communications object with a freshly started client
void EEAJoinOrHostAGameRestartClient(UIForm* inForm)
{
    if (gCommunications) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEClient;
    static_cast<EEClient*>(gCommunications)->StartThreads();
}

// Returns whether a player name is non-empty and free of control, quote, backslash and @ characters
bool EEIsValidName(const UWideString& inName)
{
    // the rule is Rust's (ee_screens single_player.rs valid_name), over each character's first byte
    unsigned long theLength = inName.GetLength();
    std::vector<unsigned char> theChars(theLength);
    for (unsigned long i = 0; i < theLength; i++)
        theChars[i] = (unsigned char)inName[(int)i];
    return ee_screens_valid_name(theChars.data(), theLength);
}

// Returns the group's first unit, or NULL when the group is empty
EEComplexUnit* EEGroup::GetFirstUnit()
{
    if (this->GetNumUnits())
        return this->GetUnit(0);
    return NULL;
}

// Returns whether the unit class is a building that handles aircraft (an air base or carrier)
bool EEUnitClass::IsAirBase()
{
    return this->mAirTrafficType != 0 && (this->mCategory == kUnitCategoryBuilding || this->mCategory == kUnitCategoryNavalBuilding || this->mCategory == kUnitCategorySpaceBuilding);
}

// Returns whether units can drop resources off at the unit class
bool EEUnitClass::IsDropSite()
{
    return this->mUnitType == kUnitTypeMechZeusPrototype ||
           this->IsLandBuilding() && this->mUnitType != kUnitTypeFarm && this->mUnitType != kUnitTypeDock && this->mUnitType != kUnitTypeNavyYard && this->mResourceType > kResourceNone && this->mResourceType < 7;
}

// Returns whether the unit class is a land, helicopter or naval transport
bool EEUnitClass::IsTransport()
{
    return this->mCategory == kUnitCategoryLandTransport || this->mCategory == kUnitCategoryHelicopterTransport || this->mCategory == kUnitCategoryNavalTransport || this->mCategory == kUnitCategorySpaceTransport || this->mCategory == kUnitCategoryFortress;
}

// Returns whether the tile is visible to a player (or the player sees everything)
bool EETile::IsVisibleTo(EEPlayer* inPlayer)
{
    return (this->mVisibility[inPlayer->mPlayerID >> 1] >> ((inPlayer->mPlayerID & 1) << 4) & 0xff) > 0 ||
           inPlayer->mSeesAll;
}

// Returns the graphic effect at an index, or NULL past the end of the table
EEGraphicEffect* EEGraphicEffect::Get(unsigned long inIndex)
{
    if (inIndex < EEGraphicEffect::sEffects.size())
        return EEGraphicEffect::sEffects[inIndex];
    return NULL;
}
