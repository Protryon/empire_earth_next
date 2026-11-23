// C:\EEX\Empire Earth\Graphics Thread\EEUnitPicker.cpp (__FILE__ 0x4be904). 0x118aa2..0x11a1cc: EEROWater's members
// (vtable 0x441c10; no __FILE__ of its own, so they may be a unit of their own), the pick priority GetUnitPickPriority, then
// EEUnitPicker (vtable 0x441c48): ctor (assert line 340), ??_G, the entries' vector deleting dtor, dtor, the pickers,
// the frustum tests, the std template COMDATs (vector<EEUnitPickerHit>::push_back, fill) and GetFogFactor (the fog
// factor the effects use). gUnitPicker's init chain sits at 0x10ab0e (before EERenderObject.cpp's chains). Unit
// flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEROWater.h"
#include "ee/EEUnitPicker.h"
#include "ee/EERenderSnapshot.h"
#include "rust/base/math.h"   // ee_gfx pick.rs (RUST_PLAN R4.4)
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/game/gamecpp.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"

// The picker's decisions are ee_gfx pick.rs (RUST_PLAN R4.4): the ray, the tolerances, the pick scales and the large
// test, the hits' ranking, GetUnitPickPriority, AddUnit's admission, the frustum's sphere and vertex tests (its named
// consts, .rdata 0x441c28.., are there); its walks pick_walk.rs's (R4.4, twenty-sixth round: FindUnit, PickUnits with
// PickUnitsOfType, PickUnitsInRectangle, GetIsEntryInView and GetIsModelInView, over the entries in place). The C++
// keeps the entries' filling, the lock, the frustum's planes and the world; the GE value classes pass as their
// #[repr(C)] mirrors.
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint) &&
              sizeof(GE3DPlane) == sizeof(EeGe3DPlane) && sizeof(GEViewport) == sizeof(EeGeViewport), "GE mirrors");

// Creates the water's render object
EEROWater::EEROWater()
{
}

// Destroys the water's render object
EEROWater::~EEROWater()
{
}

// Advances the terrain's water animation
void EEROWater::FrameStarting()
{
    gWorld.mMap->mTerrainMesh.UpdateWaterAnimation();
}

// Virtual04 and Virtual0C: one empty body, ICF-folded into 0xe123d (no address of their own here)
void EEROWater::SceneStarting()
{
}

// Draws the water unlit with fog, alpha-blended with z test only, then the effects' pass 6 (gamecpp ro_views.rs)
void EEROWater::RenderScene()
{
    ee_gamecpp_wa_render_scene();
}

void EEROWater::FrameDone()
{
}

// (GetUnitPickPriority 0x118xxx: its only caller was the picker's walk; ee_gfx pick.rs priority answers it there)

// Creates a unit picker with room for the given number of units and its lock
EEUnitPicker::EEUnitPicker(unsigned long inCapacity)
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLock) == keseOK, 340);
    this->mCapacity = inCapacity;
    this->mEntries = new EEUnitPickerEntry[inCapacity];
    this->mCount = 0;
    this->mOrthographic = false;
    this->mShowHiddenUnits = 0;
}

// Destroys the picker, releasing its units and entries and its lock
EEUnitPicker::~EEUnitPicker()
{
    this->Clear();
    if (this->mEntries)
        delete[] this->mEntries;
    TSScheduler::DestroyCriticalSection(this->mLock);
}

// The picker's bodies: gamecpp picker.rs (EEPickOps::Op's cases below)
bool EEUnitPicker::Contains(EEUnit* inUnit) { return ee_gamecpp_picker_contains(this, inUnit); }
EEUnit* EEUnitPicker::PickUnit(const GE2DPoint& inPoint, long inPreferResources) { return (EEUnit*)ee_gamecpp_picker_pick(this, &inPoint.mX, inPreferResources, true); }
// port (R5a.3): PickUnit with no reference taken (FillRenderLists, under the world's lock)
EEUnit* EEUnitPicker::PickUnitUnreferenced(const GE2DPoint& inPoint, long inPreferResources) { return (EEUnit*)ee_gamecpp_picker_pick(this, &inPoint.mX, inPreferResources, false); }
void EEUnitPicker::Clear() { ee_gamecpp_picker_clear(this); }
void EEUnitPicker::Reset() { ee_gamecpp_picker_reset(this); }
void EEUnitPicker::AddUnit(EEUnit* inUnit, const GETransformation& inTransformation, float inPixelSize) { ee_gamecpp_picker_add_unit(this, inUnit, reinterpret_cast<const float*>(&inTransformation), inPixelSize); }

