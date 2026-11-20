// The original config classes of each widget type, as they were before R6 WONLobby round 26 made them kinds of one
// ComponentConfig (ee_won_gui cfgkind.rs): their declarations (the deleted headers' and WONLobbyGUI.h's ButtonConfig,
// ContainerConfig and DialogConfig: the members their bindings and hooks use) and those hooks' statements, copied from
// bcea46987 (ConfigComponentBindings.cpp, ConfigBaseComponentBindings.cpp, SimpleComponentConfig.cpp,
// ConfigTableHost.cpp, MSEtchedPredefinedComponent.cpp, MSListPredefinedComponents.cpp, MSPredefinedComponents.cpp,
// BrowserComponentConfig.cpp). Their Virtual24 chains are won_cfgtab_test's (here every class keeps the world's
// ComponentConfig::Virtual24). Included in won_compcfg_test.cpp's namespace Orig after its ComponentConfig.
class ButtonConfig : public ComponentConfig { public: virtual void SetComponent(Component*); Button* mButton; };
class ContainerConfig : public ComponentConfig { public: virtual void SetComponent(Component*); Container* mContainer; };
class DialogConfig : public ContainerConfig { public: virtual void SetComponent(Component*); Dialog* mDialog; };
class ScrollAreaConfig : public ContainerConfig { public: virtual void SetComponent(Component*); ScrollArea* mScrollArea; };
class ListAreaConfig : public ScrollAreaConfig { public: virtual void SetComponent(Component*); ListArea* mListArea; };
class MultiListAreaConfig : public ListAreaConfig { public: virtual void SetComponent(Component*); MultiListArea* mMultiListArea; };
class MultiListHeaderConfig : public ContainerConfig {
public:
    virtual void SetComponent(Component* component) { this->mHeader=static_cast<MultiListHeader*>(component); ContainerConfig::SetComponent(component); } //39afb0
    MultiListHeader* mHeader;
};
class InputBoxConfig : public ComponentConfig { public: virtual void SetComponent(Component*); InputBox* mInputBox; };
class LabelConfig : public ComponentConfig { public: virtual void SetComponent(Component*); Label* mLabel; };
class ScrollerConfig : public ContainerConfig { public: virtual void SetComponent(Component*); Scroller* mScroller; };
class ScrollbarScrollerConfig : public ScrollerConfig { public: virtual void SetComponent(Component*); ScrollbarScroller* mScrollbarScroller; };
class ScrollbarConfig : public ContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); Scrollbar* mScrollbar;
};
class TreeAreaConfig : public ListAreaConfig { public: virtual void SetComponent(Component*); TreeArea* mTreeArea; };
class ComboBoxConfig : public ContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); ComboBox* mComboBox;
};
class TabCtrlConfig : public ContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); TabCtrl* mTabCtrl;
};
class SplitterConfig : public ContainerConfig { public: virtual void SetComponent(Component*); Splitter* mSplitter; };
class TextAreaConfig : public ScrollAreaConfig { public: virtual void SetComponent(Component*); TextArea* mTextArea; };
class ImageButtonConfig : public ComponentConfig { public: virtual void SetComponent(Component*); ImageButton* mImageButton; };
class RectangleComponentConfig : public ComponentConfig { public: virtual void SetComponent(Component*); RectangleComponent* mRectangleComponent; };
class RectangleComponent3DConfig : public ComponentConfig { public: virtual void SetComponent(Component*); RectangleComponent3D* mRectangleComponent3D; };
class ImageComponentConfig : public ComponentConfig { public: virtual void SetComponent(Component*); ImageComponent* mImageComponent; };
class ScreenContainerConfig : public ContainerConfig { public: virtual void SetComponent(Component*); ScreenContainer* mScreenContainer; };
class BrowserComponentConfig : public ContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); BrowserComponent* mBrowserComponent;
};
class MSButtonConfig : public ButtonConfig { public: virtual void SetComponent(Component*); MSButtonBase* mMSButton; };
class MSArrowButtonConfig : public ButtonConfig { public: virtual void SetComponent(Component*); Component* mMSArrowButton; };
class MSTabButtonConfig : public MSButtonConfig { public: virtual void SetComponent(Component*); Component* mMSTabButton; };
class MS3DFrameConfig : public ComponentConfig { public: virtual void SetComponent(Component*); Component* mMS3DFrame; };
class MSComboBoxConfig : public ComboBoxConfig { public: virtual void SetComponent(Component*); ComboBox* mMSComboBox; };
class MSScrollbarConfig : public ScrollbarConfig { public: virtual void SetComponent(Component*); Scrollbar* mMSScrollbar; };
class MSEtchedFrameGroupConfig : public ContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); Container* mFrame;
};
class MSTabCtrlConfig : public TabCtrlConfig { public: virtual void SetComponent(Component*); TabCtrl* mMSTabCtrl; };
class MSScrollContainerConfig : public ScrollbarScrollerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); ScrollbarScroller* mScrollContainer;
};
class MSHeaderControlConfig : public MultiListHeaderConfig { public: virtual void SetComponent(Component*); MultiListHeader* mMSHeader; };
class MSListCtrlConfig : public MSScrollContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); ScrollbarScroller* mListCtrl;
};
class WONSimpleButtonConfig : public ButtonConfig { public: virtual void SetComponent(Component*); Component* mButton; };
class WONButtonConfig : public ButtonConfig { public: virtual void SetComponent(Component*); WONButton* mButton; };
class WONScrollbarConfig : public ScrollbarConfig { };
class WONScrollContainerConfig : public ScrollbarScrollerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component* component) { ScrollbarScrollerConfig::SetComponent(component); this->mScrollContainer=static_cast<ScrollbarScroller*>(component); } //396c80
    ScrollbarScroller* mScrollContainer;
};
class WONHeaderControlConfig : public MultiListHeaderConfig { };
class WONListCtrlConfig : public WONScrollContainerConfig {
public:
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&);
    virtual void SetComponent(Component*); ScrollbarScroller* mListCtrl;
};
class WONComboBoxConfig : public ComboBoxConfig {
public:
    virtual void SetComponent(Component* component) { this->mComboBox=component; ComboBoxConfig::SetComponent(component); }
    Component* mComboBox;
};
class WONTabCtrlConfig : public TabCtrlConfig {
public:
    virtual void SetComponent(Component* component) { TabCtrlConfig::SetComponent(component); this->mTabCtrl=component; }
    Component* mTabCtrl;
};
class WONFrameComponentConfig : public ComponentConfig {
public:
    virtual bool GetNamedValue(const std::string&,int&);
    virtual void SetComponent(Component*); Component* mFrameComponent;
};
class WONSkinConfig : public ComponentConfig { public: virtual bool PreParse(ConfigParser*); };
// ConfigComponentBindings.cpp
void ButtonConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mButton=static_cast<Button*>(component);
}
void ContainerConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mContainer=static_cast<Container*>(component);
}
void DialogConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mDialog=static_cast<Dialog*>(component);
}
void InputBoxConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mInputBox=static_cast<InputBox*>(component);
}
void LabelConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mLabel=static_cast<Label*>(component);
}
void ScrollAreaConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mScrollArea=static_cast<ScrollArea*>(component);
}
void ListAreaConfig::SetComponent(Component* component)
{
    ScrollAreaConfig::SetComponent(component);
    this->mListArea=static_cast<ListArea*>(component);
}
void MultiListAreaConfig::SetComponent(Component* component)
{
    ListAreaConfig::SetComponent(component);
    this->mMultiListArea=static_cast<MultiListArea*>(component);
}
void ScrollbarConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mScrollbar=static_cast<Scrollbar*>(component);
}
void TreeAreaConfig::SetComponent(Component* component)
{
    this->mTreeArea=static_cast<TreeArea*>(component);
    ListAreaConfig::SetComponent(component);
}
void ComboBoxConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mComboBox=static_cast<ComboBox*>(component);
}
void MSComboBoxConfig::SetComponent(Component* component)
{
    this->mMSComboBox=static_cast<ComboBox*>(component);
    ComboBoxConfig::SetComponent(component);
}
void MSScrollbarConfig::SetComponent(Component* component)
{
    ScrollbarConfig::SetComponent(component);
    this->mMSScrollbar=static_cast<Scrollbar*>(component);
}
void MSEtchedFrameGroupConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mFrame=static_cast<Container*>(component);
}
void MSTabCtrlConfig::SetComponent(Component* component)
{
    TabCtrlConfig::SetComponent(component);
    this->mMSTabCtrl=static_cast<TabCtrl*>(component);
}
void MSScrollContainerConfig::SetComponent(Component* component)
{
    ScrollbarScrollerConfig::SetComponent(component);
    this->mScrollContainer=static_cast<ScrollbarScroller*>(component);
}
void MSHeaderControlConfig::SetComponent(Component* component)
{
    this->mMSHeader=static_cast<MultiListHeader*>(component);
    MultiListHeaderConfig::SetComponent(component);
}
void MSListCtrlConfig::SetComponent(Component* component)
{
    this->mListCtrl=static_cast<ScrollbarScroller*>(component);
    MSScrollContainerConfig::SetComponent(component);
}
void WONListCtrlConfig::SetComponent(Component* component)
{
    WONScrollContainerConfig::SetComponent(component);
    this->mListCtrl=static_cast<ScrollbarScroller*>(component);
}
// ConfigBaseComponentBindings.cpp
void ScrollerConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mScroller=static_cast<Scroller*>(component);
}
void ScrollbarScrollerConfig::SetComponent(Component* component)
{
    ScrollerConfig::SetComponent(component);
    this->mScrollbarScroller=static_cast<ScrollbarScroller*>(component);
}
void TabCtrlConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mTabCtrl=static_cast<TabCtrl*>(component);
}
// SimpleComponentConfig.cpp
void RectangleComponentConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mRectangleComponent = static_cast<RectangleComponent*>(theComponent);
}
void RectangleComponent3DConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mRectangleComponent3D = static_cast<RectangleComponent3D*>(theComponent);
}
void ImageComponentConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mImageComponent = static_cast<ImageComponent*>(theComponent);
}
void ScreenContainerConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mScreenContainer = static_cast<ScreenContainer*>(theComponent);
}
// ConfigTableHost.cpp
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
void WONSimpleButtonConfig::SetComponent(Component* component) { this->mButton=component; ButtonConfig::SetComponent(component); }
void WONButtonConfig::SetComponent(Component* component) { this->mButton=static_cast<WONButton*>(component); ButtonConfig::SetComponent(component); }
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
// MSEtchedPredefinedComponent.cpp
bool MSEtchedFrameGroupConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="LABEL") {
        config=NewConfigOf<LabelConfig>();
        component=(Component*)WONShell::Op(this->mFrame,EeWonShellOp::GetLabel);
    } else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// MSListPredefinedComponents.cpp
bool MSListCtrlConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HEADER") {
        component=(Component*)WONShell::Op(this->mListCtrl,EeWonShellOp::GetHeader);
        config=new MSHeaderControlConfig;
    } else if(name=="MULTILISTAREA") {
        component=this->mListCtrl->GetScrollArea();
        config=new MultiListAreaConfig;
    } else return MSScrollContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// MSPredefinedComponents.cpp
bool MSScrollContainerConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="HORZSCROLLBAR") {
        config=new MSScrollbarConfig;
        component=this->mScrollContainer->GetScrollbar(false);
    } else if(name=="VERTSCROLLBAR") {
        config=new MSScrollbarConfig;
        component=this->mScrollContainer->GetScrollbar(true);
    } else return ScrollbarScrollerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
// BrowserComponentConfig.cpp
void BrowserComponentConfig::SetComponent(Component* component)
{
    ContainerConfig::SetComponent(component);
    this->mBrowserComponent=static_cast<BrowserComponent*>(component);
}
bool BrowserComponentConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0)
    {
        config=new ScrollAreaConfig;
        component=this->mBrowserComponent->GetHTMLView();
    }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
