// EEGroupCluster.h (RECONSTRUCTION; provisional names). The groups EEGroup::Fn206F1A
// splits a unit list into (`new 0x40`; inline ctor out of line at 0x207510 after its first user in EEGroup.cpp; the
// implicit dtor at 0x934fd): the units, their common flags, their tile bounds, their continent and a kind (1: units
// near each other, 2: boats (the class's +0x2d0 bit 0), 3: by continent). EEActiveGroup::Virtual40 paths each one.
// Requires lle/LowLevelEngine.h, <vector>, ee/EECPAttackSubGroup.h (EECPASGFlags), ee/EEAIAStarStorage.h.
#ifndef EE_EEGROUPCLUSTER_H
#define EE_EEGROUPCLUSTER_H

class EEComplexUnit;

struct EEGroupCluster {
    EEGroupCluster() : mContinentID(0), mKind(0) {}

    EEComplexUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEComplexUnit*> mUnits;     // +0x00
    EECPASGFlags mFlags;                    // +0x10
    EEAIAStarBounds mBounds;                // +0x28
    unsigned short mContinentID;            // +0x38
    long mKind;                             // +0x3c
};

#endif
