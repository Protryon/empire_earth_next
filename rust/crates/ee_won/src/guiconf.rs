//! WONGUI's config readers' key tables (the *Config.cpp units, ResourceTableQueries.cpp and the lobby's control
//! configs). Each reader compared the instruction it was given with its keys (`key=="TEXT"`, exact) and the flag
//! and value names it read with its names (`name.CompareNoCase("Bold")==0`, ignoring case through the C runtime's
//! `towupper`, as GUIString compares); those lists are the tables here, in the original's order, one per reader
//! and compared variable. A reader asks once (`ee_won_gui_cfg_key`, `ee_won_gui_cfg_name`) and tests the index
//! where it compared a text; the C++ keeps the reads, the objects and what each entry does. The first entry that
//! matches wins (a name listed twice, as `WantDefault` in the button flags, answers its first place, which is
//! the branch the original took).

use std::ffi::{c_char, c_void};

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Kind {
    /// Compared exactly (a `std::string` instruction).
    Key,
    /// Compared ignoring case (a GUIString; the names end in NUL for the narrow compare).
    Name,
}

/// BrowserComponentConfig::GetPredefinedComponent(name) in BrowserComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_GETPREDEFINEDCOMPONENT_NAME: i32 = 0;
/// BrowserComponentConfig::Virtual24(instruction) in BrowserComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION: i32 = 1;
/// BrowserComponentConfig::Virtual24(value) in BrowserComponentConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_BROWSERCOMPONENTCONFIG_VIRTUAL24_VALUE: i32 = 2;
/// ButtonConfig::HandleButtonFlags(flag) in ButtonConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_BUTTONCONFIG_HANDLEBUTTONFLAGS_FLAG: i32 = 3;
/// ButtonConfig::Virtual24(inInstruction) in ButtonConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION: i32 = 4;
/// ComboBoxConfig::Virtual24(key) in ComboBoxConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY: i32 = 5;
/// ComboBoxConfig::GetPredefinedComponent(name) in ComboBoxConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME: i32 = 6;
/// ComponentConfig::ReadDirection(value) in ComponentConfigRead.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_READDIRECTION_VALUE: i32 = 7;
/// ComponentConfig::ReadHorzAlign(value) in ComponentConfigRead.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_READHORZALIGN_VALUE: i32 = 8;
/// ComponentConfig::ReadVertAlign(value) in ComponentConfigRead.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_READVERTALIGN_VALUE: i32 = 9;
/// ComponentConfig::GetNamedValue(inName) in ComponentConfigRead.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMPONENTCONFIG_GETNAMEDVALUE_INNAME: i32 = 10;
/// ComponentConfig::CallNamedFunction(inName) in ComponentConfigRead.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMPONENTCONFIG_CALLNAMEDFUNCTION_INNAME: i32 = 11;
/// ComponentConfig::HandleInstruction(type) in ComponentConfigResource.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEINSTRUCTION_TYPE: i32 = 12;
/// ComponentConfig::GetComponentConfig(name) in ComponentConfigResource.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_GETCOMPONENTCONFIG_NAME: i32 = 13;
/// ComponentConfig::HandleValueInstruction(key) in ComponentConfigResource.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEVALUEINSTRUCTION_KEY: i32 = 14;
/// ComponentConfig::Virtual24(key) in ComponentConfigResource.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY: i32 = 15;
/// ComponentConfig::Virtual24(name) in ComponentConfigResource.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_VIRTUAL24_NAME: i32 = 16;
/// ComponentConfig::ReadStandardCursor(name) in ComponentConfigValueReaders.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_READSTANDARDCURSOR_NAME: i32 = 17;
/// ComponentConfig::ReadCursor(type) in ComponentConfigValueReaders.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_READCURSOR_TYPE: i32 = 18;
/// ComponentConfig::ReadSelectionColor(type) in ComponentConfigValueReaders.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_COMPONENTCONFIG_READSELECTIONCOLOR_TYPE: i32 = 19;
/// PercentLayoutConfig::Virtual24(instruction) in ComponentLayoutConfigs.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION: i32 = 20;
/// ColorSchemeConfig::Virtual24(instruction) in ComponentLayoutConfigs.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION: i32 = 21;
/// ContainerConfig::HandleInstruction(type) in ContainerConfigGroups.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_CONTAINERCONFIG_HANDLEINSTRUCTION_TYPE: i32 = 22;
/// ContainerConfig::Virtual24(theInstruction) in ContainerConfigInstructions.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION: i32 = 23;
/// ContainerConfig::HandleNoOverlapLayout(aStr) in ContainerConfigLayouts.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_CONTAINERCONFIG_HANDLENOOVERLAPLAYOUT_ASTR: i32 = 24;
/// DialogConfig::Virtual24(instruction) in DialogConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION: i32 = 25;
/// FontConfig::GetNamedValue(name) in FontConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_FONTCONFIG_GETNAMEDVALUE_NAME: i32 = 26;
/// FontConfig::Virtual24(instruction) in FontConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION: i32 = 27;
/// FontConfig::Virtual24(flag) in FontConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_FONTCONFIG_VIRTUAL24_FLAG: i32 = 28;
/// ImageButtonConfig::Virtual24(instruction) in ImageButtonConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION: i32 = 29;
/// ImageConfig::GetNamedValue(inName) in ImageConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_IMAGECONFIG_GETNAMEDVALUE_INNAME: i32 = 30;
/// ImageConfig::Virtual24(inInstruction) in ImageConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION: i32 = 31;
/// ImageFilterConfig::HandleFilter(name) in ImageConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_IMAGEFILTERCONFIG_HANDLEFILTER_NAME: i32 = 32;
/// ImageFilterConfig::Virtual24(inInstruction) in ImageConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_IMAGEFILTERCONFIG_VIRTUAL24_ININSTRUCTION: i32 = 33;
/// InputBoxConfig::Virtual24(inInstruction) in InputBoxConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION: i32 = 34;
/// LabelConfig::Virtual24(instruction) in LabelConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION: i32 = 35;
/// ListAreaConfig::ReadSortType(name) in ListAreaConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_LISTAREACONFIG_READSORTTYPE_NAME: i32 = 36;
/// ListAreaConfig::ReadListFlags(name) in ListAreaConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_LISTAREACONFIG_READLISTFLAGS_NAME: i32 = 37;
/// ListAreaConfig::Virtual24(inInstruction) in ListAreaConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION: i32 = 38;
/// MSButtonConfig::Virtual24(key) in MSComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY: i32 = 39;
/// MSArrowButtonConfig::Virtual24(key) in MSComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY: i32 = 40;
/// MSArrowButtonConfig::Virtual24(direction) in MSComponentConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_MSARROWBUTTONCONFIG_VIRTUAL24_DIRECTION: i32 = 41;
/// MSTabButtonConfig::Virtual24(key) in MSComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY: i32 = 42;
/// MS3DFrameConfig::Virtual24(key) in MSComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY: i32 = 43;
/// MSComboBoxConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY: i32 = 44;
/// MSScrollbarConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY: i32 = 45;
/// MSEtchedFrameGroupConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY: i32 = 46;
/// MSTabCtrlConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY: i32 = 47;
/// MSScrollContainerConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY: i32 = 48;
/// MSHeaderControlConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY: i32 = 49;
/// MSListCtrlConfig::Virtual24(key) in MSExtraConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY: i32 = 50;
/// MultiListAreaConfig::Virtual24(instruction) in MultiListBoxConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION: i32 = 51;
/// MultiListAreaConfig::Virtual24(name) in MultiListBoxConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_MULTILISTAREACONFIG_VIRTUAL24_NAME: i32 = 52;
/// MultiListHeaderConfig::Virtual24(instruction) in MultiListBoxConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION: i32 = 53;
/// ResourceConfig::HandleValueInstruction(key) in ResourceConfigDispatch.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_RESOURCECONFIG_HANDLEVALUEINSTRUCTION_KEY: i32 = 54;
/// ScrollAreaConfig::Virtual24(instruction) in ResourceTableQueries.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION: i32 = 55;
/// ScrollerConfig::Virtual24(instruction) in ResourceTableQueries.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION: i32 = 56;
/// ScrollbarScrollerConfig::GetScrollbarCondition(condition) in ResourceTableQueries.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_SCROLLBARSCROLLERCONFIG_GETSCROLLBARCONDITION_CONDITION: i32 = 57;
/// ScrollbarScrollerConfig::Virtual24(instruction) in ResourceTableQueries.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION: i32 = 58;
/// ButtonScrollerConfig::GetScrollButtonCondition(aStr) in ResourceTableQueries.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_BUTTONSCROLLERCONFIG_GETSCROLLBUTTONCONDITION_ASTR: i32 = 59;
/// ButtonScrollerConfig::Virtual24(theInstruction) in ResourceTableQueries.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_BUTTONSCROLLERCONFIG_VIRTUAL24_THEINSTRUCTION: i32 = 60;
/// ScrollbarConfig::Virtual24(instruction) in ScrollbarConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION: i32 = 61;
/// ScrollbarConfig::GetPredefinedComponent(name) in ScrollbarConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME: i32 = 62;
/// RectangleComponentConfig::Virtual24(key) in SimpleComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY: i32 = 63;
/// RectangleComponent3DConfig::Virtual24(key) in SimpleComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY: i32 = 64;
/// ImageComponentConfig::Virtual24(key) in SimpleComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY: i32 = 65;
/// ScreenContainerConfig::Virtual24(key) in SimpleComponentConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY: i32 = 66;
/// SplitterConfig::ReadQuadrant(aStr) in SplitterConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_SPLITTERCONFIG_READQUADRANT_ASTR: i32 = 67;
/// SplitterConfig::ReadSplitterType(aStr) in SplitterConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_SPLITTERCONFIG_READSPLITTERTYPE_ASTR: i32 = 68;
/// SplitterConfig::Virtual24(theInstruction) in SplitterConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION: i32 = 69;
/// StretchImageConfig::Virtual24(instruction) in StretchImageConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_STRETCHIMAGECONFIG_VIRTUAL24_INSTRUCTION: i32 = 70;
/// StretchCenterImageConfig::Virtual24(instruction) in StretchImageConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION: i32 = 71;
/// TabCtrlConfig::Virtual24(key) in TabCtrlConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY: i32 = 72;
/// TabCtrlConfig::GetPredefinedComponent(name) in TabCtrlConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_TABCTRLCONFIG_GETPREDEFINEDCOMPONENT_NAME: i32 = 73;
/// TextAreaConfig::Virtual24(key) in TextAreaConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY: i32 = 74;
/// TextAreaConfig::Virtual24(flag) in TextAreaConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_TEXTAREACONFIG_VIRTUAL24_FLAG: i32 = 75;
/// TreeAreaConfig::Virtual24(instruction) in TreeAreaConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION: i32 = 76;
/// WONSimpleButtonConfig::HandleButtonState(aStateStr) in WONButtonConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_WONSIMPLEBUTTONCONFIG_HANDLEBUTTONSTATE_ASTATESTR: i32 = 77;
/// WONSimpleButtonConfig::Virtual24(theInstruction) in WONButtonConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION: i32 = 78;
/// WONButtonConfig::HandleSetButtonState(aStateStr) in WONButtonConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_WONBUTTONCONFIG_HANDLESETBUTTONSTATE_ASTATESTR: i32 = 79;
/// WONButtonConfig::Virtual24(theInstruction) in WONButtonConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION: i32 = 80;
/// WONFrameComponentConfig::Virtual24(key) in WONFrameConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY: i32 = 81;
/// WONFrameComponentConfig::GetNamedValue(key) in WONFrameConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_GETNAMEDVALUE_KEY: i32 = 82;
/// WONComboBoxConfig::Virtual24(key) in WONFrameValueConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY: i32 = 83;
/// WONTabCtrlConfig::Virtual24(key) in WONFrameValueConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY: i32 = 84;
/// WONScrollContainerConfig::Virtual24(key) in WONFrameValueConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY: i32 = 85;
/// WONScrollbarConfig::Virtual24(key) in WONListConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY: i32 = 86;
/// WONHeaderControlConfig::Virtual24(key) in WONListConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY: i32 = 87;
/// WONListCtrlConfig::Virtual24(key) in WONListConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY: i32 = 88;
/// WONSkinConfig::ReadVert(text) in WONSkinConfig.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_WONSKINCONFIG_READVERT_TEXT: i32 = 89;
/// WONSkinConfig::Virtual24(theInstruction) in WONSkinConfig.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION: i32 = 90;
/// ChatCtrlConfig::Virtual24(inKey) in ChatCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_CHATCTRLCONFIG_VIRTUAL24_INKEY: i32 = 91;
/// ChatCtrlConfig::Virtual24(group) in ChatCtrl.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_CHATCTRLCONFIG_VIRTUAL24_GROUP: i32 = 92;
/// CreditsDialogConfig::Virtual24(inInstruction) in CreditsDialog.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_CREDITSDIALOGCONFIG_VIRTUAL24_ININSTRUCTION: i32 = 93;
/// CrossPromotionButtonConfig::Virtual24(key) in CrossPromotionButton.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_CROSSPROMOTIONBUTTONCONFIG_VIRTUAL24_KEY: i32 = 94;
/// ImageTextButtonConfig::Virtual24(key) in CrossPromotionButton.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_IMAGETEXTBUTTONCONFIG_VIRTUAL24_KEY: i32 = 95;
/// GameBrowserCtrlConfig::Virtual24(instruction) in GameBrowserCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_GAMEBROWSERCTRLCONFIG_VIRTUAL24_INSTRUCTION: i32 = 96;
/// GameUserListCtrlConfig::Virtual24(instruction) in GameUserListCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_GAMEUSERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION: i32 = 97;
/// RoomLabelCtrlConfig::Virtual24(inKey) in RoomLabelCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_ROOMLABELCTRLCONFIG_VIRTUAL24_INKEY: i32 = 98;
/// ServerListCtrlConfig::GetNamedValue(inName) in ServerListCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_GETNAMEDVALUE_INNAME: i32 = 99;
/// ServerListCtrlConfig::Virtual24(inInstruction) in ServerListCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_VIRTUAL24_ININSTRUCTION: i32 = 100;
/// ServerListCtrlConfig::Virtual24(aType) in ServerListCtrl.cpp: names (ignoring case).
pub const EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE: i32 = 101;
/// UserListCtrlConfig::Virtual24(instruction) in UserListCtrl.cpp: instruction keys (exact).
pub const EE_WON_CFG_KEYS_USERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION: i32 = 102;

