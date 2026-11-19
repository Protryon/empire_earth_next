// The effects' world (docs/RUST_PLAN.md R4.4; the graphics + LLE cluster's second round): the effects and their
// manager are Rust objects (ee_gfx effect_obj.rs, ffi_manager.rs); this unit answers what they ask of the C++ world.
// The effects' constructors by the manager's entry points (their reads of the units: EEEffectConstruct), the C++
// objects an effect holds (its unit, graphic effect, models, graphic objects and sounds: their references, loads and
// releases), the map's tiles and doppelgangers, the answers of the 3D kinds (EeGfxEffectAnswers3D: ee_gfx
// effect_rising.rs and its kinds' modules) and of the 2D ones (EeGfxEffectAnswers2D: effect_2d.rs) that call the
// units, sounds, area effects and the map, the damage controller's tick facts, the goal markers' questions, the TEST
// ONLY captures' file and records, the effects' file over an FSFile, the particles' ground, the quad batches' arrays
// and the battlefield decals' entry points. Since R4.4's twenty-fifth round one file for what was EEE3DRust.cpp and
// EEE2DRust.cpp (the original's 60 effect leaves, deleted round by round), since the second cluster round also for
// EEEffect.cpp, EEEffectManager.cpp and EEGraphicalGoalManager.cpp.
#include <windows.h>
#include <math.h>

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
#include "ee/EEEffectManager.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEGamePreferences.h"
#include "lle/GERectangle.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEModelManager.h"
#include "lle/Graphics Engine.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/FSMemoryFile.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEffectMesh.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEAreaUtilities.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/SESMInstanceNode.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAEManager.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EESimCapture.h"
#include "ee/EESaveGameFiles.h"
#include "rust/base/math.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"

class EEComplexUnit;
// 0x23cee2 (EERenderLists.h): the unit's draw opacity (the farm's)
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity);

// the classes are not standard-layout; offsetof of a non-virtual base's member is the member's place in every object
#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

// where Base sits in a Derived object (a cast of a made-up address: nothing is read)
template <class Derived, class Base> static uint64_t EEBaseOffset()
{
    Derived* theObject = reinterpret_cast<Derived*>(static_cast<uintptr_t>(0x10000));
    return reinterpret_cast<uintptr_t>(static_cast<Base*>(theObject)) - 0x10000;
}

// The members ee_gfx effect_reads.rs reads in place (the graphic objects, the graphic effects' records and links, the
// complex units' physical models and turret angles)
void EEGfxObjectLayoutRegister()
{
    static_assert(sizeof(std::vector<EETurretPart*>) == 3 * sizeof(void*), "a vector is three words");
    EeGfxObjectLayout theLayout = {};
    theLayout.size = sizeof(EeGfxObjectLayout);
    theLayout.gobj_fields = EEBaseOffset<EEGraphicObject, EeDataGraphicObjectFields>();
    theLayout.gobj_models = offsetof(EEGraphicObject, mModels);
    theLayout.gobj_textures = offsetof(EEGraphicObject, mTextures);
    theLayout.gobj_tag_points = offsetof(EEGraphicObject, mTagPoints);
    theLayout.gobj_tags = offsetof(EEGraphicObject, mTags);
    theLayout.gobj_model = offsetof(EEGraphicObject, mModel);
    theLayout.gobj_default_point = offsetof(EEGraphicObject, mDefaultPoint);
    theLayout.gobj_top_point = offsetof(EEGraphicObject, mTopPoint);
    theLayout.gobj_turret_parts = offsetof(EEGraphicObject, mTurretParts);
    theLayout.gobj_orbitals = offsetof(EEGraphicObject, mOrbitals);
    theLayout.turret_part_model = offsetof(EETurretPart, mModel);
    theLayout.orbital_model = offsetof(EEOrbital, mModel);
    theLayout.orbital_speed = offsetof(EEOrbital, mSpeed);
    theLayout.orbital_angle = offsetof(EEOrbital, mAngle);
    theLayout.geffect_fields = EEBaseOffset<EEGraphicEffect, EeDataGraphicEffectFields>();
    theLayout.geffect_object = offsetof(EEGraphicEffect, mGraphicObject);
    theLayout.geffect_linked = offsetof(EEGraphicEffect, mLinkedEffect1);
    static_assert(offsetof(EEGraphicEffect, mLinkedEffect3) == offsetof(EEGraphicEffect, mLinkedEffect1) + 2 * sizeof(void*),
                  "the links in order");
    theLayout.complex_scale = offsetof(EEComplexUnit, mPhysicalModel) + offsetof(GEPhysicalModel, mScale);
    theLayout.complex_frame = offsetof(EEComplexUnit, mPhysicalModel) + offsetof(GEPhysicalModel, mFrame);
    theLayout.complex_turret_angles = offsetof(EEComplexUnit, mTurretAngles);
    theLayout.stats_model = offsetof(EEUnitClassUpgrade, mModel);
    ee_gfx_object_layout_set(&theLayout);
}

namespace {
EeGfxEffectHead* H(EEEffect* inEffect) { return reinterpret_cast<EeGfxEffectHead*>(inEffect); }
EEEffect* Fx(EeGfxEffectHead* inEffect) { return reinterpret_cast<EEEffect*>(inEffect); }
uint64_t U(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
template <class T> T* P(uint64_t inObject) { return reinterpret_cast<T*>((uintptr_t)inObject); }
EeGe3DPoint R(const GE3DPoint& inPoint) { return EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ}; }

// the map a weather storm reads and calls into (ee_gfx effect_storm.rs; the storms' and EEEMStorm's moves)
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

// a sound replaced by a referenced node (EEE3DRain::SetSound 0xf8337, SetRumble 0xf835e, EEE3DParticleEmitter::SetSound
// 0xf69bc), the node's own reference then dropped
void EESetEffectSound(SESMInstanceNode*& ioSound, SESMInstanceNode* inSound)
{
    if (inSound != ioSound) {
        if (ioSound)
            ioSound->Dereference();
        ioSound = inSound;
        if (inSound)
            inSound->Reference();
    }
    if (ioSound)
        ioSound->Dereference();
}

// a rain sound's volume, pan and pitch from the camera's distance to the storm (range 5 times its radius), scaled by
// inVolume and by 0.4 in the first 6.5 s (EEE3DRain::SetSoundLevel, 0xf80e8)
void EESetRainSoundLevel(EEEffect* inRain, SESMInstanceNode* inSound, float inVolume)
{
    GE3DPoint thePosition;
    if (!inSound)
        return;
    float theVolume;
    float thePan;
    float thePitch;
    inRain->Transformation().GetPosition(thePosition);
    EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, inRain->mRecord->bounding_radius * 5.0f);
    theVolume *= inVolume;
    if (inRain->mRecord->storm.elapsed < EE_GFX_RAIN_FADE_TIME)
        theVolume = EE_GFX_RAIN_FADE_SHARE * theVolume;
    gSoundManager->SetSoundParameters(inSound, theVolume, thePan, thePitch);
}

// the map's questions for ee_gfx volcano.rs (the volcano's build)
float EEVolcanoZ(void* inMap, uint32_t inX, uint32_t inY)
{
    return static_cast<EEMap*>(inMap)->mTerrainMesh.GetZ(inX, inY);
}
EeGfxColor EEVolcanoColor(void* inMap, uint32_t inX, uint32_t inY)
{
    const GEColor& theColor = EETerrainTable::Get(static_cast<EEMap*>(inMap)->mTerrainMesh.GetTextureSetID(inX, inY))->mColor;
    EeGfxColor theResult = {theColor.mRed, theColor.mGreen, theColor.mBlue};
    return theResult;
}
bool EEVolcanoValidTile(void* inMap, uint32_t inX, uint32_t inY)
{
    return static_cast<EEMap*>(inMap)->mTerrainMesh.GetIsValidTile(inX, inY);
}
EeGfxColor EEVolcanoLight(void* inMap, int32_t inX, int32_t inY, int32_t inCorner)
{
    const GEColor& theColor = static_cast<EEMap*>(inMap)->GetVertexColor(inX, inY, inCorner);
    EeGfxColor theResult = {theColor.mRed, theColor.mGreen, theColor.mBlue};
    return theResult;
}

// the terrain's questions for ee_gfx farm.rs (the farm's meshes)
float EEFarmZ(void* inMesh, uint32_t inX, uint32_t inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetZ(inX, inY);
}
EeGeVector EEFarmNormal(void* inMesh, uint32_t inX, uint32_t inY)
{
    const GEVector* theNormal = static_cast<GETerrainMesh*>(inMesh)->GetVertexNormal(inX, inY);
    EeGeVector theResult = {theNormal->GetX(), theNormal->GetY(), theNormal->GetZ()};
    return theResult;
}
float EEFarmMeshZ(void* inMesh, float inX, float inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetMeshZ(inX, inY);
}

