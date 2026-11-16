// The 49 table-shaped config handlers and their helpers as they were before the port (R6 WON round 39), copied
// verbatim from 306fae35 by build/r39/gen_orig.py; tests/won/won_cfgtab_test.cpp compiles them over its stand-ins.

// MSComponentConfig.cpp
bool MSButtonConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        this->mMSButton->SetFont(this->ReadFont());
    } else if(aKeyIdx==1) {
        int color=this->ReadColor();
        this->mMSButton->SetTextColor(color);
    } else if(aKeyIdx==2) {
        VertAlign align=this->ReadVertAlign();
        this->mMSButton->SetVertAlign(align);
    } else if(aKeyIdx==3) {
        HorzAlign align=this->ReadHorzAlign();
        this->mMSButton->SetHorzAlign(align);
    } else if(aKeyIdx==4) {
        this->mMSButton->SetTransparent(this->ReadBool());
    } else if(aKeyIdx==5) {
        this->mMSButton->SetAdjustHeightToFitText(this->ReadBool());
    } else if(aKeyIdx==6) {
        this->mMSButton->SetWrap(this->ReadBool());
    } else if(aKeyIdx==7) {
        int left=this->ReadInt(); this->EnsureComma();
        int right=this->ReadInt();
        this->mMSButton->IncrementHorzTextPad(left,right);
    } else if(aKeyIdx==8) {
        int top=this->ReadInt(); this->EnsureComma();
        int bottom=this->ReadInt();
        this->mMSButton->IncrementVertTextPad(top,bottom);
    } else return ButtonConfig::Virtual24(key);
    return true;
}
// MSComponentConfig.cpp
bool MSArrowButtonConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        GUIString direction=this->ReadString(false);
        const int aNameIdx_direction=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_MSARROWBUTTONCONFIG_VIRTUAL24_DIRECTION,direction.mData->mText,direction.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_direction==0) this->mMSArrowButton->SetDirection(Direction_Up);
        else if(aNameIdx_direction==1) this->mMSArrowButton->SetDirection(Direction_Down);
        else if(aNameIdx_direction==2) this->mMSArrowButton->SetDirection(Direction_Left);
        else if(aNameIdx_direction==3) this->mMSArrowButton->SetDirection(Direction_Right);
    } else return ButtonConfig::Virtual24(key);
    return true;
}
// MSComponentConfig.cpp
bool MSTabButtonConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0)
        this->mMSTabButton->SetLowerAmount(this->ReadInt());
    else return MSButtonConfig::Virtual24(key);
    return true;
}
// MSComponentConfig.cpp
bool MS3DFrameConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        int type=this->ReadInt();
        this->mMS3DFrame->SetFrameType(type);
    } else if(aKeyIdx==1) {
        bool up=this->ReadBool();
        this->mMS3DFrame->SetIsUp(up);
    } else return ComponentConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSComboBoxConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { bool on=this->ReadBool(); this->mMSComboBox->SetDrawFrame(on); }
    else return ComboBoxConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSScrollbarConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mMSScrollbar->SetTransparent(this->ReadBool());
    else return ScrollbarConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSEtchedFrameGroupConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mFrame->SetLabelText(this->ReadString(false));
    else if(aKeyIdx==1) this->mFrame->AddContainedComponent(this->ReadComponent());
    else return ContainerConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSTabCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mMSTabCtrl->SetTransparent(this->ReadBool());
    else if(aKeyIdx==1) {
        GUIString text=this->ReadString(false); this->EnsureComma();
        Container* container=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mMSTabCtrl->AddStandardTab(text,container);
    } else return TabCtrlConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSScrollContainerConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mScrollContainer->SetDrawFrame(this->ReadBool());
    else if(aKeyIdx==1) this->mScrollContainer->SetDraw3DFrame(this->ReadBool());
    else if(aKeyIdx==2) { bool on=this->ReadBool(); this->mScrollContainer->SetTransparent(on); }
    else return ScrollbarScrollerConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSHeaderControlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        int column=this->ReadInt(); this->EnsureComma(); GUIString title=this->ReadString(false);
        this->mMSHeader->SetColumn(column,title);
    } else return MultiListHeaderConfig::Virtual24(key);
    return true;
}
// MSExtraConfig.cpp
bool MSListCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { int height=this->ReadInt(); this->mListCtrl->SetHeaderHeight(height); }
    else return MSScrollContainerConfig::Virtual24(key);
    return true;
}
// ComponentLayoutConfigs.cpp
bool PercentLayoutConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    int ref=-1,param=-1;
    if(aKeyIdx==0) ref=0;
    else if(aKeyIdx==1) ref=0;
    else if(aKeyIdx==2) ref=1;
    else if(aKeyIdx==3) ref=2;
    else if(aKeyIdx==4) ref=3;
    else if(aKeyIdx==5) param=0;
    else if(aKeyIdx==6) param=1;
    else if(aKeyIdx==7) param=2;
    else if(aKeyIdx==8) param=3;
    else if(aKeyIdx==9) param=4;
    else if(aKeyIdx==10) param=5;
    else if(aKeyIdx==11) param=6;
    else if(aKeyIdx==12) param=7;
    else return false;
    if(ref>=0) this->refs[ref]=this->ReadComponent();
    else if(param>=0) this->params[param]=this->ReadInt();
    return true;
}
// ComponentLayoutConfigs.cpp
bool ColorSchemeConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    int color=0;
    if(aKeyIdx==0) color=0;
    else if(aKeyIdx==1) color=1;
    else if(aKeyIdx==2) color=2;
    else if(aKeyIdx==3) color=3;
    else if(aKeyIdx==4) color=4;
    else if(aKeyIdx==5) color=5;
    else if(aKeyIdx==6) color=6;
    else if(aKeyIdx==7) color=7;
    else if(aKeyIdx==8) color=8;
    else if(aKeyIdx==9) color=9;
    else if(aKeyIdx==10) color=10;
    else if(aKeyIdx==11) color=11;
    else if(aKeyIdx==12) color=12;
    else if(aKeyIdx==13) color=13;
    else if(aKeyIdx==14) color=14;
    else if(aKeyIdx==15) color=15;
    else if(aKeyIdx==16) color=16;
    else return false;
    ColorScheme* destination=this->mColorScheme.get();
    DWORD value=this->ReadColor();
    destination->SetStandardColor(color,value);
    return true;
}
// SimpleComponentConfig.cpp
bool RectangleComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { int color=this->ReadColor(); this->mRectangleComponent->SetSolidColor(color); }
    else if(aKeyIdx==1) { int color=this->ReadColor(); this->mRectangleComponent->SetOutlineColor(color); }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// SimpleComponentConfig.cpp
bool RectangleComponent3DConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { int color=this->ReadColor(); this->mRectangleComponent3D->SetSolidColor(color); }
    else if(aKeyIdx==1) { int color=this->ReadColor(); this->mRectangleComponent3D->SetLightColor(color); }
    else if(aKeyIdx==2) { int color=this->ReadColor(); this->mRectangleComponent3D->SetDarkColor(color); }
    else if(aKeyIdx==3) { bool up=this->ReadBool(); this->mRectangleComponent3D->SetIsUp(up); }
    else if(aKeyIdx==4) { int thickness=this->ReadInt(); this->mRectangleComponent3D->SetThickness(thickness); }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// SimpleComponentConfig.cpp
bool ImageComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mImageComponent->SetImage(this->ReadImage(false));
    else if(aKeyIdx==1) { bool tile=this->ReadBool(); this->mImageComponent->mTile=tile; }
    else if(aKeyIdx==2) { bool stretch=this->ReadBool(); this->mImageComponent->mStretch=stretch; }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// SimpleComponentConfig.cpp
