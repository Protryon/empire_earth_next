// WONGUI's config readers' key tables (the *Config.cpp units, ResourceTableQueries.cpp, the lobby's control
// configs): each reader's compares, copied from the C++ before the port in their order (repeats kept: the first one
// that matches is the branch the original took), against ee_won's guiconf tables through its C ABI. The names are
// compared through the real GUIString (GUIStringData's units): `CompareNoCase` as the readers called it. Inputs: every
// name of every table, in upper, lower and swapped case, with a character more or less, and the empty text. Run in
// the C and the English locale. Prints one line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/ee_rust.h"
using namespace WONAPI;
wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// BrowserComponentConfig::GetPredefinedComponent(name), BrowserComponentConfig.cpp
static int Orig0(const std::string& key)
{
    if(key=="HTMLVIEW") return 0;
    return -1;
}
// BrowserComponentConfig::Virtual24(instruction), BrowserComponentConfig.cpp
static int Orig1(const std::string& key)
{
    if(key=="TRANSPARENT") return 0;
    if(key=="SCROLLER") return 1;
    if(key=="FONTSIZE") return 2;
    return -1;
}
// BrowserComponentConfig::Virtual24(value), BrowserComponentConfig.cpp
static int Orig2(const GUIString& name)
{
    if(name.CompareNoCase("Smallest")==0) return 0;
    if(name.CompareNoCase("Small")==0) return 1;
    if(name.CompareNoCase("Medium")==0) return 2;
    if(name.CompareNoCase("Large")==0) return 3;
    if(name.CompareNoCase("Largest")==0) return 4;
    return -1;
}
// ButtonConfig::HandleButtonFlags(flag), ButtonConfig.cpp
static int Orig3(const GUIString& name)
{
    if(name.CompareNoCase("InvalidateOnMouseOver")==0) return 0;
    if(name.CompareNoCase("RepeatFire")==0) return 1;
    if(name.CompareNoCase("Checked")==0) return 2;
    if(name.CompareNoCase("Radio")==0) return 3;
    if(name.CompareNoCase("ActivateDown")==0) return 4;
    if(name.CompareNoCase("WantDefault")==0) return 5;
    if(name.CompareNoCase("EndDialog")==0) return 6;
    if(name.CompareNoCase("NoActivate")==0) return 7;
    if(name.CompareNoCase("WantDefault")==0) return 5;
    return -1;
}
// ButtonConfig::Virtual24(inInstruction), ButtonConfig.cpp
static int Orig4(const std::string& key)
{
    if(key=="BUTTONFLAGS") return 0;
    if(key=="TEXT") return 1;
    if(key=="SETCHECK") return 2;
    if(key=="CLICKSOUND") return 3;
    if(key=="FITTEXT") return 4;
    return -1;
}
// ComboBoxConfig::Virtual24(key), ComboBoxConfig.cpp
static int Orig5(const std::string& key)
{
    if(key=="LISTBOXWIDTH") return 0;
    if(key=="LISTBOXHEIGHT") return 1;
    if(key=="LISTBOXSIZE") return 2;
    if(key=="TEXT") return 3;
    if(key=="TEXTITEM") return 4;
    if(key=="SELITEM") return 5;
    if(key=="NOTYPE") return 6;
    if(key=="MAXCHARS") return 7;
    if(key=="SHOWBUTTONWHENDISABLED") return 8;
    return -1;
}
// ComboBoxConfig::GetPredefinedComponent(name), ComboBoxConfig.cpp
static int Orig6(const std::string& key)
{
    if(key=="LISTAREA") return 0;
    if(key=="INPUTBOX") return 1;
    return -1;
}
// ComponentConfig::ReadDirection(value), ComponentConfigRead.cpp
static int Orig7(const GUIString& name)
{
    if(name.CompareNoCase("Left")==0) return 0;
    if(name.CompareNoCase("Right")==0) return 1;
    if(name.CompareNoCase("Up")==0) return 2;
    if(name.CompareNoCase("Down")==0) return 3;
    return -1;
}
// ComponentConfig::ReadHorzAlign(value), ComponentConfigRead.cpp
static int Orig8(const GUIString& name)
{
    if(name.CompareNoCase("Left")==0) return 0;
    if(name.CompareNoCase("Center")==0) return 1;
    if(name.CompareNoCase("Right")==0) return 2;
    return -1;
}
// ComponentConfig::ReadVertAlign(value), ComponentConfigRead.cpp
static int Orig9(const GUIString& name)
{
    if(name.CompareNoCase("Top")==0) return 0;
    if(name.CompareNoCase("Center")==0) return 1;
    if(name.CompareNoCase("Bottom")==0) return 2;
    return -1;
}
// ComponentConfig::GetNamedValue(inName), ComponentConfigRead.cpp
static int Orig10(const std::string& key)
{
    if(key=="LEFT") return 0;
    if(key=="RIGHT") return 1;
    if(key=="TOP") return 2;
    if(key=="BOTTOM") return 3;
    if(key=="WIDTH") return 4;
    if(key=="HEIGHT") return 5;
    return -1;
}
// ComponentConfig::CallNamedFunction(inName), ComponentConfigRead.cpp
static int Orig11(const std::string& key)
{
    if(key=="MAX") return 0;
    if(key=="MIN") return 1;
    return -1;
}
// ComponentConfig::HandleInstruction(type), ComponentConfigResource.cpp
static int Orig12(const std::string& key)
{
    if(key=="MODIFY") return 0;
    if(key=="RESOURCE") return 1;
    return -1;
}
// ComponentConfig::GetComponentConfig(name), ComponentConfigResource.cpp
static int Orig13(const GUIString& name)
{
    if(name.CompareNoCase("this")==0) return 0;
    if(name.CompareNoCase("parent")==0) return 1;
    return -1;
}
// ComponentConfig::HandleValueInstruction(key), ComponentConfigResource.cpp
static int Orig14(const std::string& key)
{
    if(key=="SETRESOURCE") return 0;
    return -1;
}
// ComponentConfig::Virtual24(key), ComponentConfigResource.cpp
static int Orig15(const std::string& key)
{
    if(key=="MODIFY") return 0;
    if(key=="MAPCOMPONENT") return 1;
    if(key=="CONTROLID") return 2;
    if(key=="LEFT") return 3;
    if(key=="TOP") return 4;
    if(key=="WIDTH") return 5;
    if(key=="HEIGHT") return 6;
    if(key=="POS") return 7;
    if(key=="SIZE") return 8;
    if(key=="TRANSLUCENTCOLOR") return 9;
    if(key=="SIZEATLEAST") return 10;
    if(key=="SETDESIREDSIZE") return 11;
    if(key=="SETDESIREDWIDTH") return 12;
    if(key=="SETDESIREDHEIGHT") return 13;
    if(key=="POSSIZE") return 14;
    if(key=="ENABLE") return 15;
    if(key=="SETVISIBLE") return 16;
    if(key=="REQUESTFOCUS") return 17;
    if(key=="DEFAULTCURSOR") return 18;
    if(key=="COMPONENTFLAGS") return 19;
    if(key=="DECODEIMAGES") return 20;
    if(key=="SHOWPARSETIME") return 21;
    if(key=="STARTPARSETIMER") return 22;
    if(key=="STOPPARSETIMER") return 23;
    return -1;
}
// ComponentConfig::Virtual24(name), ComponentConfigResource.cpp
static int Orig16(const GUIString& name)
{
    if(name.CompareNoCase("Invisible")==0) return 0;
    if(name.CompareNoCase("WantFocus")==0) return 1;
    if(name.CompareNoCase("ParentDrag")==0) return 2;
    if(name.CompareNoCase("NoParentCursor")==0) return 3;
    if(name.CompareNoCase("Disabled")==0) return 4;
    if(name.CompareNoCase("GrabBG")==0) return 5;
    if(name.CompareNoCase("Translucent")==0) return 6;
    if(name.CompareNoCase("NoInput")==0) return 7;
    if(name.CompareNoCase("AlwaysOnTop")==0) return 8;
    if(name.CompareNoCase("WantTabFocus")==0) return 9;
    if(name.CompareNoCase("Clip")==0) return 10;
    if(name.CompareNoCase("DebugBG")==0) return 11;
    return -1;
}
// ComponentConfig::ReadStandardCursor(name), ComponentConfigValueReaders.cpp
static int Orig17(const GUIString& name)
{
    if(name.CompareNoCase("Arrow")==0) return 0;
    if(name.CompareNoCase("Hand")==0) return 1;
    if(name.CompareNoCase("IBeam")==0) return 2;
    if(name.CompareNoCase("EastWest")==0) return 3;
    if(name.CompareNoCase("NorthSouth")==0) return 4;
    if(name.CompareNoCase("NorthSouthEastWest")==0) return 5;
    return -1;
}
// ComponentConfig::ReadCursor(type), ComponentConfigValueReaders.cpp
static int Orig18(const GUIString& name)
{
    if(name.CompareNoCase("Predefined")==0) return 0;
    return -1;
}
// ComponentConfig::ReadSelectionColor(type), ComponentConfigValueReaders.cpp
static int Orig19(const GUIString& name)
{
    if(name.CompareNoCase("USEFOREGROUND")==0) return 0;
    if(name.CompareNoCase("INVERTORIGINAL")==0) return 1;
    if(name.CompareNoCase("USEORIGINAL")==0) return 2;
    return -1;
}
// PercentLayoutConfig::Virtual24(instruction), ComponentLayoutConfigs.cpp
static int Orig20(const std::string& key)
{
    if(key=="REF") return 0;
    if(key=="LEFT") return 1;
    if(key=="RIGHT") return 2;
    if(key=="TOP") return 3;
    if(key=="BOTTOM") return 4;
    if(key=="PX") return 5;
    if(key=="PY") return 6;
    if(key=="PW") return 7;
    if(key=="PH") return 8;
    if(key=="LP") return 9;
    if(key=="TP") return 10;
    if(key=="RP") return 11;
    if(key=="BP") return 12;
    return -1;
}
// ColorSchemeConfig::Virtual24(instruction), ComponentLayoutConfigs.cpp
static int Orig21(const std::string& key)
{
    if(key=="3DDARKSHADOW") return 0;
    if(key=="3DFACE") return 1;
    if(key=="3DHILIGHT") return 2;
    if(key=="3DSHADOW") return 3;
    if(key=="SCROLLBAR") return 4;
    if(key=="BUTTONTEXT") return 5;
    if(key=="GRAYTEXT") return 6;
    if(key=="HILIGHT") return 7;
    if(key=="HILIGHTTEXT") return 8;
    if(key=="TOOLTIPBACK") return 9;
    if(key=="TOOLTIPTEXT") return 10;
    if(key=="MENUBACK") return 11;
    if(key=="MENUTEXT") return 12;
    if(key=="BACK") return 13;
    if(key=="TEXT") return 14;
    if(key=="LINK") return 15;
    if(key=="LINKDOWN") return 16;
    return -1;
}
// ContainerConfig::HandleInstruction(type), ContainerConfigGroups.cpp
static int Orig22(const std::string& key)
{
    if(key=="PERCENTLAYOUT") return 0;
    if(key=="COLORSCHEME") return 1;
    return -1;
}
// ContainerConfig::Virtual24(theInstruction), ContainerConfigInstructions.cpp
static int Orig23(const std::string& key)
{
    if(key=="ADD") return 0;
    if(key=="ADDLAYOUTPOINT") return 1;
    if(key=="NEWLAYOUTPOINT") return 2;
    if(key=="LAYOUT") return 3;
    if(key=="SETLAYOUTID") return 4;
    if(key=="ENABLELAYOUT") return 5;
    if(key=="RECALCLAYOUT") return 6;
    if(key=="CHAINLAYOUT") return 7;
    if(key=="REPEATLAYOUT") return 8;
    if(key=="SAMESIZELAYOUT") return 9;
    if(key=="SAMEWIDTHLAYOUT") return 10;
    if(key=="SAMEHEIGHTLAYOUT") return 11;
    if(key=="CENTERLAYOUT") return 12;
    if(key=="HORZCENTERLAYOUT") return 13;
    if(key=="VERTCENTERLAYOUT") return 14;
    if(key=="HORZSPACEEVENLYLAYOUT") return 15;
    if(key=="VERTSPACEEVENLYLAYOUT") return 16;
    if(key=="CLEARRIGHTLAYOUT") return 17;
    if(key=="NOOVERLAPLAYOUT") return 18;
    if(key=="SIZEATLEASTLAYOUT") return 19;
    if(key=="ISOLATETABFOCUS") return 20;
    if(key=="ONLYCHILDINPUT") return 21;
    return -1;
}
// ContainerConfig::HandleNoOverlapLayout(aStr), ContainerConfigLayouts.cpp
static int Orig24(const GUIString& name)
{
    if(name.CompareNoCase("Above")==0) return 0;
    if(name.CompareNoCase("Below")==0) return 1;
    if(name.CompareNoCase("Right")==0) return 2;
    if(name.CompareNoCase("Left")==0) return 3;
    return -1;
}
// DialogConfig::Virtual24(instruction), DialogConfig.cpp
static int Orig25(const std::string& key)
{
    if(key=="ALLOWDRAG") return 0;
    if(key=="KEEPINROOT") return 1;
    if(key=="ENDONESCAPE") return 2;
    if(key=="DONTENDONESCAPE") return 3;
    if(key=="DEFAULTBUTTON") return 4;
    return -1;
}
// FontConfig::GetNamedValue(name), FontConfig.cpp
static int Orig26(const std::string& key)
{
    if(key=="SIZE") return 0;
    if(key=="HEIGHT") return 1;
    return -1;
}
// FontConfig::Virtual24(instruction), FontConfig.cpp
static int Orig27(const std::string& key)
{
    if(key=="SRC") return 0;
    if(key=="SIZE") return 1;
    if(key=="STYLE") return 2;
    if(key=="SETDEFAULTFONT") return 3;
    if(key=="SETNAMEDFONT") return 4;
    return -1;
}
// FontConfig::Virtual24(flag), FontConfig.cpp
static int Orig28(const GUIString& name)
{
    if(name.CompareNoCase("Bold")==0) return 0;
    if(name.CompareNoCase("Italic")==0) return 1;
    if(name.CompareNoCase("Plain")==0) return 2;
    if(name.CompareNoCase("Underline")==0) return 3;
    return -1;
}
// ImageButtonConfig::Virtual24(instruction), ImageButtonConfig.cpp
static int Orig29(const std::string& key)
{
    if(key=="NORMALIMAGE") return 0;
    if(key=="OVERIMAGE") return 1;
    if(key=="DOWNIMAGE") return 2;
    if(key=="DISABLEDIMAGE") return 3;
    if(key=="CHECKEDIMAGE") return 4;
    return -1;
}
// ImageConfig::GetNamedValue(inName), ImageConfig.cpp
static int Orig30(const std::string& key)
{
    if(key=="WIDTH") return 0;
    if(key=="HEIGHT") return 1;
    return -1;
}
// ImageConfig::Virtual24(inInstruction), ImageConfig.cpp
static int Orig31(const std::string& key)
{
    if(key=="SRC") return 0;
    if(key=="SIZE") return 1;
    return -1;
}
// ImageFilterConfig::HandleFilter(name), ImageConfig.cpp
static int Orig32(const GUIString& name)
{
    if(name.CompareNoCase("SCALEPERCENT")==0) return 0;
    if(name.CompareNoCase("SCALEABS")==0) return 1;
    if(name.CompareNoCase("TINT")==0) return 2;
    if(name.CompareNoCase("CROP")==0) return 3;
    if(name.CompareNoCase("PIXELTRANS")==0) return 4;
    return -1;
}
// ImageFilterConfig::Virtual24(inInstruction), ImageConfig.cpp
static int Orig33(const std::string& key)
{
    if(key=="SRC") return 0;
    if(key=="FILTER") return 1;
    if(key=="DUPLICATE") return 2;
    if(key=="MULTIHORZCROP") return 3;
    if(key=="EXPORT") return 4;
    return -1;
}
// InputBoxConfig::Virtual24(inInstruction), InputBoxConfig.cpp
static int Orig34(const std::string& key)
{
    if(key=="TEXT") return 0;
    if(key=="PASSWORDCHAR") return 1;
    if(key=="ALLOWSELECTION") return 2;
    if(key=="MAXCHARS") return 3;
    if(key=="CHARWIDTH") return 4;
    if(key=="BACKGROUND") return 5;
    if(key=="DISABLEDBACKGROUND") return 6;
    if(key=="ICON") return 7;
    if(key=="TEXTCOLOR") return 8;
    if(key=="DISABLEDTEXTCOLOR") return 9;
    if(key=="SELCOLOR") return 10;
    if(key=="SELONFOCUS") return 11;
    if(key=="MAXINPUTHISTORY") return 12;
    if(key=="USEDISABLEDCOLORS") return 13;
    if(key=="CURSORBLINKRATE") return 14;
    return -1;
}
// LabelConfig::Virtual24(instruction), LabelConfig.cpp
static int Orig35(const std::string& key)
{
    if(key=="TEXT") return 0;
    if(key=="FONT") return 1;
    if(key=="TEXTCOLOR") return 2;
    if(key=="BACKGROUND") return 3;
    if(key=="VERTALIGN") return 4;
    if(key=="HORZALIGN") return 5;
    if(key=="WRAP") return 6;
    if(key=="ADJUSTHEIGHTTOFITTEXT") return 7;
    return -1;
}
// ListAreaConfig::ReadSortType(name), ListAreaConfig.cpp
static int Orig36(const GUIString& name)
{
    if(name.CompareNoCase("ASCENDING")==0) return 0;
    if(name.CompareNoCase("DESCENDING")==0) return 1;
    if(name.CompareNoCase("NONE")==0) return 2;
    return -1;
}
// ListAreaConfig::ReadListFlags(name), ListAreaConfig.cpp
static int Orig37(const GUIString& name)
{
    if(name.CompareNoCase("SelOver")==0) return 0;
    if(name.CompareNoCase("SelOnlyWhenOver")==0) return 1;
    if(name.CompareNoCase("ToggleSel")==0) return 2;
    if(name.CompareNoCase("FullRowSelect")==0) return 3;
    if(name.CompareNoCase("HilightIcons")==0) return 4;
    if(name.CompareNoCase("ScrollFullLine")==0) return 5;
    if(name.CompareNoCase("ShowPartialLines")==0) return 6;
    if(name.CompareNoCase("ClickEventOnMouseUp")==0) return 7;
    if(name.CompareNoCase("ItemWidthCheckOnSel")==0) return 8;
    if(name.CompareNoCase("MultiSelect")==0) return 9;
    if(name.CompareNoCase("KeyMoveWrapAround")==0) return 10;
    if(name.CompareNoCase("NoAutoAdjustScrollPos")==0) return 11;
    if(name.CompareNoCase("DoListItemToolTip")==0) return 12;
    return -1;
}
// ListAreaConfig::Virtual24(inInstruction), ListAreaConfig.cpp
static int Orig38(const std::string& key)
{
    if(key=="LISTFLAGS") return 0;
    if(key=="TEXTITEM") return 1;
    if(key=="FONT") return 2;
    if(key=="TEXTCOLOR") return 3;
    if(key=="SELCOLOR") return 4;
    if(key=="LISTSORTTYPE") return 5;
    return -1;
}
// MSButtonConfig::Virtual24(key), MSComponentConfig.cpp
static int Orig39(const std::string& key)
{
    if(key=="FONT") return 0;
    if(key=="TEXTCOLOR") return 1;
    if(key=="VERTALIGN") return 2;
    if(key=="HORZALIGN") return 3;
    if(key=="TRANSPARENT") return 4;
    if(key=="ADJUSTHEIGHTTOFITTEXT") return 5;
    if(key=="WRAP") return 6;
    if(key=="INCREMENTHORZTEXTPAD") return 7;
    if(key=="INCREMENTVERTEXTPAD") return 8;
    return -1;
}
// MSArrowButtonConfig::Virtual24(key), MSComponentConfig.cpp
static int Orig40(const std::string& key)
{
    if(key=="DIRECTION") return 0;
    return -1;
}
// MSArrowButtonConfig::Virtual24(direction), MSComponentConfig.cpp
static int Orig41(const GUIString& name)
{
    if(name.CompareNoCase("Up")==0) return 0;
    if(name.CompareNoCase("Down")==0) return 1;
    if(name.CompareNoCase("Left")==0) return 2;
    if(name.CompareNoCase("Right")==0) return 3;
    return -1;
}
// MSTabButtonConfig::Virtual24(key), MSComponentConfig.cpp
static int Orig42(const std::string& key)
{
    if(key=="LOWERAMOUNT") return 0;
    return -1;
}
// MS3DFrameConfig::Virtual24(key), MSComponentConfig.cpp
static int Orig43(const std::string& key)
{
    if(key=="FRAMETYPE") return 0;
    if(key=="UP") return 1;
    return -1;
}
// MSComboBoxConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig44(const std::string& key)
{
    if(key=="DRAWFRAME") return 0;
    return -1;
}
// MSScrollbarConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig45(const std::string& key)
{
    if(key=="TRANSPARENT") return 0;
    return -1;
}
// MSEtchedFrameGroupConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig46(const std::string& key)
{
    if(key=="TITLE") return 0;
    if(key=="CONTAINEDCOMPONENT") return 1;
    return -1;
}
// MSTabCtrlConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig47(const std::string& key)
{
    if(key=="TRANSPARENT") return 0;
    if(key=="ADDSTANDARDTAB") return 1;
    return -1;
}
// MSScrollContainerConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig48(const std::string& key)
{
    if(key=="DRAWFRAME") return 0;
    if(key=="DRAW3DFRAME") return 1;
    if(key=="TRANSPARENT") return 2;
    return -1;
}
// MSHeaderControlConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig49(const std::string& key)
{
    if(key=="SETSTANDARDCOLUMN") return 0;
    return -1;
}
// MSListCtrlConfig::Virtual24(key), MSExtraConfig.cpp
static int Orig50(const std::string& key)
{
    if(key=="HEADERHEIGHT") return 0;
    return -1;
}
// MultiListAreaConfig::Virtual24(instruction), MultiListBoxConfig.cpp
static int Orig51(const std::string& key)
{
    if(key=="SORTCOLUMN") return 0;
    if(key=="COLUMNWIDTH") return 1;
    if(key=="NUMCOLUMNS") return 2;
    if(key=="DYNAMICSIZECOLUMN") return 3;
    if(key=="SORTCOLUMNFUNC") return 4;
    if(key=="AUTOGROWCOLUMNS") return 5;
    if(key=="COLUMNHALIGN") return 6;
    return -1;
}
// MultiListAreaConfig::Virtual24(name), MultiListBoxConfig.cpp
static int Orig52(const GUIString& name)
{
    if(name.CompareNoCase("NUMERIC")==0) return 0;
    return -1;
}
// MultiListHeaderConfig::Virtual24(instruction), MultiListBoxConfig.cpp
static int Orig53(const std::string& key)
{
    if(key=="SETCOLUMN") return 0;
    if(key=="MULTILISTAREA") return 1;
    return -1;
}
// ResourceConfig::HandleValueInstruction(key), ResourceConfigDispatch.cpp
static int Orig54(const std::string& key)
{
    if(key=="STRING") return 0;
    if(key=="CODESTRING") return 1;
    if(key=="IMAGE") return 2;
    if(key=="CODEIMAGE") return 3;
    if(key=="SOUND") return 4;
    if(key=="CODESOUND") return 5;
    if(key=="FONT") return 6;
    if(key=="COLOR") return 7;
    if(key=="BACKGROUND") return 8;
    if(key=="CONTROLID") return 9;
    return -1;
}
// ScrollAreaConfig::Virtual24(instruction), ResourceTableQueries.cpp
static int Orig55(const std::string& key)
{
    if(key=="BACKGROUND") return 0;
    if(key=="BORDERPADDING") return 1;
    return -1;
}
// ScrollerConfig::Virtual24(instruction), ResourceTableQueries.cpp
static int Orig56(const std::string& key)
{
    if(key=="SCROLLAREA") return 0;
    return -1;
}
// ScrollbarScrollerConfig::GetScrollbarCondition(condition), ResourceTableQueries.cpp
static int Orig57(const GUIString& name)
{
    if(name.CompareNoCase("DISABLE")==0) return 0;
    if(name.CompareNoCase("ENABLE")==0) return 1;
    if(name.CompareNoCase("CONDITIONAL")==0) return 2;
    return -1;
}
// ScrollbarScrollerConfig::Virtual24(instruction), ResourceTableQueries.cpp
static int Orig58(const std::string& key)
{
    if(key=="SCROLLBARCONDITIONS") return 0;
    return -1;
}
// ButtonScrollerConfig::GetScrollButtonCondition(aStr), ResourceTableQueries.cpp
static int Orig59(const GUIString& name)
{
    if(name.CompareNoCase("DISABLE")==0) return 0;
    if(name.CompareNoCase("ENABLE")==0) return 1;
    if(name.CompareNoCase("CONDITIONAL")==0) return 2;
    if(name.CompareNoCase("PARTIALCONDITIONAL")==0) return 3;
    return -1;
}
// ButtonScrollerConfig::Virtual24(theInstruction), ResourceTableQueries.cpp
static int Orig60(const std::string& key)
{
    if(key=="SCROLLBARCONDITIONS") return 0;
    return -1;
}
// ScrollbarConfig::Virtual24(instruction), ScrollbarConfig.cpp
static int Orig61(const std::string& key)
{
    if(key=="VERTICAL") return 0;
    if(key=="RESIZETHUMB") return 1;
    if(key=="MINTHUMBSIZE") return 2;
    return -1;
}
// ScrollbarConfig::GetPredefinedComponent(name), ScrollbarConfig.cpp
static int Orig62(const std::string& key)
{
    if(key=="THUMB") return 0;
    if(key=="UPARROW") return 1;
    if(key=="DOWNARROW") return 2;
    if(key=="SCROLLRECT") return 3;
    return -1;
}
// RectangleComponentConfig::Virtual24(key), SimpleComponentConfig.cpp
static int Orig63(const std::string& key)
{
    if(key=="SOLIDCOLOR") return 0;
    if(key=="OUTLINECOLOR") return 1;
    return -1;
}
// RectangleComponent3DConfig::Virtual24(key), SimpleComponentConfig.cpp
static int Orig64(const std::string& key)
{
    if(key=="SOLIDCOLOR") return 0;
    if(key=="LIGHTCOLOR") return 1;
    if(key=="DARKCOLOR") return 2;
    if(key=="UP") return 3;
    if(key=="THICKNESS") return 4;
    return -1;
}
// ImageComponentConfig::Virtual24(key), SimpleComponentConfig.cpp
static int Orig65(const std::string& key)
{
    if(key=="IMAGE") return 0;
    if(key=="TILE") return 1;
    if(key=="STRETCH") return 2;
    return -1;
}
// ScreenContainerConfig::Virtual24(key), SimpleComponentConfig.cpp
static int Orig66(const std::string& key)
{
    if(key=="ADDSCREEN") return 0;
    if(key=="SHOWSCREEN") return 1;
    return -1;
}
// SplitterConfig::ReadQuadrant(aStr), SplitterConfig.cpp
static int Orig67(const GUIString& name)
{
    if(name.CompareNoCase("Left")==0) return 0;
    if(name.CompareNoCase("Right")==0) return 1;
    if(name.CompareNoCase("Top")==0) return 2;
    if(name.CompareNoCase("Bottom")==0) return 3;
    if(name.CompareNoCase("TopLeft")==0) return 4;
    if(name.CompareNoCase("TopRight")==0) return 5;
    if(name.CompareNoCase("BottomLeft")==0) return 6;
    if(name.CompareNoCase("BottomRight")==0) return 7;
    return -1;
}
// SplitterConfig::ReadSplitterType(aStr), SplitterConfig.cpp
static int Orig68(const GUIString& name)
{
    if(name.CompareNoCase("LeftRight")==0) return 0;
    if(name.CompareNoCase("UpDown")==0) return 1;
    if(name.CompareNoCase("LeftRight_Top")==0) return 2;
    if(name.CompareNoCase("LeftRight_Bottom")==0) return 3;
    if(name.CompareNoCase("UpDown_Left")==0) return 4;
    if(name.CompareNoCase("UpDown_Right")==0) return 5;
    if(name.CompareNoCase("Center")==0) return 6;
    return -1;
}
// SplitterConfig::Virtual24(theInstruction), SplitterConfig.cpp
static int Orig69(const std::string& key)
{
    if(key=="ADDQUADRANT") return 0;
    if(key=="ADDSPLITTER") return 1;
    if(key=="ADDINVISIBLESPLITTER") return 2;
    if(key=="ADDCOMPLEXSPLITTER") return 3;
    if(key=="MINLEFTRIGHT") return 4;
    if(key=="MINTOPBOTTOM") return 5;
    if(key=="SPLITTERPERCENTAGES") return 6;
    if(key=="MAXQUADRANT") return 7;
    if(key=="UNMAXQUADRAN") return 8;
    if(key=="SPLITTERPOS") return 9;
    if(key=="REGISTERMAXBUTTON") return 10;
    return -1;
}
// StretchImageConfig::Virtual24(instruction), StretchImageConfig.cpp
static int Orig70(const std::string& key)
{
    if(key=="SRC") return 0;
    if(key=="TILE") return 1;
    if(key=="BORDERS") return 2;
    if(key=="SKIPCENTER") return 3;
    if(key=="MULTIEXPORT") return 4;
    if(key=="MULTICROPEXPORT") return 5;
    if(key=="EXPORT") return 6;
    return -1;
}
// StretchCenterImageConfig::Virtual24(instruction), StretchImageConfig.cpp
static int Orig71(const std::string& key)
{
    if(key=="STRETCHIMAGE") return 0;
    if(key=="CENTERIMAGE") return 1;
    return -1;
}
// TabCtrlConfig::Virtual24(key), TabCtrlConfig.cpp
static int Orig72(const std::string& key)
{
    if(key=="ADDTAB") return 0;
    if(key=="TABBARONRIGHT") return 1;
    if(key=="ARROWSONLEFT") return 2;
    if(key=="TABBARHEIGHT") return 3;
    if(key=="TABOVERLAP") return 4;
    if(key=="TABBAROVERLAP") return 5;
    if(key=="TABVERTOFFSET") return 6;
    if(key=="TABHORZOFFSET") return 7;
    return -1;
}
// TabCtrlConfig::GetPredefinedComponent(name), TabCtrlConfig.cpp
static int Orig73(const std::string& key)
{
    if(key=="CONTAINERLAYOUT") return 0;
    return -1;
}
// TextAreaConfig::Virtual24(key), TextAreaConfig.cpp
static int Orig74(const std::string& key)
{
    if(key=="TEXTFLAGS") return 0;
    if(key=="TEXT") return 1;
    if(key=="FORMATTEDTEXT") return 2;
    if(key=="FONT") return 3;
    if(key=="TEXTCOLOR") return 4;
    if(key=="SELCOLOR") return 5;
    if(key=="LINKFONT") return 6;
    if(key=="MAXCHARS") return 7;
    if(key=="MAXLINES") return 8;
    if(key=="WRAPINDENTSIZE") return 9;
    if(key=="HORZALIGN") return 10;
    return -1;
}
// TextAreaConfig::Virtual24(flag), TextAreaConfig.cpp
static int Orig75(const GUIString& name)
{
    if(name.CompareNoCase("WordWrap")==0) return 0;
    if(name.CompareNoCase("AllowSelection")==0) return 1;
    if(name.CompareNoCase("ScrollOnBottom")==0) return 2;
    if(name.CompareNoCase("ShowPartialLines")==0) return 3;
    if(name.CompareNoCase("ScrollFullLine")==0) return 4;
    if(name.CompareNoCase("DelayWordWrap")==0) return 5;
    return -1;
}
// TreeAreaConfig::Virtual24(instruction), TreeAreaConfig.cpp
static int Orig76(const std::string& key)
{
    if(key=="LISTSORTTYPE") return 0;
    return -1;
}
// WONSimpleButtonConfig::HandleButtonState(aStateStr), WONButtonConfig.cpp
static int Orig77(const GUIString& name)
{
    if(name.CompareNoCase("Normal")==0) return 0;
    if(name.CompareNoCase("Pressed")==0) return 1;
    if(name.CompareNoCase("Disabled")==0) return 2;
    if(name.CompareNoCase("Over")==0) return 3;
    return -1;
}
// WONSimpleButtonConfig::Virtual24(theInstruction), WONButtonConfig.cpp
static int Orig78(const std::string& key)
{
    if(key=="SCALEIMAGE") return 0;
    if(key=="TYPE") return 1;
    if(key=="BUTTONSTATE") return 2;
    if(key=="BUTTONSTATES") return 3;
    if(key=="CHECKMOUSEBUTTONDOWN") return 4;
    return -1;
}
// WONButtonConfig::HandleSetButtonState(aStateStr), WONButtonConfig.cpp
static int Orig79(const GUIString& name)
{
    if(name.CompareNoCase("Normal")==0) return 0;
    if(name.CompareNoCase("Pressed")==0) return 1;
    if(name.CompareNoCase("Disabled")==0) return 2;
    if(name.CompareNoCase("Over")==0) return 3;
    if(name.CompareNoCase("Default")==0) return 4;
    if(name.CompareNoCase("CheckedNormal")==0) return 5;
    if(name.CompareNoCase("CheckedPressed")==0) return 6;
    if(name.CompareNoCase("CheckedDisabled")==0) return 7;
    if(name.CompareNoCase("CheckedOver")==0) return 8;
    if(name.CompareNoCase("CheckedDefault")==0) return 9;
    return -1;
}
// WONButtonConfig::Virtual24(theInstruction), WONButtonConfig.cpp
static int Orig80(const std::string& key)
{
    if(key=="FONT") return 0;
    if(key=="TYPE") return 1;
    if(key=="TEXTCOLOR") return 2;
    if(key=="DEFAULTTEXTCOLOR") return 3;
    if(key=="VERTALIGN") return 4;
    if(key=="HORZALIGN") return 5;
    if(key=="TRANSPARENT") return 6;
    if(key=="ADJUSTHEIGHTTOFITTEXT") return 7;
    if(key=="WRAP") return 8;
    if(key=="DRAWGRAYTEXT") return 9;
    if(key=="TEXTPADDING") return 10;
    if(key=="FOCUSPADDING") return 11;
    if(key=="HORZTEXTPAD") return 12;
    if(key=="INCREMENTHORZTEXTPAD") return 13;
    if(key=="INCREMENTVERTEXTPAD") return 14;
    if(key=="BUTTONSTATE") return 15;
    if(key=="SCALEIMAGE") return 16;
    if(key=="DRAWFOCUSRECT") return 17;
    return -1;
}
// WONFrameComponentConfig::Virtual24(key), WONFrameConfig.cpp
static int Orig81(const std::string& key)
{
    if(key=="BORDERS") return 0;
    if(key=="IMAGE") return 1;
    return -1;
}
// WONFrameComponentConfig::GetNamedValue(key), WONFrameConfig.cpp
static int Orig82(const std::string& key)
{
    if(key=="LEFTPAD") return 0;
    if(key=="RIGHTPAD") return 1;
    if(key=="TOPPAD") return 2;
    if(key=="BOTTOMPAD") return 3;
    if(key=="HORZPAD") return 4;
    if(key=="VERTPAD") return 5;
    return -1;
}
// WONComboBoxConfig::Virtual24(key), WONFrameValueConfig.cpp
static int Orig83(const std::string& key)
{
    if(key=="DRAWFRAME") return 0;
    if(key=="BUTTONINFRAME") return 1;
    if(key=="FRAME") return 2;
    return -1;
}
// WONTabCtrlConfig::Virtual24(key), WONFrameValueConfig.cpp
static int Orig84(const std::string& key)
{
    if(key=="TRANSPARENT") return 0;
    if(key=="ADDSTANDARDTAB") return 1;
    if(key=="FRAME") return 2;
    return -1;
}
// WONScrollContainerConfig::Virtual24(key), WONFrameValueConfig.cpp
static int Orig85(const std::string& key)
{
    if(key=="DRAWFRAME") return 0;
    if(key=="TRANSPARENT") return 1;
    if(key=="FRAME") return 2;
    if(key=="CORNER") return 3;
    return -1;
}
// WONScrollbarConfig::Virtual24(key), WONListConfig.cpp
static int Orig86(const std::string& key)
{
    if(key=="TRANSPARENT") return 0;
    return -1;
}
// WONHeaderControlConfig::Virtual24(key), WONListConfig.cpp
static int Orig87(const std::string& key)
{
    if(key=="SETSTANDARDCOLUMN") return 0;
    return -1;
}
// WONListCtrlConfig::Virtual24(key), WONListConfig.cpp
static int Orig88(const std::string& key)
{
    if(key=="HEADERHEIGHT") return 0;
    if(key=="NUMCOLUMNS") return 1;
    if(key=="DYNAMICSIZECOLUMN") return 2;
    if(key=="LISTFLAGS") return 3;
    if(key=="SETSTANDARDCOLUMN") return 4;
    if(key=="SETCOLUMN") return 5;
    return -1;
}
// WONSkinConfig::ReadVert(text), WONSkinConfig.cpp
static int Orig89(const GUIString& name)
{
    if(name.CompareNoCase("Vert")==0) return 0;
    return -1;
}
// WONSkinConfig::Virtual24(theInstruction), WONSkinConfig.cpp
static int Orig90(const std::string& key)
{
    if(key=="WINSKIN") return 0;
    if(key=="ADDBUTTONTYPE") return 1;
    if(key=="ADDSIMPLEBUTTONTYPE") return 2;
    if(key=="SETPUSHBUTTON") return 3;
    if(key=="SETCHECKBOX") return 4;
    if(key=="SETRADIOBUTTON") return 5;
    if(key=="SETHEADERBUTTON") return 6;
    if(key=="SETTABBUTTON") return 7;
    if(key=="SETARROWBUTTON") return 8;
    if(key=="SETCOMBOBUTTON") return 9;
    if(key=="SETCLOSEBUTTON") return 10;
    if(key=="SETSCROLLGUTTER") return 11;
    if(key=="SETSCROLLTHUMB") return 12;
    if(key=="SETSCROLLBAR") return 13;
    if(key=="SETSCROLLCONTAINER") return 14;
    if(key=="SETLISTCTRL") return 15;
    if(key=="SETTABCTRL") return 16;
    if(key=="SETCOMBOBOX") return 17;
    if(key=="SETPOPUPBACK") return 18;
    if(key=="SETDIALOGBACK") return 19;
    if(key=="BUTTONCLICKSOUND") return 20;
    if(key=="IMAGECURSOR") return 21;
    return -1;
}
// ChatCtrlConfig::Virtual24(inKey), ChatCtrl.cpp
static int Orig91(const std::string& key)
{
    if(key=="GROUPTYPE") return 0;
    if(key=="CTRLLINKID") return 1;
    if(key=="COLOR_DEFAULT") return 2;
    if(key=="COLOR_ERROR") return 3;
    if(key=="COLOR_HELP") return 4;
    if(key=="COLOR_USER") return 5;
    if(key=="COLOR_EMOTE") return 6;
    if(key=="COLOR_WHISPER") return 7;
    if(key=="COLOR_WARNING") return 8;
    if(key=="COLOR_GROUP") return 9;
    if(key=="COLOR_MESSAGE") return 10;
    if(key=="COLOR_ACCEPT") return 11;
    if(key=="COLOR_REJECT") return 12;
    return -1;
}
// ChatCtrlConfig::Virtual24(group), ChatCtrl.cpp
static int Orig92(const GUIString& name)
{
    if(name.CompareNoCase("CHAT")==0) return 0;
    if(name.CompareNoCase("GAME")==0) return 1;
    if(name.CompareNoCase("TEAM")==0) return 2;
    return -1;
}
// CreditsDialogConfig::Virtual24(inInstruction), CreditsDialog.cpp
static int Orig93(const std::string& key)
{
    if(key=="ADDLINE") return 0;
    return -1;
}
// CrossPromotionButtonConfig::Virtual24(key), CrossPromotionButton.cpp
static int Orig94(const std::string& key)
{
    if(key=="DEFAULTIMAGE") return 0;
    if(key=="DEFAULTBROWSE") return 1;
    return -1;
}
// ImageTextButtonConfig::Virtual24(key), CrossPromotionButton.cpp
static int Orig95(const std::string& key)
{
    if(key=="FONT") return 0;
    if(key=="TEXT") return 1;
    if(key=="BROWSELOCATION") return 2;
    return -1;
}
// GameBrowserCtrlConfig::Virtual24(instruction), GameBrowserCtrl.cpp
static int Orig96(const std::string& key)
{
    if(key=="HOMELOCATION") return 0;
    return -1;
}
// GameUserListCtrlConfig::Virtual24(instruction), GameUserListCtrl.cpp
static int Orig97(const std::string& key)
{
    if(key=="CTRLLINKID") return 0;
    return -1;
}
// RoomLabelCtrlConfig::Virtual24(inKey), RoomLabelCtrl.cpp
static int Orig98(const std::string& key)
{
    if(key=="PREFIX") return 0;
    if(key=="GROUPTYPE") return 1;
    if(key=="SHOWUSERCOUNT") return 2;
    if(key=="SHOWSERVER") return 3;
    return -1;
}
// ServerListCtrlConfig::GetNamedValue(inName), ServerListCtrl.cpp
static int Orig99(const std::string& key)
{
    if(key=="NUMCOLUMNS") return 0;
    return -1;
}
// ServerListCtrlConfig::Virtual24(inInstruction), ServerListCtrl.cpp
static int Orig100(const std::string& key)
{
    if(key=="LISTID") return 0;
    if(key=="ADDCOLUMN") return 1;
    if(key=="DEFAULTFILTER") return 2;
    if(key=="NUMCOLUMNS") return 3;
    if(key=="SETCOLUMNFILTERTYPE") return 4;
    return -1;
}
// ServerListCtrlConfig::Virtual24(aType), ServerListCtrl.cpp
static int Orig101(const GUIString& name)
{
    if(name.CompareNoCase("net")==0) return 0;
    if(name.CompareNoCase("lan")==0) return 1;
    if(name.CompareNoCase("direct")==0) return 2;
    if(name.CompareNoCase("int")==0) return 3;
    if(name.CompareNoCase("string")==0) return 4;
    if(name.CompareNoCase("player")==0) return 5;
    return -1;
}
// UserListCtrlConfig::Virtual24(instruction), UserListCtrl.cpp
static int Orig102(const std::string& key)
{
    if(key=="GROUPTYPE") return 0;
    if(key=="CTRLLINKID") return 1;
    return -1;
}
struct Table { int id; bool name; int (*key)(const std::string&); int (*text)(const GUIString&); };
static const Table kTables[] = {
    { EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_GETPREDEFINEDCOMPONENT_NAME, false, Orig0, NULL },
    { EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION, false, Orig1, NULL },
    { EE_WON_CFG_NAMES_BROWSERCOMPONENTCONFIG_VIRTUAL24_VALUE, true, NULL, Orig2 },
    { EE_WON_CFG_NAMES_BUTTONCONFIG_HANDLEBUTTONFLAGS_FLAG, true, NULL, Orig3 },
    { EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, false, Orig4, NULL },
    { EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, false, Orig5, NULL },
    { EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME, false, Orig6, NULL },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_READDIRECTION_VALUE, true, NULL, Orig7 },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_READHORZALIGN_VALUE, true, NULL, Orig8 },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_READVERTALIGN_VALUE, true, NULL, Orig9 },
    { EE_WON_CFG_KEYS_COMPONENTCONFIG_GETNAMEDVALUE_INNAME, false, Orig10, NULL },
    { EE_WON_CFG_KEYS_COMPONENTCONFIG_CALLNAMEDFUNCTION_INNAME, false, Orig11, NULL },
    { EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEINSTRUCTION_TYPE, false, Orig12, NULL },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_GETCOMPONENTCONFIG_NAME, true, NULL, Orig13 },
    { EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEVALUEINSTRUCTION_KEY, false, Orig14, NULL },
    { EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, false, Orig15, NULL },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_VIRTUAL24_NAME, true, NULL, Orig16 },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_READSTANDARDCURSOR_NAME, true, NULL, Orig17 },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_READCURSOR_TYPE, true, NULL, Orig18 },
    { EE_WON_CFG_NAMES_COMPONENTCONFIG_READSELECTIONCOLOR_TYPE, true, NULL, Orig19 },
    { EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION, false, Orig20, NULL },
    { EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION, false, Orig21, NULL },
    { EE_WON_CFG_KEYS_CONTAINERCONFIG_HANDLEINSTRUCTION_TYPE, false, Orig22, NULL },
    { EE_WON_CFG_KEYS_CONTAINERCONFIG_VIRTUAL24_THEINSTRUCTION, false, Orig23, NULL },
    { EE_WON_CFG_NAMES_CONTAINERCONFIG_HANDLENOOVERLAPLAYOUT_ASTR, true, NULL, Orig24 },
    { EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, false, Orig25, NULL },
    { EE_WON_CFG_KEYS_FONTCONFIG_GETNAMEDVALUE_NAME, false, Orig26, NULL },
    { EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION, false, Orig27, NULL },
    { EE_WON_CFG_NAMES_FONTCONFIG_VIRTUAL24_FLAG, true, NULL, Orig28 },
    { EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION, false, Orig29, NULL },
    { EE_WON_CFG_KEYS_IMAGECONFIG_GETNAMEDVALUE_INNAME, false, Orig30, NULL },
    { EE_WON_CFG_KEYS_IMAGECONFIG_VIRTUAL24_ININSTRUCTION, false, Orig31, NULL },
    { EE_WON_CFG_NAMES_IMAGEFILTERCONFIG_HANDLEFILTER_NAME, true, NULL, Orig32 },
    { EE_WON_CFG_KEYS_IMAGEFILTERCONFIG_VIRTUAL24_ININSTRUCTION, false, Orig33, NULL },
    { EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, false, Orig34, NULL },
    { EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, false, Orig35, NULL },
    { EE_WON_CFG_NAMES_LISTAREACONFIG_READSORTTYPE_NAME, true, NULL, Orig36 },
    { EE_WON_CFG_NAMES_LISTAREACONFIG_READLISTFLAGS_NAME, true, NULL, Orig37 },
    { EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION, false, Orig38, NULL },
    { EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, false, Orig39, NULL },
    { EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY, false, Orig40, NULL },
    { EE_WON_CFG_NAMES_MSARROWBUTTONCONFIG_VIRTUAL24_DIRECTION, true, NULL, Orig41 },
    { EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY, false, Orig42, NULL },
    { EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY, false, Orig43, NULL },
    { EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY, false, Orig44, NULL },
    { EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY, false, Orig45, NULL },
    { EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY, false, Orig46, NULL },
    { EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY, false, Orig47, NULL },
    { EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, false, Orig48, NULL },
    { EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY, false, Orig49, NULL },
    { EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY, false, Orig50, NULL },
    { EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, false, Orig51, NULL },
    { EE_WON_CFG_NAMES_MULTILISTAREACONFIG_VIRTUAL24_NAME, true, NULL, Orig52 },
    { EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION, false, Orig53, NULL },
    { EE_WON_CFG_KEYS_RESOURCECONFIG_HANDLEVALUEINSTRUCTION_KEY, false, Orig54, NULL },
    { EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION, false, Orig55, NULL },
    { EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION, false, Orig56, NULL },
    { EE_WON_CFG_NAMES_SCROLLBARSCROLLERCONFIG_GETSCROLLBARCONDITION_CONDITION, true, NULL, Orig57 },
    { EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION, false, Orig58, NULL },
    { EE_WON_CFG_NAMES_BUTTONSCROLLERCONFIG_GETSCROLLBUTTONCONDITION_ASTR, true, NULL, Orig59 },
    { EE_WON_CFG_KEYS_BUTTONSCROLLERCONFIG_VIRTUAL24_THEINSTRUCTION, false, Orig60, NULL },
    { EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, false, Orig61, NULL },
    { EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME, false, Orig62, NULL },
    { EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY, false, Orig63, NULL },
    { EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY, false, Orig64, NULL },
    { EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY, false, Orig65, NULL },
    { EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY, false, Orig66, NULL },
    { EE_WON_CFG_NAMES_SPLITTERCONFIG_READQUADRANT_ASTR, true, NULL, Orig67 },
    { EE_WON_CFG_NAMES_SPLITTERCONFIG_READSPLITTERTYPE_ASTR, true, NULL, Orig68 },
    { EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, false, Orig69, NULL },
    { EE_WON_CFG_KEYS_STRETCHIMAGECONFIG_VIRTUAL24_INSTRUCTION, false, Orig70, NULL },
    { EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION, false, Orig71, NULL },
    { EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY, false, Orig72, NULL },
    { EE_WON_CFG_KEYS_TABCTRLCONFIG_GETPREDEFINEDCOMPONENT_NAME, false, Orig73, NULL },
    { EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, false, Orig74, NULL },
    { EE_WON_CFG_NAMES_TEXTAREACONFIG_VIRTUAL24_FLAG, true, NULL, Orig75 },
    { EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION, false, Orig76, NULL },
    { EE_WON_CFG_NAMES_WONSIMPLEBUTTONCONFIG_HANDLEBUTTONSTATE_ASTATESTR, true, NULL, Orig77 },
    { EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, false, Orig78, NULL },
    { EE_WON_CFG_NAMES_WONBUTTONCONFIG_HANDLESETBUTTONSTATE_ASTATESTR, true, NULL, Orig79 },
    { EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, false, Orig80, NULL },
    { EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY, false, Orig81, NULL },
    { EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_GETNAMEDVALUE_KEY, false, Orig82, NULL },
    { EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY, false, Orig83, NULL },
    { EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY, false, Orig84, NULL },
    { EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, false, Orig85, NULL },
    { EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY, false, Orig86, NULL },
    { EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY, false, Orig87, NULL },
    { EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, false, Orig88, NULL },
    { EE_WON_CFG_NAMES_WONSKINCONFIG_READVERT_TEXT, true, NULL, Orig89 },
    { EE_WON_CFG_KEYS_WONSKINCONFIG_VIRTUAL24_THEINSTRUCTION, false, Orig90, NULL },
    { EE_WON_CFG_KEYS_CHATCTRLCONFIG_VIRTUAL24_INKEY, false, Orig91, NULL },
    { EE_WON_CFG_NAMES_CHATCTRLCONFIG_VIRTUAL24_GROUP, true, NULL, Orig92 },
    { EE_WON_CFG_KEYS_CREDITSDIALOGCONFIG_VIRTUAL24_ININSTRUCTION, false, Orig93, NULL },
    { EE_WON_CFG_KEYS_CROSSPROMOTIONBUTTONCONFIG_VIRTUAL24_KEY, false, Orig94, NULL },
    { EE_WON_CFG_KEYS_IMAGETEXTBUTTONCONFIG_VIRTUAL24_KEY, false, Orig95, NULL },
    { EE_WON_CFG_KEYS_GAMEBROWSERCTRLCONFIG_VIRTUAL24_INSTRUCTION, false, Orig96, NULL },
    { EE_WON_CFG_KEYS_GAMEUSERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION, false, Orig97, NULL },
    { EE_WON_CFG_KEYS_ROOMLABELCTRLCONFIG_VIRTUAL24_INKEY, false, Orig98, NULL },
    { EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_GETNAMEDVALUE_INNAME, false, Orig99, NULL },
    { EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_VIRTUAL24_ININSTRUCTION, false, Orig100, NULL },
    { EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE, true, NULL, Orig101 },
    { EE_WON_CFG_KEYS_USERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION, false, Orig102, NULL },
};

