// EEAEManager.h (RECONSTRUCTION; file name inferred). The area-effect manager (RTTI EEAEManager; vtable 0x44bb40, ctor
// 0x27adb0; EEAEManager.cpp, named by its "fix me in EEAEManager.cpp!" message). The game's instance is a
// global object at 0x52f638 (provisional name). Only what the decompiled units call (provisional names).
#ifndef EE_EEAEMANAGER_H
#define EE_EEAEMANAGER_H
class FSFile;

#include <vector>
#include "ee/EEAreaEffect.h"               // mEffects by value (UReferenceContainer<EEAreaEffect*>)

class EEAreaEffectTemplate;
class EEUnit;
class EEComplexUnit;
class GE2DPoint;

class EEAEManager {
public:
    EEAreaEffect* FindEffectAtTile(long inKind, long inX, long inY, unsigned long inPlayer); // 0x67b527: ret 0x10, fild x/y, forwards to 0x67b3d2 (EEUnitClass 0x631ed1)
    void Clear();    // original27b5d1 clears real manager-owned containers; no implementation credit
    long Load(FSFile* inFile); // original-backed 0x27b079; caller1414a3, FSFile IO proof
    EEAEManager();                          // 0x27adb0 (Empire Earth.cpp's chain)
    virtual ~EEAEManager();                 // 0x27ae03
    void Tick(float inElapsed);             // 0x27b634 (EEPhysics::Fn153C59 every 2000; provisional name)
    // 0x27b4f3: whether an area effect of inKind covers tile (inX, inY) for inPlayer
    bool HasAreaEffect(long inKind, long inX, long inY, long inPlayer);
    // 0x27b16e: a new area effect from template inIndex for inUnit (referenced; 0 if none): by the template's kind
    // 1 healing, 2 morale, 3 calamity immunity, 4 conversion immunity, 5 morale from buildings, 6 morale from
    // military heroes, 7 economy from units, 8 cloaking (the unit is an EEComplexUnit, SetUnit's type; every
    // caller passes one)
    EEAreaEffect* CreateAreaEffect(unsigned long inIndex, EEComplexUnit* inUnit);
    // (EEAEManager.cpp; provisional names): the unit functor passes of Tick (ApplyGeneratedEffectsToOthers 0x27ae23,
    // ClearAreaEffects 0x27b04d, RebuildAreaEffectRegionList 0x27b576, RemoveRedundantAreaEffects 0x27b5ba,
    // UpdateGeneratedAreaEffects 0x27b673, UpdateAffectedByAreaEffects 0x27b693), mEffects emptied (0x27b064), the
    // effect of inKind at (inX, inY) for inPlayer (0x27b3d2; inMode: Virtual20 effects and the other relation test;
    // inExclude's effects skipped), its tile form (0x27b4ce), template inIndex's Unload (0x27b60b)
    void ApplyGeneratedAreaEffects();
    void ClearAreaEffects();
    void ClearEffects();
    EEAreaEffect* FindEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                           EEUnitAreaEffectList* inExclude);
    bool EffectAtTile(long inKind, long inX, long inY, long inPlayer, bool inMode);
    void RebuildAreaEffectRegionList();
    void RemoveRedundantAreaEffects();
    void UnloadTemplateMedia(unsigned long inIndex);
    void UpdateGeneratedAreaEffects(float inElapsed);
    void UpdateAffectedByAreaEffects(float inElapsed);
    // 0x27b50d: the same test with the other mode of 0x27b4ce (last argument true)
    bool HasAreaEffect2(long inKind, long inX, long inY, long inPlayer);
    // 0x27b54b: area effect template inIndex's slot +4 with 0 (the tech tree's event kind 2; provisional name)
    void LoadTemplateMedia(unsigned long inIndex);
    // 0x27b4a2: whether 0x27b3d2 finds an area effect of inKind at (inX, inY) for inPlayer
    // (EEE2DMarchingIcons::IsIconShown passes its unit's list) (provisional name; types inferred)
    bool HasEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode, EEUnitAreaEffectList* inList);
    // 0x27ae3a: the unit's area effects over inRadius around inLocation (EEComplexUnit::ApplyAreaEffects) (provisional
    // name)
    void CollectEffects(float inRadius, GE2DPoint& inLocation, unsigned long inPlayerID, EEUnitAreaEffectList* inList,
                  EEComplexUnit* inUnit);

    // +0x04: the templates by index (Load pushes one per record; CreateAreaEffect/LoadTemplateMedia/UnloadTemplateMedia
    // index it; EESelectedGroup::ShowSelectionAreas 0x2095c7 reads [i]->mRadius through the out-of-line operator[]
    // 0x11e2a5)
    std::vector<EEAreaEffectTemplate*> mTemplates;
    // +0x14: every live area effect (vtable 0x446e74 stored by the ctor; FindEffect walks it) (provisional name)
    UReferenceContainer<EEAreaEffect*> mEffects;
};

extern EEAEManager gAEManager;              // 0x52f638

// port (save version 0.255, EEPWorldHelper): the units' own area effects' locations, the effects acting on each unit
// and the active list (EEAEManager.cpp), which the original rebuilt only at the next area-effect tick
void EEAESaveRunningState(FSFile& outFile);
long EEAELoadRunningState(FSFile& inFile);

#endif
