// EEUGLocation.h (RECONSTRUCTION; file name inferred: one header per goal class like ee/EEUGExplore.h; split out of
// ee/EEUnitGoal.h, no __FILE__ evidence either way). Requires lle/GE3DPoint.h (the member).
#ifndef EE_EEUGLOCATION_H
#define EE_EEUGLOCATION_H

#include "ee/EEUnitGoal.h"

class EEComplexUnit;
class EEUnit;
class FSFile;
class GE2DPoint;
class GE3DPoint;
class UWideString;

// Going to a location (RTTI EEUGLocation > EEUnitGoal; vtable 0x4478a0; sizeof 0x38; four ctors)
class EEUGLocation : public EEUnitGoal {
public:
    // 0x227664: at (inX, inY, inZ) (EEMStopActiveGroup sends grounded aircraft to their tile; provisional names)
    EEUGLocation(float inX, float inY, float inZ, bool inArg4, bool inIsForcedMove, bool inShouldFace, float inYaw, bool inIsAutoWaypoint,
                 unsigned char inGroupPathState);
    // 0x2276e1: at (x, y, 0) of inLocation (EEMScrambleAircraft; provisional names)
    EEUGLocation(const GE2DPoint& inLocation, bool inArg2, bool inIsForcedMove, bool inShouldFace, float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState);
    // 0x227742 (its Virtual10 copies through it; provisional names)
    EEUGLocation(const GE3DPoint* inLocation, bool inArg2, bool inIsForcedMove, bool inShouldFace, float inYaw, bool inIsAutoWaypoint,
                 unsigned char inGroupPathState);
    // 0x2277a4: at (0, 0, 0) (EEUnitGoal::CreateGoal, EEUGHealUnit's (bool) ctor; provisional names)
    EEUGLocation(bool inArg, bool inIsForcedMove, bool inIsAutoWaypoint, unsigned char inGroupPathState);
    virtual EEUnitGoal* Clone();            // +0x10 0x2277ef
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0x227842
    virtual long GetOutOfSynchChecksum();   // +0x18 0x2278eb
    virtual void GetDebugLine(UWideString& ioText);  // +0x1c 0x227a44
    virtual void GetDebugText(UWideString& ioText);  // +0x20 0x227aa0
    virtual void GetGoalLocation(GE3DPoint& outLocation);  // +0x28 (ICF-folded 0x21d0b4: EEUGAttackGround's)
    virtual long GetStartAction();          // +0x30 (ICF-folded 0x227264: 0x1f40, EEUGLoadTransport's)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44 0x227b2a
    virtual void Start(EEComplexUnit* inUnit);       // +0x54 0x227c99
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x58 0x227d31
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0x64 0x227da5
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);  // +0x6c (ICF-folded 0x2907a3: empty)
    virtual bool Suits(EEUnit* inUnit);       // +0x70 (ICF-folded 0x220a18: true)
    // 0x22794d: turns inUnit to mF2C when it is within a degree of it (Virtual64's event 0x2337) (provisional name)
    bool FaceYaw(EEComplexUnit* inUnit);
    float GetYaw() const { return this->mYaw; }  // inline (FaceYaw 0x22794d: `fld; fsubp` after UMArcTan2) (provisional name)

    // members (names from the out-of-synch dump 0x227842; provisional name)
    GE3DPoint mLocation;                    // +0x20 (saved)
    float mYaw;                             // +0x2c "Yaw" (saved from minor version 0x1a)
    bool GetIsForcedMove() const { return this->mIsForcedMove; }
    bool mIsForcedMove;                     // +0x30 "Forced Move Flag" (saved from 0xf)
    bool mShouldFace;                       // +0x31 "Should Face Flag" (saved from 0x1a)
    bool GetIsAutoWaypoint() const { return this->mIsAutoWaypoint; }
    bool mIsAutoWaypoint;                   // +0x32 "Is Auto Waypoint Flag" (saved from 0x87)
    bool mUseGroupMovement;                 // +0x33 "Use Group Movement Flag" (saved from 0xad)
    unsigned char mGroupPathState;                   // +0x34 "Group Path State"

    static const UString sData;             // "EEUGLocation-Data" (init chain 0x227621)
};

#endif
