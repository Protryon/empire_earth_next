// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
using namespace WONAPI;

// Text area instruction segments (line break, line alignment)
// Defined before LineBreak in the original object: VC6 then knows the ctor cannot throw and omits the
// delete-on-throw guard around `new TextSegmentBase` (0x712690, 0x712730).
// FUNCTION: WON 0x00710a30
// Creates an empty text segment with no line, zero size and position, and no line break
TextSegmentBase::TextSegmentBase()
{
    this->mLine=NULL;
    this->mAbsStartChar=this->mAbsEndChar=0;
    this->mWidth=this->mHeight=this->mAscent=0;
    this->mx=this->my=0;
    this->mLineBreak=false;
}
// FUNCTION: WON 0x00712690
// Appends an empty line-break segment of the given height to the text area
void TextArea::LineBreak(int height) {
    SmartPtr<TextSegmentBase> segment = new TextSegmentBase;
    segment->mHeight = segment->mAscent = height;
    segment->mLineBreak = true;
    this->AddSegment(segment.get());
}

// FUNCTION: WON 0x00712730
// Appends an alignment instruction for the following lines unless the alignment is already current
void TextArea::SetLineAlignment(HorzAlign alignment) {
    if(alignment == this->mLineAlign)
        return;
    SmartPtr<TextAlignInstruction> instruction = new TextAlignInstruction;
    instruction->mAlign = alignment;
    this->AddSegment(instruction.get());
}

namespace WONAPI {
// FUNCTION: WON 0x00711680
// Sets the text area's current line alignment when the layout reaches this instruction
void TextAlignInstruction::PerformInstructions(TextArea* area)
{
    area->mLineAlign=this->mAlign;
}
}
