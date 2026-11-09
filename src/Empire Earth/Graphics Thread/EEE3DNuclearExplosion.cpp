// C:\EEX\Empire Earth\Graphics Thread\EEE3DNuclearExplosion.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DNuclearExplosion-Data"). 0xf43c7..0xf4bf3: the key chain, both ctors .. Virtual4C.
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
#include "ee/EEE3DNuclearExplosion.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4f43c7
// SYNTHETIC: EXE 0x4f43d1
// SYNTHETIC: EXE 0x4f43e2
// SYNTHETIC: EXE 0x4f43ee
const UString EEE3DNuclearExplosion::sData("EEE3DNuclearExplosion-Data");

// FUNCTION: EXE 0x4f440a
// Creates a nuclear explosion on a unit, with its flash and rise at 0
EEE3DNuclearExplosion::EEE3DNuclearExplosion(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DNuclearExplosion), mGraphicObject(NULL)
{
    this->mFlash = 0.0f;
    this->mRise = 0.0f;
}

// SYNTHETIC: EXE 0x4f4449
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f4465
// Creates an empty nuclear explosion for loading from a saved game
EEE3DNuclearExplosion::EEE3DNuclearExplosion()
    : EEEffect3D(kEffect3DNuclearExplosion)
{
}

// FUNCTION: EXE 0x4f4479
// Destroys the explosion, unloading its graphic's media
EEE3DNuclearExplosion::~EEE3DNuclearExplosion()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DNuclearExplosion::DrawPass2()
{
    return 0;
}

long EEE3DNuclearExplosion::DrawPass4()
{
    return 0;
}

long EEE3DNuclearExplosion::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4f449d
// Queues the explosion in render queue 4, snapshotting its flash, rise and pending texture scroll (then cleared)
long EEE3DNuclearExplosion::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFlash = this->mFlash;
        this->mDrawRise = this->mRise;
        this->mDrawScroll = this->mScroll;
        this->mScroll = 0.0f;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DNuclearExplosion::PrepareQueue()
{
}

// NEAR: terrain Z store/reload is fstp/fld in the original versus fst here
// FUNCTION: EXE 0x4f4524
// Places the explosion on the ground (not below 0) at the unit, or its tile centre when off the map, and loads
// the graphic; returns the load result
long EEE3DNuclearExplosion::SetUp()
{
    GETransformation theTransformation;
    GE3DPoint thePosition;
    EEUnit* theUnit = this->mUnit;
    if (theUnit->mTileNode)
        theUnit->mComplexUnit->GetPosition(thePosition);
    else {
        thePosition.SetX(theUnit->GetClass()->GetTileSize() * 0.5f + theUnit->mTileX);
        thePosition.SetY(theUnit->GetClass()->GetTileSize() * 0.5f + theUnit->mTileY);
    }
    thePosition.SetZ(gWorld.mMap->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY()));
    if (thePosition.GetZ() < 0.0f)
        thePosition.SetZ(0.0f);
    theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// NEAR: x87 scheduling of the opacity and the transformations' translation adds
