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

// The picker's decisions are ee_gfx pick.rs (RUST_PLAN R4.4): the ray, the tolerances, the pick scales and the large
// test, the hits' ranking, GetUnitPickPriority, AddUnit's admission, the frustum's sphere and vertex tests (its named
// consts, .rdata 0x441c28.., are there). The C++ keeps the entries, the model tests, the lock and the world; the GE
// value classes and the hits pass as their #[repr(C)] mirrors.
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint) &&
              sizeof(GE3DPlane) == sizeof(EeGe3DPlane) && sizeof(GEViewport) == sizeof(EeGeViewport), "GE mirrors");
static_assert(sizeof(EEUnitPickerHit) == sizeof(EeGfxPickHit) && offsetof(EEUnitPickerHit, mIndex) == offsetof(EeGfxPickHit, index) &&
              offsetof(EEUnitPickerHit, mExact) == offsetof(EeGfxPickHit, exact) &&
              offsetof(EEUnitPickerHit, mLarge) == offsetof(EeGfxPickHit, large), "EEUnitPickerHit");

// FUNCTION: EXE 0x518aa2
// Creates the water's render object
EEROWater::EEROWater()
{
}

// SYNTHETIC: EXE 0x518ab4
// (the scalar deleting destructor, emitted with the vtable; slot EEROWater+0x10)

// FUNCTION: EXE 0x518ad0
// Destroys the water's render object
EEROWater::~EEROWater()
{
}

// FUNCTION: EXE 0x518adb
// Advances the terrain's water animation
void EEROWater::FrameStarting()
{
    gWorld.mMap->mTerrainMesh.UpdateWaterAnimation();
}

// Virtual04 and Virtual0C: one empty body, ICF-folded into 0xe123d (no address of their own here)
void EEROWater::SceneStarting()
{
}

// FUNCTION: EXE 0x518ae9
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

// FUNCTION: EXE 0x518c40
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

// FUNCTION: EXE 0x518cf8
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

// SYNTHETIC: EXE 0x518dbb
// (the scalar deleting destructor, emitted with the vtable; slot EEUnitPicker+0x0)

// SYNTHETIC: EXE 0x518dd7
// (EEUnitPickerEntry's vector deleting destructor, for the dtor's delete[])

// FUNCTION: EXE 0x518e35
// Destroys the picker, releasing its units and entries and its lock
EEUnitPicker::~EEUnitPicker()
{
    this->Clear();
    if (this->mEntries)
        delete[] this->mEntries;
    TSScheduler::DestroyCriticalSection(this->mLock);
}

// FUNCTION: EXE 0x518e74
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

static bool GetIsEntryInView(const EEUnitPickerEntry& inEntry, std::vector<GE3DPlane>& inPlanes);

// FUNCTION: EXE 0x518eaf
// Adds the ids of a player's living pickable units of a type inside a screen rectangle (a part's carrier
// instead); without a type, its units, or failing those one building
void EEUnitPicker::PickUnitsOfType(const GERectangle& inRectangle, EEPlayer* inPlayer, long inType,
                                   std::vector<unsigned long>& outUnitIDs)
{
    std::vector<unsigned long> theUnits;
    std::vector<unsigned long> theOthers;
    std::vector<GE3DPlane> thePlanes;
    this->mLock->Enter();
    if (this->mCount > 0) {
        this->mViewport.GetFrustrumWorldPlanes(inRectangle, thePlanes, gOrthographicView);
        for (long i = this->mCount - 1; i >= 0; i--) {
            EEUnit* theEntryUnit = this->GetEntryUnit(i);   // port: ids (R5a.3)
            if (theEntryUnit == NULL)
                continue;
            EEComplexUnit* theUnit = theEntryUnit->GetComplexUnit();
            if (theUnit == NULL || !theUnit->mUnitClass->IsPickable() || !(theUnit->mHitPoints > 0) ||
                theUnit->mUnitClass->mCategory == kUnitCategoryProjectile || theUnit->mUnitClass->mCategory == kUnitCategoryBomb ||
                (theUnit->mUnitClass->mCategory == kUnitCategoryMissile && theUnit->mUnitClass->mUnitType != kUnitTypeCpICBM) ||
                theUnit->mUnitClass->mCategory == kUnitCategoryTorpedo ||
                (inPlayer != NULL && theUnit->mPlayer != inPlayer) ||
                (inType != 0 && theUnit->mUnitClass->mUnitType != inType) ||
                !GetIsEntryInView(this->mEntries[i], thePlanes))
                continue;
            while (theUnit->mPreviousPart != NULL)
                theUnit = theUnit->mPreviousPart;
            unsigned long theID;
            if (!theUnit->mUnitClass->IsLandBuilding()) {
                theID = theUnit->mUnitID;
                theUnits.push_back(theID);
            } else {
                theID = theUnit->mUnitID;
                theOthers.push_back(theID);
            }
        }
    }
    this->mLock->Leave();
    if (inType == 0) {
        if (theUnits.size() != 0) {
            for (unsigned long i = 0; i < theUnits.size(); i++)
                outUnitIDs.push_back(theUnits[i]);
        } else if (theOthers.size() > 0) {
            outUnitIDs.push_back(theOthers[0]);
        }
    } else {
        unsigned long i;
        for (i = 0; i < theUnits.size(); i++)
            outUnitIDs.push_back(theUnits[i]);
        for (i = 0; i < theOthers.size(); i++)
            outUnitIDs.push_back(theOthers[i]);
    }
}

