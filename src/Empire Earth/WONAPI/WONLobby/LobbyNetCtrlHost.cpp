// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The Internet lobby's controls in Rust (R6 WONLobby round 13): ServerListCtrl and its rows (srvlistctl.rs), ChatCtrl
// (chatctl.rs) and the room tree, the friends and user lists, the quick play, login and account panels, the login
// screen's pages, the direct-connect tab and the room label (netctl.rs) are lobbyctl.rs kinds. This unit is their host's ops (LobbyCtrlHost passes it every op from 1000 up: each op the
// original's statements on a control, a row, a text, a graphics context or a lobby global, with no decision), the rows'
// virtuals (one forward each), the filter combo's row, the chat links, the room, friend and user rows and the config
// readers.
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
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyRichText.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONAuthContext.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_lobby/eeglue.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/chatlink.h"
#include "rust/won_lobby/srvlistctl.h"
#include "rust/won_lobby/chatctl.h"
#include "rust/won_lobby/rows.h"
#include "rust/won_net/ipobj.h"
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

// ---- the host

static GUIString Txt(intptr_t inData) { return inData != 0 ? GUIString((GUIStringData*)inData) : GUIString(); }
static intptr_t Dat(const GUIString& inText) { return (intptr_t)inText.mData.get(); }
static int Lo(intptr_t v) { return (int)(int32_t)(v & 0xffffffff); }
static int Hi(intptr_t v) { return (int)(v >> 32); }
static const GUIString& ResText(intptr_t inRes) { return gLobbyResources[inRes].mString; }
static Image* ResImage(intptr_t inRes) { return gLobbyResources[inRes].mImage.get(); }

// ---- ChatCtrl's links and texts (ChatCtrl.cpp's)

// A clickable link in the chat output (a client, friend, room or Accept/Reject). The original's RTTI names it in
// the global namespace (vtable 0x852df8, 0x8c bytes).
class ChatLink : public TextLink {
public:
    explicit ChatLink(const EeWonLobbyCtlLink& l); // the original's five constructors, by kind (chatlink.rs)
    virtual bool IsLink() { return this->mEnabled; } // whether the link can still be clicked
    int mKind; // 0x48: kind (0 room, 1 accept, 2 reject, 3 client, 4 friend, 5 friend's location)
    IPAddr mAddress; // 0x4c..0x74: server or friend's address
    unsigned short mGroupId; // 0x74: room (group) id
    SmartPtr<LobbyClient> mClient; // 0x78
    SmartPtr<LobbyFriend> mFriend; // 0x7c
    bool mEnabled; // 0x80: still clickable
    SmartPtr<ChatLink> mNext; // 0x84: next link in the chain (counted reference)
    ChatLink* mPrevious; // 0x88: previous link (not counted, never released)
};

// ChatLink's constructor's statements (chatlink.rs composes them, R6 WON round 33)
static intptr_t ChatLinkHost(void* inLink, EeWonChatLinkOp inOp, intptr_t a)
{
    typedef EeWonChatLinkOp Op;
    ChatLink* k = static_cast<ChatLink*>(inLink);
    switch (inOp) {
    case Op::SetClient: k->mClient = (LobbyClient*)a; return 0;
    case Op::TextClientName: k->mText = k->mClient->GetName(); return 0;
    case Op::SetText: k->mText = Txt(a); return 0;
    case Op::SetGroupId: k->mGroupId = (unsigned short)a; return 0;
    case Op::AddressOfServer: k->mAddress = ((LobbyServer*)a)->GetAddress(); return 0;
    case Op::SetFriend: k->mFriend = (LobbyFriend*)a; return 0;
    case Op::TextFriendName: k->mText = k->mFriend->GetName(); return 0;
    case Op::AddressClone: k->mAddress = IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a)); return 0;
    case Op::SetColor: k->mColor = (int)a; return 0;
    case Op::SetFont: k->mFont = (Font*)a; return 0;
    case Op::SetEnabled: k->mEnabled = true; return 0;
    case Op::ClearPrevious: k->mPrevious = NULL; return 0;
    }
    return 0;
}
ChatLink::ChatLink(const EeWonLobbyCtlLink& l) : mKind(l.kind) { ee_won_chatlink_init(this, &l, ChatLinkHost); }

