// New (port; the merge of main's stages 5 and 6): the readers that step over the deleted C++ classic AI's blocks of a
// computer player in a retail file (save versions 0.229 to 0.250: the campaigns' scenarios; the Learning Campaign's
// are 0.231, 0.248 and 0.249 files). main deleted the combat, civilization and exploration managers (and
// EEComputerPlayer::Load reads its own, shorter block), so a scenario with a computer player read off its end and did
// not load. These read every field the managers' Load read at those versions, in order, into nothing, the attack
// commander's groups (their sub groups by kind, the sub groups' units, the orders; "the return" holds one) and the
// defence commander's aid packages included; the town centre tile (the build manager's) goes to the placement.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "ee/EECPLegacyLoad.h"

namespace {

const unsigned long kLegacyEpochs = 15;

// one field of the type the deleted manager declared (FSFile's read of that type)
template <class T> long Skip(FSFile& inFile)
{
    T theValue = T();
    return inFile.Read(theValue);
}

long SkipTile(FSFile& inFile)   // U2DPoint<long> / EETileLocation: Load reads x, y
{
    long theResult = Skip<long>(inFile);
    return theResult == kErrorOK ? Skip<long>(inFile) : theResult;
}

long SkipTile(FSFile& inFile, U2DPoint<long>& outTile)
{
    long theResult = inFile.Read(outTile.mX);
    return theResult == kErrorOK ? inFile.Read(outTile.mY) : theResult;
}

// a count, then that many of a record
template <class F> long SkipList(FSFile& inFile, F inRecord)
{
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++)
        theResult = inRecord();
    return theResult;
}

bool At(unsigned long inMajor, unsigned long inMinor, unsigned long inVersion)
{
    return inMajor > 0 || inMinor >= inVersion;
}

// EECPILocation::Load: tile, area serial, area type, player
long SkipLocation(FSFile& inFile)
{
    long theResult = SkipTile(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    return theResult;
}

// EECPILocation::Load and EECPAGTask::Load
long SkipTask(FSFile& inFile)
{
    long theResult = SkipLocation(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);   // the task: unit id, player id
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<float>(inFile);           // aggression, type, mode, active, B31
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    return theResult;
}

// EECPPersonnelRequest::Load (from 0.222: with the deadline)
long SkipRequest(FSFile& inFile, unsigned long inMajor, unsigned long inMinor)
{
    long theResult = SkipList(inFile, [&]() {
        long r = Skip<unsigned long>(inFile);   // unit id, class, unit id, B0C
        if (r == kErrorOK) r = Skip<unsigned long>(inFile);
        if (r == kErrorOK) r = Skip<unsigned long>(inFile);
        if (r == kErrorOK) r = Skip<bool>(inFile);
        return r;
    });
    if (theResult == kErrorOK) theResult = Skip<float>(inFile);             // fill, id, continent, town, tile, start
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned short>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = SkipTile(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xde))
        theResult = Skip<unsigned long>(inFile);                            // the deadline
    return theResult;
}

// EECPPersonnel::Load
long SkipPersonnel(FSFile& inFile, unsigned long inMajor, unsigned long inMinor)
{
    long theResult = SkipList(inFile, [&]() {
        long r = Skip<unsigned long>(inFile);
        return r == kErrorOK ? Skip<unsigned long>(inFile) : r;
    });
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {
            long r = Skip<long>(inFile);   // the request's id, then the request
            return r == kErrorOK ? SkipRequest(inFile, inMajor, inMinor) : r;
        });
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);   // the next id
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });   // the deferred units
    return theResult;
}

// EECPDefenseCommander::Load (from 0.208)
long SkipDefense(FSFile& inFile, unsigned long inMajor, unsigned long inMinor)
{
    long theResult = Skip<unsigned long>(inFile);   // next update time, enabled
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {
            bool theValid = false;
            long r = inFile.Read(theValid);
            if (r != kErrorOK || !theValid)
                return r;
            // an aid package (EECPMilitaryAidPackage::Load from 0.212; its result was not checked): the sub group
            // ids, the town's tile, target id, threat, town id, state, next update time, attacker id, next send time
            r = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });
            if (r == kErrorOK) r = SkipTile(inFile);
            if (r == kErrorOK) r = Skip<unsigned long>(inFile);
            if (r == kErrorOK) r = Skip<float>(inFile);
            if (r == kErrorOK) r = Skip<long>(inFile);
            if (r == kErrorOK) r = Skip<long>(inFile);
            for (int t = 0; t < 3 && r == kErrorOK; t++) r = Skip<unsigned long>(inFile);
            return r;
        });
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);   // next aid time
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });   // sub group ids
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xd0)) theResult = Skip<unsigned long>(inFile);   // naval
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xed)) theResult = Skip<unsigned long>(inFile);   // space
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xf0)) theResult = Skip<unsigned long>(inFile);   // special
    return theResult;
}

