// Tab bar and tab control: tab buttons, their pages, arrow scrolling and keyboard selection (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// Creates a tab bar for its tab control with no current tab and a 4-pixel overlap between tabs
TabBar::TabBar(TabCtrl* inCtrl) : mCurTab(NULL),mTabCtrl(inCtrl) { this->mOverlap=4; }
// FUNCTION: WON 0x00708280
// Returns the control id of the selected tab button, or -1 when no tab is selected
int TabBar::GetCurTabId() { return this->mCurTab==NULL ? -1 : this->mCurTab->mControlId; }
// FUNCTION: WON 0x007082a0
// Places a tab button right after the previous tab, overlapping it by the bar's overlap (the first tab at 0)
void TabBar::PositionTab(Button* inTab)
{
    int x=0;
    int id=inTab->mControlId;
    if(id>0 && id<=this->mTabVector.size()) {
        Button* previous=this->mTabVector[id-1].get();
        x=previous->Right()-this->mOverlap;
    }
    inTab->SetPos(x,0);
}
// The temporary smart pointer from push_back is destroyed before PositionTab
// FUNCTION: WON 0x00708300
// Appends a tab button to the bar: gives it the next id, positions it, widens the bar and adds it as a child
void TabBar::AddTab(Button* inTab)
{
    inTab->SetButtonFlags(0x40,true);
    inTab->mControlId=this->mTabVector.size();
    this->mTabVector.push_back(inTab);
    this->PositionTab(inTab);
    this->SetSize(inTab->Left()+inTab->Width(),this->Height());
    this->AddChildEx(inTab,0);
}
// mCurTab is read again after the SetCheck call
// FUNCTION: WON 0x00708400
// Selects an enabled tab by id: unchecks the old tab and puts it back on the bar, checks the new one, marks
// the user interaction and tells the tab control to switch pages; reselecting only requests focus
void TabBar::SetCurTab(int inId,bool inRequestFocus)
{
    if(inId<0 || inId>=this->mTabVector.size()) return;
    int oldId=this->GetCurTabId();
    if(oldId==inId) {
        if(inRequestFocus && this->mCurTab!=NULL) this->mCurTab->RequestFocus();
        return;
    }
    Button* tab=this->mTabVector[inId].get();
    if(tab->IsDisabled()) return;
    if(this->mCurTab!=NULL) {
        this->mCurTab->SetCheck(false,false);
        if(this->mCurTab->mParent!=this) {
            this->AddChildEx(this->mCurTab,int(this->mTabVector.size())-oldId-1);
            this->PositionTab(this->mCurTab);
        }
    }
    this->mCurTab=tab;
    tab->SetCheck(true,false);
    this->mTabCtrl->mUserHasInteracted=true;
    this->mTabCtrl->NotifyTabChange(inRequestFocus);
}
// FUNCTION: WON 0x00708500
// Selects the first tab whose button text equals the given name; does nothing when none matches
void TabBar::SetCurTabByName(const GUIString& inName,bool inRequestFocus)
{
    for(int i=0;i<this->mTabVector.size();++i) {
        if(this->mTabVector[i]->GetText()==inName) {
            this->SetCurTab(i,inRequestFocus);
            return;
        }
    }
}
// The id is not range-checked again after the nested SetCurTab
// FUNCTION: WON 0x00708560
// Enables or disables a tab; disabling the selected tab first selects its neighbour (left, or right for tab 0)
void TabBar::EnableTab(int inId,bool inEnabled)
{
    if(inId<0 || inId>=this->mTabVector.size()) return;
    int oldId=this->GetCurTabId();
    if(oldId==inId && !inEnabled)
        this->SetCurTab(oldId>0 ? oldId-1 : oldId+1,true);
    Button* tab=this->mTabVector[inId].get();
    if(tab!=NULL) tab->SetEnabled(inEnabled);
}
// FUNCTION: WON 0x007085d0
// Returns whether a tab is disabled; ids out of range and empty slots count as disabled
bool TabBar::Disabled(int inId)
{
    if(inId<0 || inId>=this->mTabVector.size())
        return true;
    Button* tab=this->mTabVector[inId].get();
    if(tab!=NULL)
        return tab->IsDisabled()?true:false;
    return true;
}
// FUNCTION: WON 0x00708610
// Selects a tab when its own button is pressed (the sender must be the button at its id); passes other events on
void TabBar::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==ComponentEvent_ButtonPressed) {
        int id=inEvent->GetControlId();
        if(id>=0 && id<this->mTabVector.size() && inEvent->mComponent==this->mTabVector[id].get()) {
            this->SetCurTab(id,true);
            return;
        }
    }
    Component::HandleComponentEvent(inEvent);
}