// The C++ answers of one call (the call's EeGfxEffectAsk: its effect, its operation, a constructor's argument; the
// reads of the graphic objects, tag points and physical models and the effects' own answers are ee_gfx's)
struct Ask {
    static EEEffect* E(void* inAsk) { return Fx(static_cast<EeGfxEffectAsk*>(inAsk)->effect); }
    // a unit's graphic (a virtual: a foundation shows its player's class graphic)
    static uint64_t UnitGraphic(void*, uint64_t inUnit) { return U(P<EEUnit>(inUnit)->GetModel()); }
    // a tracer's end: its impact effect at its position, then the target's hit (EEE3DTracers::Step, 0xfdad4)
    static void ShotHit(void* inAsk)
    {
        EEEffect* theEffect = E(inAsk);
        EEGraphicEffect* theImpact = theEffect->mGraphicEffect->mLinkedEffect1;
        if (theImpact) {
            GE3DPoint theLocation(theEffect->Transformation());
            gEffectManager->CreatePlacedEffect(theLocation, theImpact, NULL);
        }
        if (theEffect->mUnit)
            theEffect->mUnit->mUnitClass->UnitDamaged(theEffect->mUnit->mComplexUnit, NULL, 0);
    }
    // the frame's sound at the unit, kept as the effect's (EEE3DDisease::Step and SetSound, 0xec6bd)
    static void AnimationSound(void* inAsk, uint32_t inFrame, bool inChanged)
    {
        EEEffect* theEffect = E(inAsk);
        GE3DPoint theLocation;
        theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
        KeepSound(theEffect, PlayAnimationSound(theLocation, theEffect->mGraphicObject, 0, theEffect->mSound, inFrame,
                                                inChanged));
    }
    // the effect's sound replaced by inSound (referenced), then its reference dropped (as the original)
    static void KeepSound(EEEffect* inEffect, SESMInstanceNode* inSound)
    {
        if (inSound != inEffect->mSound) {
            if (inEffect->mSound)
                inEffect->mSound->Dereference();
            inEffect->mSound = inSound;
            if (inSound)
                inSound->Reference();
        }
        if (inEffect->mSound)
            inEffect->mSound->Dereference();
    }
    // the energy wall's posts on a gate: its two end parts (EEE3DEnergyWall::Place, 0xed784)
    static bool WallGatePosts(void* inAsk, EeGe3DPoint* outFirst, EeGe3DPoint* outSecond)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        if (theUnit->GetClass()->mCategory != kUnitCategoryGate)
            return false;
        EEUnitClass* theGate = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->GetClass());
        EEComplexUnit* theFirst = theGate->GetPart(theUnit, 0);
        EEComplexUnit* theSecond = theGate->GetPart(theUnit, 4);
        GE3DPoint theFrom;
        GE3DPoint theTo;
        theFirst->GetPosition(theFrom);
        theSecond->GetPosition(theTo);
        *outFirst = R(theFrom);
        *outSecond = R(theTo);
        return true;
    }
    // the height of the last living, finished wall piece or gate side part of the wall's owner on a tile (0xee00c)
    static bool WallPiece(void* inAsk, int32_t inX, int32_t inY, float* outZ)
    {
        unsigned long theOwner = E(inAsk)->mUnit->mComplexUnit->mPlayer->mPlayerID;
        bool theFound = false;
        std::vector<EEUnit*> theUnits;
        EEGetUnitsOnTile(inX, inY, theUnits);
        for (unsigned long j = 0; j < theUnits.size(); j++) {
            EEComplexUnit* theOther = dynamic_cast<EEComplexUnit*>(theUnits[j]);
            if (!theOther || theOther->mPlayer->mPlayerID != theOwner || !(theOther->mHitPoints > 0) ||
                theOther->IsFoundation())
                continue;
            long theType = theOther->GetClass()->mUnitType;
            if (theType == kUnitTypeWallSpace
                || (theType == kUnitTypeGateSpace && theOther->GetClass()->IsSidePart(theOther))) {
                GE3DPoint thePosition;
                theOther->GetPosition(thePosition);
                *outZ = thePosition.GetZ();
                theFound = true;
            }
        }
        return theFound;
    }
    static void TracerHit(void* inAsk)
    {
        EEUnit* theTarget = E(inAsk)->mTarget;
        theTarget->mUnitClass->UnitDamaged(theTarget->mComplexUnit, NULL, 0);
    }
    // the volcano's model over the record's mesh lists (EEE3DVolcano::BuildModel, 0xfe79d): each tile of its area
    // split 3x3, following the terrain, coloured by terrain and lighting and blended into textures\Volcano.png near
    // the centre, and the cone heights from the image's alpha (ee_gfx volcano.rs); the model made and referenced,
    // placed at its centre, the bounding radius to the cone's top, the texture
    static int32_t VolcanoBuild(void* inAsk, EeGe3DPoint* outPoints, float* outHeights, uint32_t inCount)
    {
        EEEffect* theEffect = E(inAsk);
        EeGfxEffectVolcano& theVolcano = theEffect->mRecord->volcano;
        GETransformation theTransformation;
        GE3DPoint theTop;
        FSFileSpec* theFile = NULL;
        GESurface* theSurface = NULL;
        GESurfaceDescription theDescription;
        float theHeightScale = theEffect->mGraphicEffect->mSize;
        EeGfxVolcanoSizes theSizes = ee_gfx_volcano_sizes(theVolcano.area.right - theVolcano.area.left);
        std::vector<EeGfxColor> theColors(theSizes.vertices);
        theDescription.mWidth = 19;
        theDescription.mHeight = 19;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mPixelFormat = kPixelFormatA8R8G8B8;
        theDescription.mSurfaceVolatility = 0;
        theDescription.m14 = 0;
        theDescription.mMipLevelCount = 0;
        theDescription.mType = kSurfaceSystemMemory;
        theDescription.mPixelDepth = 32;
        long theResult = gRasterizer->GetScreen(gScreenIndex)->CreateSurface(theDescription, &theSurface);
        ESVERIFY_AT(theResult == keseOK, 233);

        UString theFileName = UString("textures\\Volcano.png");   // the original's .tga as PNG (RUST_PLAN D9)
        theResult = gTextureManager->GetTexturePath()->FindFile(theFileName, theFile);
        ESVERIFY_AT(theResult == keseOK, 238);
        ESVERIFY_AT(theFile != NULL, 239);
        theSurface->Load(*theFile, kImageFilePNG);
        ESVERIFY_AT(theResult == keseOK, 241);
        if (theFile != NULL)
            delete theFile;
        theSurface->Lock();
        const uint32_t* thePixels = reinterpret_cast<const uint32_t*>(theSurface->GetMemoryAddress());
        ee_gfx_volcano_image(theHeightScale, theSizes.side, thePixels, theSurface->GetBytesPerRow() / 4,
                             theColors.empty() ? NULL : &theColors[0], inCount == 0 ? NULL : outHeights, theColors.size());
        theSurface->Unlock();
        theSurface->Dereference();
        GEModel* theModel = gModelManager->CreateModel(theSizes.polygons, theSizes.vertices, 1, 0, 1, 0, "volcano model", 1);
        theEffect->mVolcanoModel = theModel;
        theModel->Reference();
        theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
        theModel->GetMaterial(0)->SetPolygonRangeLength(theSizes.polygons, 0);
        theModel->GetMaterial(0)->SetVertexStartingIndex(0);
        theModel->GetMaterial(0)->SetVertexRangeLength(theSizes.vertices);
        EeGfxVolcanoTerrain theTerrain;
        theTerrain.context = ::gWorld.mMap;
        theTerrain.z = EEVolcanoZ;
        theTerrain.color = EEVolcanoColor;
        theTerrain.valid_tile = EEVolcanoValidTile;
        theTerrain.light = EEVolcanoLight;
        EEEffectMesh theMesh(theSizes.vertices, theSizes.polygons);
        EeGfxVolcanoArea theArea = theVolcano.area;
        EeGfxVolcanoBuilt theBuilt = ee_gfx_volcano_build(
            &theArea, theColors.empty() ? NULL : &theColors[0], theTerrain, inCount == 0 ? NULL : outPoints,
            inCount == 0 ? NULL : outHeights, theColors.size(), theMesh.Vertices(), theMesh.mVertices.size(),
            theMesh.Triangles(), theMesh.mTriangles.size());
        theMesh.Store(theModel);
        theVolcano.area.center.z = theBuilt.center_z;
        float theMaximum = theBuilt.maximum;
        theModel->ComputeModelNormals();
        theModel->ComputeBoundingVolumes();
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->PrepareTransforms();
        theTransformation.SetPosition(theModel->GetCenterPoint().GetX(), theModel->GetCenterPoint().GetY(),
                                      theModel->GetCenterPoint().GetZ());
        theEffect->SetTransformation(theTransformation);
        theTop.SetX(theModel->GetCenterPoint().GetX());
        theTop.SetY(theModel->GetCenterPoint().GetY());
        theTop.SetZ(theHeightScale + theMaximum);
        float theRadius = theModel->GetCenterPoint().DistanceTo(theTop);
        if (theRadius <= theModel->GetBoundingRadius(0))
            theRadius = theModel->GetBoundingRadius(0);
        theEffect->mRecord->bounding_radius = theRadius;
        if (theResult == kErrorOK)
            theModel->GetMaterial(0)->SetTexture(theEffect->mGraphicObject->mTextures[0], gScreenIndex);
        return (int32_t)theResult;
    }
    static EeGfxFarmTerrain FarmTerrain(void*)
    {
        EeGfxFarmTerrain theTerrain;
        theTerrain.context = &gWorld.mMap->mTerrainMesh;
        theTerrain.z = EEFarmZ;
        theTerrain.normal = EEFarmNormal;
        theTerrain.mesh_z = EEFarmMeshZ;
        return theTerrain;
    }
    static bool FarmDropSite(void* inAsk, int32_t* outX, int32_t* outY)
    {
        std::vector<EEUnit*> theUnits;
        if (!EEGetFarmDropSites(E(inAsk)->mUnit->mComplexUnit, theUnits))
            return false;
        *outX = (int32_t)theUnits[0]->mComplexUnit->GetTileX();
        *outY = (int32_t)theUnits[0]->mComplexUnit->GetTileY();
        return true;
    }
    static void FarmSetVariant(void* inAsk, int32_t inVariant) { E(inAsk)->mUnit->mComplexUnit->mCloaked = inVariant; }
    static bool FarmDoppelgangerAny(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        return gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit) == true;
    }
    static int32_t FarmDoppelganger(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        EEDoppelganger* theDoppelganger = NULL;
        if (theUnit->mTile)
            theDoppelganger = gDoppelgangerManager.GetDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, theCurrent->mPlayerID);
        if (theDoppelganger && !gFreeCamera && !theCurrent->mSeesAll)
            return theDoppelganger->IsBuilt() ? 1 : 0;
        return -1;
    }
    static float FarmOpacity(void* inAsk, float inOpacity)
    {
        Fn23CEE2(E(inAsk)->mUnit->mComplexUnit, false, inOpacity);
        return inOpacity;
    }
    static void SetScaledModel(void* inAsk, void* inModel)
    {
        E(inAsk)->mUnit->mComplexUnit->SetScaledModel(static_cast<GEModel*>(inModel));
    }
    // the earthquake's (EEE3DEarthquake, 0xec6e4..0xed606): the ground under a point
    static EeGe3DPoint GroundPoint(void*, const EeGe3DPoint* inPoint)
    {
        GE3DPoint theGround(inPoint->x, inPoint->y, inPoint->z);
        gWorld.mMap->GetGroundPoint(theGround, theGround);
        return R(theGround);
    }
    // the rain's (EEE3DRain::Virtual64, 0xf7792): its three sound objects' media
    static void RainMedia(void* inAsk)
    {
        for (int i = 0; i < 3; i++) {
            EESoundObject* theSound = EESoundObject::Get(E(inAsk)->mRecord->storm.sounds[i]);
            if (theSound)
                theSound->LoadMedia();
        }
    }
    // the rain's sounds (EEE3DRain::StartSounds, 0xf8195, after its thunder timer): the loop at the start or after a
    // load, the thunder inRumble (1 or 2), both levels
    static void RainSounds(void* inAsk, bool inStart, int32_t inRumble)
    {
        EEEffect* theRain = E(inAsk);
        const uint32_t* theIDs = theRain->mRecord->storm.sounds;
        EESoundObject* theSound;
        if (inStart && (theSound = EESoundObject::Get(theIDs[0])) != NULL)
            EESetEffectSound(theRain->mSound, gSoundManager->PlayAndReturnReferencedNode(
                theSound->GetSampleID(), theSound->GetVolume(), 0.0f, 1.0f, theSound->GetLooping(), "3DRain"));
        if (inRumble != 0 && (theSound = EESoundObject::Get(theIDs[inRumble])) != NULL)
            EESetEffectSound(theRain->mRumble, gSoundManager->PlayAndReturnReferencedNode(
                theSound->GetSampleID(), theSound->GetVolume(), 0.0f, 1.0f, theSound->GetLooping(), "3D Rain Rumble"));
        EESetRainSoundLevel(theRain, theRain->mSound, 0.5f);
        EESetRainSoundLevel(theRain, theRain->mRumble, 1.0f);
    }
    // the particles' tick and ground (EEE3DParticle::Update 0xf50b2, EEE3DParticleEmitter::Move 0xf60f5), the graphic
    // effect's duration (EEE3DParticle::Load 0xf53bc)
    static EeGfxParticleTick ParticleTick(void* inAsk, const EeGe3DPoint* inPosition)
    {
        return EEParticleTick(E(inAsk)->mGraphicEffect, *reinterpret_cast<const GE3DPoint*>(inPosition));
    }
    static EeGfxParticleGround ParticleGround(void*) { return EEParticleGround(); }
    // the emitter on a unit (0xf5f23): at its location unless it follows the tag point; a unit without a complex unit
    // at its tile's centre on the terrain
    static void EmitterPlace(void* inAsk)
    {
        EEEffect* theEffect = E(inAsk);
        EEUnit* theOwner = theEffect->mUnit;
        GETransformation theTransformation;
        EEComplexUnit* theUnit = theOwner->GetComplexUnit();
        if (theUnit) {
            if (!theEffect->mGraphicEffect->mFlag1)
                theTransformation = theUnit->mPhysicalModel.GetLocation();
        } else {
            GE3DPoint theLocation;
            float theHalf = theOwner->mUnitClass->mTileSize * 0.5f;
            theLocation.SetX(theOwner->mTileX + theHalf);
            theLocation.SetY(theOwner->mTileY + theHalf);
            if (::gWorld.mMap->mTerrainMesh.GetIsValidTile(theOwner->mTileX, theOwner->mTileY))
                theLocation.SetZ(::gWorld.mMap->GetHeight(theLocation));
            else
                theLocation.SetZ(0.0f);
            theTransformation.MakeIdentity();
            theTransformation.SetPosition(theLocation);
        }
        theEffect->SetTransformation(theTransformation);
    }
    // the emitter's animation sound at its place (its graphic effect's object, if any)
    static void EmitterSound(void* inAsk, const EeGe3DPoint* inLocation, bool inFirst)
    {
        EEEffect* theEffect = E(inAsk);
        EEGraphicObject* theObject = theEffect->mGraphicEffect->mGraphicObject;
        if (theObject)
            EESetEffectSound(theEffect->mSound, PlayAnimationSound(*reinterpret_cast<const GE3DPoint*>(inLocation), theObject, 0,
                                                                   theEffect->mSound, 0, inFirst));
    }
    // the 2D kinds' answers
    // the goal a state-0 confirmation shows: the last goal of the unit's goal queue as a location goal (on the tile
    // at the effect: blocked when the unit cannot stand there and its player sees it; forced; a move), else whether
    // its active goal unloads a transport (EEE3DCommandConfirmation::SetUp)
    static EeGfxConfirmGoal ConfirmGoal(void* inAsk, uint32_t inGoalIndex, int32_t inTileX, int32_t inTileY)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        if (!theUnit || !theUnit->mComplexUnit)
            return EeGfxConfirmGoal::NoUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        EEUGRust* theGoal = theComplex->mGoalQueues[inGoalIndex].mGoals.size() == 0
                                ? NULL : EELocationGoalCast(theComplex->mGoalQueues[inGoalIndex].mGoals.back());
        if (theGoal) {
            EETileCoordinate theTile(inTileX, inTileY);
            EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX, theTile.mY);
            if (EECanStandOn(theTile, theComplex) == false && theMapTile && theMapTile->IsVisibleTo(theUnit->mPlayer))
                return EeGfxConfirmGoal::Blocked;
            return theGoal->GetIsForcedMove() ? EeGfxConfirmGoal::Forced : EeGfxConfirmGoal::Move;
        }
        EEUnitGoal* theLoadGoal = theComplex->mActiveGoal;
        return theLoadGoal && theLoadGoal->mKind == kGoalUnloadTransport ? EeGfxConfirmGoal::Unloading
                                                                          : EeGfxConfirmGoal::Other;
    }
    // the unit's weapon tag point (ee_combat's weapon_point; EEE2DMuzzleFlash::SetUp 0xe458e: site 3, the list
    // assumed; sites 1 and 2 (the 2D projectile) test it)
    static EeGe3DPoint WeaponPoint(void* inAsk, int32_t inSite)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEModel* theModel = theUnit->GetModel();
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        EETagPointList* theList = theUnit->GetModel()->mTagPoints[theComplex->mAnimation];
        bool theHasList = inSite <= 2 ? theList && theList->mPoints.size() : true;
        GE3DPoint theStart;
        EECombatEffectWeaponPoint(inSite, theComplex, theModel, theList, theHasList, theStart);
        return R(theStart);
    }
    // the kept object's (inPart0: the first part's) animation sound for the frame at the location, kept as the
    // effect's (EEE2DImpact::Step and SetSound, 0xe166b / 0xe17ed)
    static void PartSound(void* inAsk, EeGe3DPoint inLocation, uint32_t inFrame, bool inChanged, bool inPart0)
    {
        EEEffect* theEffect = E(inAsk);
        GE3DPoint theLocation(inLocation.x, inLocation.y, inLocation.z);
        EEGraphicObject* theObject =
            inPart0 ? P<EEGraphicObject>(ee_gfx_effect_part_object(H(theEffect), 0)) : theEffect->mGraphicObject;
        KeepSound(theEffect, PlayAnimationSound(theLocation, theObject, 0, theEffect->mSound, inFrame, inChanged));
    }
    static bool DrawBlood(void*) { return gDrawBlood; }
    static bool TileBlocked(void*, int32_t inX, int32_t inY) { return gWorld.IsTileBlocked(inX, inY); }
    static EeGe3DPoint Wind(void*)
    {
        const GEVector& theWind = gWorld.mWindDirection;
        return EeGe3DPoint{theWind.mX, theWind.mY, theWind.mZ};
    }
    static bool AreaEffect(void* inAsk, int32_t inKind, float inX, float inY, uint32_t inPlayer)
    {
        return gAEManager.HasEffect(inKind, inX, inY, inPlayer, false, &E(inAsk)->mUnit->mComplexUnit->mAreaEffects);
    }
    static int32_t AreaEffects(void* inAsk) { return (int32_t)E(inAsk)->mUnit->mComplexUnit->mNumAreaEffects; }
    static void SetAreaEffects(void* inAsk, int32_t inCount)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        theUnit->mNumAreaEffects = inCount;
        if (inCount == 0)
            theUnit->FnE123D();
    }
    // the unit's type is in its player's active group; a town centre or capitol also with 0x23 or 0x29 there
    static bool IconsSelected(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        unsigned long theType = theUnit->mUnitClass->mUnitType;
        EESelectedGroup* theGroup = theUnit->mPlayer->mActiveGroup;
        bool theSelected = theGroup ? theGroup->HasUnitType(theType) : false;
        if (theGroup != NULL && (theType == kUnitTypeTownCenter || theType == kUnitTypeCapitol)) {
            if (theGroup->HasUnitType(0x23) || theGroup->HasUnitType(0x29))
                theSelected = true;
        }
        return theSelected;
    }
    static EeGfx2DPoint GroundAt(void*, EeGe3DPoint inAt)
    {
        EeGfx2DPoint thePoint = {};
        EEMap* theMap = gWorld.mMap;
        GE3DPoint theLocation(inAt.x, inAt.y, inAt.z);
        GE3DPoint theGround;
        if (!theMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY())))
            return thePoint;
        theMap->GetGroundPoint(theLocation, theGround);
        thePoint.has = true;
        thePoint.point = R(theGround);
        return thePoint;
    }
    static EeGfxGlowArea GlowArea(void* inAsk)
    {
        GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
        unsigned long theX, theY, theWidth, theHeight;
        theMesh.GetTerrainVisibleBottomLeftIndices(theX, theY);
        theMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
        EEUnit* theUnit = E(inAsk)->mUnit;
        return EeGfxGlowArea{(uint32_t)theX, (uint32_t)theY, (uint32_t)theWidth, (uint32_t)theHeight,
                             (int32_t)theUnit->mTileX, (int32_t)theUnit->mTileY};
    }
    static uint32_t GlowRows(void*) { return (uint32_t)gWorld.GetMap()->mTerrainMesh.GetNumVisibleTileRows(); }
    // the particle's (EEE2DParticle::Update 0xe4c31, Step 0xe520d)
    static EeGfxParticleTick PositionTick(void* inAsk) { return EEParticleTick(E(inAsk)->mGraphicEffect, E(inAsk)->Position()); }
};

