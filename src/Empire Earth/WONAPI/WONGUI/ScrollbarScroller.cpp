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
#include "rust/ee_won.h"
// The scrollbar conditions and the bars' numbers are ee_won's guiscroll module (rust/crates/ee_won/src/guiscroll.rs).
using namespace WONAPI;
// FUNCTION: WON 0x00705760
// Uses a scrollbar as the vertical or horizontal one (by its orientation) and listens to its events
void ScrollbarScroller::SetScrollbar(Scrollbar* scrollbar) {
    if(scrollbar->mVertical) this->mVertScrollbar=scrollbar;
    else this->mHorzScrollbar=scrollbar;
    scrollbar->AddListener(new ComponentListenerComp(this));
}
// FUNCTION: WON 0x00705840
// Adds the scroll area and scrollbars as children the first time the scroller gets a parent
void ScrollbarScroller::AddedToParent() {
    if(this->mVertScrollbar->mParent!=this) this->AddControls();
    Container::AddedToParent();
}
// FUNCTION: WON 0x00705860
// Hooks the scroll area to this scroller, adds it and both scrollbars, and syncs them if there is content
void ScrollbarScroller::AddControls() {
    this->mScrollArea->SetScroller(this);
    this->AddChild(this->mScrollArea.get());
    this->AddChild(this->mVertScrollbar.get());
    this->AddChild(this->mHorzScrollbar.get());
    if(this->mScrollArea->mTotalWidth>0 || this->mScrollArea->mTotalHeight>0) this->NotifyScrollChange(false);
}
// FUNCTION: WON 0x007058d0
// Shows or hides a scrollbar per its condition (conditional: when content overflows); true if visibility changed
// (while conditions are being applied, a visible conditional scrollbar is left alone)
bool ScrollbarScroller::EnableScrollbar(Scrollbar* scrollbar,ScrollbarCondition condition) {
    if(scrollbar==NULL || this->mScrollArea.get()==NULL) return false;
    bool visible=scrollbar->IsVisible();
    ScrollArea* a=this->mScrollArea.get();
    EeWonScrollArea anArea = { a->mVertOffset, a->mHorzOffset, a->mTotalWidth, a->mTotalHeight, a->mExtraHeight,
                               a->mScrollArea.x, a->mScrollArea.y, a->mScrollArea.width, a->mScrollArea.height };
    int k=ee_won_gui_scrollbar_condition(condition,this->mSettingScrollbarConditions,visible,scrollbar->mVertical,
                                         anArea);
    if(k<0) return false;
    if(k==0 || k==1) scrollbar->SetVisible(k==1);
    return visible!=scrollbar->IsVisible();
}
// FUNCTION: WON 0x00705990
// Sets when each scrollbar shows and applies it, laying out again if a scrollbar appeared or vanished
void ScrollbarScroller::SetScrollbarConditions(ScrollbarCondition horizontal,ScrollbarCondition vertical) {
    this->mHorzCondition=horizontal;
    this->mVertCondition=vertical;
    bool changed=this->EnableScrollbar(this->mHorzScrollbar.get(),this->mHorzCondition)?true:false;
    changed=this->EnableScrollbar(this->mVertScrollbar.get(),this->mVertCondition) || changed;
    this->mSettingScrollbarConditions=true;
    if(changed) {
        this->Invalidate();
        this->Virtual08();
    }
    this->mSettingScrollbarConditions=false;
}
// FUNCTION: WON 0x00705a00
// Scrolls the area to a scrollbar's new position on its scroll event; else handles the event normally
void ScrollbarScroller::HandleComponentEvent(ComponentEvent* event) {
    if(event->mType==ComponentEvent_Scroll) {
        Scrollbar* scrollbar=static_cast<Scrollbar*>(event->mComponent);
        int position=scrollbar->mPosition;
        if(scrollbar->mVertical) {
            if(scrollbar==this->mVertScrollbar.get()) { this->SetVertOffset(position);return; }
        } else if(scrollbar==this->mHorzScrollbar.get()) { this->SetHorzOffset(position);return; }
    }
    Component::HandleComponentEvent(event);
}
// FUNCTION: WON 0x00705a50
// Updates both scrollbars' sizes, steps, position and enabled state from the scroll area, then reapplies conditions
void ScrollbarScroller::NotifyScrollChange(bool becauseOfScroller) {
    ScrollArea* a=this->mScrollArea.get();
    EeWonScrollArea anArea = { a->mVertOffset, a->mHorzOffset, a->mTotalWidth, a->mTotalHeight, a->mExtraHeight,
                               a->mScrollArea.x, a->mScrollArea.y, a->mScrollArea.width, a->mScrollArea.height };
    int aTotal=0,aPage=0;
    if(this->mVertScrollbar.get()!=NULL) {
        bool anEnabled=ee_won_gui_scrollbar_sync(anArea,true,&aTotal,&aPage);
        this->mVertScrollbar->mTotalSize=aTotal;
        this->mVertScrollbar->mPageSize=aPage;
        this->mVertScrollbar->mPageScrollSize=aPage;
        this->mVertScrollbar->mLineUpScrollSize=this->mScrollArea->mUpScrollAmount;
        this->mVertScrollbar->mLineDownScrollSize=this->mScrollArea->mDownScrollAmount;
        if(!becauseOfScroller) this->mVertScrollbar->mPosition=this->mScrollArea->mVertOffset;
        this->mVertScrollbar->LayoutThumb(!becauseOfScroller);
        this->mVertScrollbar->SetEnabled(anEnabled);
        this->mVertScrollbar->Invalidate();
    }
    if(this->mHorzScrollbar.get()!=NULL) {
        bool anEnabled=ee_won_gui_scrollbar_sync(anArea,false,&aTotal,&aPage);
        this->mHorzScrollbar->mTotalSize=aTotal;
        this->mHorzScrollbar->mPageSize=aPage;
        this->mHorzScrollbar->mPageScrollSize=aPage;
        if(!becauseOfScroller) this->mHorzScrollbar->mPosition=this->mScrollArea->mHorzOffset;
        this->mHorzScrollbar->LayoutThumb(!becauseOfScroller);
        this->mHorzScrollbar->SetEnabled(anEnabled);
        this->mHorzScrollbar->Invalidate();
    }
    this->SetScrollbarConditions(this->mHorzCondition,this->mVertCondition);
}