// FUNCTION: WON 0x00708960
// Copies another tab control's tab bar offsets, height, overlap and placement flags (not padding or selection)
void TabCtrl::CopyAttributes(TabCtrl* inFrom)
{
    this->mBottomTabLeftOffset=inFrom->mBottomTabLeftOffset;
    this->mBottomTabTopOffset=inFrom->mBottomTabTopOffset;
    this->mTabBarHeight=inFrom->mTabBarHeight;
    this->mArrowsOnLeft=inFrom->mArrowsOnLeft;
    this->mTabBarOnRight=inFrom->mTabBarOnRight;
    this->mTabBarOnBottom=inFrom->mTabBarOnBottom;
    this->mTabBarOverlap=inFrom->mTabBarOverlap;
    this->mBottomTabBar->mOverlap=inFrom->mBottomTabBar->mOverlap;
}
// FUNCTION: WON 0x007089e0
// Returns the container's desired size grown by the page padding and the tab bar height
void TabCtrl::GetDesiredSize(int& outWidth,int& outHeight)
{
    Container::GetDesiredSize(outWidth,outHeight);
    outWidth+=this->mRightPad+this->mLeftPad;
    outHeight+=this->mTabBarHeight+this->mBottomPad+this->mTopPad;
}
// FUNCTION: WON 0x00708a30
// Sets whether the tab bar is aligned to the right edge
void TabCtrl::SetTabBarOnRight(bool inFlag) { this->mTabBarOnRight=inFlag; }
// FUNCTION: WON 0x00708a40
// Sets whether the tab scroll arrows sit on the left
void TabCtrl::SetArrowsOnLeft(bool inFlag) { this->mArrowsOnLeft=inFlag; }
// FUNCTION: WON 0x00708a50
// Installs the left and right tab scroll arrow buttons, kept on top, and adds them as children
void TabCtrl::SetTabArrows(Button* inLeft,Button* inRight)
{
    this->mTabBarLeft=inLeft;
    this->mTabBarRight=inRight;
    this->mTabBarLeft->SetComponentFlags(ComponentFlag_AlwaysOnTop,true);
    this->mTabBarRight->SetComponentFlags(ComponentFlag_AlwaysOnTop,true);
    this->mTabBarLeft->SetButtonFlags(2,true);
    this->mTabBarRight->SetButtonFlags(2,true);
    this->AddChild(this->mTabBarLeft.get());
    this->AddChild(this->mTabBarRight.get());
}
// The new page pointer is kept in a local across the old page's SetVisible call
// FUNCTION: WON 0x00708b20
// Shows the page of the newly selected tab: lifts the tab button onto the top bar, hides the old page,
// lays out, shows and raises the new one, scrolls the tab into view and fires a tab-selected event
void TabCtrl::NotifyTabChange(bool inRequestFocus)
{
    Button* button=this->mBottomTabBar->mCurTab;
    if(button==NULL) return;
    this->mTopTabBar->AddChild(button);
    int id=this->mBottomTabBar->GetCurTabId();
    if(id<0 || id>=this->mTabVector.size()) return;
    Container* selected=this->mTabVector[id];
    if(selected==this->mCurTab) return;
    if(this->mCurTab!=NULL) this->mCurTab->SetVisible(false);
    this->mCurTab=selected;
    this->PositionTabContainer(selected);
    this->mCurTab->SetVisible(true);
    this->BringToTop(this->mCurTab);
    if(inRequestFocus) this->mCurTab->RequestFocus();
    this->KeepTabButtonInView();
    this->Invalidate();
    this->Fn2F5380(ComponentEvent_TabCtrlTabSelected,0);
}
// The original's first branch uses the flags of the left-position add across the following
// mov/lea, so the first test is left<0, not right<0
// FUNCTION: WON 0x00708bf0
// Scrolls the tab bar so the selected tab button is fully visible (its left edge first when it does not fit)
void TabCtrl::KeepTabButtonInView()
{
    Button* button=this->mBottomTabBar->mCurTab;
    if(button==NULL) return;
    int left=this->mBottomTabBar->Left()+button->Left();
    int width=button->Width();
    int right=left+width;
    if(left>=0) {
        int limit=this->mTopTabBarContainer->Width();
        if(width<=limit) {
            if(right>limit) this->AdjustTabBarScroll(limit-right);
            return;
        }
    }
    this->AdjustTabBarScroll(-left);
}
// FUNCTION: WON 0x00708c40
// Fits a page into the page area below or above the tab bar, inside the padding
void TabCtrl::PositionTabContainer(Container* inContainer)
{
    if(inContainer==NULL) return;
    inContainer->SetPos(this->mLeftPad,this->mContainerLayout->Top()+this->mTopPad);
    inContainer->SetSize(this->Width()-this->mRightPad-this->mLeftPad,
        this->mContainerLayout->Height()-this->mBottomPad-this->mTopPad);
}
// FUNCTION: WON 0x00708cb0
// Selects the first tab when none is selected yet, then clears the user-interaction flag
void TabCtrl::AddedToParent()
{
    Container::AddedToParent();
    if(this->mCurTab==NULL) this->SetCurTab(0,false);
    this->mUserHasInteracted=false;
}
// FUNCTION: WON 0x00708ce0
// Sets the page area to the space not taken by the tab bar (above or below it, minus the overlap)
void TabCtrl::CalcContainerLayout()
{
    int top=0;
    int bottom=this->Height();
    if(this->mTabBarOnBottom) bottom=this->mTopTabBarContainer->Top()+this->mTabBarOverlap;
    else top=this->mTopTabBarContainer->Bottom()-this->mTabBarOverlap;
    this->mContainerLayout->SetPosSize(0,top,this->Width(),bottom-top);
}
// FUNCTION: WON 0x00708d40
// Places the two scroll arrows side by side at the left or right end of the tab bar row
void TabCtrl::PositionArrows()
{
    Button* left=this->mTabBarLeft.get();
    if(left==NULL || this->mTabBarRight.get()==NULL) return;
    int x=0;
    if(!this->mArrowsOnLeft) x=this->Width()-this->mTabBarRight->Width()-left->Width();
    int y;
    if(this->mTabBarOnBottom) y=this->mContainerLayout->Bottom();
    else y=this->mContainerLayout->Top()-left->Height();
    left->SetPos(x,y);
    this->mTabBarRight->SetPos(this->mTabBarLeft->Right(),y);
}
// FUNCTION: WON 0x00708dc0
// Sizes the tab bars to the tab bar height and places them at the top or bottom of the control
void TabCtrl::VertPositionTabBar()
{
    this->mBottomTabBar->SetSize(this->mBottomTabBar->Width(),
        this->mTabBarHeight-this->mTabBarOverlap-this->mBottomTabTopOffset);
    this->mBottomTabBarContainer->SetSize(this->mBottomTabBarContainer->Width(),this->mBottomTabBar->Height());
    this->mTopTabBar->SetSize(this->mTopTabBar->Width(),this->mTabBarHeight);
    this->mTopTabBarContainer->SetSize(this->mTopTabBarContainer->Width(),this->mTabBarHeight);
    int y=0;
    int offset=this->mBottomTabTopOffset;
    if(this->mTabBarOnBottom) { y=this->Height()-this->mBottomTabBarContainer->Height(); offset=-offset; }
    this->mTopTabBarContainer->SetPos(this->mTopTabBarContainer->Left(),y);
    this->mBottomTabBarContainer->SetPos(this->mBottomTabBarContainer->Left(),offset+y);
}
// FUNCTION: WON 0x00708e80
// Sets the horizontal extent of the tab bar containers, leaving room for visible scroll arrows or
// right-aligning the bar when it fits
void TabCtrl::HorzPositionTabBar()
{
    int left=0;
    int right=this->Width();
    if(this->NeedScrollButtons()) {
        if(this->mArrowsOnLeft) {
            Button* arrow=this->mTabBarRight.get();
            if(arrow!=NULL && arrow->IsVisible()) left=arrow->Right();
        } else {
            Button* arrow=this->mTabBarLeft.get();
            if(arrow!=NULL && arrow->IsVisible()) right=arrow->Left()-1;
        }
    } else if(this->mTabBarOnRight) left=this->Width()-this->mBottomTabBar->Width()-this->mBottomTabLeftOffset;
    this->mTopTabBarContainer->SetPos(left,this->mTopTabBarContainer->Top());
    this->mTopTabBarContainer->SetSize(right-left,this->mTopTabBarContainer->Height());
    this->mBottomTabBarContainer->SetPos(this->mBottomTabLeftOffset+left,this->mBottomTabBarContainer->Top());
    this->mBottomTabBarContainer->SetSize(right-this->mBottomTabLeftOffset-left,this->mBottomTabBarContainer->Height());
}
// FUNCTION: WON 0x00708f50
// Lays out the tab control: shows the arrows only when the tabs overflow, positions bars, page area, arrows
// and current page, clamps the scroll and keeps the selected tab in view until the user interacts
void TabCtrl::Virtual08()
{
    if(this->mTabBarLeft.get()!=NULL && this->mTabBarRight.get()!=NULL) {
        bool need=this->NeedScrollButtons();
        this->mTabBarLeft->SetVisible(need);
        this->mTabBarRight->SetVisible(need);
    }
    this->VertPositionTabBar();
    this->CalcContainerLayout();
    this->PositionArrows();
    this->HorzPositionTabBar();
    this->PositionTabContainer(this->mCurTab);
    this->AdjustTabBarScroll(0);
    if(!this->mUserHasInteracted) this->KeepTabButtonInView();
    Container::Virtual08();
}
// FUNCTION: WON 0x00708ff0
// Returns whether the tabs are wider than the control
bool TabCtrl::NeedScrollButtons() { return this->mBottomTabBar->Width()>this->Width(); }
// FUNCTION: WON 0x00709010
// Scrolls the tab bars by an amount, clamped to the tab extent, enabling each arrow only when it can scroll;
// when everything fits the bars return to 0 and the arrows are left as they are
void TabCtrl::AdjustTabBarScroll(int inAmount)
{
    int oldX=this->mBottomTabBar->Left();
    int x=oldX+inAmount;
    int width=this->mBottomTabBar->Width();
    int containerWidth=this->mBottomTabBarContainer->Width();
    if(containerWidth>=width) x=0;
    else {
        bool enableLeft=true,enableRight=true;
        if(x+width<containerWidth-5) { enableRight=false; x=containerWidth-width-5; }
        if(x>=0) { enableLeft=false; x=0; }
        if(this->mTabBarLeft.get()!=NULL && this->mTabBarRight.get()!=NULL) {
            this->mTabBarLeft->SetEnabled(enableLeft);
            this->mTabBarRight->SetEnabled(enableRight);
        }
    }
    if(x==oldX)
        return;
    this->mBottomTabBar->SetPos(x,this->mBottomTabBar->Top());
    this->mTopTabBar->SetPos(x,this->mTopTabBar->Top());
    this->mBottomTabBarContainer->Invalidate();
    this->mTopTabBarContainer->Invalidate();
}
// FUNCTION: WON 0x007090e0
// Scrolls the tab bar by 10 pixels when a scroll arrow is pressed; passes other events on
void TabCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==ComponentEvent_ButtonPressed) {
        int d=0;
        if(inEvent->mComponent==this->mTabBarLeft.get()) d=10;
        else if(inEvent->mComponent==this->mTabBarRight.get()) d=-10;
        if(d!=0) {
            this->mUserHasInteracted=true;
            this->AdjustTabBarScroll(d);
            return;
        }
    }
    Component::HandleComponentEvent(inEvent);
}
// mTopTabBar is read again after each child TabFocus call
// FUNCTION: WON 0x00709140
// Moves keyboard focus between the selected tab button and its page (tab button first going forward,
// page first going back); falls back to the container's order when no tab is selected
bool TabCtrl::TabFocus(bool inForward,bool inInit)
{
    Button* button=this->mBottomTabBar->mCurTab;
    if(button!=NULL) {
        Container* current=this->mCurTab;
        if(current!=NULL) {
            if(inInit) {
                if(!inForward) {
                    bool done=current->TabFocus(false,true);
                    if(done) return done;
                }
                bool result=button->TabFocus(inForward,true);
                if(result) this->mTopTabBar->SetHasUsedDialogKeys(true);
                return result;
            }
            if(button->HasFocus()) {
                if(inForward) return current->TabFocus(true,true);
                return false;
            }
            if(current->TabFocus(inForward,false)) return true;
            if(inForward) return false;
            if(button->TabFocus(false,true)) {
                this->mTopTabBar->SetHasUsedDialogKeys(true);
                return true;
            }
            return false;
        }
    }
    return Container::TabFocus(inForward,inInit);
}
// The disabled test comes before the loop bound tests
// FUNCTION: WON 0x00709240
// Selects the next or previous enabled tab on Right/Left while a tab button has focus, then focuses it
bool TabCtrl::KeyDown(int inKey)
{
    Button* button=this->mBottomTabBar->mCurTab;
    if(button!=NULL && button->HasFocus()) {
        int step;
        if(inKey==39) step=1;
        else if(inKey==37) step=-1;
        else return Container::KeyDown(inKey);
        int old=this->mBottomTabBar->GetCurTabId();
        int next=old+step;
        while(this->mBottomTabBar->Disabled(next) && next>0 && next<this->mTabVector.size()-1) {
            if(inKey==39) { ++step; ++next; }
            else if(inKey==37) { --step; --next; }
        }
        this->mBottomTabBar->SetHasUsedDialogKeys(true);
        this->mTopTabBar->SetHasUsedDialogKeys(true);
        this->mUserHasInteracted=true;
        this->SetCurTab(old+step,false);
        Button* current=this->mBottomTabBar->mCurTab;
        if(current!=NULL) current->RequestFocus();
        return true;
    }
    return Container::KeyDown(inKey);
}
// FUNCTION: WON 0x00709350
// Selects a tab by id (forwards to the bottom tab bar)
void TabCtrl::SetCurTab(int inId,bool inFocus) { this->mBottomTabBar->SetCurTab(inId,inFocus); }
// FUNCTION: WON 0x00709370
// Selects a tab by its button text (forwards to the bottom tab bar)
void TabCtrl::SetCurTabByName(const GUIString& inName,bool inFocus) { this->mBottomTabBar->SetCurTabByName(inName,inFocus); }
// FUNCTION: WON 0x007093d0
// Enables or disables a tab (forwards to the bottom tab bar)
void TabCtrl::EnableTab(int inId,bool inEnable) { this->mBottomTabBar->EnableTab(inId,inEnable); }
// FUNCTION: WON 0x00709390
// Returns the id of the tab showing the given page, or -1
int TabCtrl::GetTabIdForContainer(Container* inContainer)
{
    for(int i=0;i<this->mTabVector.size();++i) if(this->mTabVector[i]==inContainer) return i;
    return -1;
}
// FUNCTION: WON 0x007093f0
// Adds a tab button and its page (hidden, clipped); the first tab sets the bar height (default 20) and
// the control lays out again when the tabs start or stop overflowing
void TabCtrl::AddTab(Button* inTab,Container* inContainer)
{
    if(this->mTabBarHeight==0) {
        if(inTab->Height()>0) this->mTabBarHeight=inTab->Height();
        else this->mTabBarHeight=20;
    }
    inTab->SetSize(inTab->Width(),this->mTabBarHeight);
    inTab->SetComponentFlags(ComponentFlag_GrabBG,false);
    this->mTabVector.push_back(inContainer);
    bool oldNeed=this->NeedScrollButtons();
    this->mBottomTabBar->AddTab(inTab);
    if(inContainer!=NULL) {
        inContainer->SetVisible(false);
        inContainer->SetComponentFlags(ComponentFlag_Clip,true);
        this->AddChild(inContainer);
    }
    if(this->NeedScrollButtons()!=oldNeed) this->Virtual08();
}
}