static void Run()
{
    // every name of every table, and its variants
    std::vector<std::string> inputs;
    inputs.push_back("");
    int count = ee_won_gui_cfg_table_count();
    CHECK(count == (int)(sizeof(kTables) / sizeof(kTables[0])));
    for (int t = 0; t < count; t++) {
        for (size_t i = 0;; i++) {
            char buf[256];
            int64_t len;
            ee_won_gui_cfg_table_name(t, i, (uint8_t*)buf, sizeof buf, &len);
            if (len < 0) break;
            std::string s(buf, (size_t)len);
            inputs.push_back(s);
            std::string u = s, l = s, w = s;
            for (size_t k = 0; k < s.size(); k++) {
                u[k] = (char)toupper((unsigned char)s[k]);
                l[k] = (char)tolower((unsigned char)s[k]);
                w[k] = isupper((unsigned char)s[k]) ? (char)tolower((unsigned char)s[k]) : (char)toupper((unsigned char)s[k]);
            }
            inputs.push_back(u);
            inputs.push_back(l);
            inputs.push_back(w);
            inputs.push_back(s + "X");
            inputs.push_back(s + " ");
            if (!s.empty()) inputs.push_back(s.substr(0, s.size() - 1));
            inputs.push_back(std::string(s.c_str(), s.size()) + std::string(1, '\0'));
        }
    }
    for (size_t t = 0; t < sizeof(kTables) / sizeof(kTables[0]); t++) {
        const Table& tb = kTables[t];
        CHECK(tb.id == (int)t);
        for (size_t i = 0; i < inputs.size(); i++) {
            const std::string& s = inputs[i];
            if (!tb.name) {
                CHECK(tb.key(s) == ee_won_gui_cfg_key(tb.id, s.data(), s.size()));
            } else {
                GUIString g(s);
                CHECK(tb.text(g) == ee_won_gui_cfg_name(tb.id, g.mData->mText, g.mData->mLength, sizeof(wchar_t)));
            }
        }
    }
}

int main()
{
    Run();
    if (setlocale(LC_ALL, "English_United States.1252") || setlocale(LC_ALL, "en_US.UTF-8") || setlocale(LC_ALL, "C.UTF-8"))
        Run();
    printf("won_guiconf_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
