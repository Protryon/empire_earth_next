// Windows-style tab control: frame, background and scroll arrows
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
#include "ee/wonapi/WONLobbyMSPainters.h"
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x007010e0
// Creates an opaque tab control with a dialog background and a 3D frame laid out over its content area,
// and two 15 x 15 arrow buttons for scrolling the tabs
MSTabCtrl::MSTabCtrl()
{
    this->mIsTransparent=false;
    SmartPtr<MS3DFrame> frame=new MS3DFrame(1,true);
    SmartPtr<MSDlgBack> back=new MSDlgBack;
    this->Fn2F6140(back.get(),0x5003,this->mContainerLayout.get(),0,0,0,0,0);
    this->Fn2F6140(frame.get(),0x5003,this->mContainerLayout.get(),0,0,0,0,0);
    this->AddChild(back.get());
    this->AddChild(frame.get());
    SmartPtr<Button> left=new MSArrowButton(Direction_Left);
    SmartPtr<Button> right=new MSArrowButton(Direction_Right);
    left->SetSize(15,15);
    right->SetSize(15,15);
    this->SetTabArrows(left.get(),right.get());
}
// FUNCTION: WON 0x00701660
// Sets whether the tab control paints its background
void MSTabCtrl::SetTransparent(bool transparent)
{
    this->mIsTransparent=transparent;
}
// FUNCTION: WON 0x00701670
// Forwards to the base tab control when the control is added to a parent
void MSTabCtrl::AddedToParent()
{
    TabCtrl::AddedToParent();
}
// FUNCTION: WON 0x00701680
// Adds a tab with a new Windows-style tab button showing the text, switching to the container
void MSTabCtrl::AddStandardTab(const GUIString& text,Container* container)
{
    this->AddTab(new MSTabButton(text),container);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fd3e0
// (public: virtual void * __thiscall WONAPI::MSDlgBack::`scalar deleting destructor'(unsigned int)    [??_GMSDlgBack@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00701320
// (public: virtual void * __thiscall WONAPI::MSTabCtrl::`scalar deleting destructor'(unsigned int)    [??_GMSTabCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00706860
// (public: virtual __thiscall WONAPI::MSDlgBack::~MSDlgBack(void)    [??1MSDlgBack@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00701340
// (public: virtual __thiscall WONAPI::TabCtrl::~TabCtrl(void))
// TEMPLATE: WON 0x007014d0
// (public: virtual __thiscall WONAPI::MSTabCtrl::~MSTabCtrl(void))
