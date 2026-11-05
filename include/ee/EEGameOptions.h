// C:\EEX\Empire Earth\Empire Earth\EEGameOptions.h (RECONSTRUCTION; file name inferred, from __FILE__
// "EEGameOptions.cpp" at 0x12f6d8). The options of the game being set up in the lobby:
// 16 player slots, four strings, a read/write lock and the game settings. EEGameOptions.cpp: the slot ctor 0x12f4dd
// and operator= 0x12f526 (out of line), the ctor 0x12f6d8 (CreateReadWriteLock assert, line 0x130).
// The game's instance is gGameOptions (0x53125c; 66 field references). sizeof >= 0x40c. Field names are provisional:
// the lobby events (0x1d9dcf..0x1e0e1b) name the setting fields after the EEMGameOptions option they send.
// Requires lle/TSScheduler.h (TSReadWriteLock).
#ifndef EE_EEGAMEOPTIONS_H
#define EE_EEGAMEOPTIONS_H

class FSFile;
class TSReadWriteLock;

// One player slot (0x38 bytes). Ctor 0x12f4dd: mType = mU04 = 3, mName = "", the rest 0, mB36 = true.
// The game's load types (EEGameOptions::mLoadType: new game, scenario, saved game, editor map, campaign scenario,
// campaign game; behaviour in code: EEGameState::Create, EEWorld). The lobby's option lists are data
// (gameoptions.yaml); these are not.
static const unsigned long kLoadTypeCount = 6;

class EEGameOptionsPlayer {
public:
    EEGameOptionsPlayer();                                          // 0x12f4dd
    EEGameOptionsPlayer& operator=(const EEGameOptionsPlayer& inOther);   // 0x12f526 (field moves: user-written)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x12f582
    long Save(FSFile& outFile);             // 0x12f626
    void FnE123D();                         // empty (ICF-folded into 0xe123d; EEGameOptions::Fn130272 calls it on each slot) (provisional name)

    unsigned long mType;                       // +0x00 0/1 human?, 2 computer?, 3 closed (provisional name)
    long mLoadedType;                       // +0x04
    UWideString mName;                      // +0x08
    float mPing;                            // +0x1c (the lobby's EEUICPing meter)
    unsigned long mSlot;                    // +0x20 the slot index (the lobby compares it unsigned: < 8 shows the row)
    unsigned long mColor;                         // +0x24
    unsigned long mTeam;                          // +0x28
    unsigned long mStartingCitizens;              // +0x2c
    unsigned long mTransferID;              // +0x30 (a transfer in progress when > 0: the lobby's start check)
    bool mIsLocal;                          // +0x34
    bool mIsReady;                          // +0x35
    bool mB36;                              // +0x36 (true in the ctor; the host row checks it on scenario loads)
};

class EEGameOptions {
public:
    EEGameOptions();                        // 0x12f6d8
    EEGameOptions(const EEGameOptions& inOther);   // 0x12f818 (also asserts on CreateReadWriteLock: user-written)
    ~EEGameOptions();                       // 0x12f892 (out of line: the lobby's GameType calls it)
    EEGameOptions& operator=(const EEGameOptions& inOther);   // 0x12f8e0
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x12fd6b
    long Save(FSFile& outFile);                                                              // 0x130289
    bool CanBuyCivilization();              // 0x12fc0c: !gUIManager+0x13c && !mIsScenarioGame (EEMCivUpgradesPurchased; provisional name)
    void Fn130272();                        // 0x130272 (EEMStartLoading, under the read lock; provisional name)
    void GetMapDimensions(long& outWidth, long& outHeight);  // 0x12fb70 (by mMapSize; EERandomMap::CreateRandomMap)
    bool GetCampaignMapName(UWideString& outName);  // 0x12fbc5 (EEWorld::StartGame on gGameOptions; provisional name)
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
    void SetDefaults();                     // 0x12fc27 (the preferences' defaults, mLoadType 0, epochs 15/14; provisional name)
    // inline (SIE Start 0x1e82c1: the temporary of the implicit const char* -> UWideString conversion is re-addressed
    // with lea)
    void SetMapType(const UWideString& inMapType) { this->mMapType = inMapType; }

    // inline (the lobby reads one slot through it: the original never CSEs &gGameOptions between a GetPlayer site
    // and a plain mPlayers[] site) (provisional name)
    EEGameOptionsPlayer& GetPlayer(long inIndex) { return this->mPlayers[inIndex]; }
    EEGameOptionsPlayer mPlayers[16];       // +0x000
    UWideString mCampaignName;              // +0x380
    UWideString mScenarioName;              // +0x394
    UWideString mMapType;                   // +0x3a8 option 2 (the map script name; also the file the lobby loads)
    UWideString mMapName;                   // +0x3bc
    TSReadWriteLock* mLock;                 // +0x3d0
    unsigned long mGameSpeed;                        // +0x3d4 option 10
    unsigned long mLoadType;                         // +0x3d8 0 new game, 1 scenario, 2 saved game ... (< kLoadTypeCount; provisional name)
    unsigned long mMapSize;                          // +0x3dc option 1
    long mEpochStart;                       // +0x3e0 option 4 (15: ...)
    long mEpochEnd;                         // +0x3e4 option 5
    unsigned long mVariant;                          // +0x3e8 option 8
    unsigned long mResources;                        // +0x3ec option 3
    unsigned long mUnitLimit;                        // +0x3f0 option 6
    unsigned long mWondersForVictory;                // +0x3f4 option 7
    unsigned long mDifficulty;                       // +0x3f8 option 9
    unsigned long mCampaignChecksum;        // +0x3fc (unsigned: `test; jbe`, the campaign checksum)
    bool mVictory;                          // +0x400 option 17 (the lobby's ChangeVictory check box; provisional name)
    bool mLockTeams;                        // +0x401 option 14
    bool mLockSpeed;                        // +0x402 option 15
    bool mRevealMap;                        // +0x403 option 13
    bool mCheatCodes;                       // +0x404 option 16
    bool mRecordGames;                      // +0x405 option 11
    bool mAllowCustomCivs;                  // +0x406 option 12
    bool mB407;                             // +0x407
    bool mIsMultiplayer;                    // +0x408
    bool GetIsScenarioGame() const { return this->mIsScenarioGame; }  // (MGOE OpenOptionsScreen's inline budget)
    bool mIsScenarioGame;                   // +0x409 (gWorld+0x519: the options menu hides Resign when set)
};

extern EEGameOptions& gGameOptions;          // 0x53125c

class GEColor;
// 0x1318bd (another unit; provisional name): the colour of player colour inIndex (a vector<...*> at 0x4e610c, the
// colour at +4 of each entry), or the default colour 0x4e60f8 when out of range
const GEColor& EEGetPlayerColor(unsigned long inIndex);

#endif