bool ScreenContainerConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mScreenContainer->AddScreen(this->ReadComponent());
    else if(aKeyIdx==1) this->mScreenContainer->ShowScreen(this->ReadComponent(),true);
    else return ContainerConfig::Virtual24(key);
    return true;
}
// MultiListBoxConfig.cpp
bool MultiListAreaConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) {
        int column=this->ReadInt(); this->EnsureComma();
        ListSortType type=this->ReadSortType();
        this->mMultiListArea->SetSortColumn(column,type);
    } else if(aKeyIdx==1) {
        int column=this->ReadInt(); this->EnsureComma();
        int width=this->ReadInt();
        this->mMultiListArea->SetColumnWidth(column,width,false);
    } else if(aKeyIdx==2) this->mMultiListArea->SetNumColumns(this->ReadInt());
    else if(aKeyIdx==3) this->mMultiListArea->SetDynamicSizeColumn(this->ReadInt());
    else if(aKeyIdx==4) {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString name=this->ReadString(false);
        ListArea::SortFunc function=NULL;
        const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_MULTILISTAREACONFIG_VIRTUAL24_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_name==0) function=ListArea::NumberSortFunc;
        else throw ConfigObjectException("Unknown sort func: "+(std::string)name);
        this->mMultiListArea->SetSortColumnFunc(column,function);
    } else if(aKeyIdx==5) this->mMultiListArea->SetAutoGrowColumns(this->ReadBool());
    else if(aKeyIdx==6) {
        int column=this->ReadInt(); this->EnsureComma();
        HorzAlign align=this->ReadHorzAlign();
        this->mMultiListArea->SetColumnHAlign(column,align);
    } else return ListAreaConfig::Virtual24(instruction);
    return true;
}
// MultiListBoxConfig.cpp
bool MultiListHeaderConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) {
        int column=this->ReadInt(); this->EnsureComma();
        Component* component=this->ReadComponent();
        this->mHeader->SetColumnItem(column,component);
    } else if(aKeyIdx==1) {
        this->mHeader->SetMultiListArea(SafeConfigGetComponent<MultiListArea*>(this->ReadComponent(),"MultiListArea"));
    } else return ContainerConfig::Virtual24(instruction);
    return true;
}
// ScrollbarConfig.cpp
bool ScrollbarConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mScrollbar->SetVertical(this->ReadBool());
    else if(aKeyIdx==1) this->mScrollbar->SetResizeThumb(this->ReadBool());
    else if(aKeyIdx==2) this->mScrollbar->SetMinThumbSize((unsigned char)this->ReadInt());
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
// TabCtrlConfig.cpp
bool TabCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        Button* button=SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button");
        this->EnsureComma();
        Container* container=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mTabCtrl->AddTab(button,container);
    } else if(aKeyIdx==1) {
        this->mTabCtrl->SetTabBarOnRight(this->ReadBool());
    } else if(aKeyIdx==2) {
        this->mTabCtrl->SetArrowsOnLeft(this->ReadBool());
    } else if(aKeyIdx==3) {
        int height=this->ReadInt(); this->mTabCtrl->SetTabBarHeight(height);
    } else if(aKeyIdx==4) {
        int overlap=this->ReadInt(); this->mTabCtrl->SetTabOverlap(overlap);
    } else if(aKeyIdx==5) {
        int overlap=this->ReadInt(); this->mTabCtrl->SetTabBarOverlap(overlap);
    } else if(aKeyIdx==6) {
        int offset=this->ReadInt(); this->mTabCtrl->SetBottomTabTopOffset(offset);
    } else if(aKeyIdx==7) {
        int offset=this->ReadInt(); this->mTabCtrl->SetBottomTabLeftOffset(offset);
    } else return ContainerConfig::Virtual24(key);
    return true;
}
// BrowserComponentConfig.cpp
bool BrowserComponentConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
        this->mBrowserComponent->SetTransparent(true);
    else if(aKeyIdx==1)
        this->mBrowserComponent->SetScroller(SafeConfigGetComponent<Scroller*>(this->ReadComponent(),"Scroller"));
    else if(aKeyIdx==2)
    {
        HTMLFontSize size;
        GUIString value=this->ReadString(false);
        const int aNameIdx_value=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_BROWSERCOMPONENTCONFIG_VIRTUAL24_VALUE,value.mData->mText,value.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_value==0) size=HTMLFontSize_Smallest;
        else if(aNameIdx_value==1) size=HTMLFontSize_Small;
        else if(aNameIdx_value==2) size=HTMLFontSize_Medium;
        else if(aNameIdx_value==3) size=HTMLFontSize_Large;
        else if(aNameIdx_value==4) size=HTMLFontSize_Largest;
        else throw ConfigObjectException("Invalid font size: "+std::string(value));
        this->mBrowserComponent->SetFontSize(size);
    }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
