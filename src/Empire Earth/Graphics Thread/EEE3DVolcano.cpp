// C:\EEX\Empire Earth\Graphics Thread\EEE3DVolcano.cpp (__FILE__ at BuildModel 0xfe79d). 0xfe553..0x101c34: the key
// chain, both ctors, the dtor, BuildModel, the virtuals, then the template COMDATs of BuildModel's vectors
// (0x100d4d..0x101c34). Unit flags: the exe's module set.
#include <windows.h>
#include <stdint.h>

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
#include "lle/GERectangle.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEModelManager.h"
#include "lle/Graphics Engine.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEE3DVolcano.h"
#include "ee/EEEffectMesh.h"   // ee_gfx volcano.rs (RUST_PLAN R4.4)

// SYNTHETIC: EXE 0x4fe553
// SYNTHETIC: EXE 0x4fe55d
// SYNTHETIC: EXE 0x4fe56e
// SYNTHETIC: EXE 0x4fe57a
const UString EEE3DVolcano::sData("EEE3DVolcano-Data");

// FUNCTION: EXE 0x4fe596
// Creates a volcano effect over the 6x6 area around a point (its centre offset one tile in y), not yet grown
EEE3DVolcano::EEE3DVolcano(GE3DPoint inLocation, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, kEffect3DVolcano)
{
    this->mArea = GERectangle(inLocation.GetX() - 3.0f, inLocation.GetY() + 3.0f, inLocation.GetX() + 3.0f,
                        inLocation.GetY() - 3.0f);
    this->mCenter.SetX(this->mArea.GetWidth() * 0.5f + this->mArea.GetLeft());
    this->mCenter.SetY(this->mArea.GetHeight() * 0.5f + this->mArea.GetTop() + 1.0f);
    this->mGrowth = 0.0f;
    this->mGrown = false;
    this->mDone = false;
    this->mTime = 0;
    this->mTimeLeft = 0;
}

// SYNTHETIC: EXE 0x4fe6d1
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fe6ed
// Creates an empty volcano effect (for loading)
EEE3DVolcano::EEE3DVolcano()
    : EEEffect3D(kEffect3DVolcano)
{
}

// FUNCTION: EXE 0x4fe757
// Destroys the volcano, unloading its media and releasing its model (dereferenced without a null test)
EEE3DVolcano::~EEE3DVolcano()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    this->mModel->Dereference();
}

// the map's questions for ee_gfx volcano.rs
static float EEVolcanoZ(void* inMap, uint32_t inX, uint32_t inY)
{
    return static_cast<EEMap*>(inMap)->mTerrainMesh.GetZ(inX, inY);
}
static EeGfxColor EEVolcanoColor(void* inMap, uint32_t inX, uint32_t inY)
{
    const GEColor& theColor = EETerrainTable::Get(static_cast<EEMap*>(inMap)->mTerrainMesh.GetTextureSetID(inX, inY))->mColor;
    EeGfxColor theResult = {theColor.mRed, theColor.mGreen, theColor.mBlue};
    return theResult;
}
static bool EEVolcanoValidTile(void* inMap, uint32_t inX, uint32_t inY)
{
    return static_cast<EEMap*>(inMap)->mTerrainMesh.GetIsValidTile(inX, inY);
}
static EeGfxColor EEVolcanoLight(void* inMap, int32_t inX, int32_t inY, int32_t inCorner)
{
    const GEColor& theColor = static_cast<EEMap*>(inMap)->GetVertexColor(inX, inY, inCorner);
    EeGfxColor theResult = {theColor.mRed, theColor.mGreen, theColor.mBlue};
    return theResult;
}

