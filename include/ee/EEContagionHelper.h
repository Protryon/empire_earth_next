// C:\EEX\Empire Earth\Calamities\EEContagionHelper.h (RECONSTRUCTION; file name inferred). One spreading
// Layout proven: EEContagionHelper sizeof 0x38 = its new-site size, ctor 0xb5c89 exact in EEContagionHelper.cpp. Member
// names are provisional.
// calamity's helper: the units it has reached (RTTI EEContagionHelper > FSRestorable; vtable 0x43cf58; sizeof 0x38;
// EEContagionHelper.cpp 0xb5b7d..0xb662f, its four save keys' init chains first). Names are provisional (by behaviour;
// the members are defined in alphabetical order). Requires lle/LowLevelEngine.h, lle/FSRestorable.h, <stdio.h>.
#ifndef EE_EECONTAGIONHELPER_H
#define EE_EECONTAGIONHELPER_H

class EECalamity;
class EEComplexUnit;
class EEContagionFunctor;
class EEUnit;

class EEContagionHelper : public FSRestorable {
public:
    EEContagionHelper();                    // 0xb5c89 (EEContagionManager::Load)
    EEContagionHelper(EEContagionFunctor* inFunctor, EECalamity* inCalamity);   // 0xb5ce9
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0xb5feb
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0xb6233
    virtual ~EEContagionHelper();           // +0x10 0xb5d3b (??_G 0xb5ccd)
    virtual void DumpOutOfSynch(FILE* inFile) const;   // +0x14 0xb5e26
    virtual long GetOutOfSynchChecksum() const;   // +0x18 0xb5eb0
    virtual long RestorePointers();         // +0x1c 0xb61ca: the loaded unit ids -> units

    EECalamity* GetCalamity() const { return this->mCalamity; }  // inline (EEContagionManager::Save loads it early)
    bool AddUnit(EEUnit* inUnit, bool inNotify);   // 0xb5d88: false if it is there already or dead
    // 0xb5ef9: the units within inRadius of inUnit (none while it is inside a container)
    void GetUnitsInArea(float inRadius, EEComplexUnit* inUnit, std::vector<EEComplexUnit*>& outUnits);
    bool RemoveUnit(EEUnit* inUnit, bool inNotify);   // 0xb60f7
    std::vector<EEUnit*>::iterator RemoveUnit(std::vector<EEUnit*>::iterator inUnit, bool inNotify);   // 0xb616e
    void SetCalamity(EECalamity* inCalamity);   // 0xb62c9
    void SetFunctor(EEContagionFunctor* inFunctor);   // 0xb62d3
    void SetHelperID(long inHelperID);      // ICF-folded (0x1c1c)
    void Spread(float inElapsedTime);       // 0xb62dd: infects the units near the infected ones
    void SwapUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);   // 0xb64e8: inNewUnit replaces inOldUnit
    void Update(float inElapsedTime);       // 0xb6552

    EEUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEUnit*> mUnits;            // +0x04 (each referenced)
    EEContagionFunctor* mFunctor;           // +0x14
    EECalamity* mCalamity;                  // +0x18
    long mHelperID;                         // +0x1c
    float mTimeSinceLastSpread;             // +0x20
    float mTimeSinceLastUpdate;             // +0x24
    unsigned long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<unsigned long> mUnitIDs;    // +0x28 (Load; RestorePointers turns them into mUnits)

    // the save keys (init chains 0xb5b7d..0xb5c89; guard 0x4e6008; not used by Load/Save)
    static const UString mNumUnitsStringID;
    static const UString mUnitIDStringID;
    static const UString mHelperIDStringID;
    static const UString mLastSpreadTimeIDStringID;
};

#endif
