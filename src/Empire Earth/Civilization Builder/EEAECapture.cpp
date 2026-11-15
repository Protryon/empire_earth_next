// New (port), TEST ONLY: the area effects' records of the simulation capture (ee/EEAECapture.h; tools/sim_capture.py,
// every profile). Reads the world; writes nothing to it.
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/FSMemoryFile.h"

#include "ee/EETechCapture.h"
#include "ee/EEAECapture.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/EEAEManager.h"

namespace {

int EffectIndex(EEGraphicEffect* inEffect)
{
    if (inEffect == NULL)
        return -1;
    return (int)(std::find(EEGraphicEffect::sEffects.begin(), EEGraphicEffect::sEffects.end(), inEffect) -
                 EEGraphicEffect::sEffects.begin());
}

int MediaIndex(EEMedia* inMedia)
{
    if (inMedia == NULL)
        return -1;
    for (unsigned long i = 0; i < EEMedia::GetCount(); i++)
        if (EEMedia::Get(i) == inMedia)
            return (int)i;
    return -2;
}

// the morale-from-buildings count (dirty flag, cap, count); false for the other kinds
bool MoraleCount(EEAreaEffect* inEffect, bool& outDirty, long& outMax, long& outCount)
{
    if (inEffect->mKind != kAreaEffectMoraleFromBuildings)
        return false;
    outDirty = inEffect->mDirty;
    outMax = inEffect->mMax;
    outCount = inEffect->mCount;
    return true;
}

// an effect as its owner unit's id and its index in the owner's own list (as the running state saves it)
void Ref(EESimCaptureRecord& r, EEAreaEffect* inEffect)
{
    EEComplexUnit* theOwner = inEffect->mUnit;
    long theIndex = -1;
    if (theOwner != NULL)
        for (size_t i = 0; i < theOwner->mAreaEffects.mItems.size(); i++)
            if (theOwner->mAreaEffects.mItems[i] == inEffect)
                theIndex = (long)i;
    r.U32(theOwner != NULL ? (unsigned int)theOwner->GetUnitID() : 0xffffffffu);
    r.I32(theIndex);
    r.I32(inEffect->mTemplate->mKind);
}

void Own(EESimCaptureRecord& r, EEAreaEffect* e)
{
    r.I32(e->mTemplate->mKind);
    r.I32(e->mTemplate->mID);
    r.I32(e->mPlayer);
    r.F32(e->mLocation.mX);
    r.F32(e->mLocation.mY);
    r.I32(e->Checksum());
    r.I32(e->GetStrength());
    r.F32(e->GetRadius());
    r.F32(e->GetScale());
    r.U32(e->IsImmunity() ? 1 : 0);
    for (long theKind = 1; theKind <= 3; theKind++) {
        long theOut = -12345;
        bool theModified = e->Modify(theKind, 1000, theOut);
        r.U32(theModified ? 1 : 0);
        r.I32(theOut);
    }
    bool theDirty = false;
    long theMax = 0, theCount = 0;
    bool theIsMorale = MoraleCount(e, theDirty, theMax, theCount);
    r.U32((theIsMorale ? 1u : 0u) | (theDirty ? 2u : 0u));
    r.I32(theMax);
    r.I32(theCount);
    if (e->mUnit != NULL) {
        GE2DPoint thePoint = e->mLocation;
        r.U32(e->Covers((unsigned long)e->mPlayer, e->mUnit, thePoint) ? 1 : 0);
    } else {
        r.U32(2);
    }
}

void Templates()
{
    for (unsigned long i = 0; i < gAEManager.mTemplates.size(); i++) {
        EEAreaEffectTemplate* t = gAEManager.mTemplates[i];
        EESimCaptureRecord r(EECaptureTag("AETP"));
        r.U32(i);
        r.U32(t != NULL ? 1 : 0);
        if (t != NULL) {
            r.I32(t->mKind);
            r.I32(t->mID);
            r.I32(t->mShape);
            r.I32(t->mMode);
            r.F32(t->mRadius);
            r.I32(t->mUnitSet);
            r.I32(t->mMaxLevel);
            r.I32(t->mLevel);
            r.I32(t->mL44);
            r.I32(t->mL48);
            r.I32(t->mL4C);
            r.F32(t->mRed);
            r.F32(t->mGreen);
            r.F32(t->mBlue);
            r.F32(t->mAlpha);
            r.F32(t->mF60);
            r.U32((t->mB64 ? 1u : 0u) | (t->mB65 ? 2u : 0u) | (t->mB66 ? 4u : 0u) | (t->mB67 ? 8u : 0u) |
                  (t->mB68 ? 16u : 0u));
            r.F32(t->mColorRed);
            r.F32(t->mColorGreen);
            r.F32(t->mColorBlue);
            r.F32(t->mColorAlpha);
            r.I32(EffectIndex(t->mGraphicEffect));
            r.I32(EffectIndex(t->mGraphicEffect18));
            r.I32(MediaIndex(t->mMedia));
        }
        r.Commit();
    }
}

}   // namespace

void EEAECaptureTick(float inElapsed)
{
    if (!EESimCaptureRecord::Active())
        return;
    static bool sTemplates = false;
    if (!sTemplates) {
        sTemplates = true;
        Templates();
    }
    EESimCaptureRecord r(EECaptureTag("AEFX"));
    r.F32(inElapsed);
    unsigned long theUnits = 0;
    for (unsigned long p = 1; p < ::gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = ::gWorld.mPlayers[p];
        if (thePlayer == NULL)
            continue;
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* u = theUnit->mComplexUnit;
            if (u == NULL || (u->mAreaEffects.mItems.size() == 0 && u->mAffectingAreaEffects.mItems.size() == 0))
                continue;
            theUnits++;
            r.U32((unsigned int)u->GetUnitID());
            r.I32(u->mHitPoints);
            r.U32((u->mIsCloakable ? 1u : 0u) | (u->mIsCloaked ? 2u : 0u));
            r.U32((unsigned int)u->mAreaEffects.mItems.size());
            for (size_t i = 0; i < u->mAreaEffects.mItems.size(); i++)
                Own(r, u->mAreaEffects.mItems[i]);
            r.U32((unsigned int)u->mAffectingAreaEffects.mItems.size());
            for (size_t i = 0; i < u->mAffectingAreaEffects.mItems.size(); i++)
                Ref(r, u->mAffectingAreaEffects.mItems[i]);
        }
    }
    r.U32(theUnits);
    r.U32((unsigned int)gAEManager.mEffects.mItems.size());
    for (size_t i = 0; i < gAEManager.mEffects.mItems.size(); i++)
        Ref(r, gAEManager.mEffects.mItems[i]);
    FSMemoryFile theFile;
    EEAESaveRunningState(theFile);
    long theLength = 0;
    theFile.GetPosition(theLength);
    r.U32((unsigned int)theLength);
    if (theLength > 0)
        r.Bytes(theFile.GetData(), (unsigned int)theLength);
    r.Commit();
}
