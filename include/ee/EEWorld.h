// C:\EEX\Empire Earth\Empire Earth\EEWorld.h (RECONSTRUCTION; file name inferred). The game world (RTTI EEWorld >
// RTSWorld; vtable 0x441fec; ctor 0x137ba3, dtor 0x137e48). sizeof 0xba8 (the ctor's last store is +0xba4; another
// global starts at 0x5318e8). The global instance is `gWorld` (0x530d40; named by the calamity asserts,
// "::gWorld.GetContagionManager()"), so its members show up in code as absolute addresses (0x530db4 = mPlayers,
// 0x530dfc = mMap, 0x5318c4 = mCurrentPlayer).
// Layout (no gaps) from the ctor/dtor (member ctors and dtors in order), the setters
// 0x13a219..0x13c201 (each deletes the old object through its vtable) and EEPWorldHelper (0x14c1e2 copies the saved
// fields; its keys name them: "EEWorldHelper-TheTimeOfDay" etc.). Names are provisional unless an assert or a key names
// them; the element types of the two maps and the three deques are not decoded (inferred, POD by their dtors).
// Requires ee/RTSWorld.h.
#ifndef EE_EEWORLD_H
#define EE_EEWORLD_H

class EEGhostUnit;
class EEComplexUnit;

#include <deque>
#include <map>
#include <vector>
#include "ee/EEDbList.h"               // gGameVariants
#include "ee/EECameraControl.h"        // held by value (+0x9d0)
#include "ee/EEGameOptions.h"          // held by value (+0x110, +0x51c)
#include "ee/EEVoting.h"               // held by value (+0xb5c)
#include "rust/sim/app.h"              // the active unit lists (ee_app_active_*)
#include "rust/game/gamecpp.h"
#include "rust/sim/units.h"

class EEContagionManager;
class EEEffectManager;
class EEEnvironmentManager;
class EEGameState;
class EEInfluenceTable;
class EEMap;
class EEPlayer;
class EEScenarioManager;
class EEUnit;
class FSPersistent;
class TSCriticalSection;

class EEGameVariant;
extern EEDbList<EEGameVariant, EeDataList::GameVariants> gGameVariants;   // 0x4e7f78 (declared in ee/EEGameVariant.h)

class EEWorld : public RTSWorld {
public:
    EEWorld();
    virtual ~EEWorld();                     // +0x00 (??_G 0x137e2c; dtor 0x137e48)
    // +0x08 0x138551
    virtual void Clear() { ee_gamecpp_world_clear(this); }
    // 0x1388ee (before a scenario starts; provisional name)
    void RestartUnits() { ee_gamecpp_world_sight_all(this, true); }
    virtual void GetChecksum(ESChecksum& ioChecksum);   // +0x0c 0x1394e7 (RTSWorld's slot)
    // +0x10 0x13bba0
    virtual void Reset() { ee_gamecpp_world_reset(this); }
    virtual void ResortUnit(RTSUnit* inUnit);   // +0x14 0x13c105 (re-sorts the unit's complex unit in its column list; EEUnit::MoveTo)
    virtual void SetCameraTransformation(const GETransformation& inTransformation);  // +0x18 0x13c1d5 (RTSWorld's slot)
    // +0x1c 0x13c260 (RTSWorld's slot)
    virtual void SetNumPlayers(unsigned long inNumPlayers) { ee_gamecpp_world_set_count(this, false, inNumPlayers); }
    // +0x20 0x13c21e
    virtual void SetPopulationLimit(unsigned long inLimit) { ee_gamecpp_world_set_count(this, true, inLimit); }
    // +0x24 (empty: ICF-folded `ret 4` 0x157269)
    virtual void CameraChanged(class GETransformation const &) {  }
    // +0x28 0x13c445
    virtual void PlaceQueuedUnits() { ee_gamecpp_world_place_queued(this); }
    virtual void GameSpeedChanged();        // +0x2c 0x13cdb9

