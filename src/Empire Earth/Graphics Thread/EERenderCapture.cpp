// New (port), TEST ONLY: RFIL, the render fill's capture (ee/EERenderCapture.h; tools/rfil.py). Called by
// EERenderObject::FillRenderLists on the graphics thread under the world's read lock. The record is ee_gfx rfil.rs':
// this unit hands it the render lists (the pools and vectors in place, the multimaps flattened) and the world's key.
#include <windows.h>

#include <vector>

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
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"
#include "lle/GEPolygonArray.h"
#include "lle/GELine.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GELight.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/GEText.h"
#include "lle/GERectangle.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEPhysics.h"
#include "ee/EERenderCapture.h"

extern std::vector<unsigned long> gU4E9230;   // EERenderObject.cpp: the rectangle's picked unit ids
extern EERenderUnit* gU529044;               // the picked enemy's render unit
extern UWideString gU529030;                 // its owner's name

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

// a texture map in its order, as rfil.rs reads it
template <class Map> EeGfxRfilPairs Flatten(const Map& inMap, std::vector<EeGfxRfilPair>& outPairs)
{
    for (typename Map::const_iterator it = inMap.begin(); it != inMap.end(); ++it)
        outPairs.push_back({it->first, it->second});
    return {outPairs.data(), outPairs.size()};
}

template <class T> EeGfxRfilSpan Span(const std::vector<T>& inValues) { return {inValues.data(), inValues.size()}; }

EeGfxRfilLayout MakeLayout()
{
    static_assert(sizeof(EERenderShadow) == sizeof(EeGfxFillObject) && offsetof(EERenderUnitRecord, mUnit) == 0,
                  "rfil.rs reads the shadows as EeGfxFillObject, a record's unit first");
    EeGfxRfilLayout l = {};
    l.size = sizeof(EeGfxRfilLayout);
    l.unit_stride = sizeof(EERenderUnit);
    l.unit_health_bars = offsetof(EERenderUnit, mHealthBars);
    l.unit_turret_angles = offsetof(EERenderUnit, mTurretAngles);
    l.unit_turret_models = offsetof(EERenderUnit, mTurretModels);
    l.unit_orbital_angles = offsetof(EERenderUnit, mOrbitalAngles);
    l.unit_orbital_models = offsetof(EERenderUnit, mOrbitalModels);
    l.unit_wheel_angles = offsetof(EERenderUnit, mWheelAngles);
    l.unit_wheel_models = offsetof(EERenderUnit, mWheelModels);
    l.unit_name = offsetof(EERenderUnit, mName);
    l.record_stride = sizeof(EERenderUnitRecord);
    l.record_heights = offsetof(EERenderUnitRecord, mHeights);
    l.record_cells = offsetof(EERenderUnitRecord, mCells);
    l.record_tile_x = offsetof(EERenderUnitRecord, mTileX);
    l.record_tile_y = offsetof(EERenderUnitRecord, mTileY);
    l.line_stride = sizeof(EERenderLine);
    l.line_points = offsetof(EERenderLine, mPoints);
    l.line_texture = offsetof(EERenderLine, mTexture);
    l.line_width = offsetof(EERenderLine, mWidth);
    return l;
}

}   // namespace

// The render lists as ee_gfx reads them in place (rfil.rs, units_lists.rs): the pools and vectors, the multimaps
// flattened into ioPairs (which must outlive the frame's use)
void EERenderListsView(EeGfxRfilFrame& outFrame, std::vector<EeGfxRfilPair> (&ioPairs)[6])
{
    static const EeGfxRfilLayout kLayout = MakeLayout();
    const EERenderLists& l = EERenderLists::sInstance;
    EeGfxRfilFrame& f = outFrame;
    f.layout = &kLayout;
    f.units = l.mRenderUnits;
    f.units_used = l.mRenderUnitCount;
    f.shadows = reinterpret_cast<const EeGfxFillObject*>(l.mRenderShadows);
    f.shadows_used = l.mRenderShadowCount;
    f.maps[0] = Flatten(l.mUnits0, ioPairs[0]);
    f.maps[1] = Flatten(l.mUnits1, ioPairs[1]);
    f.maps[2] = Flatten(l.mUnits2, ioPairs[2]);
    f.maps[3] = Flatten(l.mShadows0, ioPairs[3]);
    f.maps[4] = Flatten(l.mShadows1, ioPairs[4]);
    f.maps[5] = Flatten(l.mShadows2, ioPairs[5]);
    f.selected_units = Span(l.mSelectedUnits);
    f.selected_objects = Span(l.mSelectedObjects);
    f.selected_unit = l.mSelectedUnit;
    f.selected_object = l.mSelectedObject;
    f.label_unit = gU529044;
    f.label_name = (const char*)gU529030;
    f.picked_ids = Span(gU4E9230);
    f.ghosts = Span(l.mGhostRecords);
    f.lines = Span(gU529050);
}

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
