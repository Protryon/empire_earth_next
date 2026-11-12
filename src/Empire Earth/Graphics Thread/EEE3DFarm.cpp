// C:\EEX\Empire Earth\Graphics Thread\EEE3DFarm.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DFarm-Data"). 0xee5c0..0xf1327: the key chain, the circle table's chain, ctors .. Virtual50, then
// vector<U2DPoint<float> >::push_back; EEE3DHurricane.cpp follows.
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
#include "lle/GEPolygonArray.h"
#include "lle/GEModelManager.h"
#include "lle/U2DPoint.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEE3DFarm.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEEffectMesh.h"   // ee_gfx farm.rs (RUST_PLAN R4.4)

// 0x4bd920 (.data): the crop variant of a tile by (x / 2 % 4, y / 2 % 4) (provisional name)
static long sFarmTileVariant[4][4] = {{0, 1, 2, 4}, {4, 3, 0, 2}, {1, 2, 4, 0}, {2, 0, 3, 1}};
// 0x4bd960 / 0x4bd974 (.data): by the unit's mCloaked (0..4) (provisional names)
static long sFarmCropKind[5] = {0, 10, 6, 3, 1};
static long sFarmCropFrame[5] = {13, 7, 8, 11, 9};

// SYNTHETIC: EXE 0x4ee5c0
// SYNTHETIC: EXE 0x4ee5ca
// SYNTHETIC: EXE 0x4ee5db
// SYNTHETIC: EXE 0x4ee5e7
const UString EEE3DFarm::sData("EEE3DFarm-Data");

// SYNTHETIC: EXE 0x4ee603
// SYNTHETIC: EXE 0x4ee60d
// SYNTHETIC: EXE 0x4ee625
// SYNTHETIC: EXE 0x4ee631
std::vector<U2DPoint<float> > EEE3DFarm::sCircle;
bool EEE3DFarm::sCircleBuilt;

// FUNCTION: EXE 0x4ee64c
// Creates the farm field effect on a farm unit: places its tile area and builds the placeholder ground and crop models
EEE3DFarm::EEE3DFarm(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DFarm)
{
    this->PlaceArea();
    this->mGroundModel = NULL;
    this->mGroundCopy = NULL;
    this->mCropsCopy = NULL;
    this->mBB4 = false;
    this->mRebuild = true;
    this->mHasCrops = false;
    this->mDrawCrops = false;
    this->mDoppelganger = false;
    this->mDrawOpacity = 1.0f;
    this->mDrawShown = false;
    this->BuildGroundModels();
}

// SYNTHETIC: EXE 0x4ee6e6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ee702
// Creates an empty farm field for loading from a saved game, with no models
EEE3DFarm::EEE3DFarm()
    : EEEffect3D(kEffect3DFarm)
{
    this->mGroundModel = NULL;
    this->mGroundCopy = NULL;
    this->mCropsCopy = NULL;
    this->mHasCrops = false;
    this->mDrawCrops = false;
    this->mSwayA = 0;
    this->mSwayB = 0;
    this->mDoppelganger = false;
    this->mDrawOpacity = 1.0f;
    this->mDrawShown = false;
}

// FUNCTION: EXE 0x4ee775
// Destroys the farm field, unloading its graphic and releasing its ground model and both draw copies
EEE3DFarm::~EEE3DFarm()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    if (this->mGroundModel)
        this->mGroundModel->Dereference();
    if (this->mGroundCopy)
        this->mGroundCopy->Dereference();
    if (this->mCropsCopy)
        this->mCropsCopy->Dereference();
}

long EEE3DFarm::DrawPass4()
{
    return 0;
}

long EEE3DFarm::DrawPass5()
{
    return 0;
}

long EEE3DFarm::DrawPass6()
{
    return 0;
}

// the empty ReplaceUnit: ICF-folded into 0x163d6f
void EEE3DFarm::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
}

// FUNCTION: EXE 0x4ee7c8
// Builds the farm's ground model; returns its result
long EEE3DFarm::BuildModels()
{
    return this->BuildGroundModel();
}

static long FarmTileVariant(long inX, long inY);

