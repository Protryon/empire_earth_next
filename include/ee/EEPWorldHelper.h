// C:\EEX\Empire Earth\Persistent\EEPWorldHelper.h (RECONSTRUCTION; layout proven: EEPWorldHelper sizeof 0x3c = its
// new-site size, ctor 0x14c151 exact in EEPWorldHelper.cpp; member names are provisional). The saved state of
// the world (RTTI EEPWorldHelper > FSPersistent; vtable 0x4422bc, tag 7). EEPWorldHelper.cpp is 0x14bd64..0x14c6b1.
// sizeof 0x3c (CreateDefault's `new`). Member types from the FSFile Read/Write overloads of Load/Save; names
// are provisional (the keys suggest some: teams, wonders to win, time of day, water level, zoom, population limit,
// game speed, victory, unit improvements, teams locked, cheat codes, town/herd id counters, carbon).
// Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/FSPersistent.h, ee/EETeam.h, <vector>.
#ifndef EE_EEPWORLDHELPER_H
#define EE_EEPWORLDHELPER_H

#include "rust/game/gamecpp.h"
#include "ee/EEPersistentTag.h"   // kPersistentWorld
class EEGameState;
class EEScenarioManager;
class EEWorld;

class EEPWorldHelper : public FSPersistent {
public:
    EEPWorldHelper();
    EEPWorldHelper(EEWorld& inWorld);
    // implicit virtual dtor: ??_G +0x10 0x14c1bc
    // Adds nothing to the checksum (empty: folded into a shared empty function)
    // +0x00 (empty: ICF-folded 0x157269)
    virtual void GetChecksum(ESChecksum& ioChecksum) const {  }
    // Reads the teams, world settings and id counters (Carbon flag from version 0.239 on), the running state (from 0.254:
    // world_save.rs), then the chat log and the main game form; returns only the main game form's error
    // +0x04 0x14c2bd
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_pwh_load(this, inMajorVersion, inMinorVersion, &inFile); }
    // Writes the teams, world settings, id counters and Carbon flag, the running state (from 0.254: world_save.rs),
    // then the chat log and the main game form; returns only the main game form's error
    // +0x0c 0x14c449
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_pwh_save(this, inMajorVersion, inMinorVersion, &outFile); }
    // Creates an empty saved-world chunk (persistent factory)
    // +0x14 0x14c2a6
    virtual FSPersistent* CreateDefault() const { return new EEPWorldHelper; }
    // 0x14c3a7: hands the saved state to the game being loaded (the loader 0x14a0a4 dynamic_casts the helper)
    // Copies the loaded teams, world settings and id counters into the game state (and its scenario manager, if any)
    void Restore(EEGameState& ioState) { ee_gamecpp_pwh_restore(this, &ioState); }

    // class static keys: guard byte 0x531c58 (bits 1..0x80), then 0x531bd0 (bits 1..0x40)

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
