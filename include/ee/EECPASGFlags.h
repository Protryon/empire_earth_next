// The path walker's flags, from the deleted attack sub group (ee/EECPAttackSubGroup.h until
// stage 5, docs/AI_RESEARCH.md §8.90): the path finder (EEAutoPathRequest, EEAIAStarPathGraphGoal,
// EEAIPathSubSector, the goal modifiers) still takes them.
#ifndef EE_EECPASGFLAGS_H
#define EE_EECPASGFLAGS_H

#include <string.h>

// A long and 17 flags (0x18 bytes at EECPAttackSubGroup +0x164; slot +0x19c builds one on its stack the same way:
// memset of the flags, then the long; a unit's +0x2c object's slot +0x28 fills it). Names are provisional.
struct EECPASGFlags {
    EECPASGFlags() {}
    // a copy keeps the flags, not the player (assignment copies both)
    EECPASGFlags(const EECPASGFlags& inOther) { memcpy(this->mFlags, inOther.mFlags, sizeof(this->mFlags)); }
    EECPASGFlags& operator=(const EECPASGFlags& inOther) = default;
    long mPlayerID = 0;                     // +0x00
    bool mFlags[17] = {};                   // +0x04
};

#endif
