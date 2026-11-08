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
#include "ee/EECameraControl.h"        // held by value (+0x9d0)
#include "ee/EEGameOptions.h"          // held by value (+0x110, +0x51c)
#include "ee/EEStatisticsHistory.h"    // held by value (+0xd8)
#include "ee/EEVoting.h"               // held by value (+0xb5c)

class EEContagionManager;
class EEEffectManager;
class EEEnvironmentManager;
class EEGameState;
class EEInfluenceTable;
class EEMap;
class EEPlayer;
class EEScenarioManager;
class EETeam;
class EEUnit;
class FSPersistent;
class TSCriticalSection;

class EEGameVariant;
extern std::vector<EEGameVariant*> gGameVariants;   // 0x4e7f78 (declared in ee/EEGameVariant.h)

class EEWorld : public RTSWorld {
public:
    EEWorld();                              // 0x137ba3
    virtual ~EEWorld();                     // +0x00 (??_G 0x137e2c; dtor 0x137e48)
    virtual void Clear();                   // +0x08 0x138551
    void RestartUnits();                    // 0x1388ee (before a scenario starts; provisional name)
    virtual void GetChecksum(ESChecksum& ioChecksum);   // +0x0c 0x1394e7 (RTSWorld's slot)
    virtual void Reset();                   // +0x10 0x13bba0
    virtual void ResortUnit(RTSUnit* inUnit);   // +0x14 0x13c105 (re-sorts the unit's complex unit in its column list; EEUnit::MoveTo)
    virtual void SetCameraTransformation(const GETransformation& inTransformation);  // +0x18 0x13c1d5 (RTSWorld's slot)
    virtual void SetNumPlayers(unsigned long inNumPlayers);   // +0x1c 0x13c260 (RTSWorld's slot)
    virtual void SetPopulationLimit(unsigned long inLimit);   // +0x20 0x13c21e
    virtual void CameraChanged(const GETransformation& inTransformation);  // +0x24 (empty: ICF-folded `ret 4` 0x157269)
    virtual void PlaceQueuedUnits();        // +0x28 0x13c445
    virtual void GameSpeedChanged();        // +0x2c 0x13cdb9

