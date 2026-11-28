// EEComplexDoppelganger.h (RECONSTRUCTION from RTTI/vtables/ctors; slot signatures as EEDoppelganger's). Names are
// provisional.
// A doppelganger of a complex unit (RTTI EEComplexDoppelganger > EEDoppelganger > FSRestorable; vtable 0x446d5c;
// sizeof 0x70: EEDoppelgangerManager's pool; ctor 0x1f1f19; Units/EEComplexDoppelganger.cpp 0x1f1f19..0x1f57d3).
// +0x10 is a GETransformation (the ctor's member ctor).
#ifndef EE_EECOMPLEXDOPPELGANGER_H
#define EE_EECOMPLEXDOPPELGANGER_H

#include "ee/EEDoppelganger.h"
#include "rust/sim/records.h"

class EEComplexDoppelganger : public EEDoppelganger, public EeUnitsComplexDopFields {
public:
    EEComplexDoppelganger() { this->Reset(); }
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return (long)ee_units_dop_body(EE_UNITS_DOP_DO_LOAD, this, &inFile, inMajorVersion, inMinorVersion); }   // +0x04 0x1f21a3
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond) { return (long)ee_units_dop_more(EE_UNITS_DOPX_DO_REPAIR_POINTERS, this, (int64_t)inFirst, (int64_t)inSecond); }   // +0x08 0x1f230c
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return (long)ee_units_dop_body(EE_UNITS_DOP_DO_SAVE, this, &outFile, inMajorVersion, inMinorVersion); }  // +0x0c 0x1f2392
    virtual void Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile) { ee_units_dop_body(EE_UNITS_DOP_DO_INITIALIZE_COPY, this, inOther, (int64_t)(uintptr_t)inPlayer, (int64_t)(uintptr_t)inTile); }   // +0x14 0x1f212e
    virtual void Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile) { ee_units_dop_body(EE_UNITS_DOP_DO_INITIALIZE, this, inUnit, (int64_t)(uintptr_t)inPlayer, (int64_t)(uintptr_t)inTile); }   // +0x10 0x1f1fdf
    virtual long GetAnimation() { return this->mAnimation; }            // +0x18 0x1f1f3f (mL54)
    virtual EEComplexDoppelganger* GetComplexDoppelganger() { return this; }  // +0x1c 0x1f1f43 (this)
    virtual bool IsBuilt() { return (this->mPackedHitPoints & 0x80000000) > 0; }                 // +0x20 0x1f1f46
    virtual long GetFrame() { return this->mFrame; }                // +0x24 0x1f1f57 (mL68)
    virtual EEModel* GetModel() { return this->mModel; }            // +0x28 0x1f1f5b (mModel)
    virtual bool IsFoundation() { return this->mIsFoundation; }            // +0x2c 0x1f1f5f (mB6C)
    virtual bool IsComplex() { return true; }               // +0x30 (ICF-folded 0x18aa9: true)
    virtual void GetTransformation(GETransformation& outTransformation) { outTransformation = this->mTransformation; }  // +0x34 0x1f1f63
    virtual float GetScale() { return this->mScale; }               // +0x38 0x1f1f74 (mF58)
    virtual long GetHitPoints() { return this->mPackedHitPoints & 0x7fffffff; }            // +0x3c 0x1f1f78 (mL5C & 0x7fffffff)
    virtual long GetTileX() { return this->mTileX; }                // +0x44 0x1f1f81 (mL60)
    virtual long GetTileY() { return this->mTileY; }                // +0x48 0x1f1f85 (mL64)
    virtual ~EEComplexDoppelganger() { this->Clear(); }       // +0x4c (??_E 0x20348e, EEDoppelgangerManager.cpp)
    virtual void Clear() { ee_units_dop_more(EE_UNITS_DOPX_DO_CLEAR, this, 0, 0); }                   // +0x50 0x1f1fc2

    void Reset() { ee_units_dop_more(EE_UNITS_DOPX_DO_RESET, this, 0, 0); }                           // 0x1f1fa0 (the ctor and Clear)

    GETransformation mTransformation;       // +0x10 (the unit's location; Initialize, Virtual34)
    // +0x50 mModel .. +0x6c mIsFoundation: the record EeUnitsComplexDopFields (ee_sim units obj_fields.rs)
};

#endif
