// C:\EEX\Empire Earth\Graphics Thread\EEE3DScreenFlash.h (RECONSTRUCTION; file name inferred).
// A flash drawn over the screen at the effect's position, fading with its age (vtable 0x440c88; RTTI EEE3DScreenFlash
// > EEEffect3D; EEE3DScreenFlash.cpp 0xf8ceb..0xf90e9). sizeof 0x80. Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DSCREENFLASH_H
#define EE_EEE3DSCREENFLASH_H

class EEGraphicObject;

class EEE3DScreenFlash : public EEEffect3D {
public:
    EEE3DScreenFlash(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);   // 0xf8d2e (kind 0x46)
    EEE3DScreenFlash();                     // 0xf8d81
    virtual ~EEE3DScreenFlash();            // +0x00 0xf8d95 (??_G 0xf8d65)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 0xf8e1a (the draw)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xf8db9 (queue 4, by texture)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 (ICF-folded into EEE3DEnergyWave's 0xee3a4)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf904c
    virtual long RepairPointers();          // +0x3c (EEEffect::RepairPointers: ICF-folded into 0xe93e0)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf9083
    virtual long Step();                    // +0x4c 0xf90a9 (ages mF78 by the frame time; 7 once it reaches 1)

    static const UString sData;             // "EEE3DScreenFlash-Data" (0x4e8ca8; provisional name)

    EEGraphicObject* mGraphicObject;        // +0x74 (GetGraphicObject(), loaded by Load/Virtual30)
    float& mAge = this->mRecord->aged.age;   // +0x78 the age, 0..1 (saved; the record's: ee_gfx effect_state.rs)
    float mDrawAge;                         // +0x7c the age the draw uses (Virtual24 copies mF78)
};

#endif
