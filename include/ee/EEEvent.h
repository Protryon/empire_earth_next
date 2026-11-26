// ee/EEEvent.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EVENT_H
#define EE_EVENT_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include "rust/sim/scenario.h"

class EEPlayer;
class FSFileSpec;
class EEComplexUnit;

// One effect of an event (from the tech tree's readers; provisional names): a 0x58-byte record of dbeffects (the
// original's fixed table at 0x531ff8 held 2000; now as many as the file has). An EEEvent holds indices into it.
typedef EeScenarioEvEffect EEEventEffect;   // the record is Rust's (ee_scenario event.rs), its fields' notes there
extern bool gEventsEnabled;                 // 0x4c5a84 (= true; EETechTree 0x1ccf5d clears it while it changes the
                                            // epoch; 0x1ca7cf reads it; provisional name)

// The tech tree's two hook calls of the original (a tech researched: event 0x7d1; an epoch made available: 0x7d2)
// named events past its 1500-event table, so they never ran anything; now that the table has every dbevents row,
// they name no event at all (Trigger ignores a negative event), so a table of more than 2001 events does not run
// a row on every research.
const long kEventHookTechResearched = -0x7d1;
const long kEventHookEpochAvailable = -0x7d2;

extern "C" int64_t ee_scenariocpp_event(uint32_t inOp, const int64_t* a);   // EEEvent.cpp

// The record and the tables (every dbeffects record, the original's 2000 at 0x531ff8; every dbevents row, the
// original's 1500 at 0x55cf80) are Rust's (ee_sim scenario event.rs EeScenarioEvent, EvTable)
class EEEvent : public EeScenarioEvent {
public:
    // 0x1ca76c (cdecl): runs the effects of event inEvent (the tech tree passes the node's event id, the tree's
    // player index, the node id, the tree's player and the research's player) (provisional names)
    // Runs every effect of an event for a player (a given player's own index wins); returns the last effect's result.
    // RunEffect 0x1ca7cf and its handlers (the stats, button, model, sounds, research, unit conversion, with the EVRN
    // and EVAT records) are ee_sim scenario event.rs, over the statements below
    static long Trigger(long inEvent, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit) { return ee_scenario_ev_trigger(inEvent, inPlayerIndex, inID, inPlayer, inUnit); }
    // inline accessors: EETechTree::BuildUnitClassInfo expands them once, then calls the
    // out-of-line GetEffect copy 0x1cdf0c (EETechTree.cpp's last function)
    unsigned long GetNumEffects() const { return this->mNumEffects; }
    long GetEffect(unsigned long inIndex) const { return inIndex < this->mNumEffects ? this->mEffects[inIndex] : 100000; }
    static EEEventEffect* GetEventEffect(unsigned long inIndex) { return ee_scenario_ev_effect_at(inIndex); }
    // the events by id: every dbevents row
    static EEEvent* Get(long inID) { return static_cast<EEEvent*>(ee_scenario_ev_event(inID)); }
    // 0x1ca63b (cdecl, 0x141937): every effect record of inEffects, then every event of inEvents (its 100-byte name,
    // +0x0c, count and indices) to the end of the file (both opened first; provisional names)
    // Loads every event effect and every event with its effect list (as many as the files hold: the original read
    // exactly 2000 effects and 1500 events); returns the last read's result (ee_sim scenario event.rs)
    static long LoadEvents(FSFileSpec& inEvents, FSFileSpec& inEffects) { return ee_scenario_ev_load(&inEvents, &inEffects); }
};

#endif
