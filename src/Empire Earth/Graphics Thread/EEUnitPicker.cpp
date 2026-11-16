// C:\EEX\Empire Earth\Graphics Thread\EEUnitPicker.cpp (__FILE__ 0x4be904). 0x118aa2..0x11a1cc: EEROWater's members
// (vtable 0x441c10; no __FILE__ of its own, so they may be a unit of their own), the pick priority GetUnitPickPriority, then
// EEUnitPicker (vtable 0x441c48): ctor (assert line 340), ??_G, the entries' vector deleting dtor, dtor, the pickers,
// the frustum tests, the std template COMDATs (vector<EEUnitPickerHit>::push_back, fill) and GetFogFactor (the fog
// factor the effects use). gUnitPicker's init chain sits at 0x10ab0e (before EERenderObject.cpp's chains). Unit
// flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEQuaternion.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEPhysicalModel.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEROWater.h"
#include "ee/EEUnitPicker.h"
#include "ee/EERenderSnapshot.h"
#include "rust/ee_ge.h"   // ee_gfx pick.rs (RUST_PLAN R4.4)
#include "rust/ee_gfx.h"

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

// Draws the water unlit with fog, alpha-blended with z test only, then the effects' pass 6
void EEROWater::RenderScene()
{
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    float theAlphaRefValue = gRasterizer->GetAlphaTestingRefValue(gScreenIndex);
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetAlphaTestingRefValue(0.0f, gScreenIndex);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    gRasterizer->SetCulling(kCullNone);
    SetLightingOff();
    SetSceneFog();
    gRasterizer->DrawWater(*gRenderSnapshot.mTerrainMesh, gScreenIndex, gOrthographicView);   // port: the snapshot's
    for (theIterator = gEffectQueues[4].begin(); theIterator != gEffectQueues[4].end(); theIterator++) {
        BeginEffectBatches(theIterator->first);
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass6();
        FlushEffectBatches();
        EndEffectBatches();
    }
    gRasterizer->SetAlphaTestingRefValue(theAlphaRefValue, gScreenIndex);
}

void EEROWater::FrameDone()
{
}

// Returns a unit's click priority: 1 land building or wall, 4 own or allied unit, 6 enemy unit or AA
// building, 2 wood, 3 stone/gold/other resource, 5 any resource when preferring resources (provisional name)
// GetUnitPickPriority's questions (pick.rs EeGfxPickQuestion), asked where the original asked them
static bool EEPickAsk(void* inUnit, uint32_t inQuestion)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    if (inQuestion == 0)
        return theUnit->mUnitClass->IsLandBuilding();
    return gWorld.GetCurrentPlayer()->IsAlly(theUnit->GetPlayer());
}

long GetUnitPickPriority(EEUnit* inUnit, bool inPreferResources)
{
    EeGfxPickUnit theFacts;
    theFacts.complex = inUnit->GetComplexUnit() != NULL;
    theFacts.gaia = inUnit->mPlayer == gWorld.mPlayers[0];
    theFacts.wall = inUnit->mUnitClass->mIsWall;
    theFacts.hit_points = (int32_t)inUnit->mHitPoints;
    theFacts.family = (uint32_t)inUnit->mUnitClass->mFamily;
    theFacts.category = (uint32_t)inUnit->mUnitClass->mCategory;
    theFacts.resource_type = (uint32_t)inUnit->mResourceType;
    theFacts.resource_amount = (int32_t)inUnit->mResourceAmount;
    return ee_gfx_pick_priority(&theFacts, inPreferResources, inUnit, EEPickAsk);
}

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

// Returns whether the picker holds a unit
bool EEUnitPicker::Contains(EEUnit* inUnit)
{
    bool theFound = false;
    if (this->mCount > 0) {
        for (long i = this->mCount - 1; i >= 0 && !theFound; i--)
            theFound = this->mEntries[i].mUnitID == inUnit->GetUnitID();   // port: ids (R5a.3)
    }
    return theFound;
}

// port (R5a.3): an entry's unit by its id; NULL once the unit has left the world (the original's reference kept it)
EEUnit* EEUnitPicker::GetEntryUnit(unsigned long inIndex)
{
    return static_cast<EEUnit*>(gWorld.GetUnit(this->mEntries[inIndex].mUnitID));
}

