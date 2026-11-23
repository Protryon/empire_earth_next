// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildWall.h (RECONSTRUCTION; file name inferred). The wall placement
// mode: the first left click fixes the start, the mouse drags a line of wall ghosts,
// each further left click builds the line and starts the next one at its end; a right click cancels (RTTI
// EEUIBuildWall, no base; vtable 0x444f40; sizeof 0x20: by-value members of EEUIFMainGame +0x494 and of the scenario
// editor form +0x1c). EEUIBuildWall.cpp is 0x25643d..0x256845 (the members in alphabetical order; its Cancel is
// ICF-folded into EEUIBuildGate's 0x2561b6). Slot and member names are provisional. Requires lle/GE2DPoint.h.
#ifndef EE_EEUIBUILDWALL_H
#define EE_EEUIBUILDWALL_H
#include "ee/EETechTreeTechID.h"

class EEUIBuildWall {
public:
    // inline (out-of-line copy 0x19cbc5 at the start of the scenario editor form's unit: the point ctors, the
    // technology, the vptr, then the body)
    EEUIBuildWall() : mTechnology(kTechNone)
    {
        this->mStart.SetX(-99.0f);
        this->mStart.SetY(-99.0f);
        this->mEnd.SetX(-99.0f);
        this->mEnd.SetY(-99.0f);
        this->mState = 0;
    }
    virtual ~EEUIBuildWall() {}             // +0x00 (??_G 0x19cbff in the scenario editor form's unit)
    // Leaves the placement mode (ICF-folded into EEUIBuildGate::Cancel 0x2561b6)
    virtual void Cancel();                  // +0x04 (ICF-folded into EEUIBuildGate's 0x2561b6)
    virtual void Initialize(long inPlayer); // +0x08 0x256456
    virtual void Terminate();               // +0x0c 0x2564a2
    virtual void Update(const GE2DPoint& inLocation);   // +0x10 0x2564df (the mouse location)
    // Asks the main game form's build action to build a wall from the start to the end point
    virtual void Build();                   // +0x14 0x25643d (mStart..mEnd)

    unsigned long mState;                   // +0x04 0: no start yet, 1: dragging (unsigned: `cmp 1; ja`)
    GE2DPoint mStart;                       // +0x08 (= -99, -99) tile centres
    GE2DPoint mEnd;                         // +0x10
    long mTechnology;                       // +0x18 (= 100000) the wall's tech tree id
    long mPlayer;                           // +0x1c the player index
};

#endif