/// The tables, by the constants above.
#[rustfmt::skip]
static TABLES: &[(Kind, &[&[u8]])] = &[
    (Kind::Key, &[b"HTMLVIEW"]),
    (Kind::Key, &[b"TRANSPARENT", b"SCROLLER", b"FONTSIZE"]),
    (Kind::Name, &[b"Smallest\0", b"Small\0", b"Medium\0", b"Large\0", b"Largest\0"]),
    (Kind::Name, &[b"InvalidateOnMouseOver\0", b"RepeatFire\0", b"Checked\0", b"Radio\0", b"ActivateDown\0", b"WantDefault\0", b"EndDialog\0", b"NoActivate\0"]),
    (Kind::Key, &[b"BUTTONFLAGS", b"TEXT", b"SETCHECK", b"CLICKSOUND", b"FITTEXT"]),
    (Kind::Key, &[b"LISTBOXWIDTH", b"LISTBOXHEIGHT", b"LISTBOXSIZE", b"TEXT", b"TEXTITEM", b"SELITEM", b"NOTYPE", b"MAXCHARS", b"SHOWBUTTONWHENDISABLED"]),
    (Kind::Key, &[b"LISTAREA", b"INPUTBOX"]),
    (Kind::Name, &[b"Left\0", b"Right\0", b"Up\0", b"Down\0"]),
    (Kind::Name, &[b"Left\0", b"Center\0", b"Right\0"]),
    (Kind::Name, &[b"Top\0", b"Center\0", b"Bottom\0"]),
    (Kind::Key, &[b"LEFT", b"RIGHT", b"TOP", b"BOTTOM", b"WIDTH", b"HEIGHT"]),
    (Kind::Key, &[b"MAX", b"MIN"]),
    (Kind::Key, &[b"MODIFY", b"RESOURCE"]),
    (Kind::Name, &[b"this\0", b"parent\0"]),
    (Kind::Key, &[b"SETRESOURCE"]),
    (Kind::Key, &[b"MODIFY", b"MAPCOMPONENT", b"CONTROLID", b"LEFT", b"TOP", b"WIDTH", b"HEIGHT", b"POS", b"SIZE", b"TRANSLUCENTCOLOR", b"SIZEATLEAST", b"SETDESIREDSIZE", b"SETDESIREDWIDTH", b"SETDESIREDHEIGHT", b"POSSIZE", b"ENABLE", b"SETVISIBLE", b"REQUESTFOCUS", b"DEFAULTCURSOR", b"COMPONENTFLAGS", b"DECODEIMAGES", b"SHOWPARSETIME", b"STARTPARSETIMER", b"STOPPARSETIMER"]),
    (Kind::Name, &[b"Invisible\0", b"WantFocus\0", b"ParentDrag\0", b"NoParentCursor\0", b"Disabled\0", b"GrabBG\0", b"Translucent\0", b"NoInput\0", b"AlwaysOnTop\0", b"WantTabFocus\0", b"Clip\0", b"DebugBG\0"]),
    (Kind::Name, &[b"Arrow\0", b"Hand\0", b"IBeam\0", b"EastWest\0", b"NorthSouth\0", b"NorthSouthEastWest\0"]),
    (Kind::Name, &[b"Predefined\0"]),
    (Kind::Name, &[b"USEFOREGROUND\0", b"INVERTORIGINAL\0", b"USEORIGINAL\0"]),
    (Kind::Key, &[b"REF", b"LEFT", b"RIGHT", b"TOP", b"BOTTOM", b"PX", b"PY", b"PW", b"PH", b"LP", b"TP", b"RP", b"BP"]),
    (Kind::Key, &[b"3DDARKSHADOW", b"3DFACE", b"3DHILIGHT", b"3DSHADOW", b"SCROLLBAR", b"BUTTONTEXT", b"GRAYTEXT", b"HILIGHT", b"HILIGHTTEXT", b"TOOLTIPBACK", b"TOOLTIPTEXT", b"MENUBACK", b"MENUTEXT", b"BACK", b"TEXT", b"LINK", b"LINKDOWN"]),
    (Kind::Key, &[b"PERCENTLAYOUT", b"COLORSCHEME"]),
    (Kind::Key, &[b"ADD", b"ADDLAYOUTPOINT", b"NEWLAYOUTPOINT", b"LAYOUT", b"SETLAYOUTID", b"ENABLELAYOUT", b"RECALCLAYOUT", b"CHAINLAYOUT", b"REPEATLAYOUT", b"SAMESIZELAYOUT", b"SAMEWIDTHLAYOUT", b"SAMEHEIGHTLAYOUT", b"CENTERLAYOUT", b"HORZCENTERLAYOUT", b"VERTCENTERLAYOUT", b"HORZSPACEEVENLYLAYOUT", b"VERTSPACEEVENLYLAYOUT", b"CLEARRIGHTLAYOUT", b"NOOVERLAPLAYOUT", b"SIZEATLEASTLAYOUT", b"ISOLATETABFOCUS", b"ONLYCHILDINPUT"]),
    (Kind::Name, &[b"Above\0", b"Below\0", b"Right\0", b"Left\0"]),
    (Kind::Key, &[b"ALLOWDRAG", b"KEEPINROOT", b"ENDONESCAPE", b"DONTENDONESCAPE", b"DEFAULTBUTTON"]),
    (Kind::Key, &[b"SIZE", b"HEIGHT"]),
    (Kind::Key, &[b"SRC", b"SIZE", b"STYLE", b"SETDEFAULTFONT", b"SETNAMEDFONT"]),
    (Kind::Name, &[b"Bold\0", b"Italic\0", b"Plain\0", b"Underline\0"]),
    (Kind::Key, &[b"NORMALIMAGE", b"OVERIMAGE", b"DOWNIMAGE", b"DISABLEDIMAGE", b"CHECKEDIMAGE"]),
    (Kind::Key, &[b"WIDTH", b"HEIGHT"]),
    (Kind::Key, &[b"SRC", b"SIZE"]),
    (Kind::Name, &[b"SCALEPERCENT\0", b"SCALEABS\0", b"TINT\0", b"CROP\0", b"PIXELTRANS\0"]),
    (Kind::Key, &[b"SRC", b"FILTER", b"DUPLICATE", b"MULTIHORZCROP", b"EXPORT"]),
    (Kind::Key, &[b"TEXT", b"PASSWORDCHAR", b"ALLOWSELECTION", b"MAXCHARS", b"CHARWIDTH", b"BACKGROUND", b"DISABLEDBACKGROUND", b"ICON", b"TEXTCOLOR", b"DISABLEDTEXTCOLOR", b"SELCOLOR", b"SELONFOCUS", b"MAXINPUTHISTORY", b"USEDISABLEDCOLORS", b"CURSORBLINKRATE"]),
    (Kind::Key, &[b"TEXT", b"FONT", b"TEXTCOLOR", b"BACKGROUND", b"VERTALIGN", b"HORZALIGN", b"WRAP", b"ADJUSTHEIGHTTOFITTEXT"]),
    (Kind::Name, &[b"ASCENDING\0", b"DESCENDING\0", b"NONE\0"]),
    (Kind::Name, &[b"SelOver\0", b"SelOnlyWhenOver\0", b"ToggleSel\0", b"FullRowSelect\0", b"HilightIcons\0", b"ScrollFullLine\0", b"ShowPartialLines\0", b"ClickEventOnMouseUp\0", b"ItemWidthCheckOnSel\0", b"MultiSelect\0", b"KeyMoveWrapAround\0", b"NoAutoAdjustScrollPos\0", b"DoListItemToolTip\0"]),
    (Kind::Key, &[b"LISTFLAGS", b"TEXTITEM", b"FONT", b"TEXTCOLOR", b"SELCOLOR", b"LISTSORTTYPE"]),
    (Kind::Key, &[b"FONT", b"TEXTCOLOR", b"VERTALIGN", b"HORZALIGN", b"TRANSPARENT", b"ADJUSTHEIGHTTOFITTEXT", b"WRAP", b"INCREMENTHORZTEXTPAD", b"INCREMENTVERTEXTPAD"]),
    (Kind::Key, &[b"DIRECTION"]),
    (Kind::Name, &[b"Up\0", b"Down\0", b"Left\0", b"Right\0"]),
    (Kind::Key, &[b"LOWERAMOUNT"]),
    (Kind::Key, &[b"FRAMETYPE", b"UP"]),
    (Kind::Key, &[b"DRAWFRAME"]),
    (Kind::Key, &[b"TRANSPARENT"]),
    (Kind::Key, &[b"TITLE", b"CONTAINEDCOMPONENT"]),
    (Kind::Key, &[b"TRANSPARENT", b"ADDSTANDARDTAB"]),
    (Kind::Key, &[b"DRAWFRAME", b"DRAW3DFRAME", b"TRANSPARENT"]),
    (Kind::Key, &[b"SETSTANDARDCOLUMN"]),
    (Kind::Key, &[b"HEADERHEIGHT"]),
    (Kind::Key, &[b"SORTCOLUMN", b"COLUMNWIDTH", b"NUMCOLUMNS", b"DYNAMICSIZECOLUMN", b"SORTCOLUMNFUNC", b"AUTOGROWCOLUMNS", b"COLUMNHALIGN"]),
    (Kind::Name, &[b"NUMERIC\0"]),
    (Kind::Key, &[b"SETCOLUMN", b"MULTILISTAREA"]),
    (Kind::Key, &[b"STRING", b"CODESTRING", b"IMAGE", b"CODEIMAGE", b"SOUND", b"CODESOUND", b"FONT", b"COLOR", b"BACKGROUND", b"CONTROLID"]),
    (Kind::Key, &[b"BACKGROUND", b"BORDERPADDING"]),
    (Kind::Key, &[b"SCROLLAREA"]),
    (Kind::Name, &[b"DISABLE\0", b"ENABLE\0", b"CONDITIONAL\0"]),
    (Kind::Key, &[b"SCROLLBARCONDITIONS"]),
    (Kind::Name, &[b"DISABLE\0", b"ENABLE\0", b"CONDITIONAL\0", b"PARTIALCONDITIONAL\0"]),
    (Kind::Key, &[b"SCROLLBARCONDITIONS"]),
    (Kind::Key, &[b"VERTICAL", b"RESIZETHUMB", b"MINTHUMBSIZE"]),
    (Kind::Key, &[b"THUMB", b"UPARROW", b"DOWNARROW", b"SCROLLRECT"]),
    (Kind::Key, &[b"SOLIDCOLOR", b"OUTLINECOLOR"]),
    (Kind::Key, &[b"SOLIDCOLOR", b"LIGHTCOLOR", b"DARKCOLOR", b"UP", b"THICKNESS"]),
    (Kind::Key, &[b"IMAGE", b"TILE", b"STRETCH"]),
    (Kind::Key, &[b"ADDSCREEN", b"SHOWSCREEN"]),
    (Kind::Name, &[b"Left\0", b"Right\0", b"Top\0", b"Bottom\0", b"TopLeft\0", b"TopRight\0", b"BottomLeft\0", b"BottomRight\0"]),
    (Kind::Name, &[b"LeftRight\0", b"UpDown\0", b"LeftRight_Top\0", b"LeftRight_Bottom\0", b"UpDown_Left\0", b"UpDown_Right\0", b"Center\0"]),
    (Kind::Key, &[b"ADDQUADRANT", b"ADDSPLITTER", b"ADDINVISIBLESPLITTER", b"ADDCOMPLEXSPLITTER", b"MINLEFTRIGHT", b"MINTOPBOTTOM", b"SPLITTERPERCENTAGES", b"MAXQUADRANT", b"UNMAXQUADRAN", b"SPLITTERPOS", b"REGISTERMAXBUTTON"]),
    (Kind::Key, &[b"SRC", b"TILE", b"BORDERS", b"SKIPCENTER", b"MULTIEXPORT", b"MULTICROPEXPORT", b"EXPORT"]),
    (Kind::Key, &[b"STRETCHIMAGE", b"CENTERIMAGE"]),
    (Kind::Key, &[b"ADDTAB", b"TABBARONRIGHT", b"ARROWSONLEFT", b"TABBARHEIGHT", b"TABOVERLAP", b"TABBAROVERLAP", b"TABVERTOFFSET", b"TABHORZOFFSET"]),
    (Kind::Key, &[b"CONTAINERLAYOUT"]),
    (Kind::Key, &[b"TEXTFLAGS", b"TEXT", b"FORMATTEDTEXT", b"FONT", b"TEXTCOLOR", b"SELCOLOR", b"LINKFONT", b"MAXCHARS", b"MAXLINES", b"WRAPINDENTSIZE", b"HORZALIGN"]),
    (Kind::Name, &[b"WordWrap\0", b"AllowSelection\0", b"ScrollOnBottom\0", b"ShowPartialLines\0", b"ScrollFullLine\0", b"DelayWordWrap\0"]),
    (Kind::Key, &[b"LISTSORTTYPE"]),
    (Kind::Name, &[b"Normal\0", b"Pressed\0", b"Disabled\0", b"Over\0"]),
    (Kind::Key, &[b"SCALEIMAGE", b"TYPE", b"BUTTONSTATE", b"BUTTONSTATES", b"CHECKMOUSEBUTTONDOWN"]),
    (Kind::Name, &[b"Normal\0", b"Pressed\0", b"Disabled\0", b"Over\0", b"Default\0", b"CheckedNormal\0", b"CheckedPressed\0", b"CheckedDisabled\0", b"CheckedOver\0", b"CheckedDefault\0"]),
    (Kind::Key, &[b"FONT", b"TYPE", b"TEXTCOLOR", b"DEFAULTTEXTCOLOR", b"VERTALIGN", b"HORZALIGN", b"TRANSPARENT", b"ADJUSTHEIGHTTOFITTEXT", b"WRAP", b"DRAWGRAYTEXT", b"TEXTPADDING", b"FOCUSPADDING", b"HORZTEXTPAD", b"INCREMENTHORZTEXTPAD", b"INCREMENTVERTEXTPAD", b"BUTTONSTATE", b"SCALEIMAGE", b"DRAWFOCUSRECT"]),
    (Kind::Key, &[b"BORDERS", b"IMAGE"]),
    (Kind::Key, &[b"LEFTPAD", b"RIGHTPAD", b"TOPPAD", b"BOTTOMPAD", b"HORZPAD", b"VERTPAD"]),
    (Kind::Key, &[b"DRAWFRAME", b"BUTTONINFRAME", b"FRAME"]),
    (Kind::Key, &[b"TRANSPARENT", b"ADDSTANDARDTAB", b"FRAME"]),
    (Kind::Key, &[b"DRAWFRAME", b"TRANSPARENT", b"FRAME", b"CORNER"]),
    (Kind::Key, &[b"TRANSPARENT"]),
    (Kind::Key, &[b"SETSTANDARDCOLUMN"]),
    (Kind::Key, &[b"HEADERHEIGHT", b"NUMCOLUMNS", b"DYNAMICSIZECOLUMN", b"LISTFLAGS", b"SETSTANDARDCOLUMN", b"SETCOLUMN"]),
    (Kind::Name, &[b"Vert\0"]),
    (Kind::Key, &[b"WINSKIN", b"ADDBUTTONTYPE", b"ADDSIMPLEBUTTONTYPE", b"SETPUSHBUTTON", b"SETCHECKBOX", b"SETRADIOBUTTON", b"SETHEADERBUTTON", b"SETTABBUTTON", b"SETARROWBUTTON", b"SETCOMBOBUTTON", b"SETCLOSEBUTTON", b"SETSCROLLGUTTER", b"SETSCROLLTHUMB", b"SETSCROLLBAR", b"SETSCROLLCONTAINER", b"SETLISTCTRL", b"SETTABCTRL", b"SETCOMBOBOX", b"SETPOPUPBACK", b"SETDIALOGBACK", b"BUTTONCLICKSOUND", b"IMAGECURSOR"]),
    (Kind::Key, &[b"GROUPTYPE", b"CTRLLINKID", b"COLOR_DEFAULT", b"COLOR_ERROR", b"COLOR_HELP", b"COLOR_USER", b"COLOR_EMOTE", b"COLOR_WHISPER", b"COLOR_WARNING", b"COLOR_GROUP", b"COLOR_MESSAGE", b"COLOR_ACCEPT", b"COLOR_REJECT"]),
    (Kind::Name, &[b"CHAT\0", b"GAME\0", b"TEAM\0"]),
    (Kind::Key, &[b"ADDLINE"]),
    (Kind::Key, &[b"DEFAULTIMAGE", b"DEFAULTBROWSE"]),
    (Kind::Key, &[b"FONT", b"TEXT", b"BROWSELOCATION"]),
    (Kind::Key, &[b"HOMELOCATION"]),
    (Kind::Key, &[b"CTRLLINKID"]),
    (Kind::Key, &[b"PREFIX", b"GROUPTYPE", b"SHOWUSERCOUNT", b"SHOWSERVER"]),
    (Kind::Key, &[b"NUMCOLUMNS"]),
    (Kind::Key, &[b"LISTID", b"ADDCOLUMN", b"DEFAULTFILTER", b"NUMCOLUMNS", b"SETCOLUMNFILTERTYPE"]),
    (Kind::Name, &[b"net\0", b"lan\0", b"direct\0", b"int\0", b"string\0", b"player\0"]),
    (Kind::Key, &[b"GROUPTYPE", b"CTRLLINKID"]),
];

