// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
using namespace WONAPI;
// FUNCTION: WON 0x00705580
// Sets the scroll area this scroller controls
void Scroller::SetScrollArea(ScrollArea* area) { this->mScrollArea=area; }
// FUNCTION: WON 0x007055e0
// Scrolls the area to the given offsets (counted as a user scroll); no-op if unchanged
void Scroller::SetOffsets(int horizontal,int vertical) {
    if(this->mScrollArea->mHorzOffset==horizontal && this->mScrollArea->mVertOffset==vertical) return;
    this->mScrollArea->mHorzOffset=horizontal;
    this->mScrollArea->mVertOffset=vertical;
    this->mScrollArea->AdjustPosition(true);
}
// FUNCTION: WON 0x00705630
// Scrolls the area to a vertical offset, keeping the horizontal one
void Scroller::SetVertOffset(int vertical) { this->SetOffsets(this->mScrollArea->mHorzOffset,vertical); }
// FUNCTION: WON 0x00705650
// Scrolls the area to a horizontal offset, keeping the vertical one
void Scroller::SetHorzOffset(int horizontal) { this->SetOffsets(horizontal,this->mScrollArea->mVertOffset); }
// FUNCTION: WON 0x00705670
// On a left press, gives focus to the scroll area, or to the scroller when one of its children has focus
void Scroller::MouseDown(int x,int y,int button) {
    Container::MouseDown(x,y,button);
    if(button==0) {
        if(this->mFocusChild==NULL) this->mScrollArea->RequestFocus();
        else this->RequestFocus();
    }
}
// FUNCTION: WON 0x007056c0
// Passes the wheel to the children, then to the scroll area; true if either handled it
bool Scroller::MouseWheel(int amount) {
    if(Container::MouseWheel(amount)) return true;
    if(this->mScrollArea.get()!=NULL) return this->mScrollArea->MouseWheel(amount);
    return false;
}
