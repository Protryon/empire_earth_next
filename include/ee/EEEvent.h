// ee/EEEvent.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EVENT_H
#define EE_EVENT_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include <memory>
#include <vector>
#include "rust/sim/scenario.h"

class EEPlayer;
class FSFileSpec;
class EEComplexUnit;

// One effect of an event (from the tech tree's readers; provisional names): a 0x58-byte record of dbeffects (the
// original's fixed table at 0x531ff8 held 2000; now as many as the file has). An EEEvent holds indices into it.
typedef EeScenarioEvEffect EEEventEffect;   // the record is Rust's (ee_scenario event.rs), its fields' notes there
extern std::vector<EEEventEffect> gEventEffects;   // every dbeffects record (the original: 2000 at 0x531ff8)
extern bool gEventsEnabled;                 // 0x4c5a84 (= true; EETechTree 0x1ccf5d clears it while it changes the
                                            // epoch; 0x1ca7cf reads it; provisional name)

// The tech tree's two hook calls of the original (a tech researched: event 0x7d1; an epoch made available: 0x7d2)
// named events past its 1500-event table, so they never ran anything; now that the table has every dbevents row,
// they name no event at all (Trigger ignores a negative event), so a table of more than 2001 events does not run
// a row on every research.
const long kEventHookTechResearched = -0x7d1;
const long kEventHookEpochAvailable = -0x7d2;

extern "C" int64_t ee_scenariocpp_event(uint32_t inOp, const int64_t* a);   // EEEvent.cpp

class EEEvent {
public:
    EEEvent();
    virtual ~EEEvent();
    // 0x1ca76c (cdecl): runs the effects of event inEvent (the tech tree passes the node's event id, the tree's
    // player index, the node id, the tree's player and the research's player) (provisional names)
    static long Trigger(long inEvent, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit);
    // inline accessors: EETechTree::BuildUnitClassInfo expands them once, then calls the
    // out-of-line GetEffect copy 0x1cdf0c (EETechTree.cpp's last function)
    unsigned long GetNumEffects() const { return this->mNumEffects; }
    long GetEffect(unsigned long inIndex) const { return inIndex < this->mNumEffects ? this->mEffects[inIndex] : 100000; }
    static EEEventEffect* GetEventEffect(unsigned long inIndex)
    {
        return inIndex < gEventEffects.size() ? &gEventEffects[inIndex] : NULL;
    }
    // the events by id: every dbevents row (the original: 1500 at 0x55cf80)
    static EEEvent* Get(long inID)
    {
        return (inID >= 0 && (unsigned long)inID < EEEvent::sEvents.size()) ? EEEvent::sEvents[inID].get() : NULL;
    }
    static std::vector<std::unique_ptr<EEEvent>> sEvents;
    // 0x1ca63b (cdecl, 0x141937): every effect record of inEffects, then every event of inEvents (its 100-byte name,
    // +0x0c, count and indices) to the end of the file (both opened first; provisional names)
    static long LoadEvents(FSFileSpec& inEvents, FSFileSpec& inEffects);
private:
    // the statements of the effects' Rust (ee_sim scenario event.rs; RunEffect 0x1ca7cf and its handlers) read them
    friend int64_t ee_scenariocpp_event(uint32_t inOp, const int64_t* a);
    int32_t* mEffects;                      // +0x04 the effect indices, 4 bytes each in the file (the dtor 0x1ca57c delete[]s it)
    unsigned long mNumEffects;              // +0x08 their count (LoadEvents: unsigned long)
    unsigned long mID;                      // +0x0c (LoadEvents: unsigned long)
};

#endif
