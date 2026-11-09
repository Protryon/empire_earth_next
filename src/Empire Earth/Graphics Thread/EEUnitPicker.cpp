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

// the picker's named consts (.rdata 0x441c28.., reloaded per use) (provisional names)
const float kU441C28 = 3.0f;
const float kMinPickPixelSize = 0.35f;
const float kFreeCameraPickWidth = 1024.0f;
const float kLargeHitFraction = 0.17f;
const float kU441C38 = 0.25f;
const float kPickScale = 1.2f;
const float kTreePickScale = 0.9f;

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
    gRasterizer->DrawWater(gWorld.mMap->mTerrainMesh, gScreenIndex, gOrthographicView);
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
long GetUnitPickPriority(EEUnit* inUnit, bool inPreferResources)
{
    long theResult = 0;
    if (inUnit->GetComplexUnit() != NULL && inUnit->mHitPoints > 0 && inUnit->mPlayer != gWorld.mPlayers[0]) {
        if (inUnit->mUnitClass->mFamily == kUnitFamilyAABuilding)
            theResult = 6;
        else if (inUnit->mUnitClass->IsLandBuilding() || inUnit->mUnitClass->mIsWall)
            theResult = 1;
        else {
            if (gWorld.GetCurrentPlayer()->IsAlly(inUnit->GetPlayer()))

                theResult = 4;
            else
                theResult = 6;
        }
    } else if (inUnit->mResourceType == kResourceWood) {
        theResult = 2;
    } else if (inUnit->mResourceType >= kResourceStone && inUnit->mResourceType < 7) {
        theResult = 3;
    }
    if (inPreferResources && inUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen && inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen &&
        inUnit->mResourceType >= kResourceFood && inUnit->mResourceType < 7 && inUnit->mResourceAmount > 0)
        theResult = 5;
    return theResult;
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
            theFound = this->mEntries[i].mUnit == inUnit;
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
            EEComplexUnit* theUnit = this->mEntries[i].mUnit->GetComplexUnit();
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
    long theOutside = 0;
    GE3DPoint theCenter;
    float theScale = inEntry.GetScale();
    GEModel* theModel = inEntry.GetModel();
    unsigned long theFrame = inEntry.GetFrame();
    bool theResult = true;
    GETransformation theTransformation(false);
    bool theBelow[6];
    unsigned long i;
    inEntry.GetLocation().Apply(theModel->GetInternalTransform(theFrame), theTransformation);
    if (theScale != 1.0f) {
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetPosition(inEntry.GetLocation().GetX(), inEntry.GetLocation().GetY(),
                                      inEntry.GetLocation().GetZ());
    }
    theTransformation.Apply(theModel->GetCenterPoint(), theCenter);
    for (i = 2; i <= 5; i++) {
        theBelow[i] = inPlanes[i].IsPointBelowPlane(theCenter);
        if (theBelow[i])
            theOutside++;
    }
    if (theOutside != 0) {
        float theRadius = theModel->GetBoundingRadius(theFrame) * theScale;
        for (i = 2; i <= 5; i++) {
            if (!theBelow[i])
                continue;
            const GE3DPoint& thePoint = inPlanes[i].GetPoint();
            GEVector theOffset(theCenter.mX - thePoint.mX, theCenter.mY - thePoint.mY,
                               theCenter.mZ - thePoint.mZ);
            float theDistance = theOffset.DotProduct(inPlanes[i].GetNormal());
            if (theDistance < 0.0f)
                theDistance = -theDistance;
            if (theDistance <= theRadius)
                theOutside--;
        }
        if (theOutside == 0) {
            unsigned long theLOD = UMFloatToLong((1.0f - kU441C38) * theModel->GetLODCount());
            if (theLOD == theModel->GetLODCount())
                theLOD = theModel->GetLODCount() - 1;
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
    unsigned long k;
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
            theInside = true;
            for (k = 2; k <= 5 && theInside; k++)
                theInside = inPlanes[k].IsPointOnOrAbovePlane(theWorldPoint);
            if (theInside)
                continue;
            inModel->GetVertexArray(inFrame)->GetPoint(thePoint,
                inModel->GetPolygonArray(inLOD)->GetVertexIndex2(j) + theVertexStart);
            theTransformation.Apply(thePoint, theWorldPoint);
            theInside = true;
            for (k = 2; k <= 5 && theInside; k++)
                theInside = inPlanes[k].IsPointOnOrAbovePlane(theWorldPoint);
            if (theInside)
                continue;
            inModel->GetVertexArray(inFrame)->GetPoint(thePoint,
                inModel->GetPolygonArray(inLOD)->GetVertexIndex3(j) + theVertexStart);
            theTransformation.Apply(thePoint, theWorldPoint);
            theInside = true;
            for (k = 2; k <= 5 && theInside; k++)
                theInside = inPlanes[k].IsPointOnOrAbovePlane(theWorldPoint);
        }
    }
    if (!theInside) {
        theCount = inModel->GetSubModelCount();
        for (i = 0; i < theCount && !theInside; i++)
            theInside = GetIsModelInView(inModel->GetSubModel(i), theTransformation, inFrame, inLOD, inPlanes);
    }
    return theInside;
}