// Returns the unit under a screen point (its carrier for a part), referenced: polygon hits beat
// bounding hits, then the nearest, with click priority deciding near ties; or null
EEUnit* EEUnitPicker::PickUnit(const GE2DPoint& inPoint, long inPreferResources)
{
    this->mLock->Enter();
    EEUnit* theResult = this->FindUnit(inPoint, inPreferResources);
    if (theResult != NULL)
        theResult->Reference();
    this->mLock->Leave();
    return theResult;
}

// port (R5a.3): PickUnit with no reference taken (FillRenderLists, under the world's lock)
EEUnit* EEUnitPicker::PickUnitUnreferenced(const GE2DPoint& inPoint, long inPreferResources)
{
    this->mLock->Enter();
    EEUnit* theResult = this->FindUnit(inPoint, inPreferResources);
    this->mLock->Leave();
    return theResult;
}

// The picker's walks (ee_gfx pick_walk.rs: FindUnit, the box selection and its frustum test over the entries in
// place and GEModel.cpp's model table) ask these (EeGfxPickHost; the context: the picker)
const EeGePhysicalModels* GEPhysicalModelTable();
static_assert(sizeof(EEUnitPickerEntry) == sizeof(EeGfxPickEntry) && offsetof(EEUnitPickerEntry, mUnitID) == offsetof(EeGfxPickEntry, unit_id),
              "EEUnitPickerEntry");
struct EEPickOps {
    static EEUnitPicker* P(void* inPicker) { return static_cast<EEUnitPicker*>(inPicker); }
    static void* Unit(void* inPicker, uint32_t inIndex) { return P(inPicker)->GetEntryUnit(inIndex); }
    static EeGfxPickFindUnit FindFacts(void*, void* inUnit)
    {
        EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
        EeGfxPickFindUnit theFacts;
        theFacts.facts.complex = theUnit->GetComplexUnit() != NULL;
        theFacts.facts.gaia = theUnit->mPlayer == gWorld.mPlayers[0];
        theFacts.facts.wall = theUnit->mUnitClass->mIsWall;
        theFacts.facts.hit_points = (int32_t)theUnit->mHitPoints;
        theFacts.facts.family = (uint32_t)theUnit->mUnitClass->mFamily;
        theFacts.facts.category = (uint32_t)theUnit->mUnitClass->mCategory;
        theFacts.facts.resource_type = (uint32_t)theUnit->mResourceType;
        theFacts.facts.resource_amount = (int32_t)theUnit->mResourceAmount;
        theFacts.tile_x = (int32_t)theUnit->GetTileX();
        return theFacts;
    }
    static bool Ask(void*, void* inUnit, uint32_t inQuestion) { return EEPickAsk(inUnit, inQuestion); }
    static bool GroupNonEmpty(void*) { return gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() != 0; }
    static float PixelSize(void* inPicker, uint32_t inIndex)
    {
        return gRasterizer->GetPixelSize(P(inPicker)->mEntries[inIndex], P(inPicker)->mViewport, P(inPicker)->mOrthographic);
    }
    // a part's outermost carrier
    static EEUnit* Outer(EEUnit* inUnit)
    {
        EEComplexUnit* theCarrier = inUnit->GetComplexUnit();
        if (theCarrier == NULL)
            return inUnit;
        while (theCarrier->mPreviousPart != NULL)
            theCarrier = theCarrier->mPreviousPart;
        return theCarrier;
    }
    static void* Carrier(void*, void* inUnit) { return Outer(static_cast<EEUnit*>(inUnit)); }
    static EeGfxPickBoxUnit BoxFacts(void*, void* inUnit)
    {
        EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
        EEUnitClass* theClass = theUnit->mUnitClass;
        EEUnit* theOuter = Outer(theUnit);
        return EeGfxPickBoxUnit{theUnit->GetComplexUnit() != NULL, theClass->IsPickable(), theUnit->mHitPoints > 0,
                                (uint32_t)theClass->mCategory, (uint32_t)theClass->mUnitType, theUnit->mPlayer,
                                (uint32_t)theOuter->mUnitID, theOuter->mUnitClass->IsLandBuilding()};
    }
    static void Push(void*, void* outIDs, uint32_t inID) { static_cast<std::vector<unsigned long>*>(outIDs)->push_back(inID); }
};
static const EeGfxPickHost kPickHost = {NULL, EEPickOps::Unit, EEPickOps::FindFacts, EEPickOps::Ask,
                                        EEPickOps::GroupNonEmpty, EEPickOps::PixelSize, EEPickOps::Carrier,
                                        EEPickOps::BoxFacts, EEPickOps::Push};

