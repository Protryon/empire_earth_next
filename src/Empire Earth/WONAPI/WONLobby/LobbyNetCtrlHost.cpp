// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The Internet lobby's controls in Rust (R6 WONLobby round 13): ServerListCtrl and its rows (srvlistctl.rs) are
// lobbyctl.rs kinds. This unit is their host's ops (LobbyCtrlHost passes it every op from 1000 up: each op the
// original's statements on a control, a row, a text, a graphics context or a lobby global, with no decision), the rows'
// virtuals (one forward each), the filter combo's row and the game list's config reader.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/srvlistctl.h"
using namespace WONAPI;

// The filter combo's add/remove row (RTTI .?AVFilterListItem@@): a TextListItem with no new virtuals
class FilterListItem : public TextListItem {
public:
    FilterListItem(const GUIString& inColumn, Font* inFont)
        : TextListItem(gEmptyGUIString, inFont), mIsAdd(false), mSelPos(0), mColumnName(inColumn) {}
    bool mIsAdd; // 0x28: true when the row adds the text, false when it removes it
    int mSelPos; // 0x2c: position of the typed text in the combo list
    GUIString mFilterText; // 0x30: the typed filter text
    GUIString mColumnName; // 0x34: the column the combo filters
};

namespace WONAPI {
void DrawLobbyPing(Graphics* inGraphics, unsigned short inPing, int inHeight, int inAlignment, int inWidth);

// ---- the game list's rows: one forward each into the list control (the list's grandparent)

static intptr_t RowCall(ListArea* inList, EeWonLobbyCtlRowCall inCall, const void* inItem, intptr_t a = 0,
                        intptr_t b = 0, intptr_t c = 0)
{
    intptr_t anArgs[3] = {a, b, c};
    LobbyRustCtrl* aCtl = static_cast<ServerListCtrl*>(inList->mParent->mParent);
    return ee_won_lobbyctl_call(aCtl->mLobbyCtl, EeWonLobbyCtlCall::Row, (intptr_t)inCall, (intptr_t)inItem,
                                (intptr_t)anArgs);
}
static intptr_t Pt(int x, int y) { return (intptr_t)(uint32_t)x | ((intptr_t)(uint32_t)y << 32); }
void ServerDetailsItem::Paint(Graphics& g, ListArea* inList)
{ RowCall(inList, EeWonLobbyCtlRowCall::Paint, this, (intptr_t)&g, (intptr_t)inList); }
void ServerDetailsItem::CalcDimensions(ListArea* inList)
{ RowCall(inList, EeWonLobbyCtlRowCall::CalcDimensions, this, (intptr_t)inList); }
int ServerDetailsItem::Compare(const ListItem* inOther, ListArea* inList) const
{
    return (int)RowCall(inList, EeWonLobbyCtlRowCall::Compare, this, (intptr_t)inOther, (intptr_t)inList,
                        static_cast<const ListItem*>(this) - inOther);
}
bool ServerDetailsItem::CanSelect(ListArea* inList, int x, int y)
{ return RowCall(inList, EeWonLobbyCtlRowCall::CanSelect, this, (intptr_t)inList, Pt(x, y)) != 0; }
void ServerDetailsItem::MouseDown(ListArea* inList, int x, int y, int inButton)
{ RowCall(inList, EeWonLobbyCtlRowCall::MouseDown, this, (intptr_t)inList, Pt(x, y), inButton); }

ServerDetailsItem::ItemFactoryFunc ServerDetailsItem::mItemFactoryFunc = ServerDetailsItem::DefaultItemFactory;

// ---- the host

static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
static intptr_t Dat(const GUIString& inText) { return (intptr_t)inText.mData.get(); }
static int Lo(intptr_t v) { return (int)(int32_t)(v & 0xffffffff); }
static int Hi(intptr_t v) { return (int)(v >> 32); }
static const GUIString& ResText(intptr_t inRes) { return gLobbyResources[inRes].mString; }
static Image* ResImage(intptr_t inRes) { return gLobbyResources[inRes].mImage.get(); }

static intptr_t ItemField(ListItem* inItem, intptr_t inField, bool inSet, intptr_t inValue)
{
    typedef EeWonLobbyCtlItemField F;
    ServerDetailsItem* aRow = static_cast<ServerDetailsItem*>(inItem);
    switch ((F)inField) {
    case F::Height: if (inSet) inItem->mHeight = (int)inValue; return inItem->mHeight;
    case F::Top: if (inSet) inItem->mTop = (int)inValue; return inItem->mTop;
    case F::Width: if (inSet) inItem->mWidth = (int)inValue; return inItem->mWidth;
    case F::Selected: if (inSet) inItem->mSelected = inValue != 0; return inItem->mSelected ? 1 : 0;
    case F::Selectable: if (inSet) inItem->mSelectable = inValue != 0; return inItem->mSelectable ? 1 : 0;
    case F::IconId: if (inSet) aRow->mIconId = (int)inValue; return aRow->mIconId;
    case F::SummaryHeight:
        if (inSet) aRow->mSummaryHeight = (unsigned char)inValue;
        return aRow->mSummaryHeight;
    }
    return 0;
}

intptr_t LobbyNetCtrlHost(LobbyRustCtrl* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    Component* aComp = reinterpret_cast<Component*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    MultiListArea* aMulti = reinterpret_cast<MultiListArea*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    ServerDetailsItem* aRow = reinterpret_cast<ServerDetailsItem*>(a);
    FilterListItem* aFilter = reinterpret_cast<FilterListItem*>(a);
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    // the model's ops are answered by its Rust objects (modelobj.rs) before the host
    case Op::RowNew: {
        ServerDetailsItem* aNew = ServerDetailsItem::CreateItem();
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::RowGame: return (intptr_t)aRow->mGame.get();
    case Op::RowSetGame: aRow->mGame = reinterpret_cast<LobbyGame*>(b); return 0;
    case Op::RowSetAddrText: aRow->mIPAddrStr = aRow->mGame->GetAddress().GetHostAndPortString(true); return 0;
    case Op::RowAddrText: return Dat(aRow->mIPAddrStr);
    case Op::ItemGet: return ItemField(reinterpret_cast<ListItem*>(a), b, false, 0);
    case Op::ItemSet: ItemField(reinterpret_cast<ListItem*>(a), b, true, c); return 0;
    case Op::RowDetailsHeight: return aRow->GetDetailsHeightHook(reinterpret_cast<MultiListArea*>(b));
    case Op::RowPaintDetails:
        aRow->PaintDetailsHook(*reinterpret_cast<Graphics*>(b), reinterpret_cast<MultiListArea*>(c));
        return 0;
    case Op::DefaultFont: return (intptr_t)aComp->GetDefaultFont();
    case Op::DefaultFontMod: return (intptr_t)aComp->GetDefaultFontMod((int)b, (int)c);
    case Op::FontMod: return (intptr_t)aComp->GetFontMod(reinterpret_cast<Font*>(b), (int)c, 0);
    case Op::ListFont: return (intptr_t)aList->mFont.get();
    case Op::FontHeight: return reinterpret_cast<Font*>(a)->GetHeight();
    case Op::ResImageHeight: return gLobbyResources[a].mImage->mHeight;
    case Op::ColumnWidth: return aMulti->GetColumnWidth((int)b);
    case Op::ListEnsureVisible: aList->EnsureVisible(reinterpret_cast<ListItem*>(b)); return 0;
    case Op::ListItemPos: return aList->GetItemPos(reinterpret_cast<ListItem*>(b));
    case Op::ListSetSel: aList->SetSelItem((int)b, c != 0); return 0;
    case Op::ListVertOffset: return aList->GetVertOffset();
    case Op::ListSetVertOffset: aList->SetVertOffset((int)b); return 0;
    case Op::ListViewHeight: return aList->GetViewHeight();
    case Op::ListItemAtY: return aList->GetItemPosAtYPos((int)b);
    case Op::ListPaintWidth: return aList->GetPaintWidth();
    case Op::ListPaintColumn: return aMulti->GetColumnPaintContext();
    case Op::ListSortColumn: return aMulti->GetSortColumn();
    case Op::ListSortColumnPrev: return aMulti->GetSortColumnPrev();
    case Op::ListDeliverPing: aList->DeliverEvent(new PingGameEvent(reinterpret_cast<LobbyGame*>(b), true), true); return 0;
    case Op::ListTotalWidth: return aList->GetTotalWidth();
    case Op::ListTextColor: return aList->GetTextColor();
    case Op::ListFlagSet: return aList->ListFlagSet((int)b) ? 1 : 0;
    case Op::GfxStandardColor: return g->GetStandardColor((StandardColor)b);
    case Op::GfxSelFore:
        return reinterpret_cast<ListArea*>(b)->GetSelColor().GetForegroundColor(*g, (unsigned long)c);
    case Op::GfxSelBack: return reinterpret_cast<ListArea*>(b)->GetSelColor().GetBackgroundColor(*g);
    case Op::GfxSetColor: g->SetColor((unsigned long)b); return 0;
    case Op::GfxSetFont: g->SetFont(reinterpret_cast<Font*>(b)); return 0;
    case Op::GfxSetSysColor: g->SetSysColor((StandardColor)b); return 0;
    case Op::GfxFillRect: g->FillRect(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxTranslate: g->Translate((int)b, (int)c); return 0;
    case Op::GfxDrawResImage: g->DrawImage(ResImage(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawText: g->DrawString(Txt(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawResText: g->DrawString(ResText(b), Lo(c), Hi(c)); return 0;
    case Op::GfxTextWidth: return g->mFont->GetStringWidth(Txt(b));
    case Op::GfxDrawPing: DrawLobbyPing(g, (unsigned short)b, (int)c, 0, 0); return 0;
    case Op::GfxDrawLine: g->DrawLine(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawImage: g->DrawImage(reinterpret_cast<Image*>(b), Lo(c), Hi(c)); return 0;
    case Op::TextNew: {
        GUIStringData* aData = new GUIStringData(reinterpret_cast<const char*>(a), (unsigned short)b);
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::TextCompareNoCase: return Txt(a).CompareNoCase(Txt(b));
    case Op::TextFindNoCase: return Txt(a).findNoCase(Txt(b), 0);
    case Op::TextFindRes: return Txt(a).findNoCase(ResText(b), 0);
    case Op::TextLen: return Txt(a).length();
    case Op::TextAt: return Txt(a)[(int)b];
    case Op::TextBytes: {
        std::string aBytes = Txt(a);
        memcpy(reinterpret_cast<char*>(b), aBytes.data(), aBytes.length() < (size_t)c ? aBytes.length() : (size_t)c);
        return (intptr_t)aBytes.length();
    }
    case Op::TextEquals: return Txt(a) == Txt(b) ? 1 : 0;
    case Op::GameCompareAddress:
        return reinterpret_cast<LobbyGame*>(a)->GetAddress().StringCompare(reinterpret_cast<LobbyGame*>(b)->GetAddress());
    case Op::ComboEditText: return Dat(aCombo->mEdit->GetText());
    case Op::PingsToDo: return LobbyGetNumPingsToDo();
    case Op::KillPings: LobbyKillPings(1); return 0;
    case Op::DeliverGameAction: inCtl->DeliverEvent(new GameActionEvent(7, reinterpret_cast<LobbyGame*>(a)), false); return 0;
    case Op::DeliverJoinGame: inCtl->DeliverEvent(new JoinGameEvent(reinterpret_cast<LobbyGame*>(a)), false); return 0;
    case Op::EventGame: return (intptr_t) static_cast<GameChangedEvent*>(anEvent)->mGame.get();
    case Op::EventGames: return (intptr_t) static_cast<SyncGameListEvent*>(anEvent)->mGames.get();
    case Op::EventX: return static_cast<ListItemClickedEvent*>(anEvent)->mX;
    case Op::EventY: return static_cast<ListItemClickedEvent*>(anEvent)->mY;
    case Op::EventItem:
        if (anEvent->mType == ComponentEvent_ComboSelChanged)
            return (intptr_t) static_cast<ComboSelChangedEvent*>(anEvent)->mItem.get();
        if (anEvent->mType == ComponentEvent_TreeItemOpened)
            return (intptr_t) static_cast<TreeItemOpenedEvent*>(anEvent)->mItem.get();
        return (intptr_t) static_cast<ListItemClickedEvent*>(anEvent)->mItem.get();
    case Op::EventActiveSel: return static_cast<ComboSelChangedEvent*>(anEvent)->mActiveSel ? 1 : 0;
    case Op::LabelSetPingSummary: {
        char aShown[33];
        sprintf(aShown, "%d", (int)b);
        reinterpret_cast<Label*>(a)->SetText(LobbyFormatString(LOBBY_STRING(ServerListCtrl_ListPingSummary_String), aShown), true);
        return 0;
    }
    case Op::LabelSetListSummary: {
        char aShown[33], aTotal[33];
        sprintf(aTotal, "%d", (int)c);
        sprintf(aShown, "%d", (int)b);
        reinterpret_cast<Label*>(a)->SetText(LobbyFormatString2(LOBBY_STRING(ServerListCtrl_ListSummary_String), aShown, aTotal), true);
        return 0;
    }
    case Op::ButtonSetRes: reinterpret_cast<Button*>(a)->SetText(ResText(b), true); return 0;
    case Op::LabelSetRes: reinterpret_cast<Label*>(a)->SetText(ResText(b), true); return 0;
    case Op::RequestTimer: inCtl->RequestTimer(a != 0); return 0;
    case Op::ClearTimerFlag: inCtl->mTimerFlags &= ~2; return 0;
    case Op::BaseTimer: return inCtl->Container::TimerEvent((int)a) ? 1 : 0;
    case Op::ComboInsertNoFilter:
        aCombo->InsertItem(new TextListItem(LOBBY_STRING(ServerListCtrl_NoFilter_String), reinterpret_cast<Font*>(b)), -1);
        return 0;
    case Op::ComboInsertFilterRow: aCombo->InsertItem(new FilterListItem(Txt(b), reinterpret_cast<Font*>(c)), -1); return 0;
    case Op::ComboAddFilterValues: {
        LobbyOptionSet aFilters = LobbyGetFilterValues(Txt(b));
        for (size_t i = 0; i < aFilters.size(); ++i) aCombo->AddString(aFilters.ValueAt(i), -1);
        return 0;
    }
    case Op::ComboItem: return (intptr_t)aCombo->mListArea->GetItem((int)b);
    case Op::ComboCount: return aCombo->mListArea->GetNumItems();
    case Op::ComboItemPos: return aCombo->mListArea->GetItemPos(reinterpret_cast<ListItem*>(b));
    case Op::TextItemIs: return reinterpret_cast<TextListItem*>(a)->mText == Txt(b) ? 1 : 0;
    case Op::ComboUpdateItem: aCombo->UpdateItem(reinterpret_cast<TextListItem*>(b)); return 0;
    case Op::ComboSetText: aCombo->SetText(Txt(b), true); return 0;
    case Op::ComboAddString: return (intptr_t)aCombo->AddString(Txt(b), -1);
    case Op::ComboDeleteItem: aCombo->DeleteItem((int)b); return 0;
    case Op::ComboSelectEnd: aCombo->mEdit->SelectRange(-1, 0); return 0;
    case Op::FilterIsAdd: return aFilter->mIsAdd ? 1 : 0;
    case Op::FilterSelPos: return aFilter->mSelPos;
    case Op::FilterText: return Dat(aFilter->mFilterText);
    case Op::FilterColumn: return Dat(aFilter->mColumnName);
    case Op::FilterClear:
        aFilter->mText = LOBBY_STRING(ServerListCtrl_AddRemove_String);
        aFilter->mIsAdd = true;
        aFilter->mFilterText = GUIString("");
        aFilter->mSelPos = 0;
        return 0;
    case Op::FilterSet: {
        aFilter->mSelPos = Lo(b);
        aFilter->mIsAdd = Hi(b) != 0;
        aFilter->mFilterText = Txt(c);
        GUIString aLabel;
        if (aFilter->mIsAdd) aLabel = LOBBY_STRING(ServerListCtrl_Add_String);
        else aLabel = LOBBY_STRING(ServerListCtrl_Remove_String);
        aLabel.append(Txt(c));
        aLabel.append(GUIString(new GUIStringData("\")")));
        aFilter->mText = aLabel;
        return 0;
    }
    case Op::AddFilterValue: LobbyAddFilterValue(Txt(a), Txt(b)); return 0;
    case Op::RemoveFilterValue: LobbyRemoveFilterValue(Txt(a), Txt(b)); return 0;
    case Op::EeColumn:
        if ((EeWonEeGlueCall)a == EeWonEeGlueCall::EpochKeep || (EeWonEeGlueCall)a == EeWonEeGlueCall::MapTypeKeep) {
            GUIString aText = Txt(c);
            return ee_won_eeglue_call((EeWonEeGlueCall)a, b, (intptr_t)&aText, 0);
        }
        return ee_won_eeglue_call((EeWonEeGlueCall)a, b, c, 0);
    default: break;
    }
    return 0;
}

// ---- the game list's config reader (ServerListCtrl.cpp's)

void ServerListCtrlConfig::SetComponent(Component* inComponent)
{
    ContainerConfig::SetComponent(inComponent);
    this->mServerList = static_cast<ServerListCtrl*>(inComponent);
}
// Answers NUMCOLUMNS with the list's column count, else asks the base config
bool ServerListCtrlConfig::GetNamedValue(const std::string& inName, int& outValue)
{
    const int aKeyIdx = ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_GETNAMEDVALUE_INNAME, inName.data(), inName.size());
    if (aKeyIdx == 0) { outValue = (int)this->mServerList->Cfg(EeWonLobbyCtlCfg::ColumnCount); return true; }
    return ComponentConfig::GetNamedValue(inName, outValue);
}
// Handles the LISTID, ADDCOLUMN, DEFAULTFILTER, NUMCOLUMNS and SETCOLUMNFILTERTYPE layout instructions
bool ServerListCtrlConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx = ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_VIRTUAL24_ININSTRUCTION, inInstruction.data(), inInstruction.size());
    if (ContainerConfig::Virtual24(inInstruction)) return true;
    if (aKeyIdx == 0) {
        GUIString aType = this->ReadString(false);
        const int aName = ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE, aType.mData->mText, aType.mData->mLength, sizeof(wchar_t));
        if (aName < 0 || aName > 2) throw ConfigObjectException("Invalid server list id: " + static_cast<std::string>(aType));
        this->mServerList->Cfg(EeWonLobbyCtlCfg::SetGameType, aName == 0 ? 2 : aName == 1 ? 1 : 3);
    } else if (aKeyIdx == 1) this->HandleAddColumn();
    else if (aKeyIdx == 2) {
        GUIString aColumn = this->ReadString(false);
        this->EnsureComma();
        GUIString aFilters = this->ReadString(false);
        LobbyAddGameOption(aColumn, aFilters, false);
    } else if (aKeyIdx == 3) this->mServerList->Cfg(EeWonLobbyCtlCfg::SetNumColumns, this->ReadInt());
    else if (aKeyIdx == 4) {
        this->ReadInt();
        this->EnsureComma();
        GUIString aType = this->ReadString(false);
        // the filter type is only validated; ReadInt's column and the type are not kept
        const int aName = ee_won_gui_cfg_name(EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE, aType.mData->mText, aType.mData->mLength, sizeof(wchar_t));
        if (aName < 3 || aName > 5) throw ConfigObjectException("Unknown filter type: " + static_cast<std::string>(aType));
    } else return false;
    return true;
}
// Adds a defined column (header component, width, filter combo) if it applies to the list's game type
void ServerListCtrlConfig::HandleAddColumn()
{
    MultiListArea* aList = (MultiListArea*)this->mServerList->Cfg(EeWonLobbyCtlCfg::List);
    if (aList == NULL) {
        Component* aComponent = this->Fn386860(std::string("ServerList"));
        if (aComponent == NULL) throw ConfigObjectException("Need ServerList before calling AddColumn");
        aList = dynamic_cast<MultiListArea*>(aComponent);
        if (aList == NULL) throw ConfigObjectException("ServerList needs to be a MultiListArea");
        this->mServerList->Cfg(EeWonLobbyCtlCfg::SetList, (intptr_t)aList);
        aComponent = this->Fn386860(std::string("ServerListHeader"));
        if (aComponent == NULL) throw ConfigObjectException("Need ServerList header before calling AddColumn");
        this->mHeader = dynamic_cast<MultiListHeader*>(aComponent);
        if (this->mHeader == NULL) throw ConfigObjectException("ServerListHeader needs to be a MultiListHeader");
    }
    GUIString aColumn = this->ReadString(false);
    this->EnsureComma();
    Component* aComponent = this->Fn386860(aColumn);
    if (aComponent == NULL) throw ConfigObjectException("Component not found: " + static_cast<std::string>(aColumn));
    int aWidth = this->ReadInt();
    ComboBox* aCombo = NULL;
    if (!this->EndOfString()) {
        this->EnsureComma();
        aCombo = SafeConfigGetComponent<ComboBox*>(this->ReadComponent(), "ComboBox");
    }
    std::string aName = aColumn;
    intptr_t aDef = this->mServerList->Cfg(EeWonLobbyCtlCfg::AddColumn, (intptr_t)aName.data(), (intptr_t)aName.size());
    if (aDef < 0) throw ConfigObjectException("Column not defined: " + aName);
    if (aDef == 0) return;
    aList->SetNumColumns(++this->mNumColumns);
    int aCol = this->mNumColumns;
    if (aWidth == 0) aList->SetDynamicSizeColumn(--aCol);
    else aList->SetColumnWidth(--aCol, aWidth, false);
    this->mHeader->SetColumnItem(this->mNumColumns - 1, aComponent);
    this->mServerList->Cfg(EeWonLobbyCtlCfg::PushColumn, aDef - 1, (intptr_t)aCombo);
}
// Creates a game list layout config
ComponentConfig* ServerListCtrlConfig::CfgFactory() { return new ServerListCtrlConfig; }
// Creates the layout config with no header and no columns
ServerListCtrlConfig::ServerListCtrlConfig()
{
    this->mHeader = NULL;
    this->mNumColumns = 0;
}
}
