// C:\EEX\Empire Earth\AI Thread\EECPASGSpaceTransport.h (RECONSTRUCTION; file name inferred). RTTI
// EECPASGSpaceTransport > EECPASGSpaceship (> EECPAttackSubGroup > UReferenceCounter) at +0,
// EECPASGTransport at +0x1e8; vtables 0x43b7cc (+0) and 0x43b9b4 (+0x1e8); sizeof 0x1f8 (no member of its own); ctor
// 0x5096f (inGroup, inArg2: EECPASGSpaceship's, EECPASGTransport's, then mLand = 0 and mOwner = this in the body),
// ??_G 0x509ad, dtor 0x509c9. Slot signatures: the introducing classes'. Names are provisional.
#ifndef EE_EECPASGSPACETRANSPORT_H
#define EE_EECPASGSPACETRANSPORT_H

#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPASGTransport.h"

class EECPASGSpaceTransport : public EECPASGSpaceship, public EECPASGTransport {
public:
    EECPASGSpaceTransport(EECPAttackGroup* inGroup, long inKind);   // 0x5096f
    virtual ~EECPASGSpaceTransport();       // +0x00 (0x509ad)
    virtual void SetTask(const EECPAGTask& inTask);     // +0x14 0x41a2e
    // +0x1c 0xac98e (`ret 0xc`)
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void Shutdown();                // +0x30 0x4e76a
    virtual float GetDangerRatio(long inArg1);  // +0x44 0x509f2
    virtual EETileLocation GetRallyPoint(bool inIncludeAllies);  // +0x88 0x50af9
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x50c4a
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x50ddc
    // +0x100 0x4ec4b
    virtual void SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag);
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion); // +0x110 0x44ada
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // +0x114 0x50fa8
    virtual bool TestCondition(long inArg1, long inArg2); // +0x12c 0x50fd9
    // EECPASGSeaTransport::IsLoadingBlocked's twin (ICF-folded into 0x4ed08) (provisional name)
    bool IsLoadingBlocked();
    virtual bool RemoveUnit(EEComplexUnit* inUnit); // +0x130 0x5110d
    virtual void PlaceFormation(const EECPILocation& inLocation);  // +0x150 0x4e781
    virtual void RequestRemoval();          // +0x160 0x4e801
    virtual void Update(long inArg1);       // +0x168 0x4e9bb
    virtual void MoveToDestination();       // +0x170 0x50e95
    virtual void LoadCargo();               // +0x1e0 0x44740
    // EECPASGTransport's slots (the second vtable, at +0x1e8): ~ is a thunk (0x5117e), Load/Restore/Save are
    // thunks into the overrides above (both bases name them alike); the three transport kinds share the bodies (ICF)
    virtual long CountCargo();              // EECPASGTransport +0x04 0x44188
    virtual long GetCargoCapacity();        // EECPASGTransport +0x08 0x50ab8
    virtual void StartLoading();            // EECPASGTransport +0x14 0x44a58
    virtual void SetLand(EECPASGLand* inLand);              // EECPASGTransport +0x20 0x44b05
    virtual void UnloadAt(const EECPILocation& inLocation);  // EECPASGTransport +0x24 0x44c2f
};

#endif
