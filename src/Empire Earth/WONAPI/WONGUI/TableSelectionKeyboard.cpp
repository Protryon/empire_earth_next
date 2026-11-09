// Table copy to clipboard and keyboard scrolling (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextSegment.h"
namespace WONAPI {
// FUNCTION: WON 0x0070fce0
// Copies the selected text to the clipboard when there is any
void Table::CopySelection()
{
    GUIString text;
    if(this->mContents.get()!=NULL)
    {
        ElementSelectionInfo selection;
        selection.mStartSelSegment=this->mStartSelSegment;
        selection.mStartSelSegmentPos=this->mStartSelSegmentPos;
        selection.mEndSelSegment=this->mEndSelSegment;
        selection.mEndSelSegmentPos=this->mEndSelSegmentPos;
        this->mContents->GetSelText(text,selection);
    }
    if(text.length()!=0) Clipboard::Set(text);
}
// FUNCTION: WON 0x0070fdc0
// Copies the selection on Ctrl+C after the scroll area declines the key; returns whether it was handled
bool Table::KeyChar(int key)
{
    if(ScrollArea::KeyChar(key)) return true;
    if(key==3)
    {
        this->CopySelection();
        return true;
    }
    return false;
}
// FUNCTION: WON 0x0070fe00
// Scrolls with the arrow keys (10 pixels sideways, the line amounts vertically) and Page Up/Down; returns
// whether the key was handled
bool Table::KeyDown(int key)
{
    if(ScrollArea::KeyDown(key)) return true;
    switch(key)
    {
    case VK_RIGHT: this->mHorzOffset+=10; break;
    case VK_LEFT: this->mHorzOffset-=10; break;
    case VK_DOWN: this->mVertOffset+=this->mDownScrollAmount; break;
    case VK_UP: this->mVertOffset-=this->mUpScrollAmount; break;
    case VK_NEXT: this->mVertOffset+=this->mScrollArea.height; break;
    case VK_PRIOR: this->mVertOffset-=this->mScrollArea.height; break;
    default: return false;
    }
    this->AdjustPosition(false);
    return true;
}
}