// EECPAttackSubGroupUnit::Load (every unit kind's: no kind overrides it): its location, unit and target ids,
// retarget time, importance, important, needs orders, out of fight, kind, F6C, aggression, courage, F68, escort id,
// state, post attack mode
long SkipSubGroupUnit(FSFile& inFile)
{
    long theResult = SkipLocation(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    for (int f = 0; f < 4 && theResult == kErrorOK; f++) theResult = Skip<float>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    return theResult;
}

// EECPAttackSubGroup::Load: the units (kind, unit; as the original, a unit's read error is not reported: the reads
// after it fail instead), the path (tile, sector kind), the area, next waypoint, rally point, task, the request id and
// the plain fields
long SkipSubGroup(FSFile& inFile)
{
    unsigned long theCount = 0;
    if (inFile.Read(theCount) == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            long theKind = 0;
            if (inFile.Read(theKind) == kErrorOK)
                SkipSubGroupUnit(inFile);
        }
    }
    long theResult = SkipList(inFile, [&]() {
        long r = SkipTile(inFile);
        return r == kErrorOK ? Skip<long>(inFile) : r;
    });
    for (int e = 0; e < 4 && theResult == kErrorOK; e++) theResult = Skip<long>(inFile);   // the area's edges
    if (theResult == kErrorOK) theResult = SkipLocation(inFile);
    if (theResult == kErrorOK) theResult = SkipLocation(inFile);
    if (theResult == kErrorOK) theResult = SkipTask(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);            // the request id, scan range, aggression
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<float>(inFile);
    for (int t = 0; t < 8 && theResult == kErrorOK; t++)                  // id, the times and counters (U1B0, U1B4, U1BC)
        theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);            // splits, state, kind
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    for (int b = 0; b < 4 && theResult == kErrorOK; b++) theResult = Skip<bool>(inFile);   // B1E0-B1E2, out of fight
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);            // mission, split kind
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    return theResult;
}

// EECPASGLand::Load's own part (from 0.227): pickup and unload locations, transport id, transport stage, a flag, the
// embark and landing tiles, the avoided tiles (tile, time)
long SkipLandSubGroup(FSFile& inFile)
{
    long theResult = SkipLocation(inFile);
    if (theResult == kErrorOK) theResult = SkipLocation(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = SkipTile(inFile);
    if (theResult == kErrorOK) theResult = SkipTile(inFile);
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {
            long r = SkipTile(inFile);
            return r == kErrorOK ? Skip<unsigned long>(inFile) : r;
        });
    return theResult;
}

// EECPAttackGroup::Load: the task, the sub groups by kind (EECPASGLand and the helicopter: the land part; the sea,
// air, submarine, spaceship and space fighter: the base only; the three transports: the base, then the transport's
// land id and cargo stage), the orders, id, continent, finished
long SkipAttackGroup(FSFile& inFile)
{
    long theResult = SkipTask(inFile);
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {
            long theKind = 0;
            long r = inFile.Read(theKind);
            if (r != kErrorOK)
                return r;
            switch (theKind) {
            case 0: case 5: case 8:   // land, helicopter
                r = SkipSubGroup(inFile);
                return r == kErrorOK ? SkipLandSubGroup(inFile) : r;
            case 1: case 7: case 2: case 6: case 9: case 11:   // sea, air, submarine, spaceship, space fighter
                return SkipSubGroup(inFile);
            case 3: case 4: case 10:   // the sea, air and space transports
                r = SkipSubGroup(inFile);
                if (r == kErrorOK) r = Skip<unsigned long>(inFile);
                return r == kErrorOK ? Skip<long>(inFile) : r;
            default:   // no sub group of that kind (the original dereferenced a null one)
                return (long)kErrorFailed;
            }
        });
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {   // EECPAGOrder::Load: location, sub group ids, unit id, L28, type, state, active, split
            long r = SkipLocation(inFile);
            for (int f = 0; f < 3 && r == kErrorOK; f++) r = Skip<unsigned long>(inFile);
            if (r == kErrorOK) r = Skip<long>(inFile);
            if (r == kErrorOK) r = Skip<long>(inFile);
            if (r == kErrorOK) r = Skip<unsigned long>(inFile);
            if (r == kErrorOK) r = Skip<bool>(inFile);
            return r == kErrorOK ? Skip<long>(inFile) : r;
        });
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned short>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    return theResult;
}

// the epochs' lists of the build, research and unit managers: one per epoch of the retail data, which those files
// were written with (15; before 0.234, not the 15th)
template <class F> long SkipPerEpoch(FSFile& inFile, unsigned long inMajor, unsigned long inMinor, F inList)
{
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < kLegacyEpochs && theResult == kErrorOK; i++) {
        if (!At(inMajor, inMinor, 0xea) && i == 14)
            continue;
        theResult = inList();
    }
    return theResult;
}

}   // namespace