// the terrain's questions and the field for ee_gfx farm.rs
static float EEFarmZ(void* inMesh, uint32_t inX, uint32_t inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetZ(inX, inY);
}
static EeGeVector EEFarmNormal(void* inMesh, uint32_t inX, uint32_t inY)
{
    const GEVector* theNormal = static_cast<GETerrainMesh*>(inMesh)->GetVertexNormal(inX, inY);
    EeGeVector theResult = {theNormal->GetX(), theNormal->GetY(), theNormal->GetZ()};
    return theResult;
}
static float EEFarmMeshZ(void* inMesh, float inX, float inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetMeshZ(inX, inY);
}
static EeGfxFarmTerrain EEFarmTerrain()
{
    EeGfxFarmTerrain theTerrain;
    theTerrain.context = &gWorld.mMap->mTerrainMesh;
    theTerrain.z = EEFarmZ;
    theTerrain.normal = EEFarmNormal;
    theTerrain.mesh_z = EEFarmMeshZ;
    return theTerrain;
}
static EeGfxFarmField EEFarmField(EEE3DFarm* inFarm)
{
    EeGfxFarmField theField;
    theField.left = inFarm->mArea.GetLeft();
    theField.top = inFarm->mArea.GetTop();
    theField.right = inFarm->mArea.GetRight();
    theField.bottom = inFarm->mArea.GetBottom();
    theField.row_length = inFarm->mRowLength;
    theField.tile_size = (float)inFarm->mUnit->GetClass()->GetTileSize();
    return theField;
}
// a model's whole range in its first material
static void EEFarmSetRanges(GEModel* inModel, long inPolygons, long inVertices)
{
    inModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    inModel->GetMaterial(0)->SetPolygonRangeLength(inPolygons, 0);
    inModel->GetMaterial(0)->SetVertexStartingIndex(0);
    inModel->GetMaterial(0)->SetVertexRangeLength(inVertices);
}

// FUNCTION: EXE 0x4ee7cd
// Rebuilds the field's ground mesh over its tiles from the terrain heights and normals (also into the draw copy),
// places it and picks the ground texture from the variant of the farm's first drop site tile. The mesh is ee_gfx
// farm.rs's (RUST_PLAN R4.4)
long EEE3DFarm::BuildGroundModel()
{
    GETransformation theTransformation;
    float theSize = (float)this->mUnit->GetClass()->GetTileSize();
    long thePolygonCount = UMFloor(this->mArea.GetRight() - this->mArea.GetLeft() + 1.0f) *
                           UMFloor(this->mArea.GetRight() - this->mArea.GetLeft() + 1.0f) * 2;
    long theVertexCount = this->mRowLength * this->mRowLength;
    if (this->mGroundModel)
        this->mGroundModel->Dereference();
    this->mGroundModel = gModelManager->CreateModel(thePolygonCount, theVertexCount, 1, 0, 1, 1,
                                            UString("farm ground model"), 0);
    this->mGroundModel->Reference();
    GEModel* theDummy = gModelManager->CreateSubModel(1, 3, 1, 0, 1, 0, 0, 0, this->mGroundModel);
    theDummy->Reference();
    theDummy->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    theDummy->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    theDummy->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    theDummy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), 0);
    theDummy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0001f, 0.0f, 0.0f), 1);
    theDummy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0001f, 0.0f), 2);
    theDummy->GetVertexArray(0)->SetOpacity(0.0f, 0);
    theDummy->GetVertexArray(0)->SetOpacity(0.0f, 1);
    theDummy->GetVertexArray(0)->SetOpacity(0.0f, 2);
    EEFarmSetRanges(theDummy, 1, 3);
    EEFarmSetRanges(this->mGroundModel, thePolygonCount, theVertexCount);
    EEFarmSetRanges(this->mGroundCopy, thePolygonCount, theVertexCount);
    EEEffectMesh theMesh(theVertexCount, thePolygonCount);
    EeGfxFarmField theField = EEFarmField(this);
    EeGe3DPoint thePlace = ee_gfx_farm_ground(&theField, EEFarmTerrain(), theMesh.Vertices(), theMesh.mVertices.size(),
                                              theMesh.Triangles(), theMesh.mTriangles.size());
    theMesh.Store(this->mGroundModel);
    theMesh.Store(this->mGroundCopy);
    theDummy->Dereference();
    this->mGroundModel->ComputeBoundingVolumes();
    this->mGroundCopy->ComputeBoundingVolumes();
    theTransformation.SetPosition(thePlace.x, thePlace.y, thePlace.z);
    this->mGroundCopy->SetGraphicsPostModelTransform(theTransformation);
    this->mGroundCopy->SetGraphicsScale(theSize);
    this->mGroundCopy->PrepareTransforms();
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = this->mGroundModel->GetBoundingRadius(0) * theSize;
    {
        std::vector<EEUnit*> theUnits;
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        if (EEGetFarmDropSites(theUnit, theUnits))
            theUnit->mCloaked = FarmTileVariant(theUnits[0]->mComplexUnit->GetTileX(), theUnits[0]->mComplexUnit->GetTileY());
        else
            theUnit->mCloaked = 0;
        GETexture* theTexture = this->mGraphicObject->mTextures[this->GetCropKind()];
        this->mGroundModel->GetMaterial(0)->SetTexture(theTexture, gScreenIndex);
        this->mGroundCopy->GetMaterial(0)->SetTexture(theTexture, gScreenIndex);
        return 0;
    }
}

