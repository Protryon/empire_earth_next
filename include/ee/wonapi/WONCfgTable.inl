// The table reader's host (R6 WON round 39): the reads, the frame and the setters of the 49 table-shaped config
// handlers whose programs are ee_won's cfgtab.rs. Included inside a namespace by ConfigTableHost.cpp (WONAPI) and
// by tests/won/won_cfgtab_test.cpp (over its stand-ins), so both run these very statements. A read or setter that
// throws is caught and kept; ComponentConfig::RunTable rethrows it after the Rust returns.
struct CfgTabFrame {
    ComponentConfig* mConfig; std::exception_ptr mError;
    int i[8]; int ni; GUIString s[2]; int ns; SmartPtr<Font> f[1]; int nf; SmartPtr<Image> m[1]; int nm;
    Component* c[2]; int nc; SmartPtr<Sound> snd; Background bg; SelectionColor sel; WONFrame fr; SmartPtr<Cursor> cur;
};
// WONFrameValueConfig's frame value: a stretch image alone (its own borders), or an image and one border or four
static WONFrame CfgReadFrame(ComponentConfig* config)
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
static void CfgTabClear(void* p)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    a.ni=a.ns=a.nf=a.nm=a.nc=0; a.f[0]=NULL; a.m[0]=NULL;
}
static bool CfgTabRead(void* p,EeWonCfgRead inKind)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    ComponentConfig* cfg=a.mConfig;
    try {
        switch(inKind) {
        case EeWonCfgRead::Int: a.i[a.ni++]=cfg->ReadInt(); break;
        case EeWonCfgRead::Bool: a.i[a.ni++]=cfg->ReadBool(); break;
        case EeWonCfgRead::Color: a.i[a.ni++]=cfg->ReadColor(); break;
        case EeWonCfgRead::Char: a.i[a.ni++]=cfg->ReadChar(); break;
        case EeWonCfgRead::VertAlign: a.i[a.ni++]=cfg->ReadVertAlign(); break;
        case EeWonCfgRead::HorzAlign: a.i[a.ni++]=cfg->ReadHorzAlign(); break;
        case EeWonCfgRead::Str: a.s[a.ns++]=cfg->ReadString(false); break;
        case EeWonCfgRead::Flag: { bool on=true; a.s[a.ns++]=cfg->ReadFlag(&on); a.i[a.ni++]=on; break; }
        case EeWonCfgRead::Font: a.f[a.nf++]=cfg->ReadFont(); break;
        case EeWonCfgRead::Image: a.m[a.nm++]=cfg->ReadImage(false); break;
        case EeWonCfgRead::Sound: a.snd=cfg->ReadSound(); break;
        case EeWonCfgRead::Background: a.bg=cfg->ReadBackground(); break;
        case EeWonCfgRead::SelColor: a.sel=cfg->ReadSelectionColor(); break;
        case EeWonCfgRead::Comp: a.c[a.nc++]=cfg->ReadComponent(); break;
        case EeWonCfgRead::Container: a.c[a.nc++]=SafeConfigGetComponent<Container*>(cfg->ReadComponent(),"Container"); break;
        case EeWonCfgRead::Button: a.c[a.nc++]=SafeConfigGetComponent<Button*>(cfg->ReadComponent(),"Button"); break;
        case EeWonCfgRead::Scroller: a.c[a.nc++]=SafeConfigGetComponent<Scroller*>(cfg->ReadComponent(),"Scroller"); break;
        case EeWonCfgRead::MultiListArea: a.c[a.nc++]=SafeConfigGetComponent<MultiListArea*>(cfg->ReadComponent(),"MultiListArea"); break;
        case EeWonCfgRead::ScrollArea: {
            ScrollArea* area=dynamic_cast<ScrollArea*>(cfg->ReadComponent());
            if(area==NULL) throw ConfigObjectException("Expecting ScrollArea");
            a.c[a.nc++]=area; break;
        }
        case EeWonCfgRead::Frame: a.fr=CfgReadFrame(cfg); break;
        case EeWonCfgRead::Cursor: a.cur=cfg->ReadCursor(); break;
        case EeWonCfgRead::Direction: a.i[a.ni++]=cfg->ReadDirection(); break;
        case EeWonCfgRead::StdCursor: a.i[a.ni++]=cfg->ReadStandardCursor(); break;
        }
        return true;
    } catch(...) { a.mError=std::current_exception(); return false; }
}
static bool CfgTabComma(void* p)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    try { a.mConfig->EnsureComma(); return true; } catch(...) { a.mError=std::current_exception(); return false; }
}
static bool CfgTabEnd(void* p) { return static_cast<CfgTabFrame*>(p)->mConfig->EndOfString(); }
static const void* CfgTabText(void* p,int32_t inSlot,uint16_t* outLen,uint32_t* outUnit)
{
    const GUIString& s=static_cast<CfgTabFrame*>(p)->s[inSlot];
    *outLen=s.mData->mLength; *outUnit=sizeof(wchar_t);
    return s.mData->mText;
}
static void CfgTabPush(void* p,int32_t inValue) { CfgTabFrame& a=*static_cast<CfgTabFrame*>(p); a.i[a.ni++]=inValue; }
static void CfgTabFail(void* p,const uint8_t* inMsg,uintptr_t inLen,int32_t inSlot)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    std::string aText((const char*)inMsg,inLen);
    if(inSlot>=0) aText+=(std::string)a.s[inSlot];
    a.mError=std::make_exception_ptr(ConfigObjectException(aText));
}
enum {
    kButton=EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, kMSButton=EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY,
    kMSArrow=EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY, kMSTabButton=EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY,
    kMS3DFrame=EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY, kMSCombo=EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY,
    kMSScrollbar=EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY, kMSEtched=EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY,
    kMSTabCtrl=EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY, kMSScrollCont=EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,
    kMSHeader=EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY, kMSListCtrl=EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY,
    kPercent=EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION, kScheme=EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION,
    kRect=EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY, kRect3D=EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY,
    kImageComp=EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY, kScreen=EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY,
    kMultiList=EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, kMultiHeader=EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION,
    kScrollbar=EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, kTabCtrl=EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY,
    kBrowser=EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION, kCombo=EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY,
    kDialog=EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, kImageButton=EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION,
    kLabel=EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, kListArea=EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION,
    kInput=EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, kTextArea=EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY,
    kTree=EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION, kSimple=EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION,
    kWONButton=EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, kSplitter=EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION,
    kWScrollbar=EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY, kWHeader=EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY,
    kWListCtrl=EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, kWFrame=EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY,
    kWCombo=EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY, kWTabCtrl=EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY,
    kWScrollCont=EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, kScrollArea=EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION,
    kScroller=EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION, kSBScroller=EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION,
    kStretchCenter=EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION,
    kComponent=EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, kSkin=EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION,
    kFont=EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION, kImage=EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION
};
#define EE_CFG_SET(t,n) case (t)<<8|(n):
#define EE_CFG(T) static_cast<T*>(a.mConfig)
// The setters, as each handler called them on its typed component (bools read as 0/1).
static void CfgTabSet(CfgTabFrame& a,int inSetter)
{
    const int* i=a.i; const GUIString* s=a.s; Component* const* c=a.c;
    switch(inSetter) {
    EE_CFG_SET(kButton,0) EE_CFG(ButtonConfig)->mButton->SetButtonFlags(i[1],i[0]!=0); break;
    EE_CFG_SET(kButton,1) EE_CFG(ButtonConfig)->mButton->SetText(s[0],true); break;
    EE_CFG_SET(kButton,2) EE_CFG(ButtonConfig)->mButton->SetCheck(i[0]!=0,false); break;
    EE_CFG_SET(kButton,3) EE_CFG(ButtonConfig)->mButton->SetClickSound(a.snd.get()); break;
    EE_CFG_SET(kButton,4) EE_CFG(ButtonConfig)->HandleFitText(); break;
    EE_CFG_SET(kMSButton,0) EE_CFG(MSButtonConfig)->mMSButton->SetFont(a.f[0]); break;
    EE_CFG_SET(kMSButton,1) EE_CFG(MSButtonConfig)->mMSButton->SetTextColor(i[0]); break;
    EE_CFG_SET(kMSButton,2) EE_CFG(MSButtonConfig)->mMSButton->SetVertAlign((VertAlign)i[0]); break;
    EE_CFG_SET(kMSButton,3) EE_CFG(MSButtonConfig)->mMSButton->SetHorzAlign((HorzAlign)i[0]); break;
    EE_CFG_SET(kMSButton,4) EE_CFG(MSButtonConfig)->mMSButton->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kMSButton,5) EE_CFG(MSButtonConfig)->mMSButton->SetAdjustHeightToFitText(i[0]!=0); break;
    EE_CFG_SET(kMSButton,6) EE_CFG(MSButtonConfig)->mMSButton->SetWrap(i[0]!=0); break;
    EE_CFG_SET(kMSButton,7) EE_CFG(MSButtonConfig)->mMSButton->IncrementHorzTextPad(i[0],i[1]); break;
    EE_CFG_SET(kMSButton,8) EE_CFG(MSButtonConfig)->mMSButton->IncrementVertTextPad(i[0],i[1]); break;
    EE_CFG_SET(kMSArrow,0) EE_CFG(MSArrowButtonConfig)->mMSArrowButton->SetDirection((Direction)i[0]); break;
    EE_CFG_SET(kMSTabButton,0) EE_CFG(MSTabButtonConfig)->mMSTabButton->SetLowerAmount(i[0]); break;
    EE_CFG_SET(kMS3DFrame,0) EE_CFG(MS3DFrameConfig)->mMS3DFrame->SetFrameType(i[0]); break;
    EE_CFG_SET(kMS3DFrame,1) EE_CFG(MS3DFrameConfig)->mMS3DFrame->SetIsUp(i[0]!=0); break;
    EE_CFG_SET(kMSCombo,0) EE_CFG(MSComboBoxConfig)->mMSComboBox->SetDrawFrame(i[0]!=0); break;
    EE_CFG_SET(kMSScrollbar,0) EE_CFG(MSScrollbarConfig)->mMSScrollbar->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kMSEtched,0) EE_CFG(MSEtchedFrameGroupConfig)->mFrame->SetLabelText(s[0]); break;
    EE_CFG_SET(kMSEtched,1) EE_CFG(MSEtchedFrameGroupConfig)->mFrame->AddContainedComponent(c[0]); break;
    EE_CFG_SET(kMSTabCtrl,0) EE_CFG(MSTabCtrlConfig)->mMSTabCtrl->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kMSTabCtrl,1) EE_CFG(MSTabCtrlConfig)->mMSTabCtrl->AddStandardTab(s[0],static_cast<Container*>(c[0])); break;
    EE_CFG_SET(kMSScrollCont,0) EE_CFG(MSScrollContainerConfig)->mScrollContainer->SetDrawFrame(i[0]!=0); break;
    EE_CFG_SET(kMSScrollCont,1) EE_CFG(MSScrollContainerConfig)->mScrollContainer->SetDraw3DFrame(i[0]!=0); break;
    EE_CFG_SET(kMSScrollCont,2) EE_CFG(MSScrollContainerConfig)->mScrollContainer->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kMSHeader,0) EE_CFG(MSHeaderControlConfig)->mMSHeader->SetColumn(i[0],s[0]); break;
    EE_CFG_SET(kMSListCtrl,0) EE_CFG(MSListCtrlConfig)->mListCtrl->SetHeaderHeight(i[0]); break;
    EE_CFG_SET(kPercent,0) EE_CFG(PercentLayoutConfig)->refs[i[0]]=c[0]; break;
    EE_CFG_SET(kPercent,1) EE_CFG(PercentLayoutConfig)->params[i[0]]=i[1]; break;
    EE_CFG_SET(kScheme,0) EE_CFG(ColorSchemeConfig)->mColorScheme.get()->SetStandardColor(i[0],(DWORD)i[1]); break;
    EE_CFG_SET(kRect,0) EE_CFG(RectangleComponentConfig)->mRectangleComponent->SetSolidColor(i[0]); break;
    EE_CFG_SET(kRect,1) EE_CFG(RectangleComponentConfig)->mRectangleComponent->SetOutlineColor(i[0]); break;
    EE_CFG_SET(kRect3D,0) EE_CFG(RectangleComponent3DConfig)->mRectangleComponent3D->SetSolidColor(i[0]); break;
    EE_CFG_SET(kRect3D,1) EE_CFG(RectangleComponent3DConfig)->mRectangleComponent3D->SetLightColor(i[0]); break;
    EE_CFG_SET(kRect3D,2) EE_CFG(RectangleComponent3DConfig)->mRectangleComponent3D->SetDarkColor(i[0]); break;
    EE_CFG_SET(kRect3D,3) EE_CFG(RectangleComponent3DConfig)->mRectangleComponent3D->SetIsUp(i[0]!=0); break;
    EE_CFG_SET(kRect3D,4) EE_CFG(RectangleComponent3DConfig)->mRectangleComponent3D->SetThickness(i[0]); break;
    EE_CFG_SET(kImageComp,0) EE_CFG(ImageComponentConfig)->mImageComponent->SetImage(a.m[0]); break;
    EE_CFG_SET(kImageComp,1) EE_CFG(ImageComponentConfig)->mImageComponent->mTile=i[0]!=0; break;
    EE_CFG_SET(kImageComp,2) EE_CFG(ImageComponentConfig)->mImageComponent->mStretch=i[0]!=0; break;
    EE_CFG_SET(kScreen,0) EE_CFG(ScreenContainerConfig)->mScreenContainer->AddScreen(c[0]); break;
    EE_CFG_SET(kScreen,1) EE_CFG(ScreenContainerConfig)->mScreenContainer->ShowScreen(c[0],true); break;
    EE_CFG_SET(kMultiList,0) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetSortColumn(i[0],(ListSortType)i[1]); break;
    EE_CFG_SET(kMultiList,1) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetColumnWidth(i[0],i[1],false); break;
    EE_CFG_SET(kMultiList,2) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetNumColumns(i[0]); break;
    EE_CFG_SET(kMultiList,3) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetDynamicSizeColumn(i[0]); break;
    EE_CFG_SET(kMultiList,4) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetSortColumnFunc(i[0],ListArea::NumberSortFunc); break;
    EE_CFG_SET(kMultiList,5) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetAutoGrowColumns(i[0]!=0); break;
    EE_CFG_SET(kMultiList,6) EE_CFG(MultiListAreaConfig)->mMultiListArea->SetColumnHAlign(i[0],(HorzAlign)i[1]); break;
    EE_CFG_SET(kMultiHeader,0) EE_CFG(MultiListHeaderConfig)->mHeader->SetColumnItem(i[0],c[0]); break;
    EE_CFG_SET(kMultiHeader,1) EE_CFG(MultiListHeaderConfig)->mHeader->SetMultiListArea(static_cast<MultiListArea*>(c[0])); break;
    EE_CFG_SET(kScrollbar,0) EE_CFG(ScrollbarConfig)->mScrollbar->SetVertical(i[0]!=0); break;
    EE_CFG_SET(kScrollbar,1) EE_CFG(ScrollbarConfig)->mScrollbar->SetResizeThumb(i[0]!=0); break;
    EE_CFG_SET(kScrollbar,2) EE_CFG(ScrollbarConfig)->mScrollbar->SetMinThumbSize((unsigned char)i[0]); break;
    EE_CFG_SET(kTabCtrl,0) EE_CFG(TabCtrlConfig)->mTabCtrl->AddTab(static_cast<Button*>(c[0]),static_cast<Container*>(c[1])); break;
    EE_CFG_SET(kTabCtrl,1) EE_CFG(TabCtrlConfig)->mTabCtrl->SetTabBarOnRight(i[0]!=0); break;
    EE_CFG_SET(kTabCtrl,2) EE_CFG(TabCtrlConfig)->mTabCtrl->SetArrowsOnLeft(i[0]!=0); break;
    EE_CFG_SET(kTabCtrl,3) EE_CFG(TabCtrlConfig)->mTabCtrl->SetTabBarHeight(i[0]); break;
    EE_CFG_SET(kTabCtrl,4) EE_CFG(TabCtrlConfig)->mTabCtrl->SetTabOverlap(i[0]); break;
    EE_CFG_SET(kTabCtrl,5) EE_CFG(TabCtrlConfig)->mTabCtrl->SetTabBarOverlap(i[0]); break;
    EE_CFG_SET(kTabCtrl,6) EE_CFG(TabCtrlConfig)->mTabCtrl->SetBottomTabTopOffset(i[0]); break;
    EE_CFG_SET(kTabCtrl,7) EE_CFG(TabCtrlConfig)->mTabCtrl->SetBottomTabLeftOffset(i[0]); break;
    EE_CFG_SET(kBrowser,0) EE_CFG(BrowserComponentConfig)->mBrowserComponent->SetTransparent(true); break;
    EE_CFG_SET(kBrowser,1) EE_CFG(BrowserComponentConfig)->mBrowserComponent->SetScroller(static_cast<Scroller*>(c[0])); break;
    EE_CFG_SET(kBrowser,2) EE_CFG(BrowserComponentConfig)->mBrowserComponent->SetFontSize((HTMLFontSize)i[0]); break;
    EE_CFG_SET(kCombo,0) EE_CFG(ComboBoxConfig)->mComboBox->SetListBoxWidth(i[0]); break;
    EE_CFG_SET(kCombo,1) EE_CFG(ComboBoxConfig)->mComboBox->SetListBoxHeight(i[0]); break;
    EE_CFG_SET(kCombo,2) EE_CFG(ComboBoxConfig)->mComboBox->SetListBoxDimensions(i[0],i[1]); break;
    EE_CFG_SET(kCombo,3) EE_CFG(ComboBoxConfig)->mComboBox->SetText(s[0],true); break;
    EE_CFG_SET(kCombo,4) EE_CFG(ComboBoxConfig)->mComboBox->AddString(s[0],-1); break;
    EE_CFG_SET(kCombo,5) EE_CFG(ComboBoxConfig)->mComboBox->SetSelItem(i[0],true); break;
    EE_CFG_SET(kCombo,6) EE_CFG(ComboBoxConfig)->mComboBox->SetNoType(i[0]!=0); break;
    EE_CFG_SET(kCombo,7) EE_CFG(ComboBoxConfig)->mComboBox->mEdit.get()->SetMaxChars(i[0]); break;
    EE_CFG_SET(kCombo,8) EE_CFG(ComboBoxConfig)->mComboBox->SetShowButtonWhenDisabled(i[0]!=0); break;
    EE_CFG_SET(kDialog,0) EE_CFG(DialogConfig)->mDialog->SetAllowDrag(i[0]!=0); break;
    EE_CFG_SET(kDialog,1) EE_CFG(DialogConfig)->mDialog->SetKeepInRoot(i[0]!=0); break;
    EE_CFG_SET(kDialog,2) EE_CFG(DialogConfig)->mDialog->SetEndOnEscape(i[0]); break;
    EE_CFG_SET(kDialog,3) EE_CFG(DialogConfig)->mDialog->SetDontEndOnEscape(); break;
    EE_CFG_SET(kDialog,4) EE_CFG(DialogConfig)->mDialog->SetDefaultButton(static_cast<Button*>(c[0])); break;
    EE_CFG_SET(kImageButton,0) EE_CFG(ImageButtonConfig)->mImageButton->SetImage((ImageButtonType)i[0],a.m[0].get()); break;
    EE_CFG_SET(kLabel,0) EE_CFG(LabelConfig)->mLabel->SetText(s[0],true); break;
    EE_CFG_SET(kLabel,1) EE_CFG(LabelConfig)->mLabel->SetFont(a.f[0]); break;
    EE_CFG_SET(kLabel,2) EE_CFG(LabelConfig)->mLabel->SetTextColor(i[0]); break;
    EE_CFG_SET(kLabel,3) EE_CFG(LabelConfig)->mLabel->SetBackground(a.bg); break;
    EE_CFG_SET(kLabel,4) EE_CFG(LabelConfig)->mLabel->SetVertAlign((VertAlign)i[0]); break;
    EE_CFG_SET(kLabel,5) EE_CFG(LabelConfig)->mLabel->SetHorzAlign((HorzAlign)i[0]); break;
    EE_CFG_SET(kLabel,6) EE_CFG(LabelConfig)->mLabel->SetWrap(i[0]!=0); break;
    EE_CFG_SET(kLabel,7) EE_CFG(LabelConfig)->mLabel->SetAdjustHeightToFitText(i[0]!=0); break;
    EE_CFG_SET(kListArea,0) EE_CFG(ListAreaConfig)->mListArea->SetListFlags((unsigned long)i[1],i[0]!=0); break;
    EE_CFG_SET(kListArea,1) EE_CFG(ListAreaConfig)->mListArea->InsertString(s[0],-1); break;
    EE_CFG_SET(kListArea,2) EE_CFG(ListAreaConfig)->mListArea->SetFont(a.f[0]); break;
    EE_CFG_SET(kListArea,3) EE_CFG(ListAreaConfig)->mListArea->mTextColor=i[0]; break;
    EE_CFG_SET(kListArea,4) EE_CFG(ListAreaConfig)->mListArea->mSelColor=a.sel; break;
    EE_CFG_SET(kListArea,5) EE_CFG(ListAreaConfig)->mListArea->SetSortType((ListSortType)i[0]); break;
    EE_CFG_SET(kInput,0) EE_CFG(InputBoxConfig)->mInputBox->SetText(s[0],true); break;
    EE_CFG_SET(kInput,1) EE_CFG(InputBoxConfig)->mInputBox->SetPasswordChar(i[0]); break;
    EE_CFG_SET(kInput,2) EE_CFG(InputBoxConfig)->mInputBox->SetAllowSelection(i[0]!=0); break;
    EE_CFG_SET(kInput,3) EE_CFG(InputBoxConfig)->mInputBox->SetMaxChars(i[0]); break;
    EE_CFG_SET(kInput,4) EE_CFG(InputBoxConfig)->mInputBox->SetCharWidth(i[0]); break;
    EE_CFG_SET(kInput,5) EE_CFG(InputBoxConfig)->mInputBox->SetBackground(a.bg); break;
    EE_CFG_SET(kInput,6) EE_CFG(InputBoxConfig)->mInputBox->SetDisabledBackground(a.bg); break;
    EE_CFG_SET(kInput,7) EE_CFG(InputBoxConfig)->mInputBox->SetIcon(a.m[0].get()); break;
    EE_CFG_SET(kInput,8) EE_CFG(InputBoxConfig)->mInputBox->mTextColor=i[0]; break;
    EE_CFG_SET(kInput,9) EE_CFG(InputBoxConfig)->mInputBox->mDisabledTextColor=i[0]; break;
    EE_CFG_SET(kInput,10) EE_CFG(InputBoxConfig)->mInputBox->mSelColor=a.sel; break;
    EE_CFG_SET(kInput,11) EE_CFG(InputBoxConfig)->mInputBox->SetSelectOnFocus(i[0]!=0); break;
    EE_CFG_SET(kInput,12) EE_CFG(InputBoxConfig)->mInputBox->SetMaxInputHistory(i[0]); break;
    EE_CFG_SET(kInput,13) EE_CFG(InputBoxConfig)->mInputBox->mUseDisabledColors=i[0]!=0; break;
    EE_CFG_SET(kInput,14) EE_CFG(InputBoxConfig)->mInputBox->SetCursorBlinkRate(i[0],a.ni>1?i[1]:i[0]); break;
    EE_CFG_SET(kTextArea,0) EE_CFG(TextAreaConfig)->mTextArea->SetTextFlags(i[1],i[0]!=0); break;
    EE_CFG_SET(kTextArea,1) EE_CFG(TextAreaConfig)->mTextArea->AddSegment(s[0],false); break;
    EE_CFG_SET(kTextArea,2) EE_CFG(TextAreaConfig)->mTextArea->AddFormatedText(s[0],false); break;
    EE_CFG_SET(kTextArea,3) EE_CFG(TextAreaConfig)->mTextArea->SetFont(a.f[0]); break;
    EE_CFG_SET(kTextArea,4) EE_CFG(TextAreaConfig)->mTextArea->mTextColor=i[0]; break;
    EE_CFG_SET(kTextArea,5) EE_CFG(TextAreaConfig)->mTextArea->mSelColor=a.sel; break;
    EE_CFG_SET(kTextArea,6) EE_CFG(TextAreaConfig)->mTextArea->SetLinkFont(a.f[0]); break;
    EE_CFG_SET(kTextArea,7) EE_CFG(TextAreaConfig)->mTextArea->SetMaxChars(i[0]); break;
    EE_CFG_SET(kTextArea,8) EE_CFG(TextAreaConfig)->mTextArea->SetMaxLines(i[0]); break;
    EE_CFG_SET(kTextArea,9) EE_CFG(TextAreaConfig)->mTextArea->SetWrapIndentSize(i[0]); break;
    EE_CFG_SET(kTextArea,10) EE_CFG(TextAreaConfig)->mTextArea->SetLineAlignment((HorzAlign)i[0]); break;
    EE_CFG_SET(kTree,0) EE_CFG(TreeAreaConfig)->mTreeArea->SetSortType((ListSortType)i[0]); break;
    EE_CFG_SET(kSimple,0) EE_CFG(WONSimpleButtonConfig)->mButton->SetScaleImage(i[0]!=0); break;
    EE_CFG_SET(kSimple,1)
        if(!gSkin->CopySimpleButtonType(s[0],EE_CFG(WONSimpleButtonConfig)->mButton))
            throw ConfigObjectException("Type not found: " + std::string(s[0]));
        break;
    EE_CFG_SET(kSimple,2) EE_CFG(WONSimpleButtonConfig)->mButton->SetImage(i[0],a.m[0].get()); break;
    EE_CFG_SET(kSimple,3) EE_CFG(WONSimpleButtonConfig)->mButton->SetCheckMouseButtonDown(i[0]!=0); break;
    EE_CFG_SET(kWONButton,0) EE_CFG(WONButtonConfig)->mButton->SetFont(a.f[0].get()); break;
    EE_CFG_SET(kWONButton,1)
        if(!gSkin->CopyButtonType(s[0],EE_CFG(WONButtonConfig)->mButton))
            throw ConfigObjectException("Type not found: " + std::string(s[0]));
        break;
    EE_CFG_SET(kWONButton,2) EE_CFG(WONButtonConfig)->mButton->SetTextColor(i[0]); break;
    EE_CFG_SET(kWONButton,3) EE_CFG(WONButtonConfig)->mButton->SetDefaultTextColor(i[0]); break;
    EE_CFG_SET(kWONButton,4) EE_CFG(WONButtonConfig)->mButton->SetVertAlign((VertAlign)i[0]); break;
    EE_CFG_SET(kWONButton,5) EE_CFG(WONButtonConfig)->mButton->SetHorzAlign((HorzAlign)i[0]); break;
    EE_CFG_SET(kWONButton,6) EE_CFG(WONButtonConfig)->mButton->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kWONButton,7) EE_CFG(WONButtonConfig)->mButton->SetAdjustHeightToFitText(i[0]!=0); break;
    EE_CFG_SET(kWONButton,8) EE_CFG(WONButtonConfig)->mButton->SetWrapText(i[0]!=0); break;
    EE_CFG_SET(kWONButton,9) EE_CFG(WONButtonConfig)->mButton->SetDrawGrayText(i[0]!=0); break;
    EE_CFG_SET(kWONButton,10) EE_CFG(WONButtonConfig)->mButton->SetTextPadding(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kWONButton,11) EE_CFG(WONButtonConfig)->mButton->SetFocusPadding(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kWONButton,12) { WONButton* b=EE_CFG(WONButtonConfig)->mButton; b->SetHorzTextPad(i[0],a.ni>1?i[1]:b->GetRightTextPad()); break; }
    EE_CFG_SET(kWONButton,15) EE_CFG(WONButtonConfig)->mButton->SetButtonState(i[0]&0xff,a.m[0].get(),a.ni>1?i[1]:-1,a.nf>0?a.f[0].get():NULL,(i[0]&0x100)!=0); break;
    EE_CFG_SET(kWONButton,16) EE_CFG(WONButtonConfig)->mButton->SetScaleImage(i[0]!=0); break;
    EE_CFG_SET(kWONButton,17) EE_CFG(WONButtonConfig)->mButton->SetFocusStyle(i[0]!=0?1:0); break;
    EE_CFG_SET(kSplitter,0) EE_CFG(SplitterConfig)->mSplitter->AddQuadrant(c[0],(Splitter::QuadrantPosition)i[0]); break;
    EE_CFG_SET(kSplitter,1) EE_CFG(SplitterConfig)->mSplitter->AddSplitter(c[0],(Splitter::SplitterType)i[0]); break;
    EE_CFG_SET(kSplitter,2) EE_CFG(SplitterConfig)->mSplitter->AddInvisibleSplitter((Splitter::SplitterType)i[0],a.ni>1?i[1]:10); break;
    EE_CFG_SET(kSplitter,3) EE_CFG(SplitterConfig)->mSplitter->AddComplexSplitter(static_cast<Container*>(c[0]),(Splitter::SplitterType)i[0],a.nc>1?c[1]:NULL); break;
    EE_CFG_SET(kSplitter,4) EE_CFG(SplitterConfig)->mSplitter->SetMinLeftRight(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,5) EE_CFG(SplitterConfig)->mSplitter->SetMinTopBottom(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,6) EE_CFG(SplitterConfig)->mSplitter->SetPercentages(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,7) EE_CFG(SplitterConfig)->mSplitter->MaxQuadrant((Splitter::QuadrantPosition)i[0]); break;
    EE_CFG_SET(kSplitter,8) EE_CFG(SplitterConfig)->mSplitter->UnMaxQuadrant(); break;
    EE_CFG_SET(kSplitter,9) EE_CFG(SplitterConfig)->mSplitter->SetSplitterPos(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,10) EE_CFG(SplitterConfig)->mSplitter->RegisterMaxButton(static_cast<Button*>(c[0]),(Splitter::QuadrantPosition)i[0]); break;
    EE_CFG_SET(kWScrollbar,0) if(i[0]!=0) { Button* rect=new Button; EE_CFG(WONScrollbarConfig)->mScrollbar->SetScrollRect(rect); } break;
    EE_CFG_SET(kWHeader,0) EE_CFG(WONHeaderControlConfig)->mHeader->SetColumn(i[0],s[0],NULL); break;
    EE_CFG_SET(kWListCtrl,0) EE_CFG(WONListCtrlConfig)->mListCtrl->mHeaderHeight=i[0]; break;
    EE_CFG_SET(kWListCtrl,1) static_cast<MultiListArea*>(EE_CFG(WONListCtrlConfig)->mListCtrl->GetScrollArea())->SetNumColumns(i[0]); break;
    EE_CFG_SET(kWListCtrl,2) static_cast<MultiListArea*>(EE_CFG(WONListCtrlConfig)->mListCtrl->GetScrollArea())->SetDynamicSizeColumn(i[0]); break;
    EE_CFG_SET(kWListCtrl,3) static_cast<ListArea*>(static_cast<MultiListArea*>(EE_CFG(WONListCtrlConfig)->mListCtrl->GetScrollArea()))->SetListFlags((unsigned long)i[1],i[0]!=0); break;
    EE_CFG_SET(kWListCtrl,4) EE_CFG(WONListCtrlConfig)->mListCtrl->SetStandarColumnHeader(i[0],s[0],i[1]); break;
    EE_CFG_SET(kWListCtrl,5) EE_CFG(WONListCtrlConfig)->mListCtrl->SetColumnHeader(i[0],c[0],i[1]); break;
    EE_CFG_SET(kWFrame,0) EE_CFG(WONFrameComponentConfig)->mFrameComponent->SetFrameLeft(i[0]); break;
    EE_CFG_SET(kWFrame,1) EE_CFG(WONFrameComponentConfig)->mFrameComponent->SetFrameTop(i[1]); break;
    EE_CFG_SET(kWFrame,2) EE_CFG(WONFrameComponentConfig)->mFrameComponent->SetFrameRight(i[2]); break;
    EE_CFG_SET(kWFrame,3) EE_CFG(WONFrameComponentConfig)->mFrameComponent->SetFrameBottom(i[3]); break;
    EE_CFG_SET(kWFrame,4) EE_CFG(WONFrameComponentConfig)->mFrameComponent->SetFrameImage(a.m[0].get()); break;
    EE_CFG_SET(kWCombo,0) EE_CFG(WONComboBoxConfig)->mComboBox->mDrawFrame=i[0]!=0; break;
    EE_CFG_SET(kWCombo,1) EE_CFG(WONComboBoxConfig)->mComboBox->mButtonInFrame=i[0]!=0; break;
    EE_CFG_SET(kWCombo,2) EE_CFG(WONComboBoxConfig)->mComboBox->SetFrame(a.fr); break;
    EE_CFG_SET(kWTabCtrl,0) EE_CFG(WONTabCtrlConfig)->mTabCtrl->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kWTabCtrl,1) EE_CFG(WONTabCtrlConfig)->mTabCtrl->AddStandardTab(s[0],static_cast<Container*>(c[0])); break;
    EE_CFG_SET(kWTabCtrl,2) EE_CFG(WONTabCtrlConfig)->mTabCtrl->SetFrame(a.fr); break;
    EE_CFG_SET(kWScrollCont,0) if(i[0]==0) EE_CFG(WONScrollContainerConfig)->mScrollContainer->SetFrame(WONFrame()); break;
    EE_CFG_SET(kWScrollCont,1) EE_CFG(WONScrollContainerConfig)->mScrollContainer->mTransparent=i[0]!=0; break;
    EE_CFG_SET(kWScrollCont,2) EE_CFG(WONScrollContainerConfig)->mScrollContainer->SetFrame(a.fr); break;
    EE_CFG_SET(kWScrollCont,3) EE_CFG(WONScrollContainerConfig)->mScrollContainer->SetCorner(a.m[0]); break;
    EE_CFG_SET(kScrollArea,0) EE_CFG(ScrollAreaConfig)->mScrollArea->SetBackground(a.bg); break;
    EE_CFG_SET(kScrollArea,1) EE_CFG(ScrollAreaConfig)->mScrollArea->SetBorderPadding(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kScroller,0) EE_CFG(ScrollerConfig)->mScroller->SetScrollArea(static_cast<ScrollArea*>(c[0])); break;
    EE_CFG_SET(kSBScroller,0) EE_CFG(ScrollbarScrollerConfig)->mScrollbarScroller->SetScrollbarConditions((ScrollbarCondition)i[0],(ScrollbarCondition)i[1]); break;
    EE_CFG_SET(kStretchCenter,0) EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetStretchImage(a.m[0].get()); break;
    EE_CFG_SET(kStretchCenter,1) EE_CFG(StretchCenterImageConfig)->mStretchCenterImage->SetCenterImage(a.m[0].get()); break;
    EE_CFG_SET(kComponent,2) a.mConfig->mComponent.get()->mControlId=i[0]; break;
    EE_CFG_SET(kComponent,3) { Component* p=a.mConfig->mComponent.get(); p->SetPos(i[0],p->Top()); break; }
    EE_CFG_SET(kComponent,4) { Component* p=a.mConfig->mComponent.get(); p->SetPos(p->Left(),i[0]); break; }
    EE_CFG_SET(kComponent,5) { Component* p=a.mConfig->mComponent.get(); p->SetSize(i[0],p->Height()); break; }
    EE_CFG_SET(kComponent,6) { Component* p=a.mConfig->mComponent.get(); p->SetSize(p->Width(),i[0]); break; }
    EE_CFG_SET(kComponent,7) a.mConfig->mComponent->SetPos(i[0],i[1]); break;
    EE_CFG_SET(kComponent,8) a.mConfig->mComponent->SetSize(i[0],i[1]); break;
    EE_CFG_SET(kComponent,9) a.mConfig->mComponent->SetTranslucentColor(i[0],a.ni>1?i[1]:4); break;
    EE_CFG_SET(kComponent,10)
        a.mConfig->mHasSetWidth=i[0]>0; a.mConfig->mHasSetHeight=i[1]>0;
        a.mConfig->mComponent->SetSizeAtLeast(i[0],i[1]); break;
    EE_CFG_SET(kComponent,11) a.mConfig->mComponent->SetDesiredSize(i[0],i[1]); break;
    EE_CFG_SET(kComponent,12) a.mConfig->mComponent.get()->SetDesiredWidth(i[0]); break;
    EE_CFG_SET(kComponent,13) a.mConfig->mComponent.get()->SetDesiredHeight(i[0]); break;
    EE_CFG_SET(kComponent,14) a.mConfig->mComponent->SetPosSize(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kComponent,15) a.mConfig->mComponent->SetEnabled(i[0]!=0); break;
    EE_CFG_SET(kComponent,16) a.mConfig->mComponent->SetVisible(i[0]!=0); break;
    EE_CFG_SET(kComponent,17) if(i[0]!=0) a.mConfig->mComponent->RequestFocus(); break;
    EE_CFG_SET(kComponent,18) a.mConfig->mComponent->SetDefaultCursor(a.cur); break;
    EE_CFG_SET(kComponent,19) a.mConfig->mComponent->SetComponentFlags(i[1],i[0]!=0); break;
    EE_CFG_SET(kComponent,20) Window::GetDefaultWindow()->SetSkipDecodeImages(!(i[0]!=0)); break;
    EE_CFG_SET(kComponent,21) gShowParseTime=i[0]!=0; break;
    EE_CFG_SET(kComponent,22) ConfigParser::ResetLineCount(); PerfTimer::GlobalStart(NULL); break;
    EE_CFG_SET(kComponent,23) PerfTimer::GlobalStop(NULL); gLinesParsed=ConfigParser::GetLineCount(); break;
    EE_CFG_SET(kComponent,30) a.mConfig->mHasSetWidth=true; break;
    EE_CFG_SET(kComponent,31) a.mConfig->mHasSetHeight=true; break;
    EE_CFG_SET(kComponent,32) a.mConfig->mHasSetWidth=a.mConfig->mHasSetHeight=true; break;
    EE_CFG_SET(kComponent,33) a.mConfig->SetDesiredSizeIfNecessary(); break;
    EE_CFG_SET(kSkin,1) gSkin->mButtonTypeMap[s[0]]=SafeConfigGetComponent<WONButton*>(c[0],"WONButton"); break;
    EE_CFG_SET(kSkin,2) gSkin->mSimpleButtonTypeMap[s[0]]=SafeConfigGetComponent<WONSimpleButton*>(c[0],"WONSimpleButton"); break;
    EE_CFG_SET(kSkin,3) gSkin->mPushButton=SafeConfigGetComponent<WONButton*>(c[0],"WONButton"); break;
    EE_CFG_SET(kSkin,4) gSkin->mCheckbox=SafeConfigGetComponent<WONButton*>(c[0],"WONButton"); break;
    EE_CFG_SET(kSkin,5)
        gSkin->mRadioButton=SafeConfigGetComponent<WONButton*>(c[0],"WONButton");
        gSkin->mRadioButton->SetButtonFlags(8,true); break;
    EE_CFG_SET(kSkin,6) gSkin->mHeaderButton=SafeConfigGetComponent<WONHeaderButton*>(c[0],"WONHeaderButton"); break;
    EE_CFG_SET(kSkin,7) gSkin->mTabButton=SafeConfigGetComponent<WONButton*>(c[0],"WONButton"); break;
    EE_CFG_SET(kSkin,8) gSkin->mArrowButton[i[0]]=SafeConfigGetComponent<WONSimpleButton*>(c[0],"WONSimpleButton"); break;
    EE_CFG_SET(kSkin,9) gSkin->mComboButton=SafeConfigGetComponent<WONSimpleButton*>(c[0],"WONSimpleButton"); break;
    EE_CFG_SET(kSkin,10) gSkin->mCloseButton=SafeConfigGetComponent<WONSimpleButton*>(c[0],"WONSimpleButton"); break;
    EE_CFG_SET(kSkin,11) gSkin->mScrollGutter[i[0]?1:0]=SafeConfigGetComponent<WONSimpleButton*>(c[0],"WONSimpleButton"); break;
    EE_CFG_SET(kSkin,12) gSkin->mScrollThumb[i[0]?1:0]=SafeConfigGetComponent<WONSimpleButton*>(c[0],"WONSimpleButton"); break;
    EE_CFG_SET(kSkin,13) gSkin->mScrollbar[i[0]?1:0]=SafeConfigGetComponent<WONScrollbar*>(c[0],"WONScrollbar"); break;
    EE_CFG_SET(kSkin,14) gSkin->mScrollContainer=SafeConfigGetComponent<WONScrollContainer*>(c[0],"WONScrollContainer"); break;
    EE_CFG_SET(kSkin,15) gSkin->mListCtrl=SafeConfigGetComponent<WONListCtrl*>(c[0],"WONListCtrl"); break;
    EE_CFG_SET(kSkin,16)
        gSkin->mTabCtrl=SafeConfigGetComponent<WONTabCtrl*>(c[0],"WONTabCtrl");
        gSkin->mTabCtrl->mSkin=NULL; break;
    EE_CFG_SET(kSkin,17) gSkin->mComboBox=SafeConfigGetComponent<WONComboBox*>(c[0],"WONComboBox"); break;
    EE_CFG_SET(kSkin,18) gSkin->mPopupBack=SafeConfigGetComponent<WONFrameComponent*>(c[0],"WONPopupBack"); break;
    EE_CFG_SET(kSkin,19) gSkin->mDialogBack=SafeConfigGetComponent<WONFrameComponent*>(c[0],"WONDialogBack"); break;
    EE_CFG_SET(kSkin,20) EE_CFG(WONSkinConfig)->HandleButtonClickSound(); break;
    EE_CFG_SET(kSkin,21) {
        SmartPtr<Cursor> aCursor=new ComponentCursor(new ImageComponent(a.m[0].get()));
        Cursor::SetStandardCursor((StandardCursor)i[0],aCursor.get()); break;
    }
    EE_CFG_SET(kFont,0) EE_CFG(FontConfig)->SetFont(a.f[0].get()); break;
    EE_CFG_SET(kFont,1) EE_CFG(FontConfig)->mDescriptor.mSize=i[0]; break;
    EE_CFG_SET(kFont,2)
        if(i[0]!=0) EE_CFG(FontConfig)->mDescriptor.mStyle|=i[1];
        else EE_CFG(FontConfig)->mDescriptor.mStyle&=~i[1];
        break;
    EE_CFG_SET(kFont,3) WindowManager::GetDefaultWindowManager()->SetDefaultFont(a.f[0].get()); break;
    EE_CFG_SET(kFont,4) WindowManager::GetDefaultWindowManager()->SetNamedFont(s[0],a.f[0].get()); break;
    EE_CFG_SET(kFont,5) EE_CFG(FontConfig)->mNeedRecalcFont=true; break;
    EE_CFG_SET(kImage,0) EE_CFG(ImageConfig)->mImage=a.m[0]; break;
    EE_CFG_SET(kImage,1) EE_CFG(ImageConfig)->mImage->SetSize(i[0],i[1]); break;
    default: break;
    }
}
#undef EE_CFG_SET
#undef EE_CFG
static bool CfgTabApply(void* p,int32_t inSetter)
{
    CfgTabFrame& a=*static_cast<CfgTabFrame*>(p);
    try { CfgTabSet(a,inSetter); return true; } catch(...) { a.mError=std::current_exception(); return false; }
}
// The handler of inTable for inKey: true when the table holds the key (and the key was handled), false to ask the
// base; a read's or setter's exception is rethrown here.
bool ComponentConfig::RunTable(int inTable,const std::string& inKey)
{
    CfgTabFrame aFrame; aFrame.mConfig=this; aFrame.ni=aFrame.ns=aFrame.nf=aFrame.nm=aFrame.nc=0;
    EeWonCfgTabHost aHost={&aFrame,CfgTabRead,CfgTabComma,CfgTabEnd,CfgTabText,CfgTabPush,CfgTabApply,CfgTabFail,CfgTabClear};
    int aResult=ee_won_cfg_tab_run(&aHost,inTable,(const uint8_t*)inKey.data(),inKey.size());
    if(aResult==1) std::rethrow_exception(aFrame.mError);
    return aResult==0;
}
// ComponentConfig's own instructions: MODIFY and MAPCOMPONENT answer through their handlers, the rest is the table
bool ComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) return this->HandleModify();
    if(aKeyIdx==1) return this->HandleMapComponent();
    return this->RunTable(kComponent,key);
}
bool WONSkinConfig::Virtual24(const std::string& key) { return this->RunTable(kSkin,key); }
bool FontConfig::Virtual24(const std::string& key) { return this->RunTable(kFont,key); }
bool ImageConfig::Virtual24(const std::string& key) { return this->RunTable(kImage,key); }
// The handlers: the table, then the base (WONHeaderControlConfig asked its base first; the percent layout and the
// colour scheme had none).
#define EE_CFG_V24(T,k,B) bool T::Virtual24(const std::string& key) { return this->RunTable(k,key) || B::Virtual24(key); }
EE_CFG_V24(ButtonConfig,kButton,ComponentConfig)
EE_CFG_V24(MSButtonConfig,kMSButton,ButtonConfig)
EE_CFG_V24(MSArrowButtonConfig,kMSArrow,ButtonConfig)
EE_CFG_V24(MSTabButtonConfig,kMSTabButton,MSButtonConfig)
EE_CFG_V24(MS3DFrameConfig,kMS3DFrame,ComponentConfig)
EE_CFG_V24(MSComboBoxConfig,kMSCombo,ComboBoxConfig)
EE_CFG_V24(MSScrollbarConfig,kMSScrollbar,ScrollbarConfig)
EE_CFG_V24(MSEtchedFrameGroupConfig,kMSEtched,ContainerConfig)
EE_CFG_V24(MSTabCtrlConfig,kMSTabCtrl,TabCtrlConfig)
EE_CFG_V24(MSScrollContainerConfig,kMSScrollCont,ScrollbarScrollerConfig)
EE_CFG_V24(MSHeaderControlConfig,kMSHeader,MultiListHeaderConfig)
EE_CFG_V24(MSListCtrlConfig,kMSListCtrl,MSScrollContainerConfig)
#ifndef EE_CFG_LAYOUT_GLOBAL // (the game's two layout configs are global classes: ConfigTableHost.cpp)
bool PercentLayoutConfig::Virtual24(const std::string& key) { return this->RunTable(kPercent,key); }
bool ColorSchemeConfig::Virtual24(const std::string& key) { return this->RunTable(kScheme,key); }
#endif
EE_CFG_V24(RectangleComponentConfig,kRect,ComponentConfig)
EE_CFG_V24(RectangleComponent3DConfig,kRect3D,ComponentConfig)
EE_CFG_V24(ImageComponentConfig,kImageComp,ComponentConfig)
EE_CFG_V24(ScreenContainerConfig,kScreen,ContainerConfig)
EE_CFG_V24(MultiListAreaConfig,kMultiList,ListAreaConfig)
EE_CFG_V24(MultiListHeaderConfig,kMultiHeader,ContainerConfig)
EE_CFG_V24(ScrollbarConfig,kScrollbar,ContainerConfig)
EE_CFG_V24(TabCtrlConfig,kTabCtrl,ContainerConfig)
EE_CFG_V24(BrowserComponentConfig,kBrowser,ContainerConfig)
EE_CFG_V24(ComboBoxConfig,kCombo,ContainerConfig)
EE_CFG_V24(DialogConfig,kDialog,ContainerConfig)
EE_CFG_V24(ImageButtonConfig,kImageButton,ComponentConfig)
EE_CFG_V24(LabelConfig,kLabel,ComponentConfig)
EE_CFG_V24(ListAreaConfig,kListArea,ScrollAreaConfig)
EE_CFG_V24(InputBoxConfig,kInput,ComponentConfig)
EE_CFG_V24(TextAreaConfig,kTextArea,ScrollAreaConfig)
EE_CFG_V24(TreeAreaConfig,kTree,ListAreaConfig)
EE_CFG_V24(WONSimpleButtonConfig,kSimple,ButtonConfig)
EE_CFG_V24(WONButtonConfig,kWONButton,ButtonConfig)
EE_CFG_V24(SplitterConfig,kSplitter,ContainerConfig)
EE_CFG_V24(WONScrollbarConfig,kWScrollbar,ScrollbarConfig)
bool WONHeaderControlConfig::Virtual24(const std::string& key) { return MultiListHeaderConfig::Virtual24(key) || this->RunTable(kWHeader,key); }
EE_CFG_V24(WONListCtrlConfig,kWListCtrl,WONScrollContainerConfig)
EE_CFG_V24(WONFrameComponentConfig,kWFrame,ComponentConfig)
EE_CFG_V24(WONComboBoxConfig,kWCombo,ComboBoxConfig)
EE_CFG_V24(WONTabCtrlConfig,kWTabCtrl,TabCtrlConfig)
EE_CFG_V24(WONScrollContainerConfig,kWScrollCont,ScrollbarScrollerConfig)
EE_CFG_V24(ScrollAreaConfig,kScrollArea,ContainerConfig)
EE_CFG_V24(ScrollerConfig,kScroller,ContainerConfig)
EE_CFG_V24(ScrollbarScrollerConfig,kSBScroller,ScrollerConfig)
EE_CFG_V24(StretchCenterImageConfig,kStretchCenter,ImageConfig)
#undef EE_CFG_V24
