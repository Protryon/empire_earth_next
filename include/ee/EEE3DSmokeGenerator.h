// C:\EEX\Empire Earth\Graphics Thread\EEE3DSmokeGenerator.h (RECONSTRUCTION; file name inferred). Emits its graphic
// effect's sub-effect mLinkedEffect1 every mL70 milliseconds around a point or a unit
// (vtable 0x440d48; RTTI EEE3DSmokeGenerator > EEEffect3D; EEE3DSmokeGenerator.cpp 0xf9871..0xf9b6d). sizeof 0x7c.
// Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DSMOKEGENERATOR_H
#define EE_EEE3DSMOKEGENERATOR_H

class EEE3DSmokeGenerator : public EEEffect3D {
public:
    EEE3DSmokeGenerator(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xf98b4
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    EEE3DSmokeGenerator(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
        : EEE3DSmokeGenerator(const_cast<GE3DPoint&>(inLocation), inGraphicEffect, inKind) {}
#endif
    EEE3DSmokeGenerator(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xf9945
    EEE3DSmokeGenerator(long inKind);       // 0xf997f
    // +0x00 empty (ICF-folded into EEE3DMultiEffect's 0xf43b8; ??_G 0xf9929)
    virtual ~EEE3DSmokeGenerator();
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 (likewise)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 (10: never drawn; ICF-folded into 0x1020dc)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 (`return 0`: ICF-folded into 0xfca98)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf99b1
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf99e6
    virtual long Step();                    // +0x4c 0xf9a1b (emits; 7 when the time is over or the unit is gone)
    virtual void EmitPuff(GE3DPoint& inLocation);    // +0x58 0xf9997 (one puff: CreatePlacedEffect(mLinkedEffect1))

    static const UString sData;             // "EEE3DSmokeGeneratorData" (0x4e8cd8; provisional name)

    float mTime;                            // +0x74 milliseconds alive (saved)
    float mSinceEmit;                       // +0x78 milliseconds since the last puff (saved)
};

#endif
