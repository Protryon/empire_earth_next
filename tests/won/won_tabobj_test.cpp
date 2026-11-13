// TabBar and TabCtrl as whole objects (R6 WON round 27): the original TabCtrl.cpp, the statements copied from the C++
// before the port with every call on a C++ object (the buttons, the pages, the arrows, the layout point, the event,
// the bar's and the control's own virtuals) replaced by the host op that makes it (TabCtrl.cpp's host runs the same
// calls in the same order), every Container::X call made on the object's real Rust container and every call on a
// container part's Container::X made on that part's Rust container, against ee_won's Rust TabBar and TabCtrl
// (tabobj.rs). The arithmetic is guictl's (ee_won_gui_tab_*, checked against the original by won_guictl_test).
//
// The world is the container goldens' (won_ct_world.h), with nineteen components: 1 the control, 2 its bottom bar,
// 3 its top bar, 4 and 5 the bars' containers, 6 the control's parent (all six Rust containers on each side), 7 the
// page area's layout point, 8 and 9 the scroll arrows, 10..14 the tab buttons, 15..19 the pages. A call the world
// makes on the control runs its overrides on the side running. A component's SetPos, SetSize, SetEnabled and
// SetVisible run Component's statements in the world (a size change calls the container's Virtual08; a state change
// tells the parent and invalidates). A button's text is its id modulo 3. The event's type, control id and component
// are the driver's. The event the control fires is traced as the type the C++ delivers.
//
// Each of 20,000 sequences builds the control (its constructor's statements over the parts), adds it to its parent
// (or not), and makes 40 random calls on it and its bar (tabs added and removed, selections by id and name, enabled
// tabs, arrows, keys, focus, events from the arrows and the tabs, layouts, fields), with random changes of the world
// between them, then frees the control, the bar and the containers. The traces must be the same, every reference
// given back. The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_tabobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"
#include "ee/wonapi/WONEventTypes.h"

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;
typedef EeWonCtCall C;
typedef EeWonTabCall T;

#define WON_CT_N 19
#define WON_CT_CT 6
static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c);
static intptr_t CtObjectOf(intptr_t ct);
static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
#include "won_ct_world.h"

static const intptr_t kCtrl = 1, kBar = 2, kTopBar = 3, kBottomCt = 4, kTopCt = 5, kParent = 6, kLayout = 7;
static const intptr_t kLeft = 8, kRight = 9, kButtons = 10, kPages = 15;
static intptr_t sEventType, sEventControl, sEventComp;
static EeWonContainer* sCt[kCt + 1];

// ---- the original (TabCtrl.cpp; Container::X on the objects' Rust containers)

