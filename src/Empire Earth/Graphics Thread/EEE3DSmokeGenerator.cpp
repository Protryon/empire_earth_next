// C:\EEX\Empire Earth\Graphics Thread\EEE3DSmokeGenerator.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DSmokeGenerator-Data"). 0xf9871..0xf9b6d: the key chain, three ctors .. Virtual4C. Unit flags: the exe's module set.
// New (port, RUST_PLAN R4.4): also the original's EEE3DDamageSmoke.cpp (0xea703..0xea9e2) and EEE3DDestructionSmoke.cpp
// (0xebbba..0xebde9), as kinds 0x2c and 0x1e; the state, tick, set-up, load and save are ee_gfx effect_state.rs's.
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

// SYNTHETIC: EXE 0x4f9871
// SYNTHETIC: EXE 0x4f987b
// SYNTHETIC: EXE 0x4f988c
// SYNTHETIC: EXE 0x4f9898
const UString EEE3DSmokeGenerator::sData("EEE3DSmokeGeneratorData");

// FUNCTION: EXE 0x4f98b4
// Creates a smoke generator for a graphic effect at a fixed point, with its timers at zero (the record's)
EEE3DSmokeGenerator::EEE3DSmokeGenerator(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    GETransformation theTransformation;
    theTransformation.SetPosition(inLocation.GetX(), inLocation.GetY(), inLocation.GetZ());
    this->SetTransformation(theTransformation);
}

// SYNTHETIC: EXE 0x4f9929
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f9945
// Creates a smoke generator that follows a unit, with its timers at zero (the record's)
EEE3DSmokeGenerator::EEE3DSmokeGenerator(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
}

// FUNCTION: EXE 0x4ebbfd
// Creates the smoke of a destroyed unit at its location, spread over half the unit class's scale
EEE3DSmokeGenerator* EEE3DSmokeGenerator::CreateDestructionSmoke(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
{
    const GETransformation& theLocation = inUnit->mComplexUnit->mPhysicalModel.GetLocation();
    EEE3DSmokeGenerator* theEffect = new EEE3DSmokeGenerator(
        GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ()), inGraphicEffect, EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE);
    theEffect->mRecord->smoke.radius = inUnit->mUnitClass->mScale * 0.5f;
    return theEffect;
}

// FUNCTION: EXE 0x4f997f
// Creates an empty smoke generator of the given kind (for loading)
EEE3DSmokeGenerator::EEE3DSmokeGenerator(long inKind)
    : EEEffect3D(inKind)
{
}

// ICF-folded into EEE3DMultiEffect's 0xf43b8
EEE3DSmokeGenerator::~EEE3DSmokeGenerator()
{
}

long EEE3DSmokeGenerator::DrawPass2()
{
    return 0;
}

long EEE3DSmokeGenerator::DrawPass4()
{
    return 0;
}

long EEE3DSmokeGenerator::DrawPass5()
{
    return 0;
}

long EEE3DSmokeGenerator::DrawPass6()
{
    return 0;
}