// port (R5a.3): an entry's unit by its id; NULL once the unit has left the world (the original's reference kept it)
EEUnit* EEUnitPicker::GetEntryUnit(unsigned long inIndex)
{
    return static_cast<EEUnit*>(gWorld.GetUnit(this->mEntries[inIndex].mUnitID));
}

// The picker's walks (ee_gfx pick_walk.rs: FindUnit, the box selection and its frustum test over the entries in
// place and GEModel.cpp's model table) ask these (EeGfxPickHost; the context: the picker)
const EeGePhysicalModels* GEPhysicalModelTable();
static_assert(sizeof(EEUnitPickerEntry) == sizeof(EeGfxPickEntry) && offsetof(EEUnitPickerEntry, mUnitID) == offsetof(EeGfxPickEntry, unit_id),
              "EEUnitPickerEntry");
struct EEPickOps {
    static int64_t Op(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
    static double OpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
};

// PickUnit's search, under mLock (port: split from PickUnit, R5a.3; ee_gfx pick_walk.rs)
EEUnit* EEUnitPicker::FindUnit(const GE2DPoint& inPoint, long inPreferResources)
{
    EeGfxPickHost theHost;
    ee_gamecpp_picker_host(this, &theHost);
    EeGfxPickView theView = {*reinterpret_cast<const EeGeViewport*>(&this->mViewport), this->mOrthographic,
                             this->mShowHiddenUnits != 0, gFreeCamera, (uint32_t)gGameWindowWidth};
    return static_cast<EEUnit*>(ee_gfx_pick_find_unit(&theHost, GEPhysicalModelTable(),
                                                      reinterpret_cast<EeGfxPickEntry*>(this->mEntries),
                                                      (uint32_t)this->mCount, &theView,
                                                      reinterpret_cast<const EeGe2DPoint*>(&inPoint),
                                                      inPreferResources != 0));
}

// The box selections' walk (ee_gfx pick_walk.rs) under mLock: the frustum of inRectangle's planes, then the entries
void EEUnitPicker::PickBox(const GERectangle& inRectangle, bool inAll, EEPlayer* inPlayer, long inType,
                           std::vector<unsigned long>& outUnitIDs)
{
    std::vector<GE3DPlane> thePlanes;
    EeGfxPickHost theHost;
    ee_gamecpp_picker_host(this, &theHost);
    this->mLock->Enter();
    outUnitIDs.clear();
    if (this->mCount > 0)
        this->mViewport.GetFrustrumWorldPlanes(inRectangle, thePlanes, gOrthographicView);
    ee_gfx_pick_box(&theHost, GEPhysicalModelTable(), reinterpret_cast<EeGfxPickEntry*>(this->mEntries),
                    (uint32_t)this->mCount, this->mCount > 0 ? reinterpret_cast<const EeGe3DPlane*>(thePlanes.data()) : NULL,
                    inAll, inPlayer, (uint32_t)inType, &outUnitIDs);
    this->mLock->Leave();
}

// Returns the ids of the pickable units of a type (citizens of either sex for the citizen types) inside
// a screen rectangle, a part's carrier instead
void EEUnitPicker::PickUnitsInRectangle(const GERectangle& inRectangle, long inType,
                                        std::vector<unsigned long>& outUnitIDs)
{
    this->PickBox(inRectangle, true, NULL, inType, outUnitIDs);
}

// Returns the ids of a player's units of a type inside a screen rectangle (male and female citizens for
// either citizen type)
void EEUnitPicker::PickUnits(const GERectangle& inRectangle, EEPlayer* inPlayer, long inType,
                             std::vector<unsigned long>& outUnitIDs)
{
    this->PickBox(inRectangle, false, inPlayer, inType, outUnitIDs);
}

// (ReferenceCarriers 0x519faa: gone with the references, R5a.3)

// (GetFogFactor: gamecpp picker.rs fog_factor, the raster table's fog_factor)

// ------------------------------------------------------------------------- round 9, lane A: the picker's C++

int64_t EECore9OpB(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
int64_t EEPickerOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f) { return EEPickOps::Op(inOp, p, a, f); }
double EEPickerOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f) { return EEPickOps::OpF(inOp, p, a, f); }

