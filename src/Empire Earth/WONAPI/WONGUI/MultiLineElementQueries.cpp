// Multi-line element size and selection-end queries (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x0070c8f0
// Returns the laid-out width
int MultiLineElement::GetWidth() { return this->mWidth; }
// FUNCTION: WON 0x0070c900
// Returns the laid-out height
int MultiLineElement::GetHeight() { return this->mHeight; }
// FUNCTION: WON 0x0070cc30
// Returns the last segment's end selection index, or the element's own start index when it has no segments
int MultiLineElement::GetEndSelIndex()
{
    if(!this->mSegmentList.empty()) return this->mSegmentList.back().mSegment->GetEndSelIndex();
    else return this->mStartSelIndex;
}
}

namespace WONAPI {
// Width calculation hook, empty here: AddSegment keeps the widths up to date (the linker folds
// this body into the identical empty one at 0x008142a0)
void MultiLineElement::CalcMinMaxWidths() {}
// FUNCTION: WON 0x0070d000
// Returns the widest single segment's minimum width
int MultiLineElement::GetMinWidth()
{
    if(this->mMinWidth==0) this->CalcMinMaxWidths();
    return this->mMinWidth;
}
// FUNCTION: WON 0x0070d020
// Returns the widest unwrapped line's width
int MultiLineElement::GetMaxWidth()
{
    if(this->mMaxWidth==0) this->CalcMinMaxWidths();
    return this->mMaxWidth;
}
}

namespace WONAPI {
// Returns true when there is no text or it ends with a line break (not identified in the original exe)

bool MultiLineElement::IsLastLineEmpty()
{
	if(this->mSegmentList.empty())
		return true;
	else
		return (this->mSegmentList.back().mFlags & 0x0008 /* SDK LineSegmentFlag_PostLineBreak */)?true:false;
}
}
