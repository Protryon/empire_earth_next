// C:\EEX\Empire Earth\Graphics Thread\EEEDamagedEffect.cpp (no __FILE__; name from the RTTI class and its key
// "EEEDamagedEffect-Data"). 0x101fce..0x10251d: the key chain, both ctors, the dtor, Virtual24, Load, Save, Virtual4C.
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
#include "ee/EEWorld.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEDamagedEffect.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x501fce
// SYNTHETIC: EXE 0x501fd8
// SYNTHETIC: EXE 0x501fe9
// SYNTHETIC: EXE 0x501ff5
const UString EEEDamagedEffect::sData("EEEDamagedEffect-Data");

// FUNCTION: EXE 0x502011
// Creates the damage controller effect for a unit, not yet finished
EEEDamagedEffect::EEEDamagedEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffectDamaged, false)
{
}

// SYNTHETIC: EXE 0x50205b
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x502077
// Creates an empty damage controller effect for loading from a saved game
EEEDamagedEffect::EEEDamagedEffect()
    : EEEffect2D(gEffect2DModel, kEffectDamaged, false)
{
}

// FUNCTION: EXE 0x5020ba
// Destroys the damage controller (members only; the child effects are not destroyed here)
EEEDamagedEffect::~EEEDamagedEffect()
{
}

long EEEDamagedEffect::DrawPass2()
{
    return 0;
}

long EEEDamagedEffect::DrawPass4()
{
    return 0;
}

long EEEDamagedEffect::DrawPass5()
{
    return 0;
}

long EEEDamagedEffect::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x5020dc
// Queues nothing: the controller draws nothing itself and returns 10
long EEEDamagedEffect::Queue(EEEffectQueue* inQueues)
{
    return 10;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEEDamagedEffect::PrepareQueue()
{
}

long EEEDamagedEffect::SetUp()
{
    return 0;
}

// FUNCTION: EXE 0x5020e2
// Reads the handles of the child damage effects (and their secondary partners when saved) and, from save
// version 0.249 on, the finished flag (ee_gfx effect_state.rs); returns the first read error (the base class result
// is ignored)
long EEEDamagedEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_effect_load_damaged(this->mRecord, &theFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// FUNCTION: EXE 0x5021a2
// Writes whether a secondary effect is linked, the child effect handles (paired with their secondaries) and
// the finished flag (ee_gfx effect_state.rs); returns the first write error (the base class result is ignored)
long EEEDamagedEffect::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save_damaged(this->mRecord, &theFile, this->mGraphicEffect->mLinkedEffect2 != NULL);
}

// The world the controller's Rust tick calls into: its children made and destroyed (provisional name)
struct EEDamagedHost {
    static EEEDamagedEffect& Self(void* inContext) { return *static_cast<EEEDamagedEffect*>(inContext); }
    static uint32_t Create(void* inContext, bool inSecondary, uint32_t inIndex)
    {
        EEEDamagedEffect& theEffect = Self(inContext);
        EEEffectHandle theHandle;
        EEGraphicEffect* theLinked = inSecondary ? theEffect.GetGraphicEffect()->GetLinkedEffect2()
                                                 : theEffect.GetGraphicEffect()->GetLinkedEffect1();
        gWorld.GetEffectManager()->CreateUnitEffect(theEffect.GetUnit(), theLinked, &theHandle, 0, inIndex, 0);
        return theHandle.mValue;
    }
    static void Destroy(void*, uint32_t inHandle)
    {
        EEEffectHandle theHandle = {inHandle};
        gWorld.GetEffectManager()->DestroyEffect(theHandle);
    }
};

// FUNCTION: EXE 0x50225c
// Keeps one child damage effect (plus its secondary) per damage tag point, more as the unit's health drops;
// hides them all on a cloaked enemy unit, and once finished destroys them and returns 7 (remove) (the counts and
// the children's handles are ee_gfx effect_state.rs's)
long EEEDamagedEffect::Step()
{
    EeGfxDamagedTick theTick = {};
    EEGraphicEffect* theEffect = this->GetGraphicEffect();
    theTick.has_linked2 = theEffect->GetLinkedEffect2() != NULL;
    theTick.param2 = theEffect->GetParam2();
    EEUnit* theUnit = this->mUnit;
    if (theUnit && !this->mIsFinished) {
        theTick.has_unit = true;
        if (theUnit->mTileNode) {
            while (theUnit->mComplexUnit->mPreviousPart)
                theUnit = theUnit->mComplexUnit->mPreviousPart;
        }
        EETagPointList* theList;
        if (theUnit->mTileNode)
            theList = theUnit->GetModel()->GetTagPoints(theUnit->mComplexUnit->GetAnimation(),
                                                        theEffect->GetLinkedEffect1()->mTagPointKind);
        else
            theList = theUnit->GetModel()->GetTagPoints(0, theEffect->GetLinkedEffect1()->mTagPointKind);
        theTick.tag_count = theList ? (uint32_t)theList->GetNumTagPoints() : 1;
        theTick.hit_points = (int32_t)theUnit->GetHitPoints();
        theTick.max_hit_points = (int32_t)theUnit->GetStats()->GetMaxHitPoints();
        if (theUnit->GetTileNode()) {
            EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
            if (theComplexUnit->GetIsCloaked() && !gWorld.GetCurrentPlayer()->IsAlly(theComplexUnit->GetPlayer()))
                theTick.hidden = true;
        }
    }
    EeGfxDamagedHost theHost = {this, EEDamagedHost::Create, EEDamagedHost::Destroy};
    return ee_gfx_effect_damaged_step(this->mRecord, &theTick, &theHost);
}