namespace orig {
struct TabCtrl;
static intptr_t H(intptr_t self, Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return Host((void*)self, op, a, b, c, d); }
static intptr_t Ct(intptr_t ct, C call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_ct_call(sCt[ct], call, a, b, c); }
static int Left(intptr_t s, intptr_t c) { return (int)H(s, Op::ChildLeft, c); }
static int Top(intptr_t s, intptr_t c) { return (int)H(s, Op::ChildTop, c); }
static int Width(intptr_t s, intptr_t c) { return (int)H(s, Op::ChildWidth, c); }
static int Height(intptr_t s, intptr_t c) { return (int)H(s, Op::ChildHeight, c); }
static int Right(intptr_t s, intptr_t c) { return (int)H(s, Op::ChildRight, c); }
static int Bottom(intptr_t s, intptr_t c) { return Top(s, c) + Height(s, c); }
static void SetPos(intptr_t s, intptr_t c, int x, int y) { H(s, Op::ChildSetPos, c, x, y); }
static void SetSize(intptr_t s, intptr_t c, int w, int h) { H(s, Op::ChildSetSize, c, w, h); }
static bool IsDisabled(intptr_t s, intptr_t c) { return (H(s, Op::CtBits, c) & 0x80) != 0; }
static bool HasFocus(intptr_t s, intptr_t c) { return (H(s, Op::CtBits, c) & 0x10) != 0; }
static bool IsVisible(intptr_t s, intptr_t c) { return (H(s, Op::CtBits, c) & 0x1) == 0; }

struct TabBar {
    std::vector<intptr_t> mTabVector;
    intptr_t mCurTab;
    TabCtrl* mTabCtrl;
    int mOverlap;
    TabBar(TabCtrl* inCtrl) : mCurTab(0), mTabCtrl(inCtrl) { this->mOverlap = 4; }
    int GetCurTabId() { return this->mCurTab == 0 ? -1 : (int)H(kBar, Op::ChildControlId, this->mCurTab); }
    void PositionTab(intptr_t inTab)
    {
        int id = (int)H(kBar, Op::ChildControlId, inTab);
        int aPrevRight = 0;
        if (id > 0 && id <= (int)this->mTabVector.size()) aPrevRight = Right(kBar, this->mTabVector[id - 1]);
        SetPos(kBar, inTab, ee_won_gui_tab_x(id, this->mTabVector.size(), aPrevRight, this->mOverlap), 0);
    }
    void AddTab(intptr_t inTab)
    {
        H(kBar, Op::ButtonSetFlags, inTab, 0x40, 1);
        H(kBar, Op::ChildSetControlId, inTab, this->mTabVector.size());
        H(kBar, Op::ChildHold, inTab);
        this->mTabVector.push_back(inTab);
        this->PositionTab(inTab);
        SetSize(kBar, kBar, Left(kBar, inTab) + Width(kBar, inTab), Height(kBar, kBar));
        Ct(kBar, C::AddChildEx, inTab, 0);
    }
    void SetCurTab(int inId, bool inRequestFocus);
    void SetCurTabByName(intptr_t inName, bool inRequestFocus)
    {
        for (int i = 0; i < (int)this->mTabVector.size(); ++i) {
            if (H(kBar, Op::TabTextIs, this->mTabVector[i], inName)) {
                this->SetCurTab(i, inRequestFocus);
                return;
            }
        }
    }
    void EnableTab(int inId, bool inEnabled)
    {
        if (inId < 0 || inId >= (int)this->mTabVector.size()) return;
        int oldId = this->GetCurTabId();
        if (oldId == inId && !inEnabled) this->SetCurTab(ee_won_gui_tab_neighbour(oldId), true);
        intptr_t tab = this->mTabVector[inId];
        if (tab != 0) H(kBar, Op::ChildSetEnabled, tab, inEnabled);
    }
    bool Disabled(int inId)
    {
        if (inId < 0 || inId >= (int)this->mTabVector.size()) return true;
        intptr_t tab = this->mTabVector[inId];
        if (tab != 0) return IsDisabled(kBar, tab);
        return true;
    }
    void HandleComponentEvent(intptr_t inEvent)
    {
        if (H(kBar, Op::EventType, inEvent) == 15) {
            int id = (int)H(kBar, Op::EventControlId, inEvent);
            if (id >= 0 && id < (int)this->mTabVector.size() && H(kBar, Op::EventComponent, inEvent) == this->mTabVector[id]) {
                this->SetCurTab(id, true);
                return;
            }
        }
        H(kBar, Op::ComponentHandleEvent, inEvent);
    }
    void RemoveTab(int theId)
    {
        if (theId < 0 || theId >= (int)this->mTabVector.size()) return;
        int aCurTab = this->GetCurTabId();
        (void)aCurTab;
        intptr_t gone = this->mTabVector[theId];
        H(kBar, Op::CtVirt, (intptr_t)V::RemoveChild, gone);
        this->mTabVector.erase(this->mTabVector.begin() + theId);
        H(kBar, Op::ChildRelease, gone);
        int i;
        for (i = 0; i < (int)this->mTabVector.size(); i++) H(kBar, Op::ChildSetControlId, this->mTabVector[i], i);
        this->mCurTab = 0;
        for (i = 0; i < (int)this->mTabVector.size(); i++) {
            intptr_t aButton = this->mTabVector[i];
            if (H(kBar, Op::CtParentOf, aButton) != kBar) Ct(kBar, C::AddChildEx, aButton, this->mTabVector.size() - i - 1);
            this->PositionTab(aButton);
        }
    }
    void Destroy()
    {
        for (size_t i = 0; i < this->mTabVector.size(); i++) H(kBar, Op::ChildRelease, this->mTabVector[i]);
        this->mTabVector.clear();
    }
};

struct TabCtrl {
    TabBar* mBottomTabBar;
    int mLeftPad, mRightPad, mTopPad, mBottomPad;
    int mBottomTabLeftOffset, mBottomTabTopOffset;
    int mTabBarHeight, mTabBarOverlap;
    intptr_t mTabBarLeft, mTabBarRight;
    bool mArrowsOnLeft, mTabBarOnRight, mTabBarOnBottom, mUserHasInteracted;
    std::vector<intptr_t> mTabVector;
    intptr_t mCurTab;
    static const intptr_t me = kCtrl;

