// C:\EEX\Empire Earth\Civilization Builder\EEAEManager.cpp (named by its "fix me in EEAEManager.cpp!" message; folder
// inferred). 0x27adb0..0x27b911: the area-effect manager (vtable 0x44bb40; the game's gAEManager): ctor, ??_G,
// dtor, the unit functor passes (vtables 0x44bb50.., 0x44bd28..), CollectEffects, Load, CreateAreaEffect with the inline
// area-effect kinds it emitted (EEAEMorale's slots and ??_G 0x27b2d7..0x27b335, the immunities' and economy's ??_G and
// slots 0x27b357..0x27b3d2; port: one class, ee/EEAreaEffect.h), the effect queries, the template loads, Tick, the functors' slots, then the template COMDATs
// (vector<EEAreaEffectTemplate*>::push_back 0x27b751, EEGetUnitsInRectangle<EEAEAliveUnit> 0x27b77d).
// EEAEMoraleFromBuildings.cpp starts at 0x27b911. Unit flags: the exe's module set.
#include <windows.h>
#include <string.h>
#include <vector>
#include <functional>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEAECapture.h"
#include <memory>
#include <stdint.h>

// The unit functors of the manager's passes (RTTI names; each > EEUUUnitFunctor, slot +0x04 only; their ??_G is
// ICF-shared with EEPSuggestRecomputeAllAreaEffects's 0x16b555)
class ApplyGeneratedEffectsToOthersFunctor : public EEUUUnitFunctor {   // vtable 0x44bb50
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->ApplyAreaEffects(); }  // 0x27b6bf
};

class SetAffectedByAreaEffectFunctor : public EEUUUnitFunctor {         // vtable 0x44bb60
public:
    SetAffectedByAreaEffectFunctor(EEAreaEffect* inEffect, unsigned long inPlayer, const GE2DPoint& inLocation)
        : mEffect(inEffect), mPlayer(inPlayer), mLocation(inLocation) {}
    virtual void Apply(EEComplexUnit* inUnit);       // 0x27b015

    EEAreaEffect* mEffect;                  // +0x04
    unsigned long mPlayer;                  // +0x08
    GE2DPoint mLocation;                    // +0x0c
};

class ClearAreaEffectsFunctor : public EEUUUnitFunctor {                // vtable 0x44bb70
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->RemoveAffectingAreaEffects(); }  // 0x27b6b3
};

class RebuildAreaEffectRegionListFunctor : public EEUUUnitFunctor {     // vtable 0x44bd28
public:
    RebuildAreaEffectRegionListFunctor(UReferenceContainer<EEAreaEffect*>* inEffects);   // 0x27b593
    virtual void Apply(EEComplexUnit* inUnit);       // 0x27b70c

    UReferenceContainer<EEAreaEffect*>* mEffects;   // +0x04
};

class RemoveRedundantAreaEffectsFunctor : public EEUUUnitFunctor {      // vtable 0x44bd38
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAffectingAreaEffects.KeepStrongest(); }  // 0x27b6e0
};

class UpdateGeneratedAreaEffectsFunctor : public EEUUUnitFunctor {      // vtable 0x44bd48
public:
    UpdateGeneratedAreaEffectsFunctor(float inElapsed) : mElapsed(inElapsed) {}
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->UpdateAreaEffects((unsigned long)this->mElapsed); }  // 0x27b6cb

    float mElapsed;                         // +0x04
};

class UpdateAffectedByAreaEffectsFunctor : public EEUUUnitFunctor {     // vtable 0x44bd58
public:
    UpdateAffectedByAreaEffectsFunctor(float inElapsed) : mElapsed(inElapsed) {}
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAffectingAreaEffects.Tick(inUnit, this->mElapsed); }    // 0x27b6f2

    float mElapsed;                         // +0x04
};

// The living units (expanded into the collector 0x27b77d) (provisional name)
struct EEAEAliveUnit {
    bool operator()(EEComplexUnit* inUnit) const { return inUnit->mHitPoints > 0; }
};

// FUNCTION: EXE 0x67adb0
// Creates the area-effect manager with no templates or effects
EEAEManager::EEAEManager()
{
}

