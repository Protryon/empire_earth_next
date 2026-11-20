// New (port), TEST ONLY: RFIL, the render fill's capture (ee/EERenderCapture.h; tools/rfil.py). Called by
// EERenderObject::FillRenderLists on the graphics thread under the world's read lock. The record is ee_gfx rfil.rs':
// this unit hands it the render lists (the pools and vectors in place, the multimaps flattened) and the world's key.
#include <windows.h>

#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderLists.h"
#include "ee/EEPhysics.h"
#include "ee/EERenderCapture.h"


namespace {

const uint64_t kFnvBasis = 14695981039346656037ull;

uint64_t Fnv(const void* inData, size_t inSize, uint64_t inHash)
{
    const unsigned char* theBytes = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inSize; i++) {
        inHash ^= theBytes[i];
        inHash *= 1099511628211ull;
    }
    return inHash;
}

// The fill's world, as far as two runs can differ in it (the frames' second key): every tile of the visible rows
// (its sight and remembered units) and every unit on them (its id, hit points, resource, and a complex unit's
// location, frame, animation, command, build state and selection), the current player and its viewed units. Two runs
// of the same seed reach the same world at a tick unless the harness's input changed it (a selection, an order).
uint64_t WorldKey()
{
    uint64_t theHash = kFnvBasis;
    auto theAdd = [&theHash](uint32_t inValue) { theHash = Fnv(&inValue, 4, theHash); };
    EEMap* theMap = gWorld.mMap;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    theAdd((uint32_t)thePlayer->GetPlayerID());
    theAdd(thePlayer->mViewedUnit != NULL ? (uint32_t)thePlayer->mViewedUnit->GetUnitID() : 0u);
    theAdd(thePlayer->mSelectedViewedUnit != NULL ? (uint32_t)thePlayer->mSelectedViewedUnit->GetUnitID() : 0u);
    unsigned long theRows = theMap->mTerrainMesh.GetNumVisibleTileRows();
    for (unsigned long theRow = 0; theRow < theRows; theRow++) {
        long theY;
        long theFirst;
        long theLast;
        theMap->mTerrainMesh.GetVisibleTileRow(theRow, theY, theFirst, theLast);
        for (long theX = theFirst; theX <= theLast; theX++) {
            EETile* theTile = theMap->GetTile(theX, theY);
            theAdd((uint32_t)theX);
            theAdd((uint32_t)theY);
            theAdd((theTile->IsVisibleTo(thePlayer) ? 1u : 0u) | (theTile->IsSeenBy(thePlayer) ? 2u : 0u));
            theAdd((uint32_t)theTile->GetNumDoppelgangers());
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                theAdd((uint32_t)theUnit->GetUnitID());
                theAdd((uint32_t)theUnit->mHitPoints);
                theAdd((uint32_t)theUnit->mResourceAmount);
                EEComplexUnit* theComplex = theUnit->mComplexUnit;
                if (theComplex == NULL)
                    continue;
                const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
                theHash = Fnv(&theLocation, sizeof(theLocation), theHash);
                theAdd((uint32_t)theComplex->mPhysicalModel.GetFrame());
                theAdd((uint32_t)theComplex->mNextFrame);
                theHash = Fnv(&theComplex->mAnimationFraction, 4, theHash);
                theAdd((uint32_t)theComplex->mAnimation);
                theAdd(theComplex->mCommand != NULL ? (uint32_t)theComplex->mCommand->mType : 0u);
                theAdd((theComplex->mIsBuilt ? 1u : 0u) | (thePlayer->mSelectedGroup->Contains(theComplex) ? 2u : 0u));
            }
        }
    }
    return theHash;
}

}   // namespace

void EERenderCaptureFill(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount)
{
    if (!ee_gfx_rfil_active())
        return;
    std::vector<EeGfxRfilPair> thePairs[6];
    EeGfxRfilFrame f = {};
    EERenderListsView(f, thePairs);
    f.frame = (uint32_t)inInfo.mFrame;
    f.ticks = gPhysics != NULL ? (uint32_t)gPhysics->mTicksRun : 0u;
    f.sway = inSwayClock;
    f.yaw = inInfo.mYaw;
    f.world_transformation = *reinterpret_cast<const EeGeTransformation*>(&gWorld.mTransformation);
    f.flags = (gFreeCamera ? 1u : 0u) | (gOrthographicView ? 2u : 0u) | (inInfo.mAmbient ? 4u : 0u);
    f.picked = inInfo.mPickedUnit != NULL ? (int32_t)inInfo.mPickedUnit->GetUnitID() : -1;
    f.world_key = WorldKey();
    f.fighting = inInfo.mNumFightingUnits;
    f.unit_count = inUnitCount;
    ee_gfx_rfil_write(&f);
}

// the renderer's hook (ee/EERenderCapture.h)
static const bool sEERenderCaptureRegistered = (gEERenderCaptureFill = EERenderCaptureFill, true);
