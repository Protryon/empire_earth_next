// C:\EEX\Empire Earth\Database\EEGraphicEffect.h (RECONSTRUCTION; file name inferred). One effect of
// db\dbgfxeffects.dat (RTTI bases UReferenceCounter +0, EEMedia +8; vtables 0x43e534/0x43e524). Field names
// are provisional; offsets proven by the calamity units. Requires lle/LowLevelEngine.h, ee/EEMedia.h.
#ifndef EE_EEGRAPHICEFFECT_H
#define EE_EEGRAPHICEFFECT_H

#include <vector>

#include "rust/data/records.h"
#include "rust/data/tables.h"
#include "ee/EEMedia.h"

class EEGraphicObject;
class FSFile;
class FSFileSpec;

// The record's members (mSize .. mIsEmitFlat but the graphic, the links and mNumLoads) are EeDataGraphicEffectFields
// (rust/crates/ee_data tables.rs: the record and its copy), the third base.
class EEGraphicEffect : public UReferenceCounter, public EEMedia, public EeDataGraphicEffectFields {
public:
    // the table of every effect by index (out-of-line COMDAT 0x432f: 0 past the end; vector 0x4e8424) (provisional
    // name); the table (and the per-kind media and the not-preloaded ids) are ee_gfx geffect_table.rs'
    static EEGraphicEffect* Get(unsigned long inIndex);
    // (inline in the original: COMDAT 0x185a71 after its first user, EESETSETGame.cpp's GraphicEffect::Virtual14 loop)
    static unsigned long GetCount();
    EEGraphicEffect* GetLinkedEffect1() const { return this->mLinkedEffect1; }  // inline: 0x50225c
    EEGraphicEffect* GetLinkedEffect2() const { return this->mLinkedEffect2; }  // inline: 0x50225c
    float GetSize() const { return this->mSize; }  // inline (the hurricane copies it through the FP stack)
    // inline: the 3D effects load the parameters as values (provisional name)
    float GetParam2() const { return this->mParam2; }
    // the unit EEGraphicEffect.cpp (0xd6bed..0xd72c1); the ctor copies a database record
    EEGraphicEffect(const EeDataGraphicEffectFields& inFields, EEGraphicObject* inGraphicObject) : EeDataGraphicEffectFields(inFields), mGraphicObject(inGraphicObject) {  }
    // ??_G 0xd6e5d (EEMedia's adjusting thunk 0xd726c), dtor 0xd6e79
    virtual ~EEGraphicEffect() {  }
    virtual long LoadMedia();               // EEMedia +0x04 0xd72a9
    virtual long UnloadMedia();             // EEMedia +0x08 0xd72b6
    static long LoadTable(FSFile* inFile);  // 0xd6e9e (db\dbgfxeffects.dat)
    static void DumpIDs(FSFileSpec& inFile);    // 0xd6fe6 (fprintf to the spec's stream; EEWorld::DumpMedia)
    static void SortMediaByKind();
    void Load(bool inIsOnDemand);           // 0xd7100 (counts a use at +0x80) (provisional name)
    void Unload();

    // members (the ctor 0xd6ceb's stores, a database record copied field by field; types by width only,
    // provisional names; sizeof 0x90)
    EEGraphicObject* mGraphicObject;        // +0x0c
    EEGraphicEffect* mLinkedEffect1 = NULL;       // +0x10 a sub-effect (EEE3DMultiEffect ctors pass it to CreatePlacedEffect)
    EEGraphicEffect* mLinkedEffect2 = NULL;       // +0x14 a sub-effect (EEE3DMultiEffect ctors pass it to CreatePlacedEffect)
    EEGraphicEffect* mLinkedEffect3 = NULL;       // +0x18 a sub-effect (EEE3DMultiEffect ctors pass it to CreatePlacedEffect)
    long mNumLoads = 0;                         // +0x80
};

// dbgfxeffects' effects by table index (EEGraphicEffectID): Rust's enum (ee_sim game_ids.rs), emitted back as a plain C++ enum with its evidence comments.
#include "rust/sim/game_ids.h"

#endif