// FUNCTION: EXE 0x4fe79d
// Builds the volcano mesh over its area (each tile split 3x3, following the terrain, coloured by terrain and lighting
// and blended into textures\Volcano.png near the centre) and the cone heights from the image's alpha. The image's
// colours and heights and the mesh are ee_gfx volcano.rs's (RUST_PLAN R4.4)
long EEE3DVolcano::BuildModel()
{
    UString theFileName;
    GETransformation theTransformation;
    GE3DPoint theTop;
    FSFileSpec* theFile = NULL;
    GESurface* theSurface = NULL;
    GESurfaceDescription theDescription;
    float theHeightScale = this->mGraphicEffect->mSize;
    EeGfxVolcanoSizes theSizes = ee_gfx_volcano_sizes(this->mArea.GetWidth());
    this->mPoints.resize(theSizes.vertices, GE3DPoint());
    this->mHeights.resize(theSizes.vertices, 0.0f);
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

    theFileName = UString("textures\\Volcano.png");   // the original's .tga as PNG (RUST_PLAN D9)
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
                         theColors.empty() ? NULL : &theColors[0], this->mHeights.empty() ? NULL : &this->mHeights[0],
                         theColors.size());
    theSurface->Unlock();
    theSurface->Dereference();
    this->mModel = gModelManager->CreateModel(theSizes.polygons, theSizes.vertices, 1, 0, 1, 0, "volcano model", 1);
    this->mModel->Reference();
    this->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mModel->GetMaterial(0)->SetPolygonRangeLength(theSizes.polygons, 0);
    this->mModel->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mModel->GetMaterial(0)->SetVertexRangeLength(theSizes.vertices);
    EeGfxVolcanoArea theArea;
    theArea.left = this->mArea.GetLeft();
    theArea.top = this->mArea.GetTop();
    theArea.right = this->mArea.GetRight();
    theArea.bottom = this->mArea.GetBottom();
    theArea.center.x = this->mCenter.GetX();
    theArea.center.y = this->mCenter.GetY();
    theArea.center.z = this->mCenter.GetZ();
    EeGfxVolcanoTerrain theTerrain;
    theTerrain.context = ::gWorld.mMap;
    theTerrain.z = EEVolcanoZ;
    theTerrain.color = EEVolcanoColor;
    theTerrain.valid_tile = EEVolcanoValidTile;
    theTerrain.light = EEVolcanoLight;
    EEEffectMesh theMesh(theSizes.vertices, theSizes.polygons);
    EeGfxVolcanoBuilt theBuilt = ee_gfx_volcano_build(
        &theArea, theColors.empty() ? NULL : &theColors[0], theTerrain,
        this->mPoints.empty() ? NULL : reinterpret_cast<EeGe3DPoint*>(&this->mPoints[0]),
        this->mHeights.empty() ? NULL : &this->mHeights[0], theColors.size(), theMesh.Vertices(),
        theMesh.mVertices.size(), theMesh.Triangles(), theMesh.mTriangles.size());
    theMesh.Store(this->mModel);
    this->mCenter.SetZ(theBuilt.center_z);
    float theMaximum = theBuilt.maximum;
    this->mModel->ComputeModelNormals();
    this->mModel->ComputeBoundingVolumes();
    this->mModel->SetGraphicsPostModelTransform(theTransformation);
    this->mModel->PrepareTransforms();
    theTransformation.SetPosition(this->mModel->GetCenterPoint().GetX(), this->mModel->GetCenterPoint().GetY(),
                                  this->mModel->GetCenterPoint().GetZ());
    this->SetTransformation(theTransformation);
    theTop.SetX(this->mModel->GetCenterPoint().GetX());
    theTop.SetY(this->mModel->GetCenterPoint().GetY());
    theTop.SetZ(theHeightScale + theMaximum);
    float theRadius = this->mModel->GetCenterPoint().DistanceTo(theTop);
    if (theRadius <= this->mModel->GetBoundingRadius(0))
        theRadius = this->mModel->GetBoundingRadius(0);
    this->mBoundingRadius = theRadius;
    if (theResult == kErrorOK)
        this->mModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], gScreenIndex);
    return theResult;
}