// ICF-folded into 0x1020dc
long EEE3DSmokeGenerator::Queue(EEEffectQueue* inQueues)
{
    return 10;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DSmokeGenerator::PrepareQueue()
{
}

// The world the smoke generators' Rust tick and set-up call into: the damage smoke's tag points, the puffs and lines
// they create (mLinkedEffect1, as the originals) (provisional name)
struct EESmokeHost {
    EEE3DSmokeGenerator* mEffect;
    EETagPointList* mTagPoints;
    unsigned long mFrame;
    GETransformation mLocation;

    static EESmokeHost& Self(void* inContext) { return *static_cast<EESmokeHost*>(inContext); }
    static void TagPoint(void* inContext, uint32_t inIndex, EeGe3DPoint* outPoint)
    {
        EESmokeHost& theHost = Self(inContext);
        theHost.mTagPoints->mPoints[inIndex]->GetPoint(theHost.mFrame, theHost.mLocation,
                                                       *reinterpret_cast<GE3DPoint*>(outPoint));
    }
    static int32_t Place(void* inContext, const EeGe3DPoint* inPoint)
    {
        GE3DPoint thePoint = *reinterpret_cast<const GE3DPoint*>(inPoint);
        return (int32_t)gEffectManager->CreatePlacedEffect(thePoint, Self(inContext).mEffect->mGraphicEffect->mLinkedEffect1,
                                                           NULL);
    }
    static void Line(void* inContext, const EeGe3DPoint* inStart, const EeGe3DPoint* inDirection)
    {
        GE3DLine theLine;
        theLine.SetStart(*reinterpret_cast<const GE3DPoint*>(inStart));
        theLine.SetDirection(GEVector(inDirection->x, inDirection->y, inDirection->z));
        gEffectManager->CreateLineEffect(theLine, Self(inContext).mEffect->mGraphicEffect->mLinkedEffect1, NULL);
    }
    EeGfxSmokeHost Ops() { EeGfxSmokeHost theOps = {this, TagPoint, Place, Line}; return theOps; }
};

// FUNCTION: EXE 0x4ebc56
// The destruction smoke launches linked line effects outward in random directions from the centre, more for a
// bigger radius (ee_gfx effect_state.rs); the other kinds set up nothing; returns 0
long EEE3DSmokeGenerator::SetUp()
{
    if (this->mKind != EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE)
        return 0;
    EESmokeHost theHost = {this, NULL, 0};
    EeGfxSmokeHost theOps = theHost.Ops();
    EeGfxSmokeSetUp theSetUp;
    theSetUp.param1 = this->mGraphicEffect->GetParam1();
    theSetUp.param2 = this->mGraphicEffect->mParam2;
    theSetUp.int_param1 = (int32_t)this->mGraphicEffect->mIntParam1;
    return ee_gfx_effect_smoke_set_up(this->mRecord, &theSetUp, &theOps);
}

// FUNCTION: EXE 0x4f99b1
// Reads the generator's running time and time since the last puff (and the destruction smoke's radius) after the 3D
// effect's state (ee_gfx effect_state.rs); returns the first read error (the destruction smoke: the radius's)
long EEE3DSmokeGenerator::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_effect_load_smoke(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4f99e6
// Writes them (ee_gfx effect_state.rs); returns the first write error (the destruction smoke: the radius's)
long EEE3DSmokeGenerator::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save_smoke(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4f9a1b
// Emits puffs spread randomly by param 1 around its unit or point once the emit interval passes (the damage smoke
// 0xea75c: at the unit's tag points, the interval by its health) (ee_gfx effect_state.rs); returns 7 when it ends
long EEE3DSmokeGenerator::Step()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    EeGfxSmokeTick theTick = {};
    theTick.tick_seconds = gPhysics->GetTickSeconds();
    theTick.tick_time = gPhysics->GetTickTime();
    theTick.duration = (uint32_t)theEffect->mDuration;
    theTick.time1 = (uint32_t)theEffect->mTime1;
    theTick.time2 = (int32_t)theEffect->mTime2;
    theTick.param1 = theEffect->GetParam1();
    theTick.param2 = theEffect->GetParam2();
    EESmokeHost theHost = {this, NULL, 0};
    if (this->mKind == EE_GFX_EFFECT_KIND_DAMAGE_SMOKE) {
        // the damage smoke reads its unit whole (it is always on one)
        EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
        theHost.mLocation = theUnit->mPhysicalModel.GetLocation();
        theHost.mTagPoints = this->mUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0);
        theHost.mFrame = theUnit->mPhysicalModel.GetFrame();
        // Scale only a list that exists (a model without type-0 tag points has none).
        if (theHost.mTagPoints)
            EETagPointList::Scale(theHost.mLocation, theUnit->mPhysicalModel.GetScale());
        theTick.has_unit = true;
        const GETransformation& theUnitLocation = theUnit->mPhysicalModel.GetLocation();
        theTick.unit_position.x = theUnitLocation.GetX();
        theTick.unit_position.y = theUnitLocation.GetY();
        theTick.unit_position.z = theUnitLocation.GetZ();
        theTick.unit_command = theUnit->mCommand ? (int32_t)theUnit->mCommand->mType : 0;
        theTick.hit_points = (int32_t)this->mUnit->mHitPoints;
        theTick.max_hit_points = (int32_t)this->mUnit->mStats->mMaxHitPoints;
        theTick.has_tag_points = theHost.mTagPoints != NULL;
        theTick.tag_point_count = theHost.mTagPoints ? (uint32_t)theHost.mTagPoints->mPoints.size() : 0;
    } else if (this->mUnit) {
        GE3DPoint theLocation;
        this->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
        theTick.has_unit = true;
        theTick.unit_position = *reinterpret_cast<const EeGe3DPoint*>(&theLocation);
        theTick.unit_command = (int32_t)this->mUnit->mComplexUnit->GetCommandType();
    }
    EeGfxSmokeHost theOps = theHost.Ops();
    return ee_gfx_effect_smoke_step(this->mRecord, &theTick, &theOps);
}
