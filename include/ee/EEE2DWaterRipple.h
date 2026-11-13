// C:\EEX\Empire Earth\Graphics Thread\EEE2DWaterRipple.h (RECONSTRUCTION; file name inferred). A
// ripple quad flat on the water at a point; ends early when its tile is not water (RTTI EEE2DWaterRipple >
// EEEffect2D; vtable 0x440210; EEE2DWaterRipple.cpp 0xe7d67..0xe8158; sizeof 0xa0). Queued on pass 3. Names
// are provisional. Requires ee/EEEffect2D.h, lle/GETransformation.h.
#ifndef EE_EEE2DWATERRIPPLE_H
#define EE_EEE2DWATERRIPPLE_H

#include "ee/EEEffect2D.h"

class EEGraphicObject;

class EEE2DWaterRipple : public EEEffect2D {
public:
    static const UString sData;             // "EEE2DWaterRipple-Data" (chain 0xe7d67)

    EEE2DWaterRipple(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect);   // 0xe7daa (kind 0x22)
    EEE2DWaterRipple();                     // 0xe7e2b (the load-time ctor)
    EEE2DWaterRipple(const GETransformation& inTransformation, EEGraphicEffect* inGraphicEffect);   // 0xe7e50
    virtual ~EEE2DWaterRipple();            // +0x00 0xe7eac (??_G 0xe7e0f)
    virtual long DrawPass2();               // +0x08 (0xfca98)
    virtual long DrawPass4();               // +0x10 (0xfca98)
    virtual long DrawPass5();               // +0x14 0xe7f90 (the ground quad 0x1033f1)
    virtual long DrawPass6();               // +0x18 (0xfca98)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xe7ecd
    virtual void PrepareQueue();            // +0x2c (empty: 0xe123d)
    virtual long SetUp();                   // +0x30 0xe7f34
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x38 0xe803e
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x40 0xe8089
    virtual long Step();                    // +0x4c 0xe80c0

    // +0x54 (at water level: z = 0; saved) and the age: the record's (ee_gfx effect_state.rs)
    GETransformation& mTransformation = *reinterpret_cast<GETransformation*>(&this->mRecord->aged.transformation);
    EEGraphicObject* mGraphicObject;        // +0x94 (UnloadMedia in the dtor)
    float& mAge = this->mRecord->aged.age;   // +0x98 0..1 (saved)
    float mDrawAge;                         // +0x9c (Virtual24's copy of mAge)
};

#endif
