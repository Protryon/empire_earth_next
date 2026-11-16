// The path walker's flags and the path-cost weights, from the deleted attack sub group (ee/EECPAttackSubGroup.h until
// stage 5, docs/AI_RESEARCH.md §8.90): the path finder (EEAutoPathRequest, EEAIAStarPathGraphGoal,
// EEAIPathSubSector, the goal modifiers) still takes them.
#ifndef EE_EECPASGFLAGS_H
#define EE_EECPASGFLAGS_H

#include <string.h>

// A long and 17 flags (0x18 bytes at EECPAttackSubGroup +0x164; slot +0x19c builds one on its stack the same way:
// memset of the flags, then the long; a unit's +0x2c object's slot +0x28 fills it). And 0x4c2c8 keeps a flag only
// where inOther has it too. Names are provisional.
struct EECPASGFlags {
    EECPASGFlags()
    {
        memset(this->mFlags, 0, sizeof(this->mFlags));
        this->mPlayerID = 0;
    }
    EECPASGFlags(const EECPASGFlags& inOther)
    {
        memcpy(this->mFlags, inOther.mFlags, sizeof(this->mFlags));
        this->mPlayerID = 0;
    }
    EECPASGFlags& operator=(const EECPASGFlags& inOther)
    {
        memcpy(this->mFlags, inOther.mFlags, sizeof(this->mFlags));
        this->mPlayerID = inOther.mPlayerID;
        return *this;
    }
    // 0x44c2c8: keeps each of the 17 flags only where the other flag set has it too
    void And(const EECPASGFlags& inOther)
    {
        for (long i = 16; i >= 0; i--)
            this->mFlags[i] = this->mFlags[i] != 0 && inOther.mFlags[i] != 0;
    }

    long mPlayerID;                         // +0x00
    bool mFlags[17];                        // +0x04
};

// The path-cost weights (0x3c bytes: 15 floats; the attack sub groups' slot +0x54 filled them). Names are provisional.
struct EECPASGWeights {
    // the influence-cost modifier ctor 0x40fe8f zeroes mWeights (rep stosd = Fill) before its member
    // stores and the vptr store, i.e. in the member-init phase: mWeights(0.0f) (provisional name)
    EECPASGWeights() {}
    EECPASGWeights(float inValue) { this->Fill(inValue); }
    void Fill(float inValue) { for (long i = 0; i < 15; i++) this->mValues[i] = inValue; }
    float mValues[15];
};

#endif
