// C:\EEX\Empire Earth\AI Thread\EECPASGAirTransport.h (RECONSTRUCTION; file name inferred).
// RTTI EECPASGAirTransport > EECPASGAir (> EECPAttackSubGroup > UReferenceCounter) at +0, EECPASGTransport at
// +0x1e8; vtables 0x43a8e0 (+0) and 0x43aadc (+0x1e8); sizeof 0x1f8 (no member of its own); ctor 0x440cd (inGroup,
// inArg2: EECPASGAir's, EECPASGTransport's, then mLand = 0 and mOwner = this in the body), ??_G 0x4410b, dtor
// 0x44127. Slot signatures: the introducing classes'. Names are provisional.
#ifndef EE_EECPASGAIRTRANSPORT_H
#define EE_EECPASGAIRTRANSPORT_H

#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGAir.h"
#include "ee/EECPASGTransport.h"

class EECPASGAirTransport : public EECPASGAir, public EECPASGTransport {
public:
    EECPASGAirTransport(EECPAttackGroup* inGroup, long inKind);   // 0x440cd
    virtual ~EECPASGAirTransport();         // +0x00 (0x4410b)
    virtual void SetTask(const EECPAGTask& inTask);     // +0x14 0x44150
    // +0x1c 0xac98e (`ret 0xc`)
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void Shutdown();                // +0x30 0x4e76a
    virtual float GetDangerRatio(long inArg1);  // +0x44 0x509f2
    virtual void GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights);   // +0x54 0x459d6
    virtual EETileLocation GetRallyPoint(bool inArg);  // +0x88 0x441cb
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x44643
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x449fb
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion); // +0x110 0x44ada
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // +0x114 0x50fa8
    virtual bool TestCondition(long inArg1, long inArg2); // +0x12c 0x44b11
    virtual bool RemoveUnit(EEComplexUnit* inUnit); // +0x130 0x5110d
    virtual void RequestRemoval();          // +0x160 0x4454f
    virtual void Update(long inArg1);       // +0x168 0x447e3
    virtual void Launch(unsigned long inTime);       // +0x1e8 0x443f3
    virtual void GoIdle();                  // +0x1ec 0x4446b
    virtual void LoadCargo();               // +0x1f4 0x44740
    // EECPASGTransport's slots (the second vtable, at +0x1e8): ~ is a thunk (0x44e63), Load/Restore/Save are
    // thunks into the overrides above (both bases name them alike); the three transport kinds share the bodies (ICF)
    virtual long CountCargo();              // EECPASGTransport +0x04 0x44188
    virtual long GetCargoCapacity();        // EECPASGTransport +0x08 0x50ab8
    virtual void StartLoading();            // EECPASGTransport +0x14 0x44a58
    virtual void SetLand(EECPASGLand* inLand);              // EECPASGTransport +0x20 0x44b05
    virtual void UnloadAt(const EECPILocation& inLocation);  // EECPASGTransport +0x24 0x44c2f
};

#endif
