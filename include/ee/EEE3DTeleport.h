// C:\EEX\Empire Earth\Graphics Thread\EEE3DTeleport.h (RECONSTRUCTION; file name inferred). A
// unit's model rising out of (or sinking into) the ground, with two line effects (vtable 0x440eb0; RTTI EEE3DTeleport
// > EEEffect3D; EEE3DTeleport.cpp 0xfb9f6..0xfbec6, no key chain). Load/Virtual3C/Save are ICF-folded into
// EEE3DTimeWarp's (the same layout). Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DTELEPORT_H
#define EE_EEE3DTELEPORT_H

class EEGraphicObject;

class EEE3DTeleport : public EEEffect3D {
public:
    EEE3DTeleport(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xfb9f6
    EEE3DTeleport(long inKind);             // 0xfbb63
    virtual ~EEE3DTeleport();               // +0x00 0xfbb8a (??_G 0xfbb47)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 0xfbc66 (the draw)
    virtual long DrawPass5();               // +0x14 (`return 0`)
    virtual long DrawPass6();               // +0x18 (`return 0`)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xfbbbc (queue 2, by texture; ICF: EEE3DTimeWarp)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xfbc35 (loads the graphic)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 (0xfc0bb)
    virtual long RepairPointers();          // +0x3c (0xfc0e8)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 (0xfc297)
    virtual long Step();                    // +0x4c 0xfbe08 (moves up or down; 7 at the end of the duration)

    GE3DPoint mLocation;                    // +0x74 (the position the draw uses)
    EEGraphicObject* mGraphicObject;        // +0x80
    float& mAge = this->mRecord->aged.age;   // +0x84 milliseconds (saved; the record's: ee_gfx effect_state.rs)
    float mDrawAge;                         // +0x88 (Virtual24 copies mAge)
};

#endif