// FUNCTION: EXE 0x4ef5dd
// Returns the crop variant (0..4) of a tile from its position in a 4 x 4 pattern of 2-tile blocks
static long FarmTileVariant(long inX, long inY)
{
    return sFarmTileVariant[(inX >> 1) & 3][(inY >> 1) & 3];
}

// FUNCTION: EXE 0x4ef5fa
// Builds the crop rows as a sub-model of the ground: per tile row, upright quads following the terrain, as many
// rows as the effect's first int parameter; textures them with the variant's crop frame. The mesh is ee_gfx
// farm.rs's (RUST_PLAN R4.4)
long EEE3DFarm::BuildCrops()
{
    GETransformation theTransformation;
    float theSize = (float)this->mUnit->GetClass()->GetTileSize();
    unsigned long theRows = (unsigned long)this->mGraphicEffect->mIntParam1;
    long theWidth = UMFloor(this->mArea.GetRight() - this->mArea.GetLeft() + 1.0f);
    long thePolygonCount = theWidth * theWidth * theRows * 2;
    long theVertexCount = theWidth * this->mRowLength * theRows * 2;
    GEModel* theModel = gModelManager->CreateSubModel(thePolygonCount, theVertexCount, 1, 0, 1, 0, 0, 0,
                                                   this->mGroundModel);
    EEFarmSetRanges(theModel, thePolygonCount, theVertexCount);
    EEFarmSetRanges(this->mCropsCopy, thePolygonCount, theVertexCount);
    EEEffectMesh theMesh(theVertexCount, thePolygonCount);
    EeGfxFarmField theField = EEFarmField(this);
    EeGe3DPoint thePlace = ee_gfx_farm_crops(&theField, theRows, this->mGraphicEffect->GetSize(), EEFarmTerrain(),
                                             theMesh.Vertices(), theMesh.mVertices.size(), theMesh.Triangles(),
                                             theMesh.mTriangles.size());
    theMesh.Store(theModel);
    theMesh.Store(this->mCropsCopy);
    theModel->ComputeBoundingVolumes();
    this->mCropsCopy->ComputeBoundingVolumes();
    theTransformation.SetPosition(thePlace.x, thePlace.y, thePlace.z);
    this->mCropsCopy->SetGraphicsPostModelTransform(theTransformation);
    this->mCropsCopy->SetGraphicsScale(theSize);
    this->mCropsCopy->PrepareTransforms();
    GETexture* theTexture = this->mGraphicObject->mTextures[this->GetCropFrame()];
    theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    theModel->GetMaterial(0)->SetTexture(theTexture, gScreenIndex);
    this->mCropsCopy->GetMaterial(0)->SetTexture(theTexture, gScreenIndex);
    return 0;
}

// NEAR: the point is built through two memory floats in the original (frame 0x10)
// FUNCTION: EXE 0x4f01cb
// Fills the shared sway circle with 64 points of the given radius scaled by 1 / size
void EEE3DFarm::BuildCircle(float inSize, float inRadius)
{
    EEE3DFarm::sCircleBuilt = true;
    inRadius = 1.0f / inSize * inRadius;
    for (inSize = 0.0f; inSize < 6.2831855f; inSize += 0.09817477f) {
        float theX = UMCos(inSize) * inRadius;
        float theY = UMCos(1.5707964f - inSize) * inRadius;
        U2DPoint<float> thePoint(theX, theY);
        EEE3DFarm::sCircle.push_back(thePoint);
    }
}

