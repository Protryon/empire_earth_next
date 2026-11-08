// C:\EEX\Empire Earth\Graphics Thread\EEE3DEnergyWave.h (RECONSTRUCTION; file name inferred). A
// ring expanding over the ground around a unit, its corners lifted except over space tiles (vtable 0x440744; RTTI
// EEE3DEnergyWave > EEEffect3D; EEE3DEnergyWave.cpp 0xee1b4..0xee5c0). Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DENERGYWAVE_H
#define EE_EEE3DENERGYWAVE_H

class EEGraphicObject;

class EEE3DEnergyWave : public EEEffect3D {
public:
    EEE3DEnergyWave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);   // 0xee1f7 (kind 0x47)
    EEE3DEnergyWave();                      // 0xee24f
    virtual ~EEE3DEnergyWave();             // +0x00 0xee263 (??_G 0xee233)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 0xee41b (the draw: AddGroundQuad; not for a cloaked enemy unit)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xee287 (the corner heights; queue 4)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xee3a4 (at the unit; loads the graphic)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xee4dd
    virtual long RepairPointers();          // +0x3c (EEEffect::RepairPointers; ICF-folded into EEE3DBlastWave's
                                            // 0xe93e0: the original vtable holds it, not EEEffect3D's inline forwarder)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xee51f
    virtual long Step();                    // +0x4c 0xee550 (the phase wraps at 1; 7 at the end of the duration)

    static const UString sData;             // "EEE3DEnergyWave-Data" (0x4e8b70; provisional name)

    EEGraphicObject* mGraphicObject;        // +0x74
    float mAge;                             // +0x78 0..1 (saved)
    float mPhase;                           // +0x7c 0..1 (saved)
    float mDrawPhase;                       // +0x80 (Virtual24 copies mPhase)
    float mHeights[4];                      // +0x84 the corners' (0 over a space tile)
};

#endif