    TabCtrl()
    {
        this->mLeftPad = this->mRightPad = this->mTopPad = this->mBottomPad = 5;
        this->mBottomTabLeftOffset = 2;
        this->mBottomTabTopOffset = 3;
        this->mCurTab = 0;
        this->mTabBarHeight = 0;
        this->mTabBarOverlap = 1;
        this->mTabBarOnBottom = this->mTabBarOnRight = this->mArrowsOnLeft = false;
        this->mUserHasInteracted = false; // (the original left it unset)
        this->mTabBarLeft = this->mTabBarRight = 0;
        this->mBottomTabBar = NULL;
    }
    void Init(TabBar* inBar)
    {
        this->mBottomTabBar = inBar;
        Ct(kTopBar, C::SetField, (intptr_t)EeWonCtField::OnlyChildInput, 1);
        Ct(kTopCt, C::SetField, (intptr_t)EeWonCtField::OnlyChildInput, 1);
        H(me, Op::ChildSetFlags, kTopCt, 0x100, 1);
        H(me, Op::ChildSetFlags, kBottomCt, 0x100, 1);
        H(me, Op::ChildSetFlags, kBottomCt, 0x8, 0);
        H(me, Op::ChildSetFlags, kBottomCt, 0x2000, 1);
        H(me, Op::ChildSetFlags, kTopCt, 0x3000, 1);
        Ct(kBottomCt, C::AddChild, kBar);
        Ct(kTopCt, C::AddChild, kTopBar);
        Ct(me, C::AddChild, kBottomCt);
        H(me, Op::TabAddLayoutPoint, kLayout);
        Ct(me, C::AddChild, kTopCt);
    }
    void Destroy()
    {
        if (this->mTabBarRight != 0) H(me, Op::ChildRelease, this->mTabBarRight);
        if (this->mTabBarLeft != 0) H(me, Op::ChildRelease, this->mTabBarLeft);
        this->mTabBarLeft = this->mTabBarRight = 0;
        this->mBottomTabBar->mTabCtrl = NULL;
    }
    void SetArrow(intptr_t& field, intptr_t b)
    {
        if (field != b) {
            if (field != 0) H(me, Op::ChildRelease, field);
            field = b;
            if (b != 0) H(me, Op::ChildHold, b);
        }
    }
    void GetDesiredSize(int& outWidth, int& outHeight)
    {
        Ct(me, C::DesiredSize, (intptr_t)&outWidth, (intptr_t)&outHeight);
        outWidth += this->mRightPad + this->mLeftPad;
        outHeight += this->mTabBarHeight + this->mBottomPad + this->mTopPad;
    }
    void SetTabArrows(intptr_t inLeft, intptr_t inRight)
    {
        SetArrow(this->mTabBarLeft, inLeft);
        SetArrow(this->mTabBarRight, inRight);
        H(me, Op::ChildSetFlags, this->mTabBarLeft, 0x1000, 1);
        H(me, Op::ChildSetFlags, this->mTabBarRight, 0x1000, 1);
        H(me, Op::ButtonSetFlags, this->mTabBarLeft, 2, 1);
        H(me, Op::ButtonSetFlags, this->mTabBarRight, 2, 1);
        Ct(me, C::AddChild, this->mTabBarLeft);
        Ct(me, C::AddChild, this->mTabBarRight);
    }
    void NotifyTabChange(bool inRequestFocus)
    {
        intptr_t button = this->mBottomTabBar->mCurTab;
        if (button == 0) return;
        Ct(kTopBar, C::AddChild, button);
        int id = this->mBottomTabBar->GetCurTabId();
        if (id < 0 || id >= (int)this->mTabVector.size()) return;
        intptr_t selected = this->mTabVector[id];
        if (selected == this->mCurTab) return;
        if (this->mCurTab != 0) H(me, Op::ChildSetVisible, this->mCurTab, 0);
        this->mCurTab = selected;
        this->PositionTabContainer(selected);
        H(me, Op::ChildSetVisible, this->mCurTab, 1);
        Ct(me, C::BringToTop, this->mCurTab);
        if (inRequestFocus) H(me, Op::CtChild, this->mCurTab, (intptr_t)V::RequestFocus);
        this->KeepTabButtonInView();
        H(me, Op::Invalidate);
        H(me, Op::FireEvent, -1, 0); // Fn2F5380(ComponentEvent_TabCtrlTabSelected, 0): see ExtraLine
    }
    void KeepTabButtonInView()
    {
        intptr_t button = this->mBottomTabBar->mCurTab;
        if (button == 0) return;
        int amount;
        if (ee_won_gui_tab_keep_in_view(Left(me, kBar) + Left(me, button), Width(me, button), Width(me, kTopCt), &amount))
            this->AdjustTabBarScroll(amount);
    }
    void PositionTabContainer(intptr_t inContainer)
    {
        if (inContainer == 0) return;
        EeWonRect4 r = ee_won_gui_tab_page_rect(this->mLeftPad, this->mRightPad, this->mTopPad, this->mBottomPad,
                                                Top(me, kLayout), Height(me, kLayout), Width(me, me));
        SetPos(me, inContainer, r.x, r.y);
        SetSize(me, inContainer, r.w, r.h);
    }
    void AddedToParent()
    {
        Ct(me, C::AddedToParent);
        if (this->mCurTab == 0) this->mBottomTabBar->SetCurTab(0, false);
        this->mUserHasInteracted = false;
    }
    void CalcContainerLayout()
    {
        int height;
        int top = ee_won_gui_tab_area(this->mTabBarOnBottom, Height(me, me), Top(me, kTopCt), Bottom(me, kTopCt),
                                      this->mTabBarOverlap, &height);
        EeWonRect4 r = {0, top, Width(me, me), height};
        H(me, Op::ChildSetPosSize, kLayout, (intptr_t)&r);
    }
    void PositionArrows()
    {
        intptr_t left = this->mTabBarLeft;
        if (left == 0 || this->mTabBarRight == 0) return;
        int y;
        int x = ee_won_gui_tab_arrows(this->mArrowsOnLeft, this->mTabBarOnBottom, Width(me, me), Width(me, this->mTabBarRight),
                                      Width(me, left), Height(me, left), Top(me, kLayout), Bottom(me, kLayout), &y);
        SetPos(me, left, x, y);
        SetPos(me, this->mTabBarRight, Right(me, this->mTabBarLeft), y);
    }
    void VertPositionTabBar()
    {
        SetSize(me, kBar, Width(me, kBar), ee_won_gui_tab_bottom_height(this->mTabBarHeight, this->mTabBarOverlap, this->mBottomTabTopOffset));
        SetSize(me, kBottomCt, Width(me, kBottomCt), Height(me, kBar));
        SetSize(me, kTopBar, Width(me, kTopBar), this->mTabBarHeight);
        SetSize(me, kTopCt, Width(me, kTopCt), this->mTabBarHeight);
        int aBottomY;
        int y = ee_won_gui_tab_vert(this->mTabBarOnBottom, Height(me, me), Height(me, kBottomCt), this->mBottomTabTopOffset, &aBottomY);
        SetPos(me, kTopCt, Left(me, kTopCt), y);
        SetPos(me, kBottomCt, Left(me, kBottomCt), aBottomY);
    }
    void HorzPositionTabBar()
    {
        bool aNeed = this->NeedScrollButtons();
        intptr_t aRightArrow = this->mTabBarRight;
        intptr_t aLeftArrow = this->mTabBarLeft;
        bool aRightShown = aNeed && this->mArrowsOnLeft && aRightArrow != 0 && IsVisible(me, aRightArrow);
        bool aLeftShown = aNeed && !this->mArrowsOnLeft && aLeftArrow != 0 && IsVisible(me, aLeftArrow);
        int right;
        int left = ee_won_gui_tab_horz(aNeed, this->mArrowsOnLeft, aRightShown, aRightShown ? Right(me, aRightArrow) : 0, aLeftShown,
                                       aLeftShown ? Left(me, aLeftArrow) : 0, this->mTabBarOnRight, Width(me, me),
                                       Width(me, kBar), this->mBottomTabLeftOffset, &right);
        SetPos(me, kTopCt, left, Top(me, kTopCt));
        SetSize(me, kTopCt, right - left, Height(me, kTopCt));
        SetPos(me, kBottomCt, this->mBottomTabLeftOffset + left, Top(me, kBottomCt));
        SetSize(me, kBottomCt, right - this->mBottomTabLeftOffset - left, Height(me, kBottomCt));
    }
    void Virtual08()
    {
        if (this->mTabBarLeft != 0 && this->mTabBarRight != 0) {
            bool need = this->NeedScrollButtons();
            H(me, Op::ChildSetVisible, this->mTabBarLeft, need);
            H(me, Op::ChildSetVisible, this->mTabBarRight, need);
        }
        this->VertPositionTabBar();
        this->CalcContainerLayout();
        this->PositionArrows();
        this->HorzPositionTabBar();
        this->PositionTabContainer(this->mCurTab);
        this->AdjustTabBarScroll(0);
        if (!this->mUserHasInteracted) this->KeepTabButtonInView();
        Ct(me, C::SizeChanged);
    }
    bool NeedScrollButtons() { return Width(me, kBar) > Width(me, me); }
    void AdjustTabBarScroll(int inAmount)
    {
        int oldX = Left(me, kBar);
        int x;
        bool enableLeft, enableRight;
        if (ee_won_gui_tab_scroll(oldX, inAmount, Width(me, kBar), Width(me, kBottomCt), &x, &enableLeft, &enableRight)) {
            if (this->mTabBarLeft != 0 && this->mTabBarRight != 0) {
                H(me, Op::ChildSetEnabled, this->mTabBarLeft, enableLeft);
                H(me, Op::ChildSetEnabled, this->mTabBarRight, enableRight);
            }
        }
        if (x == oldX) return;
        SetPos(me, kBar, x, Top(me, kBar));
        SetPos(me, kTopBar, x, Top(me, kTopBar));
        H(me, Op::ChildInvalidate, kBottomCt);
        H(me, Op::ChildInvalidate, kTopCt);
    }
    void HandleComponentEvent(intptr_t inEvent)
    {
        if (H(me, Op::EventType, inEvent) == 15) {
            int d = 0;
            if (H(me, Op::EventComponent, inEvent) == this->mTabBarLeft) d = 10;
            else if (H(me, Op::EventComponent, inEvent) == this->mTabBarRight) d = -10;
            if (d != 0) {
                this->mUserHasInteracted = true;
                this->AdjustTabBarScroll(d);
                return;
            }
        }
        H(me, Op::ComponentHandleEvent, inEvent);
    }
    bool ChildTabFocus(intptr_t c, bool f, bool i) { return H(me, Op::CtChild, c, (intptr_t)V::TabFocus, f, i) != 0; }
    bool TabFocus(bool inForward, bool inInit)
    {
        intptr_t button = this->mBottomTabBar->mCurTab;
        if (button != 0) {
            intptr_t current = this->mCurTab;
            if (current != 0) {
                if (inInit) {
                    if (!inForward) {
                        bool done = ChildTabFocus(current, false, true);
                        if (done) return done;
                    }
                    bool result = ChildTabFocus(button, inForward, true);
                    if (result) Ct(kTopBar, C::SetField, (intptr_t)EeWonCtField::HasUsedDialogKeys, 1);
                    return result;
                }
                if (HasFocus(me, button)) {
                    if (inForward) return ChildTabFocus(current, true, true);
                    return false;
                }
                if (ChildTabFocus(current, inForward, false)) return true;
                if (inForward) return false;
                if (ChildTabFocus(button, false, true)) {
                    Ct(kTopBar, C::SetField, (intptr_t)EeWonCtField::HasUsedDialogKeys, 1);
                    return true;
                }
                return false;
            }
        }
        return Ct(me, C::TabFocus, inForward, inInit) != 0;
    }
    static bool DisabledOf(const void* inBar, int32_t i) { return ((TabBar*)inBar)->Disabled(i); }
    bool KeyDown(int inKey)
    {
        intptr_t button = this->mBottomTabBar->mCurTab;
        if (button != 0 && HasFocus(me, button)) {
            int aTarget;
            if (!ee_won_gui_tab_key_target(inKey, this->mBottomTabBar->GetCurTabId(), this->mTabVector.size(), DisabledOf,
                                           this->mBottomTabBar, &aTarget))
                return Ct(me, C::KeyDown, inKey) != 0;
            Ct(kBar, C::SetField, (intptr_t)EeWonCtField::HasUsedDialogKeys, 1);
            Ct(kTopBar, C::SetField, (intptr_t)EeWonCtField::HasUsedDialogKeys, 1);
            this->mUserHasInteracted = true;
            this->mBottomTabBar->SetCurTab(aTarget, false);
            intptr_t current = this->mBottomTabBar->mCurTab;
            if (current != 0) H(me, Op::CtChild, current, (intptr_t)V::RequestFocus);
            return true;
        }
        return Ct(me, C::KeyDown, inKey) != 0;
    }
    int GetTabIdForContainer(intptr_t inContainer)
    {
        for (int i = 0; i < (int)this->mTabVector.size(); ++i) if (this->mTabVector[i] == inContainer) return i;
        return -1;
    }
    void AddTab(intptr_t inTab, intptr_t inContainer)
    {
        this->mTabBarHeight = ee_won_gui_tab_bar_height(this->mTabBarHeight, Height(me, inTab));
        SetSize(me, inTab, Width(me, inTab), this->mTabBarHeight);
        H(me, Op::ChildSetFlags, inTab, 0x100, 0);
        this->mTabVector.push_back(inContainer);
        bool oldNeed = this->NeedScrollButtons();
        this->mBottomTabBar->AddTab(inTab);
        if (inContainer != 0) {
            H(me, Op::ChildSetVisible, inContainer, 0);
            H(me, Op::ChildSetFlags, inContainer, 0x2000, 1);
            Ct(me, C::AddChild, inContainer);
        }
        if (this->NeedScrollButtons() != oldNeed) H(me, Op::CtVirt, (intptr_t)V::SizeChanged);
    }
    void RemoveTab(int theId)
    {
        if (theId < 0 || theId >= (int)this->mTabVector.size()) return;
        int aCurTabId = this->mBottomTabBar->GetCurTabId();
        H(me, Op::CtChild, kTopBar, (intptr_t)V::RemoveChild, this->mBottomTabBar->mCurTab);
        this->mCurTab = 0;
        H(me, Op::CtVirt, (intptr_t)V::RemoveChild, this->mTabVector[theId]);
        this->mTabVector.erase(this->mTabVector.begin() + theId);
        this->mBottomTabBar->RemoveTab(theId);
        this->mBottomTabBar->SetCurTab(ee_won_gui_tab_after_remove(theId, aCurTabId), true);
    }
    intptr_t Field(int f)
    {
        switch ((EeWonTabField)f) {
        case EeWonTabField::CurTabId: return this->mBottomTabBar->GetCurTabId();
        case EeWonTabField::TabBarHeight: return this->mTabBarHeight;
        case EeWonTabField::TabOverlap: return this->mBottomTabBar->mOverlap;
        case EeWonTabField::TabBarOverlap: return this->mTabBarOverlap;
        case EeWonTabField::BottomTabTopOffset: return this->mBottomTabTopOffset;
        case EeWonTabField::BottomTabLeftOffset: return this->mBottomTabLeftOffset;
        case EeWonTabField::UserHasInteracted: return this->mUserHasInteracted;
        }
        return 0;
    }
    void SetField(int f, intptr_t v)
    {
        switch ((EeWonTabField)f) {
        case EeWonTabField::TabBarHeight: this->mTabBarHeight = (int)v; break;
        case EeWonTabField::TabOverlap: this->mBottomTabBar->mOverlap = (int)v; break;
        case EeWonTabField::TabBarOverlap: this->mTabBarOverlap = (int)v; break;
        case EeWonTabField::BottomTabTopOffset: this->mBottomTabTopOffset = (int)v; break;
        case EeWonTabField::BottomTabLeftOffset: this->mBottomTabLeftOffset = (int)v; break;
        case EeWonTabField::UserHasInteracted: this->mUserHasInteracted = v != 0; break;
        default: break;
        }
    }
    intptr_t Call(T call, intptr_t a, intptr_t b)
    {
        switch (call) {
        case T::SizeChanged: Virtual08(); return 0;
        case T::DesiredSize: GetDesiredSize(*(int*)a, *(int*)b); return 0;
        case T::KeyDown: return KeyDown((int)a);
        case T::AddedToParent: AddedToParent(); return 0;
        case T::HandleComponentEvent: HandleComponentEvent(a); return 0;
        case T::TabFocus: return TabFocus(a != 0, b != 0);
        case T::CopyAttributes: return 0; // (another control: the Rust unit test)
        case T::SetTabBarOnRight: this->mTabBarOnRight = a != 0; return 0;
        case T::SetArrowsOnLeft: this->mArrowsOnLeft = a != 0; return 0;
        case T::SetTabArrows: SetTabArrows(a, b); return 0;
        case T::SetCurTab: this->mBottomTabBar->SetCurTab((int)a, b != 0); return 0;
        case T::SetCurTabByName: this->mBottomTabBar->SetCurTabByName(a, b != 0); return 0;
        case T::GetTabIdForContainer: return GetTabIdForContainer(a);
        case T::EnableTab: this->mBottomTabBar->EnableTab((int)a, b != 0); return 0;
        case T::AddTab: AddTab(a, b); return 0;
        case T::RemoveTab: RemoveTab((int)a); return 0;
        case T::GetField: return Field((int)a);
        case T::SetField: SetField((int)a, b); return 0;
        }
        return 0;
    }
};

void TabBar::SetCurTab(int inId, bool inRequestFocus)
{
    if (inId < 0 || inId >= (int)this->mTabVector.size()) return;
    int oldId = this->GetCurTabId();
    if (oldId == inId) {
        if (inRequestFocus && this->mCurTab != 0) H(kBar, Op::CtChild, this->mCurTab, (intptr_t)V::RequestFocus);
        return;
    }
    intptr_t tab = this->mTabVector[inId];
    if (IsDisabled(kBar, tab)) return;
    if (this->mCurTab != 0) {
        H(kBar, Op::ButtonSetCheck, this->mCurTab, 0);
        if (H(kBar, Op::CtParentOf, this->mCurTab) != kBar) {
            Ct(kBar, C::AddChildEx, this->mCurTab, int(this->mTabVector.size()) - oldId - 1);
            this->PositionTab(this->mCurTab);
        }
    }
    this->mCurTab = tab;
    H(kBar, Op::ButtonSetCheck, tab, 1);
    if (this->mTabCtrl != NULL) {
        this->mTabCtrl->mUserHasInteracted = true;
        this->mTabCtrl->NotifyTabChange(inRequestFocus);
    }
}
}