// a shot's start: the unit's weapon point (ee_combat's weapon_point; inMode 6 the pulse cannon's, 7 the tracers');
// whether the unit's model has tag points at its animation
bool ShotStart(long inMode, EEUnit* inUnit, EeGe3DPoint& outStart)
{
    GE3DPoint theStart;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->GetAnimation()];
    bool theTagged = theList != NULL && theList->GetNumTagPoints() != 0;
    EECombatEffectWeaponPoint(inMode, theUnit, theModel, theList, theTagged, theStart);
    outStart = R(theStart);
    return theTagged;
}

// the unit's mouth through the transformation, with the offsets of the cavalry archer, chariot archer, bronze spear
// cavalry and knight (EEE2DWinterBreath::GetMouthPoint)
GE3DPoint EEMouthPoint(EEUnit* inUnit, const GETransformation& inTransformation)
{
    GE3DPoint theOffset(0.0f, 0.4f, 0.405f);
    GE3DPoint theResult;
    switch (inUnit->mUnitClass->mUnitType) {
    case kUnitTypeArch06CavalryArcher:
        theOffset.SetY(0.465f);
        theOffset.SetZ(0.385f);
        break;
    case kUnitTypeArch04ChariotArcher:
        theOffset.SetY(0.872f);
        theOffset.SetZ(0.465f);
        break;
    case kUnitTypeCav04BronzeSpearCavalry:
    case kUnitTypeCav06Knight:
        theOffset.SetY(0.272f);
        theOffset.SetZ(0.235f);
        break;
    }
    inTransformation.Apply(theOffset, theResult);
    return theResult;
}

