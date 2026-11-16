// New (port; the merge of main's stages 5 and 6): the readers that step over the deleted C++ classic AI's blocks of a
// computer player in a retail file (save versions 0.232 to 0.250: the campaigns' scenarios). main deleted the
// combat, civilization and exploration managers (and EEComputerPlayer::Load reads its own, shorter block), so a
// scenario with a computer player read off its end and did not load. These read every field the managers' Load read
// at those versions, in order, into nothing; the town centre tile (the build manager's) goes to the placement. The
// combat manager's attack groups and the defence commander's aid packages are not stepped over: a file that holds
// one fails as before (the campaigns' scenarios, saved before their game started, hold none).
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

// EECPILocation::Load and EECPAGTask::Load
long SkipTask(FSFile& inFile)
{
    long theResult = SkipTile(inFile);   // the location: tile, area serial, area type, player
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);
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
            return r == kErrorOK && theValid ? (long)kErrorFailed : r;   // an aid package: not stepped over
        });
    if (theResult == kErrorOK) theResult = Skip<unsigned long>(inFile);   // next aid time
    if (theResult == kErrorOK)
        theResult = SkipList(inFile, [&]() { return Skip<unsigned long>(inFile); });   // sub group ids
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xd0)) theResult = Skip<unsigned long>(inFile);   // naval
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xed)) theResult = Skip<unsigned long>(inFile);   // space
    if (theResult == kErrorOK && At(inMajor, inMinor, 0xf0)) theResult = Skip<unsigned long>(inFile);   // special
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
    if (theResult == kErrorOK) {   // the attack commander: its groups (none stepped over), L28
        unsigned long theGroups = 0;
        theResult = inFile.Read(theGroups);
        if (theResult == kErrorOK && theGroups != 0)
            theResult = kErrorFailed;
        if (theResult == kErrorOK) theResult = Skip<long>(inFile);
    }
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
