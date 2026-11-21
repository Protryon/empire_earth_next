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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/EESaveGameFiles.h"
#include "rust/game/gamecpp.h"
#include <memory>
#include <stdint.h>

// port (round 10, lane C): the unit functors of the manager's passes (ApplyGeneratedEffectsToOthersFunctor,
// SetAffectedByAreaEffectFunctor, ClearAreaEffectsFunctor, RebuildAreaEffectRegionListFunctor,
// RemoveRedundantAreaEffectsFunctor, UpdateGeneratedAreaEffectsFunctor, UpdateAffectedByAreaEffectsFunctor; vtables
// 0x44bb50.., 0x44bd28..) are gamecpp ae_effect.rs's walks over every player's units

// The living units (expanded into the collector 0x27b77d) (provisional name)
struct EEAEAliveUnit {
    bool operator()(EEComplexUnit* inUnit) const { return inUnit->mHitPoints > 0; }
};

// Creates the area-effect manager with no templates or effects
EEAEManager::EEAEManager()
{
}

// Destroys the manager, releasing every template and effect
EEAEManager::~EEAEManager()
{
    this->Clear();
}

// Adds the list's effects to every living unit they cover around a location (a radius-0 effect: to every
// unit of every player); a healing effect skips its own unit and shows on the units it covers
void EEAEManager::CollectEffects(float inRadius, GE2DPoint& inLocation, unsigned long inPlayerID,
                           EEUnitAreaEffectList* inList, EEComplexUnit* inUnit)
{
    ee_gamecpp_ae_collect_effects(this, inRadius, &inLocation, inPlayerID, inList, inUnit);
}

// Empties the list of active area effects
void EEAEManager::ClearEffects()
{
    ee_gamecpp_ae_clear_effects(this);
}

// Reads the area-effect templates from the area-effect file (an empty slot for records with id 0); returns 3
// when a template fails to initialize
long EEAEManager::Load(FSFile* inFile)
{
    return ee_gamecpp_ae_load(this, inFile);
}

// Creates the area effect of the template at an index (its kind's: healing, morale, calamity and conversion immunity,
// morale from buildings, from military heroes, economy from units, cloaking) for a unit; NULL for an unknown kind
EEAreaEffect* EEAEManager::CreateAreaEffect(unsigned long inIndex, EEComplexUnit* inUnit)
{
    return (EEAreaEffect*)ee_gamecpp_ae_create(this, inIndex, inUnit);
}

// Finds an active effect of a kind covering a point that affects the player (immunities only, by the
// reversed player test, in immunity mode), skipping protected units' effects and those in the exclusion list
EEAreaEffect* EEAEManager::FindEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                                    EEUnitAreaEffectList* inExclude)
{
    return (EEAreaEffect*)ee_gamecpp_ae_find(this, inKind, inX, inY, inPlayer, inMode, inExclude);
}

// Tells whether a point lies in the effect's scaled area around its location
bool EEAreaEffect::IsInRange(const GE2DPoint& inPoint)
{
    return this->mTemplate->IsWithin(inPoint, this->mLocation, this->GetScale(), NULL);
}

// Tells whether any active effect of the kind covers the point for the player
bool EEAEManager::HasEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                           EEUnitAreaEffectList* inList)
{
    return this->FindEffect(inKind, inX, inY, inPlayer, inMode, inList) != NULL;
}

// Tells whether an effect of the kind covers a tile for the player
bool EEAEManager::EffectAtTile(long inKind, long inX, long inY, long inPlayer, bool inMode)
{
    return this->HasEffect(inKind, (float)inX, (float)inY, inPlayer, inMode, NULL);
}

// Tells whether an effect of the kind covers a tile and affects the player
bool EEAEManager::HasAreaEffect(long inKind, long inX, long inY, long inPlayer)
{
    return this->EffectAtTile(inKind, inX, inY, inPlayer, false);
}

// Tells whether an immunity effect of the kind covers a tile against the player
bool EEAEManager::HasAreaEffect2(long inKind, long inX, long inY, long inPlayer)
{
    return this->EffectAtTile(inKind, inX, inY, inPlayer, true);
}

// Returns the effect of the kind that covers a tile and affects the player, or NULL
EEAreaEffect* EEAEManager::FindEffectAtTile(long inKind, long inX, long inY, unsigned long inPlayer)
{
    return this->FindEffect(inKind, (float)inX, (float)inY, inPlayer, false, NULL);
}

