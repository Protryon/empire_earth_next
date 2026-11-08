// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <deque>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
using namespace WONAPI;
// FUNCTION: WON 0x006fb7b0
// Creates the tooltip of a list box, showing no item
ListAreaToolTip::ListAreaToolTip(ListArea* area) {
    this->mListArea=area;
    this->mItem=NULL;
}
// FUNCTION: WON 0x006fb8d0
// Draws the tipped list item in full on a tooltip background with a tooltip-coloured border, using the
// list box's colour scheme unless the tooltip has its own
void ListAreaToolTip::Paint(Graphics& g) {
    Component::Paint(g);
    if(this->mItem.get()==NULL) return;
    ColorScheme* scheme=g.mColorScheme;
    if(this->GetOwnColorScheme()==NULL) g.mColorScheme=this->mListArea->GetColorScheme();
    int x=this->mListArea->mLeftPad,y=-this->mItem->mTop+2;
    g.SetSysColor(StandardColor_ToolTipBack);
    g.FillRect(0,0,this->Width(),this->Height());
    g.PushClipRect(0,2,this->Width(),this->Height()-4);
    g.Translate(x,y);
    this->mListArea->ScrollPaint(g);
    g.Translate(-x,-y);
    g.PopClipRect();
    g.SetSysColor(StandardColor_ToolTipText);
    g.DrawRect(0,0,this->Width(),this->Height());
    g.mColorScheme=scheme;
}
// FUNCTION: WON 0x006fb9b0
// Opens the tooltip over a list item, at least as wide as the list box; closes the old tip first unless
// the same item is already shown, and stops there for no item
void ListAreaToolTip::DoTip(ListItem* item) {
    if(this->mItem.get()==item && this->mItem.get()!=NULL && this->mParent!=NULL) return;
    this->EndTip();
    this->mItem=item;
    if(this->mItem.get()==NULL) return;
    int x=0,y=this->mListArea->mTopPad-this->mListArea->mVertOffset;
    this->mListArea->LocalToRoot(x,y);
    y+=this->mItem->mTop-2;
    PopupParams params(this,0x6247,this->mListArea,x,y);
    int width=this->mItem->mWidth+this->mListArea->mLeftPad+this->mListArea->mRightPad;
    if(width<this->mListArea->Width()) width=this->mListArea->Width();
    this->SetSize(width,this->mItem->mHeight+4);
    this->mListArea->GetRoot()->OpenPopup(params);
}
// FUNCTION: WON 0x006fbab0
// Closes the tooltip popup if it is open
void ListAreaToolTip::EndTip() {
    if(this->mParent!=NULL) this->mListArea->GetRoot()->ClosePopup(this);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fb840
// (public: virtual void * __thiscall WONAPI::ListAreaToolTip::`scalar deleting destructor'(unsigned int)    [??_GListAreaToolTip@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006fb860
// (ListAreaToolTip implicit destructor)
