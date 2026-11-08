// Painting and layout of the Windows-style scrollbar, combo box and scroll container (file name inferred)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"

namespace WONAPI {
// FUNCTION: WON 0x006ffa70
// Fills the scrollbar track when it needs repainting (unless transparent), then paints the buttons and thumb
void MSScrollbar::Paint(Graphics& g)
{
    if(this->mScrollRect->mInvalidState!=0 && !this->mIsTransparent)
    {
        g.SetSysColor(StandardColor_Scrollbar);
        g.FillRect(this->mScrollRect->Left(),this->mScrollRect->Top(),this->mScrollRect->Width(),this->mScrollRect->Height());
    }
    Container::Paint(g);
}

// FUNCTION: WON 0x006ffdf0
// Paints the combo box and, if framed, a sunken 3D frame around it
void MSComboBox::Paint(Graphics& g)
{
    ComboBox::Paint(g);
    if(this->mDrawFrame)
        MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),true);
}

// Returns the combo box's preferred size, 100 x 22 (the linker folds this body into the identical one at 0x006ffb00)
void MSComboBox::GetDesiredSize(int& width,int& height)
{
    width=100;
    height=22;
}

// FUNCTION: WON 0x006ffe30
// Lays out the combo box: the arrow button (if visible) at the right inside the frame, the edit box
// filling the rest of the width and the full height
void MSComboBox::Virtual08()
{
    ComboBox::Virtual08();
    int topPad=0,bottomPad=0,rightPad=0,leftPad=0;
    if(this->mDrawFrame)
        topPad=bottomPad=rightPad=2;
    int inputHeight=this->Height();
    int buttonHeight=inputHeight-bottomPad-topPad;
    int buttonWidth=this->mButton->Width();
    if(!this->mButton->IsVisible()) buttonWidth=0;
    int inputWidth=this->Width()-buttonWidth-leftPad-rightPad;
    if(this->mButton->IsVisible())
        this->mButton->SetPosSize(inputWidth+leftPad,topPad,buttonWidth,buttonHeight);
    this->mEdit->SetPosSize(leftPad,0,inputWidth,inputHeight);
}

// FUNCTION: WON 0x00700290
// Lays out the scroll container: visible scrollbars along the right and bottom edges inside the
// 3D frame, the scroll area in the rest below the header
void MSScrollContainer::Virtual08()
{
    Container::Virtual08();
    if(this->mScrollArea.get()==NULL) return;
    bool horzOn=this->mHorzScrollbar->IsVisible();
    bool vertOn=this->mVertScrollbar->IsVisible();
    int xoff=0,yoff=0;
    if(this->mDrawFrame && this->mDraw3DFrame) xoff=yoff=2;
    int scrollbarWidth=this->mVertScrollbar->Width();
    int scrollbarHeight=this->mHorzScrollbar->Height();
    int scrollWidth=this->Width()-xoff*2;
    if(vertOn) scrollWidth-=scrollbarWidth;
    int scrollHeight=this->Height()-yoff*2;
    if(horzOn) scrollHeight-=scrollbarHeight;
    if(vertOn)
        this->mVertScrollbar->SetPosSize(this->Width()-scrollbarWidth-xoff,yoff,scrollbarWidth,scrollHeight);
    if(horzOn)
        this->mHorzScrollbar->SetPosSize(xoff,this->Height()-scrollbarHeight-yoff,scrollWidth,scrollbarHeight);
    this->mScrollArea->SetPosSize(xoff,this->mHeaderHeight+yoff,scrollWidth,scrollHeight-this->mHeaderHeight);
}

// FUNCTION: WON 0x00700390
// Paints the scroll container: fills the corner between two visible scrollbars and draws a 3D or
// dark one-pixel frame
void MSScrollContainer::Paint(Graphics& g)
{
    Container::Paint(g);
    if(this->mVertScrollbar->IsVisible() && this->mHorzScrollbar->IsVisible() && !this->mTransparent)
    {
        g.SetSysColor(StandardColor_Scrollbar);
        g.FillRect(this->mVertScrollbar->Left(),this->mVertScrollbar->Top()+this->mVertScrollbar->Height(),
                   this->mVertScrollbar->Width(),this->mHorzScrollbar->Height());
    }
    if(!this->mDrawFrame)
        return;
    if(this->mDraw3DFrame)
        LobbyDraw3DFrame(g,0,0,this->Width(),this->Height(),true);
    else
    {
        g.SetSysColor(StandardColor_3DDarkShadow);
        g.DrawRect(0,0,this->Width(),this->Height());
    }
}
}
