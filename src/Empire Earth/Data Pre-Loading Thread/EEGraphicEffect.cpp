// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEGraphicEffect.cpp (name from RTTI EEGraphicEffect; no __FILE__).
// 0xd6bed..0xd72c1: the effect table of db\dbgfxeffects.dat (EEGraphicEffect : UReferenceCounter, EEMedia). The
// three statics' init chains (sEffects 0x4e8420, the 0x48 per-kind media vectors 0x4e7f98, the id set 0x4e7f88;
// guard byte 0x4e8418) open the unit. Unit flags: the exe's module set.
#include <algorithm>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"



// The tables (sEffects 0x4e8420, the 0x48 per-kind media vectors 0x4e7f98, the id set 0x4e7f88: the unit's three
// init chains) and the loads are ee_gfx geffect_table.rs'
extern std::vector<EEMedia*> gMediaTable;   // 0x4e8448 (EEGraphicObject.cpp)

// Reads the graphic effect table from the file (the record and its copy are ee_data tables.rs'; rows whose id is not
// above 0 stay NULL) and links each effect's three follow-on effects by index; returns the last read result
long EEGraphicEffect::LoadTable(FSFile* inFile)
{
    return ee_preload_load_effects(inFile);   // (preload_media.rs)
}

// Writes to the file every graphic effect that was used without being preloaded (the recorded ids, each read as a
// table index as the original did; ee_game preload_media.rs)
void EEGraphicEffect::DumpIDs(FSFileSpec& inFile)
{
    ee_preload_dump_effect_ids(&inFile);
}

// Releases every graphic effect in the table and empties it (preload_media.rs)
void EEGraphicEffect::UnloadTable()
{
    ee_preload_unload_effects();
}

EEGraphicEffect* EEGraphicEffect::Get(unsigned long inIndex)
{
    return static_cast<EEGraphicEffect*>(ee_gfx_geffect_get(inIndex));
}

unsigned long EEGraphicEffect::GetCount()
{
    return ee_gfx_geffect_count();
}

unsigned long EEGraphicEffect::IndexOf(const EEGraphicEffect* inEffect)
{
    return ee_gfx_geffect_index_of(inEffect);
}

// Returns the effect's graphic, or a random graphic of its kind when it has none of its own (preload_media.rs)
EEMedia* EEGraphicEffect::GetMedia()
{
    return static_cast<EEMedia*>(ee_preload_effect_media(this));
}

// Files every loaded graphic object under its kind (kinds below 0x48) for kind-only effects
void EEGraphicEffect::SortMediaByKind()
{
    ee_gfx_geffect_sort_media(reinterpret_cast<void* const*>(gMediaTable.data()), gMediaTable.size());
}

// The graphics' loads (their EEMedia virtuals) for geffect_table.rs (an effect's EEGraphicObject is its first base,
// EEMedia, at the same address)
static const EeGfxGeffectHost kGeffectHost = {
    [](void* inMedia) { static_cast<EEMedia*>(inMedia)->LoadMedia(); },
    [](void* inMedia) { static_cast<EEMedia*>(inMedia)->UnloadMedia(); },
};

// Loads the effect's graphic (or every graphic of its kind) and its linked effects; on demand, records it as
// not preloaded
void EEGraphicEffect::Load(bool inIsOnDemand)
{
    ee_gfx_geffect_load(this, inIsOnDemand, &kGeffectHost);
}

// Unloads the effect's graphic (or every graphic of its kind) and its linked effects
void EEGraphicEffect::Unload()
{
    ee_gfx_geffect_unload(this, &kGeffectHost);
}

// Returns the number of entries in the media table
unsigned long EEMedia::GetCount()
{
    return gMediaTable.size();
}

// Loads the effect as preloaded media; always returns 0
long EEGraphicEffect::LoadMedia()
{
    this->Load(false);
    return 0;
}

// Unloads the effect's media; always returns 0
long EEGraphicEffect::UnloadMedia()
{
    this->Unload();
    return 0;
}
