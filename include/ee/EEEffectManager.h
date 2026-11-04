// C:\EEX\Empire Earth\Graphics Thread\EEEffectManager.h (RECONSTRUCTION; file name inferred). The graphics
// thread's effects (vtable 0x4411c0; EEEffectManager.cpp around 0x1054f6). The game holds one through
// `gEffectManager` (0x53185c, provisional name). Method names are provisional (by behaviour). Requires lle/GE3DPoint.h.
#ifndef EE_EEEFFECTMANAGER_H
#define EE_EEEFFECTMANAGER_H

#include <map>
#include <vector>
#include "lle/FSRestorable.h"
#include "ee/EEGraphicalGoalManager.h"

class EEGraphicEffect;
class EEUnit;
class GE3DLine;
class GETransformation;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

// EEEffectHandle / kNoEffectHandle and the effect class itself: ee/EEEffect.h
#include "ee/EEEffect.h"

// RTTI EEEffectManager > FSRestorable (vtable 0x4411c0; ctor 0x10552f; sizeof 0x34). The world owns it
// (gWorld.mEffectManager, +0xb1c = the address of gEffectManager below). Layout.
class EEEffectManager : public FSRestorable {
public:
    EEEffectManager();                      // 0x10552f
    // 0x1074c9: hands the tile area to the +0x20 object's slot +0x50 (EESEAPaintElevation::Virtual4)
    // (provisional name)
    void ElevationChanged(long inLeft, long inTop, long inRight, long inBottom);
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x106978
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x08 0x1068d8
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x107305
    virtual ~EEEffectManager();             // +0x10 (??_G 0x10558c)
    // +0x14 0x107430: a unit swap (EEPhysics 0x153430); forwards both to every effect's slot +0x20
    virtual void ReplaceUnit(EEUnit* inOld, EEUnit* inNew);
    void Update();                          // 0x107450 (EEPhysics::Tick, every tick) (provisional name)
    void ClearEffects();                    // 0x106889 (EEWorld::Clear/Virtual10; provisional name)

    EEEffect* GetEffect(EEEffectHandle inHandle);   // 0x106852: 0 if none (provisional name; EEFireFunctor::Cure)
    // 0x106038: an effect of inEffect's kind at inLocation (outHandle: its handle)
    long CreateEffect(EEUnit* inUnit, GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle,
                      long inUnknown);
    // 0x1061e9: the same for a placed effect (outHandle may be 0: the logic bomb's first effect, 0xb2408)
    long CreatePlacedEffect(GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    long CreatePlacedEffect(const GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
        { return CreatePlacedEffect(const_cast<GE3DPoint&>(inLocation), inEffect, outHandle); }
#endif
    // 0x106596: an effect placed on a line (EEE3DTeleport 0xfbb22 passes a GE3DLine local: its position and a vertical
    // direction; EEE3DMultiEffect's second ctor 0xf42c3 calls it and is created by it) (provisional name)
    long CreateLineEffect(GE3DLine& inLine, EEGraphicEffect* inEffect, EEEffectHandle* outHandle);
    long DestroyEffect(EEEffectHandle inHandle);      // 0x1068c6
    // 0x10667c: an effect drawn from inFrom to inTo (the plague's kind 0x23) (provisional name)
    long CreateEffectBetween(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inEffect, EEEffectHandle* outHandle);
    // 0x105e21: an effect from inFrom to inTo (the plague's kind 0x40) (provisional name)
    long CreateEffectBetweenUnits(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inEffect, EEEffectHandle* outHandle);
    // 0x10562c: an effect on inUnit (outHandle may be 0; the morale calamity keeps it) (provisional name)
    // (inArg4: EEE2DLightGlow's flag; inArg5: EEE3DParticleEmitter's; inArg6: EEE3DCommandConfirmation's)
    long CreateUnitEffect(EEUnit* inUnit, EEGraphicEffect* inEffect, EEEffectHandle* outHandle, bool inKeepsUnit,
                          long inTagPoint, long inGoalIndex);
    // 0x1064e6: an effect on a transformation (kinds 0x22, 0x32) (provisional name)
    long CreateTransformationEffect(const GETransformation& inTransformation, EEGraphicEffect* inEffect,
                                    EEEffectHandle* outHandle);
    // 0x10673e: a weather effect over inTiles (kinds 0x2a rain, 0x3d snow) (provisional name)
    long CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect,
                             EEEffectHandle* outHandle);
    void AddEffect(EEEffect* inEffect, bool inNewHandle);   // 0x1055d0 (links and maps it) (provisional name)
    void RemoveEffect(EEEffect* inEffect);  // 0x1072d0 (unlinks and dereferences it) (provisional name)
    long DestroyQueued();                   // 0x1067ce (the handles DestroyEffect queued) (provisional name)

    // 0x4e8e38 "EEEffectManager-Data": a class static (its atexit dtor 0x105513 is guarded) (provisional name)
    static const UString sData;

    std::map<unsigned long, EEEffect*> mEffectsByHandle;  // +0x04 the effects by handle (find 0x820bd compares the key with jb)
    std::vector<unsigned long> mDestroyedHandles;  // +0x10 the handles DestroyEffect queued (saved)
    EEEffect* mEffects;                     // +0x20 the effect list (= 0; linked by EEEffect::mNext; EEE2DBattlefieldAmbient 0xde5ca)
    EEGraphicalGoalManager mGoals;          // +0x24 (ctor 0x10779d; sizeof 0x10)
};

extern EEEffectManager* gEffectManager;     // 0x53185c

#endif