fn table(t: i32) -> Option<(Kind, &'static [&'static [u8]])> {
    usize::try_from(t).ok().and_then(|i| TABLES.get(i)).copied()
}

/// The first place of an instruction key in a key table, -1 when absent (or not a key table).
pub fn key_index(t: i32, key: &[u8]) -> i32 {
    match table(t) {
        Some((Kind::Key, names)) => names.iter().position(|n| *n == key).map_or(-1, |i| i as i32),
        _ => -1,
    }
}

/// The first place of a name in a name table by `same` (the GUIString compare ignoring case of the NUL-terminated
/// name), -1 when absent (or not a name table).
pub fn name_index(t: i32, mut same: impl FnMut(&[u8]) -> bool) -> i32 {
    match table(t) {
        Some((Kind::Name, names)) => names.iter().position(|n| same(n)).map_or(-1, |i| i as i32),
        _ => -1,
    }
}

/// The names of a table (without the names' NUL), for the drivers.
pub fn names(t: i32) -> Vec<&'static [u8]> {
    match table(t) {
        Some((Kind::Key, n)) => n.to_vec(),
        Some((Kind::Name, n)) => n.iter().map(|s| &s[..s.len() - 1]).collect(),
        None => vec![],
    }
}

// ---- the C ABI

/// `key_index` over `len` bytes of an instruction (`std::string::data()`).
///
/// # Safety
/// `key` has `len` bytes (may be NULL when 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_cfg_key(table: i32, key: *const c_char, len: usize) -> i32 {
    let k: &[u8] = if len == 0 || key.is_null() {
        &[]
    } else {
        // SAFETY: `len` bytes (the contract)
        unsafe { std::slice::from_raw_parts(key as *const u8, len) }
    };
    key_index(table, k)
}

