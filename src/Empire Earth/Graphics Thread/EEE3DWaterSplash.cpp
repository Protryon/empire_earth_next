// C:\EEX\Empire Earth\Graphics Thread\EEE3DWaterSplash.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DWaterSplashData"). 0x101c34..0x101fce: the key chain, both ctors .. Virtual4C.
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
#include "ee/EEE3DWaterSplash.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x501c34
// SYNTHETIC: EXE 0x501c3e
// SYNTHETIC: EXE 0x501c4f
// SYNTHETIC: EXE 0x501c5b
const UString EEE3DWaterSplash::sData("EEE3DWaterSplashData");

// FUNCTION: EXE 0x501c77
// Creates a water splash effect at a point, starting at age 0
EEE3DWaterSplash::EEE3DWaterSplash(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, kEffect3DWaterSplash)
{
    GETransformation theTransformation;
    theTransformation.SetPosition(inLocation.GetX(), inLocation.GetY(), inLocation.GetZ());
    this->SetTransformation(theTransformation);
    this->mAge = 0.0f;
}

// SYNTHETIC: EXE 0x501ce6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x501d02
// Creates an empty water splash effect (for loading)
EEE3DWaterSplash::EEE3DWaterSplash()
    : EEEffect3D(kEffect3DWaterSplash), mAge(0.0f)
{
}

// FUNCTION: EXE 0x501d1b
// Destroys the splash, unloading its graphic object's media
EEE3DWaterSplash::~EEE3DWaterSplash()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DWaterSplash::DrawPass2()
{
    return 0;
}

long EEE3DWaterSplash::DrawPass4()
{
    return 0;
}

long EEE3DWaterSplash::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x501d3f
// Snapshots the splash's age for drawing and queues it in render queue 4 under its texture
long EEE3DWaterSplash::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DWaterSplash::PrepareQueue()
{
}

// FUNCTION: EXE 0x501da0
// Loads the splash's graphic object media
long EEE3DWaterSplash::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x501db4
// Draws the splash growing from its start size (eased; the eased age is written back), flattening with age and fading
// out over the last tenth
// NEAR: the original computes the flatness after loading the rasterizer; ours before
long EEE3DWaterSplash::DrawPass6()
{
    GETransformation theTransformation(this->mTransformation);
    float theFlatness = 1.0f - UMSquare(this->mDrawAge);
    GEModel* theModel = this->mGraphicObject->mModel;
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetUnitLighting();
    theModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    this->mDrawAge = 1.0f - UMSquare(1.0f - this->mDrawAge);
    theTransformation.Scale(1.0f, 1.0f, theFlatness);
    float theScale = (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * this->mDrawAge + this->mGraphicEffect->GetStartSize();
    if (theScale != 0.0f)
        theModel->SetGraphicsScale(theScale);
    else
        theModel->SetGraphicsScale(0.00001f);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetOpacity(this->mDrawAge >= 0.9f ? (1.0f - this->mDrawAge) * 10.0f : 1.0f);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    return gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
}

// FUNCTION: EXE 0x501f32
// Reads the splash's age from a saved game after loading its media; returns the read error
long EEE3DWaterSplash::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    return inFile.Read(this->mAge);
}

// FUNCTION: EXE 0x501f69
// Writes the splash's age to the saved game; returns the write error
long EEE3DWaterSplash::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    return outFile.Write(this->mAge);
}

// FUNCTION: EXE 0x501f8f
// Ages the splash by the tick over the effect's duration; returns 7 once past 1
long EEE3DWaterSplash::Step()
{
    long theResult = 0;
    float theElapsed = gPhysics->GetTickSeconds() * 1000.0f;
    this->mAge += theElapsed / this->mGraphicEffect->mDuration;
    if (this->mAge > 1.0f)
        theResult = 7;
    return theResult;
}