// SYNTHETIC: EXE 0x67ade7
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x67ae03
// Destroys the manager, releasing every template and effect
EEAEManager::~EEAEManager()
{
    this->Clear();
}

// FUNCTION: EXE 0x67ae23
// Lets every unit apply the area effects it generates to the units around it
void EEAEManager::ApplyGeneratedAreaEffects()
{
    ApplyGeneratedEffectsToOthersFunctor theFunctor;
    EEForEachPlayersUnit(&theFunctor);
}

// FUNCTION: EXE 0x67ae3a
// Adds the list's effects to every living unit they cover around a location (a radius-0 effect: to every
// unit of every player); a healing effect skips its own unit and shows on the units it covers
void EEAEManager::CollectEffects(float inRadius, GE2DPoint& inLocation, unsigned long inPlayerID,
                           EEUnitAreaEffectList* inList, EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*> theUnits;
    GE2DPoint theCenter = inLocation;
    long theBottom = UMFloor(theCenter.mY + inRadius);
    long theRight = UMFloor(theCenter.mX + inRadius);
    long theTop = UMFloor(theCenter.mY - inRadius);
    long theLeft = UMFloor(theCenter.mX - inRadius);
    U2DRectangle<long> theRect;
    theRect.mTop = theTop;
    theRect.mLeft = theLeft;
    theRect.mBottom = theBottom;
    theRect.mRight = theRight;
    EEGetUnitsInRectangle(theRect, theUnits, EEAEAliveUnit(), true);
    for (std::vector<EEAreaEffect*>::iterator theIterator = inList->mItems.begin(); theIterator != inList->mItems.end();
         ++theIterator) {
        EEAreaEffect* theEffect = *theIterator;
        if (theEffect->mUnit != NULL && theEffect->mUnit->IsProtected())
            continue;
        if (theEffect->mTemplate->mRadius <= 0.0f) {
            SetAffectedByAreaEffectFunctor theFunctor(theEffect, inPlayerID, inLocation);
            EEForEachPlayersUnit(&theFunctor);
        } else {
            unsigned long theCount = theUnits.size();
            for (unsigned long i = 0; i < theCount; i++) {
                EEComplexUnit* theUnit = theUnits[i]->mComplexUnit;
                if (!theEffect->Covers(inPlayerID, theUnit, inLocation) ||
                    (theUnit == inUnit && theEffect->mTemplate->mKind == kAreaEffectHealing))
                    continue;
                theUnit->mAffectingAreaEffects.Add(theEffect);
                theEffect->AppliedTo(theUnit);
                if (theEffect->mTemplate->mKind == kAreaEffectHealing)
                    theEffect->ShowOn(theUnit);
            }
        }
    }
}

// FUNCTION: EXE 0x67b015
// Adds the effect to a covered unit's affecting effects and applies it
void SetAffectedByAreaEffectFunctor::Apply(EEComplexUnit* inUnit)
{
    if (this->mEffect->Covers(this->mPlayer, inUnit, this->mLocation)) {
        inUnit->mAffectingAreaEffects.Add(this->mEffect);
        this->mEffect->AppliedTo(inUnit);
    }
}

// FUNCTION: EXE 0x67b04d
// Clears the affecting area effects of every unit
void EEAEManager::ClearAreaEffects()
{
    ClearAreaEffectsFunctor theFunctor;
    EEForEachPlayersUnit(&theFunctor);
}

// FUNCTION: EXE 0x67b064
// Empties the list of active area effects
void EEAEManager::ClearEffects()
{
    UReferenceContainer<EEAreaEffect*>& theEffects = this->mEffects;
    theEffects.Clear();
    theEffects.mItems.clear();
}

