// C:\EEX\Empire Earth\Graphics Thread\EEE3DAdditivePass.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DAdditivePass-Data"). 0xe89c6..0xe8ed8: the key chain, both ctors .. Virtual4C, then EEEffect3D's ctor
// EEEffect3D(long), its Virtual3C and ??_G (this is EEEffect3D's first user; EEE3DBlastWave.cpp's chain follows).
// Unit flags: the exe's module set.
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
#include "ee/EEE3DAdditivePass.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e89c6
// SYNTHETIC: EXE 0x4e89d0
// SYNTHETIC: EXE 0x4e89e1
// SYNTHETIC: EXE 0x4e89ed
const UString EEE3DAdditivePass::sData("EEE3DAdditivePass-Data");

// FUNCTION: EXE 0x4e8a09
// Creates an additive glow pass over a unit's model, with no model or graphic loaded yet
EEE3DAdditivePass::EEE3DAdditivePass(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DAdditivePass), mDrawScale(0.0f), mScale(0.0f), mGraphicObject(NULL), mModel(NULL)
{
    this->mFrame = 0;
    this->mModelIndex = 0;
}

// SYNTHETIC: EXE 0x4e8a71
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e8a8d
// Creates an empty additive pass for loading from a saved game
EEE3DAdditivePass::EEE3DAdditivePass()
    : EEEffect3D(kEffect3DAdditivePass)
{
}

// FUNCTION: EXE 0x4e8aaa
// Destroys the pass, unloading its graphic's and model's media
EEE3DAdditivePass::~EEE3DAdditivePass()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    if (this->mModel)
        this->mModel->UnloadMedia();
}

long EEE3DAdditivePass::DrawPass2()
{
    return 0;
}

long EEE3DAdditivePass::DrawPass4()
{
    return 0;
}

long EEE3DAdditivePass::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4e8ae0
// Queues the pass in render queue 3 under its texture, snapshotting the unit's transform, frame, animation and scale
long EEE3DAdditivePass::Queue(EEEffectQueue* inQueues)
{
    EEUnit* theUnit = this->mUnit;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawTransformation = this->mTransformation;
        this->mFrame = theUnit->mComplexUnit->mPhysicalModel.GetFrame();
        this->mModelIndex = theUnit->mComplexUnit->mAnimation;
        this->mDrawScale = this->mScale;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4e8b78
// Loads the graphic and the unit's model, follows the unit's scale and location; returns the model load result
long EEE3DAdditivePass::SetUp()
{
    EEUnit* theUnit = this->mUnit;
    this->mGraphicObject = this->GetGraphicObject();
    this->mModel = theUnit->GetModel();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mModel->LoadMedia();
    this->mScale = theUnit->mStats->GetScale();
    this->SetTransformation(theUnit->mComplexUnit->mPhysicalModel.GetLocation());
    this->mBoundingRadius = this->mModel->mModels[0]->GetBoundingRadius(0) * this->mScale;
    return theResult;
}

// FUNCTION: EXE 0x4e8bfb
// Redraws the unit's model additively (one-one blending, z test only) with the effect texture, lit by the terrain
// light at its position; restores the fog and returns the draw result
long EEE3DAdditivePass::DrawPass5()
{
    GE3DPoint thePosition;
    GEModel* theModel = this->mModel->mModels[this->mModelIndex];
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    this->mDrawTransformation.GetPosition(thePosition);
    float theLight;
    GetFogFactor(thePosition, theFog, theLight);
    GEColor theColor(theLight, theLight, theLight);
    gRasterizer->SetAmbientColor(theColor, theScreen);
    theModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theModel->SetGraphicsCurrentFrame(this->mFrame);
    theModel->SetGraphicsScale(this->mDrawScale);
    theModel->SetGraphicsPostModelTransform(this->mDrawTransformation);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// the empty PrepareQueue: ICF-folded into 0xe123d
void EEE3DAdditivePass::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e8d9c
// Reads the base 3D effect and reloads the graphic; returns the first error
long EEE3DAdditivePass::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        this->mGraphicObject = this->GetGraphicObject();
        theResult = this->mGraphicObject->LoadMedia();
    }
    return theResult;
}

// FUNCTION: EXE 0x4e8dcc
// Relinks the base pointers, then reloads the unit's model and scale after a game load; returns the first error
long EEE3DAdditivePass::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult != 0)
        return theResult;
    this->mModel = this->mUnit->GetModel();
    theResult = this->mModel->LoadMedia();
    this->mScale = this->mUnit->mStats->GetScale();
    return theResult;
}

// FUNCTION: EXE 0x4e8e01
// Writes the base 3D effect; returns its result
long EEE3DAdditivePass::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
}

// FUNCTION: EXE 0x4e8e15
// Follows the unit's scale and location; returns 7 (remove) once the unit has no hit points
long EEE3DAdditivePass::Step()
{
    long theResult = 0;
    EEUnit* theUnit = this->mUnit;
    if (theUnit->mHitPoints == 0) {
        theResult = 7;
    } else {
        this->mScale = theUnit->mStats->GetScale();
        this->SetTransformation(theUnit->mComplexUnit->mPhysicalModel.GetLocation());
        this->mBoundingRadius = this->mModel->mModels[0]->GetBoundingRadius(0) * this->mScale;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e8e76
// Creates an empty 3D effect of a kind for loading from a saved game
// NEAR: the original expands EEEffect(long) here (ours calls it out of line: the OPEN in EEEffect.h)
// EEEffect3D's load-time ctor (see EEEffect3D.h: every derived default ctor calls it out of line; this unit is the
// first user, and the body sits here with the class's Virtual3C and ??_G) (placement inferred)
EEEffect3D::EEEffect3D(long inKind)
    : EEEffect(inKind)
{
}

// FUNCTION: EXE 0x4e8eb1
// Relinks the 3D effect's pointers after a load (the inline forwarder, emitted with the class vtable)

// SYNTHETIC: EXE 0x4e8eb6
// (EEEffect3D's scalar deleting destructor, emitted with the class vtable)
