// C:\EEX\Empire Earth\Graphics Thread\EEE3DBlastWave.h (RECONSTRUCTION; file name inferred). A
// model that grows and fades at its unit or a point (vtable 0x440394; RTTI EEE3DBlastWave > EEEffect3D;
// EEE3DBlastWave.cpp 0xe8ed8..0xe9474). sizeof 0x84. Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DBLASTWAVE_H
#define EE_EEE3DBLASTWAVE_H

class EEGraphicObject;
class GE3DPoint;

class EEE3DBlastWave : public EEEffect3D {
public:
    EEE3DBlastWave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xe8f1b
    EEE3DBlastWave(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xe8f85
    EEE3DBlastWave(long inKind);            // 0xe900e
    virtual ~EEE3DBlastWave();              // +0x00 0xe9026 (??_G 0xe8f69)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 0xe916c (the draw)
    virtual void ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);  // +0x20 (ICF-folded into 0xf717b)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 (ICF-folded into EEE3DScreenFlash's 0xf8db9)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xe9145
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xe9393
    virtual long RepairPointers();          // +0x3c 0xe93e0 (EEEffect::RepairPointers)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xe93e5
    virtual long Step();                    // +0x4c 0xe9421 (follows the unit; ages mF78; 7 past 1)

    void FollowUnit();                      // 0xe904a: the transformation from the unit's model (provisional name)

    static const UString sData;             // "EEE3DBlastWave-Data" (0x4e8a68; provisional name)

    EEGraphicObject* mGraphicObject;        // +0x74
    float& mAge = this->mRecord->aged.age;   // +0x78 the age, 0..1 (saved; the record's: ee_gfx effect_state.rs)
    float mDrawAge;                         // +0x7c the age the draw uses (Virtual24 copies mF78)
    bool& mIsAging = this->mRecord->aged.aging;   // +0x80 (true: ages; saved; the record's)
};

#endif