/// `name_index` over a GUIString's units (its `mText`, `mLength`), compared as `GUIStringData::CompareNoCase`
/// compares with narrow text.
///
/// # Safety
/// `text` as `ee_won_guistring_compare_narrow`'s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_cfg_name(table: i32, text: *const c_void, len: u16, wchar_size: u32) -> i32 {
    name_index(table, |n| {
        // SAFETY: the units (the contract); the name ends in NUL
        unsafe { crate::guistring::ee_won_guistring_compare_narrow(text, len, 0, n.as_ptr(), true, wchar_size) == 0 }
    })
}

/// The number of tables, for the drivers.
#[no_mangle]
pub extern "C" fn ee_won_gui_cfg_table_count() -> i32 {
    TABLES.len() as i32
}

/// A table's kind (0 keys, 1 names, -1 none) and its `i`th name into `out` (room for `room` bytes; the name's
/// length is returned through `out_len`, -1 past the end), for the drivers.
///
/// # Safety
/// `out` has room for `room` bytes; `out_len` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_cfg_table_name(
    table: i32,
    i: usize,
    out: *mut u8,
    room: usize,
    out_len: *mut i64,
) -> i32 {
    let kind = match self::table(table) {
        Some((Kind::Key, _)) => 0,
        Some((Kind::Name, _)) => 1,
        None => -1,
    };
    let n = names(table);
    // SAFETY: writable (the contract)
    unsafe {
        match n.get(i) {
            Some(s) => {
                let c = s.len().min(room);
                std::ptr::copy_nonoverlapping(s.as_ptr(), out, c);
                *out_len = s.len() as i64;
            }
            None => *out_len = -1,
        }
    }
    kind
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::chat::{CChat, ChatCtype};

    fn nocase(t: i32, s: &str) -> i32 {
        let units: Vec<u32> = s.chars().map(u32::from).collect();
        name_index(t, |n| {
            let narrow = crate::guistring::widen(n, 4);
            crate::guistring::compare_no_case_narrow(&units, 0, &narrow, &CChat) == 0
        })
    }

    #[test]
    fn keys() {
        assert_eq!(key_index(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, b"BUTTONFLAGS"), 0);
        assert_eq!(key_index(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, b"FITTEXT"), 4);
        assert_eq!(key_index(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, b"fittext"), -1);
        assert_eq!(key_index(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, b"FITTEXT "), -1);
        assert_eq!(key_index(EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, b""), -1);
        assert_eq!(key_index(-1, b"TEXT"), -1);
        assert_eq!(key_index(TABLES.len() as i32, b"TEXT"), -1);
        assert_eq!(key_index(EE_WON_CFG_NAMES_BUTTONCONFIG_HANDLEBUTTONFLAGS_FLAG, b"Radio"), -1);
        assert_eq!(key_index(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, b"STOPPARSETIMER"), 23);
    }

    #[test]
    fn names_ignoring_case() {
        let t = EE_WON_CFG_NAMES_BUTTONCONFIG_HANDLEBUTTONFLAGS_FLAG;
        assert_eq!(nocase(t, "radio"), 3);
        assert_eq!(nocase(t, "WANTDEFAULT"), 5);
        assert_eq!(nocase(t, "Radi"), -1);
        assert_eq!(nocase(t, "Radios"), -1);
        assert_eq!(nocase(EE_WON_CFG_NAMES_LISTAREACONFIG_READSORTTYPE_NAME, "none"), 2);
        assert_eq!(names(t)[3], b"Radio");
        assert!(CChat.towupper(u32::from(b'a')) == u32::from(b'A'));
    }

    #[test]
    fn tables_hold_their_names_once_where_first() {
        for t in 0..TABLES.len() as i32 {
            let n = names(t);
            assert!(!n.is_empty());
            for (i, s) in n.iter().enumerate() {
                if TABLES[t as usize].0 == Kind::Key {
                    assert!(key_index(t, s) <= i as i32);
                }
            }
        }
    }
}
