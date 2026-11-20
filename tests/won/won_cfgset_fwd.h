// The inline forwards the cfgset.rs move left without a caller in the game (R6 WONLobby round 27), as they were in
// WONLobbyGUI.h, WONLobbyTextAreaConfig.h, WONGUIShell.h and WONLobbySplitterConfig.h at c9969d833, copied verbatim into
// subclasses of the game's classes, for tests/won/won_cfgset_test.cpp (which opens the classes' private call helpers).
struct OrigMSButtonBase : MSButtonBase {
    void SetTransparent(bool t) { this->MCall(EeWonMsBtnCall::SetTransparent,t); } //2fe8a0
    void SetAdjustHeightToFitText(bool a) { this->MCall(EeWonMsBtnCall::SetAdjustHeightToFitText,a); } //2fe9f0
    void SetTextColor(int c) { this->MSet(EeWonMsBtnField::TextColor,c); }
    void SetVertAlign(VertAlign v) { this->MSet(EeWonMsBtnField::VAlign,v); }
    void SetHorzAlign(HorzAlign h) { this->MCall(EeWonMsBtnCall::SetHorzAlign,(intptr_t)h); }
    void SetFocusStyle(FocusStyle s) { this->MSet(EeWonMsBtnField::FocusStyle,s); }
    void IncrementHorzTextPad(int l,int r) { this->MCall(EeWonMsBtnCall::IncrementHorzTextPad,l,r); }
    void IncrementVertTextPad(int t,int b) { this->MCall(EeWonMsBtnCall::IncrementVertTextPad,t,b); }
};
template<> struct OrigOf<MSButtonBase> { typedef OrigMSButtonBase type; };
struct OrigMultiListArea : MultiListArea {
    void SetSortColumn(int c,ListSortType t) { this->MCall(EeWonMultiCall::SetSortColumn,c,t); }
    void SetAutoGrowColumns(bool on) { this->MCall(EeWonMultiCall::SetAutoGrowColumns,on); }
    void SetColumnHAlign(int c,HorzAlign a) { this->MCall(EeWonMultiCall::SetColumnHAlign,c,a); }
};
template<> struct OrigOf<MultiListArea> { typedef OrigMultiListArea type; };
struct OrigScrollbar : Scrollbar {
    void SetVertical(bool on) { this->Call(EeWonBarCall::SetVertical,on); }
    void SetResizeThumb(bool on) { this->Call(EeWonBarCall::SetResizeThumb,on); }
    void SetMinThumbSize(unsigned char n) { this->Call(EeWonBarCall::SetMinThumbSize,n); }
};
template<> struct OrigOf<Scrollbar> { typedef OrigScrollbar type; };
struct OrigTabCtrl : TabCtrl {
    void AddTab(Button* t,Container* c) { this->TCall(EeWonTabCall::AddTab,(intptr_t)static_cast<Component*>(t),(intptr_t)static_cast<Component*>(c)); }
    void SetTabBarOnRight(bool v) { this->TCall(EeWonTabCall::SetTabBarOnRight,v); }
    void SetArrowsOnLeft(bool v) { this->TCall(EeWonTabCall::SetArrowsOnLeft,v); }
    void SetTabBarHeight(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::TabBarHeight,v); }
    void SetTabOverlap(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::TabOverlap,v); }
    void SetTabBarOverlap(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::TabBarOverlap,v); }
    void SetBottomTabTopOffset(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::BottomTabTopOffset,v); }
    void SetBottomTabLeftOffset(int v) { this->TCall(EeWonTabCall::SetField,(intptr_t)EeWonTabField::BottomTabLeftOffset,v); }
};
template<> struct OrigOf<TabCtrl> { typedef OrigTabCtrl type; };
struct OrigComboBox : ComboBox {
    void SetListBoxWidth(int w) { this->BCall(EeWonComboCall::SetListBoxWidth,w); }
    void SetListBoxHeight(int h) { this->BCall(EeWonComboCall::SetListBoxHeight,h); }
    void SetListBoxDimensions(int w,int h) { this->BCall(EeWonComboCall::SetListBoxDimensions,w,h); }
    void SetNoType(bool v) { this->BCall(EeWonComboCall::SetNoType,v); }
    void SetShowButtonWhenDisabled(bool v) { this->BCall(EeWonComboCall::SetField,(intptr_t)EeWonComboField::ShowButtonWhenDisabled,v); } // ComboBoxConfig
};
template<> struct OrigOf<ComboBox> { typedef OrigComboBox type; };
struct OrigDialog : Dialog {
    void SetAllowDrag(bool v) { this->DCall(EeWonDlgCall::SetField,(intptr_t)EeWonDlgField::AllowDrag,v); }
    void SetKeepInRoot(bool v) { this->DCall(EeWonDlgCall::SetField,(intptr_t)EeWonDlgField::KeepInRoot,v); }
    void SetDontEndOnEscape() { this->DCall(EeWonDlgCall::SetDontEndOnEscape); }
};
template<> struct OrigOf<Dialog> { typedef OrigDialog type; };
struct OrigLabel : Label {
    void SetTextColor(int theColor) { this->Call(EeWonLabelCall::SetTextColor,theColor); }
    void SetVertAlign(VertAlign theAlign) { this->Call(EeWonLabelCall::SetVertAlign,(intptr_t)theAlign); }
};
template<> struct OrigOf<Label> { typedef OrigLabel type; };
struct OrigListArea : ListArea {
    void SetSortType(ListSortType t) { this->Call(EeWonListCall::SetSortType,t); }
};
template<> struct OrigOf<ListArea> { typedef OrigListArea type; };
struct OrigTreeArea : TreeArea {
    void SetSortType(ListSortType t) { this->TCall(EeWonTreeCall::SetSortType,t); }
};
template<> struct OrigOf<TreeArea> { typedef OrigTreeArea type; };
struct OrigInputBox : InputBox {
    void SetPasswordChar(char c) { this->Call(EeWonInputCall::SetPasswordChar,(intptr_t)c); }
    void SetCharWidth(int em) { this->Call(EeWonInputCall::SetCharWidth,em); }
    void SetSelectOnFocus(bool on) { this->Call(EeWonInputCall::SetSelectOnFocus,on); }
    void SetMaxInputHistory(int n) { this->Call(EeWonInputCall::SetMaxInputHistory,n); }
    void SetCursorBlinkRate(int on,int off=0) { this->Call(EeWonInputCall::SetCursorBlinkRate,on,off); }
};
template<> struct OrigOf<InputBox> { typedef OrigInputBox type; };
struct OrigTextArea : TextArea {
    void SetMaxChars(int chars) { this->Call(EeWonTextCall::SetMaxChars,chars); }
    void SetMaxLines(int lines) { this->Call(EeWonTextCall::SetMaxLines,lines); }
    void SetWrapIndentSize(int size) { this->Call(EeWonTextCall::SetWrapIndentSize,size); }
    void SetLineAlignment(HorzAlign a) { this->Call(EeWonTextCall::SetLineAlignment,(intptr_t)a); }
};
template<> struct OrigOf<TextArea> { typedef OrigTextArea type; };
struct OrigWONButton : WONButton {
    void SetTextColor(int c) { this->WCall(EeWonWbCall::SetTextColor,c); }
    void SetDefaultTextColor(int v) { this->WSet(EeWonWbField::DefaultTextColor,v); }
    void SetVertAlign(VertAlign a) { this->WSet(EeWonWbField::VAlign,(unsigned char)a); } // stores the low byte
    void SetHorzAlign(HorzAlign a) { this->WCall(EeWonWbCall::SetHorzAlign,(intptr_t)a); }
    void SetAdjustHeightToFitText(bool a) { this->WCall(EeWonWbCall::SetAdjustHeightToFitText,a); }
    void SetWrapText(bool w) { this->WCall(EeWonWbCall::SetWrapText,w); }
    void SetDrawGrayText(bool v) { this->WSet(EeWonWbField::DrawGrayText,v); }
    void SetTextPadding(int l,int t,int r,int b) { this->WSet(EeWonWbField::LeftTextPad,l); this->WSet(EeWonWbField::TopTextPad,t); this->WSet(EeWonWbField::RightTextPad,r); this->WSet(EeWonWbField::BottomTextPad,b); }
    void SetFocusPadding(int l,int t,int r,int b) { this->WSet(EeWonWbField::LeftFocusPad,l); this->WSet(EeWonWbField::TopFocusPad,t); this->WSet(EeWonWbField::RightFocusPad,r); this->WSet(EeWonWbField::BottomFocusPad,b); }
    void SetScaleImage(bool v) { this->WSet(EeWonWbField::ScaleImage,v); }
    void SetFocusStyle(unsigned char v) { this->WSet(EeWonWbField::FocusStyle,v); }
    void SetTransparent(bool t) { this->WCall(EeWonWbCall::SetTransparent,t); }
};
template<> struct OrigOf<WONButton> { typedef OrigWONButton type; };
struct OrigSplitter : Splitter {
    void AddQuadrant(Component* c,QuadrantPosition p) { this->Call(EeWonSplitCall::AddQuadrant,(intptr_t)c,p); }
    void AddSplitter(Component* c,SplitterType t) { this->Call(EeWonSplitCall::AddSplitter,(intptr_t)c,t); }
    void AddInvisibleSplitter(SplitterType t,int size=10) { this->Call(EeWonSplitCall::AddInvisibleSplitter,t,size); }
    void AddComplexSplitter(Container* c,SplitterType t,Component* drag=NULL) { this->Call(EeWonSplitCall::AddComplexSplitter,(intptr_t)static_cast<Component*>(c),t,(intptr_t)drag); }
    void SetMinLeftRight(int l,int r) { this->Call(EeWonSplitCall::SetMinLeftRight,l,r); }
    void SetMinTopBottom(int t,int b) { this->Call(EeWonSplitCall::SetMinTopBottom,t,b); }
    void SetPercentages(int l,int t) { this->Call(EeWonSplitCall::SetPercentages,l,t); }
    void MaxQuadrant(QuadrantPosition p) { this->Call(EeWonSplitCall::MaxQuadrant,p); }
    void UnMaxQuadrant() { this->Call(EeWonSplitCall::UnMaxQuadrant); }
    void SetSplitterPos(int x,int y,bool remember=true) { this->Call(EeWonSplitCall::SetSplitterPos,x,y,remember); }
    void RegisterMaxButton(Button* b,QuadrantPosition p) { this->Call(EeWonSplitCall::RegisterMaxButton,(intptr_t)b,p); }
};
template<> struct OrigOf<Splitter> { typedef OrigSplitter type; };
