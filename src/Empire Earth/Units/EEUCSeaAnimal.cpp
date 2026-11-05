// C:\EEX\Empire Earth\Units\EEUCSeaAnimal.cpp (file name inferred: no __FILE__; the class name). The sea animal's unit
// class: 0x219d67..0x219fe3 (after EEUCResource.cpp; EEUCSpaceBuilding.cpp follows). Unit flags: the exe's module
// set. Its ctor is inline. Data (.rdata, in order): the four named depth constants 0x447340..0x44734c.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUCSubmergedNaval.h"
#include "ee/EEUCSeaAnimal.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EmpireEarth.h"

// the depths a sea animal swims at (provisional names)
const float kSeaAnimalBottomClearance = 0.09f;
const float kSeaAnimalMaximumStep = 0.48f;
const float kSeaAnimalMinimumDepth = -0.6f;
const float kSeaAnimalMaximumDepthFactor = 0.81f;

// FUNCTION: EXE 0x619d67
// Places a location as a submerged ship does
void EEUCSeaAnimal::GroundLocation(GE3DPoint& ioLocation)
{
    EEUCSubmergedNaval::GroundLocation(ioLocation);
}

// FUNCTION: EXE 0x619d73
// Returns a location's tile as a submerged ship does
void EEUCSeaAnimal::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    EEUCSubmergedNaval::LocationToTile(inLocation, outX, outY);
}

// FUNCTION: EXE 0x619d87
// Gives each path point (from the end back) a depth one random step toward a random depth: dolphins step 0.2..1.2
// and may dive deeper, others step up to 0.48; kept under -0.1 (dolphins) or -0.6 and 0.09 above the sea floor
void EEUCSeaAnimal::AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    const GETerrainMesh& theMesh = ::gWorld.mMap->mTerrainMesh;
    float theDepth = inUnit->mPhysicalModel.GetLocation().GetZ();
    bool theSurfaced = true;
    for (long i = ioPath.size() - 1; i >= 0; i--) {
        float theGround = theMesh.GetMeshZ(ioPath[i].GetX(), ioPath[i].GetY());
        float theTarget;
        bool theDive = this->mUnitType == kUnitTypeResDolphin;
        if (theDive) {
            theDive = i != 0;
            if (theDive) {
                theDive = !theSurfaced;
                if (theDive)
                    theDive = gPhysics->mRandom.Generate() < 0.35f;
            }
        }
        if (!theDive) {
            theTarget = gPhysics->GetRandom(0.1f, kSeaAnimalMaximumDepthFactor) * theGround;
            theTarget = min(kSeaAnimalMinimumDepth, theTarget);
        } else {
            theTarget = gPhysics->GetRandom(0.45f, 0.85f) * theGround;
            theSurfaced = true;
        }
        if (theSurfaced)
            theSurfaced = false;
        float theDistance = UMAbs(theDepth - theTarget);
        float theLimit;
        if (this->mUnitType == kUnitTypeResDolphin) {
            float theStep = gPhysics->GetRandom(0.2f, 1.2f);
            if (theDepth < theTarget)
                theDepth += theStep;
            else
                theDepth -= theStep;
            theDistance = UMAbs(theDistance - theStep);
            theLimit = -0.1f;
        } else {
            float theStep = gPhysics->GetRandom(0.2f, kSeaAnimalMaximumStep);
            if (theDepth < theTarget)
                theDepth += theStep;
            else
                theDepth -= theStep;
            theDistance = UMAbs(theDistance - theStep);
            theLimit = kSeaAnimalMinimumDepth;
        }
        theDepth = min(theLimit, theDepth);
        if (theDepth <= theGround + kSeaAnimalBottomClearance)
            theDepth = theGround + kSeaAnimalBottomClearance;
        ioPath[i].SetZ(theDepth);
    }
}

// FUNCTION: EXE 0x619fd7
// Updates the sea animal as a submerged ship
void EEUCSeaAnimal::Update(EEComplexUnit* inUnit)
{
    EEUCSubmergedNaval::Update(inUnit);
}

// ICF-FOLDED: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
// (no address of its own: the same body as EEUCBasicCitizen::UpdateEffects, which ICF kept)
void EEUCSeaAnimal::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}
