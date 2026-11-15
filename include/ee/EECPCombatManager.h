// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.h (RECONSTRUCTION; file name inferred).
// A computer player's combat manager (RTTI EECPCombatManager; EECPCombatManager.cpp 0x71c7f..0x77084;
// EEComputerPlayer::mCombatManager +0xbc0). Stage 5 (docs/AI_RESEARCH.md §8.90) deleted the original's combat tree it
// ran (the attack and defence commanders, the attack groups and sub-groups, personnel, intelligence, the unit selector,
// the military aid packages): the Lua AIs command their own soldiers. What is left are the military class tests the
// engine and the next engine's view still ask.
#ifndef EE_EECPCOMBATMANAGER_H
#define EE_EECPCOMBATMANAGER_H

class FSFile;
class EEUnit;
class EEComplexUnit;
class EEComputerPlayer;
class EEUnitClass;

class EECPCombatManager {
public:
    EECPCombatManager(EEComputerPlayer* inPlayer);   // 0x71d74
    virtual ~EECPCombatManager();           // +0x00 (??_G 0x71e8a, dtor 0x71ea6)

    bool IsMilitaryUnit(EEComplexUnit* inUnit);  // 0x7377e (ret 4)
    bool IsMilitaryClass(EEUnitClass* inClass);  // 0x737c9 (ret 4; this unused)
    static bool IsMilitaryType(unsigned long inUnitType);  // 0x75122 (cdecl)
    static bool IsMilitaryTypeUnit(EEUnit* inUnit);  // 0x7517d (cdecl)
    // the saved game's block (stage 5: empty; the original's commanders, tasks and threat counts are gone)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);

    EEComputerPlayer* mPlayer;              // +0x44
};

#endif