// ---- the constructors (the manager's entry points; the original leaves' of the graphic effect's kind)

void Attach(EEEffect* e, EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit)
{
    ee_gfx_effect_attach(H(e), U(inUnit), U(inGraphicEffect), (int32_t)inKind, inKeepsUnit);
}

long Call(EEEffect* e, EeGfxEffect3DOp inOp, void* inArg = NULL) { return ee_gfx_effect_call(H(e), inOp, inArg); }

// the 2D particle's start at a point, moving along inVelocity when given, for its graphic effect's duration from now
void StartParticle(EEEffect* e, const GE3DPoint& inPosition, const GEVector* inVelocity)
{
    ee_gfx_effect_set_position(H(e), R(inPosition));
    e->mRecord->particle.draw_position = R(inPosition);
    if (inVelocity)
        e->mRecord->particle.velocity = EeGe3DPoint{inVelocity->GetX(), inVelocity->GetY(), inVelocity->GetZ()};
    e->mRecord->particle.last_time = gPhysics->GetGameClock();
    e->mRecord->particle.duration = e->mGraphicEffect->mDuration;
}

// at a point: the 2D particle's start (0xe49c7); the battlefield decal's size and corners on the terrain (hidden and
// over off the map or not level, 0xde74f); the others' point for Rust (a 2D kind's sprite.start, a 3D kind's
// aged.location)
void PlaceAt(EEEffect* e, const GE3DPoint& inLocation)
{
    if (e->GetKind() == 0x2e) {
        StartParticle(e, inLocation, NULL);
        return;
    }
    EEGraphicEffect* theEffect = e->mGraphicEffect;
    if (e->GetKind() == 0x32) {
        EeGfxAmbientNew theNew = {theEffect->GetStartSize(), theEffect->GetSize(), (int32_t)theEffect->mIntParam1,
                                  theEffect->mFlag1};
        EeGfxParticleGround theGround = EEParticleGround();
        ee_gfx_effect_ambient_new(e->mRecord, reinterpret_cast<const EeGe3DPoint*>(&inLocation), &theNew, &theGround);
        return;
    }
    (e->mIs2D ? e->mRecord->sprite.start : e->mRecord->aged.location) = R(inLocation);
    Call(e, EeGfxEffect3DOp::CreateAt, const_cast<GE3DPoint*>(&inLocation));
}

// on a unit: the destruction smoke (0x1e) at its location, spread over half its class's scale (0x4ebbfd); the 2D
// particle (0x2e) at it or its tile's centre on the ground (0 off the map) (0xe489b); the multi-effect's own unit is
// none (its linked effects are made on inUnit); the light glow keeps its unit when asked (0xe1a7a), the command
// confirmation shows goal queue inGoalIndex (0xe9d1e), the emitter follows tag point inTagPoint
void OnUnit(EEEffect* e, EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit, long inTagPoint,
            unsigned long inGoalIndex)
{
    if (inKind == 0x1e) {
        const GETransformation& theLocation = inUnit->mComplexUnit->mPhysicalModel.GetLocation();
        Attach(e, NULL, inGraphicEffect, inKind, false);
        PlaceAt(e, GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ()));
        e->mRecord->smoke.radius = inUnit->mUnitClass->mScale * 0.5f;
        return;
    }
    if (inKind == 0x2e) {
        Attach(e, inUnit, inGraphicEffect, inKind, false);
        GE3DPoint theLocation;
        EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
        if (theComplexUnit) {
            const GETransformation& theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
            theLocation = GE3DPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        } else {
            theLocation.SetX(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileX);
            theLocation.SetY(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileY);
            if (gWorld.GetMap()->mTerrainMesh.GetIsValidTile(inUnit->mTileX, inUnit->mTileY))
                theLocation.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
        }
        StartParticle(e, theLocation, NULL);
        return;
    }
    Attach(e, inKind == 0x3b ? NULL : inUnit, inGraphicEffect, inKind, inKind == 0xd && inKeepsUnit);
    if (inKind == 0x41)
        e->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
    Call(e, EeGfxEffect3DOp::Create, inUnit);
    if (inKind == 0x3a)
        e->mRecord->emitter.tag_point = (uint32_t)inTagPoint;
}