static orig::TabCtrl* sOrig;
static orig::TabBar* sOrigBar;
static EeWonTabCtrl* sRust;
static EeWonTabBar* sRustBar;

static intptr_t TabCall(T call, intptr_t a, intptr_t b)
{
    if (sRustSide) return ee_won_tab_call(sRust, call, a, b);
    return sOrig->Call(call, a, b);
}
static bool TabAlive() { return sRustSide ? sRust != NULL : sOrig != NULL; }

// the control's overrides of the container's virtuals
static T TabOverride(C call)
{
    switch (call) {
    case C::SizeChanged: return T::SizeChanged;
    case C::DesiredSize: return T::DesiredSize;
    case C::KeyDown: return T::KeyDown;
    case C::AddedToParent: return T::AddedToParent;
    case C::TabFocus: return T::TabFocus;
    default: return (T)0;
    }
}

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    if (ct == kCtrl && TabAlive() && (int)TabOverride(call) != 0) return TabCall(TabOverride(call), a, b);
    return ee_won_ct_call(sCt[ct], call, a, b, c);
}
static intptr_t CtObjectOf(intptr_t ct) { return (intptr_t)sCt[ct]; }

static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)self; (void)c; (void)d;
    switch (op) {
    case Op::ChildRight: *r = sW[a].x + sW[a].w; return true;
    case Op::EventType: *r = sEventType; return true;
    case Op::EventControlId: *r = sEventControl; return true;
    case Op::EventComponent: *r = sEventComp; return true;
    case Op::TabTextIs: *r = a % 3 == b; return true;
    default: return false;
    }
}

