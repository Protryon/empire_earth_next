// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.h (RECONSTRUCTION; file name inferred).
// A computer player's combat manager (RTTI EECPCombatManager; EECPCombatManager.cpp 0x71c7f..0x77084;
// EEComputerPlayer::mCombatManager +0xbc0). Stage 5 (docs/AI_RESEARCH.md §8.90) deleted the original's combat tree it
// ran (the attack and defence commanders, the attack groups and sub-groups, personnel, intelligence, the unit selector,
// the military aid packages): the Lua AIs command their own soldiers. What is left are the military class tests the
// engine and the next engine's view still ask.
#ifndef EE_EECPCOMBATMANAGER_H
#define EE_EECPCOMBATMANAGER_H

class FSFile;
class EEComputerPlayer;

// (header-only since the game's leaves round 1: its one test, IsMilitaryClass, is ee_ai military.rs's
// ee_ai_military_class, which every caller asks directly; EECPCombatManager.cpp went)
class EECPCombatManager {
public:
    EECPCombatManager(EEComputerPlayer* inPlayer) : mPlayer(inPlayer) {}
    virtual ~EECPCombatManager() {}         // +0x00 (??_G 0x71e8a, dtor 0x71ea6)

    // the saved game's block (stage 5: empty; the original's commanders, tasks and threat counts are gone)
    long Load(unsigned long, unsigned long, FSFile&) { return 0; }
    long Save(unsigned long, unsigned long, FSFile&) { return 0; }

    EEComputerPlayer* mPlayer;              // +0x44
};

#endif