    // 0x139528: mCalamityHelper's +0xc (out of line)
    EEContagionManager* GetContagionManager();
    // 0x139589: a test of map tile (inX, inY) (the teleport calamity refuses a location where it holds)
    // (provisional name)
    bool IsTileBlocked(unsigned long inX, unsigned long inY);
    // 0x1396a9: a unit at tile (inX, inY) for inUnitID (EEComplexUnit::FindWaypointMarker's fallback) (provisional
    // name)
    EEComplexUnit* FindQueuedUnitAt(long inX, long inY, unsigned long inUnitID);
    // 0x138fe5 / 0x1396f3: inUnit leaves / joins a world list (EEComplexUnit::SetGatheredResource's mShadowScale)
    // (provisional name)
    void ReleaseResourceUser(EEUnit* inUnit);
    void AddResourceUser(EEUnit* inUnit);
    // 0x1395cf: inPoint (a screen location) as a GE3DPoint for the camera transformation (EEACalamity; provisional
    // name)
    void ScreenToWorld(const GE2DPoint& inPoint, GE3DPoint& outPoint);
    EEUnit* GetUnit(unsigned long inUnitID);    // 0x14804f: by id (its top byte is the player); 0 if none
    long GetPlayerIndex(EEPlayer* inPlayer);    // 0x148024: index in mPlayers[0..mNumPlayers), -1 if none
    EEPlayer* GetPlayerBySlot(unsigned char inSlot);   // 0x139670: the player whose +0x464 is inSlot, 0 if none
    // inline: the chat log evaluates it before the entry it is compared with (0x1d3608)
    EEPlayer* GetCurrentPlayer() { return this->mPlayers[this->mCurrentPlayer]; }
    void RemovePlayerFromGame(bool inHasWon, unsigned long inPlayerID);  // 0x1391f2 (EEMResign: (0, player id); bool: the callee's `cmp byte`; provisional name)
    void SetDiplomaticStance(unsigned long inPlayerID, long inOther, long inStance);  // 0x1384dd (EEMSetDiplomaticStance; provisional name)
    long PauseGame();                       // 0x13a7e8 (pauses a single-player game; 2: already paused; provisional name)
    void RemoveDisconnectedPlayer(unsigned long inPlayer);  // 0x13bc4a (EEServer::ExecuteRemovePlayer: a player left; provisional name)
    // inline: EEMGameSpeed::Execute (0xbefb9) stores the speed and calls slot +0x2c through the vtable (provisional
    // name)
    void SetGameSpeed(float inSpeed) { this->mGameSpeed = inSpeed; this->GameSpeedChanged(); }
    // inline: EEPWorldHelper's world ctor copies it through the FPU (fld/fstp)
    float GetTimeOfDay() const { return this->mTimeOfDay; }
    void SetTimeOfDay(float inTimeOfDay);   // 0x13c301 (stores +0xb74, then the sun's transformation; EEPhysics 0x152f45 passes 0.35f; provisional name)
    // inline: its COMDAT 0x1cedbe follows its first user EETechTreeEpoch::GetCost (the vector's operator[]
    // expanded) (provisional name)
    EEGameVariant* GetGameVariant() { return gGameVariants[this->mGameOptions.mVariant]; }
    long UnpauseGame();                     // 0x13c964 (returns 0)
    void StartGame(EEGameState* inState, bool inArg);  // 0x139749 (EEDataPreLoading::LoadGame; provisional name)
    // the owning setters: delete the old object (through its vtable), then store
    void SetEffectManager(EEEffectManager* inManager);           // 0x13a219 (+0xb1c)
    // the ghost units (EEGhostUnit::Create; both take mGhostUnitsLock; provisional names)
    void ClearGhostUnits();                 // 0x1390e8 (the list at +0x9c0)
    // inline (out-of-line COMDAT 0xe4510 at the end of EEE2DMarchingIcons.cpp, after the unit's template COMDATs; 12
    // callers call it) (provisional name)
    unsigned long GetNumGhostUnits() { return this->mGhostUnits.size(); }
    void AddGhostUnit(EEGhostUnit* inGhost);   // 0x137f68
    void ActivateUnit(EEComplexUnit* inUnit);  // 0x13b8e8 (EEUnit::SetHitPoints: a dead unit of a +0x312 class) (provisional name)
    void DeactivateUnit(EEComplexUnit* inUnit);  // 0x13b965 (EEUnit::Remove) (provisional name)
    // 0x13c5ad (thiscall, ret 0x14: the 5th argument is unused; the body names gWorld; EEUIEMGEnterScenarioEditor)
    // (provisional name)
    void MovePlayerToSlot(unsigned char inSlot, char inNewSlot, unsigned char inPlayer, UWideString& inName, long inUnused);
    unsigned long GetResourceUsers(EEUnit* inUnit); // 0x139532 (EEUGBuildRepair::FindNextWork: below 1, a gather goal) (provisional name)
    void SetScenarioManager(EEScenarioManager* inManager);       // 0x13a237 (+0xb24; 0x1a594e: a new 0x23c-byte one)
    void SetEnvironmentManager(EEEnvironmentManager* inManager); // 0x13c1e3 (+0xb28; 0x1a596d: a new 0x5c-byte one)
    void SetInfluenceTable(EEInfluenceTable* inTable);           // 0x13c201 (+0xb20; deleted through slot 0)
    void UpdateActiveUnits();               // 0x13b6bc (EEPhysics::Tick, every tick) (provisional name)
    long CheckGameOver();                   // 0x13c9b4 (EEPhysics::Tick: the game result, kept in the scenario manager's +0x200) (provisional name)
    void SetCalamityHelper(FSPersistent* inObject);  // 0x13c1ad (+0xb2c; skips the delete when it is the same object)
    void CreateInfluenceTable(EEMap* inMap);    // 0x13a255: SetInfluenceTable(new EEInfluenceTable(inMap, mNumPlayers))
    unsigned long GetNumTeams();            // 0x3f81e (mTeams.size(); EECPIntelligence 0x8120b calls it) (provisional name)
    // EEWorld.cpp's other members (provisional names)
    void AddTeam(const std::vector<unsigned long>& inPlayers);   // 0x137f7b (merges the teams none of inPlayers is at war with)
    void AddAlliance(long inPlayer1, long inPlayer2);   // 0x138185
    void ApplyShareBonus();                 // 0x138304 (the +0xb20 population bonus of the players)
    void ReplacePlayer(unsigned long inIndex, unsigned long inMajorVersion, unsigned long inMinorVersion, EEMap* inMap);   // 0x138919 (a human player through an FSMemoryFile)
    void RebuildTeams();                    // 0x138e5e
    void RemoveAllUnits();                  // 0x13900d
    void DumpMedia(FSFileSpec& inFile, bool inLeaks);   // 0x13913e
    void AddComputerPlayers(EEGameState* inState);   // 0x139264 (fills the free slots up to 17)
    void SetCamera(const GETransformation& inTransformation, bool inForce);   // 0x1394b2 (Virtual18's)
    void CopyPlayers(std::vector<EEPlayer*>& outPlayers);  // 0x139631
    void SetMap(EEMap* inMap);              // 0x13a1fb (deletes the old map through its vtable)
    void CreateMiniMap(EEGameState* inState);   // 0x13a285
    void CreatePlayers(EEGameState* inState);   // 0x13a312
    void LoadMedia();                       // 0x13a719 (the shared effects and sounds; mMediaLoaded)
    void RecordStatistics();                // 0x13a845 (the end-of-game scores into mStatistics)
    void ClearContinents();                 // 0x13b6ab (gContinentManager.Clear(mMap))
    void RebuildActiveUnits();              // 0x13b9db
    void RefreshUnits();                    // 0x13bc1f
    void RemovePlayerFromTeams(unsigned long inPlayer);  // 0x13bc81 (inPlayer leaves its teams)
    void RemoveShareBonus();                // 0x13bd9d
    void BreakAlliance(long inPlayer1, long inPlayer2);   // 0x13bf76
    void NotifyPlayersLoaded(unsigned long inMajorVersion, unsigned long inMinorVersion);  // 0x13c0cb (each player's VirtualA0)
    void MarkPlayerOut(unsigned long inPlayer);  // 0x13c2a2 (marks the player out: mObserverMode, its options slot type 2)
    void UnloadMedia();                     // 0x13c772 (LoadMedia's twin)
    void TeamsChanged();                    // (an empty member: ICF-folded into 0xe123d)