// from a unit to a point: the command confirmation there (0xe9d91); the smoke muzzle blast and the 2D projectile from
// the unit's weapon point (0xe6a07, 0xe31dd); the pulse cannon ball from its weapon point (0xf6a26), the citizen beam
// towards it (0xe94b7), the tracer beam to it (0xfc49d)
void UnitToPoint(EEEffect* e, EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind,
                 unsigned long inGoalIndex)
{
    EeGe3DPoint theTarget = R(inTarget);
    if (inKind == 0x14 || inKind == 0x30) {
        Attach(e, NULL, inGraphicEffect, inKind, false);
        GE3DPoint theStart;
        EEModel* theModel = inUnit->GetModel();
        EEComplexUnit* theUnit = inUnit->mComplexUnit;
        EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->mAnimation];
        if (inKind == 0x30)
            EECombatEffectWeaponPoint(2, theUnit, theModel, theList, theList && theList->mPoints.size(), theStart);
        else
            EECombatEffectWeaponPoint(5, theUnit, theModel, theList, true, theStart);
        e->mRecord->sprite.start = R(theStart);
        e->mRecord->sprite.draw_position = theTarget;
        Call(e, EeGfxEffect3DOp::CreateBetween);
        return;
    }
    Attach(e, inUnit, inGraphicEffect, inKind, false);
    if (inKind == 0x41) {
        e->Position() = inTarget;
        e->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
        Call(e, EeGfxEffect3DOp::CreateAt);
        return;
    }
    if (inKind == 0x40) {
        e->mRecord->tracer.target_point = theTarget;
        e->mRecord->tracer.weapon_tag_point = inUnit->mComplexUnit->mWeaponTagPoint;
    } else if (inKind == 0x33) {
        e->mRecord->shot.end = theTarget;
    } else {
        ShotStart(6, inUnit, e->mRecord->shot.path.start);
        e->mRecord->shot.location = theTarget;
    }
    Call(e, EeGfxEffect3DOp::Create);
}