// FUNCTION: EXE 0x67b079
// Reads the area-effect templates from the area-effect file (an empty slot for records with id 0); returns 3
// when a template fails to initialize
long EEAEManager::Load(FSFile* inFile)
{
    EEAreaEffectTemplate* theTemplate = NULL;
    unsigned long theCount = 0;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EEAreaEffectData theData;
            unsigned long theRead;
            memset(&theData, 0, sizeof(theData));
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (theData.mID > 0) {
                theTemplate = new EEAreaEffectTemplate;
                if (!theTemplate->Init(theData)) {
                    ESDebugOut("Error reading area effect template %s...fix me in EEAEManager.cpp!\n", theData.mName);
                    theResult = 3;
                }
            } else {
                theTemplate = NULL;
            }
            this->mTemplates.push_back(theTemplate);
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x67b16e
// Creates the area effect of the template at an index (its kind's: healing, morale, calamity and conversion immunity,
// morale from buildings, from military heroes, economy from units, cloaking) for a unit; NULL for an unknown kind
EEAreaEffect* EEAEManager::CreateAreaEffect(unsigned long inIndex, EEComplexUnit* inUnit)
{
    EEAreaEffect* theEffect = NULL;
    if (inIndex < this->mTemplates.size()) {
        EEAreaEffectTemplate* theTemplate = this->mTemplates[inIndex];
        if (ee_area_effect_known_kind(theTemplate->mKind))
            theEffect = new EEAreaEffect(theTemplate);
        if (theEffect != NULL) {
            theEffect->SetUnit(inUnit);
            theEffect->mPlayer = inUnit->mPlayer->mPlayerID;
        }
    }
    return theEffect;
}

// FUNCTION: EXE 0x67b3d2
// Finds an active effect of a kind covering a point that affects the player (immunities only, by the
// reversed player test, in immunity mode), skipping protected units' effects and those in the exclusion list
EEAreaEffect* EEAEManager::FindEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                                    EEUnitAreaEffectList* inExclude)
{
    GE2DPoint thePoint(inX, inY);
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mEffects.mItems.begin(); ; ++theIterator) {
        if (theIterator == this->mEffects.mItems.end())
            break;
        EEAreaEffect* theEffect = *theIterator;
        if (theEffect->mUnit != NULL && theEffect->mUnit->IsProtected())
            continue;
        if (inExclude != NULL) {
            std::vector<EEAreaEffect*>::iterator theOther = inExclude->mItems.begin();
            for (; theOther != inExclude->mItems.end(); ++theOther) {
                if (*theOther == theEffect)
                    break;
            }
            if (theOther != inExclude->mItems.end())
                continue;
        }
        if (inMode && !theEffect->IsImmunity())
            continue;
        if (theEffect->mTemplate->mKind != inKind)
            continue;
        if (!(!inMode ? theEffect->mTemplate->AffectsPlayer(theEffect->mPlayer, inPlayer) :
                        theEffect->mTemplate->AffectsPlayerReversed(theEffect->mPlayer, inPlayer)))
            continue;
        if (theEffect->IsInRange(thePoint))
            return theEffect;
    }
    return NULL;
}

// FUNCTION: EXE 0x67b480
// Tells whether a point lies in the effect's scaled area around its location
bool EEAreaEffect::IsInRange(const GE2DPoint& inPoint)
{
    return this->mTemplate->IsWithin(inPoint, this->mLocation, this->GetScale(), NULL);
}

// FUNCTION: EXE 0x67b4a2
// Tells whether any active effect of the kind covers the point for the player
bool EEAEManager::HasEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                           EEUnitAreaEffectList* inList)
{
    return this->FindEffect(inKind, inX, inY, inPlayer, inMode, inList) != NULL;
}

// FUNCTION: EXE 0x67b4ce
// Tells whether an effect of the kind covers a tile for the player
bool EEAEManager::EffectAtTile(long inKind, long inX, long inY, long inPlayer, bool inMode)
{
    return this->HasEffect(inKind, (float)inX, (float)inY, inPlayer, inMode, NULL);
}

// FUNCTION: EXE 0x67b4f3
// Tells whether an effect of the kind covers a tile and affects the player
bool EEAEManager::HasAreaEffect(long inKind, long inX, long inY, long inPlayer)
{
    return this->EffectAtTile(inKind, inX, inY, inPlayer, false);
}

// FUNCTION: EXE 0x67b50d
// Tells whether an immunity effect of the kind covers a tile against the player
bool EEAEManager::HasAreaEffect2(long inKind, long inX, long inY, long inPlayer)
{
    return this->EffectAtTile(inKind, inX, inY, inPlayer, true);
}