static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)c; (void)d;
    if (op == Op::FireEvent) {
        // the original fires ComponentEvent_TabCtrlTabSelected (the enum); the Rust side's event becomes a type
        // through ee_won_gui_event_type, the call TabCtrl.cpp's host makes
        long long type = a == -1 ? (long long)WONAPI::ComponentEvent_TabCtrlTabSelected : ee_won_gui_event_type((EeWonGuiEvent)a);
        Line("op %lld fire %lld %lld @%lld", (long long)op, type, b, self);
        return true;
    }
    if (op == Op::ChildSetPosSize) {
        const EeWonRect4* q = (const EeWonRect4*)b;
        Line("op %lld %lld posrect %lld %lld %lld %lld", (long long)op, a, q->x, q->y, q->w, q->h);
        sTrace.back() += " @" + std::to_string((long long)self);
        return true;
    }
    return false;
}

// Component's SetSize, SetEnabled and SetVisible in the world
static void WorldSetSize(intptr_t a, int w, int h)
{
    int ow, oh;
    if (!ee_won_gui_set_size(sW[a].w, sW[a].h, w, h, &ow, &oh)) return;
    Set((int)a, 6, oh);
    Set((int)a, 5, ow);
    VCall(a, V::SizeChanged);
}
static void WorldStateChanged(intptr_t a)
{
    if (sW[a].parent != 0) Do(sW[a].parent, C::ChildStateChanged, a);
    VCall(a, V::Invalidate);
}

