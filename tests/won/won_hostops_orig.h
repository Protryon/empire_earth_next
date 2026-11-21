// The control hosts' code R6 WONLobby round 29 moved to Rust (ee_won_lobby hostops.rs), as it was (copied by
// script from the units at c01702f3f), for won_hostops_test over won_hostops_world.h's stand-ins.
#ifndef WON_HOSTOPS_ORIG_H
#define WON_HOSTOPS_ORIG_H
#define ee_won_ipaddr_clone WonWorld::IpClone
namespace WonHostopsOrig {
using namespace WonWorld;

// ---- DrawLobbyPing (MOTDCtrl.cpp's): the game list's and the game room's ping bars
// The ping ceiling (0x8df4cc) starts at 5000 and is taken from the lobby config while it is still 5000
static unsigned long sLobbyPingCeiling=5000;
// The text is always empty, so its branches never draw anything (as in the original)
// Draws a ping as 1-10 green bars scaled by the config's ping ceiling, or the searching or failed icon
void DrawLobbyPing(Graphics* inGraphics,unsigned short inPing,int inHeight,int inAlignment,int inWidth)
{
    if(inPing==0xfffe) return;
    if(sLobbyPingCeiling==5000) {
        LobbyConfig* config=GetLobbyConfig();
        if(config!=NULL) sLobbyPingCeiling=config->mPingCeiling;
    }
    GUIString text(GetEmptyGUIStringData());
    SmartPtr<Image> image;
    int bars=0;
    if(inPing==0xfffd) image=LOBBY_IMAGE(LobbyGlobal_Searching_Image).get();
    else if(inPing==0xfffc) { image=LOBBY_IMAGE(LobbyGlobal_Searching_Image); image=image.get(); }
    else if(inPing==0xffff) image=LOBBY_IMAGE(ServerListCtrl_PingFailed_Image).get();
    else {
        bars=10-int((static_cast<unsigned long>(inPing)*10)/sLobbyPingCeiling);
        if(bars<1) bars=1;
        else if(bars>10) bars=10;
    }
    int x=0;
    if(inAlignment!=0) {
        int width;
        if(text.length()!=0) width=inGraphics->mFont->GetStringWidth(text,0,text.length());
        else if(image.get()!=NULL) width=image->GetWidth();
        else width=bars*3;
        if(inAlignment==1) x=(inWidth-width)/2;
        else x=inWidth-width;
        if(x<0) x=0;
    }
    if(text.length()!=0) inGraphics->DrawString(text,x,0);
    else if(image.get()!=NULL) {
        int y=(inHeight-image->GetHeight())/2;
        if(y<0) y=0;
        inGraphics->DrawImage(image.get(),x,y);
    } else {
        inGraphics->SetColor(0x10d810);
        int height=inHeight-2;
        while(bars>0) {
            inGraphics->FillRect(x,1,2,height);
            x+=3;
            --bars;
        }
    }
}

// ---- the hosts' cases (their switches' local declarations as the hosts had them)
class FilterListItem : public TextListItem {
public:
    FilterListItem(const GUIString& inColumn, Font* inFont)
        : TextListItem(gEmptyGUIString, inFont), mIsAdd(false), mSelPos(0), mColumnName(inColumn) {}
    bool mIsAdd; // 0x28: true when the row adds the text, false when it removes it
    int mSelPos; // 0x2c: position of the typed text in the combo list
    GUIString mFilterText; // 0x30: the typed filter text
    GUIString mColumnName; // 0x34: the column the combo filters
};

static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
static int Lo(intptr_t v) { return (int)(int32_t)(v & 0xffffffff); }
static int Hi(intptr_t v) { return (int)(v >> 32); }

// LobbyNetCtrlHost's
static intptr_t Net(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    FilterListItem* aFilter = reinterpret_cast<FilterListItem*>(a);
    TextListItem* aText = static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a));
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    (void)inCtl;
    switch (inOp) {
    case Op::ItemSetTextCount: {
        char aCount[50];
        sprintf(aCount, " (%d)", Lo(c));
        aText->mText = Txt(b);
        if (Hi(c) != 0) aText->mText.append(aCount);
        return 0;
    }
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
    case Op::ComboAddFilterValues: {
        GUIString aColumn = Txt(b);
        for (size_t i = 0, n = LobbyGetFilterCount(aColumn); i < n; ++i) aCombo->AddString(LobbyGetFilterValueAt(aColumn, i), -1);
        return 0;
    }
    case Op::EventItem:
        if (anEvent->mType == ComponentEvent_ComboSelChanged)
            return (intptr_t) static_cast<ComboSelChangedEvent*>(anEvent)->mItem.get();
        if (anEvent->mType == ComponentEvent_TreeItemOpened)
            return (intptr_t) static_cast<TreeItemOpenedEvent*>(anEvent)->mItem.get();
        return (intptr_t) static_cast<ListItemClickedEvent*>(anEvent)->mItem.get();
    default: return INTPTR_MIN;
    }
    return 0;
}
// its SmallOp's
static intptr_t Small(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    (void)b; (void)c;
    switch (inOp) {
    case Op::DeliverJoinRoom: {
        // the row's address and room id (rows.rs)
        const EeWonIpAddr* anAddress = (const EeWonIpAddr*)ee_won_lobby_rows_ctl_op(Op::RoomItemAddr, a, 0, 0);
        unsigned long anId = (unsigned long)ee_won_lobby_rows_ctl_op(Op::RoomItemGroupId, a, 0, 0);
        inCtl->DeliverEvent(JoinRoomEvent(IPAddr::FromValue(ee_won_ipaddr_clone(anAddress)), anId), false);
        return 0;
    }
    case Op::DeliverCreateRoom: {
        IPAddr anAddress;
        if (a != 0) anAddress = IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a));
        inCtl->DeliverEvent(CreateRoomEvent(anAddress), false);
        return 0;
    }
    case Op::ConfigIsPublic: { LobbyConfig* aConfig = GetLobbyConfig(); return aConfig != NULL && aConfig->mPublic ? 1 : 0; }
    default: return INTPTR_MIN;
    }
    return 0;
}
// LobbyCtrlHost's
static intptr_t Ctl(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    Label* aLabel = reinterpret_cast<Label*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    (void)inCtl; (void)c;
    switch (inOp) {
    case Op::LabelSetNumber: {
        char aPort[20]; // a 20-byte buffer, as in the original
        sprintf(aPort, "%d", (int)b);
        aLabel->SetText(aPort, true);
        return 0;
    }
    case Op::ComboAddNumber: {
        char aBuffer[50];
        sprintf(aBuffer, "%d", (int)b);
        aCombo->AddString(aBuffer);
        return 0;
    }
    case Op::LabelSetPercent: {
        char theBuffer[50];
        sprintf(theBuffer, "%d", (int)b);
        aLabel->SetText(LobbyFormatString(EE_LOBBY_STRING(gEELobbyPlayerOptionsCtrl_TransferPercent), theBuffer));
        return 0;
    }
    case Op::ChooseSavedGame: {
        // the saved games' directory, the file dialog over its .ees files (no dialog: -1)
        GUIString theDirectory = EELobbyFn2988E9();
        SmartPtr<EELobbyFileDialog> theDialog = EELobbyGetFileDialog();
        int theResult = -1;
        if (theDialog.mPtr != NULL) {
            theDialog->ListFiles(theDirectory, ".ees");
            theResult = LobbyDoDialog(theDialog.mPtr);
        }
        return theResult;
    }
    default: return INTPTR_MIN;
    }
    return 0;
}
}
#undef ee_won_ipaddr_clone
#endif