// FUNCTION: EXE 0x50068a
// Morphs the mesh between the terrain and the cone by the growth fraction, jittering vertices near the centre every
// effect time 2 while rising, then queues the volcano in render queue 2
long EEE3DVolcano::Queue(EEEffectQueue* inQueues)
{
    static URandom sRandom;
    GE3DPoint thePoint;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        if (!this->mDone) {
            unsigned long i;
            if (this->mTime > this->mGraphicEffect->mTime2) {
                this->mTime = 0;
                thePoint.SetX(this->mCenter.mX);
                thePoint.SetY(this->mArea.GetTop() - 0.2f);
                thePoint.SetZ(this->mCenter.mZ);
                float theAmplitude = this->mGraphicEffect->mStartSize;
                float theScale = 1.0f / this->mCenter.DistanceToSquared(thePoint);
                for (i = 0; i < this->mModel->GetVertexArray(0)->GetSize(); i++) {
                    this->mModel->GetVertexArray(0)->GetPoint(thePoint, i);
                    this->mCenter.SetZ(this->mPoints[i].GetZ());
                    float theFactor = 1.0f - this->mCenter.DistanceToSquared(this->mPoints[i]) * theScale;
                    if (theFactor >= 1.0f)
                        theFactor = 1.0f;
                    else if (theFactor <= 0.0f)
                        theFactor = 0.0f;
                    float theX = this->mPoints[i].GetX();
                    thePoint.SetX((sRandom.GenerateUnit() * 2.0f - 1.0f) * (1.0f - this->mGrowth) * theFactor * theAmplitude
                                  + theX);
                    float theY = this->mPoints[i].GetY();
                    thePoint.SetY((sRandom.Generate() * 2.0f - 1.0f) * (1.0f - this->mGrowth) * theFactor * theAmplitude
                                  + theY);
                    float theZ = this->mPoints[i].GetZ();
                    thePoint.SetZ((1.0f - this->mGrowth) * theZ + this->mHeights[i] * this->mGrowth);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
                }
            } else {
                for (i = 0; i < this->mModel->GetVertexArray(0)->GetSize(); i++) {
                    this->mModel->GetVertexArray(0)->GetPoint(thePoint, i);
                    float theZ = this->mPoints[i].GetZ();
                    thePoint.SetZ((1.0f - this->mGrowth) * theZ + this->mGrowth * this->mHeights[i]);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
                }
            }
            this->mModel->ComputeBoundingVolumes();
            this->mModel->ComputeModelNormals();
            if (this->mGrowth == 1.0f)
                this->mDone = true;
        }
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : 0;
        inQueues[2][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x500a24
// Destroys Queue's static random generator at exit (empty)

// FUNCTION: EXE 0x500a25
// Loads the volcano's media and builds its model; returns the first error
long EEE3DVolcano::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult == 0)
        theResult = this->BuildModel();
    return theResult;
}

// FUNCTION: EXE 0x500a47
// Draws the volcano mesh with vertex colours and z writes at depth range 0..1, and leaves the shared viewport at its
// normal depth range 0..1
long EEE3DVolcano::DrawPass4()
{
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    SetTerrainLighting();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(this->mModel, theScreen, 1.0f, theSorted);
    // Leave the shared viewport at its normal depth range 0..1 for the draws that follow.
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    return theResult;
}

long EEE3DVolcano::DrawPass2()
{
    return 0;
}

long EEE3DVolcano::DrawPass5()
{
    return 0;
}

long EEE3DVolcano::DrawPass6()
{
    return 0;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DVolcano::PrepareQueue()
{
}

// FUNCTION: EXE 0x500b46
// Reads the volcano's area, centre, growth, time left, rise timer and grown flag (marking the mesh for reshaping);
// returns the first read error
long EEE3DVolcano::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mDone = false;
    long theResult = this->mArea.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mCenter.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mGrowth);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTimeLeft);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mGrown);
    return theResult;
}

// FUNCTION: EXE 0x500be2
// After loading, repairs the base effect's pointers and rebuilds the volcano model
long EEE3DVolcano::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0)
        theResult = this->BuildModel();
    return theResult;
}

// FUNCTION: EXE 0x500bf7
// Writes the volcano's area, centre, growth, time left, rise timer and grown flag; returns the first write error
long EEE3DVolcano::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mArea.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mCenter.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mGrowth);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTimeLeft);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mGrown);
    return theResult;
}