// ButtonConfig.cpp
void ButtonConfig::HandleButtonFlags()
{
    while(true)
    {
        bool on=true;
        GUIString flag=this->ReadFlag(&on);
        if(flag.length()==0) break;
        int value;
        const int aNameIdx_flag=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_BUTTONCONFIG_HANDLEBUTTONFLAGS_FLAG,flag.mData->mText,flag.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_flag==0) value=1;
        else if(aNameIdx_flag==1) value=2;
        else if(aNameIdx_flag==2) value=4;
        else if(aNameIdx_flag==3) value=8;
        else if(aNameIdx_flag==4) value=0x40;
        else if(aNameIdx_flag==5) value=0x400;
        else if(aNameIdx_flag==6) value=0x100;
        else if(aNameIdx_flag==7) value=0x200;
        else if(aNameIdx_flag==5) value=0x400;
        else throw ConfigObjectException("Unknown button flag"+(std::string)flag);
        this->mButton->SetButtonFlags(value,on);
    }
}
// ButtonConfig.cpp
bool ButtonConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) this->HandleButtonFlags();
    else if(aKeyIdx==1) this->mButton->SetText(this->ReadString(false),true);
    else if(aKeyIdx==2) this->mButton->SetCheck(this->ReadBool(),false);
    else if(aKeyIdx==3) this->mButton->SetClickSound(this->ReadSound().get());
    else if(aKeyIdx==4) this->HandleFitText();
    else return ComponentConfig::Virtual24(inInstruction);
    return true;
}
// ComboBoxConfig.cpp
bool ComboBoxConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        int width=this->ReadInt(); this->mComboBox->SetListBoxWidth(width);
    } else if(aKeyIdx==1) {
        int height=this->ReadInt(); this->mComboBox->SetListBoxHeight(height);
    } else if(aKeyIdx==2) {
        int width=this->ReadInt();
        int height=this->ReadInt();
        this->mComboBox->SetListBoxDimensions(width,height);
    } else if(aKeyIdx==3) {
        this->mComboBox->SetText(this->ReadString(false),true);
    } else if(aKeyIdx==4) {
        while(!this->EndOfString()) {
            this->mComboBox->AddString(this->ReadString(false),-1);
            if(!this->EndOfString()) this->EnsureComma();
        }
    } else if(aKeyIdx==5) {
        this->mComboBox->SetSelItem(this->ReadInt(),true);
    } else if(aKeyIdx==6) {
        this->mComboBox->SetNoType(this->ReadBool());
    } else if(aKeyIdx==7) {
        InputBox* input=this->mComboBox->mEdit.get();
        input->SetMaxChars(this->ReadInt());
    } else if(aKeyIdx==8) {
        bool show=this->ReadBool(); this->mComboBox->SetShowButtonWhenDisabled(show);
    } else return ContainerConfig::Virtual24(key);
    return true;
}
// DialogConfig.cpp
bool DialogConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) {
        bool value=this->ReadBool();
        this->mDialog->SetAllowDrag(value);
    } else if(aKeyIdx==1) {
        bool value=this->ReadBool();
        this->mDialog->SetKeepInRoot(value);
    } else if(aKeyIdx==2) {
        int id=this->ReadInt();
        this->mDialog->SetEndOnEscape(id);
    } else if(aKeyIdx==3) this->mDialog->SetDontEndOnEscape();
    else if(aKeyIdx==4) {
        Button* button=SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button");
        this->mDialog->SetDefaultButton(button);
    } else return ContainerConfig::Virtual24(instruction);
    return true;
}
// ImageButtonConfig.cpp
bool ImageButtonConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    ImageButtonType type;
    if(aKeyIdx==0) type=ImageButtonType_Normal;
    else if(aKeyIdx==1) type=ImageButtonType_Over;
    else if(aKeyIdx==2) type=ImageButtonType_Down;
    else if(aKeyIdx==3) type=ImageButtonType_Disabled;
    else if(aKeyIdx==4) type=ImageButtonType_Checked;
    else return ComponentConfig::Virtual24(instruction);
    this->mImageButton->SetImage(type,this->ReadImage(false).get());
    return true;
}
// LabelConfig.cpp
bool LabelConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mLabel->SetText(this->ReadString(false),true);
    else if(aKeyIdx==1) this->mLabel->SetFont(this->ReadFont());
    else if(aKeyIdx==2) this->mLabel->SetTextColor(this->ReadColor());
    else if(aKeyIdx==3) this->mLabel->SetBackground(this->ReadBackground());
    else if(aKeyIdx==4) this->mLabel->SetVertAlign(this->ReadVertAlign());
    else if(aKeyIdx==5) this->mLabel->SetHorzAlign(this->ReadHorzAlign());
    else if(aKeyIdx==6) this->mLabel->SetWrap(this->ReadBool());
    else if(aKeyIdx==7) this->mLabel->SetAdjustHeightToFitText(this->ReadBool());
    else return ComponentConfig::Virtual24(instruction);
    return true;
}
// ListAreaConfig.cpp
ListSortType ListAreaConfig::ReadSortType()
{
    GUIString name=this->ReadString(false);
    const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_LISTAREACONFIG_READSORTTYPE_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_name==0) return ListSortType_Ascending;
    if(aNameIdx_name==1) return ListSortType_Descending;
    if(aNameIdx_name==2) return ListSortType_None;
    throw ConfigObjectException("Invalid list sort type.");
}
// ListAreaConfig.cpp
void ListAreaConfig::ReadListFlags(ComponentConfig* inConfig,ListArea* inArea)
{
    while(true)
    {
        bool on=true;
        GUIString name=inConfig->ReadFlag(&on);
        if(name.length()==0) break;
        unsigned long flags;
        const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_LISTAREACONFIG_READLISTFLAGS_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
        if(aNameIdx_name==0) flags=2;
        else if(aNameIdx_name==1) flags=4;
        else if(aNameIdx_name==2) flags=8;
        else if(aNameIdx_name==3) flags=0x10;
        else if(aNameIdx_name==4) flags=0x20;
        else if(aNameIdx_name==5) flags=0x40;
        else if(aNameIdx_name==6) flags=0x80;
        else if(aNameIdx_name==7) flags=0x100;
        else if(aNameIdx_name==8) flags=0x200;
        else if(aNameIdx_name==9) flags=0x400;
        else if(aNameIdx_name==10) flags=0x800;
        else if(aNameIdx_name==11) flags=0x1000;
        else if(aNameIdx_name==12) flags=0x2000;
        else throw ConfigObjectException("Unknown list flag: "+(std::string)name);
        inArea->SetListFlags(flags,on);
    }
}
// ListAreaConfig.cpp
bool ListAreaConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) ListAreaConfig::ReadListFlags(this,this->mListArea);
    else if(aKeyIdx==1)
    {
        while(!this->EndOfString())
        {
            this->mListArea->InsertString(this->ReadString(false),-1);
            if(!this->EndOfString()) this->EnsureComma();
        }
    }
    else if(aKeyIdx==2) this->mListArea->SetFont(this->ReadFont());
    else if(aKeyIdx==3) this->mListArea->mTextColor=this->ReadColor();
    else if(aKeyIdx==4) this->mListArea->mSelColor=this->ReadSelectionColor();
    else if(aKeyIdx==5) this->mListArea->SetSortType(this->ReadSortType());
    else return ScrollAreaConfig::Virtual24(inInstruction);
    return true;
}
// InputBoxConfig.cpp
bool InputBoxConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) this->mInputBox->SetText(this->ReadString(false),true);
    else if(aKeyIdx==1) this->mInputBox->SetPasswordChar(this->ReadChar());
    else if(aKeyIdx==2) this->mInputBox->SetAllowSelection(this->ReadBool());
    else if(aKeyIdx==3) this->mInputBox->SetMaxChars(this->ReadInt());
    else if(aKeyIdx==4) this->mInputBox->SetCharWidth(this->ReadInt());
    else if(aKeyIdx==5) this->mInputBox->SetBackground(this->ReadBackground());
    else if(aKeyIdx==6) this->mInputBox->SetDisabledBackground(this->ReadBackground());
    else if(aKeyIdx==7) this->mInputBox->SetIcon(this->ReadImage(false).get());
    else if(aKeyIdx==8) this->mInputBox->mTextColor=this->ReadColor();
    else if(aKeyIdx==9) this->mInputBox->mDisabledTextColor=this->ReadColor();
    else if(aKeyIdx==10) this->mInputBox->mSelColor=this->ReadSelectionColor();
    else if(aKeyIdx==11) this->mInputBox->SetSelectOnFocus(this->ReadBool());
    else if(aKeyIdx==12) this->mInputBox->SetMaxInputHistory(this->ReadInt());
    else if(aKeyIdx==13) this->mInputBox->mUseDisabledColors=this->ReadBool();
    else if(aKeyIdx==14)
    {
        int on=this->ReadInt();
        int off;
        if(!this->EndOfString())
        {
            this->EnsureComma();
            off=this->ReadInt();
        }
        else
            off=on;
        this->mInputBox->SetCursorBlinkRate(on,off);
    }
    else return ComponentConfig::Virtual24(inInstruction);
    return true;
}
// TextAreaConfig.cpp
bool TextAreaConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        while(true) {
            bool add=true;
            GUIString flag=this->ReadFlag(&add);
            if(flag.length()==0) break;
            int mask;
            const int aNameIdx_flag=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_TEXTAREACONFIG_VIRTUAL24_FLAG,flag.mData->mText,flag.mData->mLength,sizeof(wchar_t));
            if(aNameIdx_flag==0) mask=1;
            else if(aNameIdx_flag==1) mask=2;
            else if(aNameIdx_flag==2) mask=4;
            else if(aNameIdx_flag==3) mask=8;
            else if(aNameIdx_flag==4) mask=0x10;
            else if(aNameIdx_flag==5) mask=0x20;
            else throw ConfigObjectException("Unknown text flag: "+(std::string)flag);
            this->mTextArea->SetTextFlags(mask,add);
        }
    } else if(aKeyIdx==1) {
        this->mTextArea->AddSegment(this->ReadString(false),false);
    } else if(aKeyIdx==2) {
        this->mTextArea->AddFormatedText(this->ReadString(false),false);
    } else if(aKeyIdx==3) {
        this->mTextArea->SetFont(this->ReadFont());
    } else if(aKeyIdx==4) {
        this->mTextArea->mTextColor=this->ReadColor();
    } else if(aKeyIdx==5) {
        this->mTextArea->mSelColor=this->ReadSelectionColor();
    } else if(aKeyIdx==6) {
        this->mTextArea->SetLinkFont(this->ReadFont());
    } else if(aKeyIdx==7) {
        this->mTextArea->SetMaxChars(this->ReadInt());
    } else if(aKeyIdx==8) {
        this->mTextArea->SetMaxLines(this->ReadInt());
    } else if(aKeyIdx==9) {
        this->mTextArea->SetWrapIndentSize(this->ReadInt());
    } else if(aKeyIdx==10) {
        this->mTextArea->SetLineAlignment(this->ReadHorzAlign());
    } else return ScrollAreaConfig::Virtual24(key);
    return true;
}
// TreeAreaConfig.cpp
bool TreeAreaConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
    {
        this->mTreeArea->SetSortType(this->ReadSortType());
        return true;
    }
    return ListAreaConfig::Virtual24(instruction);
}
// WONButtonConfig.cpp
void WONSimpleButtonConfig::HandleButtonState()
{
	GUIString aStateStr = this->ReadString(false);
	this->EnsureComma();

	int aState = 0;
	const int aNameIdx_aStateStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_WONSIMPLEBUTTONCONFIG_HANDLEBUTTONSTATE_ASTATESTR,aStateStr.mData->mText,aStateStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStateStr==0) aState = 0;
	else if(aNameIdx_aStateStr==1) aState = 1;
	else if(aNameIdx_aStateStr==2) aState = 2;
	else if(aNameIdx_aStateStr==3) aState = 3;
	else
		throw ConfigObjectException("Invalid button state: " + (std::string)aStateStr);

	SmartPtr<Image> anImage = this->ReadImage(false);
	this->mButton->SetImage(aState,anImage.get());
}
// WONButtonConfig.cpp
void WONSimpleButtonConfig::HandleButtonStates()
{	
	for(int i=0; i<4; i++)
	{
		this->mButton->SetImage(i,this->ReadImage(false).get()); 
		if(this->EndOfString())
			return;

		this->EnsureComma();
	}
}
// WONButtonConfig.cpp
bool WONSimpleButtonConfig::Virtual24(const std::string &theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
		this->mButton->SetScaleImage(this->ReadBool());
	else if(aKeyIdx==1)
	{
		GUIString aType = this->ReadString(false);
		if(!gSkin->CopySimpleButtonType(aType,this->mButton))
			throw ConfigObjectException("Type not found: " + std::string(aType));
	}
	else if(aKeyIdx==2)
		this->HandleButtonState();
	else if(aKeyIdx==3)
		this->HandleButtonStates();
	else if(aKeyIdx==4)
		this->mButton->SetCheckMouseButtonDown(this->ReadBool());
	else
		return ButtonConfig::Virtual24(theInstruction);

	return true;
}
// WONButtonConfig.cpp
void WONButtonConfig::HandleSetButtonState()
{
	GUIString aStateStr = this->ReadString(false);
	this->EnsureComma();

	int aState = 0;
	bool checked = false;
	const int aNameIdx_aStateStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_WONBUTTONCONFIG_HANDLESETBUTTONSTATE_ASTATESTR,aStateStr.mData->mText,aStateStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStateStr==0) aState = 0;
	else if(aNameIdx_aStateStr==1) aState = 1;
	else if(aNameIdx_aStateStr==2) aState = 2;
	else if(aNameIdx_aStateStr==3) aState = 3;
	else if(aNameIdx_aStateStr==4) aState = 4;
	else if(aNameIdx_aStateStr==5) { aState = 0; checked = true; }
	else if(aNameIdx_aStateStr==6) { aState = 1; checked = true; }
	else if(aNameIdx_aStateStr==7) { aState = 2; checked = true; }
	else if(aNameIdx_aStateStr==8) { aState = 3; checked = true; }
	else if(aNameIdx_aStateStr==9) { aState = 4; checked = true; }
	else
		throw ConfigObjectException("Invalid button state: " + (std::string)aStateStr);

	SmartPtr<Image> anImage = this->ReadImage(false);
	int aColor = -1;
	SmartPtr<Font> aFont;
	if(!this->EndOfString())
	{
		this->EnsureComma();
		aColor = this->ReadColor();
		if(!this->EndOfString())
		{
			this->EnsureComma();
			aFont = this->ReadFont();
		}
	}

	this->mButton->SetButtonState(aState,anImage.get(),aColor,aFont.get(),checked);
}
// WONButtonConfig.cpp
bool WONButtonConfig::Virtual24(const std::string& theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
		this->mButton->SetFont(this->ReadFont().get());
	else if(aKeyIdx==1)
	{
		GUIString aType = this->ReadString(false);
		if(!gSkin->CopyButtonType(aType,this->mButton))
			throw ConfigObjectException("Type not found: " + std::string(aType));
	}
	else if(aKeyIdx==2)
		this->mButton->SetTextColor(this->ReadColor());
	else if(aKeyIdx==3)
		this->mButton->SetDefaultTextColor(this->ReadColor());
	else if(aKeyIdx==4)
		this->mButton->SetVertAlign(this->ReadVertAlign());
	else if(aKeyIdx==5)
		this->mButton->SetHorzAlign(this->ReadHorzAlign());
	else if(aKeyIdx==6)
		this->mButton->SetTransparent(this->ReadBool());
	else if(aKeyIdx==7)
		this->mButton->SetAdjustHeightToFitText(this->ReadBool());
	else if(aKeyIdx==8)
		this->mButton->SetWrapText(this->ReadBool());
	else if(aKeyIdx==9)
		this->mButton->SetDrawGrayText(this->ReadBool());
	else if(aKeyIdx==10)
	{
		int left = this->ReadInt(); this->EnsureComma();
		int top = this->ReadInt(); this->EnsureComma();
		int right = this->ReadInt(); this->EnsureComma();
		int bottom = this->ReadInt();
		this->mButton->SetTextPadding(left,top,right,bottom);
	}
	else if(aKeyIdx==11)
	{
		int left = this->ReadInt(); this->EnsureComma();
		int top = this->ReadInt(); this->EnsureComma();
		int right = this->ReadInt(); this->EnsureComma();
		int bottom = this->ReadInt();
		this->mButton->SetFocusPadding(left,top,right,bottom);
	}
	else if(aKeyIdx==12)
	{
		int left = this->ReadInt(); 
		int right = this->mButton->GetRightTextPad();
		if(!this->EndOfString())
		{
			this->EnsureComma(); 
			right = this->ReadInt();
		}
		this->mButton->SetHorzTextPad(left,right);
	}
	else if(aKeyIdx==13)
	{
/*		int left = ReadInt(); EnsureComma();
		int right = ReadInt();
		mButton->IncrementHorzTextPad(left,right);
*/	}
	else if(aKeyIdx==14)
	{
/*		int top = ReadInt(); EnsureComma();
		int bottom = ReadInt();
		mButton->IncrementVertTextPad(top,bottom);
*/	}
	else if(aKeyIdx==15)
		this->HandleSetButtonState();
	else if(aKeyIdx==16)
		this->mButton->SetScaleImage(this->ReadBool());
	else if(aKeyIdx==17)
		this->mButton->SetFocusStyle(this->ReadBool()?1:0);
	else
		return ButtonConfig::Virtual24(theInstruction);

	return true;
}
// SplitterConfig.cpp
Splitter::QuadrantPosition SplitterConfig::ReadQuadrant()
{
	GUIString aStr = this->ReadString(false);
	const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SPLITTERCONFIG_READQUADRANT_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStr==0)
		return Splitter::Position_Left;
	else if(aNameIdx_aStr==1)
		return Splitter::Position_Right;
	else if(aNameIdx_aStr==2)
		return Splitter::Position_Top;
	else if(aNameIdx_aStr==3)
		return Splitter::Position_Bottom;
	else if(aNameIdx_aStr==4)
		return Splitter::Position_TopLeft;
	else if(aNameIdx_aStr==5)
		return Splitter::Position_TopRight;
	else if(aNameIdx_aStr==6)
		return Splitter::Position_BottomLeft;
	else if(aNameIdx_aStr==7)
		return Splitter::Position_BottomRight;
	else
		throw ConfigObjectException("Invalid quadrant: " + (std::string)aStr);
}
// SplitterConfig.cpp
Splitter::SplitterType SplitterConfig::ReadSplitterType()
{
	GUIString aStr = this->ReadString(false);
	const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SPLITTERCONFIG_READSPLITTERTYPE_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
	if(aNameIdx_aStr==0)
		return Splitter::Splitter_LeftRight;
	else if(aNameIdx_aStr==1)
		return Splitter::Splitter_UpDown;
	else if(aNameIdx_aStr==2)
		return Splitter::Splitter_LeftRight_Top;
	else if(aNameIdx_aStr==3)
		return Splitter::Splitter_LeftRight_Bottom;
	else if(aNameIdx_aStr==4)
		return Splitter::Splitter_UpDown_Left;
	else if(aNameIdx_aStr==5)
		return Splitter::Splitter_UpDown_Right;
	else if(aNameIdx_aStr==6)
		return Splitter::Splitter_Center;
	else
		throw ConfigObjectException("Invalid splitter type: " + (std::string)aStr);
}
// SplitterConfig.cpp
bool SplitterConfig::Virtual24(const std::string& theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
	{
		Component *aComponent = this->ReadComponent(); this->EnsureComma();
		Splitter::QuadrantPosition aPosition = this->ReadQuadrant();
		this->mSplitter->AddQuadrant(aComponent,aPosition);
	}
	else if(aKeyIdx==1)
	{
		Component *aComponent = this->ReadComponent(); this->EnsureComma();
		Splitter::SplitterType aType = this->ReadSplitterType();
		this->mSplitter->AddSplitter(aComponent,aType);
	}
	else if(aKeyIdx==2)
	{
		Splitter::SplitterType aType = this->ReadSplitterType();
		int aSize = 10;
		if(!this->EndOfString())
		{
			this->EnsureComma();
			aSize = this->ReadInt();
		}
		this->mSplitter->AddInvisibleSplitter(aType,aSize);
	}
	else if(aKeyIdx==3)
	{
		Container *aContainer= SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container"); this->EnsureComma();
		Splitter::SplitterType aType = this->ReadSplitterType();
		Component *aDragComponent = NULL;
		if(!this->EndOfString())
		{
			this->EnsureComma();
			aDragComponent = this->ReadComponent();
		}
		this->mSplitter->AddComplexSplitter(aContainer,aType,aDragComponent);
	}
	else if(aKeyIdx==4)
	{
		int minLeft = this->ReadInt(); this->EnsureComma();
		int minRight = this->ReadInt(); 
		this->mSplitter->SetMinLeftRight(minLeft,minRight);
	}
	else if(aKeyIdx==5)
	{
		int minTop = this->ReadInt(); this->EnsureComma();
		int minBottom = this->ReadInt();
		this->mSplitter->SetMinTopBottom(minTop,minBottom);
	}
	else if(aKeyIdx==6)
	{
		int leftPercent = this->ReadInt(); this->EnsureComma();
		int topPercent = this->ReadInt();
		this->mSplitter->SetPercentages(leftPercent,topPercent);
	}
	else if(aKeyIdx==7)
		this->mSplitter->MaxQuadrant(this->ReadQuadrant());
	else if(aKeyIdx==8)
		this->mSplitter->UnMaxQuadrant();
	else if(aKeyIdx==9)
	{
		int x = this->ReadInt(); this->EnsureComma();
		int y = this->ReadInt();
		this->mSplitter->SetSplitterPos(x,y);
	}
	else if(aKeyIdx==10)
	{
		Button *aButton = SafeConfigGetComponent<Button*>(this->ReadComponent(),"Button"); this->EnsureComma();
		Splitter::QuadrantPosition aPosition = this->ReadQuadrant();
		this->mSplitter->RegisterMaxButton(aButton,aPosition);
	}
	else
		return ContainerConfig::Virtual24(theInstruction);

	return true;
}
// WONListConfig.cpp
bool WONScrollbarConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        if(this->ReadBool()) {
            Button* rect=new Button;
            this->mScrollbar->SetScrollRect(rect);
        }
    } else return ScrollbarConfig::Virtual24(key);
    return true;
}
// WONListConfig.cpp
bool WONHeaderControlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(MultiListHeaderConfig::Virtual24(key)) return true;
    if(aKeyIdx==0) {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString title=this->ReadString(false);
        this->mHeader->SetColumn(column,title,NULL);
    } else return false;
    return true;
}
// WONListConfig.cpp
bool WONListCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        int height=this->ReadInt(); this->mListCtrl->mHeaderHeight=height;
    } else if(aKeyIdx==1) {
        MultiListArea* area=static_cast<MultiListArea*>(this->mListCtrl->GetScrollArea());
        int count=this->ReadInt(); area->SetNumColumns(count);
    } else if(aKeyIdx==2) {
        MultiListArea* area=static_cast<MultiListArea*>(this->mListCtrl->GetScrollArea());
        int column=this->ReadInt(); area->SetDynamicSizeColumn(column);
    } else if(aKeyIdx==3) {
        ListAreaConfig::ReadListFlags(this,static_cast<MultiListArea*>(this->mListCtrl->GetScrollArea()));
    } else if(aKeyIdx==4) {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString title=this->ReadString(false); this->EnsureComma();
        int width=this->ReadInt(); this->mListCtrl->SetStandarColumnHeader(column,title,width);
    } else if(aKeyIdx==5) {
        int column=this->ReadInt(); this->EnsureComma();
        Component* component=this->ReadComponent(); this->EnsureComma();
        int width=this->ReadInt(); this->mListCtrl->SetColumnHeader(column,component,width);
    } else return WONScrollContainerConfig::Virtual24(key);
    return true;
}
// WONFrameConfig.cpp
bool WONFrameComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        this->mFrameComponent->SetFrameLeft(this->ReadInt()); this->EnsureComma();
        this->mFrameComponent->SetFrameTop(this->ReadInt()); this->EnsureComma();
        this->mFrameComponent->SetFrameRight(this->ReadInt()); this->EnsureComma();
        this->mFrameComponent->SetFrameBottom(this->ReadInt());
    } else if(aKeyIdx==1) {
        this->mFrameComponent->SetFrameImage(this->ReadImage(false).get());
    } else return ComponentConfig::Virtual24(key);
    return true;
}
// WONFrameValueConfig.cpp
static WONFrame ReadFrame(ComponentConfig* config)
{
    SmartPtr<Image> image=config->ReadImage(false);
    StretchImage* stretch=dynamic_cast<StretchImage*>(image.get());
    if(stretch!=NULL) return stretch;
    config->EnsureComma();
    int left=config->ReadInt();
    if(config->EndOfString()) return WONFrame(image.get(),left);
    config->EnsureComma();
    int top=config->ReadInt(); config->EnsureComma();
    int right=config->ReadInt(); config->EnsureComma();
    int bottom=config->ReadInt();
    return WONFrame(image.get(),left,top,right,bottom);
}
// WONFrameValueConfig.cpp
bool WONComboBoxConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { bool on=this->ReadBool(); this->mComboBox->mDrawFrame=on; }
    else if(aKeyIdx==1) { bool on=this->ReadBool(); this->mComboBox->mButtonInFrame=on; }
    else if(aKeyIdx==2) this->mComboBox->SetFrame(ReadFrame(this));
    else return ComboBoxConfig::Virtual24(key);
    return true;
}
// WONFrameValueConfig.cpp
bool WONTabCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mTabCtrl->SetTransparent(this->ReadBool());
    else if(aKeyIdx==1) {
        GUIString name=this->ReadString(false); this->EnsureComma();
        Container* tab=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mTabCtrl->AddStandardTab(name,tab);
    } else if(aKeyIdx==2) this->mTabCtrl->SetFrame(ReadFrame(this));
    else return TabCtrlConfig::Virtual24(key);
    return true;
}
// WONFrameValueConfig.cpp
bool WONScrollContainerConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        if(!this->ReadBool()) this->mScrollContainer->SetFrame(WONFrame());
    } else if(aKeyIdx==1) { bool on=this->ReadBool(); this->mScrollContainer->mTransparent=on; }
    else if(aKeyIdx==2) this->mScrollContainer->SetFrame(ReadFrame(this));
    else if(aKeyIdx==3) this->mScrollContainer->SetCorner(this->ReadImage(false));
    else return ScrollbarScrollerConfig::Virtual24(key);
    return true;
}
// ResourceTableQueries.cpp
bool ScrollAreaConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mScrollArea->SetBackground(this->ReadBackground());
    else if(aKeyIdx==1)
    {
        int left=this->ReadInt();this->EnsureComma();
        int top=this->ReadInt();this->EnsureComma();
        int right=this->ReadInt();this->EnsureComma();
        int bottom=this->ReadInt();
        this->mScrollArea->SetBorderPadding(left,top,right,bottom);
    }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
