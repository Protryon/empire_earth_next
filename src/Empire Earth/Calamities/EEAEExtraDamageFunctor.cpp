// C:\EEX\Empire Earth\Calamities\EEAEExtraDamageFunctor.cpp (file name inferred: no __FILE__; the class name, as the
// sibling functor units; alphabetical before EEAEMindControlFunctor.cpp and EEAreaEffectFunctor.cpp, then
// EECalamity.cpp). The extra-damage area effect's functor (RTTI EEAEExtraDamageFunctor, vtable 0x43c5b0):
// 0xac7b9..0xac891 (the base's inline GetRadius default COMDAT 0xac7e2 sits after the ctor). Its empty Apply is
// ICF-folded into EEAEMindControlFunctor's 0xac98e. Unit flags: the exe module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

extern EEEffectManager*& gEffectManager;

// FUNCTION: EXE 0x4ac7b9
// Creates the extra-damage area functor for a unit set and damage mode; the flag picks the pulse cannon effect
EEAEExtraDamageFunctor::EEAEExtraDamageFunctor(unsigned long inValue, long inMode, bool inFlag)
    : EEAreaEffectFunctor(NULL, NULL, 0, 0, inValue, inMode)
{
    this->mFlag = inFlag;
}

// TEMPLATE: EXE 0x4ac7e2
// ?GetRadius@EEContagionFunctor@@UAEMXZ (the base's inline default)

// FUNCTION: EXE 0x4ac7e9
// Destroys the functor and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x4ac805
// Destroys the functor (the base destructor runs twice, as in the original)
// the body calls the base destructor itself (the original: two calls of 0xac9e7, the second the implicit one)
EEAEExtraDamageFunctor::~EEAEExtraDamageFunctor()
{
    EEAreaEffectFunctor::~EEAreaEffectFunctor();
}

// FUNCTION: EXE 0x4ac81c
// Marks an enemy of the caster (any unit when there is no caster) with its class's pulse cannon or
// battle cry effect and gives it one more negative morale
void EEAEExtraDamageFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (theCaster != NULL && (inUnit->mPlayer == theCaster->mPlayer
                              || theCaster->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] == 0))
        return;
    EEGraphicEffect* theEffect;
    if (this->mFlag)
        theEffect = inUnit->mUnitClass->mPulseCannonEffect;
    else
        theEffect = inUnit->mUnitClass->mBattleCryEffect;
    if (theEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, false, 0, 0);
    inUnit->mComplexUnit->mNegativeMorale++;
}

// FUNCTION: EXE 0x4ac881
// Takes the unit's extra negative morale away again
void EEAEExtraDamageFunctor::Cure(EEUnit* inUnit, EECalamity* inCalamity)
{
    inUnit->mComplexUnit->mNegativeMorale--;
}

void EEAEExtraDamageFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
}