// Loads the media of the template at an index, if any
void EEAEManager::LoadTemplateMedia(unsigned long inIndex)
{
    ee_gamecpp_ae_template_media(this, inIndex, true);
}

// Releases every template and empties the active effect list
void EEAEManager::Clear()
{
    ee_gamecpp_ae_clear(this);
}

// Unloads the media of the template at an index, if any
void EEAEManager::UnloadTemplateMedia(unsigned long inIndex)
{
    ee_gamecpp_ae_template_media(this, inIndex, false);
}

// Runs one area-effect tick: clear, apply, keep the strongest, update generators, rebuild the list, tick effects
// (each a pass over every player's units)
void EEAEManager::Tick(float inElapsed)
{
    ee_gamecpp_aem_tick(this, inElapsed);   // and, TEST ONLY, AEFX (ee/EEAECapture.h)
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

}

void EEAESaveRunningState(FSFile& outFile)
{
    ee_gamecpp_ae_save_running(&outFile);
}

long EEAELoadRunningState(FSFile& inFile)
{
    return ee_gamecpp_ae_load_running(&inFile);
}

// ------------------------------------------------------------------------- round 9, lane D: the area effects' C++

int64_t EECore9OpD5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEAreaEffect.cpp

static_assert(kAreaEffectHealing == EE_AREA_EFFECT_HEALING && kAreaEffectMoraleFromBuildings == EE_AREA_EFFECT_MORALE_FROM_BUILDINGS,
              "gamecpp civ.rs");
static_assert(sizeof(EEAreaEffectData) == sizeof(EeAreaEffectData), "the area effect record");
static_assert(sizeof(U2DRectangle<long>) == 4 * sizeof(long) && offsetof(U2DRectangle<long>, mBottom) == 0
              && offsetof(U2DRectangle<long>, mTop) == 3 * sizeof(long), "gamecpp civ.rs AeUnitsInRect");
static_assert(sizeof(GE2DPoint) == 2 * sizeof(float), "gamecpp civ.rs: a location's two floats");

#define P64(x) ((int64_t)(intptr_t)(x))
#define AE ((EEAreaEffect*)p)
#define CX ((EEComplexUnit*)p)
#define MGR ((EEAEManager*)p)
#define LIST ((UReferenceContainer<EEAreaEffect*>*)p)
#define UVEC (*(std::vector<EEComplexUnit*>*)p)
#define AFILE ((FSFile*)p)

