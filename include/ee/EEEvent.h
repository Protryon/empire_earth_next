// ee/EEEvent.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EVENT_H
#define EE_EVENT_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include <memory>
#include <vector>

class EEPlayer;
class FSFileSpec;
class EEComplexUnit;

// One effect of an event (from the tech tree's readers; provisional names): a 0x58-byte record of dbeffects (the
// original's fixed table at 0x531ff8 held 2000; now as many as the file has). An EEEvent holds indices into it.
struct EEEventEffect {
    int32_t mUnused1;
    int32_t mUnusedID;
    float mValue;                           // +0x08 the absolute value (0x1ca591/0x1ca5df: used when +0x0c/+0x10 are 0)
    float mOffset;                          // +0x0c the offset added to the base
    float mFactor;                          // +0x10 the factor of the value
    int32_t mUnitClass;                     // +0x14 a gUnitClassTable index (< EEUnitClassCount())
    int32_t mNewUnitClass;                  // +0x18 a unit class (kind 0x13: its media)
    int32_t mUnitSet;
    int32_t mKind;                          // +0x20
    int32_t mMediaIndex;                    // +0x24 an EEMedia index (kind 6)
    int32_t mTechnology;                    // +0x28 a tech tree id (kind 9)
    int32_t mAttribute;                     // +0x2c (kind 2: 0x28 = an area effect)
    int32_t mSoundID;                       // +0x30 an EESoundObject index (kinds 0xf..0x12, 0x14, 0x15)
    int32_t mButton;                        // +0x34 an EEButtonObject index (kind 1)
    int32_t mAreaEffect;                    // +0x38 an EEAEManager index (kind 2)
    int32_t mTerrainType;
    int32_t mUnused2;
    unsigned char mRunOnLoad;               // +0x44 (RunEffect 0x1ca7cf: while loading (gLoadingSavedGame) only effects with 1 run)
    unsigned char mUnused3;
    unsigned short mUnused4;
    int32_t mUnused5;
    int32_t mUnused6;
    int32_t mUnused7;
    int32_t mUnused8;                       // (sizeof 0x58)
};
static_assert(sizeof(EEEventEffect) == 88, "on-disk record: 88 bytes on every target");
extern std::vector<EEEventEffect> gEventEffects;   // every dbeffects record (the original: 2000 at 0x531ff8)
extern bool gEventsEnabled;                 // 0x4c5a84 (= true; EETechTree 0x1ccf5d clears it while it changes the
                                            // epoch; 0x1ca7cf reads it; provisional name)

// The tech tree's two hook calls of the original (a tech researched: event 0x7d1; an epoch made available: 0x7d2)
// named events past its 1500-event table, so they never ran anything; now that the table has every dbevents row,
// they name no event at all (Trigger ignores a negative event), so a table of more than 2001 events does not run
// a row on every research.
const long kEventHookTechResearched = -0x7d1;
const long kEventHookEpochAvailable = -0x7d2;

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
    static unsigned long GetNumEvents() { return (unsigned long)EEEvent::sEvents.size(); }
    static std::vector<std::unique_ptr<EEEvent>> sEvents;
    // 0x1ca63b (cdecl, 0x141937): every effect record of inEffects, then every event of inEvents (its 100-byte name,
    // +0x0c, count and indices) to the end of the file (both opened first; provisional names)
    static long LoadEvents(FSFileSpec& inEvents, FSFileSpec& inEffects);
private:
    // 0x1ca7cf (cdecl; Trigger's per-effect call: the effect index, then Trigger's player index, id and players;
    // provisional name)
    static long RunEffect(unsigned long inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit);
    int32_t* mEffects;                      // +0x04 the effect indices, 4 bytes each in the file (the dtor 0x1ca57c delete[]s it)
    unsigned long mNumEffects;              // +0x08 their count (LoadEvents: unsigned long)
    unsigned long mID;                      // +0x0c (LoadEvents: unsigned long)
};

#endif