// between two units: the tracer beam of inFrom on inTo, referenced, aimed at a point picked on its model (0xfc3c5: the
// original tested the target for null, then used it); the tracers and shells on inTo from inFrom's weapon point to a
// point of inTo's model in inFrom's player colour (0xfd373); the winter breath at inFrom's mouth tag point (or its
// mouth by unit type, the breath then inert), its scaled location, whether the tag list was found and whether it moves
// (0xe819b); the smoke muzzle blast and the 2D projectile from inFrom's weapon tag point to inTo's model point on its
// scaled location, the blast's 15 higher for a high-arc shooter (0xe6664, 0xe2f58)
void BetweenUnits(EEEffect* e, EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
{
    static const float kTargetLift = 15.0f;
    if (inKind == 0x40) {
        Attach(e, inFrom, inGraphicEffect, inKind, false);
        e->mTarget = inTo;
        if (e->mTarget) {
            e->mTarget->Reference();
            e->mRecord->tracer.target_point = R(e->mTarget->GetModel()->PickPoint());
        }
        e->mRecord->tracer.weapon_tag_point = inFrom->mComplexUnit->mWeaponTagPoint;
        Call(e, EeGfxEffect3DOp::Create);
        return;
    }
    if (inKind == 0x21 || inKind == 0x26) {
        Attach(e, inTo, inGraphicEffect, inKind, false);
        GE3DPoint theEnd;
        GETransformation theToTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
        float theToScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
        if (ShotStart(7, inFrom, e->mRecord->shot.path.start))
            EETagPointList::Scale(theToTransformation, theToScale);
        GE3DPoint theOffset = inTo->GetModel()->PickPoint();
        theToTransformation.Apply(theOffset, theEnd);
        const GEColor& theColor = inFrom->mPlayer->mColor;
        e->mRecord->shot.location = R(theEnd);
        e->mRecord->shot.color = EeGfxColor{theColor.mRed, theColor.mGreen, theColor.mBlue};
        Call(e, EeGfxEffect3DOp::Create);
        return;
    }
    Attach(e, NULL, inGraphicEffect, inKind, false);
    if (inKind == 0x16) {
        GE3DPoint thePosition;
        GETransformation theTransformation(inFrom->mComplexUnit->mPhysicalModel.GetLocation());
        EEComplexUnit* theBreather = inFrom->mComplexUnit;
        EETagPointList* theMouths = inFrom->GetModel()->GetTagPoints(theBreather->GetAnimation(), 0x16);
        float theScale = inFrom->mComplexUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theBreather->mPhysicalModel.GetFrame();
        EETagPointList::Scale(theTransformation, theScale);
        if (theMouths)
            theMouths->GetTagPoint(0)->GetPoint(theFrame, theTransformation, thePosition);
        else
            thePosition = EEMouthPoint(inFrom, theTransformation);
        e->mRecord->sprite.start = R(thePosition);
        e->mRecord->aged.transformation = *reinterpret_cast<const EeGeTransformation*>(&theTransformation);
        e->mRecord->sprite.flag = theMouths != NULL;
        e->mRecord->sprite.shrinking = inFrom->mComplexUnit->mVelocity != 0.0f;
        Call(e, EeGfxEffect3DOp::CreateBetween);
        return;
    }
    bool theProjectile = inKind == 0x30;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theTargetTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = inFrom->GetModel();
    EEComplexUnit* theUnit = inFrom->mComplexUnit;
    EETagPointList* theList = inFrom->GetModel()->mTagPoints[theUnit->mAnimation];
    float theTargetScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
    // the projectile's target point is on its scaled model whatever the shooter has (the same scaling either way)
    EECombatEffectWeaponPoint(theProjectile ? 1 : 4, theUnit, theModel, theList,
                              theProjectile ? theList && theList->mPoints.size() : true, theStart);
    EETagPointList::Scale(theTargetTransformation, theTargetScale);
    theOffset = inTo->GetModel()->mDefaultPoint;
    theTargetTransformation.Apply(theOffset, theEnd);
    if (!theProjectile
        && (inFrom->mUnitClass->mShootsHigh || (inFrom->mStats->mCanFireOverWalls && inFrom->mComplexUnit->mIsFiringHigh)))
        theEnd.SetZ(theEnd.GetZ() + kTargetLift);
    e->mRecord->sprite.start = R(theStart);
    e->mRecord->sprite.draw_position = R(theEnd);
    Call(e, EeGfxEffect3DOp::CreateBetween);
}

// The effect of an entry point (the original's constructors by the entry and the graphic effect's kind): between two
// points the marching line (0xf359c: no set-up call), the linear projectile from the start to the end (0xf2dd6), the
// earthquake's crack (0xec727); along a line the 2D particle at its start with its direction as velocity (0xe4a4a),
// the velocity particle, the 3D particle (0xf4e27) and the emitter (0xf5e82) at its start moving along it, the
// multi-effect's linked effects along it (0xf42c3); the rain and the snow over the tiles, centred on them (0xfb0c8;
// the rain's first thunder, 0xf7394); the water ripple at a transformation (0xe7e50), the battlefield decal at its
// point
EeGfxEffectHead* EEEffectConstruct(const EeGfxEffectArgs* a)
{
    EEEffect* e = Fx(ee_gfx_effect_new());
    EEGraphicEffect* g = P<EEGraphicEffect>(a->graphic_effect);
    long theKind = g->mKind;
    const GE3DPoint& theAt = *reinterpret_cast<const GE3DPoint*>(&a->at);
    switch (a->entry) {
    case EeGfxEffectEntry::Unit:
        OnUnit(e, P<EEUnit>(a->unit), g, theKind, a->keeps_unit, a->tag_point, a->goal_index);
        break;
    case EeGfxEffectEntry::BetweenUnits:
        BetweenUnits(e, P<EEUnit>(a->unit), P<EEUnit>(a->to_unit), g, theKind);
        break;
    case EeGfxEffectEntry::UnitToPoint:
        UnitToPoint(e, P<EEUnit>(a->unit), theAt, g, theKind, a->goal_index);
        break;
    case EeGfxEffectEntry::Placed:
        Attach(e, NULL, g, theKind, false);
        PlaceAt(e, theAt);
        break;
    case EeGfxEffectEntry::Transformation:
        Attach(e, NULL, g, theKind, false);
        if (theKind == kEffect2DBattlefieldAmbient) {
            PlaceAt(e, GE3DPoint(*reinterpret_cast<const GETransformation*>(&a->transformation)));
            break;
        }
        e->mRecord->aged.transformation = a->transformation;
        Call(e, EeGfxEffect3DOp::CreateTransformation);
        break;
    case EeGfxEffectEntry::Line: {
        Attach(e, NULL, g, theKind, false);
        const GE3DLine& theLine = *reinterpret_cast<const GE3DLine*>(&a->line);
        const GEVector& theDirection = theLine.GetDirection();
        EeGe3DPoint theVelocity = {theDirection.GetX(), theDirection.GetY(), theDirection.GetZ()};
        if (theKind == 0x2e) {
            StartParticle(e, theLine.GetStart(), &theDirection);
            break;
        }
        if (theKind == 0xc) {
            e->mRecord->sprite.start = R(theLine.GetStart());
            e->mRecord->sprite.velocity = theVelocity;
        } else if (theKind == 0x39 || theKind == 0x3a) {
            e->mRecord->aged.location = R(theLine.GetStart());
            (theKind == 0x39 ? e->mRecord->particle.velocity : e->mRecord->emitter.velocity) = theVelocity;
        }
        Call(e, EeGfxEffect3DOp::CreateLine, const_cast<GE3DLine*>(&theLine));
        break;
    }
    case EeGfxEffectEntry::Between:
        Attach(e, NULL, g, theKind, false);
        if (theKind == 0x2d) {
            e->mRecord->march.from = a->at;
            e->mRecord->march.to = a->to;
            break;
        }
        e->mRecord->shot.path.start = a->at;
        e->mRecord->shot.location = a->to;
        Call(e, EeGfxEffect3DOp::Create);
        break;
    case EeGfxEffectEntry::Weather:
        Attach(e, NULL, g, theKind, false);
        ee_gfx_storm_new(e->mRecord, a->tiles, a->tile_count);
        Call(e, EeGfxEffect3DOp::Create);
        break;
    }
    return H(e);
}

// the damage controller's tick facts (EEEDamagedEffect::Step, 0x10225c): its graphic effect's, then its unit's (the
// outer part on the map), the damage tag points of linked effect 1's kind, its health, hidden when cloaked to the
// current player's enemy
EeGfxDamagedTick EEDamagedTick(EeGfxEffectHead* inEffect)
{
    EEEffect* theSelf = Fx(inEffect);
    EeGfxDamagedTick theTick = {};
    EEGraphicEffect* theEffect = theSelf->GetGraphicEffect();
    theTick.has_linked2 = theEffect->GetLinkedEffect2() != NULL;
    theTick.param2 = theEffect->GetParam2();
    EEUnit* theUnit = theSelf->mUnit;
    if (theUnit && !theSelf->mRecord->damaged.finished) {
        theTick.has_unit = true;
        if (theUnit->mTileNode) {
            while (theUnit->mComplexUnit->mPreviousPart)
                theUnit = theUnit->mComplexUnit->mPreviousPart;
        }
        EETagPointList* theList;
        if (theUnit->mTileNode)
            theList = theUnit->GetModel()->GetTagPoints(theUnit->mComplexUnit->GetAnimation(),
                                                        theEffect->GetLinkedEffect1()->mTagPointKind);
        else
            theList = theUnit->GetModel()->GetTagPoints(0, theEffect->GetLinkedEffect1()->mTagPointKind);
        theTick.tag_count = theList ? (uint32_t)theList->GetNumTagPoints() : 1;
        theTick.hit_points = (int32_t)theUnit->GetHitPoints();
        theTick.max_hit_points = (int32_t)theUnit->GetStats()->GetMaxHitPoints();
        if (theUnit->GetTileNode()) {
            EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
            if (theComplexUnit->GetIsCloaked() && !gWorld.GetCurrentPlayer()->IsAlly(theComplexUnit->GetPlayer()))
                theTick.hidden = true;
        }
    }
    return theTick;
}

// The goal markers' questions (goal_markers.rs EeGfxGoalHost; the selection is the last player asked; the markers'
// effects are ee_gfx's)
struct EEGoalOps {
    static EESelectedGroup* sGroup;
    static EEComplexUnit* Unit(uint32_t inUnit) { return sGroup->GetUnit(inUnit); }
    static uint32_t CurrentPlayer(void*) { return (uint32_t)::gWorld.mCurrentPlayer; }
    static uint32_t PlayerCount(void*) { return (uint32_t)::gWorld.mNumPlayers; }
    static EeGfxGoalPlayer Player(void*, uint32_t inPlayer)
    {
        EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
        sGroup = thePlayer->mSelectedGroup;
        return EeGfxGoalPlayer{thePlayer->mTechnologies->mCurrentEpoch >= kTechEpochAtomicWW2, thePlayer->mShiftMode,
                               (uint32_t)sGroup->GetNumUnits()};
    }
    static EeGfxGoalUnit Facts(void*, uint32_t inUnit)
    {
        EEComplexUnit* theUnit = Unit(inUnit);
        EEUnitClass* theClass = theUnit->mUnitClass;
        return EeGfxGoalUnit{(uint32_t)theClass->mCategory, (uint32_t)theClass->mUnitType, (int32_t)theClass->mAirTrafficType,
                             theUnit->mTile != NULL, theUnit->mContainer != NULL, (uint32_t)theUnit->GetNumGoalQueues()};
    }
    static uint32_t GoalCount(void*, uint32_t inUnit, uint32_t inQueue)
    {
        return (uint32_t)Unit(inUnit)->mGoalQueues[inQueue].mGoals.size();
    }
    // a goal's kind, automatic waypoint, garrison container or attack target, and location
    static EeGfxGoalStep Goal(void*, uint32_t inUnit, uint32_t inQueue, uint32_t inGoal)
    {
        EEUnitGoal* theGoal = Unit(inUnit)->mGoalQueues[inQueue].mGoals[inGoal];
        EeGfxGoalStep theStep = {};
        theStep.kind = theGoal->mKind;
        if (theGoal->mKind == kGoalLocation) {
            theStep.auto_waypoint = EELocationGoalCast(theGoal)->GetIsAutoWaypoint();
            if (theStep.auto_waypoint)
                return theStep;
        }
        GE3DPoint theTo;
        theGoal->GetGoalLocation(theTo);
        theStep.to = R(theTo);
        EEUGRust* theCast = theGoal->mKind == kGoalLoadGarrison ? EELoadGarrisonGoalCast(theGoal)
                            : theGoal->mKind == kGoalAttackUnit  ? EEAttackUnitCast(theGoal) : NULL;
        if (theCast != NULL) {
            theStep.has_target = true;
            theStep.target = theGoal->mKind == kGoalLoadGarrison ? (void*)theCast->GetLoadContainer()
                                                                 : (void*)theCast->GetAttackTarget();
        }
        return theStep;
    }
    static bool GoalLocation(void*, uint32_t inUnit, uint32_t inQueue, uint32_t inGoal, EeGe3DPoint* outPoint)
    {
        EEUnitGoal* thePrevious = Unit(inUnit)->mGoalQueues[inQueue].GetGoalAt(inGoal);
        if (thePrevious == NULL)
            return false;
        GE3DPoint theFrom;
        thePrevious->GetGoalLocation(theFrom);
        *outPoint = R(theFrom);
        return true;
    }
    static EeGe3DPoint UnitLocation(void*, uint32_t inUnit, bool inOnTile)
    {
        EEComplexUnit* theUnit = Unit(inUnit);
        return R(GE3DPoint((inOnTile ? theUnit : theUnit->mContainer)->mPhysicalModel.GetLocation()));
    }
    // a placed target's outer part; a land building's: the goal's point on its centre's tile corner
    static void* Target(void*, void* inTarget, EeGe3DPoint* ioTo)
    {
        EEUnit* theTarget = static_cast<EEUnit*>(inTarget);
        if (theTarget->mTileNode != NULL) {
            EEComplexUnit* theOuter = theTarget->mComplexUnit;
            while (theOuter->mPreviousPart != NULL)
                theOuter = theOuter->mPreviousPart;
            theTarget = theOuter;
        }
        if (theTarget->mUnitClass->IsLandBuilding()) {
            if (theTarget->mTileNode != NULL) {
                ioTo->x = theTarget->mComplexUnit->mPhysicalModel.GetLocation().GetX();
                ioTo->y = theTarget->mComplexUnit->mPhysicalModel.GetLocation().GetY();
            }
            ioTo->x = ioTo->x + theTarget->mUnitClass->mTileSize * 0.5f - 0.25f;
            ioTo->y = ioTo->y - theTarget->mUnitClass->mTileSize * 0.5f + 0.25f;
        }
        return theTarget;
    }
    static bool Ground(void*, const EeGe3DPoint* inPoint, EeGe3DPoint* outPoint)
    {
        if (::gWorld.mMap->GetTile(UMFloor(inPoint->x), UMFloor(inPoint->y)) == NULL)
            return false;
        GE3DPoint theGround;
        ::gWorld.mMap->GetGroundPoint(GE3DPoint(inPoint->x, inPoint->y, inPoint->z), theGround);
        *outPoint = R(theGround);
        return true;
    }
};
EESelectedGroup* EEGoalOps::sGroup;

// The effects' file over an FSFile (each the FSFile call of that type) and the TEST ONLY captures' memory files
struct EEFileOps {
    static FSFile& File(void* inContext) { return *static_cast<FSFile*>(inContext); }
    static bool IsOpen(void* inContext) { return File(inContext).GetIsOpen(); }
    static int32_t ReadU32(void* inContext, uint32_t* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t ReadI32(void* inContext, int32_t* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t ReadF32(void* inContext, float* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t ReadBool(void* inContext, bool* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t WriteU32(void* inContext, uint32_t inValue) { return (int32_t)File(inContext).Write(inValue); }
    static int32_t WriteI32(void* inContext, int32_t inValue) { return (int32_t)File(inContext).Write(inValue); }
    static int32_t WriteF32(void* inContext, float inValue) { return (int32_t)File(inContext).Write(inValue); }
    static int32_t WriteBool(void* inContext, bool inValue) { return (int32_t)File(inContext).Write(inValue); }
    static EeGfxEffectFile Memory() { return EEEffectFile(*new FSMemoryFile); }
    static const uint8_t* MemoryData(void* inContext, uint32_t* outLength)
    {
        unsigned long theLength = 0;
        static_cast<FSMemoryFile*>(inContext)->GetLength(theLength);
        *outLength = (uint32_t)theLength;
        return static_cast<FSMemoryFile*>(inContext)->GetData();
    }
    static void MemoryRewind(void* inContext) { static_cast<FSMemoryFile*>(inContext)->SetPosition(0, kFileOriginBegin, false); }
    static void MemoryFree(void* inContext) { delete static_cast<FSMemoryFile*>(inContext); }
};

// The capture's records (EESimCaptureRecord) as the manager's captures write them
uint32_t EECapture(EeGfxCaptureOp inOp, uint32_t inTag, const uint8_t* inData, uint32_t inLength)
{
    switch (inOp) {
    case EeGfxCaptureOp::Active:
        return EESimCaptureRecord::Active() ? 1 : 0;
    case EeGfxCaptureOp::Keeps:
        return EESimCaptureRecord::Keeps(inTag) ? 1 : 0;
    case EeGfxCaptureOp::Commit: {
        EESimCaptureRecord theRecord(inTag);
        theRecord.Bytes(inData, inLength);
        theRecord.Commit();
        return 0;
    }
    case EeGfxCaptureOp::Only:
        if (inData != NULL)
            EESimCaptureRecord::Only(reinterpret_cast<const unsigned int*>(inData), inLength);
        else
            EESimCaptureRecord::Only(0);
        return 0;
    case EeGfxCaptureOp::Exclusive:
        EESimCaptureRecord::Exclusive(inTag != 0);
        return 0;
    }
    return 0;
}

// The world the effects and their manager call into (ee_gfx effect_obj.rs EeGfxEffectWorld, ffi_manager.rs
// EeGfxManagerWorld), registered before main
template <class T> void Deref(uint64_t inObject) { P<T>(inObject)->Dereference(); }
template <class T> void Ref(uint64_t inObject) { P<T>(inObject)->Reference(); }

EeGfxEffectWorld EEMakeWorld()
{
    EeGfxEffectWorld w = {};
    w.unit_reference = Ref<EEUnit>;
    w.unit_release = Deref<EEUnit>;
    w.unit_by_id = [](uint32_t inID) { return U(gWorld.GetUnit(inID)); };
    w.graphic_effect_reference = Ref<EEGraphicEffect>;
    w.graphic_effect_release = Deref<EEGraphicEffect>;
    w.graphic_effect_load = [](uint64_t inEffect, bool inOnDemand) { P<EEGraphicEffect>(inEffect)->Load(inOnDemand); };
    w.graphic_effect_unload = [](uint64_t inEffect) { P<EEGraphicEffect>(inEffect)->Unload(); };
    w.graphic_effect_media = [](uint64_t inEffect) {
        return U(static_cast<EEGraphicObject*>(P<EEGraphicEffect>(inEffect)->GetMedia()));
    };
    w.graphic_effect_by_id = [](uint32_t inID) { return U(EEGraphicEffect::Get(inID)); };
    w.graphic_effect_count = []() { return (uint32_t)EEGraphicEffect::GetCount(); };
    w.model_reference = Ref<GEModel>;
    w.model_release = Deref<GEModel>;
    w.media_load = [](uint64_t inObject) { return (int32_t)P<EEGraphicObject>(inObject)->LoadMedia(); };
    w.media_unload = [](uint64_t inObject) { P<EEGraphicObject>(inObject)->UnloadMedia(); };
    w.sound_release = [](uint64_t inNode) {
        gSoundManager->Stop(P<SESMInstanceNode>(inNode), false);
        P<SESMInstanceNode>(inNode)->Dereference();
    };
    w.sound_object_unload = [](uint32_t inID) {
        EESoundObject* theSound = EESoundObject::Get(inID);
        if (theSound)
            theSound->UnloadMedia();
    };
    w.loading_saved_game = []() { return (bool)gLoadingSavedGame; };
    w.regions = []() { return gWorld.mMap != NULL ? gWorld.mMap->mEffectRegions : (EeGfxEffectRegions*)NULL; };
    w.tile_seen = [](int64_t inX, int64_t inY) {
        EEMapTile* theTile = gWorld.mMap->GetTile((long)inX, (long)inY);
        return theTile == NULL ? -1 : theTile->IsSeenBy(gWorld.GetCurrentPlayer()) ? 1 : 0;
    };
    w.doppelganger = [](int64_t inX, int64_t inY, uint64_t inUnit, bool inComplex) {
        EEMapTile* theTile = gWorld.mMap->GetTile((long)inX, (long)inY);
        EEUnit* theUnit = P<EEUnit>(inUnit);
        return gDoppelgangerManager.HasDoppelganger(theTile, inComplex ? theUnit->mComplexUnit : theUnit,
                                                    gWorld.mCurrentPlayer);
    };
    w.in_view = [](const EeGe3DPoint* inPoint, float inRadius) {
        return ee_gfx_effect_in_view(reinterpret_cast<const EeGeViewport*>(&gViewport), gOrthographicView, inPoint, inRadius);
    };
    w.tick = []() {
        return EeGfxEffectTick{gPhysics->GetTickSeconds(), gPhysics->GetTickTime(), (uint32_t)(unsigned long)gPhysics->mGameTime,
                               gFreeCamera, gPhysics->mSpeed, gWorld.GetGameSpeed()};
    };
    w.screen = [](bool inPreferences) {
        return (uint32_t)(inPreferences ? gGamePreferences.mSettings.GetScreenID() : gScreenIndex);
    };
    w.height = [](const EeGe3DPoint* inPoint) { return gWorld.mMap->GetHeight(*reinterpret_cast<const GE3DPoint*>(inPoint)); };
    w.mesh = []() { return (const EeGeTerrainMesh*)gWorld.mMap->mTerrainMesh.GetRust(); };
    w.raster = &gEERustRaster;
    w.unit_graphic = Ask::UnitGraphic;
    w.storm = EeGfxStormHost{NULL, EEStormHostOps::OnMap, EEStormHostOps::IsWater, EEStormHostOps::GroundZ,
                             EEStormHostOps::StepToward, EEStormHostOps::Ripple};
    w.answers3d = EeGfxEffectAnswers3D{Ask::ShotHit, Ask::AnimationSound, Ask::WallGatePosts, Ask::WallPiece, Ask::TracerHit,
                                       Ask::VolcanoBuild, Ask::FarmTerrain, Ask::FarmDropSite, Ask::FarmSetVariant,
                                       Ask::FarmDoppelgangerAny, Ask::FarmDoppelganger, Ask::FarmOpacity,
                                       Ask::SetScaledModel, Ask::GroundPoint, Ask::RainMedia, Ask::RainSounds,
                                       Ask::ParticleTick, Ask::ParticleGround, Ask::EmitterPlace, Ask::EmitterSound};
    w.answers2d = EeGfxEffectAnswers2D{Ask::ConfirmGoal, Ask::WeaponPoint, Ask::PartSound, Ask::DrawBlood,
                                       Ask::TileBlocked, Ask::Wind, Ask::AreaEffect, Ask::AreaEffects, Ask::SetAreaEffects,
                                       Ask::IconsSelected, Ask::GroundAt, Ask::GlowArea, Ask::GlowRows,
                                       Ask::PositionTick, Ask::ParticleGround};
    return w;
}

EeGfxManagerWorld EEMakeManagerWorld()
{
    EeGfxManagerWorld w = {};
    w.construct = EEEffectConstruct;
    w.damaged_tick = EEDamagedTick;
    w.effect_detail = [](int64_t inSet) {
        if (inSet >= 0)
            gEffectDetail = (unsigned long)inSet;
        return (uint32_t)gEffectDetail;
    };
    w.current = []() { return gEffectManager != NULL ? gEffectManager->mRust : (EeGfxEffectManager*)NULL; };
    w.goals = EeGfxGoalHost{NULL, EEGoalOps::CurrentPlayer, EEGoalOps::PlayerCount, EEGoalOps::Player, EEGoalOps::Facts,
                            EEGoalOps::GoalCount, EEGoalOps::Goal, EEGoalOps::GoalLocation, EEGoalOps::UnitLocation,
                            EEGoalOps::Target, EEGoalOps::Ground, NULL, NULL, NULL, NULL};
    w.capture = EECapture;
    w.memory_file = EEFileOps::Memory;
    w.memory_file_data = EEFileOps::MemoryData;
    w.memory_file_rewind = EEFileOps::MemoryRewind;
    w.memory_file_free = EEFileOps::MemoryFree;
    w.save_major = kSaveGameMajorVersion;
    w.save_minor = kSaveGameMinorVersion;
    return w;
}

const EeGfxEffectWorld kWorld = EEMakeWorld();
const EeGfxManagerWorld kManagerWorld = EEMakeManagerWorld();
const bool kRegistered = (EEGfxObjectLayoutRegister(), ee_gfx_effect_world_set(&kWorld), ee_gfx_manager_world_set(&kManagerWorld), true);
}

EeGfxEffectFile EEEffectFile(FSFile& inFile)
{
    EeGfxEffectFile theFile = {&inFile, EEFileOps::IsOpen, EEFileOps::ReadU32, EEFileOps::ReadI32, EEFileOps::ReadF32,
                               EEFileOps::ReadBool, EEFileOps::WriteU32, EEFileOps::WriteI32, EEFileOps::WriteF32,
                               EEFileOps::WriteBool};
    return theFile;
}

// The terrain mesh as the particles' move asks it
EeGfxParticleGround EEParticleGround()
{
    struct Ops {
        static GETerrainMesh& Mesh() { return gWorld.mMap->mTerrainMesh; }
        static bool IsValidTile(void*, uint32_t inX, uint32_t inY) { return Mesh().GetIsValidTile(inX, inY); }
        static bool IsSpaceTile(void*, uint32_t inX, uint32_t inY) { return Mesh().GetIsSpaceTile(inX, inY); }
        static float MeshZ(void*, float inX, float inY) { return Mesh().GetMeshZ(inX, inY); }
        static void MeshNormal(void*, float inX, float inY, EeGe3DPoint* outNormal)
        {
            GEVector theNormal;
            Mesh().GetMeshNormal(inX, inY, theNormal);
            *outNormal = EeGe3DPoint{theNormal.mX, theNormal.mY, theNormal.mZ};
        }
    };
    EeGfxParticleGround theGround = {NULL, Ops::IsValidTile, Ops::IsSpaceTile, Ops::MeshZ, Ops::MeshNormal};
    return theGround;
}

// The move's world reads (both particles) (provisional name; from the deleted EEE2DParticle.cpp)
EeGfxParticleTick EEParticleTick(EEGraphicEffect* inEffect, const GE3DPoint& inPosition)
{
    EeGfxParticleTick theTick;
    const GEVector& theWind = gWorld.mWindDirection;
    theTick.now = gPhysics->mGameClock;
    theTick.game_over = gPhysics->mGameOver;
    theTick.wind.x = theWind.GetX();
    theTick.wind.y = theWind.GetY();
    theTick.wind.z = theWind.GetZ();
    theTick.position = R(inPosition);
    theTick.duration = (uint32_t)inEffect->mDuration;
    theTick.param4 = inEffect->mParam4;
    theTick.param8 = inEffect->mParam8;
    theTick.param9 = inEffect->GetParam9();
    theTick.param10 = inEffect->GetParam10();
    theTick.param11 = inEffect->mParam11;
    theTick.flag2 = inEffect->mFlag2;
    return theTick;
}

// the weather storms' (EEEMStorm's): every tile one step towards its place around the new centre, the storm
// recentred on the terrain there; the tiles' bounds (ee_gfx effect_storm.rs)
void EEEffect::Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4] = {(int32_t)outBounds.mLeft, (int32_t)outBounds.mTop, (int32_t)outBounds.mRight,
                            (int32_t)outBounds.mBottom};
    if (!ee_gfx_effect_storm_move(H(this), (int32_t)inCenter.mX, (int32_t)inCenter.mY, theBounds))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
}

void EEEffect::GetBounds(U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4];
    if (!ee_gfx_storm_bounds(this->mRecord, theBounds))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
}

U2DPoint<long> EEEffect::GetCenterPoint() const
{
    return U2DPoint<long>(this->mRecord->storm.center_x, this->mRecord->storm.center_y);
}

// rain or snow over a set of tiles (their coordinates as x, y pairs)
long EEEffectManager::CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect,
                                          EEEffectHandle* outHandle)
{
    std::vector<int32_t> theTiles;
    for (size_t i = 0; i < inTiles.size(); i++) {
        theTiles.push_back((int32_t)inTiles[i].mX);
        theTiles.push_back((int32_t)inTiles[i].mY);
    }
    EeGfxEffectArgs a = Args(EeGfxEffectEntry::Weather, inEffect);
    a.tiles = theTiles.empty() ? NULL : &theTiles[0];
    a.tile_count = (uint32_t)inTiles.size();
    return this->Create(a, outHandle);
}