long EECPSkipLegacyCombatManager(FSFile& inFile, unsigned long inMajor, unsigned long inMinor)
{
    if (!At(inMajor, inMinor, 0xe4))
        return kErrorFailed;
    long theResult = SkipPersonnel(inFile, inMajor, inMinor);
    if (theResult == kErrorOK)   // the attack commander (EECPAttackCommander::Load): its groups, L28
        theResult = SkipList(inFile, [&]() { return SkipAttackGroup(inFile); });
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = SkipDefense(inFile, inMajor, inMinor);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);   // next group and sub group ids
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);            // was attacked, improvements enabled
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = SkipList(inFile, [&]() { return SkipTask(inFile); });   // defend tasks
    if (theResult == kErrorOK) theResult = SkipList(inFile, [&]() { return SkipTask(inFile); });   // attack tasks
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);            // wonder race, its attack times
    if (theResult == kErrorOK) theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {   // the tile counts: tile, count
            long r = SkipTile(inFile);
            return r == kErrorOK ? Skip<long>(inFile) : r;
        });
    return theResult;
}

long EECPSkipLegacyCivilizationManager(FSFile& inFile, unsigned long inMajor, unsigned long inMinor,
                                       U2DPoint<long>& outTownCenterTile)
{
    // the resource manager: the drop site classes, needs, wanted gatherers, B180, flags and timers
    long theResult = kErrorOK;
    for (int r = 0; r < 7 && theResult == kErrorOK; r++)
        theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });
    for (int r = 0; r < 7 && theResult == kErrorOK; r++) theResult = Skip<long>(inFile);
    for (int r = 0; r < 7 && theResult == kErrorOK; r++) theResult = Skip<unsigned long>(inFile);
    for (int r = 0; r < 7 && theResult == kErrorOK; r++) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    for (int t = 0; t < 4 && theResult == kErrorOK; t++) theResult = Skip<unsigned long>(inFile);
    // the build manager: the queues (class, tile), the town centre tile, the next update time (from 0.220)
    if (theResult == kErrorOK)
        theResult = SkipPerEpoch(inFile, inMajor, inMinor, [&]() {
            return SkipList(inFile, [&]() {
                long r = Skip<unsigned long>(inFile);
                return r == kErrorOK ? SkipTile(inFile) : r;
            });
        });
    if (theResult == kErrorOK) theResult = SkipTile(inFile, outTownCenterTile);
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xdc)) theResult = Skip<unsigned long>(inFile);
    // the research manager: candidates, the epochs' ids, next time, epoch, enabled
    if (theResult == kErrorOK) theResult = SkipList(inFile, [&]() { return Skip<long>(inFile); });
    if (theResult == kErrorOK)
        theResult = SkipPerEpoch(inFile, inMajor, inMinor, [&]() {
            return SkipList(inFile, [&]() { return Skip<long>(inFile); });
        });
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    // the unit manager: the epochs' requests (class, count, continent, town), the waiting counts, waiting, enabled
    if (theResult == kErrorOK)
        theResult = SkipPerEpoch(inFile, inMajor, inMinor, [&]() {
            return SkipList(inFile, [&]() {
                long r = Skip<unsigned long>(inFile);
                if (r == kErrorOK) r = Skip<unsigned long>(inFile);
                if (r == kErrorOK) r = Skip<unsigned short>(inFile);
                if (r == kErrorOK) r = Skip<long>(inFile);
                return r;
            });
        });
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {
            long r = Skip<unsigned long>(inFile);
            return r == kErrorOK ? Skip<unsigned long>(inFile) : r;
        });
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    // the civilization manager's own: flags, the epoch advance and building count times, the garrison
    for (int f = 0; f < 4 && theResult == kErrorOK; f++) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    for (int f = 0; f < 3 && theResult == kErrorOK; f++) theResult = Skip<bool>(inFile);
    for (int f = 0; f < 3 && theResult == kErrorOK; f++) theResult = Skip<unsigned long>(inFile);
    // the wall manager: walls (unit, enemy), gates, the gate and wall tiles, F70, F74, L7C, planned, next time
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() {
            long r = Skip<unsigned long>(inFile);
            return r == kErrorOK ? Skip<unsigned long>(inFile) : r;
        });
    if (theResult == kErrorOK) theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });
    for (int t = 0; t < 3 && theResult == kErrorOK; t++) theResult = SkipTile(inFile);
    if (theResult == kErrorOK) theResult = Skip<float>(inFile);
    if (theResult == kErrorOK) theResult = Skip<float>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    // the fortress, its time, morale buildings enabled
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    return theResult;
}

long EECPSkipLegacyExplorationManager(FSFile& inFile, unsigned long inMajor, unsigned long inMinor)
{
    long theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });   // the explorers
    if (theResult == kErrorOK && !At(inMajor, inMinor, 0x21)) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);   // B2C, enabled
    if (theResult == kErrorOK) theResult = Skip<bool>(inFile);
    if (theResult == kErrorOK) theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });   // requests
    return theResult;
}