// a held text: one reference the Rust gives back
static intptr_t Held(const GUIString& inText)
{
    GUIStringData* aData = inText.mData.get();
    aData->AddRef();
    return (intptr_t)aData;
}

// an object with one reference the Rust gives back
static intptr_t HeldRef(RefCount* inObject)
{
    inObject->AddRef();
    return (intptr_t)inObject;
}

// the lobby events' members are read in Rust (lobbyevt.rs ctl_op); the chat link of a WONGUI event stays here
static intptr_t EvField(ComponentEvent* e, EeWonLobbyCtlEvField f)
{
    if (f == EeWonLobbyCtlEvField::TlaLink)
        return (intptr_t)dynamic_cast<ChatLink*>(static_cast<TextLinkActivatedEvent*>(e)->mLink.get());
    return 0;
}

static intptr_t SmallOp(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c);

static intptr_t ChatOp(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    typedef EeWonLobbyCtlLinkField L;
    ChatLink* aLink = reinterpret_cast<ChatLink*>(a);
    TextArea* anOutput = reinterpret_cast<TextArea*>(a);
    InputBox* anInput = reinterpret_cast<InputBox*>(a);
    ChatCommandParser* aParser = reinterpret_cast<ChatCommandParser*>(a);
    switch (inOp) {
    case Op::EvGet: return EvField(reinterpret_cast<ComponentEvent*>(a), (EeWonLobbyCtlEvField)b);
    case Op::ChatLinkNew: return HeldRef(new ChatLink(*reinterpret_cast<const EeWonLobbyCtlLink*>(a)));
    case Op::LinkGet:
        switch ((L)b) {
        case L::Kind: return aLink->mKind;
        case L::Client: return (intptr_t)aLink->mClient.get();
        case L::Friend: return (intptr_t)aLink->mFriend.get();
        case L::Next: return (intptr_t)aLink->mNext.get();
        case L::Previous: return (intptr_t)aLink->mPrevious;
        default: return 0;
        }
    case Op::LinkSet:
        switch ((L)b) {
        case L::Color: aLink->mColor = (int)c; return 0;
        case L::Enabled: aLink->mEnabled = c != 0; return 0;
        case L::Next: aLink->mNext = reinterpret_cast<ChatLink*>(c); return 0;
        case L::Previous: aLink->mPrevious = reinterpret_cast<ChatLink*>(c); return 0;
        default: return 0;
        }
    case Op::ChatAddFormatted: {
        const EeWonLobbyCtlFormat* f = reinterpret_cast<const EeWonLobbyCtlFormat*>(a);
        GUIString aStrings[2] = {Txt(f->strings[0]), Txt(f->strings[1])};
        const GUIString* aText[2] = {&aStrings[0], &aStrings[1]};
        TextSegmentBase* aLinks[3] = {reinterpret_cast<ChatLink*>(f->links[0]), reinterpret_cast<ChatLink*>(f->links[1]),
                                      reinterpret_cast<ChatLink*>(f->links[2])};
        Font* aFonts[3] = {reinterpret_cast<Font*>(f->fonts[0]), reinterpret_cast<Font*>(f->fonts[1]),
                           reinterpret_cast<Font*>(f->fonts[2])};
        LobbyAppendFormattedText(reinterpret_cast<TextArea*>(f->output), Txt(f->format), f->num_strings ? aText : NULL,
                                 f->num_strings, f->colors, 9, aFonts, 3, f->num_links ? aLinks : NULL, f->num_links,
                                 f->line_break != 0);
        return 0;
    }
    case Op::ResText: return Dat(gLobbyResources[a].mString);
    case Op::TextAreaFont: return (intptr_t)anOutput->mFont.get();
    case Op::TextAreaLinkFont: return (intptr_t)anOutput->mLinkFont.get();
    case Op::TextAreaAddSegment: anOutput->AddSegment(Txt(b), (int)c, true); return 0;
    case Op::TextAreaLineBreak: anOutput->LineBreak((int)b); return 0;
    case Op::TextAreaReset: anOutput->Reset(b != 0); return 0;
    case Op::InputText: return Dat(anInput->GetText());
    case Op::CurrentServer: return (intptr_t)LobbyGetCurrentServer();
    case Op::MyClient: return (intptr_t)GetMyLobbyClient();
    case Op::ChatParser: return (intptr_t)LobbyGetChatCommandParser();
    case Op::RoomChatParser: return (intptr_t)LobbyGetRoomChatParser((int)a);
    case Op::ParserNumber: return b == 0 ? aParser->GetPrefix() : b == 1 ? aParser->GetError() : aParser->GetCommand();
    case Op::ParserText: return Held(b == 0 ? aParser->GetTargetName() : aParser->GetText());
    case Op::ParserHandleInput: return aParser->HandleInput(Txt(b)) ? 1 : 0;
    case Op::ParserHelp: return Held(aParser->GetHelp((unsigned short)b));
    case Op::ParserClients: return (intptr_t)aParser->GetClients();
    case Op::ParserCommandText: {
        // the prefix, the command, then the quoted name (the target's without one)
        GUIString aText(GetEmptyGUIStringData());
        aText.append(aParser->GetPrefix());
        aText.append(aParser->GetCommandString((int)b));
        aText.append(GUIString(" \""));
        aText.append(c != 0 ? Txt(c) : aParser->GetTargetName());
        aText.append(GUIString(new GUIStringData("\" ")));
        return Held(aText);
    }
    case Op::FormatTarget: return Held(LobbyFormatString(gLobbyResources[a].mString, reinterpret_cast<ChatCommandParser*>(b)->GetTargetName()));
    case Op::TextAppend: case Op::TextAppendBytes: {
        GUIString aText((GUIStringData*)a);
        ((GUIStringData*)a)->Release();
        if (inOp == Op::TextAppend) aText.append(Txt(b));
        else aText.append(GUIString(new GUIStringData(reinterpret_cast<const char*>(b), (unsigned short)c)));
        return Held(aText);
    }
    case Op::TextFind: return Txt(a).find(Txt(b), 0);
    case Op::IgnoredAt: return Held(LobbyGetIgnoredNameAt((size_t)a));
    case Op::DeliverFriendAction: inCtl->DeliverEvent(FriendActionEvent((int)a, reinterpret_cast<LobbyFriend*>(b), (int)c), false); return 0;
    case Op::DeliverJoinFriendsRoom: inCtl->DeliverEvent(JoinFriendsRoomEvent(reinterpret_cast<LobbyFriend*>(a)), false); return 0;
    case Op::DeliverJumpToRoom: inCtl->DeliverEvent(JumpToRoomEvent(aLink->mText, aLink->mGroupId, aLink->mAddress), false); return 0;
    case Op::DeliverAcceptClient: inCtl->DeliverEvent(AcceptClientEvent(reinterpret_cast<LobbyClient*>(a), b != 0), false); return 0;
    default: return SmallOp(inCtl, inOp, a, b, c);
    }
    return 0;
}