static bool GetIsModelInView(GEModel* inModel, const GETransformation& inTransformation, unsigned long inFrame,
                     unsigned long inLOD, std::vector<GE3DPlane>& inPlanes);

// FUNCTION: EXE 0x51911c
// Returns whether an entry's model lies inside the side planes 2..5 of a selection frustum (bounding sphere
// first, then the vertices)
static bool GetIsEntryInView(const EEUnitPickerEntry& inEntry, std::vector<GE3DPlane>& inPlanes)
{
    GE3DPoint theCenter;
    float theScale = inEntry.GetScale();
    GEModel* theModel = inEntry.GetModel();
    unsigned long theFrame = inEntry.GetFrame();
    bool theResult = true;
    GETransformation theTransformation(false);
    bool theBelow[4];
    const EeGe3DPlane* thePlanes = reinterpret_cast<const EeGe3DPlane*>(inPlanes.data());
    inEntry.GetLocation().Apply(theModel->GetInternalTransform(theFrame), theTransformation);
    if (theScale != 1.0f) {
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetPosition(inEntry.GetLocation().GetX(), inEntry.GetLocation().GetY(),
                                      inEntry.GetLocation().GetZ());
    }
    theTransformation.Apply(theModel->GetCenterPoint(), theCenter);
    const EeGe3DPoint* theRustCenter = reinterpret_cast<const EeGe3DPoint*>(&theCenter);
    if (ee_gfx_pick_sphere_below(theRustCenter, thePlanes, theBelow) != 0) {
        float theRadius = theModel->GetBoundingRadius(theFrame) * theScale;
        if (ee_gfx_pick_sphere_outside(theRustCenter, thePlanes, theBelow, theRadius) == 0) {
            unsigned long theLOD = ee_gfx_pick_view_lod((uint32_t)theModel->GetLODCount());
            theResult = GetIsModelInView(theModel, theTransformation, theFrame, theLOD, inPlanes);
        } else {
            theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5192f2
// Returns whether a vertex of a model (or of a sub model) is on or above every side plane 2..5
static bool GetIsModelInView(GEModel* inModel, const GETransformation& inTransformation, unsigned long inFrame,
                     unsigned long inLOD, std::vector<GE3DPlane>& inPlanes)
{
    GETransformation theTransformation(false);
    GE3DPoint thePoint;
    GE3DPoint theWorldPoint;
    unsigned long i;
    const EeGe3DPlane* thePlanes = reinterpret_cast<const EeGe3DPlane*>(inPlanes.data());
    if (inModel->GetParentModel() == NULL)
        theTransformation = inTransformation;
    else
        inTransformation.Apply(inModel->GetInternalTransform(inFrame), theTransformation);
    unsigned long theCount = inModel->GetMaterialCount();
    bool theInside = false;
    for (i = 0; i < theCount && !theInside; i++) {
        GEMaterial* theMaterial = inModel->GetMaterial(i);
        unsigned long theVertexStart = theMaterial->GetVertexStartingIndex();
        unsigned long theEnd = theMaterial->GetPolygonRangeLength(inLOD) + theMaterial->GetPolygonStartingIndex(inLOD);
        for (unsigned long j = theMaterial->GetPolygonStartingIndex(inLOD); j < theEnd && !theInside; j++) {
            inModel->GetVertexArray(inFrame)->GetPoint(thePoint,
                inModel->GetPolygonArray(inLOD)->GetVertexIndex1(j) + theVertexStart);
            theTransformation.Apply(thePoint, theWorldPoint);
            theInside = ee_gfx_pick_point_in_view(thePlanes, reinterpret_cast<const EeGe3DPoint*>(&theWorldPoint));
            if (theInside)
                continue;
            inModel->GetVertexArray(inFrame)->GetPoint(thePoint,
                inModel->GetPolygonArray(inLOD)->GetVertexIndex2(j) + theVertexStart);
            theTransformation.Apply(thePoint, theWorldPoint);
            theInside = ee_gfx_pick_point_in_view(thePlanes, reinterpret_cast<const EeGe3DPoint*>(&theWorldPoint));
            if (theInside)
                continue;
            inModel->GetVertexArray(inFrame)->GetPoint(thePoint,
                inModel->GetPolygonArray(inLOD)->GetVertexIndex3(j) + theVertexStart);
            theTransformation.Apply(thePoint, theWorldPoint);
            theInside = ee_gfx_pick_point_in_view(thePlanes, reinterpret_cast<const EeGe3DPoint*>(&theWorldPoint));
        }
    }
    if (!theInside) {
        theCount = inModel->GetSubModelCount();
        for (i = 0; i < theCount && !theInside; i++)
            theInside = GetIsModelInView(inModel->GetSubModel(i), theTransformation, inFrame, inLOD, inPlanes);
    }
    return theInside;
}

// port (R5a.3): an entry's unit by its id; NULL once the unit has left the world (the original's reference kept it)
EEUnit* EEUnitPicker::GetEntryUnit(unsigned long inIndex)
{
    return static_cast<EEUnit*>(gWorld.GetUnit(this->mEntries[inIndex].mUnitID));
}

// FUNCTION: EXE 0x519540
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

// the current player's selection holds a unit (FindUnit's choice asks it only with both an exact and a bounding hit)
static bool EEPickGroupNonEmpty(void*)
{
    return gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() != 0;
}

// PickUnit's search, under mLock (port: split from PickUnit, R5a.3)
EEUnit* EEUnitPicker::FindUnit(const GE2DPoint& inPoint, long inPreferResources)
{
    EeGfxPickRay theRay;
    GE3DLine theLine;
    std::vector<EEUnitPickerHit> theHits;
    EEUnitPickerHit theHit;
    EEUnit* theResult = NULL;
    const EeGeViewport* theViewport = reinterpret_cast<const EeGeViewport*>(&this->mViewport);
    EeGfxPickTolerances theTolerances = ee_gfx_pick_tolerances(theViewport, gFreeCamera, (uint32_t)gGameWindowWidth);
    unsigned long theBest = this->mCount;
    float theSavedScale = 1.0f;
    float theDistance;
    long i;
    if (this->mCount != 0) {
        ee_gfx_pick_ray(theViewport, this->mOrthographic, reinterpret_cast<const EeGe2DPoint*>(&inPoint), &theRay);
        theLine.Define(*reinterpret_cast<const GE3DPoint*>(&theRay.front), *reinterpret_cast<const GE3DPoint*>(&theRay.rear));
        for (i = this->mCount - 1; i >= 0; i--) {
            EEUnit* theEntryUnit = this->GetEntryUnit(i);   // port: ids (R5a.3)
            if (theEntryUnit == NULL)
                continue;
            theSavedScale = this->mEntries[i].GetScale();
            long theKind = GetUnitPickPriority(theEntryUnit, false);
            this->mEntries[i].SetScale(ee_gfx_pick_scale((int32_t)theKind, theSavedScale));
            if (theEntryUnit->GetTileX() != -10000 &&
                this->mEntries[i].Intersects(theLine, 1, theHit.mDistance, false)) {
                theHit.mIndex = i;
                theHit.mPriority = GetUnitPickPriority(theEntryUnit, inPreferResources != 0);
                theHit.mExact = false;
                theHit.mLarge = false;
                theHit.mKind = theKind;
                theHits.push_back(theHit);
            }
            this->mEntries[i].SetScale(theSavedScale);
        }
        for (i = theHits.size() - 1; i >= 0; i--) {
            if (theHits[i].mKind == 2) {
                theSavedScale = this->mEntries[theHits[i].mIndex].GetScale();
                this->mEntries[theHits[i].mIndex].SetScale(ee_gfx_pick_scale(2, theSavedScale));   // the tree's scale
            }
            if (this->mEntries[theHits[i].mIndex].Intersects(theLine, 2, theDistance, false)) {
                theHits[i].mDistance = theDistance;
                theHits[i].mExact = true;
            } else {
                float thePixels = gRasterizer->GetPixelSize(this->mEntries[theHits[i].mIndex], this->mViewport, this->mOrthographic);
                theHits[i].mLarge = ee_gfx_pick_is_large(thePixels, theTolerances.pixel, (int32_t)theHits[i].mKind);
            }
            if (theHits[i].mKind == 2)
                this->mEntries[theHits[i].mIndex].SetScale(theSavedScale);
        }
        ptrdiff_t theChoice = ee_gfx_pick_best(reinterpret_cast<const EeGfxPickHit*>(theHits.data()), theHits.size(),
                                               theTolerances.tolerance, this->mShowHiddenUnits != 0, NULL,
                                               EEPickGroupNonEmpty);
        if (theChoice >= 0)
            theBest = (unsigned long)theChoice;
        if (theBest < this->mCount) {
            theResult = this->GetEntryUnit(theHits[theBest].mIndex);
            if (theResult->GetComplexUnit() != NULL) {
                for (EEComplexUnit* theCarrier = theResult->GetComplexUnit()->mPreviousPart; theCarrier != NULL;
                     theCarrier = theCarrier->GetComplexUnit()->mPreviousPart)
                    theResult = theCarrier;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x519d63
// Returns the ids of the pickable units of a type (citizens of either sex for the citizen types) inside
// a screen rectangle, a part's carrier instead
void EEUnitPicker::PickUnitsInRectangle(const GERectangle& inRectangle, long inType,
                                        std::vector<unsigned long>& outUnitIDs)
{
    std::vector<GE3DPlane> thePlanes;
    this->mLock->Enter();
    outUnitIDs.clear();
    if (this->mCount > 0) {
        this->mViewport.GetFrustrumWorldPlanes(inRectangle, thePlanes, gOrthographicView);
        for (long i = this->mCount - 1; i >= 0; i--) {
            EEUnit* theUnit = this->GetEntryUnit(i);   // port: ids (R5a.3)
            if (theUnit == NULL)
                continue;
            if (!theUnit->mUnitClass->IsPickable())
                continue;
            EEUnitClass* theClass = theUnit->mUnitClass;
            if (theClass->mCategory == kUnitCategoryProjectile || theClass->mCategory == kUnitCategoryBomb ||
                (theClass->mCategory == kUnitCategoryMissile && theClass->mUnitType != kUnitTypeCpICBM) ||
                (inType != 0 && theClass->mUnitType != inType && (inType != 0x20 || theClass->mUnitType != kUnitTypeFemaleCitizen) &&
                 (inType != 0x1e0 || theClass->mUnitType != kUnitTypeCitizen)) ||
                !GetIsEntryInView(this->mEntries[i], thePlanes))
                continue;
            EEComplexUnit* theCarrier = theUnit->GetComplexUnit();
            if (theCarrier != NULL) {
                while (theCarrier->mPreviousPart != NULL)
                    theCarrier = theCarrier->mPreviousPart;
                theUnit = theCarrier;
            }
            unsigned long theID = theUnit->mUnitID;
            outUnitIDs.push_back(theID);
        }
    }
    this->mLock->Leave();
}

// FUNCTION: EXE 0x519ea2
// Returns the ids of a player's units of a type inside a screen rectangle (male and female citizens for
// either citizen type)
void EEUnitPicker::PickUnits(const GERectangle& inRectangle, EEPlayer* inPlayer, long inType,
                             std::vector<unsigned long>& outUnitIDs)
{
    this->mLock->Enter();
    outUnitIDs.clear();
    if (inType == 0x20 || inType == 0x1e0) {
        this->PickUnitsOfType(inRectangle, inPlayer, kUnitTypeCitizen, outUnitIDs);
        this->PickUnitsOfType(inRectangle, inPlayer, kUnitTypeFemaleCitizen, outUnitIDs);
    } else {
        this->PickUnitsOfType(inRectangle, inPlayer, inType, outUnitIDs);
    }

    this->mLock->Leave();
}

// FUNCTION: EXE 0x519f08
// Empties the picker (port, R5a.3: ids, nothing to release; the original dereferenced every
// unit and carrier here)
void EEUnitPicker::Clear()
{
    for (unsigned long i = 0; i < this->mCount; i++)
        this->mEntries[i].SetModel(NULL);
    this->mCount = 0;
}

// FUNCTION: EXE 0x519f7c
// Empties the picker and takes the frame's viewport, projection and see-through mode
void EEUnitPicker::Reset()
{
    this->Clear();
    this->mViewport = gU4E8F08;
    this->mOrthographic = gOrthographicView;
    this->mShowHiddenUnits = gHiddenUnitsMode;
}

// (ReferenceCarriers 0x519faa: gone with the references, R5a.3)

// FUNCTION: EXE 0x519ffb
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

// TEMPLATE: EXE 0x51a0e1
// (vector<EEUnitPickerHit>::push_back; its end() and _Ptrit copy are ICF-shared)

// TEMPLATE: EXE 0x51a10d
// (std::fill over EEUnitPickerHit, ICF-named for GEVideoMode)

// FUNCTION: EXE 0x51a130
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