// the C++ calls and members of the area effects' bodies in Rust (gamecpp civ.rs): one statement each
int64_t EECore9OpD4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the effects (p an EEAreaEffect*), their templates and the units' lists (p a container)
    case O::AeFields: return P64(static_cast<EeAreaEffectFields*>(AE));
    case O::AeTemplateFields: return P64(static_cast<EeAreaEffectTemplateFields*>(AE->mTemplate));
    case O::AeTplFields: return P64(static_cast<EeAreaEffectTemplateFields*>((EEAreaEffectTemplate*)p));
    case O::AeOwner: return P64(AE->mUnit);
    case O::AeLocation: return P64(&AE->mLocation);
    case O::AeIsImmunity: return AE->IsImmunity();
    case O::AeAffectsPlayer: return AE->mTemplate->AffectsPlayer((long)a[0], (long)a[1]);
    case O::AeAffectsPlayerReversed: return AE->mTemplate->AffectsPlayerReversed((long)a[0], (long)a[1]);
    case O::AeIsInRange: return AE->IsInRange(GE2DPoint(f[0], f[1]));
    case O::AeCovers: return AE->Covers((unsigned long)a[0], (EEComplexUnit*)a[1], *(GE2DPoint*)a[2]);
    case O::AeAppliedTo: AE->AppliedTo((EEComplexUnit*)a[0]);
        break;
    case O::AeShowOn: AE->ShowOn((EEComplexUnit*)a[0]);
        break;
    case O::AeSetUnit: AE->SetUnit((EEComplexUnit*)a[0]);
        break;
    case O::AeNew: return P64(new EEAreaEffect((EEAreaEffectTemplate*)a[0]));
    case O::AeListCount: return (int64_t)LIST->mItems.size();
    case O::AeListAt: return P64(LIST->mItems[a[0]]);
    case O::AeListAdd: LIST->Add((EEAreaEffect*)a[0]);
        break;
    case O::AeListDereference: LIST->Clear();
        break;
    case O::AeListEmpty: LIST->mItems.clear();
        break;
    // the units (p an EEComplexUnit*, an EEUnit* for the Unit ops)
    case O::AeUnitProtected: return CX->IsProtected();
    case O::AeUnitBuilt: return CX->mIsBuilt;
    case O::AeUnitOwnList: return P64(static_cast<UReferenceContainer<EEAreaEffect*>*>(&CX->mAreaEffects));
    case O::AeUnitAffectingList: return P64(static_cast<UReferenceContainer<EEAreaEffect*>*>(&CX->mAffectingAreaEffects));
    case O::AeUnitAffectingAdd: CX->mAffectingAreaEffects.Add((EEAreaEffect*)a[0]);
        break;
    case O::AeUnitPlayerId: return (int64_t)CX->mPlayer->mPlayerID;
    case O::AeUnitId: return (int64_t)CX->GetUnitID();
    case O::AeUnitComplex: return P64(((EEUnit*)p)->mComplexUnit);
    case O::AeUnitNext: return P64(static_cast<EEUnit*>(((EEUnit*)p)->mNext));
    case O::AeWorldUnit: return P64(::gWorld.GetUnit((unsigned long)a[0]));
    case O::AeWorldNumPlayers: return (int64_t)::gWorld.mNumPlayers;
    case O::AeWorldPlayer: return P64(::gWorld.mPlayers[a[0]]);
    case O::AePlFirstUnit: return P64(((EEPlayer*)p)->mFirstUnit);
    // the unit searches (p a vector the Rust made)
    case O::AeNewUnitVec: return P64(new std::vector<EEComplexUnit*>);
    case O::AeDeleteUnitVec: delete &UVEC;
        break;
    case O::AeUnitsInRect: EEGetUnitsInRectangle(*(U2DRectangle<long>*)a[0], UVEC, EEAEAliveUnit(), true);
        break;
    case O::AeVecCount: return (int64_t)UVEC.size();
    case O::AeVecComplexAt: return P64(UVEC[a[0]]->mComplexUnit);
    // the manager (p the EEAEManager)
    case O::AeManager: return P64(&gAEManager);
    case O::AeManagerEffects: return P64(&MGR->mEffects);
    case O::AeTemplateCount: return (int64_t)MGR->mTemplates.size();
    case O::AeTemplateAt: return P64(MGR->mTemplates[a[0]]);
    case O::AeTemplatesClear: MGR->mTemplates.clear();
        break;
    case O::AeTemplatePush: MGR->mTemplates.push_back((EEAreaEffectTemplate*)a[0]);
        break;
    case O::AeTemplateDereference: ((EEAreaEffectTemplate*)a[0])->Dereference();
        break;
    case O::AeTemplateLoad: ((EEAreaEffectTemplate*)a[0])->Load(false);
        break;
    case O::AeTemplateUnload: ((EEAreaEffectTemplate*)a[0])->Unload();
        break;
    case O::AeNewTemplate: return P64(new EEAreaEffectTemplate);
    case O::AeTemplateInit: return ((EEAreaEffectTemplate*)a[0])->Init(*(const EEAreaEffectData*)a[1]);
    case O::AeTemplateError: ESDebugOut("Error reading area effect template %s...fix me in EEAEManager.cpp!\n", ((EEAreaEffectData*)a[0])->mName);
        break;
    // the area effects file (p an FSFile*) and the running state's
    case O::AeFileOpenRead: AFILE->Open(kFileAccessRead);
        break;
    case O::AeFileReadULong: return AFILE->Read(*(unsigned long*)a[0]);
    case O::AeFileReadData: return AFILE->Read((unsigned char*)a[0], sizeof(EEAreaEffectData), *(unsigned long*)a[1]);
    case O::AeFileClose: AFILE->Close();
        break;
    case O::AeIoCall: return P64(&EEAEIoCall);
    case O::AeAfterLoad: EEAfterLoad([theState = std::shared_ptr<EeAreaEffectRunning>((EeAreaEffectRunning*)a[0], ee_area_effect_running_free)]() { ee_gamecpp_ae_restore_running(theState.get()); });
        break;
    case O::AeLostRefs: ESDebugOut("EEAEManager: %lu saved area effects are not the loaded ones\n", (unsigned long)a[0]);
        break;
    default: return EECore9OpD5(inOp, p, a, f);
    }
    return 0;
}
