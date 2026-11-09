// C:\EEX\Empire Earth\Graphics Thread\EEE3DTeleport.cpp (no __FILE__, no key chain; name from the RTTI class).
// 0xfb9f6..0xfbec6: the ctors .. Virtual4C; EEE3DTimeWarp.cpp follows.
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
#include "ee/EEEffectManager.h"
#include "ee/EEE3DTeleport.h"

// FUNCTION: EXE 0x4fb9f6
// Creates a teleport effect on a unit, starting at the unit (flag 1) or one model height below it, and launches the
// effect's two linked line effects from the far end of that span
EEE3DTeleport::EEE3DTeleport(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    GETransformation theTransformation;
    GE3DLine theLine;
    GE3DPoint thePoint;
    bool theRising = this->mGraphicEffect->mFlag1;
    inUnit->GetPosition(thePoint);
    theTransformation.SetPosition(thePoint);
    float theHeight = this->mGraphicEffect->mGraphicObject->mModels[0]->GetBoundingBox(0).GetHeight() *
                      this->mGraphicEffect->GetSize();
    if (theRising) {
        theTransformation.SetZ(thePoint.GetZ());
        thePoint.SetZ(thePoint.GetZ() + theHeight);
    } else {
        theTransformation.SetZ(thePoint.GetZ() - theHeight);
        thePoint.SetZ(thePoint.GetZ());
    }
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    GEVector theDirection(0.0f, 0.0f, theRising ? -this->mGraphicEffect->mParam1 : this->mGraphicEffect->mParam1);
    theLine.SetStart(thePoint);
    theLine.SetDirection(theDirection);
    gEffectManager->CreateLineEffect(theLine, this->mGraphicEffect->mLinkedEffect1, NULL);
    gEffectManager->CreateLineEffect(theLine, this->mGraphicEffect->mLinkedEffect2, NULL);
}

// SYNTHETIC: EXE 0x4fbb47
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fbb63
// Creates an empty teleport effect of the given kind (for loading)
EEE3DTeleport::EEE3DTeleport(long inKind)
    : EEEffect3D(inKind)
{
}

// FUNCTION: EXE 0x4fbb8a
// Destroys the effect, unloading its graphic object's media
EEE3DTeleport::~EEE3DTeleport()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

long EEE3DTeleport::DrawPass2()
{
    return 0;
}

long EEE3DTeleport::DrawPass5()
{
    return 0;
}

long EEE3DTeleport::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4fbbbc
// Snapshots the effect's age and location for drawing and queues it in render queue 2 under its texture
long EEE3DTeleport::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mTransformation.GetPosition(this->mLocation);
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[2][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DTeleport::PrepareQueue()
{
}

// FUNCTION: EXE 0x4fbc35
// Loads the graphic effect's media and starts the effect at age 0
long EEE3DTeleport::SetUp()
{
    float theHeight = this->mGraphicEffect->mSize;
    this->mGraphicObject = this->mGraphicEffect->mGraphicObject;
    this->mGraphicObject->LoadMedia();
    this->mAge = 0.0f;
    this->mBoundingRadius = theHeight;
    return 0;
}

// ICF-folded into EEE3DTimeWarp's 0xfc0bb
long EEE3DTeleport::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mAge);
    return theResult;
}

// ICF-folded into EEE3DTimeWarp's 0xfc0e8
long EEE3DTeleport::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0) {
        this->mGraphicObject = this->GetGraphicObject();
        theResult = this->mGraphicObject->LoadMedia();
    }
    return theResult;
}

// FUNCTION: EXE 0x4fbc66
// Draws the effect's model fully transparent with z writes and no alpha test (a depth-only mask), then restores the
// ambient colour, fog and alpha testing
long EEE3DTeleport::DrawPass4()
{
    GETransformation theTransformation(this->mTransformation);
    GEColor theAmbientColor;
    float theValue;
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    theAmbientColor = gRasterizer->GetAmbientColor();
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    gRasterizer->SetAlphaTesting(0, theScreen);
    theModel->SetOpacity(0.0f);
    const GE3DPoint& theLocation = this->mLocation;
    GetFogFactor(theLocation, theFog, theValue);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetAmbientColor(theAmbientColor, theScreen);
    gRasterizer->SetFog(theFog, theScreen);
    gRasterizer->SetAlphaTesting(1, theScreen);
    return theResult;
}

// ICF-folded into EEE3DTimeWarp's 0xfc297
long EEE3DTeleport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mAge);
    return theResult;
}

// FUNCTION: EXE 0x4fbe08
// Moves the effect down (flag 1) or up at param 2 per second; returns 7 once it is older than the effect's duration
long EEE3DTeleport::Step()
{
    GETransformation theTransformation(this->mTransformation);
    float theElapsed = gPhysics->GetTickSeconds();
    float theSpeed = this->mGraphicEffect->mParam2;
    bool theRising = this->mGraphicEffect->mFlag1;
    long theResult = 0;
    this->mAge += theElapsed * 1000.0f;
    if (this->mAge <= this->mGraphicEffect->mDuration) {
        GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        if (theRising)
            thePosition.SetZ(thePosition.GetZ() - theSpeed * theElapsed);
        else
            thePosition.SetZ(thePosition.GetZ() + theSpeed * theElapsed);
        theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
        this->SetTransformation(theTransformation);
    } else {
        theResult = 7;
    }
    return theResult;
}
