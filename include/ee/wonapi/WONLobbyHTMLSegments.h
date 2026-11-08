#ifndef EE_WONAPI_WONLOBBYHTMLSEGMENTS_H
#define EE_WONAPI_WONLOBBYHTMLSEGMENTS_H
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
enum LineSegmentFlags {
    LineSegmentFlag_FloatLeft=1,LineSegmentFlag_FloatRight=2,
    LineSegmentFlag_PreLineBreak=4,LineSegmentFlag_PostLineBreak=8,
    LineSegmentFlag_BreakClearLeft=16,LineSegmentFlag_BreakClearRight=32
};
// 0x18-byte segments. The original's allocations set RefCount +4 to 0 and these members only;
// the inherited LineSegment +0x08/+0x0c/+0x10 stay uninitialized. The implicit destructor
// (0x00711480, which calls 0x00738c70) only destroys the base.
class LineBreakSegment : public LineSegment {
public:
    int mHeight; //14
    LineBreakSegment(int height):mHeight(height) {}
    virtual int GetHeight() { return this->mHeight; } // body at 0x007385b0
    virtual void GetSelText(GUIString&,ElementSelectionInfo&); // at 0x00709a40
};
class AlignLineSegment : public LineSegment {
public:
    HorzAlign mAlign; //14
    AlignLineSegment(HorzAlign align):mAlign(align) {}
    virtual bool PerformInstructions(MultiLineBuilder*); // at 0x007099c0
};
// 0x1c bytes; same base-only implicit destructor.
class MarginLineSegment : public LineSegment {
public:
    int mLeftMargin,mNextLeftMargin; // offsets 0x14, 0x18
    MarginLineSegment(int left,int next):mLeftMargin(left),mNextLeftMargin(next) {}
    virtual bool PerformInstructions(MultiLineBuilder*); // at 0x007099d0
};
}
#endif
