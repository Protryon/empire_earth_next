// New (port), TEST ONLY: the capture of the A* searches (docs/RUST_PLAN.md R5b.1; ee/EESimCapture.h,
// tools/sim_capture.py). With EE_SIM_CAPTURE set, every EEAIAStarMachine::Run (the tile searches) appends one record
// (tag ASRN), every EEAIAStarGraphMachine::Run (the graph searches: sub sector paths, continental chains) one record
// (tag ASGR): the search's inputs, a digest of everything it asked (the machine's questions to its goal and the goal's
// queries of its view, in order, with the answers) and its outputs (the machine's members, the goal's flags, every
// node of the storage in list order). With EE_SIM_CAPTURE_ASTAR_EVENTS=1 a record also carries the events themselves
// (big). The capture reads only: games with it on keep their traces.
#ifndef EE_EEAIASTARCAPTURE_H
#define EE_EEAIASTARCAPTURE_H

#include <vector>

// A node as the goals and the capture see it (the same layout as rust/ee_rust.h's EeAStarNodeView)
struct EEAIAStarNodeView {
    long mX;
    long mY;
    unsigned long mCost;
    long mPathCost;
    long mEstimate;
    bool mHasParent;
    long mParentX;
    long mParentY;
};

// The storage's lists (EEAIAStarStorage::ListNodes)
enum EEAIAStarList {
    kAStarListSortedOpen = 0,
    kAStarListFastOpen = 1,
    kAStarListFastClosed = 2,
    kAStarListOpenTable = 3,    // every open bucket, the last first
    kAStarListClosedTable = 4,  // every closed bucket, the last first
};

// The events (the first byte of each)
enum EEAIAStarEventKind {
    // the machine's questions to its goal
    kAStarEventAccept = 'A',        // x, y, flag -> accepted
    kAStarEventCost = 'C',          // from x, y, to x, y -> cost
    kAStarEventEstimate = 'E',      // from x, y, to x, y -> estimate
    kAStarEventGoalNode = 'G',      // has node, x, y -> is the goal
    kAStarEventSearchDone = 'S',    // -> failed, took nearest
    kAStarEventMarkTile = 'M',      // x, y, accept (the goal's Prepare)
    // the goals' queries of their view (EEAIAStarView): x, y (and arguments) -> the answer
    kAStarEventView = 'V',
    // the graph machine's questions to its goal (EEAIAStarGraphMachine)
    kAStarEventGraphStart = 'o',    // -> the start object's id
    kAStarEventGraphEstimate = 'e', // from x, y, to x, y -> estimate
    kAStarEventGraphGoalNode = 'g', // the node's index (-1: none) -> is the goal
    kAStarEventGraphEnter = 'n',    // the neighbour's id, the object's id -> may enter
    kAStarEventGraphCost = 'c',     // the parent's index, the object's id -> step cost
    kAStarEventGraphSearchDone = 's',   // -> failed, took nearest
};

// the records' tags
enum {
    kAStarCaptureTileRun = 0x4e525341,  // "ASRN"
    kAStarCaptureGraphRun = 0x52475341, // "ASGR"
};

class EEAIAStarCaptureRun {
public:
    static bool Active();

    EEAIAStarCaptureRun();                  // becomes Current() when the capture is on
    ~EEAIAStarCaptureRun();
    bool IsOn() const { return this->mOn; }

    void Event(unsigned char inKind, long inA, long inB, long inC, long inD, long inE, long inAnswer);
    // the record's fields (little-endian)
    void U8(unsigned char inValue) { this->mData.push_back(inValue); }
    void I32(long inValue);
    void Node(const EEAIAStarNodeView& inNode);
    void Nodes(const std::vector<EEAIAStarNodeView>& inNodes);
    void Commit(unsigned long inTag = kAStarCaptureTileRun);   // the events' count and digest, then the record

    std::vector<unsigned char> mData;       // the inputs, then the outputs
    std::vector<unsigned char> mEvents;     // the raw events (EE_SIM_CAPTURE_ASTAR_EVENTS)
    unsigned long long mEventHash;
    unsigned long mEventCount;
    unsigned long long mStorageHash;

private:
    bool mOn;
    EEAIAStarCaptureRun* mOuter;
};

// the search being captured on this thread, NULL when none (or the capture is off)
extern thread_local EEAIAStarCaptureRun* gEEAIAStarCaptureRun;

// The view's and the machine's notes: no-ops unless a search is being captured on this thread
inline long EEAIAStarNote(unsigned char inKind, long inA, long inB, long inC, long inD, long inE, long inAnswer)
{
    EEAIAStarCaptureRun* theRun = gEEAIAStarCaptureRun;
    if (theRun)
        theRun->Event(inKind, inA, inB, inC, inD, inE, inAnswer);
    return inAnswer;
}

#endif
