// C:\EEX\Empire Earth\Graphics Thread\EEE2DShockwave.h (RECONSTRUCTION; file name inferred). A
// ring model that grows (or shrinks) and fades once, at a unit's tag point of kind 4 or at a point (RTTI
// EEE2DShockwave > EEEffect2D; vtable 0x43ffbc; EEE2DShockwave.cpp 0xe614f..0xe6621; sizeof 0x60). Its Virtual24
// folds into EEE2DLightning's 0xe2a0b. Names are provisional. Requires ee/EEEffect2D.h.
#ifndef EE_EEE2DSHOCKWAVE_H
#define EE_EEE2DSHOCKWAVE_H

#include "ee/EEEffect2D.h"

class EEGraphicObject;

class EEE2DShockwave : public EEEffect2D {
public:
    static const UString sData;             // "EEE2DShockwave-Data" (chain 0xe614f)

    EEE2DShockwave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);   // 0xe6192 (kind 0x12, model gU4E8DF4)
    EEE2DShockwave(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect);   // 0xe61e2
    EEE2DShockwave();                       // 0xe621f (the load-time ctor)
    virtual ~EEE2DShockwave();              // +0x00 0xe623b (??_G 0xe61c6)
    virtual long DrawPass2();               // +0x08 (0xfca98)
    virtual long DrawPass4();               // +0x10 (0xfca98)
    virtual long DrawPass5();               // +0x14 (0xfca98)
    virtual long DrawPass6();               // +0x18 0xe6383
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 (ICF: 0xe2a0b)
    virtual void PrepareQueue();            // +0x2c (empty: 0xe123d)
    virtual long SetUp();                   // +0x30 0xe6259 (a random tag point of kind 4 on the unit)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x38 0xe6585
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x40 0xe65bc
    virtual long Step();                    // +0x4c 0xe65e2

    EEGraphicObject* mGraphicObject;        // +0x54 (UnloadMedia in the dtor)
    float& mAge = this->mRecord->aged.age;   // +0x58 0..1 (saved; the record's: ee_gfx effect_state.rs)
    float mDrawAge;                         // +0x5c (Virtual24's copy of mAge)
};

#endif