// NEAR: the original keeps the tile x in a memory float and duplicates y on the FP stack
// FUNCTION: EXE 0x4f0247
// Sets the field's tile rectangle, centre and vertex row length from the farm unit's tile and size
void EEE3DFarm::PlaceArea()
{
    EEUnit* theUnit = this->mUnit;
    float theX = (float)theUnit->GetTileX();
    float theY = (float)theUnit->GetTileY();
    float theSize = (float)theUnit->GetClass()->GetTileSize();
    this->mArea = GERectangle(theX, theY, theSize + theX - 1.0f, theSize + theY - 1.0f);
    this->mCenter.SetX((float)((theUnit->GetClass()->GetTileSize() + 1) / 2) + theUnit->GetTileX());
    this->mCenter.SetY((float)((theUnit->GetClass()->GetTileSize() + 1) / 2) + theUnit->GetTileY());
    this->mRowLength = theUnit->GetClass()->GetTileSize() + 1;
}

// FUNCTION: EXE 0x4f02e7
// Copies the ground and crop meshes into their draw copies and queues them in render queue 0 (a doppelganger's
// built state decides the crops); returns 10 when the copies are missing
long EEE3DFarm::Queue(EEEffectQueue* inQueues)
{
    GEVector theNormal;
    GE3DPoint thePoint;
    EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
    GEVertexArray* theFrom = this->mGroundModel->GetVertexArray(0);
    GEVertexArray* theTo = this->mGroundCopy->GetVertexArray(0);
    unsigned long i;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0 && this->mGroundCopy && this->mCropsCopy) {
        EEDoppelganger* theDoppelganger;
        if (this->mUnit->mTile)
            theDoppelganger = gDoppelgangerManager.GetDoppelganger(static_cast<EETile*>(this->mUnit->mTile), this->mUnit, theCurrent->mPlayerID);
        else
            theDoppelganger = NULL;
        if (theDoppelganger && !gFreeCamera && !theCurrent->mSeesAll) {
            this->mDrawCrops = theDoppelganger->IsBuilt();
            this->mDoppelganger = true;
        } else {
            this->mDoppelganger = false;
            this->mDrawCrops = this->mHasCrops;
        }
        if (this->mHasCrops) {
            GEVertexArray* theCrops = this->mGroundModel->GetSubModel(0)->GetVertexArray(0);
            GEVertexArray* theCopy = this->mCropsCopy->GetVertexArray(0);
            for (i = 0; i < theCrops->GetSize(); i++) {
                theCrops->GetPoint(thePoint, i);
                theCopy->SetPoint(thePoint, i);
            }
            this->mCropsCopy->ComputeBoundingVolumes();
        }
        for (i = 0; i < theFrom->GetSize(); i++) {
            theFrom->GetPoint(thePoint, i);
            theTo->SetPoint(thePoint, i);
            theFrom->GetNormal(theNormal, i);
            theTo->SetNormal(theNormal, i);
        }
        this->mGroundCopy->ComputeBoundingVolumes();
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[0][theTexture].push_back(this);
        // port (R5a.3): DrawPass2's reads of the live unit, in its order (the opacity, the free camera's rebuild of
        // a moved field after the copies above, then the drawn test), under the world's lock
        Fn23CEE2(this->mUnit->mComplexUnit, false, this->mDrawOpacity);
        if (gFreeCamera && this->HasMoved() && this->mUnit->GetTileX() != -10000 && this->mUnit->GetTileY() != -10000) {
            this->PlaceArea();
            this->SetUp();
            if (this->mUnit->mComplexUnit->mIsBuilt)
                this->BuildCrops();
        }
        this->mDrawShown = (this->mUnit->mHitPoints > 0 || !gFreeCamera) && this->mUnit->GetTileX() != -10000
                           && this->mUnit->GetTileY() != -10000;
    } else
        theResult = 10;
    return theResult;
}

