// C:\EEX\Empire Earth\Graphics Thread\EEE3DMarchingLine.h (RECONSTRUCTION, from EEE3DMarchingLine.cpp
// 0xf3580..0xf4247; file name inferred). The marching line between two points or units (a 2D GELine on screen; kind
// 0x2d, created by EEEffectManager::CreateEffectBetween). It registers itself in the map's effect regions along
// the line (vtable 0x44099c; RTTI EEE3DMarchingLine > EEEffect3D). sizeof 0xd4. Names are provisional.
// Requires ee/EEEffect3D.h, lle/U2DPoint.h.
#ifndef EE_EEE3DMARCHINGLINE_H
#define EE_EEE3DMARCHINGLINE_H

#include <vector>
#include "ee/EEEffect3D.h"

class EEGraphicObject;

class EEE3DMarchingLine : public EEEffect3D {
public:
    EEE3DMarchingLine(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect);   // 0xf359c
    EEE3DMarchingLine();                    // 0xf3693
    virtual ~EEE3DMarchingLine();           // +0x00 0xf3736 (??_G 0xf3580)
    virtual long DrawPass4();               // +0x10 0xf3c68 (the draw: a GELine)
    virtual long DrawPass6();               // +0x18 (`return 0`: ICF-folded into 0xfca98)
    virtual void ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);  // +0x20 0xf4017
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xf3793 (queue 2)
    virtual void ReleaseResources();        // +0x28 0xf3882
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xf38a4
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf3b66
    virtual long RepairPointers();          // +0x3c 0xf3ee8 (the saved unit ids back to units)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf3f56
    virtual void MoveToTransformation(const GETransformation& inTransformation);  // +0x44 0xf3e52
    virtual void MoveToPoint(const GE3DPoint& inLocation);  // +0x48 0xf3e29
    virtual long Step();                    // +0x4c 0xf4068 (7 once the graphic's duration is over)
    virtual bool SetFrom(const GE3DPoint& inPoint);   // +0x58 0xf3e96 (false while it follows a unit)
    virtual bool SetTo(const GE3DPoint& inPoint);     // +0x5c 0xf3ebf

    void Reset();                           // 0xf3893 (the two timers)
    void AddRegions(const GE3DPoint& inFrom, const GE3DPoint& inTo);   // 0xf38c3
    void RemoveRegions();                   // 0xf3c02

    std::vector<U2DPoint<long> > mRegions;  // +0x74 the effect regions it is listed in
    GE3DPoint mFrom;                        // +0x84
    GE3DPoint mTo;                          // +0x90
    GE3DPoint mDrawFrom;                    // +0x9c (mFromUnit's position, else mFrom)
    GE3DPoint mDrawTo;                      // +0xa8
    GE3DPoint mCamera;                      // +0xb4 (gWorld.mTransformation's translation)
    EEGraphicObject* mGraphicObject;        // +0xc0
    EEUnit* mFromUnit;                      // +0xc4 (referenced; saved as its id)
    EEUnit* mToUnit;                        // +0xc8
    float mCC;                              // +0xcc (saved)
    float mTime;                            // +0xd0 (saved)
};

#endif
