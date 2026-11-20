// The table setters ee_won_gui's cfgset.rs makes (R6 WONLobby round 27): WONCfgTable.inl's cases as they were at
// fd4680a44, copied verbatim (build/wl27: the lines the move deleted), for tests/won/won_cfgset_test.cpp. The
// driver defines CfgSetFrame (the host frame's members these read), EE_C (the configured component as the kind's
// type) and, for the shell lines, a WONShell whose Op is its tracer.
// the setter tables' keys (WONCfgTable.inl's)
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
    kFont=EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION, kImage=EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION,
    kContainer=EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION,
    kStretchImage=EE_WON_CFG_KEYS_STRETCHIMAGECONFIG_VIRTUAL24_INSTRUCTION, kImageFilter=EE_WON_CFG_KEYS_IMAGEFILTERCONFIG_VIRTUAL24_ININSTRUCTION
};
#define EE_CFG_SET(t,n) case (t)<<8|(n):
// the setters on the component's inline forwards
static bool OrigSet(CfgSetFrame& a,int inSetter)
{
    const int* i=a.i; const GUIString* s=a.s; Component* const* c=a.c; (void)i; (void)s; (void)c;
    switch(inSetter) {
    EE_CFG_SET(kButton,0) EE_C(Button)->SetButtonFlags(i[1],i[0]!=0); break;
    EE_CFG_SET(kButton,2) EE_C(Button)->SetCheck(i[0]!=0,false); break;
    EE_CFG_SET(kMSButton,1) EE_C(MSButtonBase)->SetTextColor(i[0]); break;
    EE_CFG_SET(kMSButton,2) EE_C(MSButtonBase)->SetVertAlign((VertAlign)i[0]); break;
    EE_CFG_SET(kMSButton,3) EE_C(MSButtonBase)->SetHorzAlign((HorzAlign)i[0]); break;
    EE_CFG_SET(kMSButton,4) EE_C(MSButtonBase)->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kMSButton,5) EE_C(MSButtonBase)->SetAdjustHeightToFitText(i[0]!=0); break;
    EE_CFG_SET(kMSButton,7) EE_C(MSButtonBase)->IncrementHorzTextPad(i[0],i[1]); break;
    EE_CFG_SET(kMSButton,8) EE_C(MSButtonBase)->IncrementVertTextPad(i[0],i[1]); break;
    EE_CFG_SET(kMultiList,0) EE_C(MultiListArea)->SetSortColumn(i[0],(ListSortType)i[1]); break;
    EE_CFG_SET(kMultiList,1) EE_C(MultiListArea)->SetColumnWidth(i[0],i[1],false); break;
    EE_CFG_SET(kMultiList,2) EE_C(MultiListArea)->SetNumColumns(i[0]); break;
    EE_CFG_SET(kMultiList,3) EE_C(MultiListArea)->SetDynamicSizeColumn(i[0]); break;
    EE_CFG_SET(kMultiList,5) EE_C(MultiListArea)->SetAutoGrowColumns(i[0]!=0); break;
    EE_CFG_SET(kMultiList,6) EE_C(MultiListArea)->SetColumnHAlign(i[0],(HorzAlign)i[1]); break;
    EE_CFG_SET(kMultiHeader,0) EE_C(MultiListHeader)->SetColumnItem(i[0],c[0]); break;
    EE_CFG_SET(kMultiHeader,1) EE_C(MultiListHeader)->SetMultiListArea(static_cast<MultiListArea*>(c[0])); break;
    EE_CFG_SET(kScrollbar,0) EE_C(Scrollbar)->SetVertical(i[0]!=0); break;
    EE_CFG_SET(kScrollbar,1) EE_C(Scrollbar)->SetResizeThumb(i[0]!=0); break;
    EE_CFG_SET(kScrollbar,2) EE_C(Scrollbar)->SetMinThumbSize((unsigned char)i[0]); break;
    EE_CFG_SET(kTabCtrl,0) EE_C(TabCtrl)->AddTab(static_cast<Button*>(c[0]),static_cast<Container*>(c[1])); break;
    EE_CFG_SET(kTabCtrl,1) EE_C(TabCtrl)->SetTabBarOnRight(i[0]!=0); break;
    EE_CFG_SET(kTabCtrl,2) EE_C(TabCtrl)->SetArrowsOnLeft(i[0]!=0); break;
    EE_CFG_SET(kTabCtrl,3) EE_C(TabCtrl)->SetTabBarHeight(i[0]); break;
    EE_CFG_SET(kTabCtrl,4) EE_C(TabCtrl)->SetTabOverlap(i[0]); break;
    EE_CFG_SET(kTabCtrl,5) EE_C(TabCtrl)->SetTabBarOverlap(i[0]); break;
    EE_CFG_SET(kTabCtrl,6) EE_C(TabCtrl)->SetBottomTabTopOffset(i[0]); break;
    EE_CFG_SET(kTabCtrl,7) EE_C(TabCtrl)->SetBottomTabLeftOffset(i[0]); break;
    EE_CFG_SET(kCombo,0) EE_C(ComboBox)->SetListBoxWidth(i[0]); break;
    EE_CFG_SET(kCombo,1) EE_C(ComboBox)->SetListBoxHeight(i[0]); break;
    EE_CFG_SET(kCombo,2) EE_C(ComboBox)->SetListBoxDimensions(i[0],i[1]); break;
    EE_CFG_SET(kCombo,3) EE_C(ComboBox)->SetText(s[0],true); break;
    EE_CFG_SET(kCombo,4) EE_C(ComboBox)->AddString(s[0],-1); break;
    EE_CFG_SET(kCombo,5) EE_C(ComboBox)->SetSelItem(i[0],true); break;
    EE_CFG_SET(kCombo,6) EE_C(ComboBox)->SetNoType(i[0]!=0); break;
    EE_CFG_SET(kCombo,8) EE_C(ComboBox)->SetShowButtonWhenDisabled(i[0]!=0); break;
    EE_CFG_SET(kDialog,0) EE_C(Dialog)->SetAllowDrag(i[0]!=0); break;
    EE_CFG_SET(kDialog,1) EE_C(Dialog)->SetKeepInRoot(i[0]!=0); break;
    EE_CFG_SET(kDialog,2) EE_C(Dialog)->SetEndOnEscape(i[0]); break;
    EE_CFG_SET(kDialog,3) EE_C(Dialog)->SetDontEndOnEscape(); break;
    EE_CFG_SET(kLabel,2) EE_C(Label)->SetTextColor(i[0]); break;
    EE_CFG_SET(kLabel,4) EE_C(Label)->SetVertAlign((VertAlign)i[0]); break;
    EE_CFG_SET(kLabel,5) EE_C(Label)->SetHorzAlign((HorzAlign)i[0]); break;
    EE_CFG_SET(kLabel,6) EE_C(Label)->SetWrap(i[0]!=0); break;
    EE_CFG_SET(kLabel,7) EE_C(Label)->SetAdjustHeightToFitText(i[0]!=0); break;
    EE_CFG_SET(kListArea,0) EE_C(ListArea)->SetListFlags((unsigned long)i[1],i[0]!=0); break;
    EE_CFG_SET(kListArea,5) EE_C(ListArea)->SetSortType((ListSortType)i[0]); break;
    EE_CFG_SET(kInput,0) EE_C(InputBox)->SetText(s[0],true); break;
    EE_CFG_SET(kInput,1) EE_C(InputBox)->SetPasswordChar(i[0]); break;
    EE_CFG_SET(kInput,2) EE_C(InputBox)->SetAllowSelection(i[0]!=0); break;
    EE_CFG_SET(kInput,3) EE_C(InputBox)->SetMaxChars(i[0]); break;
    EE_CFG_SET(kInput,4) EE_C(InputBox)->SetCharWidth(i[0]); break;
    EE_CFG_SET(kInput,11) EE_C(InputBox)->SetSelectOnFocus(i[0]!=0); break;
    EE_CFG_SET(kInput,12) EE_C(InputBox)->SetMaxInputHistory(i[0]); break;
    EE_CFG_SET(kInput,14) EE_C(InputBox)->SetCursorBlinkRate(i[0],a.ni>1?i[1]:i[0]); break;
    EE_CFG_SET(kTextArea,0) EE_C(TextArea)->SetTextFlags(i[1],i[0]!=0); break;
    EE_CFG_SET(kTextArea,7) EE_C(TextArea)->SetMaxChars(i[0]); break;
    EE_CFG_SET(kTextArea,8) EE_C(TextArea)->SetMaxLines(i[0]); break;
    EE_CFG_SET(kTextArea,9) EE_C(TextArea)->SetWrapIndentSize(i[0]); break;
    EE_CFG_SET(kTextArea,10) EE_C(TextArea)->SetLineAlignment((HorzAlign)i[0]); break;
    EE_CFG_SET(kTree,0) EE_C(TreeArea)->SetSortType((ListSortType)i[0]); break;
    EE_CFG_SET(kWONButton,0) EE_C(WONButton)->SetFont(a.f[0].get()); break;
    EE_CFG_SET(kWONButton,2) EE_C(WONButton)->SetTextColor(i[0]); break;
    EE_CFG_SET(kWONButton,3) EE_C(WONButton)->SetDefaultTextColor(i[0]); break;
    EE_CFG_SET(kWONButton,4) EE_C(WONButton)->SetVertAlign((VertAlign)i[0]); break;
    EE_CFG_SET(kWONButton,5) EE_C(WONButton)->SetHorzAlign((HorzAlign)i[0]); break;
    EE_CFG_SET(kWONButton,6) EE_C(WONButton)->SetTransparent(i[0]!=0); break;
    EE_CFG_SET(kWONButton,7) EE_C(WONButton)->SetAdjustHeightToFitText(i[0]!=0); break;
    EE_CFG_SET(kWONButton,8) EE_C(WONButton)->SetWrapText(i[0]!=0); break;
    EE_CFG_SET(kWONButton,9) EE_C(WONButton)->SetDrawGrayText(i[0]!=0); break;
    EE_CFG_SET(kWONButton,10) EE_C(WONButton)->SetTextPadding(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kWONButton,11) EE_C(WONButton)->SetFocusPadding(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kWONButton,15) EE_C(WONButton)->SetButtonState(i[0]&0xff,a.m[0].get(),a.ni>1?i[1]:-1,a.nf>0?a.f[0].get():NULL,(i[0]&0x100)!=0); break;
    EE_CFG_SET(kWONButton,16) EE_C(WONButton)->SetScaleImage(i[0]!=0); break;
    EE_CFG_SET(kWONButton,17) EE_C(WONButton)->SetFocusStyle(i[0]!=0?1:0); break;
    EE_CFG_SET(kSplitter,0) EE_C(Splitter)->AddQuadrant(c[0],(Splitter::QuadrantPosition)i[0]); break;
    EE_CFG_SET(kSplitter,1) EE_C(Splitter)->AddSplitter(c[0],(Splitter::SplitterType)i[0]); break;
    EE_CFG_SET(kSplitter,2) EE_C(Splitter)->AddInvisibleSplitter((Splitter::SplitterType)i[0],a.ni>1?i[1]:10); break;
    EE_CFG_SET(kSplitter,3) EE_C(Splitter)->AddComplexSplitter(static_cast<Container*>(c[0]),(Splitter::SplitterType)i[0],a.nc>1?c[1]:NULL); break;
    EE_CFG_SET(kSplitter,4) EE_C(Splitter)->SetMinLeftRight(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,5) EE_C(Splitter)->SetMinTopBottom(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,6) EE_C(Splitter)->SetPercentages(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,7) EE_C(Splitter)->MaxQuadrant((Splitter::QuadrantPosition)i[0]); break;
    EE_CFG_SET(kSplitter,8) EE_C(Splitter)->UnMaxQuadrant(); break;
    EE_CFG_SET(kSplitter,9) EE_C(Splitter)->SetSplitterPos(i[0],i[1]); break;
    EE_CFG_SET(kSplitter,10) EE_C(Splitter)->RegisterMaxButton(static_cast<Button*>(c[0]),(Splitter::QuadrantPosition)i[0]); break;
    EE_CFG_SET(kScrollArea,1) EE_C(ScrollArea)->SetBorderPadding(i[0],i[1],i[2],i[3]); break;
    EE_CFG_SET(kScroller,0) EE_C(Scroller)->SetScrollArea(static_cast<ScrollArea*>(c[0])); break;
    EE_CFG_SET(kSBScroller,0) EE_C(ScrollbarScroller)->SetScrollbarConditions((ScrollbarCondition)i[0],(ScrollbarCondition)i[1]); break;
    default: return false;
    }
    return true;
}
// the shell kinds' own methods (WONShell::Op: the driver's tracer)
static bool OrigShellSet(CfgSetFrame& a,int inSetter)
{
    const int* i=a.i; const GUIString* s=a.s; Component* const* c=a.c; (void)i; (void)s; (void)c;
    switch(inSetter) {
    EE_CFG_SET(kMSArrow,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetDirection,i[0]); break;
    EE_CFG_SET(kMSTabButton,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetLowerAmount,i[0]); break;
    EE_CFG_SET(kMS3DFrame,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetFrameType,i[0]); break;
    EE_CFG_SET(kMS3DFrame,1) WONShell::Op(EE_C(Component),EeWonShellOp::SetIsUp,i[0]!=0); break;
    EE_CFG_SET(kMSCombo,0) WONShell::Op(EE_C(ComboBox),EeWonShellOp::SetDrawFrame,i[0]!=0); break;
    EE_CFG_SET(kMSScrollbar,0) WONShell::Op(EE_C(Scrollbar),EeWonShellOp::SetTransparent,i[0]!=0); break;
    EE_CFG_SET(kMSEtched,0) WONShell::Op(EE_C(Container),EeWonShellOp::SetLabelText,(intptr_t)&s[0]); break;
    EE_CFG_SET(kMSEtched,1) WONShell::Op(EE_C(Container),EeWonShellOp::AddContainedComponent,(intptr_t)c[0]); break;
    EE_CFG_SET(kMSTabCtrl,0) WONShell::Op(EE_C(TabCtrl),EeWonShellOp::SetTransparent,i[0]!=0); break;
    EE_CFG_SET(kMSTabCtrl,1) WONShell::Op(EE_C(TabCtrl),EeWonShellOp::AddStandardTab,(intptr_t)&s[0],(intptr_t)static_cast<Container*>(c[0])); break;
    EE_CFG_SET(kMSScrollCont,0) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetDrawFrame,i[0]!=0); break;
    EE_CFG_SET(kMSScrollCont,1) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetDraw3DFrame,i[0]!=0); break;
    EE_CFG_SET(kMSScrollCont,2) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetTransparent,i[0]!=0); break;
    EE_CFG_SET(kMSHeader,0) WONShell::Op(EE_C(MultiListHeader),EeWonShellOp::SetColumn,i[0],(intptr_t)&s[0]); break;
    EE_CFG_SET(kMSListCtrl,0) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetHeaderHeight,i[0]); break;
    EE_CFG_SET(kSimple,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetScaleImage,i[0]!=0); break;
    EE_CFG_SET(kSimple,2) WONShell::Op(EE_C(Component),EeWonShellOp::SetImage,i[0],(intptr_t)a.m[0].get()); break;
    EE_CFG_SET(kSimple,3) WONShell::Op(EE_C(Component),EeWonShellOp::SetCheckMouseButtonDown,i[0]!=0); break;
    EE_CFG_SET(kWHeader,0) { intptr_t aCol[2]={(intptr_t)&s[0],0}; WONShell::Op(EE_C(MultiListHeader),EeWonShellOp::WonSetColumn,i[0],(intptr_t)aCol); break; }
    EE_CFG_SET(kWListCtrl,0) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetHeaderHeightField,i[0]); break;
    EE_CFG_SET(kWListCtrl,4) { intptr_t aCol[2]={(intptr_t)&s[0],i[1]}; WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetStandardColumnHeader,i[0],(intptr_t)aCol); break; }
    EE_CFG_SET(kWListCtrl,5) { intptr_t aCol[2]={(intptr_t)c[0],i[1]}; WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetColumnHeader,i[0],(intptr_t)aCol); break; }
    EE_CFG_SET(kWFrame,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetFrameBorder,0,i[0]); break;
    EE_CFG_SET(kWFrame,1) WONShell::Op(EE_C(Component),EeWonShellOp::SetFrameBorder,1,i[1]); break;
    EE_CFG_SET(kWFrame,2) WONShell::Op(EE_C(Component),EeWonShellOp::SetFrameBorder,2,i[2]); break;
    EE_CFG_SET(kWFrame,3) WONShell::Op(EE_C(Component),EeWonShellOp::SetFrameBorder,3,i[3]); break;
    EE_CFG_SET(kWFrame,4) WONShell::Op(EE_C(Component),EeWonShellOp::SetFrameImage,(intptr_t)a.m[0].get()); break;
    EE_CFG_SET(kWCombo,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetDrawFrameField,i[0]!=0); break;
    EE_CFG_SET(kWCombo,1) WONShell::Op(EE_C(Component),EeWonShellOp::SetButtonInFrame,i[0]!=0); break;
    EE_CFG_SET(kWTabCtrl,0) WONShell::Op(EE_C(Component),EeWonShellOp::SetTransparent,i[0]!=0); break;
    EE_CFG_SET(kWTabCtrl,1) WONShell::Op(EE_C(Component),EeWonShellOp::AddStandardTab,(intptr_t)&s[0],(intptr_t)c[0]); break;
    EE_CFG_SET(kWScrollCont,1) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetTransparentField,i[0]!=0); break;
    EE_CFG_SET(kWScrollCont,3) WONShell::Op(EE_C(ScrollbarScroller),EeWonShellOp::SetCorner,(intptr_t)a.m[0].get()); break;
    default: return false;
    }
    return true;
}
#undef EE_CFG_SET
