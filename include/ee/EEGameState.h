// C:\EEX\Empire Earth\Empire Earth\EEGameState.h (RECONSTRUCTION; file name inferred; all names are
// provisional). The game being set up or loaded (RTTI EEGameState > TSLockable, vtable 0x442178: ??_G 0x140ccf;
// ctor 0x14013d, dtor 0x1401da; EEGameState.cpp 0x1306f2..0x131b7c, __FILE__ 0x1306f2's asserts). The game's
// instance is gGameState (0x52f678, an object: its members show up as absolute addresses). Only what the decompiled
// units call. Layout gap-free: the ctor 0x14013d (member ctors; TSLockable's through its import)
// and dtor 0x1401da are verify-exact against it (bind_only: the vtable and Init/Clear names); sizeof 0x504
// (offsetof probes; the global's next neighbour is 0x52fb80, so 4 bytes at 0x52fb7c are unaccounted for). Member
// names are provisional. (0x52fa88, once named `gMultiplayer`, is mGameOptions.mIsMultiplayer.)
// Includes what its members need (TSLockable.h by bare name: ee.EEX_INCLUDE_DIRS); requires lle/LowLevelEngine.h,
// lle/FSBasicFile.h and lle/TSScheduler.h first.
#ifndef EE_EEGAMESTATE_H
#define EE_EEGAMESTATE_H

#include <vector>
#include "TSLockable.h"
#include "ee/EEGameOptions.h"

class UWideString;
class EEMap;
class EEScenarioManager;
class EEEnvironmentManager;
class EEEffectManager;
class EEPCalamityHelper;

class EEGameState : public TSLockable {
public:
    EEGameState();                          // 0x14013d (members, then Init)
    virtual ~EEGameState();                 // 0x1401da (Clear, then the members; ??_G 0x140ccf)
    // 0x1306f2: builds the game from inOptions; false with outError set when it fails (EEDataPreLoading::LoadGame)
    bool Create(EEGameOptions& inOptions, UWideString& outError);
    // 0x1317eb (+0x4c0..+0x4d0 = 0, the vector at +0x480 cleared)
    void Reset() { ee_gamecpp_state_reset(this); }
    // 0x1305fb (the scalars below; the teams and mLoadedPlayers cleared; mChecksum = ESChecksum(false))
    void Init() { ee_gamecpp_state_init(this); }
    // The bodies are Rust (ee_game gamecpp state.rs) over the cases below (EEGameStateOp)
    // 0x1304e4 (deletes +0x4c0..+0x4d0 and zeroes them, clears the vectors)
    void Clear() { ee_gamecpp_state_clear(this); }
    // 0x14aa24: deletes the old one (virtual dtor), then stores (EEPWorldHelper::Restore; provisional name)
    void SetScenarioManager(EEScenarioManager* inManager);
    // the loader unit's (Persistent/EEGameStateLoader.cpp 0x14a9ac..0x14aa24; provisional names)
    // Centres the camera on the middle of the map, if there is one
    // 0x14a9ac: mMapCenter = the map's (height + width) * 0.5 + 0.5 in x and y, z 0
    void SetEffectManager(EEEffectManager* inManager);   // 0x14a9e8 (deletes the old one)
    void SetMap(EEMap* inMap);              // 0x14aa06 (deletes the old one)
    // empty (ret 8), ICF-folded at 0x163d6f; the loader 0x14a5ba calls it with the save's versions (provisional name)
    // Does nothing after a game is loaded (no address: empty, ICF-folded into 0x163d6f; EEGameStateLoader::LoadGame
    // 0x14a5ba calls it with the save's versions: `push hdr.mMinorVersion; mov ecx, [esi] (the EEGameState*); push
    // hdr.mMajorVersion; call 0x163d6f`, a direct thiscall on the state; provisional name)
    void FinishLoad(unsigned long inMajorVersion, unsigned long inMinorVersion) {  }
    // 0x131854 (deletes the old one; provisional name)
    void SetEnvironmentManager(EEEnvironmentManager* inManager) { ee_gamecpp_state_set_environment(this, inManager); }

    EEGameOptions mGameOptions;             // +0x008
    GE3DPoint mMapCenter;                   // +0x414 (Create: the map's size * 0.5, z 0)
    ESChecksum mChecksum;                   // +0x420
    FSFileSpec mFileSpec;                   // +0x438
    // +0x480 the players, +0x490 the loaded players past the slots, +0x4a0 the teams, +0x4b0 the restorables (four
    // standard vectors): Rust's (ee_game gamecpp state.rs, teams.rs)
    EEMap* mMap;                            // +0x4c0 (Create: new EEMap 0x11b8d9)
    EEScenarioManager* mScenarioManager;    // +0x4c4 (new, 0x23c bytes)
    EEEnvironmentManager* mEnvironmentManager;  // +0x4c8 (new, 0x5c bytes)
    EEEffectManager* mEffectManager;        // +0x4cc (new, 0x34 bytes)
    EEPCalamityHelper* mCalamityHelper;     // +0x4d0 (new, 0x10 bytes)
    float mZoom;                            // +0x4d4 (Init: 1.0)
    float mTimeOfDay;                       // +0x4d8 (Init: 0.35)
    float mGameSpeed;                       // +0x4dc (Init: 1.0; EEWorldTable's Load: GetFloatValue(42))
    long mLocalPlayer;                      // +0x4e0
    long mRandomSeed;                       // +0x4e4
    long mMajorVersion;                     // +0x4e8
    unsigned long mMinorVersion;            // +0x4ec (unsigned: 0x54a0a4 jbe)
    long mNumPlayersMade;                   // +0x4f0 (Init: 3; Create: 0)
    long mTownIDCounter;                    // +0x4f4
    short mHerdIDCounter;                   // +0x4f8 (a word store)
    signed char mC4FA;                      // +0x4fa (EEPWorldHelper::Restore copies its signed char +0x36 byte for byte)
    bool mB4FB;                             // +0x4fb
    bool mIsLoadedGame;                     // +0x4fc (0x52fb74: compared by one reader)
    bool mInScenarioEditor;                 // +0x4fd
    bool mTeamsLocked;                      // +0x4fe (Init: true)
    bool mUnitImprovementsAllowed;          // +0x4ff (Init: true)
    bool mUsingCarbon;                      // +0x500
    bool mB501;                             // +0x501
    bool mRevealMap;                        // +0x502
};

extern EEGameState gGameState;

#endif
