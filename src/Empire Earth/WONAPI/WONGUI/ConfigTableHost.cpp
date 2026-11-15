// The resource configs' table reader's host (R6 WON round 39): the 45 pure-table config handlers are ee_won's
// cfgtab.rs programs; their reads, frame and setters are WONCfgTable.inl (shared with tests/won/won_cfgtab_test.cpp).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <exception>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyComboBoxConfig.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyMSConfig.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbySimpleConfig.h"
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLobbyTabCtrlConfig.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbyWONButtonConfig.h"
#include "ee/wonapi/WONLobbyWONFrameConfig.h"
#include "ee/wonapi/WONLobbyWONFrameValueConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLayoutConfigs.h"
#include "ee/wonapi/EELobbySound.h"
#include "rust/ee_won.h"
#define EE_CFG_LAYOUT_GLOBAL
namespace WONAPI {
#include "ee/wonapi/WONCfgTable.inl"
}
bool PercentLayoutConfig::Virtual24(const std::string& key) { return this->RunTable(WONAPI::kPercent,key); }
bool ColorSchemeConfig::Virtual24(const std::string& key) { return this->RunTable(WONAPI::kScheme,key); }

// What the handlers' units kept besides their instructions (ButtonConfig.cpp, ComboBoxConfig.cpp, MSComponentConfig.cpp,
// ScrollbarConfig.cpp, SplitterConfig.cpp, TabCtrlConfig.cpp, TextAreaConfig.cpp, ImageButtonConfig.cpp,
// WONButtonConfig.cpp, WONFrameConfig.cpp, WONFrameValueConfig.cpp, now deleted): the bindings, the predefined
// children and named values, ButtonConfig's FITTEXT.
namespace WONAPI {
// Each binds the config to the component it configures (SplitterConfig's 0x0078f960, ImageButtonConfig's 0x0078f8d0,
// MSArrowButtonConfig's 0x0078f3c0, TextAreaConfig's 0x0078f860, MSTabButtonConfig's 0x0078f7b0, WONFrame's 0x00796ba0)
#define EE_CFG_BIND(T,B,M,C) void T::SetComponent(Component* component) { B::SetComponent(component); this->M=static_cast<C*>(component); }
EE_CFG_BIND(SplitterConfig,ContainerConfig,mSplitter,Splitter)
EE_CFG_BIND(ImageButtonConfig,ComponentConfig,mImageButton,ImageButton)
EE_CFG_BIND(TextAreaConfig,ScrollAreaConfig,mTextArea,TextArea)
EE_CFG_BIND(MSArrowButtonConfig,ButtonConfig,mMSArrowButton,MSArrowButton)
EE_CFG_BIND(MS3DFrameConfig,ComponentConfig,mMS3DFrame,MS3DFrame)
EE_CFG_BIND(MSButtonConfig,ButtonConfig,mMSButton,MSButtonBase)
EE_CFG_BIND(MSTabButtonConfig,MSButtonConfig,mMSTabButton,MSTabButton)
EE_CFG_BIND(WONFrameComponentConfig,ComponentConfig,mFrameComponent,WONFrameComponent)
#undef EE_CFG_BIND
// 0x00796a20: the skinned buttons' configs set their pointer before the base's
void WONSimpleButtonConfig::SetComponent(Component* component) { this->mButton=static_cast<WONSimpleButton*>(component); ButtonConfig::SetComponent(component); }
void WONButtonConfig::SetComponent(Component* component) { this->mButton=static_cast<WONButton*>(component); ButtonConfig::SetComponent(component); }
// Out-of-line copy constructor (not identified separately in the original exe)
WONFrame::WONFrame(const WONFrame& from) { this->Copy(from); }
// FUNCTION: WON 0x00781d00
// Grows the button to fit the largest of a comma-separated list of texts, keeping its current text
void ButtonConfig::HandleFitText()
{
    int width=this->mButton->Width();
    int height=this->mButton->Height();
    GUIString oldText=this->mButton->GetText();
    for(;;)
    {
        GUIString text=this->ReadString(false);
        this->mButton->SetText(text,true);
        int desiredWidth=0,desiredHeight=0;
        this->mButton->GetDesiredSize(desiredWidth,desiredHeight);
        if(desiredWidth>width) width=desiredWidth;
        if(desiredHeight>height) height=desiredHeight;
        if(this->EndOfString()) break;
        this->EnsureComma();
    }
    this->mButton->SetText(oldText,true);
    this->mButton->SetSize(width,height);
}
// FUNCTION: WON 0x00782070
// The predefined LISTAREA and INPUTBOX children; other names go to the container configuration
bool ComboBoxConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) { config=ListAreaConfig::CfgFactory(); component=this->mComboBox->mListArea.get(); }
    else if(aKeyIdx==1) { config=InputBoxConfig::CfgFactory(); component=this->mComboBox->mEdit.get(); }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// FUNCTION: WON 0x007949c0
// The scrollbar's THUMB, UPARROW, DOWNARROW or SCROLLRECT with a new config for it; else the container's
bool ScrollbarConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) { config=new ComponentConfig; component=this->mScrollbar->GetThumb(); }
    else if(aKeyIdx==1) { config=new ButtonConfig; component=this->mScrollbar->GetUpArrow(); }
    else if(aKeyIdx==2) { config=new ButtonConfig; component=this->mScrollbar->GetDownArrow(); }
    else if(aKeyIdx==3) { config=new ButtonConfig; component=this->mScrollbar->GetScrollRect(); }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// FUNCTION: WON 0x007958e0
// A plain configuration and the tab control's page layout point for CONTAINERLAYOUT; else the container's
bool TabCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TABCTRLCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) { config=new ComponentConfig; component=this->mTabCtrl->mContainerLayout; }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// FUNCTION: WON 0x00798590
// The frame paddings LEFTPAD, RIGHTPAD, TOPPAD, BOTTOMPAD, HORZPAD and VERTPAD; else the base config's
bool WONFrameComponentConfig::GetNamedValue(const std::string& key,int& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_GETNAMEDVALUE_KEY,key.data(),key.size());
    WONFrameComponent* f=this->mFrameComponent;
    if(aKeyIdx==0) value=f->GetFrameLeft();
    else if(aKeyIdx==1) value=f->GetFrameRight();
    else if(aKeyIdx==2) value=f->GetFrameTop();
    else if(aKeyIdx==3) value=f->GetFrameBottom();
    else if(aKeyIdx==4) value=f->GetFrameRight()+f->GetFrameLeft();
    else if(aKeyIdx==5) value=f->GetFrameBottom()+f->GetFrameTop();
    else return ComponentConfig::GetNamedValue(key,value);
    return true;
}
}