    // 0x139528: mCalamityHelper's +0xc (out of line)
    EEContagionManager* GetContagionManager();
    // 0x139589: a test of map tile (inX, inY) (the teleport calamity refuses a location where it holds)
    // (provisional name)
    bool IsTileBlocked(unsigned long inX, unsigned long inY);
    // 0x138fe5 / 0x1396f3: inUnit leaves / joins a world list (EEComplexUnit::SetGatheredResource's mShadowScale)
    // (provisional name)
    void ReleaseResourceUser(EEUnit* inUnit) { ee_gamecpp_world_resource_users(this, inUnit, -1); }
    void AddResourceUser(EEUnit* inUnit) { ee_gamecpp_world_resource_users(this, inUnit, 1); }
    // 0x1395cf: inPoint (a screen location) as a GE3DPoint for the camera transformation (EEACalamity; provisional
    // name)
    void ScreenToWorld(const GE2DPoint& inPoint, GE3DPoint& outPoint) { ee_gamecpp_world_screen_to_world((const float(*)[2])&inPoint.mX, (float(*)[3])&outPoint.mX); }
    EEUnit* GetUnit(unsigned long inUnitID);    // 0x14804f: by id (its top byte is the player); 0 if none
    // 0x148024: index in mPlayers[0..mNumPlayers), -1 if none
    long GetPlayerIndex(EEPlayer* inPlayer) { return ee_gamecpp_world_player_index(this, inPlayer); }
    // 0x139670: the player whose +0x464 is inSlot, 0 if none
    EEPlayer* GetPlayerBySlot(unsigned char inSlot) { return (EEPlayer*)ee_gamecpp_world_player_by_slot(this, inSlot); }
    // inline: the chat log evaluates it before the entry it is compared with (0x1d3608)
    EEPlayer* GetCurrentPlayer() { return this->mPlayers[this->mCurrentPlayer]; }
    // 0x1391f2 (EEMResign: (0, player id); bool: the callee's `cmp byte`; provisional name)
    void RemovePlayerFromGame(bool inHasWon, unsigned long inPlayerID);   // out of line: the test profile's harness hook
    // 0x1384dd (EEMSetDiplomaticStance; provisional name)
    void SetDiplomaticStance(unsigned long inPlayerID, long inOther, long inStance) { ee_gamecpp_world_set_stance(this, inPlayerID, inOther, inStance); }
    // 0x13a7e8 (pauses a single-player game; 2: already paused; provisional name)
    long PauseGame() { return ee_gamecpp_world_pause(true); }
    // 0x13bc4a (EEServer::ExecuteRemovePlayer: a player left; provisional name)
    void RemoveDisconnectedPlayer(unsigned long inPlayer) { ee_gamecpp_world_remove_disconnected(this, inPlayer); }
    // inline: EEMGameSpeed::Execute (0xbefb9) stores the speed and calls slot +0x2c through the vtable (provisional
    // name)
    void SetGameSpeed(float inSpeed) { this->mGameSpeed = inSpeed; this->GameSpeedChanged(); }
    // inline: EEPWorldHelper's world ctor copies it through the FPU (fld/fstp)
    float GetTimeOfDay() const { return this->mTimeOfDay; }
    // 0x13c301 (stores +0xb74, then the sun's transformation; EEPhysics 0x152f45 passes 0.35f; provisional name)
    void SetTimeOfDay(float inTimeOfDay) { ee_gamecpp_world_set_time_of_day(this, inTimeOfDay); }
    // inline: its COMDAT 0x1cedbe follows its first user EETechTreeEpoch::GetCost (the vector's operator[]
    // expanded) (provisional name)
    EEGameVariant* GetGameVariant() { return gGameVariants[this->mGameOptions.mVariant]; }
    // 0x13c964 (returns 0)
    long UnpauseGame() { return ee_gamecpp_world_pause(false); }
    // 0x139749 (EEDataPreLoading::LoadGame; provisional name)
    void StartGame(EEGameState* inState, bool inLocked) { ee_gamecpp_world_start_game(this, inState, inLocked); }
    // the ghost units (EEGhostUnit::Create; both take mGhostUnitsLock; provisional names)
    // 0x1390e8 (the list at +0x9c0)
    void ClearGhostUnits() { ee_gamecpp_world_clear_ghosts(this); }
    // inline (out-of-line COMDAT 0xe4510 at the end of EEE2DMarchingIcons.cpp, after the unit's template COMDATs; 12
    // callers call it) (provisional name)
    unsigned long GetNumGhostUnits() { return ee_app_ghost_count(); }
    EEGhostUnit* GetGhostUnit(size_t inIndex) { return (EEGhostUnit*)ee_app_ghost_at(inIndex); }   // NULL past the end
    // appends a ghost unit to the world's ghost list (the list: ee_sim app ghosts.rs)
    void AddGhostUnit(EEGhostUnit* inGhost) { ee_app_ghost_push(inGhost); }
    // 0x13b8e8 (EEUnit::SetHitPoints: a dead unit of a +0x312 class) (provisional name)
    void ActivateUnit(EEComplexUnit* inUnit) { ee_gamecpp_world_activate(inUnit); }
    // 0x13b965 (EEUnit::Remove) (provisional name)
    void DeactivateUnit(EEComplexUnit* inUnit) { ee_gamecpp_world_deactivate(inUnit); }
    // 0x139532 (EEUGBuildRepair::FindNextWork: below 1, a gather goal) (provisional name)
    unsigned long GetResourceUsers(EEUnit* inUnit) { return ee_gamecpp_world_resource_users(this, inUnit, 0); }
    void SetScenarioManager(EEScenarioManager* inManager);       // 0x13a237 (+0xb24; 0x1a594e: a new 0x23c-byte one)
    void SetEnvironmentManager(EEEnvironmentManager* inManager); // 0x13c1e3 (+0xb28; 0x1a596d: a new 0x5c-byte one)
    void UpdateActiveUnits();               // 0x13b6bc (EEPhysics::Tick, every tick) (provisional name)
    void CreateInfluenceTable(EEMap* inMap);    // 0x13a255: SetInfluenceTable(new EEInfluenceTable(inMap, mNumPlayers))
    // EEWorld.cpp's other members (provisional names)
    void ApplyShareBonus();                 // 0x138304 (the +0xb20 population bonus of the players)
    void DumpMedia(FSFileSpec& inFile, bool inLeaks) { ee_gamecpp_world_dump_media(this, &inFile, inLeaks); }
    void SetMap(EEMap* inMap);              // 0x13a1fb (deletes the old map through its vtable)
    void CreateMiniMap(EEGameState* inState);
    void RecordStatistics();                // 0x13a845 (the end-of-game scores into mStatistics)
    void RemoveShareBonus();
    // the three members other lanes' C++ still calls (the scenario editor's host, EEScreens.cpp)
    void RebuildActiveUnits() { ee_gamecpp_world_rebuild_active(this); }
    void RefreshUnits() { ee_gamecpp_world_sight_all(this, false); }
    // 0x13c5ad (thiscall, ret 0x14: the 5th argument is unused; the body names gWorld; EEUIEMGEnterScenarioEditor)
    void MovePlayerToSlot(unsigned char inSlot, char inNewSlot, unsigned char inPlayer, UWideString& inName, long inUnused) { ee_gamecpp_world_move_to_slot(inSlot, (uint8_t)inNewSlot, inPlayer, &inName); }

