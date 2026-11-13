// C:\EEX\Empire Earth\Graphics Thread\EEE3DTimeWarp.h (RECONSTRUCTION; file name inferred). A
// unit's model with two line effects (the teleport's twin; vtable 0x440f0c; RTTI EEE3DTimeWarp
// > EEEffect3D; EEE3DTimeWarp.cpp 0xfbec6..0xfc382, no key chain). EEE3DTeleport's Load/Virtual3C/Save fold into
// this class's. Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DTIMEWARP_H
#define EE_EEE3DTIMEWARP_H

class EEGraphicObject;

class EEE3DTimeWarp : public EEEffect3D {
public:
    EEE3DTimeWarp(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xfbec6
    EEE3DTimeWarp(long inKind);             // 0xfc039
    virtual ~EEE3DTimeWarp();               // +0x00 0xfc060 (??_G 0xfc01d)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 0xfc10a (the draw)
    virtual long DrawPass5();               // +0x14 (`return 0`)
    virtual long DrawPass6();               // +0x18 (`return 0`)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 (ICF-folded into EEE3DTeleport 0xfbbbc)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xfc092 (loads the graphic)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xfc0bb
    virtual long RepairPointers();          // +0x3c 0xfc0e8
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xfc297
    virtual long Step();                    // +0x4c 0xfc2c4 (moves up or down; 7 at the end of the duration)

    GE3DPoint mLocation;                    // +0x74 (the position the draw uses)
    EEGraphicObject* mGraphicObject;        // +0x80
    float& mAge = this->mRecord->aged.age;   // +0x84 milliseconds (saved; the record's: ee_gfx effect_state.rs)
    float mDrawAge;                         // +0x88 (Virtual24 copies mAge)
};

#endif