// ResourceTableQueries.cpp
bool ScrollerConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
    {
        ScrollArea* area=dynamic_cast<ScrollArea*>(this->ReadComponent());
        if(area==NULL) throw ConfigObjectException("Expecting ScrollArea");
        this->mScroller->SetScrollArea(area);
        return true;
    }
    return ContainerConfig::Virtual24(instruction);
}
// ResourceTableQueries.cpp
ScrollbarCondition ScrollbarScrollerConfig::GetScrollbarCondition()
{
    GUIString condition=this->ReadString(false);
    const int aNameIdx_condition=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SCROLLBARSCROLLERCONFIG_GETSCROLLBARCONDITION_CONDITION,condition.mData->mText,condition.mData->mLength,sizeof(wchar_t));
    if(aNameIdx_condition==0) return ScrollbarCondition_Disable;
    if(aNameIdx_condition==1) return ScrollbarCondition_Enable;
    if(aNameIdx_condition==2) return ScrollbarCondition_Conditional;
    throw ConfigObjectException("Invalid scrollbar condition: "+(std::string)condition);
}
// ResourceTableQueries.cpp
bool ScrollbarScrollerConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0)
    {
        ScrollbarCondition horz=this->GetScrollbarCondition();this->EnsureComma();
        ScrollbarCondition vert=this->GetScrollbarCondition();
        this->mScrollbarScroller->SetScrollbarConditions(horz,vert);
        return true;
    }
    return ScrollerConfig::Virtual24(instruction);
}
// StretchImageConfig.cpp
bool StretchCenterImageConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mStretchCenterImage->SetStretchImage(this->ReadImage(false).get());
    else if(aKeyIdx==1) this->mStretchCenterImage->SetCenterImage(this->ReadImage(false).get());
    else return ImageConfig::Virtual24(instruction);
    return true;
}
// ComponentConfigResource.cpp
bool ComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if (aKeyIdx==0) return this->HandleModify();
    if (aKeyIdx==1) return this->HandleMapComponent();
    if (aKeyIdx==2) {
        Component* component=this->mComponent.get();
        component->mControlId=this->ReadInt();
    } else if (aKeyIdx==3) {
        Component* component=this->mComponent.get();
        int value=this->ReadInt();
        component->SetPos(value,component->Top());
    } else if (aKeyIdx==4) {
        Component* component=this->mComponent.get();
        int value=this->ReadInt();
        component->SetPos(component->Left(),value);
    } else if (aKeyIdx==5) {
        Component* component=this->mComponent.get();
        this->mHasSetWidth=true;
        int value=this->ReadInt();
        component->SetSize(value,component->Height());
    } else if (aKeyIdx==6) {
        Component* component=this->mComponent.get();
        this->mHasSetHeight=true;
        int value=this->ReadInt();
        component->SetSize(component->Width(),value);
    } else if (aKeyIdx==7) {
        int x=this->ReadInt(); this->EnsureComma();
        int y=this->ReadInt();
        this->mComponent->SetPos(x,y);
    } else if (aKeyIdx==8) {
        this->mHasSetWidth=this->mHasSetHeight=true;
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mComponent->SetSize(width,height);
    } else if (aKeyIdx==9) {
        int color=this->ReadColor();
        if (this->EndOfString()) this->mComponent->SetTranslucentColor(color,4);
        else {
            this->EnsureComma();
            this->mComponent->SetTranslucentColor(color,this->ReadInt());
        }
    } else if (aKeyIdx==10) {
        this->SetDesiredSizeIfNecessary();
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mHasSetWidth=width>0;
        this->mHasSetHeight=height>0;
        this->mComponent->SetSizeAtLeast(width,height);
    } else if (aKeyIdx==11) {
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mComponent->SetDesiredSize(width,height);
    } else if (aKeyIdx==12) {
        Component* component=this->mComponent.get();
        int width=this->ReadInt();
        component->SetDesiredWidth(width);
    } else if (aKeyIdx==13) {
        Component* component=this->mComponent.get();
        int height=this->ReadInt();
        component->SetDesiredHeight(height);
    } else if (aKeyIdx==14) {
        this->mHasSetWidth=this->mHasSetHeight=true;
        int x=this->ReadInt(); this->EnsureComma();
        int y=this->ReadInt(); this->EnsureComma();
        int width=this->ReadInt(); this->EnsureComma();
        int height=this->ReadInt();
        this->mComponent->SetPosSize(x,y,width,height);
    } else if (aKeyIdx==15) {
        this->mComponent->SetEnabled(this->ReadBool());
    } else if (aKeyIdx==16) {
        this->mComponent->SetVisible(this->ReadBool());
    } else if (aKeyIdx==17) {
        if (this->ReadBool()) this->mComponent->RequestFocus();
    } else if (aKeyIdx==18) {
        this->mComponent->SetDefaultCursor(this->ReadCursor());
    } else if (aKeyIdx==19) {
        while (true) {
            bool add=true;
            GUIString name=this->ReadFlag(&add);
            if (name.length()==0) break;
            int flag;
            const int aNameIdx_name=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_COMPONENTCONFIG_VIRTUAL24_NAME,name.mData->mText,name.mData->mLength,sizeof(wchar_t));
            if (aNameIdx_name==0) flag=0x1;
            else if (aNameIdx_name==1) flag=0x8;
            else if (aNameIdx_name==2) flag=0x20;
            else if (aNameIdx_name==3) flag=0x40;
            else if (aNameIdx_name==4) flag=0x80;
            else if (aNameIdx_name==5) flag=0x100;
            else if (aNameIdx_name==6) flag=0x200;
            else if (aNameIdx_name==7) flag=0x800;
            else if (aNameIdx_name==8) flag=0x1000;
            else if (aNameIdx_name==9) flag=0x4000;
            else if (aNameIdx_name==10) flag=0x2000;
            else if (aNameIdx_name==11) flag=0x20000;
            else throw ConfigObjectException("Unknown component flag: "+(std::string)name);
            this->mComponent->SetComponentFlags(flag,add);
        }
    } else if (aKeyIdx==20) {
        Window* window=Window::GetDefaultWindow();
        window->SetSkipDecodeImages(!this->ReadBool());
    } else if (aKeyIdx==21) {
        gShowParseTime=this->ReadBool();
    } else if (aKeyIdx==22) {
        ConfigParser::ResetLineCount();
        PerfTimer::GlobalStart(NULL);
    } else if (aKeyIdx==23) {
        PerfTimer::GlobalStop(NULL);
        gLinesParsed=ConfigParser::GetLineCount();
    } else return false;
    return true;
}
// WONSkinConfig.cpp
bool WONSkinConfig::ReadVert()
{
    GUIString text=this->ReadString(false);
    const int aNameIdx_text=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_WONSKINCONFIG_READVERT_TEXT,text.mData->mText,text.mData->mLength,sizeof(wchar_t));
    return aNameIdx_text==0;
}
// WONSkinConfig.cpp
bool WONSkinConfig::Virtual24(const std::string &theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
	{

	}
	else if(aKeyIdx==1)
	{
		GUIString aName = this->ReadString(false); this->EnsureComma();
		WONButton* aButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
		gSkin->mButtonTypeMap[aName] = aButton;
	}
	else if(aKeyIdx==2)
	{
		GUIString aName = this->ReadString(false); this->EnsureComma();
		WONSimpleButton* aButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
		gSkin->mSimpleButtonTypeMap[aName] = aButton;
	}
	else if(aKeyIdx==3)
		gSkin->mPushButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(aKeyIdx==4)
		gSkin->mCheckbox = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(aKeyIdx==5)
	{
		gSkin->mRadioButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
		gSkin->mRadioButton->SetButtonFlags(8,true);
	}
	else if(aKeyIdx==6)
		gSkin->mHeaderButton = SafeConfigGetComponent<WONHeaderButton*>(this->ReadComponent(),"WONHeaderButton");
	else if(aKeyIdx==7)
		gSkin->mTabButton = SafeConfigGetComponent<WONButton*>(this->ReadComponent(),"WONButton");
	else if(aKeyIdx==8)
	{
		Direction aDirection = this->ReadDirection(); this->EnsureComma();
		gSkin->mArrowButton[aDirection] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");

	}
	else if(aKeyIdx==9)
		gSkin->mComboButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	else if(aKeyIdx==10)
		gSkin->mCloseButton = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	else if(aKeyIdx==11)
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollGutter[isVert?1:0] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	}
	else if(aKeyIdx==12)
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollThumb[isVert?1:0] = SafeConfigGetComponent<WONSimpleButton*>(this->ReadComponent(),"WONSimpleButton");
	}
	else if(aKeyIdx==13)
	{
		bool isVert = this->ReadVert(); this->EnsureComma();
		gSkin->mScrollbar[isVert?1:0] = SafeConfigGetComponent<WONScrollbar*>(this->ReadComponent(),"WONScrollbar");
	}
	else if(aKeyIdx==14)
		gSkin->mScrollContainer = SafeConfigGetComponent<WONScrollContainer*>(this->ReadComponent(),"WONScrollContainer");
	else if(aKeyIdx==15)
		gSkin->mListCtrl = SafeConfigGetComponent<WONListCtrl*>(this->ReadComponent(),"WONListCtrl");
	else if(aKeyIdx==16)
	{
		gSkin->mTabCtrl = SafeConfigGetComponent<WONTabCtrl*>(this->ReadComponent(),"WONTabCtrl");
		gSkin->mTabCtrl->mSkin=NULL;
	}
	else if(aKeyIdx==17)
		gSkin->mComboBox = SafeConfigGetComponent<WONComboBox*>(this->ReadComponent(),"WONComboBox");
	else if(aKeyIdx==18)
		gSkin->mPopupBack = SafeConfigGetComponent<WONFrameComponent*>(this->ReadComponent(),"WONPopupBack");
	else if(aKeyIdx==19)
		gSkin->mDialogBack = SafeConfigGetComponent<WONFrameComponent*>(this->ReadComponent(),"WONDialogBack");
	else if(aKeyIdx==20)
		this->HandleButtonClickSound();
	else if(aKeyIdx==21)
	{
		StandardCursor aType = this->ReadStandardCursor(); this->EnsureComma();
		SmartPtr<Image> anImage = this->ReadImage(false);
		SmartPtr<Cursor> aCursor = new ComponentCursor(new ImageComponent(anImage.get()));
		Cursor::SetStandardCursor(aType,aCursor.get());
	}
	else
		return false;

	return true;
}
// FontConfig.cpp
bool FontConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->SetFont(this->ReadFont().get());
    else if(aKeyIdx==1) {
        this->mNeedRecalcFont=true;
        this->mDescriptor.mSize=this->ReadInt();
    } else if(aKeyIdx==2) {
        while(true) {
            this->mNeedRecalcFont=true;
            bool add=true;
            GUIString flag=this->ReadFlag(&add);
            if(flag.length()==0) break;
            int style=0;
            const int aNameIdx_flag=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_FONTCONFIG_VIRTUAL24_FLAG,flag.mData->mText,flag.mData->mLength,sizeof(wchar_t));
            if(aNameIdx_flag==0) style=FontStyle_Bold;
            else if(aNameIdx_flag==1) style=FontStyle_Italic;
            else if(aNameIdx_flag==2) style=FontStyle_Plain;
            else if(aNameIdx_flag==3) style=FontStyle_Underline;
            else throw ConfigObjectException("Unknown font style: "+(std::string)flag);
            if(add) this->mDescriptor.mStyle|=style;
            else this->mDescriptor.mStyle&=~style;
        }
    } else if(aKeyIdx==3) WindowManager::GetDefaultWindowManager()->SetDefaultFont(this->ReadFont().get());
    else if(aKeyIdx==4) {
        GUIString name=this->ReadString(false);
        this->EnsureComma();
        SmartPtr<Font> font=this->ReadFont();
        WindowManager::GetDefaultWindowManager()->SetNamedFont(name,font.get());
    } else return false;
    return true;
}
// ImageConfig.cpp
bool ImageConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) this->mImage=this->ReadImage(false);
    else if(aKeyIdx==1)
    {
        int width=this->ReadInt();
        this->EnsureComma();
        int height=this->ReadInt();
        this->mImage->SetSize(width,height);
    }
    else return false;
    return true;
}