void EEEffectCaptureSweep()
{
    ee_gfx_effects_sweep(gEffectManager != NULL ? gEffectManager->mRust : NULL);
}

// Returns the goal at an index of the queue (EEUnitVectorItem's member, out of line after its first user, the goal
// markers' rebuild)
EEUnitGoal* EEUnitVectorItem::GetGoalAt(unsigned long inIndex)
{
    return this->mGoals[inIndex];
}

// The quad batches' arrays (the camera batch's vertices 0x4e8db8, the ground batch's 0x4e8da8 and their polygons
// 0x4e8df0: two triangles a quad); their textures, counts, alpha references, quads, flushes and draws are ee_gfx
// effect_batch.rs's, the shared plane models (0x4e8df4, 0x4e8df8) too. The GE value classes pass as their mirrors.
static GEVertexArray* sEffectVertices[2];
static GEPolygonArray* sEffectPolygons;

// A batch's quad: its four vertices from inIndex on (the raster table's batch_quad; AddEffectQuad)
void EEEffectBatchQuad(uint32_t inWhich, const EeGfxEffectQuad* inQuad, uint32_t inIndex)
{
    const GEColor& theColor = *reinterpret_cast<const GEColor*>(&inQuad->color);
    const GEVector& theNormal = *reinterpret_cast<const GEVector*>(&inQuad->normal);
    for (unsigned long i = 0; i < 4; i++)
        sEffectVertices[inWhich]->SetVertex(theColor, *reinterpret_cast<const GE3DPoint*>(&inQuad->points[i]), theNormal,
                                            *reinterpret_cast<const GETextureCoordinate*>(&inQuad->coords[i]),
                                            inQuad->opacities[i], inIndex + i);
}

