// port (new): the timers of the computer players' experiments (docs/AI_RESEARCH.md §8), per player id. They were
// function statics of EEComputerPlayer.cpp and EECPResourceManager.cpp, outside every saved object, so a game loaded
// from a save started them at 0 where the saved game had them running (seed 2's 2100 s save: the loaded game queued a
// settlement at its first AI update that the saved game queued 20 s later). From save version 0.258 they are saved
// (EEPWorldHelper, ee/EESaveGameFiles.h EESaveHasExperimentTimers). Zero at program start, as the statics were.
// Requires lle/FSFile.h for the save functions.
#ifndef EE_EECPEXPERIMENTTIMERS_H
#define EE_EECPEXPERIMENTTIMERS_H

class FSFile;

struct EECPExperimentTimers {
    enum { kPlayers = 64 };
    long mBoats[kPlayers];                     // "boatrush2": the fishing boats counted at the last check
    unsigned long mBoatHold[kPlayers];         // "boatrush2": no boat is bought before this game ms
    unsigned long mSettleLast[kPlayers];       // economy.forage_patches: the last patch settlement queued (game ms)
    unsigned long mGuardLast[kPlayers];        // the resource sites' guard towers: the last check (game ms)
    unsigned long mMilitiaLast[kPlayers];      // the citizens' militia: the last call (game ms)
    unsigned long mNavalFleeLast[kPlayers];    // the naval flee: the last one (game ms)
    long mScheduleEpoch[kPlayers];             // EECPScheduleStart: the epoch last seen
    unsigned long mScheduleStart[kPlayers];    // EECPScheduleStart: when it was first seen (game ms)
    unsigned long mThreatChecked[kPlayers];    // military.prehistoric_army: the last threat check (game ms)
    unsigned long mThreatUntil[kPlayers];      // military.prehistoric_army: the cap is lifted until this game ms
};

extern EECPExperimentTimers gCPExperimentTimers;

// the arrays in the order above, each as its length and its values; Load reads into a copy and keeps the running
// timers when the file is short
long EECPSaveExperimentTimers(FSFile& outFile);
long EECPLoadExperimentTimers(FSFile& inFile);

#endif