namespace WONAPI {
// FUNCTION: WON 0x00708670
// Creates a tab control with 5-pixel padding, no tabs, and its tab bar containers and page layout point
TabCtrl::TabCtrl()
{
    this->mLeftPad=this->mRightPad=this->mTopPad=this->mBottomPad=5;
    this->mBottomTabLeftOffset=2;
    this->mBottomTabTopOffset=3;
    this->mCurTab=NULL;
    this->mTabBarHeight=0;
    this->mTabBarOverlap=1;
    this->mTabBarOnBottom=this->mTabBarOnRight=this->mArrowsOnLeft=false;
    this->mBottomTabBarContainer=new Container;
    this->mTopTabBarContainer=new Container;
    this->mBottomTabBar=new TabBar(this);
    this->mTopTabBar=new Container;
    this->mTopTabBar->SetOnlyChildInput(true);
    this->mTopTabBarContainer->SetOnlyChildInput(true);
    this->mTopTabBarContainer->SetComponentFlags(ComponentFlag_GrabBG,true);
    this->mBottomTabBarContainer->SetComponentFlags(ComponentFlag_GrabBG,true);
    this->mBottomTabBarContainer->SetComponentFlags(ComponentFlag_WantFocus,false);
    this->mBottomTabBarContainer->SetComponentFlags(ComponentFlag_Clip,true);
    this->mTopTabBarContainer->SetComponentFlags(0x3000,true);
    this->mBottomTabBarContainer->AddChild(this->mBottomTabBar.get());
    this->mTopTabBarContainer->AddChild(this->mTopTabBar.get());
    this->AddChild(this->mBottomTabBarContainer.get());
    this->mContainerLayout=new LayoutPoint;
    this->AddLayoutPoint(this->mContainerLayout.get());
    this->AddChild(this->mTopTabBarContainer.get());
}
}

