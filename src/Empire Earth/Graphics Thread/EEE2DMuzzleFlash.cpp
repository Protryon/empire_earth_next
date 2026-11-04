// C:\EEX\Empire Earth\Graphics Thread\EEE2DMuzzleFlash.cpp (no __FILE__, no key chain; name from the RTTI class).
// 0xe4529..0xe4729: both ctors .. Step; EEE2DParticle.cpp's key chains follow.
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
#include "ee/EEE2DImpact.h"
#include "ee/EEE2DMuzzleFlash.h"

// FUNCTION: EXE 0x4e4529
// Creates a muzzle flash (an impact of kind 9) on a firing unit
EEE2DMuzzleFlash::EEE2DMuzzleFlash(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEE2DImpact(inUnit, inGraphicEffect, 9)
{
}

// SYNTHETIC: EXE 0x4e4547
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e4563
// Creates an empty muzzle flash for loading from a saved game
EEE2DMuzzleFlash::EEE2DMuzzleFlash()
    : EEE2DImpact(9)
{
}

// FUNCTION: EXE 0x4e4577
// Destroys the muzzle flash (the impact base does the cleanup)
EEE2DMuzzleFlash::~EEE2DMuzzleFlash()
{
}

// FUNCTION: EXE 0x4e4582
// Queues the flash as an impact
long EEE2DMuzzleFlash::Queue(EEEffectQueue* inQueues)
{
    return EEE2DImpact::Queue(inQueues);
}

// FUNCTION: EXE 0x4e458e
// Places the flash on the unit's weapon tag point (turret-aware; the tag list is not null-checked) and loads its
// graphic at the first frame; returns 0
long EEE2DMuzzleFlash::SetUp()
{
    GETransformation theTransformation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = this->mUnit->GetModel();
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    unsigned long thePart = theUnit->mAnimation;
    EETagPointList* theList = this->mUnit->GetModel()->mTagPoints[thePart];
    float theScale = theUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    EETagPointList::Scale(theTransformation, theScale);
    if (theModel->GetNumParts() <= 0) {
        theList->mPoints[theIndex]->GetPoint(theFrame, theTransformation, this->mStart);
    } else {
        GETransformation thePartTransformation(false);
        unsigned long theModelIndex;
        thePartTransformation.MakeIdentity();
        if (theModel->FindTurretPart((*(theList->mPoints.begin() + theIndex))->mModel, theModelIndex) == 0)
            thePartTransformation.SetOrientationYPR(theUnit->mTurretAngles[theModelIndex], 0.0f, 0.0f);
        (*(theList->mPoints.begin() + theIndex))->GetPoint(theFrame, theTransformation, thePartTransformation, this->mStart);
    }
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mTime = 0.0f;
    this->mDrawFrame = 0;
    this->mFrame = this->mGraphicObject->mStartFrames[0];
    this->SetPosition(this->mStart);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return 0;
}

// FUNCTION: EXE 0x4e46f7
// Draws the flash as an impact
long EEE2DMuzzleFlash::DrawPass6()
{
    return EEE2DImpact::DrawPass6();
}

// FUNCTION: EXE 0x4e46fc
// Reads the flash as an impact; returns its result
long EEE2DMuzzleFlash::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return EEE2DImpact::Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x4e4710
// Writes the flash as an impact; returns its result
long EEE2DMuzzleFlash::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return EEE2DImpact::Save(inMajorVersion, inMinorVersion, outFile);
}

// FUNCTION: EXE 0x4e4724
// Animates the flash as an impact; returns its result
long EEE2DMuzzleFlash::Step()
{
    return EEE2DImpact::Step();
}
