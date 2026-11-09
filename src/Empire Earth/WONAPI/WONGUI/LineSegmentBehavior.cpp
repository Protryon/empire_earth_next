// Line segment behaviour: selection, wrapping, viewport placement and HTML instruction segments
// (file name inferred)
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
// FUNCTION: WON 0x007098d0
// Returns 1 when the point lies in the right half of the segment, else 0
int LineSegment::GetSelChar(int x,int y)
{
    if(x>=this->GetWidth()/2) return 1;
    else return 0;
}
// FUNCTION: WON 0x00709520
// Adds the segment to the current line, starting a new line first unless it must go on the first line
void LineSegment::Wrap(MultiLineBuilder* builder,bool needSegmentOnFirstLine)
{
    if(!needSegmentOnFirstLine) builder->AddLine();
    builder->AddSegmentToCurLine(this);
}
// FUNCTION: WON 0x00709550
// Records the segment's absolute position (the viewport is ignored)
void LineSegment::SetViewport(ScrollAreaViewport*,int x,int y)
{
    this->mAbsX=x;
    this->mAbsY=y;
}
}

// HTML instruction segments and line-break selection text
#include "ee/wonapi/WONLobbyHTMLSegments.h"
#include "ee/wonapi/WONLobbyHTMLAnchorSegment.h"
namespace WONAPI {
// FUNCTION: WON 0x007099c0
// Sets the builder's line alignment; returns false (the segment takes no space)
bool AlignLineSegment::PerformInstructions(MultiLineBuilder* builder)
{
    builder->SetLineAlignment(this->mAlign);
    return false;
}
// FUNCTION: WON 0x007099d0
// Sets the builder's left margins for this and following lines; returns false
bool MarginLineSegment::PerformInstructions(MultiLineBuilder* builder)
{
    builder->SetIndentSize(this->mLeftMargin,this->mNextLeftMargin);
    return false;
}
// FUNCTION: WON 0x007099f0
// Returns true: an anchor is kept in the line
bool AnchorLineSegment::PerformInstructions(MultiLineBuilder*)
{
    return true;
}
// FUNCTION: WON 0x00709a00
// Records the anchor's position and registers its name at that position with the owning table
void AnchorLineSegment::SetViewport(ScrollAreaViewport* viewport,int x,int y)
{
    LineSegment::SetViewport(viewport,x,y);
    if(viewport!=NULL) {
        Table* table=static_cast<Table*>(viewport->GetParent());
        table->AddAnchor(this->mName,x,y);
    }
}
// FUNCTION: WON 0x00709a40
// Appends a line break (two for a paragraph break with height) when the break is inside the selection
void LineBreakSegment::GetSelText(GUIString& text,ElementSelectionInfo& info)
{
    if(this->mStartSelIndex<info.mStartSelSegment || this->mStartSelIndex>info.mEndSelSegment)
        return;
    if(this->mHeight>0) text.append("\r\n\r\n");
    else text.append("\r\n");
}
}
