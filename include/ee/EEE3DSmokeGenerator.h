// C:\EEX\Empire Earth\Graphics Thread\EEE3DSmokeGenerator.h (RECONSTRUCTION; file name inferred). Emits its graphic
// effect's sub-effect mLinkedEffect1 every mL70 milliseconds around a point or a unit
// (vtable 0x440d48; RTTI EEE3DSmokeGenerator > EEEffect3D; EEE3DSmokeGenerator.cpp 0xf9871..0xf9b6d). sizeof 0x7c.
// Names are provisional. Requires ee/EEEffect3D.h.
//
// New (port, RUST_PLAN R4.4): the class is also the original's EEE3DDamageSmoke (kind 0x2c, vtable 0x4404b4: puffs from
// a damaged unit's tag points, more often the lower its hit points) and EEE3DDestructionSmoke (kind 0x1e, vtable
// 0x4405b8: smoke lines thrown out around a destroyed unit). Its state is the effect's record and its tick, set-up,
// load and save are ee_gfx effect_state.rs's, by kind; the C++ keeps the world's reads and the effects it creates.
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
    // 0xf9945 (kinds 0x2b; 0x2c: the damage smoke, EEE3DDamageSmoke 0xea703)
    EEE3DSmokeGenerator(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);
    // the destruction smoke at a destroyed unit's location, spread over half its class's scale (kind 0x1e;
    // EEE3DDestructionSmoke 0xebbfd)
    static EEE3DSmokeGenerator* CreateDestructionSmoke(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);
    EEE3DSmokeGenerator(long inKind);       // 0xf997f (the load's: 0x2b, 0x2c, 0x1e)
    // +0x00 empty (ICF-folded into EEE3DMultiEffect's 0xf43b8; ??_G 0xf9929)
    virtual ~EEE3DSmokeGenerator();
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 (likewise)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 (10: never drawn; ICF-folded into 0x1020dc)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 (the destruction smoke's lines; else 0)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf99b1
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf99e6
    // +0x4c 0xf9a1b (emits; 7 when the time is over or the unit is gone; the damage smoke's 0xea75c)
    virtual long Step();

    static const UString sData;             // "EEE3DSmokeGeneratorData" (0x4e8cd8; provisional name)
};

#endif
