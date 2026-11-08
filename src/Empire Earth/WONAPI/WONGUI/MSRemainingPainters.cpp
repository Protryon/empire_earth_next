// Paint methods of the Windows-style header button, close button, scroll thumb and label
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"

namespace WONAPI {
// FUNCTION: WON 0x006ff000
// Paints a list header button and, when its column is sorted, a small up or down triangle after the text
// (shifted by one pixel while pressed)
void MSHeaderButton::Paint(Graphics& g)
{
    MSButton::Paint(g);
    ListSortType sortType=MultiListHeader::GetSortTypeForButton(this);
    if(sortType==ListSortType_None)
        return;
    g.SetSysColor(StandardColor_ButtonText);
    int x;
    if(this->mText->GetWidth()==0)
        x=(this->Width()-4)/2;
    else
        x=this->mText->GetWidth()+10+this->mLeftTextPad;
    int y=this->Height()/2-2;
    int offset=(mButtonFlags&0x20)!=0?1:0;
    x+=offset;
    y+=offset;
    if(sortType==ListSortType_Ascending)
    {
        for(int i=0;i<4;i++)
            g.DrawLine(x-i,y+i,x+i,y+i);
    }
    else if(sortType==ListSortType_Descending)
    {
        for(int i=0;i<4;i++)
            g.DrawLine(x-(3-i),y+i,x+(3-i),y+i);
    }
}

// FUNCTION: WON 0x006ff2a0
// Paints a close button: face background, a raised or sunken 3D frame and the X, shifted while pressed
void MSCloseButton::Paint(Graphics& g)
{
    Button::Paint(g);
    bool down=(mButtonFlags&0x20)!=0;
    int offset=down?1:0;
    g.SetSysColor(StandardColor_3DFace);
    g.FillRect(0,0,this->Width(),this->Height());
    LobbyDraw3DFrame(g,0,0,this->Width(),this->Height(),down);
    MSGraphicsUtil::DrawCloseX(g,offset,offset,this->Width(),this->Height());
}

// FUNCTION: WON 0x006ff360
// Paints a scrollbar thumb: face background with a raised frame
void MSScrollThumb::Paint(Graphics& g)
{
    Component::Paint(g);
    g.SetSysColor(StandardColor_3DFace);
    g.FillRect(0,0,this->Width(),this->Height());
    LobbyDrawFrame(g,0,0,this->Width(),this->Height(),false);
}

// FUNCTION: WON 0x006ff7d0
// Paints a label: face background unless transparent, then the text
void MSLabel::Paint(Graphics& g)
{
    MSButtonBase::Paint(g);
    g.SetSysColor(StandardColor_3DFace);
    if(!this->mIsTransparent)
        g.FillRect(0,0,this->Width(),this->Height());
    this->DrawButtonText(g,0,0);
}
}