    EEStatisticsHistory mStatistics;        // +0x0d8 (provisional name; Reset() again at the end of the ctor)
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
    std::map<EEUnit*, unsigned long> mResourceUsers; // +0x9a4 (unsigned: ReleaseResourceUser's `jbe`, GetResourceUsers's return) (keyed by the unit: ReleaseResourceUser/GetResourceUsers/AddResourceUser; dtor 0x13ce79)
    std::vector<EETeam> mTeams;             // +0x9b0 ("TheTeams"; dtor 0x13dc94, element dtor 0x131b02)
    std::vector<EEGhostUnit*> mGhostUnits;  // +0x9c0 the ghost units (EERenderObject 0x10c952; vector<long>'s _Buy/_Tidy are the ICF twins)
    EECameraControl mCameraControl;         // +0x9d0 (0x531710; wheel zoom, hot-key selection)
    std::map<long, EEComplexUnit*> mActiveUnits;  // +0xad0 (unit id -> referenced unit: UpdateActiveUnits/RebuildActiveUnits; dtor 0x88750)
    std::deque<EEComplexUnit*> mUpdateUnits;  // +0xadc (EEPhysics 0x15401e calls EEComplexUnit::Step on each; dtor 0x2003a4 for all three)
    std::deque<EEComplexUnit*> mActivateQueue;  // +0xaf0 (units pushed and Reference()d by ActivateUnit)
    std::deque<EEComplexUnit*> mDeactivateQueue;  // +0xb04 (DeactivateUnit)
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
    unsigned long GetNumberOfWondersToWin() const { return this->mNumberOfWondersToWin; }
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
    bool GetCheatsEnabled() const { return this->mCheatsEnabled; }
    bool GetTeamsLocked() const { return this->mTeamsLocked; }  // (the diplomacy form reads it through a temporary, EEUIEDSOpen 0x1d4165)
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

extern EEWorld gWorld;                      // 0x530d40

#endif