// ---- the small controls' ops

static intptr_t SmallOp(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    TreeArea* aTree = reinterpret_cast<TreeArea*>(a);
    TreeItem* aNode = reinterpret_cast<TreeItem*>(a);
    TextListItem* aText = reinterpret_cast<TextListItem*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    InputBox* anInput = reinterpret_cast<InputBox*>(a);
    Label* aLabel = reinterpret_cast<Label*>(a);
    switch (inOp) {
    case Op::DeliverQueryGroups:
        inCtl->DeliverEvent(QueryServerGroupsEvent(IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a))), false);
        return 0;
    case Op::ServerListGet: return (intptr_t)LobbyGetServerList();
    case Op::ChatGroup: return (intptr_t)LobbyGetChatGroup();
    case Op::ItemSetText: aText->mText = Txt(b); return 0;
    case Op::ItemSetImageRes: aText->mImage = b < 0 ? NULL : ResImage(b); return 0;
    case Op::ItemSetFont: aText->mFont = reinterpret_cast<Font*>(b); return 0;
    case Op::LabelSetUserWelcome:
        aLabel->SetText(LobbyFormatString(LOBBY_STRING(InternetQuickPlay_UserWelcome_String), LobbyUserText(reinterpret_cast<LobbyUserInfo*>(b), 0)), true);
        return 0;
    case Op::DeliverQuickPlay: inCtl->DeliverEvent(QuickPlayEvent((int)a), false); return 0;
    case Op::ComboAddUserName: return (intptr_t)aCombo->AddString(LobbyGetUserNameAt((size_t)b), -1);
    case Op::UserPassword: return Held(LobbyGetUserPassword(Txt(a)));
    case Op::AddUser: LobbyAddUser(Txt(a), Txt(b), true); return 0;
    case Op::ComboSetTextFlag: aCombo->SetText(Txt(b), c != 0); return 0;
    case Op::DeliverLogin: inCtl->DeliverEvent(LoginEvent(anInput->GetText(), reinterpret_cast<Button*>(b)->IsChecked(), reinterpret_cast<ComboBox*>(c)->mEdit.mPtr->GetText()), false); return 0;
    case Op::TextToInt: return Txt(a).mData->ToInt(0);
    case Op::DeliverCreateAccount: {
        const intptr_t* f = reinterpret_cast<const intptr_t*>(a);
        typedef LobbyEvtField EF;
        SmartPtr<LobbyEvent> event = CreateAccountEvent();
        LobbyEvtSetStr(event.get(), EF::UserName, (std::wstring)reinterpret_cast<InputBox*>(f[0])->GetText());
        LobbyEvtSetStr(event.get(), EF::Password, (std::wstring)reinterpret_cast<InputBox*>(f[1])->GetText());
        LobbyEvtSetStr(event.get(), EF::Email, (std::string)reinterpret_cast<InputBox*>(f[2])->GetText());
        LobbyEvtSet(event.get(), EF::Day, (unsigned char)reinterpret_cast<InputBox*>(f[3])->GetText().mData->ToInt(0));
        LobbyEvtSet(event.get(), EF::Month, (unsigned char)reinterpret_cast<InputBox*>(f[4])->GetText().mData->ToInt(0));
        LobbyEvtSet(event.get(), EF::Year, (unsigned short)reinterpret_cast<InputBox*>(f[5])->GetText().mData->ToInt(0));
        LobbyEvtSet(event.get(), EF::Newsletter, reinterpret_cast<Button*>(f[6])->IsChecked());
        inCtl->DeliverEvent(event.get(), false);
        return 0;
    }
    case Op::AuthContext: return (intptr_t)GetLobbyAuthContext();
    case Op::InputSetAuthName:
        anInput->SetText(GUIString(new GUIStringData(reinterpret_cast<AuthContext*>(b)->GetName())), true);
        return 0;
    case Op::AuthPassword: return Held(GUIString(new GUIStringData(reinterpret_cast<AuthContext*>(a)->GetPassword())));
    case Op::DeliverUpdateAccount: inCtl->DeliverEvent(UpdateAccountEvent(anInput->GetText(), reinterpret_cast<InputBox*>(b)->GetText(), reinterpret_cast<Button*>(c)->IsChecked()), false); return 0;
    case Op::DeliverLostPassword: inCtl->DeliverEvent(GetLostPasswordEvent(aCombo->mEdit->GetText()), false); return 0;
    case Op::DeliverLostUserName: inCtl->DeliverEvent(GetLostUserNameEvent(anInput->GetText()), false); return 0;
    case Op::BrowserGoLoginHelp:
        reinterpret_cast<BrowserComponent*>(a)->Go(ComponentConfig::GetResourceFile("help/loginhelp.html"), false, 0, -1);
        return 0;
    case Op::LabelSetData: aLabel->SetText(Txt(b), true); return 0;
    case Op::EnableLayout: reinterpret_cast<Container*>(a)->EnableLayoutInstructions((int)b, c != 0); return 0;
    case Op::Relayout: reinterpret_cast<Container*>(a)->Relayout(); return 0;
    case Op::HttpDocIsMotd: return reinterpret_cast<RustServerOp*>(a)->GetNum(EE_WON_HTTPDOC_TYPE) == HTTPDocType_MOTD ? 1 : 0;
    case Op::HttpDocPath: {
        std::string aPath = reinterpret_cast<RustServerOp*>(a)->GetBytes(EE_WON_HTTPDOC_PATH, b == 0 ? HTTPDocOwner_Sys : HTTPDocOwner_Game);
        memcpy(reinterpret_cast<char*>(c), aPath.data(), aPath.length() < 1024 ? aPath.length() : 1024);
        return (intptr_t)aPath.length();
    }
    case Op::BrowserShowFiles: reinterpret_cast<BrowserComponent*>(a)->ShowFiles(std::string((const char*)reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->sys, reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->sys_len), std::string((const char*)reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->game, reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->game_len), true); return 0;
    case Op::ScreenShow: reinterpret_cast<ScreenContainer*>(a)->ShowScreen(reinterpret_cast<Component*>(b), c != 0); return 0;
    case Op::ListSelGame: return (intptr_t)reinterpret_cast<ServerListCtrl*>(a)->GetSelGame();
    case Op::DeliverDirectConnect: inCtl->DeliverEvent(ChangeDirectConnectEvent((int)a, reinterpret_cast<LobbyGame*>(b)), false); return 0;
    case Op::LabelSetFormat3: aLabel->SetText(LobbyFormatString3(Txt(b), Txt(reinterpret_cast<const intptr_t*>(c)[0]), Txt(reinterpret_cast<const intptr_t*>(c)[1]), Txt(reinterpret_cast<const intptr_t*>(c)[2])), true); return 0;
    default: break;
    }
    return 0;
}