// FUNCTION: EXE 0x500c7e
// Raises the volcano over effect time 1, holds it for the effect's duration, then lowers it again over time 1;
// returns 7 once its time runs out
long EEE3DVolcano::Step()
{
    long theResult = 0;
    long theElapsed = UMFloatToLong(gPhysics->GetTickTime());
    if (theElapsed <= 0)
        theElapsed = 1;
    if (!this->mGrown) {
        this->mGrowth += gPhysics->GetTickTime() / this->mGraphicEffect->mTime1;
        this->mTime += theElapsed;
        if (this->mGrowth >= 1.0f) {
            this->mGrowth = 1.0f;
            this->mTimeLeft = this->mGraphicEffect->mDuration + this->mGraphicEffect->mTime1;
            this->mGrown = true;
        }
    } else {
        this->mTimeLeft -= theElapsed;
        if (this->mTimeLeft < (long)this->mGraphicEffect->mTime1) {
            this->mDone = false;
            this->mGrowth = (float)this->mTimeLeft / this->mGraphicEffect->mTime1;
        }
        if (this->mTimeLeft < 0)
            theResult = 7;
    }
    return theResult;
}

// the template COMDATs of BuildModel's vectors (vector<GEColor>, vector<vector<GEColor> >, vector<vector<float> >)

// FUNCTION: EXE 0x500d4d
// Resizes the vector of colour rows (vector<vector<GEColor> >::resize(n))

// FUNCTION: EXE 0x500d73
// Resizes the vector of height rows (vector<vector<float> >::resize(n))

// FUNCTION: EXE 0x500d99
// Resizes a point vector, filling with a given point (vector<GE3DPoint>::resize)

// FUNCTION: EXE 0x500e4d
// Resizes the vector of colour rows, filling with a given row (vector<vector<GEColor> >::resize)

// FUNCTION: EXE 0x500f00
// Destroys and frees the colour rows (vector<vector<GEColor> >::_Tidy)

// FUNCTION: EXE 0x500f38
// Resizes the vector of height rows, filling with a given row (vector<vector<float> >::resize)

// FUNCTION: EXE 0x500feb
// Destroys and frees the height rows (vector<vector<float> >::_Tidy)

// FUNCTION: EXE 0x501023
// Inserts n copies of a point (vector<GE3DPoint>::insert)

// FUNCTION: EXE 0x5012d8
// Erases a range of 12-byte elements (bound to vector<EECPASGLandPathPoint>::erase, which shares this code)

// FUNCTION: EXE 0x501322
// Inserts n copies of a colour row (vector<vector<GEColor> >::insert)

// FUNCTION: EXE 0x5015d6
// Erases a range of colour rows (vector<vector<GEColor> >::erase)

// FUNCTION: EXE 0x50163f
// Inserts n copies of an inner vector (bound to vector<vector<long> >::insert, which shares this code)

// FUNCTION: EXE 0x5018f3
// Copy-constructs a colour vector (vector<GEColor> copy constructor)

// FUNCTION: EXE 0x50193d
// Copy-constructs n colour rows into raw storage (vector<vector<GEColor> >::_Ufill)

// FUNCTION: EXE 0x501968
// Copy-constructs n height rows into raw storage (vector<vector<float> >::_Ufill)

// FUNCTION: EXE 0x501993
// Copy-constructs a range of points into raw storage (vector<GE3DPoint>::_Ucopy)

// FUNCTION: EXE 0x5019c3
// Copy-constructs a range of colour rows into raw storage (vector<vector<GEColor> >::_Ucopy)

// FUNCTION: EXE 0x5019ef
// Assigns a colour row to every element of a range (std::fill)

// FUNCTION: EXE 0x501a0c
// Copies a range of colour rows backwards (std::copy_backward)

// FUNCTION: EXE 0x501a38
// Copies a range of colour rows (std::copy)

// FUNCTION: EXE 0x501a5c
// Copies a range of height rows (std::copy)

// FUNCTION: EXE 0x501a80
// Copy-constructs one colour row in place (std::_Construct)

// FUNCTION: EXE 0x501a92
// Destroys a colour vector and optionally frees it (vector<GEColor> scalar deleting destructor)

// FUNCTION: EXE 0x501aae
// Destroys a float vector and optionally frees it (vector<float> scalar deleting destructor)

// FUNCTION: EXE 0x501aca
// Assigns one colour vector to another (vector<GEColor>::operator=)
