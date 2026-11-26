// C:\EEX\Empire Earth\Empire Earth\EEGameOptions.h (RECONSTRUCTION; file name inferred, from __FILE__
// "EEGameOptions.cpp" at 0x12f6d8). The options of the game being set up in the lobby:
// 16 player slots, four strings, a read/write lock and the game settings. EEGameOptions.cpp: the slot ctor 0x12f4dd
// and operator= 0x12f526 (out of line), the ctor 0x12f6d8 (CreateReadWriteLock assert, line 0x130).
// The game's instance is gGameOptions (0x53125c; 66 field references). sizeof >= 0x40c. Field names are provisional:
// the lobby events (0x1d9dcf..0x1e0e1b) name the setting fields after the EEMGameOptions option they send.
// Requires lle/TSScheduler.h (TSReadWriteLock).
#ifndef EE_EEGAMEOPTIONS_H
#define EE_EEGAMEOPTIONS_H

#include "rust/game/gamecpp.h"

class FSFile;
class TSReadWriteLock;

// One player slot (0x38 bytes in the original). Ctor 0x12f4dd: mType = mU04 = 3, mName = "", the rest 0, mB36 = true.
// The game's load types (EEGameOptions::mLoadType: new game, scenario, saved game, editor map, campaign scenario,
// campaign game; behaviour in code: EEGameState::Create, EEWorld). The lobby's option lists are data
// (gameoptions.yaml); these are not.
static const unsigned long kLoadTypeCount = 6;

// The slot's plain members are EeAppOptionsPlayerFields (rust/ee_app.h; ee_app game_options.rs: mType, mLoadedType,
// mPing, mSlot, mColor, mTeam, mStartingCitizens, mTransferID, mIsLocal, mIsReady, mB36, mEngine), the base of the
// class; the name stays here. The bodies are Rust (EEGameOptions.cpp).
class EEGameOptionsPlayer : public EeAppOptionsPlayerFields {
public:
    EEGameOptionsPlayer();
    EEGameOptionsPlayer& operator=(const EEGameOptionsPlayer& inOther)   // 0x12f526 (every member)
    {
        static_cast<EeAppOptionsPlayerFields&>(*this) = inOther;
        this->mName = inOther.mName;
        return *this;
    }

    UWideString mName;                      // +0x08 in the original
};

// The options' plain members are EeAppOptionsFields (rust/ee_app.h; ee_app game_options.rs: mGameSpeed (option 10),
// mLoadType (0 new game, 1 scenario, 2 saved game ...; < kLoadTypeCount), mMapSize (1), mEpochStart (4), mEpochEnd
// (5), mVariant (8), mResources (3), mUnitLimit (6), mWondersForVictory (7), mDifficulty (9), mCampaignChecksum,
// mVictory (17), mLockTeams (14), mLockSpeed (15), mRevealMap (13), mCheatCodes (16), mRecordGames (11),
// mAllowCustomCivs (12), mB407, mIsMultiplayer, mIsScenarioGame (gWorld+0x519: the options menu hides Resign)), the
// base of the class; the slots, strings and lock stay here. The constructors, operator=, SetDefaults, Load and Save
// are Rust over the limits record and the file's host (EEGameOptions.cpp).
class EEGameOptions : public EeAppOptionsFields {
public:
    // Creates default game options (random epochs, normal speed and map size, unit limit 50, three wonders to win)
    // with sixteen empty player slots (ee_game gamecpp options.rs; the limits, the strings and the slots)
    EEGameOptions() { ee_gamecpp_options_init(this); }
    // Creates a copy of other game options, with its own lock (the copy keeps the defaults' mB407: operator= leaves it)
    // 0x12f818 (also asserts on CreateReadWriteLock: user-written)
    EEGameOptions(const EEGameOptions& inOther) { ee_gamecpp_options_copy(this, &inOther); }
    ~EEGameOptions();                       // 0x12f892 (out of line: the lobby's GameType calls it)
    EEGameOptions& operator=(const EEGameOptions& inOther);
    // Reads the game options and the sixteen slots from a saved game (ee_app game_options.rs load); returns the first
    // read error
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_options_load(this, inMajorVersion, inMinorVersion, &inFile); }
    // Writes the sixteen player slots and the game options to a saved game (ee_app game_options.rs save); returns the
    // first write error
    long Save(FSFile& outFile) { return ee_gamecpp_options_save(this, &outFile); }
    // Returns whether civilizations may be bought: not in the scenario editor and not in a scenario game
    // 0x12fc0c: !gUIManager+0x13c && !mIsScenarioGame (EEMCivUpgradesPurchased; provisional name)
    bool CanBuyCivilization() { return ee_gamecpp_options_can_buy_civilization(this); }
    // inline const accessors (EEMFullGameOptions 0xbe6c6 expands all 19; provisional names)
    const UWideString& GetMapType() const { return this->mMapType; }
    long GetGameSpeed() const { return this->mGameSpeed; }
    long GetLoadType() const { return this->mLoadType; }
    long GetMapSize() const { return this->mMapSize; }
    long GetEpochStart() const { return this->mEpochStart; }
    long GetEpochEnd() const { return this->mEpochEnd; }
    long GetVariant() const { return this->mVariant; }
    long GetResources() const { return this->mResources; }
    long GetUnitLimit() const { return this->mUnitLimit; }
    long GetWondersForVictory() const { return this->mWondersForVictory; }
    long GetDifficulty() const { return this->mDifficulty; }
    bool GetVictory() const { return this->mVictory; }
    bool GetLockTeams() const { return this->mLockTeams; }
    bool GetLockSpeed() const { return this->mLockSpeed; }
    bool GetRevealMap() const { return this->mRevealMap; }
    bool GetCheatCodes() const { return this->mCheatCodes; }
    bool GetRecordGames() const { return this->mRecordGames; }
    bool GetAllowCustomCivs() const { return this->mAllowCustomCivs; }
    bool GetB407() const { return this->mB407; }
    // inline (SIE Start 0x1e82c1: the temporary of the implicit const char* -> UWideString conversion is re-addressed
    // with lea)
    void SetMapType(const UWideString& inMapType) { this->mMapType = inMapType; }

    // inline (the lobby reads one slot through it: the original never CSEs &gGameOptions between a GetPlayer site
    // and a plain mPlayers[] site) (provisional name)
    EEGameOptionsPlayer mPlayers[16];       // +0x000
    UWideString mCampaignName;              // +0x380
    UWideString mScenarioName;              // +0x394
    UWideString mMapType;                   // +0x3a8 option 2 (the map script name; also the file the lobby loads)
    UWideString mMapName;                   // +0x3bc
    TSReadWriteLock* GetLock() const { return this->mLock; }
    void SetLoadType(unsigned long inType) { this->mLoadType = inType; }
    void SetIsMultiplayer(bool inValue) { this->mIsMultiplayer = inValue; }
    TSReadWriteLock* mLock;                 // +0x3d0
    bool GetIsScenarioGame() const { return this->mIsScenarioGame; }  // (MGOE OpenOptionsScreen's inline budget)
};


class GEColor;
// 0x1318bd (another unit; provisional name): the colour of player colour inIndex (a vector<...*> at 0x4e610c, the
// colour at +4 of each entry), or the default colour 0x4e60f8 when out of range
GEColor EEGetPlayerColor(unsigned long inIndex);

#endif