#define PK_P64(x) ((int64_t)(intptr_t)(x))
#define PK ((EEUnitPicker*)p)
#define PK_UN ((EEUnit*)p)
#define PK_ENTRY ((EEUnitPickerEntry*)p)

// the C++ calls and members of the picker's bodies in Rust (gamecpp picker.rs): one statement each
int64_t EEPickOps::Op(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PkCount: return PK_P64(&PK->mCount);
    case O::PkCapacity: return PK->mCapacity;
    case O::PkEntry: return PK_P64(&PK->mEntries[a[0]]);
    case O::PkEntryId: return PK->mEntries[a[0]].mUnitID;
    case O::PkEntryUnit: return PK_P64(PK->GetEntryUnit((unsigned long)a[0]));
    case O::PkEntryReleaseModel: PK->mEntries[a[0]].SetModel(NULL);
        break;
    case O::PkTakeView: PK->mViewport = gU4E8F08, PK->mOrthographic = gOrthographicView, PK->mShowHiddenUnits = gHiddenUnitsMode;
        break;
    case O::PkLock: PK->mLock->Enter();
        break;
    case O::PkUnlock: PK->mLock->Leave();
        break;
    case O::PkFindUnit: return PK_P64(PK->FindUnit(*(const GE2DPoint*)a[0], (long)a[1]));
    case O::PkEntrySetId: PK_ENTRY->mUnitID = (unsigned long)a[0];
        break;
    case O::PkEntryAssignPhysical: (GEPhysicalModel&)*PK_ENTRY = ((EEComplexUnit*)a[0])->mPhysicalModel;
        break;
    case O::PkEntrySetLocation: PK_ENTRY->SetLocation(*(const GETransformation*)a[0]);
        break;
    case O::PkEntrySetFrame0: PK_ENTRY->SetFrame(0);
        break;
    case O::PkEntrySetModel: PK_ENTRY->SetModel((GEModel*)a[0]);
        break;
    case O::PkEntrySetScale: PK_ENTRY->SetScale(f[0]);
        break;
    case O::PkUnitReference: PK_UN->Reference();
        break;
    case O::PkClassIsWall: return PK_UN->mUnitClass->mIsWall;
    case O::PkClassFamily: return PK_UN->mUnitClass->mFamily;
    case O::PkClassCategory: return PK_UN->mUnitClass->mCategory;
    case O::PkClassUnitType: return PK_UN->mUnitClass->mUnitType;
    case O::PkClassPickable: return PK_UN->mUnitClass->IsPickable();
    case O::PkClassProjectile: return PK_UN->mUnitClass->mIsProjectile;
    case O::PkUnitResourceType: return PK_UN->mResourceType;
    case O::PkUnitTileX: return PK_UN->GetTileX();
    case O::PkUnitIdField: return PK_UN->mUnitID;
    case O::PkCurrentPlayerAllied: return gWorld.GetCurrentPlayer()->IsAlly(PK_UN->GetPlayer());
    case O::PkGroupNonEmpty: return gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() != 0;
    case O::PkPushId: ((std::vector<unsigned long>*)p)->push_back((unsigned long)a[0]);
        break;
    case O::PkFogAttributes: gRasterizer->GetFogAttributes(*(GERFogAttributes*)a[0], gScreenIndex);
        break;
    case O::PkRastFogOff: gRasterizer->SetFog(0, gScreenIndex);
        break;
    case O::PkMainCameraInverse: return PK_P64(&gU4E8F08.GetCameraInverse());
    default: return EECore9OpB(inOp, p, a, f);
    }
    return 0;
}

double EEPickOps::OpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PkPixelSize: return gRasterizer->GetPixelSize(PK->mEntries[a[0]], PK->mViewport, PK->mOrthographic);
    case O::PkStatsScale: return PK_UN->mStats->mScale;
    default: return EECore9OpBF(inOp, p, a, f);
    }
}
