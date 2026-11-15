// EEAirTrafficControl.h (RECONSTRUCTION; file name inferred; provisional member names). RTTI
// EEAirTrafficControl (vtable 0x4422e8: ??_G, Load +4, Save +8; ctor 0x14e87c; EEAirTrafficControl.cpp
// 0x14e839..0x151c28). One per player and air traffic type: the airports (carrying capacity and landing / takeoff
// queues) the player's aircraft are routed to.
// port (R5b.4, round 30): the class is Rust's whole (ee_units atc.rs, atc_save.rs): its airports, queues and cargo
// totals are the Rust state behind mRust (the original's vector of airports, each with two deques of queued planes,
// and the totals +0x18 / +0x1c); the methods forward to it, the units' reads and the world calls are
// EEAirTrafficControl.cpp's host.
// Requires lle/LowLevelEngine.h, ee/EEUnit.h.
#ifndef EE_EEAIRTRAFFICCONTROL_H
#define EE_EEAIRTRAFFICCONTROL_H

#include <stdio.h>
#include <vector>

#include "rust/ee_units.h"

class EEComplexUnit;
class EEPlayer;
class FSFile;

// A replaced unit is an EEPUnitSwap (ee/EEPhysics.h): EEPlayer::UnitsSwapped passes one vector to 0x14f88d and 0x1fc6e8
struct EEPUnitSwap;

class EEAirTrafficControl {
public:
    // the save key (0x531cb8, guard byte 0x531cb0: the guarded dtor 0x14e860)
    static const UString sData;             // "EEAirTrafficControl Data"
    EEAirTrafficControl(EEPlayer* inPlayer);   // 0x14e87c (EEPlayer::Reset passes this)
    virtual ~EEAirTrafficControl();         // ??_G 0x14e8b7, dtor 0x14e8d3
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile);   // +0x04 0x14f75c
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile);   // +0x08 0x14fead

    void AddCargoWeight(EEComplexUnit* inUnit);  // 0x14e9a8 (used cargo += the unit's weight)
    void AddAirport(EEComplexUnit* inAirport);
    long QueueRequest(EEComplexUnit* inAirport, EEComplexUnit* inUnit, long inKind);  // 0x14ea34 (0 queued, 3 no such airport, 7 no room)
    long GetFreeCargoWeight(EEComplexUnit* inAirport);  // 0x14eaf7 (the airport's free cargo weight, -1 if unknown)
    bool RemoveFromTakeoffQueues(EEComplexUnit* inUnit);  // 0x14ec8c (drops inUnit from the takeoff queues)
    bool ClearAirportQueue(EEComplexUnit* inAirport, long inKind);  // 0x14ed30 (drops the idle takeoffs of a kind)
    void Dump(FILE* inFile);
    long GetChecksum();                     // 0x14f47d (an out-of-sync checksum)
    unsigned long GetQueueLength(EEComplexUnit* inAirport);  // 0x14f5fa (its takeoff queue's length)
    long GetQueuePosition(EEComplexUnit* inUnit, long inWhich);  // 0x14f61c (inUnit's place in its airport's queue, -1)
    void ReplaceUnits(std::vector<EEPUnitSwap>& inSwaps);  // 0x14f88d (replaces the old units by the new in the queues)
    void RemoveCargoWeight(EEComplexUnit* inUnit);  // 0x14fc39 (used cargo -= the unit's weight)
    void RemoveAirport(EEComplexUnit* inAirport);  // 0x14fc4c (the airport is gone)
    void RemovePlane(EEComplexUnit* inAirport, EEComplexUnit* inUnit);
    long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x14fcf4 (ids back to units after a load)
    EEComplexUnit* FindNearestAirport(EEComplexUnit* inUnit);  // 0x15014c (the nearest airport that fits inUnit)
    void Update();

    // the airports (inline in the original over mSlots: 0x20b671, EEUCPlanetSpaceship::Virtual74 0x618e34)
    unsigned long GetNumSlots() { return ee_units_atc_slot_count(this->mRust); }
    EEComplexUnit* GetSlotAirport(unsigned long inIndex)
        { return static_cast<EEComplexUnit*>(ee_units_atc_slot_airport(this->mRust, inIndex)); }
    // the cargo totals (+0x18 "max cargo weight", +0x1c "used cargo weight")
    long GetMaxCargo() const { return ee_units_atc_cargo(this->mRust, 0); }
    long GetUsedCargo() const { return ee_units_atc_cargo(this->mRust, 1); }

    EEPlayer* mPlayer;                      // +0x14 (ctor argument: the owner)
    EeUnitsAtc* mRust;                      // the airports, their queues and the cargo totals (Rust's)

private:
    int64_t Call(int32_t inCall, EEComplexUnit* inUnit, EEComplexUnit* inOther = NULL, long inValue = 0);
};

#endif