// FUNCTION: EXE 0x67b527
// Returns the effect of the kind that covers a tile and affects the player, or NULL
EEAreaEffect* EEAEManager::FindEffectAtTile(long inKind, long inX, long inY, unsigned long inPlayer)
{
    return this->FindEffect(inKind, (float)inX, (float)inY, inPlayer, false, NULL);
}

// FUNCTION: EXE 0x67b54b
// Loads the media of the template at an index, if any
void EEAEManager::LoadTemplateMedia(unsigned long inIndex)
{
    if (inIndex >= this->mTemplates.size())
        return;
    EEAreaEffectTemplate* theTemplate = this->mTemplates[inIndex];
    if (theTemplate != NULL)
        theTemplate->Load(false);
}

// FUNCTION: EXE 0x67b576
// Rebuilds the list of active area effects from every built unit's own effects
void EEAEManager::RebuildAreaEffectRegionList()
{
    RebuildAreaEffectRegionListFunctor theFunctor(&this->mEffects);
    EEForEachPlayersUnit(&theFunctor);
}

// FUNCTION: EXE 0x67b593
// Starts a rebuild of the active effect list by emptying it
RebuildAreaEffectRegionListFunctor::RebuildAreaEffectRegionListFunctor(UReferenceContainer<EEAreaEffect*>* inEffects)
{
    this->mEffects = inEffects;
    inEffects->Clear();
    inEffects->mItems.clear();
}

// FUNCTION: EXE 0x67b5ba
// Keeps only each unit's strongest affecting effect of each kind
void EEAEManager::RemoveRedundantAreaEffects()
{
    RemoveRedundantAreaEffectsFunctor theFunctor;
    EEForEachPlayersUnit(&theFunctor);
}

// FUNCTION: EXE 0x67b5d1
// Releases every template and empties the active effect list
void EEAEManager::Clear()
{
    for (std::vector<EEAreaEffectTemplate*>::iterator theIterator = this->mTemplates.begin();
         theIterator != this->mTemplates.end(); ++theIterator) {
        if (*theIterator != NULL)
            (*theIterator)->Dereference();
    }
    this->mTemplates.clear();
    UReferenceContainer<EEAreaEffect*>& theEffects = this->mEffects;
    theEffects.Clear();
    theEffects.mItems.clear();
}

// FUNCTION: EXE 0x67b60b
// Unloads the media of the template at an index, if any
void EEAEManager::UnloadTemplateMedia(unsigned long inIndex)
{
    if (inIndex >= this->mTemplates.size())
        return;
    EEAreaEffectTemplate* theTemplate = this->mTemplates[inIndex];
    if (theTemplate != NULL)
        theTemplate->Unload();
}

// FUNCTION: EXE 0x67b634
// Runs one area-effect tick: clear, apply, keep the strongest, update generators, rebuild the list, tick effects
void EEAEManager::Tick(float inElapsed)
{
    this->ClearAreaEffects();
    this->ApplyGeneratedAreaEffects();
    this->RemoveRedundantAreaEffects();
    this->UpdateGeneratedAreaEffects(inElapsed);
    this->RebuildAreaEffectRegionList();
    this->UpdateAffectedByAreaEffects(inElapsed);
    EEAECaptureTick(inElapsed);   // TEST ONLY: AEFX (ee/EEAECapture.h)
}

// FUNCTION: EXE 0x67b673
// Updates the area effects every unit generates for the elapsed time
void EEAEManager::UpdateGeneratedAreaEffects(float inElapsed)
{
    UpdateGeneratedAreaEffectsFunctor theFunctor(inElapsed);
    EEForEachPlayersUnit(&theFunctor);
}

// FUNCTION: EXE 0x67b693
// Ticks every unit's affecting area effects for the elapsed time
void EEAEManager::UpdateAffectedByAreaEffects(float inElapsed)
{
    UpdateAffectedByAreaEffectsFunctor theFunctor(inElapsed);
    EEForEachPlayersUnit(&theFunctor);
}

// FUNCTION: EXE 0x67b70c
// Adds a built unit's own area effects to the active effect list
void RebuildAreaEffectRegionListFunctor::Apply(EEComplexUnit* inUnit)
{
    if (!inUnit->mIsBuilt)
        return;
    for (std::vector<EEAreaEffect*>::iterator theIterator = inUnit->mAreaEffects.mItems.begin();
         theIterator != inUnit->mAreaEffects.mItems.end(); ++theIterator)
        this->mEffects->Add(*theIterator);
}

