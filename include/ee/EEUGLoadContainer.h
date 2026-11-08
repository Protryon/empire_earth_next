// EEUGLoadContainer.h (RECONSTRUCTION; file name inferred). Loading units into a container (RTTI EEUGLoadContainer >
// EEUnitGoal; vtable 0x4476b0; EEUGLoadContainer.cpp 0x22585a..0x226a43): the base of the garrison and transport
// goals. Member names from its out-of-synch dump 0x225cc5 (provisional name); sizeof 0x68 (EEUGLoadTransport
// adds no member: its new-site size). Requires lle/LowLevelEngine.h, lle/U2DPoint.h, lle/GE3DPoint.h.
#ifndef EE_EEUGLOADCONTAINER_H
#define EE_EEUGLOADCONTAINER_H

#include "lle/U2DPoint.h"
#include "ee/EEUnitGoal.h"
#include "ee/UReferenceContainer.h"

class EEUnit;
class EEUnitGoal;

class EEUGLoadContainer : public EEUnitGoal {
public:
    // 0x22585a: the container (referenced), the units to load, the two rendezvous tiles and the minimum distance
    EEUGLoadContainer(unsigned char inKind, EEComplexUnit* inContainer,
                      const UReferenceContainer<EEComplexUnit*>& inUnits, const U2DPoint<long>& inContainerTile,
                      const U2DPoint<long>& inUnitTile, float inMinimumDistanceSquared, bool inArg);
    // 0x225917: the units given as a list (sorted, duplicates dropped)
    EEUGLoadContainer(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                      bool inArg);
    EEUGLoadContainer(unsigned char inKind, bool inArg);   // 0x225a18 (the (bool) ctors of the subclasses)
    virtual ~EEUGLoadContainer();           // +0x00 0x225a80 (??_G 0x2258fb)
    // +0x04 0x2258ef: AddUnit (EEUnitGoal declares the slot with an unsigned long: provisional name)
    virtual void AddCreatedUnit(EEComplexUnit* inUnit);
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);  // +0x08 0x225b4b
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0x225cc5
    virtual long GetOutOfSynchChecksum();   // +0x18 0x225d83
    virtual void GetDebugLine(UWideString& ioText) = 0;  // +0x1c
    virtual void GetDebugText(UWideString& ioText);  // +0x20 0x225dfe
    virtual bool IsLoadGoal();              // +0x24 (ICF-folded 0x18aa9: true)
    virtual void GetGoalLocation(GE3DPoint& outLocation);  // +0x28 0x225f7b (the destination)
    virtual long GetMaxPathAttempts();      // +0x2c (ICF-folded 0xc1ee0: 6)
    virtual long GetStartAction();          // +0x30 0x227264 (0x1f40; EEUGLoadTransport.cpp's range)
    virtual void Step(EEComplexUnit* inUnit);        // +0x40 0x225f93
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) = 0;   // +0x44
    virtual void Abandon(EEComplexUnit* inUnit);     // +0x4c 0x226229
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);  // +0x50 0x22627f
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) = 0;   // +0x58
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0x64 0x2263b1
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);  // +0x6c 0x2265ae
    virtual bool Suits(EEUnit* inUnit);       // +0x70 (ICF-folded 0x220a18: true)
    virtual long GetUsedCapacity() = 0;     // +0x78
    virtual long GetCapacity() = 0;         // +0x7c
    virtual bool CanContain(EEComplexUnit* inUnit) = 0;  // +0x80 (can the container take inUnit)
    virtual long GetRemainingLoad() = 0;    // +0x84
    virtual long GetFreeCapacity() = 0;     // +0x88
    virtual void RemoveUnitAt(std::vector<EEComplexUnit*>::iterator inIterator);  // +0x8c 0x226219 (erases that unit from the list)
    virtual bool LoadUnit(EEComplexUnit* inUnit) = 0;    // +0x90 (loads inUnit)
    virtual void ComputeLoadDistance() = 0;  // +0x94 (sets mMinimumDistanceSquared)
    virtual void ComputeRendezvous() = 0;   // +0x98 (sets the destination and the rendezvous tiles)

    void AddUnit(EEComplexUnit* inUnit);    // 0x225ab4 (referenced, once) (provisional name)
    void AddUnits(const UReferenceContainer<EEComplexUnit*>& inUnits);   // 0x225ae9 (provisional name)
    void AddUnits(const std::vector<EEComplexUnit*>& inUnits);   // 0x225b0e (provisional name)
    void ClearUnitsIfFull();                // 0x225b33: empties the list when Virtual88 is 0 (provisional name)
    void ClearUnits();                      // 0x225cb0 (provisional name)
    bool IsLoadable(EEComplexUnit* inUnit); // 0x225efd (provisional name)
    // 0x2260c3 / 0x2262f3: the subclasses' Load/Save (with their own key) (provisional names)
    long LoadContainer(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile, const UString& inKey);
    long SaveContainer(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile, const UString& inKey);
    void RemoveUnit(EEComplexUnit* inUnit); // 0x2262e3 (provisional name)
    bool HasUnit(EEComplexUnit* inUnit);    // 0x226596 (provisional name)

    UReferenceContainer<EEComplexUnit*> mUnits;   // +0x20 "Loadable Unit List" (vtable 0x4429d8)
    unsigned long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<unsigned long> mUnitIDs;    // +0x34 (the ids between Load and Virtual50)
    GE3DPoint mDestination;                 // +0x44 "Actual Destination"
    EEComplexUnit* mContainer;              // +0x50 "Container Unit" (referenced)
    U2DPoint<long> mContainerTile;          // +0x54 "Container Rendevous Tile"
    U2DPoint<long> mUnitTile;               // +0x5c "Loadable Unit Rendevous Tile"
    float mMinimumDistanceSquared;          // +0x64 "Minimum Loading Distance Squared"
};

#endif
