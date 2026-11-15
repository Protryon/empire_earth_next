// New (port), TEST ONLY: the area effects' records of the simulation capture (tools/sim_capture.py; every profile).
// Reads the world; writes nothing to it.
//   AETP  every area-effect template as Init left it (once, at the first area-effect tick): index, the numbers, the
//         graphic effects and media by table index
//   AEFX  the area effects after each EEAEManager::Tick: the elapsed time; per unit (players in order, each list in
//         order) that owns or is affected by an effect: its id, hit points and cloakable flag, each own effect's kind,
//         player, location, checksum, strength, radius, scale, immunity, the three modifiers of a probe value, the
//         morale count, and its Covers of its own unit; each affecting effect as its owner and index; then the active
//         list the same way and the running state's save bytes (EEAESaveRunningState)
#ifndef EE_EEAECAPTURE_H
#define EE_EEAECAPTURE_H

void EEAECaptureTick(float inElapsed);

#endif
