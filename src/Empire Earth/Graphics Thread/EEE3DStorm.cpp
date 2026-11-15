// C:\EEX\Empire Earth\Graphics Thread\EEE3DStorm.cpp (no __FILE__; name from the RTTI class). 0xfb088..0xfb9f6: the
// init chain of a file-scope map, the ctors .. Save, then the map's _Tree COMDATs.
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
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include <map>
#include "lle/U2DPoint.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEE3DStorm.h"
#include "ee/EEEffectManager.h"

// The world a storm reads and calls into (ee_gfx effect_storm.rs; provisional name)
struct EEStormHostOps {
    static bool OnMap(void*, int32_t inX, int32_t inY) { return gWorld.GetMap()->GetTile(inX, inY) != NULL; }
    static bool IsWater(void*, int32_t inX, int32_t inY)
    {
        EEMapTile* theTile = gWorld.GetMap()->GetTile(inX, inY);
        return theTile && (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater);
    }
    static float GroundZ(void*, int32_t inX, int32_t inY) { return gWorld.GetMap()->mTerrainMesh.GetZ(inX, inY); }
    static void StepToward(void*, int32_t inFromX, int32_t inFromY, int32_t inToX, int32_t inToY, int32_t* outTile)
    {
        EETileCoordinate theFrom;
        EETileCoordinate theTile;
        theFrom.mX = inFromX;
        theFrom.mY = inFromY;
        EEGetNextTile(theFrom, theTile, EEGetTileDirection(inFromX, inFromY, inToX, inToY), 1);
        outTile[0] = (int32_t)theTile.mX;
        outTile[1] = (int32_t)theTile.mY;
    }
    static void Ripple(void*, float inX, float inY)
    {
        GE3DPoint theSplash(inX, inY, 0.0f);
        gWorld.GetEffectManager()->CreatePlacedEffect(theSplash, EEGraphicEffect::Get(kGraphicEffectLandRipple), NULL);
    }
};

static EeGfxStormHost EEStormHost()
{
    EeGfxStormHost theHost = {NULL, EEStormHostOps::OnMap, EEStormHostOps::IsWater, EEStormHostOps::GroundZ,
                              EEStormHostOps::StepToward, EEStormHostOps::Ripple};
    return theHost;
}

// Moves the storm to a point on the terrain (a fresh transformation there)
static void EEPlaceStorm(EEE3DStorm* inStorm, const EeGe3DPoint& inPoint)
{
    GETransformation theTransformation;
    theTransformation.SetPosition(inPoint.x, inPoint.y, inPoint.z);
    inStorm->SetTransformation(theTransformation);
}

// FUNCTION: EXE 0x4fb0c8
// Creates a weather storm of the given kind over a copy of the tiles, 5 s long at full intensity (the record's
// defaults), centred on them
EEE3DStorm::EEE3DStorm(long inKind, const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    std::vector<int32_t> theTiles;
    for (size_t i = 0; i < inTiles.size(); i++) {
        theTiles.push_back((int32_t)inTiles[i].mX);
        theTiles.push_back((int32_t)inTiles[i].mY);
    }
    this->mModel = NULL;
    ee_gfx_storm_new(this->mRecord, theTiles.empty() ? NULL : &theTiles[0], (uint32_t)inTiles.size());
    this->GetCenter();
}

// SYNTHETIC: EXE 0x4fb175
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fb191
// Creates an empty storm of the given kind (for loading)
EEE3DStorm::EEE3DStorm(long inKind)
    : EEEffect3D(inKind)
{
    this->mModel = NULL;
}

// FUNCTION: EXE 0x4fb217
// Destroys the storm, releasing its model (the record frees the tiles and drops)
EEE3DStorm::~EEE3DStorm()
{
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
}

// FUNCTION: EXE 0x4fb252
// Steps every storm tile one tile towards its place around the new centre (a tile that would leave the map stays),
// recentres the storm and returns the tiles' bounds; the radius becomes half the longer side (ee_gfx
// effect_storm.rs)
void EEE3DStorm::Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4] = {(int32_t)outBounds.mLeft, (int32_t)outBounds.mTop, (int32_t)outBounds.mRight,
                            (int32_t)outBounds.mBottom};
    EeGfxStormHost theHost = EEStormHost();
    EeGe3DPoint thePoint;
    if (!ee_gfx_storm_move(this->mRecord, (int32_t)inCenter.mX, (int32_t)inCenter.mY, theBounds, &theHost, &thePoint))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
    EEPlaceStorm(this, thePoint);
}