// ---- the room label's and the user list's config readers (RoomLabelCtrl.cpp's, UserListCtrl.cpp's)

// Reads the PREFIX, GROUPTYPE (CHAT or GAME), SHOWUSERCOUNT and SHOWSERVER layout keys
bool RoomLabelCtrlConfig::Virtual24(const std::string& inKey)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_ROOMLABELCTRLCONFIG_VIRTUAL24_INKEY,inKey.data(),inKey.size());
    if(aKeyIdx==0) this->mRoomLabel->SetField(0,Dat(this->ReadString(false)));
    else if(aKeyIdx==1)
    {
        GUIString aGroup=this->ReadString(false);
        if(aGroup.CompareNoCase("CHAT",0)==0) this->mRoomLabel->SetField(1,1);
        else if(aGroup.CompareNoCase("GAME",0)==0) this->mRoomLabel->SetField(1,2);
        else throw ConfigObjectException("Invalid Group Type: "+(std::string)aGroup);
    }
    else if(aKeyIdx==2) this->mRoomLabel->SetField(2,this->ReadBool());
    else if(aKeyIdx==3) this->mRoomLabel->SetField(3,this->ReadBool());
    else return ContainerConfig::Virtual24(inKey);
    return true;
}
// SetComponent has no address of its own: the linker folded it with the identical body at 0x79afb0
void RoomLabelCtrlConfig::SetComponent(Component* inComponent)
{
    this->mRoomLabel=static_cast<RoomLabelCtrl*>(inComponent);
    ContainerConfig::SetComponent(inComponent);
}
// Accepts (and ignores) the GROUPTYPE and CTRLLINKID layout keys; passes other keys to the container's reader
bool UserListCtrlConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_USERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0 || aKeyIdx==1) return true;
    return ContainerConfig::Virtual24(instruction);
}

