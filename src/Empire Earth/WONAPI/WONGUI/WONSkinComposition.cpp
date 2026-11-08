// Skin factories that assemble scrollbars, combo boxes, scroll containers and tab controls (file name inferred)
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
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
#include "ee/wonapi/WONLobbySkinTab.h"
namespace WONAPI {
// FUNCTION: WON 0x007156a0
// Returns a new scrollbar from the skin template with its thumb, gutter and arrows, one arrow wide or high
WONScrollbar* WONSkin::GetScrollbar(bool vertical)
{
    WONScrollbar* bar=new WONScrollbar(vertical);
    bar->CopyAttributes(this->mScrollbar[vertical?1:0].get());
    bar->mThumb=this->GetScrollThumb(vertical);
    bar->mScrollRect=this->GetScrollGutter(vertical);
    if(vertical) {
        bar->mUpArrow=this->GetArrowButton(Direction_Up);
        bar->mDownArrow=this->GetArrowButton(Direction_Down);
        bar->SetSize(bar->mUpArrow->Width(),bar->Height());
    } else {
        bar->mUpArrow=this->GetArrowButton(Direction_Left);
        bar->mDownArrow=this->GetArrowButton(Direction_Right);
        bar->SetSize(bar->Width(),bar->mUpArrow->Height());
    }
    return bar;
}
// FUNCTION: WON 0x007158a0
// Returns a new combo box from the skin template with its arrow button and a framed popup list
WONComboBox* WONSkin::GetComboBox()
{
    WONComboBox* combo=this->mComboBox->Duplicate();
    Button* arrow=this->mComboButton->Duplicate();
    arrow->SetDesiredSize(0,0); // old-version SetDesiredSize call
    arrow->SetButtonFlags(2,false);
    WONScrollContainer* list=this->GetScrollContainer();
    list->SetScrollbarConditions(ScrollbarCondition_Disable,ScrollbarCondition_Conditional);
    list->SetFrame(WONFrame(new WONMSSkin::MSImage(WONMSSkin::MSImageType_ComboPopupFrame),0));
    combo->mButton=arrow; // inline ComboBox::SetButton
    combo->SetListBox(list);
    return combo;
}
// FUNCTION: WON 0x00715ae0
// Returns a new scroll container from the skin template with skinned vertical and horizontal scrollbars
WONScrollContainer* WONSkin::GetScrollContainer()
{
    WONScrollContainer* container=this->mScrollContainer->Duplicate();
    container->SetScrollbar(this->GetScrollbar(true));
    container->SetScrollbar(this->GetScrollbar(false));
    return container;
}
// FUNCTION: WON 0x00715c10
// Returns a new tab control from the skin template, bound to this skin, with skinned left and right arrows
WONTabCtrl* WONSkin::GetTabCtrl()
{
    WONTabCtrl* tab=this->mTabCtrl->Duplicate();
    tab->mSkin=this; // inline SetSkin
    tab->SetTabArrows(this->GetArrowButton(Direction_Left),this->GetArrowButton(Direction_Right));
    return tab;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728770
// (public: virtual void * __thiscall WONAPI::WONMSSkin::MSImage::`scalar deleting destructor'(unsigned int)    [??_GMSImage@WONMSSkin@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007159f0
// (public: virtual void __thiscall WONAPI::ProgramImage::SetSize(int,int))
