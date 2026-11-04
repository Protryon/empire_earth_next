// C:\EEX\Empire Earth\Graphics Thread\EEE3DDamageSmoke.cpp (no __FILE__, no key chain; name from the RTTI class).
// 0xea703..0xea9e2: both ctors, the dtor, Virtual4C; EEE3DDamageVent.cpp follows.
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
#include "ee/EEE3DDamageSmoke.h"

// a coordinate moved by up to half of inSpread either way (inferred: the by-value copies of both arguments;
// provisional name)
static inline float EEDamageSmokeJitter(float inValue, float inSpread)
{
    return (0.5f - EEEffect::GetRandom()) * inSpread + inValue;
}

// FUNCTION: EXE 0x4ea703
// Creates a damage smoke generator on a unit
EEE3DDamageSmoke::EEE3DDamageSmoke(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEE3DSmokeGenerator(inUnit, inGraphicEffect, 0x2c)
{
}

// SYNTHETIC: EXE 0x4ea721
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ea73d
// Creates an empty damage smoke generator for loading from a saved game
EEE3DDamageSmoke::EEE3DDamageSmoke()
    : EEE3DSmokeGenerator(0x2c)
{
}

// FUNCTION: EXE 0x4ea751
// Destroys the damage smoke generator
EEE3DDamageSmoke::~EEE3DDamageSmoke()
{
}

// FUNCTION: EXE 0x4ea75c
// While the unit lives below the effect's health threshold, emits jittered smoke at each tag point (else at the
// unit) more often the lower its health; returns 7 (remove) once it is dead or healed
// NEAR: register allocation (this in edi, the unit in ebx) and the element access (the original calls begin() and
// operator+ out of line).
long EEE3DDamageSmoke::Step()
{
    GETransformation theLocation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePoint;
    long theResult = 0;
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    EETagPointList* theList = this->mUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0);
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    float theScale = theUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    float theHealth = (float)this->mUnit->mHitPoints;
    theHealth /= this->mUnit->mStats->mMaxHitPoints;
    this->mSinceEmit += gPhysics->GetTickTime();
    long theCommand = theUnit->mCommand ? theUnit->mCommand->mType : 0;
    if (theCommand != kActionDead && theHealth < theEffect->GetParam2()) {
        unsigned long theInterval =
            UMFloatToLong((theEffect->mTime2 - theEffect->mTime1) * (theHealth / theEffect->GetParam2()) + theEffect->mTime1);
        if (this->mSinceEmit >= theInterval) {
            if (theList) {
                // Scale only a list that exists (a model without type-0 tag points has none).
                EETagPointList::Scale(theLocation, theScale);
                for (unsigned long i = 0; i < theList->mPoints.size(); i++) {
                    theList->mPoints[i]->GetPoint(theFrame, theLocation, thePoint);
                    thePoint.SetX(EEDamageSmokeJitter(thePoint.GetX(), theEffect->GetParam1()));
                    thePoint.SetY(EEDamageSmokeJitter(thePoint.GetY(), theEffect->GetParam1()));
                    thePoint.SetZ(EEDamageSmokeJitter(thePoint.GetZ(), theEffect->GetParam1()));
                    theResult = gEffectManager->CreatePlacedEffect(thePoint, theEffect->mLinkedEffect1, NULL);
                }
            } else {
                const GETransformation& theUnitLocation = theUnit->mPhysicalModel.GetLocation();
                thePoint.SetX(theUnitLocation.GetX());
                thePoint.SetY(theUnitLocation.GetY());
                thePoint.SetZ(theUnitLocation.GetZ());
                thePoint.SetX(EEDamageSmokeJitter(thePoint.GetX(), theEffect->GetParam1()));
                thePoint.SetY(EEDamageSmokeJitter(thePoint.GetY(), theEffect->GetParam1()));
                thePoint.SetZ(EEDamageSmokeJitter(thePoint.GetZ(), theEffect->GetParam1()));
                theResult = gEffectManager->CreatePlacedEffect(thePoint, theEffect->mLinkedEffect1, NULL);
            }
            this->mSinceEmit -= theInterval;
        }
    } else {
        theResult = 7;
    }
    return theResult;
}