// port (save version 0.256, EEPWorldHelper): the area effects' running state. The original saved none of it: a
// loaded game made each unit's own effects anew (MakeAreaEffects: no location, morale from buildings to be counted)
// and every unit's affecting list and the active list were empty until the next area-effect tick (up to 2 s: no
// morale in the damage, no immunities, no cloaking). Saved: per unit, its own effects' location (and the
// morale-from-buildings count), then the effects acting on it, each as its owner unit and index; then the active list
// the same way (the record and its bytes: ee_tech area_effect_save.rs). Put back at the loaded game's first tick
// (EEAfterLoad).
namespace {
// FSFile's typed reads and writes for the Rust record (the calamities' I/O ops)
long EEAEIoCall(void* inContext, uint32_t inOp, void* ioValue)
{
    FSFile& theFile = *static_cast<FSFile*>(inContext);
    switch (inOp) {
    case EE_CALAMITY_IO_READ_ULONG: return theFile.Read(*static_cast<unsigned long*>(ioValue));
    case EE_CALAMITY_IO_READ_LONG: return theFile.Read(*static_cast<long*>(ioValue));
    case EE_CALAMITY_IO_READ_FLOAT: return theFile.Read(*static_cast<float*>(ioValue));
    case EE_CALAMITY_IO_READ_BOOL: return theFile.Read(*static_cast<bool*>(ioValue));
    case EE_CALAMITY_IO_WRITE_ULONG: return theFile.Write(*static_cast<const unsigned long*>(ioValue));
    case EE_CALAMITY_IO_WRITE_LONG: return theFile.Write(*static_cast<const long*>(ioValue));
    case EE_CALAMITY_IO_WRITE_FLOAT: return theFile.Write(*static_cast<const float*>(ioValue));
    case EE_CALAMITY_IO_WRITE_BOOL: return theFile.Write(*static_cast<const bool*>(ioValue));
    }
    return 1;
}

EeAreaEffectSavedRef EEAERefOf(EEAreaEffect* inEffect)
{
    EeAreaEffectSavedRef theRef = {(unsigned long)-1, -1};
    EEComplexUnit* theOwner = inEffect->mUnit;
    if (theOwner == NULL)
        return theRef;
    theRef.mOwner = theOwner->GetUnitID();
    for (size_t i = 0; i < theOwner->mAreaEffects.mItems.size(); i++)
        if (theOwner->mAreaEffects.mItems[i] == inEffect)
            theRef.mIndex = (long)i;
    return theRef;
}

EEAreaEffect* EEAEEffectOf(const EeAreaEffectSavedRef& inRef)
{
    if (inRef.mIndex < 0)
        return NULL;
    EEUnit* theUnit = ::gWorld.GetUnit(inRef.mOwner);
    EEComplexUnit* theOwner = theUnit != NULL ? theUnit->mComplexUnit : NULL;
    if (theOwner == NULL || (size_t)inRef.mIndex >= theOwner->mAreaEffects.mItems.size())
        return NULL;
    return theOwner->mAreaEffects.mItems[inRef.mIndex];
}

EEComplexUnit* EEAEComplexUnit(unsigned long inID)
{
    EEUnit* theUnit = ::gWorld.GetUnit(inID);
    return theUnit != NULL ? theUnit->mComplexUnit : NULL;
}
}

