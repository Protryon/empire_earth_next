// C:\EEX\Empire Earth\Empire Earth\EEEMStorm.h (RECONSTRUCTION; file name inferred; provisional member names). A
// storm of the environment manager (RTTI EEEMStorm, vtable 0x441f18, sizeof 0x74; EEEMStorm.cpp 0x12dba8..0x12ed4e).
// Layout from the ctor 0x12de72 (member ctors, then the body's stores) and Load 0x12e599 / Save
// 0x12e876. Slots from EEEnvironmentManager's calls: 0 the deleting dtor, +4 Load,
// +8 RepairPointers, +0xc Save.
// Requires lle/LowLevelEngine.h (UWideString), lle/U2DPoint.h, lle/U2DRectangle.h, ee/EETileCoordinate.h, <vector>.
#ifndef EE_EEEMSTORM_H
#define EE_EEEMSTORM_H

#include <vector>
#include "ee/EETileCoordinate.h"
#include "rust/ee_app.h"   // EeAppStormFields (ee_app storm.rs)

class EEEnvironmentManager;
class FSFile;
class EEEffect; typedef EEEffect EEE3DRust;   // the weather effect (one class EEEffect since R4.4 round 25)
class EEAutoPathRequest;

class EEEMStorm : public EeAppStormFields {
public:
    EEEMStorm(long inType, EEEnvironmentManager* inManager);
    virtual ~EEEMStorm();                   // +0x00
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04
    // +0x08 0x12e6b5 (RepairPointers: mStorm from its saved handle, 9 when gone; the name again; state 2 -> 8;
    // non-zero: EEEnvironmentManager::RepairPointers drops the storm)
    virtual long Restore();
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c
    // 0x12ddbf (cdecl, by value): 0 "Unknown", 1 "Rain", 2 "Snow", 3 "Hurricane", 4 "Blizzard", 5..8 "PlaceHolder1".."4"
    // (each `theName += ...`), else `theName = "Type Not defined"` (provisional name)
    static UWideString GetTypeName(long inType);
    // In-class getters: EEEnvironmentManager::IsInStorm 0x52ee1b reads the type and tests the area
    // through them (the extra free inline sites make VC6 refuse the first four _Ptrit::operator+ as the original does;
    // exact only with both).
    long GetType() const { return this->mType; }
    bool Contains(const U2DPoint<long>& inPoint) const { return this->mArea.Contains(inPoint); }
    void Begin();                           // 0x12df63 (the ctor's, type != 0): start/goal, the shaped path, the weather effect
    bool IsOver();                          // 0x12e588 (true: the storm is over; EEEnvironmentManager::Update)
    void RequestRoute();                    // 0x12e74e: cancel the old request; RequestPath with a Snow/Rain route planner (the caller sets state 2)
    void TakePath();                        // 0x12e817: mPath = the request's path, reversed; release the request (the step sets state 3)
    void Release();                         // 0x12e9eb (before the storm is deleted)
    void Step(unsigned long inTime);        // 0x12ea48 (each update)

    static UString sStormData;              // "Storm-Data" (chain 0x12dba8; guarded atexit, bit 1 of 0x529b20; unreferenced)
    static UString sEnvironmentData;        // "Environment-Data" (chain 0x12dbeb; bit 2)

    UWideString mName;                      // +0x04
    std::vector<U2DPoint<long> > mPath;     // +0x18 (Load 0x12e5cf: resize 0x8a21 + U2DPoint<long>::Load per element; Save 0x12e8c2)
    U2DRectangle<long> mArea;               // +0x28 (EEEnvironmentManager::IsInStorm tests Contains)
    U2DPoint<long> mStart;                  // +0x38 (U2DPoint<long>::Load/Save 0x12e60a/0x12e8e1; FindPath's from)
    U2DPoint<long> mGoal;                   // +0x40 (a random town's/area's GetRandomPoint; FindPath's to)
    EEE3DRust* mStorm;                     // +0x48 (dynamic_cast of the weather effect, referenced; saved as its +0x1c handle)
    EEAutoPathRequest* mRequest;            // +0x4c (referenced; gPathManager.CancelAutoPathRequest on cancel)
    EEEnvironmentManager* mManager;         // +0x50
    // port: mTargetIntensity (+0x54), mIntensity, mStep, mType, mState, mNextMoveTime, mEndTime and mIsSevere (+0x70)
    // are the base EeAppStormFields (ee_app storm.rs: the fades and the step)
};

#endif
