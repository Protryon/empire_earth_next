// C:\EEX\Empire Earth\Calamities\EEContagionManager.h (RECONSTRUCTION; file name inferred). The world's
// Layout proven: EEContagionManager sizeof 0x28 = its new-site size, ctor 0xb6723 exact in EEContagionManager.cpp.
// Member names are provisional.
// contagion helpers (one per spreading calamity), by id (RTTI EEContagionManager > FSRestorable; vtable 0x43cf7c;
// EEContagionManager.cpp 0xb662f..0xb6bd1, its three save keys' init chains first). Method names from the calamity
// asserts ("::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID)", EECCAEExtraDamage.cpp);
// the other names are provisional. Requires lle/LowLevelEngine.h, lle/FSRestorable.h, <stdio.h>.
#ifndef EE_EECONTAGIONMANAGER_H
#define EE_EECONTAGIONMANAGER_H

class EECalamity;
class EEContagionFunctor;
class EEContagionHelper;
class EEUnit;

class EEContagionManager : public FSRestorable {
public:
    EEContagionManager();
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0xb6940
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0xb6abe
    virtual ~EEContagionManager();          // +0x10 0xb677a (??_G 0xb675e)
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0xb6851
    virtual long GetOutOfSynchChecksum();   // +0x18 0xb68e8
    // +0x1c 0xb69ef: the loaded helpers' calamities by index (helpers whose calamity is gone are destroyed)
    virtual long RestorePointers(std::vector<EECalamity*>& inCalamities);

    // 0xb6795: the new helper's id (the plague passes its target unit, the area effects 0)
    long CreateContagionHelper(EEContagionFunctor* inFunctor, EECalamity* inCalamity, EEUnit* inTargetUnit);
    bool DestroyContagionHelper(long inHelperID);
    // 0xb68c3: inCalamities[inIndex], 0 past the end
    EECalamity* GetCalamity(std::vector<EECalamity*>& inCalamities, unsigned long inIndex);
    EEContagionHelper* GetContagionHelper(long inHelperID);      // 0xb691d: 0 if none
    void SwapUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);         // 0xb6b4f (every helper's)

    std::vector<EEContagionHelper*> mHelpers;   // +0x04 (owned)
    long mNextHelperID;                     // +0x14 (the last id handed out)
    std::vector<unsigned long> mCalamityIndexes;   // +0x18 (Load: each helper's calamity index, for RestorePointers)

};

#endif