// FUNCTION: EXE 0x4f04ee
// Creates the ground and crop draw copies, sized for the field, filled with invisible placeholder triangles
void EEE3DFarm::BuildGroundModels()
{
    long i;
    long j;
    this->mGroundCopy = gModelManager->CreateModel(UMFloor((this->mArea.GetWidth() + 1.0f) * UMFloor(this->mArea.GetWidth() + 1.0f)) * 2,
                                          this->mRowLength * this->mRowLength, 1, 0, 1, 0, UString("graphics farm ground model"), 1);
    this->mGroundCopy->Reference();
    for (i = 0; i < this->mRowLength * this->mRowLength; i += 3) {
        this->mGroundCopy->GetPolygonArray(0)->SetVertexIndex1(0, i);
        this->mGroundCopy->GetPolygonArray(0)->SetVertexIndex2(0, i + 1);
        this->mGroundCopy->GetPolygonArray(0)->SetVertexIndex3(0, i + 2);
        this->mGroundCopy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), i);
        this->mGroundCopy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0001f, 0.0f, 0.0f), i + 1);
        this->mGroundCopy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0001f, 0.0f), i + 2);
        this->mGroundCopy->GetVertexArray(0)->SetOpacity(0.0f, i);
        this->mGroundCopy->GetVertexArray(0)->SetOpacity(0.0f, i + 1);
        this->mGroundCopy->GetVertexArray(0)->SetOpacity(0.0f, i + 2);
    }
    this->mGroundCopy->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mGroundCopy->GetMaterial(0)->SetPolygonRangeLength(
        UMFloor((this->mArea.GetWidth() + 1.0f) * UMFloor(this->mArea.GetWidth() + 1.0f)) * 2, 0);
    this->mGroundCopy->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mGroundCopy->GetMaterial(0)->SetVertexRangeLength(this->mRowLength * this->mRowLength);
    this->mGroundCopy->ComputeBoundingVolumes();
    this->mCropsCopy = gModelManager->CreateModel(
        UMFloor(this->mArea.GetWidth() + 1.0f) * UMFloor(this->mArea.GetWidth() + 1.0f) * this->mGraphicEffect->GetIntParam1() * 2,
        this->mGraphicEffect->GetIntParam1() * this->mRowLength * UMFloor(this->mArea.GetWidth() + 1.0f) * 2, 1, 0, 1, 0,
        UString("graphics farm stalk model"), 1);
    this->mCropsCopy->Reference();
    for (j = 0; j < this->mGraphicEffect->GetIntParam1() * this->mRowLength * UMFloor(this->mArea.GetWidth() + 1.0f) * 2; j += 3) {
        this->mCropsCopy->GetPolygonArray(0)->SetVertexIndex1(0, j);
        this->mCropsCopy->GetPolygonArray(0)->SetVertexIndex2(0, j + 1);
        this->mCropsCopy->GetPolygonArray(0)->SetVertexIndex3(0, j + 2);
        this->mCropsCopy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, 0.0f), j);
        this->mCropsCopy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0001f, 0.0f, 0.0f), j + 1);
        this->mCropsCopy->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0001f, 0.0f), j + 2);
        this->mCropsCopy->GetVertexArray(0)->SetOpacity(0.0f, j);
        this->mCropsCopy->GetVertexArray(0)->SetOpacity(0.0f, j + 1);
        this->mCropsCopy->GetVertexArray(0)->SetOpacity(0.0f, j + 2);
    }
    this->mCropsCopy->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mCropsCopy->GetMaterial(0)->SetPolygonRangeLength(
        UMFloor(this->mArea.GetWidth() + 1.0f) * UMFloor(this->mArea.GetWidth() + 1.0f) * this->mGraphicEffect->GetIntParam1() * 2, 0);
    this->mCropsCopy->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mCropsCopy->GetMaterial(0)->SetVertexRangeLength(
        this->mGraphicEffect->GetIntParam1() * this->mRowLength * UMFloor(this->mArea.GetWidth() + 1.0f) * 2);
    this->mCropsCopy->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x4f0aa1
// Returns whether the farm unit's centre tile differs from the field's centre
bool EEE3DFarm::HasMoved()
{
    EEUnit* theUnit = this->mUnit;
    return UMFloor(this->mCenter.GetX()) != (theUnit->GetClass()->GetTileSize() + 1) / 2 + theUnit->GetTileX() ||
           UMFloor(this->mCenter.GetY()) != (theUnit->GetClass()->GetTileSize() + 1) / 2 + theUnit->GetTileY();
}