static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)self; (void)d; (void)r;
    switch (op) {
    case Op::ChildSetPos: Set((int)a, 3, b); Set((int)a, 4, c); break;
    case Op::ChildSetSize: WorldSetSize(a, (int)b, (int)c); break;
    case Op::ChildSetPosSize: {
        const EeWonRect4* q = (const EeWonRect4*)b;
        Set((int)a, 3, q->x); Set((int)a, 4, q->y);
        WorldSetSize(a, q->w, q->h);
        break;
    }
    case Op::ChildSetEnabled:
        if ((b != 0) == !(sW[a].bits & 0x80)) break;
        Set((int)a, 0, b ? (sW[a].bits & ~0x80u) : (sW[a].bits | 0x80));
        WorldStateChanged(a);
        break;
    case Op::ChildSetVisible:
        if ((b != 0) == !(sW[a].bits & 1)) break;
        Set((int)a, 0, b ? (sW[a].bits & ~1u) : (sW[a].bits | 1));
        Set((int)a, 1, 0);
        WorldStateChanged(a);
        VCall(a, V::NotifyRootVisibility, b);
        break;
    case Op::ChildSetControlId: Set((int)a, 7, b); break;
    case Op::TabAddLayoutPoint: Set((int)a, 8, self); break;
    default: break; // the buttons' flags and checks, the event to the component: traced only
    }
}

