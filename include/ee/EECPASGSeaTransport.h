// C:\EEX\Empire Earth\AI Thread\EECPASGSeaTransport.h (RECONSTRUCTION; file name inferred).
// RTTI EECPASGSeaTransport > EECPASGSea (> EECPAttackSubGroup > UReferenceCounter) at +0, EECPASGTransport at
// +0x1e8; vtables 0x43b1d8 (+0) and 0x43b3c4 (+0x1e8); sizeof 0x1f8 (no member of its own); ctor 0x4e6e7 (inGroup,
// inArg2: EECPASGSea's, EECPASGTransport's, then mLand = 0 and mOwner = this in the body), ??_G 0x4e725, dtor
// 0x4e741. Slot signatures: the introducing classes'. Names are provisional.
#ifndef EE_EECPASGSEATRANSPORT_H
#define EE_EECPASGSEATRANSPORT_H

#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGSea.h"
#include "ee/EECPASGTransport.h"

class EECPASGSeaTransport : public EECPASGSea, public EECPASGTransport {
public:
    EECPASGSeaTransport(EECPAttackGroup* inGroup, long inKind);   // 0x4e6e7
    virtual ~EECPASGSeaTransport();         // +0x00 (0x4e725)
    virtual void SetTask(const EECPAGTask& inTask);     // +0x14 0x41a2e
    // +0x1c 0xac98e (`ret 0xc`)
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void Shutdown();                // +0x30 0x4e76a
    // +0x40 0x4f114 / +0x64 0x4f128: the bodies sit in EECPASGSeaTransport.cpp's range, so this class overrides them
    // (the identical EECPASGSea/Spaceship overrides fold into these copies: ICF)
    virtual bool FindPosition(EETileLocation& outTile, bool inArg2, bool inForce);
    virtual long GetNumTargets();
    virtual float GetDangerRatio(long inArg1);  // +0x44 0x509f2
    virtual EETileLocation GetRallyPoint(bool inIncludeAllies);  // +0x88 0x50af9
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x4e829
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x50ddc
    // +0x100 0x4ec4b
    virtual void SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag);
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion); // +0x110 0x44ada
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // +0x114 0x50fa8
    virtual bool TestCondition(long inArg1, long inArg2); // +0x12c 0x50fd9
    // 0x4ed08 (TestCondition's state 5005): an unfinished unit stands next to a loading unit with no free tile on
    // either side (provisional name)
    bool IsLoadingBlocked();
    virtual bool RemoveUnit(EEComplexUnit* inUnit); // +0x130 0x5110d
    virtual void PlaceFormation(const EECPILocation& inLocation);  // +0x150 0x4e781
    virtual void RequestRemoval();          // +0x160 0x4e801
    virtual void Update(long inArg1);       // +0x168 0x4e9bb
    virtual bool DecideSplit();             // +0x16c 0x27c0b4 (`xor al, al; ret`)
    virtual void MoveToDestination();       // +0x170 0x50e95
    virtual void LoadCargo();               // +0x1e4 0x44740
    // EECPASGTransport's slots (the second vtable, at +0x1e8): ~ is a thunk (0x4f0bb), Load/Restore/Save are
    // thunks into the overrides above (both bases name them alike); the three transport kinds share the bodies (ICF)
    virtual long CountCargo();              // EECPASGTransport +0x04 0x44188
    virtual long GetCargoCapacity();        // EECPASGTransport +0x08 0x50ab8
    virtual void StartLoading();            // EECPASGTransport +0x14 0x44a58
    virtual void SetLand(EECPASGLand* inLand);              // EECPASGTransport +0x20 0x44b05
    virtual void UnloadAt(const EECPILocation& inLocation);  // EECPASGTransport +0x24 0x44c2f
};

#endif