void EEAESaveRunningState(FSFile& outFile)
{
    EeAreaEffectRunning* theState = ee_area_effect_running_new();
    for (unsigned long p = 1; p < ::gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = ::gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* u = theUnit->mComplexUnit;
            if (u == NULL || (u->mAreaEffects.mItems.size() == 0 && u->mAffectingAreaEffects.mItems.size() == 0))
                continue;
            ee_area_effect_running_add_unit(theState, (unsigned long)u->GetUnitID());
            for (size_t i = 0; i < u->mAreaEffects.mItems.size(); i++) {
                EEAreaEffect* e = u->mAreaEffects.mItems[i];
                bool theIsMorale = e->mKind == kAreaEffectMoraleFromBuildings;
                EeAreaEffectSavedOwn theOwn = {e->mLocation.mX, e->mLocation.mY, theIsMorale, theIsMorale && e->mDirty,
                                               theIsMorale ? e->mCount : 0};
                ee_area_effect_running_add_own(theState, theOwn);
            }
            for (size_t i = 0; i < u->mAffectingAreaEffects.mItems.size(); i++)
                ee_area_effect_running_add_ref(theState, EEAERefOf(u->mAffectingAreaEffects.mItems[i]), false);
        }
    }
    for (size_t i = 0; i < gAEManager.mEffects.mItems.size(); i++)
        ee_area_effect_running_add_ref(theState, EEAERefOf(gAEManager.mEffects.mItems[i]), true);
    EeCalamityIoFfi theIo = {&outFile, &EEAEIoCall};
    ee_area_effect_running_save(theState, theIo);
    ee_area_effect_running_free(theState);
}

long EEAELoadRunningState(FSFile& inFile)
{
    EeCalamityIoFfi theIo = {&inFile, &EEAEIoCall};
    long theResult = 0;
    std::shared_ptr<EeAreaEffectRunning> theState(ee_area_effect_running_load(theIo, &theResult),
                                                  ee_area_effect_running_free);
    if (theResult == 0)
        EEAfterLoad([theState]() {
            const EeAreaEffectRunning* s = theState.get();
            unsigned long theLost = 0;
            size_t theUnits = ee_area_effect_running_count(s, false), theOwn = 0, theAffecting = 0;
            for (size_t u = 0; u < theUnits; u++) {
                EEComplexUnit* theUnit = EEAEComplexUnit(ee_area_effect_running_unit(s, u, &theOwn, &theAffecting));
                if (theUnit == NULL || theUnit->mAreaEffects.mItems.size() != theOwn) {
                    theLost++;
                    continue;
                }
                for (size_t i = 0; i < theOwn; i++) {
                    EeAreaEffectSavedOwn theSaved = ee_area_effect_running_own(s, u, i);
                    EEAreaEffect* theEffect = theUnit->mAreaEffects.mItems[i];
                    theEffect->mLocation.mX = theSaved.mX;
                    theEffect->mLocation.mY = theSaved.mY;
                    if (theEffect->mKind == kAreaEffectMoraleFromBuildings && theSaved.mIsMorale) {
                        theEffect->mDirty = theSaved.mDirty;
                        theEffect->mCount = theSaved.mCount;
                    }
                }
            }
            // the effects acting on each unit (every list is empty after a load), in their saved order
            for (size_t u = 0; u < theUnits; u++) {
                EEComplexUnit* theUnit = EEAEComplexUnit(ee_area_effect_running_unit(s, u, &theOwn, &theAffecting));
                if (theUnit == NULL)
                    continue;
                theUnit->mAffectingAreaEffects.Clear();
                for (size_t i = 0; i < theAffecting; i++) {
                    EEAreaEffect* theEffect = EEAEEffectOf(ee_area_effect_running_ref(s, u, i));
                    if (theEffect != NULL)
                        theUnit->mAffectingAreaEffects.Add(theEffect);
                    else
                        theLost++;
                }
            }
            gAEManager.ClearEffects();
            for (size_t i = 0; i < ee_area_effect_running_count(s, true); i++) {
                EEAreaEffect* theEffect = EEAEEffectOf(ee_area_effect_running_ref(s, SIZE_MAX, i));
                if (theEffect != NULL)
                    gAEManager.mEffects.Add(theEffect);
                else
                    theLost++;
            }
            if (theLost != 0)
                ESDebugOut("EEAEManager: %lu saved area effects are not the loaded ones\n", theLost);
        });
    return theResult;
}

// Inline slots and template COMDATs emitted in this unit's range (first-reference order)
// TEMPLATE: EXE 0x67b6b3
// TEMPLATE: EXE 0x67b6bf
// TEMPLATE: EXE 0x67b6cb
// TEMPLATE: EXE 0x67b6e0
// TEMPLATE: EXE 0x67b6f2
// TEMPLATE: EXE 0x67b751
// TEMPLATE: EXE 0x67b77d