// FUNCTION: EXE 0x4fb500
// Recomputes the storm centre as the mean of its tiles and moves the effect there on the terrain (ee_gfx
// effect_storm.rs)
void EEE3DStorm::GetCenter()
{
    EeGfxStormHost theHost = EEStormHost();
    EeGe3DPoint thePoint;
    if (ee_gfx_storm_center(this->mRecord, &theHost, &thePoint))
        EEPlaceStorm(this, thePoint);
}

// FUNCTION: EXE 0x4fb6b4
// Returns the bounding rectangle of the storm's tiles (unchanged when it has none)
void EEE3DStorm::GetBounds(U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4];
    if (!ee_gfx_storm_bounds(this->mRecord, theBounds))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
}

// FUNCTION: EXE 0x4fb72e
// Reads fall speed, spawn height, density and duration from the storm's graphic effect
void EEE3DStorm::Virtual64()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    this->mFallSpeed = theEffect->GetParam1();
    this->mSpawnHeight = theEffect->GetParam2();
    this->mDensity = theEffect->GetParam3();
    this->mDuration = theEffect->mDuration;
}

// FUNCTION: EXE 0x4fa291
// Reads the settings, then places the storm at the mean of its tiles on the terrain, with a radius reaching its
// farthest tile in x or y (the rain's and the snow's; ee_gfx effect_storm.rs)
long EEE3DStorm::SetUp()
{
    EeGfxStormHost theHost = EEStormHost();
    EeGe3DPoint thePoint;
    this->Virtual64();
    ee_gfx_storm_set_up(this->mRecord, &theHost, &thePoint);
    EEPlaceStorm(this, thePoint);
    return 0;
}

// FUNCTION: EXE 0x4f76da
// FUNCTION: EXE 0x4f9e42
// Copies the drops into the draw list and queues the storm in render queue 4 under its texture
long EEE3DStorm::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDraws.resize(ee_gfx_storm_drop_count(this->mRecord));
        if (!this->mDraws.empty())
            ee_gfx_storm_draws(this->mRecord, &this->mDraws[0], (uint32_t)this->mDraws.size());
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// The draw passes' render state: the graphic object's alpha reference, depth test only, alpha blending, the
// clouds' light and the scene's fog (provisional name)
void EEE3DStorm::BeginDraw()
{
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetCloudLighting();
    SetSceneFog();
}

// FUNCTION: EXE 0x4fb756
// Reads the storm's elapsed time, tiles, centre, intensity and sustained flag, then the leaf's drop count, the
// rain's thunder delay, a position (discarded), the bounding radius and the number of drops to recreate (ee_gfx
// effect_storm.rs); returns the first read error
long EEE3DStorm::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        EeGfxEffectFile theFile = EEEffectFile(inFile);
        theResult = ee_gfx_storm_load(this->mRecord, &theFile);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f7d0f
// FUNCTION: EXE 0x4fa5f3
// After loading, reloads the settings, recreates the saved number of drops at random heights and marks the storm
// restored (the rain's sound restarts)
long EEE3DStorm::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0) {
        this->Virtual64();
        EeGfxStormHost theHost = EEStormHost();
        ee_gfx_storm_repair(this->mRecord, &theHost);
    }
    return theResult;
}

// FUNCTION: EXE 0x4fb83f
// Writes the base effect, elapsed time, tiles, centre, intensity and sustained flag, then the leaf's drop count, the
// rain's thunder delay, the position, the radius and the live drop count (ee_gfx effect_storm.rs); returns the
// first write error
long EEE3DStorm::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0) {
        EeGfxEffectFile theFile = EEEffectFile(outFile);
        theResult = ee_gfx_storm_save(this->mRecord, &theFile);
    }
    return theResult;
}

// FUNCTION: EXE 0x4fa70d
// Spawns and moves the drops (the snow's fall by the tick over the game speed) and ages the storm; returns 7 once
// its duration has passed (the snow's: and its last flake landed) unless it is sustained (ee_gfx effect_storm.rs)
long EEE3DStorm::Step()
{
    EeGfxStormHost theHost = EEStormHost();
    return ee_gfx_storm_step(this->mRecord, gPhysics->GetTickTime(), gWorld.GetGameSpeed(), &theHost);
}
