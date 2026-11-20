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
#include "ee/wonapi/WONLobbyLayout.h"
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
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/won_gui/guiconf.h"
#include "ee/wonapi/WONGUIShell.h"
extern bool gShowParseTime; // ComponentConfigHost.cpp's
extern int gLinesParsed;
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
EE_CFG_BIND(MSArrowButtonConfig,ButtonConfig,mMSArrowButton,Component)
EE_CFG_BIND(MS3DFrameConfig,ComponentConfig,mMS3DFrame,Component)
EE_CFG_BIND(MSButtonConfig,ButtonConfig,mMSButton,MSButtonBase)
EE_CFG_BIND(MSTabButtonConfig,MSButtonConfig,mMSTabButton,Component)
EE_CFG_BIND(WONFrameComponentConfig,ComponentConfig,mFrameComponent,Component)
#undef EE_CFG_BIND
// 0x00796a20: the skinned buttons' configs set their pointer before the base's
void WONSimpleButtonConfig::SetComponent(Component* component) { this->mButton=component; ButtonConfig::SetComponent(component); }
void WONButtonConfig::SetComponent(Component* component) { this->mButton=static_cast<WONButton*>(component); ButtonConfig::SetComponent(component); }
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
// The predefined LISTAREA and INPUTBOX children; other names go to the container configuration
bool ComboBoxConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) { config=NewConfigOf<ListAreaConfig>(); component=this->mComboBox->mListArea.get(); }
    else if(aKeyIdx==1) { config=NewConfigOf<InputBoxConfig>(); component=this->mComboBox->mEdit.get(); }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
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
// A plain configuration and the tab control's page layout point for CONTAINERLAYOUT; else the container's
bool TabCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TABCTRLCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) { config=new ComponentConfig; component=this->mTabCtrl->mContainerLayout; }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// The frame paddings LEFTPAD, RIGHTPAD, TOPPAD, BOTTOMPAD, HORZPAD and VERTPAD; else the base config's
bool WONFrameComponentConfig::GetNamedValue(const std::string& key,int& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_GETNAMEDVALUE_KEY,key.data(),key.size());
    Component* f=this->mFrameComponent;
    auto B=[f](int side) { return (int)WONShell::Op(f,EeWonShellOp::GetFrameBorder,side); };
    if(aKeyIdx==0) value=B(0);
    else if(aKeyIdx==1) value=B(2);
    else if(aKeyIdx==2) value=B(1);
    else if(aKeyIdx==3) value=B(3);
    else if(aKeyIdx==4) value=B(2)+B(0);
    else if(aKeyIdx==5) value=B(3)+B(1);
    else return ComponentConfig::GetNamedValue(key,value);
    return true;
}
// The scroll container's HORZSCROLLBAR or VERTSCROLLBAR with a new WON scrollbar config; else the base config's
bool WONScrollContainerConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HORZSCROLLBAR") {
        config=new WONScrollbarConfig;
        component=this->mScrollContainer->GetScrollbar(false);
    } else if(name=="VERTSCROLLBAR") {
        config=new WONScrollbarConfig;
        component=this->mScrollContainer->GetScrollbar(true);
    } else return ScrollbarScrollerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// The list's HEADER or MULTILISTAREA with a new config for it; else the scroll container config's
bool WONListCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HEADER") {
        component=(Component*)WONShell::Op(this->mListCtrl,EeWonShellOp::GetHeader);
        config=new WONHeaderControlConfig;
    } else if(name=="MULTILISTAREA") {
        component=static_cast<MultiListArea*>(this->mListCtrl->GetScrollArea());
        config=new MultiListAreaConfig;
    } else return WONScrollContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// The base pre-parse, then the game's skin (the Windows look) when there is none (skinobj.rs)
bool WONSkinConfig::PreParse(ConfigParser* parser)
{
    if(!ComponentConfig::PreParse(parser))
        return false;
    ee_won_skin_ensure(ee_won_guicpp_shell_call);
    return true;
}
// A sound read, then the click sound of the skin's template buttons and every named button type
void WONSkinConfig::HandleButtonClickSound()
{
    SmartPtr<Sound> sound=this->ReadSound();
    ee_won_skin_click_sound(WONSkinGlobal(),sound.get());
}
}