    // +0x0d8 the statistics history (the end-of-game records): Rust's (ee_game gamecpp stats_history.rs)
    EEGameOptions& GetGameOptions() { return this->mGameOptions; }
    // inline (the lobby reaches gGameOptions 0x53125c through it: GetPlayer(i - k) folds into `lea [eax + base -
    // k*0x38]` only with this from an accessor) (provisional name)
    EEGameOptions& GetLobbyOptions() { return this->mLobbyOptions; }  // (MGOE OpenOptionsScreen's inline budget)
    EEGameOptions mGameOptions;             // +0x110 (0x530e50; provisional name; 0x1397d7 copies 0x52f680 into it)
    EEGameOptions mLobbyOptions;            // +0x51c (0x53125c: what include/ee/EEGameOptions.h calls gGameOptions)
    GEVector mU928;                         // +0x928 (GEVector or GE3DPoint: three fldz)
    GEVector mU934;                         // +0x934
    GETransformation mU940;                 // +0x940
    GEVector mMoonDirection;                // +0x980
    GEVector mSunDirection;                 // +0x98c
    GEVector mWindDirection;                // +0x998 (10 readers each of +0x998/+0x99c/+0x9a0)
    // +0x9a4 the resource users (a std::map by unit handle: ReleaseResourceUser/GetResourceUsers/AddResourceUser):
    // Rust's (ee_game gamecpp world.rs RESOURCE_USERS)
    // +0x9b0 the teams ("TheTeams", std::vector<EETeam>): Rust's (ee_sim app teams_ffi.rs)
    // +0x9c0 the ghost units (std::vector<EEGhostUnit*>): Rust's (ee_sim app ghosts.rs; GetGhostUnit)
    EECameraControl mCameraControl;         // +0x9d0 (0x531710; wheel zoom, hot-key selection)
    // +0xad0 .. +0xb17 (mActiveUnits, mUpdateUnits, mActivateQueue, mDeactivateQueue): the active unit lists are one
    // Rust object (ee_sim app active.rs; rust/sim/app.h ee_app_active_*): the stepped units' ids in their order, the
    // queues, the stepped set; the units' references stay C++ (EEWorld.cpp's EEActiveRefs)
    size_t GetNumUpdateUnits() const { return ee_app_active_list(0, NULL, 0); }
    uint32_t GetUpdateUnitID(size_t inIndex) const { return ee_app_active_at(0, inIndex); }
    TSCriticalSection* mGhostUnitsLock;     // +0xb18 (CreateCriticalSection in the ctor)
    EEEffectManager* GetEffectManager() { return this->mEffectManager; }  // inline (callers load it before the arguments)
    EEEffectManager* mEffectManager;        // +0xb1c (0x53185c: what ee/EEEffectManager.h calls gEffectManager)
    EEInfluenceTable* mInfluenceTable;      // +0xb20
    EEScenarioManager* GetScenarioManager() const { return this->mScenarioManager; }
    EEScenarioManager* mScenarioManager;    // +0xb24 ("The Scenario Manager"; 120 readers)
    EEEnvironmentManager* mEnvironmentManager;   // +0xb28
    FSPersistent* mCalamityHelper;          // +0xb2c (deleted through slot +0x10; its +0xc is the contagion manager)
    long mUB30[10];                         // +0xb30 (all = 1: one `rep stosd`)
    long mUB58;                             // +0xb58 (= 2)
    EEVoting mVoting;                       // +0xb5c (provisional name)
    unsigned short mHerdIDCounter;          // +0xb68 ("HerdIDCounter")
    long mTownIDCounter;                    // +0xb6c ("TownIDCounter")
    float mSavedGameSpeed;                  // +0xb70 (= 0)
    float mTimeOfDay;                       // +0xb74 ("TheTimeOfDay")
    float mWindYaw;                         // +0xb78 (= 0)
    long mCollisionDetectionFlags;          // +0xb7c (= 15)
    long mUB80;                             // +0xb80 (= 0)
    long mCurrentPlayer;                    // +0xb84 the local player's index
    unsigned long mNumberOfWondersToWin;    // +0xb88 ("TheNumberOfWondersToWin"; = 1)
    long mMaxPlayers;                       // +0xb8c (= 3)
    long mStartTime;                        // +0xb90 (= 0)
    unsigned short mLagStatus;              // +0xb94 (= 0)
    bool mVictoryEnabled;                   // +0xb96 ("TheVictoryEnabled"; not set by the ctor)
    bool mGameScreenActive;                 // +0xb97 (21 readers)
    bool mLoadingSavedGame;                 // +0xb98 (34 readers)
    bool mClearing;                         // +0xb99
    // inline accessor: one more inline expansion in EEUIECESendMessage::ProcessEvent makes VC6 refuse the
    // _Ptrit::operator+ of UIEditBox::GetText(0) as in the original (0x1d30a0); the GetMainGameForm() accessor
    // variant does the same, this one is the guess
    bool mCheatsEnabled;                    // +0xb9a ("TheCheatCodesAllowed"; 0x5318da: the chat line is parsed for cheats)
    bool mUnitImprovementsAllowed;          // +0xb9b ("TheUnitImprovementsAllowed"; = true)
    bool mShowCollisionSpheres;             // +0xb9c
    bool mBB9D;                             // +0xb9d (= true)
    bool mExecutingLocalSelection;          // +0xb9e
    bool mTeamsLocked;                      // +0xb9f ("TheTeamsLocked")
    bool mInCinematic;                      // +0xba0 (0x5318e0; set: the wheel/hot-key zoom skip the camera-mode reset;
                                            // provisional name)
    bool mMediaLoaded;                      // +0xba1
    bool mUserPaused;                       // +0xba2
    bool mLoadingPlayerMedia;               // +0xba3
    bool mUsingCarbon;                      // +0xba4 ("UsingCarbon")
};

extern EEWorld gWorld;

#endif