// FUNCTION: EXE 0x519540
// Returns the unit under a screen point (its carrier for a part), referenced: polygon hits beat
// bounding hits, then the nearest, with click priority deciding near ties; or null
EEUnit* EEUnitPicker::PickUnit(const GE2DPoint& inPoint, long inPreferResources)
{
    GE3DPoint theNear;
    GE3DPoint theFar;
    GE3DPoint theNearWorld;
    GE3DPoint theFarWorld;
    GE3DLine theLine;
    std::vector<EEUnitPickerHit> theHits;
    EEUnitPickerHit theHit;
    float theScale = 1.0f / this->mViewport.GetHalfWidth();
    EEUnit* theResult = NULL;
    float theTolerance = kU441C28 / (this->mViewport.GetRearClippingDistance() - this->mViewport.GetFrontClippingDistance());
    float theWidth = kFreeCameraPickWidth;
    if (!gFreeCamera)
        theWidth = (float)gGameWindowWidth;
    unsigned long theBest = this->mCount;
    float thePixel = 1.0f / theWidth;
    float theSavedScale = 1.0f;
    unsigned long theFirst = theBest;
    unsigned long theSecond = theBest;
    float theDistance;
    long i;
    this->mLock->Enter();
    if (this->mCount != 0) {
        float theX = (inPoint.GetX() - this->mViewport.GetXCenter()) * theScale;
        float theY = -((inPoint.GetY() - this->mViewport.GetYCenter()) * theScale);
        theNear = GE3DPoint(theX, theY, 0.0f);
        theFar = GE3DPoint(theX, theY, 1.0f);
        if (!this->mOrthographic) {
            this->mViewport.DenormalizePerspectivePoint(theNear, this->mViewport.GetFrontClippingDistance(), theNearWorld);
            this->mViewport.DenormalizePerspectivePoint(theFar, this->mViewport.GetRearClippingDistance(), theFarWorld);
        } else {
            this->mViewport.DenormalizeOrthographicPoint(theNear, theNearWorld);
            this->mViewport.DenormalizeOrthographicPoint(theFar, theFarWorld);
        }
        theLine.Define(theNearWorld, theFarWorld);
        for (i = this->mCount - 1; i >= 0; i--) {
            theSavedScale = this->mEntries[i].GetScale();
            long theKind = GetUnitPickPriority(this->mEntries[i].mUnit, false);
            this->mEntries[i].SetScale((theKind != 2 ? kPickScale : kTreePickScale) * theSavedScale);
            if (this->mEntries[i].mUnit->GetTileX() != -10000 &&
                this->mEntries[i].Intersects(theLine, 1, theHit.mDistance, false)) {
                theHit.mIndex = i;
                theHit.mPriority = GetUnitPickPriority(this->mEntries[i].mUnit, inPreferResources != 0);
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
                this->mEntries[theHits[i].mIndex].SetScale(kTreePickScale * theSavedScale);
            }
            if (this->mEntries[theHits[i].mIndex].Intersects(theLine, 2, theDistance, false)) {
                theHits[i].mDistance = theDistance;
                theHits[i].mExact = true;
            } else {
                bool theLarge;
                if (gRasterizer->GetPixelSize(this->mEntries[theHits[i].mIndex], this->mViewport, this->mOrthographic) * thePixel >
                        kLargeHitFraction || theHits[i].mKind == 2)
                    theLarge = true;
                else
                    theLarge = false;
                theHits[i].mLarge = theLarge;
            }
            if (theHits[i].mKind == 2)
                this->mEntries[theHits[i].mIndex].SetScale(theSavedScale);
        }
        for (i = theHits.size() - 1; i >= 0; i--) {
            if (theHits[i].mLarge)
                continue;
            if (!theHits[i].mExact) {
                if (theSecond != this->mCount) {
                    float theDelta = theHits[i].mDistance - theHits[theSecond].mDistance;
                    if ((-theTolerance <= theDelta && theDelta <= theTolerance) || this->mShowHiddenUnits != 0) {
                        if (theHits[i].mPriority <= theHits[theSecond].mPriority) {
                            if (theDelta >= 0.0f)
                                continue;
                            if (theHits[i].mPriority != theHits[theSecond].mPriority)
                                continue;
                        }
                    } else if (theDelta >= 0.0f) {
                        continue;
                    }
                }
                theSecond = i;
            } else {
                if (theFirst != this->mCount) {
                    float theDelta = theHits[i].mDistance - theHits[theFirst].mDistance;
                    if ((-theTolerance <= theDelta && theDelta <= theTolerance) || this->mShowHiddenUnits != 0) {
                        if (theHits[i].mPriority <= theHits[theFirst].mPriority) {
                            if (theDelta >= 0.0f)
                                continue;
                            if (theHits[i].mPriority != theHits[theFirst].mPriority)
                                continue;
                        }
                    } else if (theDelta >= 0.0f) {
                        continue;
                    }
                }
                theFirst = i;
            }
        }
        if (theSecond < this->mCount) {
            if (theFirst < this->mCount) {
                theBest = theFirst;
                if (gWorld.GetCurrentPlayer()->mSelectedGroup->GetNumUnits() != 0 &&
                    theHits[theFirst].mPriority < theHits[theSecond].mPriority)
                    theBest = theSecond;
            } else {
                theBest = theSecond;
            }
        } else if (theFirst < this->mCount) {
            theBest = theFirst;
        }
        if (theBest < this->mCount) {
            theResult = this->mEntries[theHits[theBest].mIndex].mUnit;
            if (theResult->GetComplexUnit() != NULL) {
                for (EEComplexUnit* theCarrier = theResult->GetComplexUnit()->mPreviousPart; theCarrier != NULL;
                     theCarrier = theCarrier->GetComplexUnit()->mPreviousPart)
                    theResult = theCarrier;
            }
            theResult->Reference();
        }
    }
    this->mLock->Leave();
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
            EEUnit* theUnit = this->mEntries[i].mUnit;
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
// Releases every picked unit and carrier and empties the picker
void EEUnitPicker::Clear()
{
    unsigned long i;
    for (i = 0; i < this->mCount; i++) {
        this->mEntries[i].SetModel(NULL);
        this->mEntries[i].mUnit->Dereference();
    }
    this->mCount = 0;
    for (i = 0; i < this->mCarriers.size(); i++)
        this->mCarriers[i]->Dereference();
    this->mCarriers.clear();
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

// FUNCTION: EXE 0x519faa
// Keeps a reference to every other part of a unit's chain (its carriers and riders)
void EEUnitPicker::ReferenceCarriers(EEComplexUnit* inUnit)
{
    EEComplexUnit* theUnit = inUnit;
    while (theUnit->mPreviousPart != NULL)
        theUnit = theUnit->mPreviousPart;
    for (; theUnit != NULL; theUnit = theUnit->mNextPart) {
        if (theUnit != inUnit) {
            theUnit->Reference();
            this->mCarriers.push_back(theUnit);
        }
    }
}

// FUNCTION: EXE 0x519ffb
// Adds a unit at a transformation when there is room and it is big enough on screen (no path points;
// no ambient units or projectiles but ICBMs unless the camera is free)
void EEUnitPicker::AddUnit(EEUnit* inUnit, const GETransformation& inTransformation, float inPixelSize)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    EEComplexUnit* theComplexUnit = inUnit->GetComplexUnit();
    EEUnitPickerEntry& theEntry = this->mEntries[this->mCount];
    if (!(this->mCount < this->mCapacity) || !(inPixelSize >= kMinPickPixelSize) || theClass->mUnitType == kUnitTypePathPoint ||
        (!gFreeCamera && (theClass->mCategory == kUnitCategoryAmbient || (theClass->mIsProjectile && theClass->mUnitType != kUnitTypeCpICBM))))
        return;
    inUnit->Reference();
    theEntry.mUnit = inUnit;
    if (theComplexUnit != NULL) {
        this->ReferenceCarriers(theComplexUnit);
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
