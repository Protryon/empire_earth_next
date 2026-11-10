// C:\EEX\Empire Earth\Graphics Thread\EEE3DVolcano.h (RECONSTRUCTION, from EEE3DVolcano.cpp 0xfe553..0x101c34;
// file name inferred). The volcano's 3D effect: a
// terrain-shaped model over a 4x4-tile area that rises (mGrowth 0..1) and sinks again (vtable 0x441090; RTTI
// EEE3DVolcano > EEEffect3D). sizeof 0xc8. Names are provisional. Requires ee/EEEffect3D.h, lle/GERectangle.h,
// lle/GE3DPoint.h.
#ifndef EE_EEE3DVOLCANO_H
#define EE_EEE3DVOLCANO_H

#include <vector>
#include "ee/EEEffect3D.h"
#include "ee/EEEffectKindIDs.h"

class EEGraphicObject;
class GEModel;

class EEE3DVolcano : public EEEffect3D {
public:
    // 0xfe596: EEEffect3D(0, inGraphicEffect, 0x20) inline, then the area around inLocation (by value: the effect
    // manager copies it with movsd; ret 0x10)
    EEE3DVolcano(GE3DPoint inLocation, EEGraphicEffect* inGraphicEffect);
    EEE3DVolcano();                         // 0xfe6ed (EEEffect3D(0x20): the COMDAT 0xe8e76)
    virtual ~EEE3DVolcano();                // +0x00 dtor 0xfe757 (??_G 0xfe6d1)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 0x100a47 (the draw)
    virtual long DrawPass5();               // +0x14 (likewise 0xfca98)
    virtual long DrawPass6();               // +0x18 (likewise 0xfca98)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0x10068a (moves the vertices, queue 2)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0x100a25 (loads the graphic, builds the model)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0x100b46
    virtual long RepairPointers();          // +0x3c 0x100be2 (EEEffect::RepairPointers, then builds the model)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0x100bf7
    virtual long Step();                    // +0x4c 0x100c7e (growth, duration)

    long BuildModel();                      // 0xfe79d (the terrain model from "textures\volcano.png") (provisional name)

    static const UString sData;             // "EEE3DVolcano-Data" (0x4e8d58; provisional name)

    std::vector<GE3DPoint> mPoints;         // +0x74 the model's resting vertices (_Buy 0x150afb)
    std::vector<float> mHeights;            // +0x84 the vertices' raised heights (Virtual24 blends to them)
    GERectangle mArea;                      // +0x94 (inLocation -/+ 3)
    GE3DPoint mCenter;                      // +0xa4 (the area's centre, y + 1.0)
    EEGraphicObject* mGraphicObject;        // +0xb0
    GEModel* mModel;                        // +0xb4 (referenced by BuildModel; Dereference in the dtor)
    float mGrowth;                          // +0xb8 0..1 (saved)
    long mTimeLeft;                         // +0xbc (saved)
    long mTime;                             // +0xc0 (saved; Virtual24 shakes the vertices when above the graphic's +0x74)
    bool mGrown;                            // +0xc4 (saved)
    bool mDone;                             // +0xc5 (mGrowth reached 1: the vertices stay put)
};

#endif