// ---- the driver

static void TabTop(T call, intptr_t a = 0, intptr_t b = 0)
{
    Line("tab %lld %lld %lld", (long long)call, a, b);
    if (call == T::DesiredSize) {
        int w = -1, h = -1;
        TabCall(call, (intptr_t)&w, (intptr_t)&h);
        Line("-> %lld %lld", w, h);
        return;
    }
    long long r = TabCall(call, a, b);
    Line("-> %lld 0", r);
}

static void BarEvent(intptr_t a)
{
    Line("bar %lld %lld", (long long)EeWonTabBarCall::HandleComponentEvent, a);
    long long r = 0;
    if (sRustSide) r = ee_won_tabbar_call(sRustBar, EeWonTabBarCall::HandleComponentEvent, a);
    else sOrigBar->HandleComponentEvent(a);
    Line("-> %lld 0", r);
}

// the driver's own view of the tabs (the same on both sides: its own calls only)
static std::vector<intptr_t> sDrvButtons, sDrvPages;

static void SetEvent(intptr_t type, intptr_t control, intptr_t comp)
{
    sEventType = type; sEventControl = control; sEventComp = comp;
    Line("env 10 %lld", sEventType);
    Line("env 12 %lld %lld", sEventControl, sEventComp);
}

static intptr_t FreeOf(intptr_t first, const std::vector<intptr_t>& used)
{
    std::vector<intptr_t> free;
    for (intptr_t i = first; i < first + 5; i++) {
        bool in = false;
        for (size_t k = 0; k < used.size(); k++) in = in || used[k] == i;
        if (!in) free.push_back(i);
    }
    return free.empty() ? 0 : free[DR((unsigned)free.size())];
}

