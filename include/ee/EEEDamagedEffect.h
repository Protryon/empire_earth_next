// C:\EEX\Empire Earth\Graphics Thread\EEEDamagedEffect.h (RECONSTRUCTION, slots and handle types from
// EEEDamagedEffect.cpp 0x101fce..0x10251d; file name inferred). RTTI EEEDamagedEffect >
// EEEffect2D (vtable 0x441154; sizeof 0x78): the effect of kind 0x3e (its ctor passes it with the global model
// 0x4e8df8). It keeps one sub-effect of its graphic's mLinkedEffect1 (and of mLinkedEffect2) per missing share of the unit's hit
// points (the fire/smoke of a damaged building). Requires ee/EEEffect2D.h.
#ifndef EE_EEEDAMAGEDEFFECT_H
#define EE_EEEDAMAGEDEFFECT_H

class EEEDamagedEffect : public EEEffect2D {
public:
    EEEDamagedEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);   // 0x102011
    EEEDamagedEffect();                     // 0x102077 (the load-time ctor)
    virtual ~EEEDamagedEffect();            // +0x00 0x1020ba (??_G 0x10205b)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 (likewise)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0x1020dc (10: never drawn)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 (`return 0`: 0xfca98)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0x1020e2
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0x1021a2
    virtual long Step();                    // +0x4c 0x10225c (adds/removes sub-effects by the unit's damage)

    static const UString sData;             // "EEEDamagedEffect-Data" (0x4e8d88; provisional name)

    // +0x54 / +0x64: the handles of the graphic's mLinkedEffect1 and mLinkedEffect2 sub-effects are the record's (ee_gfx
    // effect_state.rs `EeGfxEffectLists`); the tick, load and save are Rust's
    bool& mIsFinished = this->mRecord->damaged.finished;   // +0x74 (the fire functor sets it instead of destroying the effect)
};

#endif
