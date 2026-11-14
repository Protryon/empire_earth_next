// New (port), TEST ONLY: the calamity lane's records of the simulation capture (tools/sim_capture.py, the calamity
// profile; EE-SIM --calamities). Reads the world; the copies it loads are its own and are deleted.
#ifndef EE_EECALAMITYCAPTURE_H
#define EE_EECALAMITYCAPTURE_H

// CALI: every calamity class as Initialize left it (the base fields, the effects and sounds by table index, the
// functor and the class's own numbers), once, at the calamity game's first cast
void EECalamityCaptureClasses();
// CALK: every active calamity's out-of-sync checksum, its Save's bytes and the state a fresh calamity loads from
// them; the contagion manager's checksum, its Save's bytes and what a fresh manager loads; every five game minutes
// of the calamity game, before its casts
void EECalamityCaptureSaves();
class EECalamity;
// CALK of one calamity (inIndex: its place among the active ones), and of the contagion manager
void EECalamityCaptureSave(EECalamity* inCalamity, unsigned long inIndex);
void EECalamityCaptureContagionSave();
// both CALKs whenever the calamity's time alive crossed a multiple of five seconds in its update (inBefore: the time
// alive before it)
void EECalamityCaptureAged(EECalamity* inCalamity, unsigned long inIndex, float inBefore);

#endif
