// C:\EEX\Empire Earth\Graphics Thread\EEE3DDestructionSmoke.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DDestructionSmoke"). 0xebbba..0xebde9: the key chain, the ctor, Virtual30, Load, Save; EEE3DDisease.cpp
// follows.
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
#include "ee/EEE3DSmokeGenerator.h"
#include "ee/EEE3DDestructionSmoke.h"

// SYNTHETIC: EXE 0x4ebbba
// SYNTHETIC: EXE 0x4ebbc4
// SYNTHETIC: EXE 0x4ebbd5
// SYNTHETIC: EXE 0x4ebbe1
const UString EEE3DDestructionSmoke::sData("EEE3DDestructionSmoke");

// FUNCTION: EXE 0x4ebbfd
// Creates the smoke of a destroyed unit at its location, spread over half the unit class's scale
EEE3DDestructionSmoke::EEE3DDestructionSmoke(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEE3DSmokeGenerator(GE3DPoint(inUnit->mComplexUnit->mPhysicalModel.GetLocation().GetX(),
                                    inUnit->mComplexUnit->mPhysicalModel.GetLocation().GetY(),
                                    inUnit->mComplexUnit->mPhysicalModel.GetLocation().GetZ()),
                          inGraphicEffect, 0x1e)
{
    this->mRadius = inUnit->mUnitClass->mScale * 0.5f;
}

// FUNCTION: EXE 0x4ebc56
// Launches linked line effects outward in random directions from the centre, more for a bigger radius; returns 0
// NEAR: the frame layout and the centre's copies differ (logic as the original's)
long EEE3DDestructionSmoke::SetUp()
{
    GE3DLine theLine;
    GETransformation theTransformation;
    float theRadius = this->mRadius;
    GE3DPoint theOffset;
    GE3DPoint theRotated;
    float theSpeed = this->mGraphicEffect->mParam2;
    unsigned long theCount = UMFloatToLong(theRadius / this->mGraphicEffect->GetParam1() * this->mGraphicEffect->mIntParam1);
    GE3DPoint theCenter(this->mTransformation.GetX(), this->mTransformation.GetY(), this->mTransformation.GetZ());
    for (; theCount; theCount--) {
        float theDistance = (1.0f - EEEffect::GetRandom() * EEEffect::GetRandom()) * theRadius;
        theTransformation.SetOrientationYPR(EEEffect::GetRandom() * 6.2831855f, 0.0f, 0.0f);
        theOffset.SetX(theDistance);
        theTransformation.Apply(theOffset, theRotated);
        theLine.SetStart(GE3DPoint(theRotated.GetX() + theCenter.GetX(), theRotated.GetY() + theCenter.GetY(),
                                   theCenter.GetZ()));
        theLine.SetDirection(GEVector(theRotated.GetX() * theSpeed, theRotated.GetY() * theSpeed, 0.0f));
        gEffectManager->CreateLineEffect(theLine, this->mGraphicEffect->mLinkedEffect1, NULL);
    }
    return 0;
}

// FUNCTION: EXE 0x4ebd9d
// Reads the smoke generator (result ignored) and the radius; returns the radius read result
long EEE3DDestructionSmoke::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEE3DSmokeGenerator::Load(inMajorVersion, inMinorVersion, inFile);
    return inFile.Read(this->mRadius);
}

// FUNCTION: EXE 0x4ebdc3
// Writes the smoke generator (result ignored) and the radius; returns the radius write result
long EEE3DDestructionSmoke::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEE3DSmokeGenerator::Save(inMajorVersion, inMinorVersion, outFile);
    return outFile.Write(this->mRadius);
}

// EmitPuff (empty; ICF-folded into 0x557269)
void EEE3DDestructionSmoke::EmitPuff(GE3DPoint& inLocation)
{
}