// FUNCTION: EXE 0x4f4601
// Draws the mushroom cloud and stem (squashing and fading as it rises, textures scrolling), then the additive
// flash sprite on top of everything, dimming with the flash; restores the render state
long EEE3DNuclearExplosion::DrawPass6()
{
    unsigned long i;
    GETransformation theCloudTransformation;
    GETransformation theStemTransformation;
    GETransformation theFlashTransformation(gWorld.mTransformation);
    GETextureCoordinate theCoordinate;
    GE3DPoint thePosition;
    GEModel* theFlashModel = gEffect2DModel;
    unsigned long theScreen = gScreenIndex;
    float theOpacity = this->mDrawRise * this->mDrawRise;
    theOpacity = 1.0f - theOpacity;
    GEModel* theCloud = this->mGraphicObject->mModels[11];
    GEModel* theStem = this->mGraphicObject->mModels[9];
    float theHeight = this->mGraphicEffect->GetStartSize();
    float theFlashScale = this->mGraphicEffect->GetSize();
    float theScroll = theOpacity * this->mDrawScroll;
    float theFall = 1.0f - this->mDrawRise;
    float theLift = (1.0f - theFall * theFall) * theHeight * 0.5f;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetUnitLighting();
    theCloud->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[9], theScreen);
    theStem->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[9], theScreen);
    thePosition.SetX(this->mTransformation.GetX());
    thePosition.SetY(this->mTransformation.GetY());
    thePosition.SetZ(this->mTransformation.GetZ());
    float theSquash = (theHeight - theLift) / theHeight;
    theCloudTransformation.Scale(theSquash, theSquash, 1.0f);
    theCloudTransformation.mMatrix[0][3] += thePosition.GetX();
    theCloudTransformation.mMatrix[1][3] += thePosition.GetY();
    theCloudTransformation.mMatrix[2][3] += thePosition.GetZ();
    theCloud->SetGraphicsPostModelTransform(theCloudTransformation);
    theCloud->SetGraphicsScale(theHeight);
    theStemTransformation.mMatrix[0][3] += thePosition.GetX();
    theStemTransformation.mMatrix[1][3] += thePosition.GetY();
    theStemTransformation.mMatrix[2][3] += thePosition.GetZ();
    theStem->SetGraphicsPostModelTransform(theStemTransformation);
    theStem->SetGraphicsScale(theLift + theHeight);
    theCloud->SetOpacity(theOpacity);
    theStem->SetOpacity(theOpacity);
    for (i = 0; i < theCloud->GetVertexArray(0)->GetSize(); i++) {
        theCloud->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, i);
        theCoordinate.SetV(theCoordinate.GetV() + theScroll);
        theCloud->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, i);
    }
    for (i = 0; i < theStem->GetVertexArray(0)->GetSize(); i++) {
        theStem->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, i);
        theCoordinate.SetV(theCoordinate.GetV() - theScroll);
        theStem->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, i);
    }
    theCloud->PrepareTransforms();
    gRasterizer->Draw(theCloud, theScreen, 1.0f, gOrthographicView);
    theStem->PrepareTransforms();
    gRasterizer->Draw(theStem, theScreen, 1.0f, gOrthographicView);
    SetWhiteLighting();
    float theLight;
    GetFogFactor(thePosition, theFog, theLight);
    theLight = (1.0f - this->mDrawFlash * this->mDrawFlash) * theLight;
    GEColor theColor(theLight, theLight, theLight);
    gRasterizer->SetAmbientColor(theColor, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 0.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetZCompare(kCompareLess, theScreen);
    theFlashModel->GetMaterial(0)->SetColor(GEColor(1.0f, 1.0f, 1.0f));
    theFlashModel->GetMaterial(0)->SetOpacity(1.0f);
    theFlashTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
    theFlashModel->SetGraphicsPostModelTransform(theFlashTransformation);
    theFlashModel->SetGraphicsScale(theFlashScale);
    theFlashModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theFlashModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theFlashModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4f4af8
// Reloads the graphic and reads the flash and rise; returns the first read error
long EEE3DNuclearExplosion::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mFlash);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mRise);
    return theResult;
}

// RepairPointers (relinks the effect's pointers): ICF-folded into EEE3DBlastWave's 0xe93e0
long EEE3DNuclearExplosion::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4f4b41
// Writes the base 3D effect (result ignored), the flash and rise; returns the first write error
long EEE3DNuclearExplosion::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mFlash);
    if (theResult == 0)
        theResult = outFile.Write(this->mRise);
    return theResult;
}

// FUNCTION: EXE 0x4f4b79
// Advances the flash (over the duration) and the rise, sets this tick's texture scroll; returns 7 once fully risen
long EEE3DNuclearExplosion::Step()
{
    long theResult = 0;
    float theElapsed = gPhysics->GetTickSeconds() * 1000.0f;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    this->mFlash += theElapsed / theEffect->mDuration;
    this->mRise += theElapsed / theEffect->GetParam1();
    this->mScroll = theElapsed / theEffect->GetParam2();
    if (this->mFlash > 1.0f)
        this->mFlash = 1.0f;
    if (this->mRise > 1.0f)
        theResult = 7;
    return theResult;
}