// A batch's draw: inCount vertices, half as many polygons (the raster table's batch_draw)
int32_t EEEffectBatchDraw(uint32_t inWhich, uint32_t inCount, void* inTexture)
{
    return (int32_t)gRasterizer->Draw(*sEffectPolygons, inCount / 2, *sEffectVertices[inWhich], inCount,
                                      static_cast<GETexture*>(inTexture), gScreenIndex, gOrthographicView);
}

// Creates the quad batches' shared polygon array (two triangles per quad, 0x200 quads) and both vertex arrays
// (provisional name)
void CreateEffectBatchBuffers()
{
    sEffectPolygons = gRasterizer->CreatePolygonArray(0x400);
    unsigned long j = 0;
    for (unsigned long i = 0; i < 0x200; i++) {
        sEffectPolygons->SetVertexIndices(j, i * 4, i * 4 + 1, i * 4 + 2);
        sEffectPolygons->SetVertexIndices(j + 1, i * 4, i * 4 + 2, i * 4 + 3);
        j += 2;
    }
    sEffectVertices[EE_GFX_BATCH_CAMERA] = gRasterizer->CreateVertexArray(1, 0x800);
    sEffectVertices[EE_GFX_BATCH_GROUND] = gRasterizer->CreateVertexArray(1, 0x800);
}

// Deletes the quad batches' polygon array and vertex arrays (provisional name)
void DeleteEffectBatchBuffers()
{
    if (sEffectPolygons)
        delete sEffectPolygons;
    for (int i = 0; i < 2; i++) {
        if (sEffectVertices[i])
            delete sEffectVertices[i];
    }
    sEffectPolygons = NULL;
    sEffectVertices[0] = sEffectVertices[1] = NULL;
}

// Creates and releases the two shared quad models (provisional names)
void CreatePlaneModels() { ee_gfx_effect_planes_create(&gEERustRaster); }
void ReleasePlaneModels() { ee_gfx_effect_planes_release(&gEERustRaster); }

// Places up to N shell-hole decals of the unit's class around a point (ambient effects on; ee_gfx
// effect_placement.rs over the manager's list); the first one renews a decal already on that spot instead, unless
// its scatter roll moves it
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation)
{
    EEGraphicEffect* theEffect;
    if (gAmbientEffectsEnabled && (theEffect = inUnit->mUnitClass->mShellHoleEffect) != 0)
        ee_gfx_effects_decal_impact(U(theEffect), R(inLocation));
}

// Places the unit class's blood-stain decal under the unit if ambient effects and blood are on and its roll passes
// (moved forward along the heading for giraffes and horses)
void EEPlaceTrackDecal(EEComplexUnit* inUnit)
{
    EEGraphicEffect* theEffect;
    if (!gAmbientEffectsEnabled || !gDrawBlood || (theEffect = inUnit->mUnitClass->mBloodStainEffect) == 0)
        return;
    GE3DPoint theLocation;
    inUnit->GetPosition(theLocation);
    unsigned long theType = inUnit->mUnitClass->mUnitType;
    const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
    ee_gfx_effects_decal_track(U(theEffect), R(theLocation),
                               theType == kUnitTypeResGiraffe ? EE_GFX_TRACK_GIRAFFE : theType == kUnitTypeResHorse ? EE_GFX_TRACK_HORSE : 0,
                               theTransformation.mMatrix[0][0], theTransformation.mMatrix[1][0]);
}