namespace WONAPI {
// Tab removal (not identified in the original exe): TabBar::RemoveTab removes a tab button, renumbers
// the rest and puts them back on the bar; TabCtrl::RemoveTab also removes the page and reselects a tab
void TabBar::RemoveTab(int theId)
{
	if(theId<0 || theId>=this->mTabVector.size())
		return;

	int aCurTab = this->GetCurTabId();

	this->RemoveChild(this->mTabVector[theId]);
	this->mTabVector.erase(this->mTabVector.begin() + theId);
	int i;
	for(i=0; i<this->mTabVector.size(); i++)
		this->mTabVector[i]->mControlId=i;

	this->mCurTab = NULL;
	for(i=0; i<this->mTabVector.size(); i++)
	{
		Button *aButton = this->mTabVector[i].get();
		if(aButton->GetParent()!=this)
			this->AddChildEx(aButton,this->mTabVector.size()-i-1);
			
		this->PositionTab(aButton);
	}
}

void TabCtrl::RemoveTab(int theId)
{
	if(theId<0 || theId>=this->mTabVector.size())
		return;

	int aCurTabId = this->mBottomTabBar->GetCurTabId();

	this->mTopTabBar->RemoveChild(this->mBottomTabBar->mCurTab);
	this->mCurTab = NULL;

	this->RemoveChild(this->mTabVector[theId]);
	this->mTabVector.erase(this->mTabVector.begin() + theId);
	this->mBottomTabBar->RemoveTab(theId);
	

	if(theId<aCurTabId)
		this->SetCurTab(aCurTabId-1,true);
	else if(theId>aCurTabId)
		this->SetCurTab(aCurTabId,true);
	else 
		this->SetCurTab(0,true);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007081d0
// (public: virtual void * __thiscall WONAPI::TabBar::`scalar deleting destructor'(unsigned int)    [??_GTabBar@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00708940
// (public: virtual void * __thiscall WONAPI::TabCtrl::`scalar deleting destructor'(unsigned int)    [??_GTabCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071d270
// (protected: bool __thiscall std::vector<class WONAPI::Container *,class std::allocator<class WONAPI::Container *> >::_Buy(unsigned int)    [?_Buy@?$vec)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007081f0
// (public: virtual __thiscall WONAPI::TabBar::~TabBar(void))
// TEMPLATE: WON 0x00723cf0
// (protected: class WONAPI::SmartPtr<class WONAPI::Button> * __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::Button>,class std::allocator<cla)
// TEMPLATE: WON 0x00708160
// (public: __thiscall WONAPI::TabBar::TabBar(class WONAPI::TabCtrl *))
// TEMPLATE: WON 0x0071d1b0
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::Button>,int,class WONAPI::SmartPtr<class WONAPI::Button> *,class WONAPI::SmartPtr<class)
// TEMPLATE: WON 0x0071d210
// (public: class std::_Ptrit<class WONAPI::Container *,int,class WONAPI::Container * *,class WONAPI::Container * &,class WONAPI::Container * *,class WONA)
// TEMPLATE: WON 0x00721820
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::Button>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::Button> > >)
// TEMPLATE: WON 0x00721c10
// (public: void __thiscall std::vector<class WONAPI::Container *,class std::allocator<class WONAPI::Container *> >::insert(class std::_Ptrit<class WONAPI)
// TEMPLATE: WON 0x00723900
// (protected: void __thiscall std::vector<class WONAPI::Container *,class std::allocator<class WONAPI::Container *> >::_Xlen(void)const )
