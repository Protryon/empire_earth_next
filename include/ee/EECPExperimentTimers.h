// port (new): the timers of the computer players' experiments (docs/AI_RESEARCH.md §8), per player id. They were
// function statics of EEComputerPlayer.cpp and EECPResourceManager.cpp, outside every saved object, so a game loaded
// from a save started them at 0 where the saved game had them running (seed 2's 2100 s save: the loaded game queued a
// settlement at its first AI update that the saved game queued 20 s later). From save version 0.258 they are saved
// (EEPWorldHelper, ee/EESaveGameFiles.h EESaveHasExperimentTimers). Zero at program start, as the statics were.
// Requires lle/FSFile.h for the save functions.
#ifndef EE_EECPEXPERIMENTTIMERS_H
#define EE_EECPEXPERIMENTTIMERS_H

class FSFile;

// port (lane B of the AI Thread): the timers themselves are Rust's (ee_sim::cp::timers, in this order: the boats
// counted and their hold time ("boatrush2"), the last patch settlement, guard check, militia call and naval flee,
// the schedule's epoch and start, the prehistoric army's threat check and lift, the claim ranges' steps and last
// step), 64 players each

// the arrays, each as its length (64) and its values; Load reads into a copy and keeps the running
// timers when the file is short
long EECPSaveExperimentTimers(FSFile& outFile);
long EECPLoadExperimentTimers(FSFile& inFile);

// port (save 0.265, ee/EESaveGameFiles.h EESaveHasResourceState): the state FindResourceTarget reads outside the saved
// objects: the claim ranges above and each computer player's danger zones (EECPResourceManager::mDangerZones). A load
// started them empty: a loaded player's claim range was the first one and its first pass took other mines (seed 4's
// 2100 s save). Load puts them in place at the loaded game's first tick
long EECPSaveResourceState(FSFile& outFile);
long EECPLoadResourceState(FSFile& inFile);

#endif