// PickUnit's search, under mLock (port: split from PickUnit, R5a.3; ee_gfx pick_walk.rs)
EEUnit* EEUnitPicker::FindUnit(const GE2DPoint& inPoint, long inPreferResources)
{
    EeGfxPickHost theHost = kPickHost;
    theHost.context = this;
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
    EeGfxPickHost theHost = kPickHost;
    theHost.context = this;
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

// Empties the picker (port, R5a.3: ids, nothing to release; the original dereferenced every
// unit and carrier here)
void EEUnitPicker::Clear()
{
    for (unsigned long i = 0; i < this->mCount; i++)
        this->mEntries[i].SetModel(NULL);
    this->mCount = 0;
}

// Empties the picker and takes the frame's viewport, projection and see-through mode
void EEUnitPicker::Reset()
{
    this->Clear();
    this->mViewport = gU4E8F08;
    this->mOrthographic = gOrthographicView;
    this->mShowHiddenUnits = gHiddenUnitsMode;
}

// (ReferenceCarriers 0x519faa: gone with the references, R5a.3)

// Adds a unit at a transformation when there is room and it is big enough on screen (no path points;
// no ambient units or projectiles but ICBMs unless the camera is free)
void EEUnitPicker::AddUnit(EEUnit* inUnit, const GETransformation& inTransformation, float inPixelSize)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    EEComplexUnit* theComplexUnit = inUnit->GetComplexUnit();
    EEUnitPickerEntry& theEntry = this->mEntries[this->mCount];
    EeGfxPickAdd theAdd;
    theAdd.count = (uint32_t)this->mCount;
    theAdd.capacity = (uint32_t)this->mCapacity;
    theAdd.pixel_size = inPixelSize;
    theAdd.unit_type = (uint32_t)theClass->mUnitType;
    theAdd.category = (uint32_t)theClass->mCategory;
    theAdd.projectile = theClass->mIsProjectile;
    theAdd.free_camera = gFreeCamera;
    if (!ee_gfx_pick_admits(&theAdd))
        return;
    theEntry.mUnitID = inUnit->GetUnitID();   // port: the id, no reference (R5a.3)
    if (theComplexUnit != NULL) {
        (GEPhysicalModel&)theEntry = theComplexUnit->mPhysicalModel;
        theEntry.SetLocation(inTransformation);
    } else {
        EEUnitClassUpgrade* theStats = inUnit->mStats;
        theEntry.SetFrame(0);
        theEntry.SetModel(inUnit->GetModel()->mModels[0]);
        float theScale = theStats->mScale;
        theEntry.SetLocation(inTransformation);
        theEntry.SetScale(theScale);
    }
    this->mCount++;
}

// Computes the fog visibility of a point into outValue (linear fog only, which it also turns off; else 1)
void GetFogFactor(const GE3DPoint& inLocation, unsigned long inFog, float& outValue)
{
    GERFogAttributes theAttributes;
    unsigned long theScreen = gScreenIndex;
    if (inFog == 1) {
        gRasterizer->GetFogAttributes(theAttributes, theScreen);
        gRasterizer->SetFog(0, theScreen);
        float theDistance = inLocation.GetY() * gU4E8F08.GetCameraInverse().GetElement(1, 1) + inLocation.GetZ() * gU4E8F08.GetCameraInverse().GetElement(1, 2) +
                            inLocation.GetX() * gU4E8F08.GetCameraInverse().GetElement(1, 0) + gU4E8F08.GetCameraInverse().GetElement(1, 3);
        if (theDistance <= theAttributes.mStart)
            outValue = 1.0f;
        else if (theDistance <= theAttributes.mEnd)
            outValue = 1.0f - (theDistance - theAttributes.mStart) * theAttributes.mScale;
        else
            outValue = 0.0f;
    } else
        outValue = 1.0f;
}
