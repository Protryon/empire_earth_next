// C:\EEX\Empire Earth\AI Thread\EECPUMGatherUnitsScreenBase.h (RECONSTRUCTION; file name inferred). The unit
// manager's gatherer function objects: EECPUnitManager 0x93ed4 walks the player's
// units (slot +0x0c per class with its deque, or slot +0x08 per unit when mB0E), then the pending requests (slot +0x04
// with an EECPUMRequest) and the tech tree's upgrades, as the three flags ask. The base (RTTI
// EECPUMGatherUnitsScreenBase, vtable 0x43a4ac: ??_G 0x7895f, slots 0x2b1c1/0x2b1da/0x2b1ec) has its ctor (0x78937,
// an out-of-line COMDAT after its first user EECPDefenseCommander 0x78866: members before the vptr) and ??_G in
// EECPDefenseCommander.cpp's range; its slots and those of the gatherers declared here sit before EEComputerPlayer.cpp
// (0x29f57..0x2b549) and are not decompiled yet (declared, not defined). Names are provisional (slot names by what
// they count; alphabetical as this developer declares). Requires <deque>.
#ifndef EE_EECPUMGATHERUNITSSCREENBASE_H
#define EE_EECPUMGATHERUNITSSCREENBASE_H

#include <deque>

class EEComplexUnit;
class EEComputerPlayer;
class EEUnitClass;
struct EECPUMRequest;                       // ee/EECPCivilizationManager.h

class EECPUMGatherUnitsScreenBase {
public:
    EECPUMGatherUnitsScreenBase(bool inByClass, bool inRequests, bool inByUnit)
        : mUnits(0), mRequested(0), mByClass(inByClass), mRequests(inRequests), mByUnit(inByUnit)
    {
    }
    virtual ~EECPUMGatherUnitsScreenBase() {}   // +0x00 (??_G 0x7895f: the derived gatherers' implicit dtors fold into it)
    // +0x04 0x2b1c1 (ret 8): a pending request of inClass: mRequested += its count
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);
    // +0x08 0x2b1da (ret 4): one unit: mUnits++
    virtual bool CountUnit(EEComplexUnit* inUnit);
    // +0x0c 0x2b1ec (ret 8): all units of inClass: mUnits += the deque's size
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);

    // inline: units plus requests (EECPDefenseCommander::Fn78866 0x478866 adds them as `add edx, ecx` with mRequested
    // loaded first) (provisional name)
    long GetTotal() const { return this->mUnits + this->mRequested; }
    long GetUnits() const { return this->mUnits; }
    long mUnits;                            // +0x04
    long GetRequested() const { return this->mRequested; }
    long mRequested;                        // +0x08
    bool mByClass;                          // +0x0c (0x93ed4: walk the unit manager's class map)
    bool mRequests;                         // +0x0d (... then the requests and upgrades)
    bool mByUnit;                           // +0x0e (slot +0x08 per unit instead of +0x0c per class)
};

// RTTI EECPUGatherSpecialUnitsCount > EECPUMGatherUnitsScreenBase (vtable 0x43c144: slots 0x2b450/0x2b48b/0x2b4c2): the
// units and requests of one unit type (or, when mUnitType is 0, of one category +0x1d0). EECPDefenseCommander builds
// it on the stack (0x7b923, 0x7bc5b).
class EECPUGatherSpecialUnitsCount : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherSpecialUnitsCount(EEComputerPlayer* inPlayer, long inUnitType, long inCategory, bool inByClass = true)
        : EECPUMGatherUnitsScreenBase(inByClass, true, false)
    {
        this->mPlayer = inPlayer;
        this->mUnitType = inUnitType;
        this->mCategory = inCategory;
    }
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest);   // +0x04 0x2b450
    virtual bool CountUnit(EEComplexUnit* inUnit);   // +0x08 0x2b48b
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits);   // +0x0c 0x2b4c2

    EEComputerPlayer* mPlayer;              // +0x10
    long mUnitType;                         // +0x14 (EEUnitClass +0x1bc)
    long mCategory;                         // +0x18 (EEUnitClass +0x1d0)
};

#endif