// R6 WON round 40: ContainerConfig's instructions and its layout handlers as they were, copied verbatim from de9ba76a
// by build/r40/gen_cfgcont.py (HandleLayout, HandleNewLayoutPoint and HandleNoOverlapLayout stay C++: the world's)
// Reads a comma-separated list of components and adds each to the container
bool ContainerConfig::HandleAdd()
{
	while(true)
	{
		Component *aComp = this->ReadComponent();
		this->mContainer->AddChild(aComp);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}


	return true;
}
// Reads a comma-separated list of layout points and adds each to the container
bool ContainerConfig::HandleAddLayoutPoint()
{
	while(true)
	{
		LayoutPoint *aPoint = SafeConfigGetComponent<LayoutPoint*>(this->ReadComponent(),"LayoutPoint");
		this->mContainer->AddLayoutPoint(aPoint);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}


	return true;
}
// Handles the container config instructions (ADD, LAYOUT and its variants, layout ids, tab focus,
// child-only input); other instructions go to the base config
bool ContainerConfig::Virtual24(const std::string& theInstruction)
{
	const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION,theInstruction.data(),theInstruction.size());
	if(aKeyIdx==0)
		return this->HandleAdd();
	else if(aKeyIdx==1)
		return this->HandleAddLayoutPoint();
	else if(aKeyIdx==2)
		return this->HandleNewLayoutPoint();
	else if(aKeyIdx==3)
		return this->HandleLayout();
	else if(aKeyIdx==4)
		this->mCurLayoutId = this->ReadInt();
	else if(aKeyIdx==5)
	{
		bool enable = this->ReadBool();
		while(true)
		{
			this->EnsureComma();
			int anId = this->ReadInt();
			this->mContainer->EnableLayoutInstructions(anId,enable);
			if(this->EndOfString())
				break;
		}
	}
	else if(aKeyIdx==6)
		this->mContainer->Relayout();
	else if(aKeyIdx==7)
		return this->HandleLayout(true);
	else if(aKeyIdx==8)
		return this->HandleLayout(false,true);
	else if(aKeyIdx==9)
		return this->HandleSameSizeLayout(true,true);
	else if(aKeyIdx==10)
		return this->HandleSameSizeLayout(true,false);
	else if(aKeyIdx==11)
		return this->HandleSameSizeLayout(false,true);
	else if(aKeyIdx==12)
		return this->HandleCenterLayout(true,true);
	else if(aKeyIdx==13)
		return this->HandleCenterLayout(true,false);
	else if(aKeyIdx==14)
		return this->HandleCenterLayout(false,true);
	else if(aKeyIdx==15)
		return this->HandleSpaceEvenlyLayout(true);
	else if(aKeyIdx==16)
		return this->HandleSpaceEvenlyLayout(false);
	else if(aKeyIdx==17)
		return this->HandleClearRightLayout();
	else if(aKeyIdx==18)
		return this->HandleNoOverlapLayout();
	else if(aKeyIdx==19)
		return this->HandleSizeAtLeastLayout();
	else if(aKeyIdx==20)
		this->mContainer->SetIsolateTabFocus(this->ReadBool());
	else if(aKeyIdx==21)
		this->mContainer->SetOnlyChildInput(this->ReadBool());
	else
		return ComponentConfig::Virtual24(theInstruction);

	return true;
}
// Reads a component list into a layout that gives them the same width and/or height
bool ContainerConfig::HandleSameSizeLayout(bool sameWidth, bool sameHeight)
{	
	SmartPtr<SameSizeLayout> aLayout = new SameSizeLayout(sameWidth, sameHeight);

	while(true)
	{	
		Component *aComp = this->ReadComponent();
		aLayout->Add(aComp);
		if(this->EndOfString())
			break;
		else
			this->EnsureComma();
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// Reads a reference component and a component list into a layout that centres them on it
bool ContainerConfig::HandleCenterLayout(bool horz, bool vert)
{
	Component *aComp = this->ReadComponent();
	SmartPtr<CenterLayout> aLayout = new CenterLayout(aComp,horz,vert);
	while(!this->EndOfString())
	{	
		this->EnsureComma();
		aComp = this->ReadComponent();
		aLayout->Add(aComp);
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// Reads a component, a padding and a component list into a clear-right layout
bool ContainerConfig::HandleClearRightLayout()
{
	Component *aComp = this->ReadComponent(); this->EnsureComma();
	int aPad = this->ReadInt(); this->EnsureComma();

	SmartPtr<ClearRightLayout> aLayout = new ClearRightLayout(aComp,aPad);
	while(true)
	{
		aComp = this->ReadComponent();
		aLayout->Add(aComp);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// Reads a bounding component, two paddings and a component list into a layout that spaces them evenly
bool ContainerConfig::HandleSpaceEvenlyLayout(bool horz)
{
	Component *aSpaceBox = this->ReadComponent(); this->EnsureComma();
	int aLeftPad = this->ReadInt(); this->EnsureComma();
	int aRightPad = this->ReadInt(); this->EnsureComma();

	SmartPtr<SpaceEvenlyLayout> aLayout = new SpaceEvenlyLayout(aSpaceBox,aLeftPad,aRightPad,horz);
	while(!this->EndOfString())
	{
		Component *aComponent = this->ReadComponent();
		aLayout->Add(aComponent);
		if(this->EndOfString())
			break;

		this->EnsureComma();
	}

	this->mContainer->AddChildLayout(aLayout.get(),this->mCurLayoutId);
	return true;
}
// Reads a size-at-least layout (target, paddings, components); it is added under the default layout id
bool ContainerConfig::HandleSizeAtLeastLayout()
{
	Component *target = this->ReadComponent(); this->EnsureComma();
	int aHorzPad = this->ReadInt(); this->EnsureComma();
	int aVertPad = this->ReadInt(); 

	SmartPtr<SizeAtLeastLayout> aLayout = new SizeAtLeastLayout(target,aHorzPad, aVertPad);
	while(!this->EndOfString())
	{
		this->EnsureComma();
		aLayout->Add(this->ReadComponent());		
	}
	this->mContainer->AddChildLayout(aLayout.get());
	return true;
}

// ---- R6 WON round 41: ContainerConfig's LAYOUT family, NOOVERLAPLAYOUT and NEWLAYOUTPOINT (copied from
// ContainerConfigInstructions.cpp and ContainerConfigPoints.cpp before the round)
// Reads a layout rule (target, instruction names, reference, up to four offsets) and adds it to the container;
// chain and repeat forms reuse the previous rule's instructions and reference
bool ContainerConfig::HandleLayout(bool chain, bool repeat)
{
	Component *target = this->ReadComponent(); 
	int instructions = 0;
	Component *ref;

	if(!chain && !repeat)
	{
		this->EnsureComma();
		while(true)
		{
			GUIString aStr = this->ReadString(true);
			if(aStr.length()==0)
				break;

			// the names in the original's order, each compared as the original compared it
			const uint8_t* aName = NULL;
			int aFlag = 0;
			bool aFound = false;
			for(int k = 0; !aFound && ee_won_gui_layout_keyword(k, &aName, &aFlag); k++)
				if(aStr.CompareNoCase((const char*)aName)==0) { instructions |= aFlag; aFound = true; }
			if(!aFound)
				throw ConfigObjectException("Unknown layout instruction: " + (std::string)aStr);
		}
	}
	else
		instructions = this->mLayoutInstruction;

	if(chain || (repeat && this->EndOfString()))
	{
		if(this->mLayoutRef==NULL)
			throw ConfigObjectException("Invalid LayoutChain");

		ref = this->mLayoutRef;
	}
	else
	{
		this->EnsureComma();
		ref = this->ReadComponent();
	}

	if(!this->EndOfString())
		this->EnsureComma();

	int i;
	for(i=0; i<4; i++)
	{
		if(this->EndOfString())
			break;

		this->mLayoutParams[i] = this->ReadInt();
		if(!this->EndOfString())
			this->EnsureComma();
	}

	if(ee_won_gui_layout_zero_rest(chain, repeat, i))
	{
		for(; i<4; i++)
			this->mLayoutParams[i] = 0;
	}

	this->mContainer->Fn2F6140(target,instructions,ref,this->mLayoutParams[0],this->mLayoutParams[1],
		this->mLayoutParams[2],this->mLayoutParams[3],this->mCurLayoutId);

	if(repeat)
		this->mLayoutRef = ref;
	else
		this->mLayoutRef = target;

	this->mLayoutInstruction = instructions;

	return true;
}
// Reads a no-overlap layout (target, sides, reference, paddings, more components); it is added
// under the default layout id, not the current one
bool ContainerConfig::HandleNoOverlapLayout()
{
	Component *target = this->ReadComponent();
	this->EnsureComma();

	int instructions = 0;
	while(true)
	{
		GUIString aStr = this->ReadString(true);
		if(aStr.length()==0)
			break;

		const int aNameIdx_aStr=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_CONTAINERCONFIG_HANDLENOOVERLAPLAYOUT_ASTR,aStr.mData->mText,aStr.mData->mLength,sizeof(wchar_t));
		if(aNameIdx_aStr==0) instructions |= 0x100;
		else if(aNameIdx_aStr==1) instructions |= 0x200;
		else if(aNameIdx_aStr==2) instructions |= 0x400;
		else if(aNameIdx_aStr==3) instructions |= 0x800;
		else 
			throw ConfigObjectException("Unknown no overlap instruction: " + (std::string)aStr);
	}

	this->EnsureComma();
	Component *ref = this->ReadComponent(); this->EnsureComma();
	int horzPad, vertPad;
	horzPad = this->ReadInt(); this->EnsureComma();
	vertPad = this->ReadInt(); 

	SmartPtr<NoOverlapLayout> aLayout = new NoOverlapLayout(target,instructions,ref,horzPad,vertPad);
	while(!this->EndOfString())
	{
		this->EnsureComma();
		Component *aComponent = this->ReadComponent();
		aLayout->Add(aComponent);
	}

	this->mContainer->AddChildLayout(aLayout.get());

	return true;
}
// Creates a named layout point for each name in a comma-separated list, registers its config and adds it
// to the container
bool ContainerConfig::HandleNewLayoutPoint()
{
    while(true) {
        SmartPtr<ComponentConfig> config=new ComponentConfig;
        SmartPtr<LayoutPoint> point=new LayoutPoint;
        config->SetComponent(point.get());
        config->mParent=this;
        this->mConfigMap[this->ReadString(false)]=config;
        this->mContainer->AddLayoutPoint(point.get());
        if(this->EndOfString()) break;
        this->EnsureComma();
    }
    return true;
}
