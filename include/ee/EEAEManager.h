// EEAEManager.h (RECONSTRUCTION; file name inferred). The area-effect manager (RTTI EEAEManager; vtable 0x44bb40, ctor
// 0x27adb0; EEAEManager.cpp, named by its "fix me in EEAEManager.cpp!" message). The game's instance is a
// global object at 0x52f638 (provisional name). Only what the decompiled units call (provisional names).
#ifndef EE_EEAEMANAGER_H
#define EE_EEAEMANAGER_H
#include "rust/game/screens.h"
class FSFile;

#include <vector>
#include "ee/EEAreaEffect.h"               // mEffects by value (UReferenceContainer<EEAreaEffect*>)

class EEAreaEffectTemplate;
class EEComplexUnit;
class GE2DPoint;

class EEAEManager {
public:
    // Returns the effect of the kind that covers a tile and affects the player, or NULL
    EEAreaEffect* FindEffectAtTile(long inKind, long inX, long inY, unsigned long inPlayer)
    {
        return this->FindEffect(inKind, (float)inX, (float)inY, inPlayer, false, NULL);
    } // 0x67b527: ret 0x10, fild x/y, forwards to 0x67b3d2 (EEUnitClass 0x631ed1)
    // Releases every template and empties the active effect list
    void Clear() { ee_screens_aem_clear((uintptr_t)this); }    // original27b5d1 clears real manager-owned containers; no implementation credit
    // Reads the area-effect templates from the area-effect file (an empty slot for records with id 0); returns 3
    // when a template fails to initialize
    long Load(FSFile* inFile) { return ee_screens_aes_load_templates(this, inFile); } // original-backed 0x27b079; caller1414a3, FSFile IO proof
    EEAEManager();                          // 0x27adb0 (Empire Earth.cpp's chain)
    // Destroys the manager, releasing every template and effect
    virtual ~EEAEManager() { this->Clear(); }
    // Runs one area-effect tick: clear, apply, keep the strongest, update generators, rebuild the list, tick effects
    void Tick(float inElapsed) { ee_screens_aem_tick((uintptr_t)this, inElapsed); }             // 0x27b634 (EEPhysics::Fn153C59 every 2000; provisional name)
    // 0x27b4f3: whether an area effect of inKind covers tile (inX, inY) for inPlayer
    // Tells whether an effect of the kind covers a tile and affects the player
    bool HasAreaEffect(long inKind, long inX, long inY, long inPlayer)
    {
        return this->EffectAtTile(inKind, inX, inY, inPlayer, false);
    }
    // 0x27b16e: a new area effect from template inIndex for inUnit (referenced; 0 if none): by the template's kind
    // 1 healing, 2 morale, 3 calamity immunity, 4 conversion immunity, 5 morale from buildings, 6 morale from
    // military heroes, 7 economy from units, 8 cloaking (the unit is an EEComplexUnit, SetUnit's type; every
    // caller passes one)
    // Creates the area effect of the template at an index (its kind's: healing, morale, calamity and conversion immunity,
    // morale from buildings, from military heroes, economy from units, cloaking) for a unit; NULL for an unknown kind
    EEAreaEffect* CreateAreaEffect(unsigned long inIndex, EEComplexUnit* inUnit)
    {
        return (EEAreaEffect*)ee_screens_aem_create((uintptr_t)this, inIndex, (uintptr_t)inUnit);
    }
    // (EEAEManager.cpp; provisional names): mEffects emptied (0x27b064; the unit functor passes of Tick,
    // ApplyGeneratedEffectsToOthers 0x27ae23, ClearAreaEffects 0x27b04d, RebuildAreaEffectRegionList 0x27b576,
    // RemoveRedundantAreaEffects 0x27b5ba, UpdateGeneratedAreaEffects 0x27b673, UpdateAffectedByAreaEffects 0x27b693,
    // are Rust's: ee_game screens area_effect_obj.rs), the
    // effect of inKind at (inX, inY) for inPlayer (0x27b3d2; inMode: Virtual20 effects and the other relation test;
    // inExclude's effects skipped), its tile form (0x27b4ce), template inIndex's Unload (0x27b60b)
    // Empties the list of active area effects
    void ClearEffects() { ee_screens_aem_clear_effects((uintptr_t)this); }
    // Finds an active effect of a kind covering a point that affects the player (immunities only, by the
    // reversed player test, in immunity mode), skipping protected units' effects and those in the exclusion list
    EEAreaEffect* FindEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode, EEUnitAreaEffectList* inExclude)
    {
        return static_cast<EEAreaEffect*>(ee_screens_aem_find(this, inKind, inX, inY, inPlayer, inMode, inExclude));
    }
    // Tells whether an effect of the kind covers a tile for the player
    bool EffectAtTile(long inKind, long inX, long inY, long inPlayer, bool inMode)
    {
        return this->HasEffect(inKind, (float)inX, (float)inY, inPlayer, inMode, NULL);
    }
    // Unloads the media of the template at an index, if any
    void UnloadTemplateMedia(unsigned long inIndex) { ee_screens_aem_template_media((uintptr_t)this, inIndex, false); }
    // 0x27b50d: the same test with the other mode of 0x27b4ce (last argument true)
    // Tells whether an immunity effect of the kind covers a tile against the player
    bool HasAreaEffect2(long inKind, long inX, long inY, long inPlayer)
    {
        return this->EffectAtTile(inKind, inX, inY, inPlayer, true);
    }
    // 0x27b54b: area effect template inIndex's slot +4 with 0 (the tech tree's event kind 2; provisional name)
    // Loads the media of the template at an index, if any
    void LoadTemplateMedia(unsigned long inIndex) { ee_screens_aem_template_media((uintptr_t)this, inIndex, true); }
    // 0x27b4a2: whether 0x27b3d2 finds an area effect of inKind at (inX, inY) for inPlayer
    // (EEE2DMarchingIcons::IsIconShown passes its unit's list) (provisional name; types inferred)
    // Tells whether any active effect of the kind covers the point for the player
    bool HasEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode, EEUnitAreaEffectList* inList)
    {
        return this->FindEffect(inKind, inX, inY, inPlayer, inMode, inList) != NULL;
    }
    // 0x27ae3a: the unit's area effects over inRadius around inLocation (EEComplexUnit::ApplyAreaEffects) (provisional
    // name)
    // Adds the list's effects to every living unit they cover around a location (a radius-0 effect: to every
    // unit of every player); a healing effect skips its own unit and shows on the units it covers
    void CollectEffects(float inRadius, GE2DPoint& inLocation, unsigned long inPlayerID, EEUnitAreaEffectList* inList, EEComplexUnit* inUnit)
    {
        ee_screens_aem_collect(inRadius, inLocation.mX, inLocation.mY, inPlayerID, inList, inUnit);
    }

    // +0x04: the templates by index (Load pushes one per record; CreateAreaEffect/LoadTemplateMedia/UnloadTemplateMedia
    // index it; EESelectedGroup::ShowSelectionAreas 0x2095c7 reads [i]->mRadius through the out-of-line operator[]
    // 0x11e2a5)
    std::vector<EEAreaEffectTemplate*> mTemplates;
    // +0x14: every live area effect (vtable 0x446e74 stored by the ctor; FindEffect walks it) (provisional name)
    UReferenceContainer<EEAreaEffect*> mEffects;
};

extern EEAEManager gAEManager;

// port (save version 0.256, EEPWorldHelper): the units' own area effects' locations, the effects acting on each unit
// and the active list (EEAEManager.cpp), which the original rebuilt only at the next area-effect tick
void EEAESaveRunningState(FSFile& outFile);
long EEAELoadRunningState(FSFile& inFile);

#endif
