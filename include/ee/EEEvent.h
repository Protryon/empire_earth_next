// ee/EEEvent.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EVENT_H
#define EE_EVENT_H

class EEPlayer;
class EEComplexUnit;

// One effect of an event (from the tech tree's readers; provisional names): a 0x58-byte record of
// the table at 0x531ff8 (2000 of them: 0x531ff8 + 2000 * 0x58 is EEEvent's guard byte 0x55cf78). An EEEvent holds
// indices into it. No arrays: an array member costs every includer a VC6 `$E` number.
struct EEEventEffect {
    long mL00;
    long mID;
    float mValue;                           // +0x08 the absolute value (0x1ca591/0x1ca5df: used when +0x0c/+0x10 are 0)
    float mOffset;                          // +0x0c the offset added to the base
    float mFactor;                          // +0x10 the factor of the value
    long mUnitClass;                        // +0x14 a gUnitClassTable index (< 0x367)
    long mNewUnitClass;                     // +0x18 a unit class (kind 0x13: its media)
    long mUnitSet;
    long mKind;                             // +0x20
    long mMediaIndex;                       // +0x24 an EEMedia index (kind 6)
    long mTechnology;                       // +0x28 a tech tree id (kind 9)
    long mAttribute;                        // +0x2c (kind 2: 0x28 = an area effect)
    long mSoundID;                          // +0x30 an EESoundObject index (kinds 0xf..0x12, 0x14, 0x15)
    long mButton;                           // +0x34 an EEButtonObject index (kind 1)
    long mAreaEffect;                       // +0x38 an EEAEManager index (kind 2)
    long mTerrainType;
    long mL40;
    unsigned char mRunOnLoad;               // +0x44 (RunEffect 0x1ca7cf: while loading (gLoadingSavedGame) only effects with 1 run)
    unsigned char mB45;
    unsigned short mU46;
    long mL48;
    long mL4C;
    long mL50;
    long mL54;                              // (sizeof 0x58)
};
extern EEEventEffect gEventEffects[2000];   // 0x531ff8 (provisional name)
extern bool gEventsEnabled;                 // 0x4c5a84 (= true; EETechTree 0x1ccf5d clears it while it changes the
                                            // epoch; 0x1ca7cf reads it; provisional name)

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
        return inIndex < 2000 ? &gEventEffects[inIndex] : NULL;
    }
    // the events by id (0x55cf80, 1500 of them: the `$E` chain 0x1ca4ee's loop; guarded dtor = a class static).
    // the tech tree units define it (unguarded)
    // events 0..1499 (1500 used to be accepted, one past the table)
    static EEEvent* Get(long inID) { return (inID >= 0 && inID < 1500) ? &EEEvent::sEvents[inID] : NULL; }
    static EEEvent sEvents[1500];
    // 0x1ca63b (cdecl, 0x141937): the effect table from inEffects, then every event's 100-byte name, +0x0c, count and
    // indices from inEvents (both opened first; provisional names)
    static long LoadEvents(FSFile& inEvents, FSFile& inEffects);
private:
    // 0x1ca7cf (cdecl; Trigger's per-effect call: the effect index, then Trigger's player index, id and players;
    // provisional name)
    static long RunEffect(unsigned long inEffect, long inPlayerIndex, long inID, EEPlayer* inPlayer, EEComplexUnit* inUnit);
    long* mEffects;                         // +0x04 the effect indices (the dtor 0x1ca57c delete[]s it)
    unsigned long mNumEffects;              // +0x08 their count (LoadEvents: unsigned long)
    unsigned long mID;                      // +0x0c (LoadEvents: unsigned long)
};

#endif
