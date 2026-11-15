// C:\EEX\Empire Earth\Actions\EEABuild.h (RECONSTRUCTION; file name inferred). The build action (RTTI EEABuild >
// EEAction > UReferenceCounter; vtable 0x438aa0; EEABuild.cpp 0x173b..0x1a8d):
// the dtor 0x173b (out of line: ??_G 0x1a71 calls it), Build 0x1760 / BuildGate 0x18dd / BuildWall 0x199e (each asks
// the current player whether it can build mTechnology, then posts the matching EEM* message), Virtual4 (ICF-folded
// into EEALasso's 0x28c4: process mMessage), Virtual8 the shared empty slot. Names are provisional (by behaviour).
#ifndef EE_EEABUILD_H
#define EE_EEABUILD_H

#include "ee/EEAction.h"

class EEMessage;
class GE2DPoint;

class EEABuild : public EEAction {
public:
    // 0x266572: defined in EEUIFMainGame.cpp (between its members; the main form's ctor calls it out of line, so it
    // is not an inline COMDAT after its first user)
    EEABuild();
    virtual ~EEABuild();                // +0x00 (??_G 0x1a71 calls the out-of-line dtor 0x173b)
    virtual void Virtual4();            // +0x04 (ICF: 0x28c4)
    virtual void Virtual8();            // +0x08 (0x1720c6)
    void Build(const GE2DPoint& inLocation);                            // 0x1760 (called from 0x27012b)
    void BuildGate(const GE2DPoint& inLocation, long inSide);           // 0x18dd
    void BuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd);   // 0x199e

    EEMessage* mMessage;                // +0x0c (Dereference()d by the dtor)
    long mTechnology;                   // +0x10 the tech tree id of what is built
    unsigned char mIsQueued;            // +0x14 (EEMBuildUnit's +0x1c)
};

#endif
