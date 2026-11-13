// TabBar and TabCtrl (R6 WON round 27): the Rust objects' calls out (rust/crates/ee_won/src/tabobj.rs, EeWonGuiOp's
// tab ops; the container's fall back to Container.cpp's), the constructors and destructors. The original's
// TabCtrl.cpp (the tabs' places and selection, the bar's scroll and arrows, the page area, focus and keys) is the
// Rust objects; each op here is a run of its C++ statements with no decision between them.
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
#include "ee/wonapi/WONLobbyLayout.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static Component* Comp(intptr_t a) { return (Component*)a; }

extern "C" intptr_t ee_won_guicpp_tab_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Container* t = static_cast<Container*>((Component*)inComp);
    switch (op) {
    case Op::ChildRight: return Comp(a)->Right();
    case Op::ChildSetPos: Comp(a)->SetPos((int)b, (int)c); return 0;
    case Op::ChildSetSize: Comp(a)->SetSize((int)b, (int)c); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* r = (const EeWonRect4*)b;
        Comp(a)->SetPosSize(r->x, r->y, r->w, r->h);
        return 0;
    }
    case Op::ChildSetEnabled: Comp(a)->SetEnabled(b != 0); return 0;
    case Op::ChildSetVisible: Comp(a)->SetVisible(b != 0); return 0;
    case Op::ChildSetControlId: Comp(a)->mControlId = (int)b; return 0;
    case Op::ButtonSetFlags: static_cast<Button*>(Comp(a))->SetButtonFlags((int)b, c != 0); return 0;
    case Op::ButtonSetCheck: static_cast<Button*>(Comp(a))->SetCheck(b != 0, false); return 0;
    case Op::TabTextIs: return static_cast<Button*>(Comp(a))->GetText() == *(const GUIString*)b ? 1 : 0;
    case Op::TabAddLayoutPoint: t->AddLayoutPoint(static_cast<LayoutPoint*>(Comp(a))); return 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventControlId: return ((ComponentEvent*)a)->GetControlId();
    case Op::EventComponent: return (intptr_t)((ComponentEvent*)a)->mComponent;
    case Op::ComponentHandleEvent: t->Component::HandleComponentEvent((ComponentEvent*)a); return 0;
    // the type is Rust's map, which the golden checks against the enum
    case Op::FireEvent: t->Fn2F5380(ee_won_gui_event_type((EeWonGuiEvent)a), b != 0); return 0;
    default: return ContainerHostCall(t, op, a, b, c, d);
    }
}

namespace WONAPI {
// Creates a tab bar for its tab control (the Rust bar: no tab, none selected, a 4-pixel overlap)
TabBar::TabBar(TabCtrl* inCtrl)
{
    this->mBar=ee_won_tabbar_new(static_cast<Component*>(this),ee_won_guicpp_tab_call,this->mCt,
                                 inCtrl!=NULL ? inCtrl->mTab : NULL);
}
// Destroys the tab bar: the Rust bar gives the buttons back
TabBar::~TabBar() { ee_won_tabbar_free(this->mBar); }
// FUNCTION: WON 0x00708670
// Creates a tab control with 5-pixel padding and no tabs: makes its tab bars, their containers and the page area's
// layout point, which the Rust control sets up and adds
TabCtrl::TabCtrl()
{
    this->mTab=ee_won_tab_new(static_cast<Component*>(this),ee_won_guicpp_tab_call,this->mCt);
    this->mBottomTabBarContainer=new Container;
    this->mTopTabBarContainer=new Container;
    this->mBottomTabBar=new TabBar(this);
    this->mTopTabBar=new Container;
    this->mContainerLayout=new LayoutPoint;
    EeWonTabParts aParts;
    aParts.bar=this->mBottomTabBar->mBar;
    aParts.bottom_bar=(intptr_t)static_cast<Component*>(this->mBottomTabBar.get());
    aParts.top_bar=(intptr_t)static_cast<Component*>(this->mTopTabBar.get());
    aParts.bottom_bar_container=(intptr_t)static_cast<Component*>(this->mBottomTabBarContainer.get());
    aParts.top_bar_container=(intptr_t)static_cast<Component*>(this->mTopTabBarContainer.get());
    aParts.layout=(intptr_t)static_cast<Component*>(this->mContainerLayout.get());
    ee_won_tab_init(this->mTab,&aParts);
}
// Destroys the tab control: the Rust control gives the arrows back (its members go after)
TabCtrl::~TabCtrl() { ee_won_tab_free(this->mTab); }
}
