// C:\EEX\Empire Earth\Graphics Thread\EEE3DScreenFlash.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DScreenFlash-Data"). 0xf8ceb..0xf90e9: the key chain, both ctors .. Virtual4C; EEE3DShells.cpp's Virtual14
// follows. Unit flags: the exe's module set.
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
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "ee/EEE3DScreenFlash.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4f8ceb
// SYNTHETIC: EXE 0x4f8cf5
// SYNTHETIC: EXE 0x4f8d06
// SYNTHETIC: EXE 0x4f8d12
const UString EEE3DScreenFlash::sData("EEE3DScreenFlash-Data");

// FUNCTION: EXE 0x4f8d2e
// Creates a screen flash effect for a unit, starting at age 0
EEE3DScreenFlash::EEE3DScreenFlash(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DScreenFlash), mGraphicObject(NULL)
{
    this->mAge = 0.0f;
}

// SYNTHETIC: EXE 0x4f8d65
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f8d81
// Creates an empty screen flash effect (for loading)
EEE3DScreenFlash::EEE3DScreenFlash()
    : EEEffect3D(kEffect3DScreenFlash)
{
}

// FUNCTION: EXE 0x4f8d95
// Destroys the flash, unloading its graphic object's media
EEE3DScreenFlash::~EEE3DScreenFlash()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DScreenFlash::DrawPass2()
{
    return 0;
}

long EEE3DScreenFlash::DrawPass4()
{
    return 0;
}

long EEE3DScreenFlash::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4f8db9
// Snapshots the flash's age for drawing and queues it in render queue 4 under its texture
long EEE3DScreenFlash::Queue(EEEffectQueue* inQueues)
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

// FUNCTION: EXE 0x4f8e1a
// Draws the flash quad additively in front of everything (zero depth range), fading with the square of its age;
// restores the viewport depth range, z-buffer writes and z compare
// NEAR: the original schedules the fade's `fld st(0)` after the gRasterizer load and forms the model's +0x48
// address before the opacity store; ours does not
long EEE3DScreenFlash::DrawPass6()
{
    GETransformation theTransformation(gWorld.mTransformation);
    GE3DPoint theLocation;
    float theValue;
    GEModel* theModel = gEffect2DModel;
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    theLocation.SetX(this->mTransformation.GetX());
    theLocation.SetY(this->mTransformation.GetY());
    theLocation.SetZ(this->mTransformation.GetZ());
    SetWhiteLighting();
    GetFogFactor(theLocation, theFog, theValue);
    theValue *= 1.0f - this->mDrawAge * this->mDrawAge;
    gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 0.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetZCompare(kCompareLess, theScreen);
    theModel->GetMaterial(0)->SetColor(GEColor(1.0f, 1.0f, 1.0f));
    theModel->GetMaterial(0)->SetOpacity(1.0f);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(this->mGraphicEffect->mSize);
    theModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DScreenFlash::PrepareQueue()
{
}

// ICF-folded into EEE3DEnergyWave's 0xee3a4
long EEE3DScreenFlash::SetUp()
{
    GETransformation theTransformation;
    GE3DPoint thePoint;
    this->mUnit->mComplexUnit->GetPosition(thePoint);
    theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4f904c
// Reads the flash's age from a saved game after loading its media; returns the read error
long EEE3DScreenFlash::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    return inFile.Read(this->mAge);
}

// ICF-folded into 0xe93e0
long EEE3DScreenFlash::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4f9083
// Writes the flash's age to the saved game; returns the write error
long EEE3DScreenFlash::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    return outFile.Write(this->mAge);
}

// FUNCTION: EXE 0x4f90a9
// Ages the flash by the tick over the effect's duration; returns 7 once it reaches 1
long EEE3DScreenFlash::Step()
{
    this->mAge += (gPhysics->GetTickSeconds() * 1000.0f) / this->mGraphicEffect->mDuration;
    if (this->mAge >= 1.0f)
        return 7;
    return 0;
}