// ChatCtrlConfig (WONLobbyControlConfig.h): GROUPTYPE (CHAT, GAME or TEAM) and the text colours, stored in the panel's
// Rust object
bool ChatCtrlConfig::Virtual24(const std::string& inKey)
{
    const int aKeyIdx = ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CHATCTRLCONFIG_VIRTUAL24_INKEY, inKey.data(), inKey.size());
    if (ContainerConfig::Virtual24(inKey)) return true;
    if (aKeyIdx == 0) {
        GUIString group = this->ReadString(false);
        const int aName = ee_won_gui_cfg_name(EE_WON_CFG_NAMES_CHATCTRLCONFIG_VIRTUAL24_GROUP, group.mData->mText, group.mData->mLength, sizeof(wchar_t));
        if (aName < 0 || aName > 2) throw ConfigObjectException("Invalid Group Type: " + (std::string)group);
        this->mChatCtrl->SetField(20, aName == 0 ? 1 : aName == 1 ? 2 : 0x12);
    } else if (aKeyIdx == 1) {
    } else if (aKeyIdx >= 2 && aKeyIdx <= 12) {
        // DEFAULT, ERROR, HELP, USER, EMOTE, WHISPER, WARNING, GROUP, MESSAGE, ACCEPT, REJECT
        static const int kColor[11] = {0, 5, 6, 1, 3, 2, 4, 7, 8, 9, 10};
        this->mChatCtrl->SetField(kColor[aKeyIdx - 2], this->ReadColor());
    } else return false;
    return true;
}
// Remembers the configured component as the chat panel (the original's body is at 0x7c8380; no marker)
void ChatCtrlConfig::SetComponent(Component* inComponent)
{
    ContainerConfig::SetComponent(inComponent);
    this->mChatCtrl = static_cast<ChatCtrl*>(inComponent);
}


