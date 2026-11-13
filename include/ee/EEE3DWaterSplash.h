// C:\EEX\Empire Earth\Graphics Thread\EEE3DWaterSplash.h (RECONSTRUCTION; file name inferred).
// A splash model that grows from its graphic effect's +0x20 size to its height while flattening and fading (vtable
// 0x4410f4; RTTI EEE3DWaterSplash > EEEffect3D; EEE3DWaterSplash.cpp 0x101c34..0x101fce). Names are provisional.
// Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DWATERSPLASH_H
#define EE_EEE3DWATERSPLASH_H

class EEGraphicObject;

class EEE3DWaterSplash : public EEEffect3D {
public:
    EEE3DWaterSplash(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect);   // 0x101c77 (kind 0x1b)
    EEE3DWaterSplash();                     // 0x101d02
    virtual ~EEE3DWaterSplash();            // +0x00 0x101d1b (??_G 0x101ce6)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 0x101db4 (the draw)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0x101d3f (queue 4, by texture)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0x101da0 (loads the graphic)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0x101f32
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0x101f69
    virtual long Step();                    // +0x4c 0x101f8f (7 past the duration)

    static const UString sData;             // "EEE3DWaterSplashData" (0x4e8d70; provisional name)

    float& mAge = this->mRecord->aged.age;   // +0x74 0..1 (saved; the record's: ee_gfx effect_state.rs)
    float mDrawAge;                         // +0x78 (Virtual24 copies mAge; the draw eases it)
    EEGraphicObject* mGraphicObject;        // +0x7c
};

#endif
