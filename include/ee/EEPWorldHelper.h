// C:\EEX\Empire Earth\Persistent\EEPWorldHelper.h (RECONSTRUCTION; layout proven: EEPWorldHelper sizeof 0x3c = its
// new-site size, ctor 0x14c151 exact in EEPWorldHelper.cpp; member names are provisional). The saved state of
// the world (RTTI EEPWorldHelper > FSPersistent; vtable 0x4422bc, tag 7). EEPWorldHelper.cpp is 0x14bd64..0x14c6b1.
// sizeof 0x3c (CreateDefault's `new`). Member types from the FSFile Read/Write overloads of Load/Save; names
// are provisional (the keys suggest some: teams, wonders to win, time of day, water level, zoom, population limit,
// game speed, victory, unit improvements, teams locked, cheat codes, town/herd id counters, carbon).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/FSPersistent.h, ee/EETeam.h, <vector>.
#ifndef EE_EEPWORLDHELPER_H
#define EE_EEPWORLDHELPER_H

class EEGameState;
class EEScenarioManager;
class EEWorld;

class EEPWorldHelper : public FSPersistent {
public:
    EEPWorldHelper();                                                           // 0x14c151
    EEPWorldHelper(EEWorld& inWorld);                                           // 0x14c1e2
    // implicit virtual dtor: ??_G +0x10 0x14c1bc
    virtual void GetChecksum(ESChecksum& ioChecksum) const;                     // +0x00 (empty: ICF-folded 0x157269)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x14c2bd
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x14c449
    virtual FSPersistent* CreateDefault() const;                                // +0x14 0x14c2a6
    // 0x14c3a7: hands the saved state to the game being loaded (the loader 0x14a0a4 dynamic_casts the helper)
    void Restore(EEGameState& ioState);
    // inline: Restore hands the floats over through the FPU (fld/fstp; provisional names from the keys)
    float GetTimeOfDay() const { return this->mTimeOfDay; }
    float GetZoom() const { return this->mZoom; }
    float GetGameSpeed() const { return this->mGameSpeed; }

    // class static keys: guard byte 0x531c58 (bits 1..0x80), then 0x531bd0 (bits 1..0x40)
    static const UString sTheScenarioManager;   // "EEWorldHelper-The Scenario Manager" (sic)
    static const UString sTheTeams;
    static const UString sTheNumberOfWondersToWin;
    static const UString sTheTimeOfDay;
    static const UString sTheWaterLevel;
    static const UString sTheZoom;
    static const UString sThePopulationLimit;
    static const UString sTheGameSpeed;
    static const UString sTheVictoryEnabled;
    static const UString sTheUnitImprovementsAllowed;
    static const UString sTheTeamsLocked;
    static const UString sTheCheatCodesAllowed;
    static const UString sTownIDCounter;
    static const UString sHerdIDCounter;
    static const UString sUsingCarbon;

    EEScenarioManager* mScenarioManager;    // +0x08 (0; Restore hands it to the game state when set)
    std::vector<EETeam> mTeams;             // +0x0c
    float mTimeOfDay;                       // +0x1c
    float mZoom;                            // +0x20
    float mGameSpeed;                       // +0x24
    unsigned long mPopulationLimit;         // +0x28
    unsigned long mNumberOfWondersToWin;    // +0x2c
    long mTownIDCounter;                    // +0x30
    unsigned short mHerdIDCounter;          // +0x34
    signed char mC36;                       // +0x36
    bool mVictoryEnabled;                   // +0x37
    bool mUnitImprovementsAllowed;          // +0x38 (true)
    bool mTeamsLocked;                      // +0x39 (true)
    bool mCheatsEnabled;                    // +0x3a (true)
    bool mUsingCarbon;                      // +0x3b (saved from 0.0xef on)
};

#endif