static intptr_t ItemField(ListItem* inItem, intptr_t inField, bool inSet, intptr_t inValue)
{
    typedef EeWonLobbyCtlItemField F;
    switch ((F)inField) {
    case F::Height: if (inSet) inItem->mHeight = (int)inValue; return inItem->mHeight;
    case F::Top: if (inSet) inItem->mTop = (int)inValue; return inItem->mTop;
    case F::Width: if (inSet) inItem->mWidth = (int)inValue; return inItem->mWidth;
    case F::Selected: if (inSet) inItem->mSelected = inValue != 0; return inItem->mSelected ? 1 : 0;
    case F::Selectable: if (inSet) inItem->mSelectable = inValue != 0; return inItem->mSelectable ? 1 : 0;
    case F::Color:
        if (inSet) static_cast<TextListItem*>(inItem)->mColor = inValue;
        return (intptr_t)static_cast<TextListItem*>(inItem)->mColor;
    default: break; // the friend and user rows' members are rows.rs's
    }
    return 0;
}

intptr_t LobbyNetCtrlHost(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    Component* aComp = reinterpret_cast<Component*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    MultiListArea* aMulti = reinterpret_cast<MultiListArea*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    FilterListItem* aFilter = reinterpret_cast<FilterListItem*>(a);
    TextListItem* aText = static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a));
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    // the model's ops are answered by its Rust objects (modelobj.rs) before the host
    // the game list's rows are rows.rs's: their control (the list's grandparent) and their game's address text
    case Op::ListOwnerCtl: return (intptr_t) static_cast<LobbyRustCtrl*>(static_cast<ServerListCtrl*>(aList->mParent->mParent))->mLobbyCtl;
    case Op::GameAddrText: return Held(reinterpret_cast<LobbyGame*>(a)->GetAddress().GetHostAndPortString(true));
    // round 16: the lobby's rows (rows.rs; RustRow and RustTextRow over their Rust states)
    case Op::RowShellNew: return b != 0 ? (intptr_t) static_cast<ListItem*>(new RustTextRow(reinterpret_cast<EeWonRow*>(a))) : (intptr_t) static_cast<ListItem*>(new RustRow(reinterpret_cast<EeWonRow*>(a)));
    case Op::RowTextPaint: aText->TextListItem::Paint(*reinterpret_cast<Graphics*>(b), reinterpret_cast<ListArea*>(c)); return 0;
    case Op::RowTextCalc: aText->TextListItem::CalcDimensions(reinterpret_cast<ListArea*>(b)); return 0;
    case Op::RowTextCompare: return aText->TextListItem::Compare(reinterpret_cast<ListItem*>(b), reinterpret_cast<ListArea*>(c));
    case Op::ItemSetTextRes: aText->mText = ResText(b); return 0;
    case Op::ItemTextErase: aText->mText.erase(); return 0;
    case Op::ItemImage: return (intptr_t)aText->mImage.get();
    case Op::ItemFont: return (intptr_t)aText->mFont.get();
    case Op::ItemTextWidth: return reinterpret_cast<Font*>(b)->GetStringWidth(aText->mText, 0, aText->mText.length());
    case Op::ItemTextDraw: reinterpret_cast<Graphics*>(b)->DrawString(aText->mText, Lo(c), Hi(c)); return 0;
    case Op::ItemTextCompare: return aText->mText.CompareNoCase(reinterpret_cast<TextListItem*>(b)->mText);
    case Op::ImageWidth: return reinterpret_cast<Image*>(a)->GetWidth();
    case Op::ImageHeight: return reinterpret_cast<Image*>(a)->GetHeight();
    case Op::ResImage: return (intptr_t)ResImage(a);
    case Op::ItemGet: return ItemField(reinterpret_cast<ListItem*>(a), b, false, 0);
    case Op::ItemSet: ItemField(reinterpret_cast<ListItem*>(a), b, true, c); return 0;
    case Op::DefaultFont: return (intptr_t)aComp->GetDefaultFont();
    case Op::DefaultFontMod: return (intptr_t)aComp->GetDefaultFontMod((int)b, (int)c);
    case Op::FontMod: return (intptr_t)aComp->GetFontMod(reinterpret_cast<Font*>(b), (int)c, 0);
    case Op::ListFont: return (intptr_t)aList->mFont.get();
    case Op::FontHeight: return reinterpret_cast<Font*>(a)->GetHeight();
    case Op::ResImageHeight: return gLobbyResources[a].mImage->GetHeight();
    case Op::ListDeliverPing: aList->DeliverEvent(PingGameEvent(reinterpret_cast<LobbyGame*>(b), true), true); return 0;
    case Op::ListTextColor: return aList->GetTextColor();
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
    // the statements of the cases hostops.rs answers (R6 WONLobby round 29: the ping bars, the rows' counts, the
    // summaries, the filter row, the room events, the event items, the numbers, the saved game dialog)
    case Op::LobbyConfigGet: return (intptr_t)GetLobbyConfig();
    case Op::ConfigPingCeiling: return reinterpret_cast<LobbyConfig*>(a)->mPingCeiling;
    case Op::ConfigPublic: return reinterpret_cast<LobbyConfig*>(a)->mPublic ? 1 : 0;
    case Op::ItemTextAppend: aText->mText.append(Txt(b));
        return 0;
    case Op::FilterSetSelPos: aFilter->mSelPos = (int)b;
        return 0;
    case Op::FilterSetIsAdd: aFilter->mIsAdd = b != 0;
        return 0;
    case Op::FilterSetText: aFilter->mFilterText = Txt(b);
        return 0;
    case Op::FilterClearText: aFilter->mFilterText = GUIString("");
        return 0;
    case Op::LabelSetFormatRes: reinterpret_cast<Label*>(a)->SetText(LobbyFormatString(ResText(b), Txt(c)), true);
        return 0;
    case Op::LabelSetFormat2Res: reinterpret_cast<Label*>(a)->SetText(LobbyFormatString2(ResText(b), Txt(reinterpret_cast<const intptr_t*>(c)[0]), Txt(reinterpret_cast<const intptr_t*>(c)[1])), true);
        return 0;
    case Op::FilterCount: return (intptr_t)LobbyGetFilterCount(Txt(a));
    case Op::ComboAddFilterValue: aCombo->AddString(LobbyGetFilterValueAt(Txt(b), (size_t)c), -1);
        return 0;
    case Op::DeliverJoinRoomAt: inCtl->DeliverEvent(JoinRoomEvent(IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a)), (unsigned long)b), false);
        return 0;
    case Op::DeliverCreateRoomAt: inCtl->DeliverEvent(CreateRoomEvent(IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a))), false);
        return 0;
    case Op::DeliverCreateRoomAny: inCtl->DeliverEvent(CreateRoomEvent(IPAddr()), false);
        return 0;
    case Op::EvType: return anEvent->mType;
    case Op::EvComboItem: return (intptr_t) static_cast<ComboSelChangedEvent*>(anEvent)->mItem.get();
    case Op::EvTreeItem: return (intptr_t) static_cast<TreeItemOpenedEvent*>(anEvent)->mItem.get();
    case Op::EvListItem: return (intptr_t) static_cast<ListItemClickedEvent*>(anEvent)->mItem.get();
    case Op::LabelSetPercentText: reinterpret_cast<Label*>(a)->SetText(LobbyFormatString(EE_LOBBY_STRING(gEELobbyPlayerOptionsCtrl_TransferPercent), Txt(b)));
        return 0;
    case Op::FileDialogGet: return (intptr_t)EELobbyGetFileDialog();
    case Op::FileDialogListSaves: reinterpret_cast<EELobbyFileDialog*>(a)->ListFiles(EELobbyFn2988E9(), ".ees");
        return 0;
    case Op::DoDialog: return LobbyDoDialog(reinterpret_cast<EELobbyFileDialog*>(a));
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
    case Op::DeliverGameAction: inCtl->DeliverEvent(GameActionEvent(7, reinterpret_cast<LobbyGame*>(a)), false); return 0;
    case Op::DeliverJoinGame: inCtl->DeliverEvent(JoinGameEvent(reinterpret_cast<LobbyGame*>(a)), false); return 0;
    case Op::EventX: return static_cast<ListItemClickedEvent*>(anEvent)->mX;
    case Op::EventY: return static_cast<ListItemClickedEvent*>(anEvent)->mY;
    case Op::EventActiveSel: return static_cast<ComboSelChangedEvent*>(anEvent)->mActiveSel ? 1 : 0;
    case Op::ButtonSetRes: reinterpret_cast<Button*>(a)->SetText(ResText(b), true); return 0;
    case Op::LabelSetRes: reinterpret_cast<Label*>(a)->SetText(ResText(b), true); return 0;
    case Op::ComboInsertNoFilter:
        aCombo->InsertItem(new TextListItem(LOBBY_STRING(ServerListCtrl_NoFilter_String), reinterpret_cast<Font*>(b)), -1);
        return 0;
    case Op::ComboInsertFilterRow: aCombo->InsertItem(new FilterListItem(Txt(b), reinterpret_cast<Font*>(c)), -1); return 0;
    case Op::ComboItem: return (intptr_t)aCombo->mListArea->GetItem((int)b);
    case Op::ComboCount: return aCombo->mListArea->GetNumItems();
    case Op::ComboItemPos: return aCombo->mListArea->GetItemPos(reinterpret_cast<ListItem*>(b));
    case Op::TextItemIs: return reinterpret_cast<TextListItem*>(a)->mText == Txt(b) ? 1 : 0;
    case Op::ComboUpdateItem: aCombo->UpdateItem(reinterpret_cast<TextListItem*>(b)); return 0;
    case Op::ComboSetText: aCombo->SetText(Txt(b), true); return 0;
    case Op::ComboAddString: return (intptr_t)aCombo->AddString(Txt(b), -1);
    case Op::ComboSelectEnd: aCombo->mEdit->SelectRange(-1, 0); return 0;
    case Op::FilterIsAdd: return aFilter->mIsAdd ? 1 : 0;
    case Op::FilterSelPos: return aFilter->mSelPos;
    case Op::FilterText: return Dat(aFilter->mFilterText);
    case Op::FilterColumn: return Dat(aFilter->mColumnName);
    case Op::AddFilterValue: LobbyAddFilterValue(Txt(a), Txt(b)); return 0;
    case Op::RemoveFilterValue: LobbyRemoveFilterValue(Txt(a), Txt(b)); return 0;
    case Op::EeColumn:
        if ((EeWonEeGlueCall)a == EeWonEeGlueCall::EpochKeep || (EeWonEeGlueCall)a == EeWonEeGlueCall::MapTypeKeep) {
            GUIString aText = Txt(c);
            return ee_won_eeglue_call((EeWonEeGlueCall)a, b, (intptr_t)&aText, 0);
        }
        return ee_won_eeglue_call((EeWonEeGlueCall)a, b, c, 0);
    default: return ChatOp(inCtl, inOp, a, b, c);
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
// Creates the layout config with no header and no columns
ServerListCtrlConfig::ServerListCtrlConfig()
{
    this->mHeader = NULL;
    this->mNumColumns = 0;
}
}