static void RandomTabCall()
{
    int n = (int)sDrvButtons.size();
    switch (DR(26)) {
    case 0: case 1: case 2: {
        intptr_t b = FreeOf(kButtons, sDrvButtons), p = FreeOf(kPages, sDrvPages);
        if (b == 0 || p == 0) break;
        sDrvButtons.push_back(b); sDrvPages.push_back(p);
        TabTop(T::AddTab, b, p);
        break;
    }
    case 3: {
        int id = (int)DR((unsigned)n + 2) - 1;
        if (id >= 0 && id < n) { sDrvButtons.erase(sDrvButtons.begin() + id); sDrvPages.erase(sDrvPages.begin() + id); }
        TabTop(T::RemoveTab, id);
        break;
    }
    case 4: case 5: TabTop(T::SetCurTab, (int)DR((unsigned)n + 2) - 1, DR(2)); break;
    case 6: TabTop(T::SetCurTabByName, DR(4), DR(2)); break;
    case 7: case 8: TabTop(T::EnableTab, (int)DR((unsigned)n + 2) - 1, DR(3) != 0); break;
    case 9: TabTop(T::GetTabIdForContainer, kPages + DR(6)); break;
    case 10: TabTop(T::SetTabArrows, DR(4) ? kLeft : kRight, DR(4) ? kRight : kLeft); break;
    case 11: TabTop(T::SetTabBarOnRight, DR(2)); break;
    case 12: TabTop(T::SetArrowsOnLeft, DR(2)); break;
    case 13: case 14: TabTop(T::SizeChanged); break;
    case 15: TabTop(T::DesiredSize); break;
    case 16: { static const int kKeys[4] = {37, 39, 9, 65}; TabTop(T::KeyDown, kKeys[DR(4)]); break; }
    case 17: TabTop(T::TabFocus, DR(2), DR(2)); break;
    case 18: {
        static const intptr_t kComps[4] = {kLeft, kRight, kButtons, 0};
        intptr_t comp = kComps[DR(4)];
        if (comp == kButtons) comp = kButtons + DR(5);
        SetEvent(DR(4) ? 15 : 3, (int)DR(6) - 1, comp);
        TabTop(T::HandleComponentEvent, kEvent);
        break;
    }
    case 19: {
        // a tab button's press: its own control id, or another's
        intptr_t comp = n > 0 && DR(3) ? sDrvButtons[DR((unsigned)n)] : kButtons + DR(5);
        SetEvent(DR(5) ? 15 : 21, DR(4) ? sW[comp].controlId : (int)DR(6) - 1, comp);
        BarEvent(kEvent);
        break;
    }
    case 20: TabTop(T::GetField, 1 + DR(7)); break;
    case 21: {
        static const int kVals[5] = {0, 1, 4, 20, -3};
        TabTop(T::SetField, 2 + DR(6), kVals[DR(5)]);
        break;
    }
    case 22: TopCall(kParent, DR(2) ? C::AddChild : C::RemoveChild, kCtrl, 0, 0); break;
    case 23: TopCall(kCtrl, C::MouseMove, (int)DR(200) - 20, (int)DR(200) - 20, 0); break;
    default: {
        // the focus on a tab button or a page
        intptr_t c = DR(2) ? kButtons + DR(5) : kPages + DR(5);
        Set((int)c, 0, sW[c].bits ^ 0x10);
        break;
    }
    }
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sCursorRefs.clear();
    sRustSide = inRust;
    sDriverSeed = inSeq * 40503u + 13u;
    sAnswerSeed = inSeq * 2654435761u + 7u;
    InitWorld();
    sEventType = sEventControl = sEventComp = 0;
    sDrvButtons.clear(); sDrvPages.clear();
    for (int ct = 1; ct <= kCt; ct++) {
        Line("new %lld", ct);
        sCt[ct] = ee_won_ct_new((void*)(intptr_t)ct, Host, false);
    }
    Line("tabctrl");
    if (inRust) sRust = ee_won_tab_new((void*)kCtrl, Host, sCt[kCtrl]);
    else sOrig = new orig::TabCtrl;
    Line("tabbar");
    if (inRust) sRustBar = ee_won_tabbar_new((void*)kBar, Host, sCt[kBar], sRust);
    else sOrigBar = new orig::TabBar(sOrig);
    Line("tabinit");
    if (inRust) {
        EeWonTabParts aParts = {sRustBar, kBar, kTopBar, kBottomCt, kTopCt, kLayout};
        ee_won_tab_init(sRust, &aParts);
    } else sOrig->Init(sOrigBar);
    if (DR(3)) TabTop(T::SetTabArrows, kLeft, kRight);
    if (DR(4)) TopCall(kParent, C::AddChild, kCtrl, 0, 0);
    Env(0, DR(2) ? kParent : 1);
    for (int i = 0; i < inCalls; i++) {
        if (DR(3) == 0) RandomChange();
        RandomTabCall();
    }
    Line("free-tab");
    if (inRust) { ee_won_tab_free(sRust); sRust = NULL; }
    else { sOrig->Destroy(); delete sOrig; sOrig = NULL; }
    Line("free-bar");
    if (inRust) { ee_won_tabbar_free(sRustBar); sRustBar = NULL; }
    else { sOrigBar->Destroy(); delete sOrigBar; sOrigBar = NULL; }
    for (int ct = kCt; ct >= 1; ct--) {
        Line("free %lld", ct);
        ee_won_ct_free(sCt[ct]);
        sCt[ct] = NULL;
    }
    for (int id = 1; id <= kN; id++) CHECK(sW[id].refs == 0);
    for (std::map<intptr_t, int>::iterator i = sCursorRefs.begin(); i != sCursorRefs.end(); ++i) CHECK(i->second == 0);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the event the control fires is the original enum's type
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::TabCtrlTabSelected) == WONAPI::ComponentEvent_TabCtrlTabSelected);
    const int kSequences = 20000;
    const int kCallsPer = aPrint ? 16 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCallsPer);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCallsPer);
        aLines += a.size();
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) {
                        printf("  line %u\n", (unsigned)i);
                        for (size_t j = i > 6 ? i - 6 : 0; j < i; j++) printf("  both: %s\n", a[j].c_str());
                        printf("  orig: %s\n  rust: %s\n", x, y);
                        break;
                    }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 4, "tab ") == 0) {
                std::string key = a[i].substr(0, a[i].find(' ', 4));
                if (aSeen.insert(key).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_tabobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