// FUNCTION: EXE 0x4f0b1c
// Returns whether any tile of a living farm is visible, else whether the current player holds a doppelganger of it
bool EEE3DFarm::IsTileVisible(EEMapTile* inTile)
{
    bool theResult = false;
    if (gWorld.mMap) {
        if (this->mUnit->mHitPoints > 0) {
            unsigned long theSize = this->mUnit->GetClass()->GetTileSize();
            for (unsigned long y = 0; y < theSize && !theResult; y++) {
                for (unsigned long x = 0; x < theSize && !theResult; x++) {
                    EEMapTile* theTile = gWorld.mMap->GetTile(this->mUnit->GetTileX() + x, this->mUnit->GetTileY() + y);
                    if (theTile)
                        theResult = EEEffect3D::IsTileVisible(theTile);
                }
            }
        }
        if (!theResult) {
            long thePlayer = gWorld.mCurrentPlayer;
            EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
            theResult = gDoppelgangerManager.HasDoppelganger(inTile, theUnit, thePlayer);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4f0bdc
// Returns the ground texture index for the farm's crop variant
long EEE3DFarm::GetCropKind()
{
    return sFarmCropKind[this->mUnit->mComplexUnit->mCloaked];
}

// FUNCTION: EXE 0x4f0bf0
// Returns the crop texture index for the farm's crop variant
long EEE3DFarm::GetCropFrame()
{
    return sFarmCropFrame[this->mUnit->mComplexUnit->mCloaked];
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DFarm::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f0c04
// Loads the graphic, rebuilds the ground model and resets the crop sway; crops show once the farm is built
long EEE3DFarm::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult == 0)
        theResult = this->BuildModels();
    this->mHasCrops = this->mUnit->mComplexUnit->mIsBuilt;
    this->mGroundModel->GetSubModel(0)->GetVertexArray(0)->GetSize();
    this->mSwayA = 0;
    this->mPhase = 0.0f;
    this->mSwayB = 0;
    return theResult;
}

// FUNCTION: EXE 0x4f0c71
// Reloads the graphic and reads the field's area, centre and rebuild flag (one more flag is read and cleared)
long EEE3DFarm::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mArea.Load(inFile);
    if (theResult == 0)
        theResult = this->mCenter.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mRebuild);
    if (theResult == 0) {
        theResult = inFile.Read(this->mBB4);
        this->mBB4 = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f0ce2
// Draws the ground (and the crops when shown) at the unit's opacity; restores the render state and returns the last
// draw result. Port (R5a.3): the original read the unit here and rebuilt a moved farm in free camera first; Queue
// does both under the world's lock now (mDrawOpacity, mDrawShown)
long EEE3DFarm::DrawPass2()
{
    long theResult = 0;
    unsigned long theScreen = gScreenIndex;
    float theOpacity = this->mDrawOpacity;
    if (this->mDrawShown) {
        unsigned long theSource;
        unsigned long theDestination;
        gRasterizer->GetBlending(theSource, theDestination, theScreen);
        unsigned long theZBuffer = gRasterizer->GetZBuffer(theScreen);
        float theAlphaRefValue = gRasterizer->GetAlphaTestingRefValue(theScreen);
        if (this->mGraphicEffect->mGraphicObject)
            gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
        else
            gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
        gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        gRasterizer->SetShadingColorSource(0, theScreen);
        if (!this->mDoppelganger)
            SetTerrainLighting();
        else
            DimUnitLighting();
        this->mGroundCopy->SetOpacity(theOpacity);
        this->mCropsCopy->SetOpacity(theOpacity);
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(this->mGroundCopy, theScreen, 1.0f, theSorted);
        if (this->mDrawCrops) {
            bool theSorted = gOrthographicView;
            theResult = gRasterizer->Draw(this->mCropsCopy, theScreen, 1.0f, theSorted);
        }
        gRasterizer->SetAlphaTestingRefValue(theAlphaRefValue, theScreen);
        gRasterizer->SetZBuffer(theZBuffer, theScreen);
        gRasterizer->SetBlending(theSource, theDestination, theScreen);
        gRasterizer->SetShadingColorSource(1, theScreen);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f0f26
// After a load, clamps the area to the farm's size and rebuilds the models (and the crops if built)
long EEE3DFarm::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0) {
        long theSize = this->mUnit->GetClass()->GetTileSize();
        float theFloatSize = (float)theSize;
        this->mRowLength = theSize + 1;
        if (this->mArea.mRight - this->mArea.mLeft > theFloatSize - 1.0f && this->mArea.mBottom - this->mArea.mTop > theFloatSize - 1.0f) {
            this->mArea.SetRight(this->mArea.GetLeft() + theFloatSize - 1.0f);
            this->mArea.mTop = this->mArea.mBottom - theFloatSize + 1.0f;
        }
        this->BuildGroundModels();
        theResult = this->BuildModels();
        this->mHasCrops = this->mUnit->mComplexUnit->mIsBuilt;
        if (this->mHasCrops == true) {
            this->BuildCrops();
            this->mRebuild = false;
        } else
            this->mRebuild = true;
        this->mPhase = 0.0f;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f0fe9
// Writes the base 3D effect (result ignored), the area, centre and both flags; returns the first write error
long EEE3DFarm::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mArea.Save(outFile);
    if (theResult == 0)
        theResult = this->mCenter.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mRebuild);
    if (theResult == 0)
        theResult = outFile.Write(this->mBB4);
    return theResult;
}

// FUNCTION: EXE 0x4f1043
// Ends with a dead farm unless a doppelganger remains; builds the crops once the farm is built and sways the crop
// tops around the shared circle; hands the ground model to the unit
long EEE3DFarm::Step()
{
    unsigned long i;
    long theResult = 0;
    GE3DPoint thePoint;
    GE3DPoint theBase;
    if (!EEE3DFarm::sCircleBuilt)
        EEE3DFarm::BuildCircle((float)this->mUnit->GetClass()->GetTileSize(), this->mGraphicEffect->GetParam3());
    if (this->mUnit->mHitPoints <= 0 && gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(this->mUnit->mTile), this->mUnit) != true) {
        theResult = 7;
    } else {
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        long theSize = theUnit->GetClass()->GetTileSize();
        if (this->mRebuild == true && theUnit->mIsBuilt == true) {
            this->BuildCrops();
            this->mRebuild = false;
        }
        this->mHasCrops = theUnit->mIsBuilt;
        if (this->mHasCrops) {
            GEModel* theCrops = this->mGroundModel->GetSubModel(0);
            if (!this->mGraphicObject->mLoops[this->GetCropFrame()]) {
                this->mPhase = gPhysics->GetTickTime() / this->mGraphicEffect->GetParam2() + this->mPhase;
                if (this->mPhase > 1.0) {
                    this->mSwayB++;
                    this->mSwayA--;
                    this->mPhase -= 1.0;
                    if (this->mSwayA < 0)
                        this->mSwayA = 0x3f;
                    this->mSwayA = this->mSwayA % 64;
                    this->mSwayB = this->mSwayB % 64;
                    unsigned long theCount = theCrops->GetVertexArray(0)->GetSize();
                    for (i = 0; i < theCount; i++) {
                        if (!(i % (this->mRowLength * 2) <= (unsigned long)theSize))
                            continue;
                        theCrops->GetVertexArray(0)->GetPoint(thePoint, i);
                        theCrops->GetVertexArray(0)->GetPoint(theBase, this->mRowLength + i);
                        long theIndex;
                        if ((i & 1) == 1)
                            theIndex = this->mSwayA;
                        else
                            theIndex = this->mSwayB;
                        thePoint.SetX(theBase.GetX() + EEE3DFarm::sCircle[theIndex].mX);
                        thePoint.SetY(theBase.GetY() + EEE3DFarm::sCircle[theIndex].mY);
                        theCrops->GetVertexArray(0)->SetPoint(thePoint, i);
                    }
                    theCrops->ComputeBoundingVolumes();
                }
            }
        }
        theUnit->SetScaledModel(this->mGroundModel);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f128a
// Rebuilds the field (and the crops if built) when a changed terrain rectangle overlaps it
void EEE3DFarm::ElevationChanged(long inLeft, long inBottom, long inRight, long inTop)
{
    GERectangle theRectangle(inLeft, inTop, inRight, inBottom);
    if (!(theRectangle.mLeft <= this->mArea.mRight) || !(theRectangle.mRight >= this->mArea.mLeft) ||
        !(theRectangle.mTop <= this->mArea.mBottom) || !(theRectangle.mBottom >= this->mArea.mTop))
        return;
    this->PlaceArea();
    this->SetUp();
    if (this->mUnit->mComplexUnit->mIsBuilt)
        this->BuildCrops();
}

// FUNCTION: EXE 0x4f12fb
// Appends a point to a vector<U2DPoint<float> > (a COMDAT from <vector>)
