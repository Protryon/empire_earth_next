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
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyCrossPromotion.h"
#include "ee/wonapi/WONLobbyDialogs.h"
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
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/lobbycfgrd.h"
#include "rust/won_gui/compcfg.h"
#include "rust/won_net/http.h"
using namespace WONAPI;

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

// the locals of the compound cases hostops.rs composes (R6 WON round 35): a text's bytes, the text being built,
// ChatAddFormatted's arrays, DeliverCreateAccount's event and EeColumn's text
static std::string sEELobbyBytes;
static GUIString sEELobbyText;
static GUIString sEELobbyFmtStrings[2];
static const GUIString* const sEELobbyFmtText[2] = {&sEELobbyFmtStrings[0], &sEELobbyFmtStrings[1]};
static TextSegmentBase* sEELobbyFmtLinks[3];
static Font* sEELobbyFmtFonts[3];
static SmartPtr<LobbyEvent> sEELobbyEvent;
static GUIString sEELobbyColumnText;
static const EeWonLobbyCtlFormat* Fmt(intptr_t inFormat) { return reinterpret_cast<const EeWonLobbyCtlFormat*>(inFormat); }

static intptr_t SmallOp(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c);

static intptr_t ChatOp(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    ChatLink* aLink = reinterpret_cast<ChatLink*>(a);
    TextArea* anOutput = reinterpret_cast<TextArea*>(a);
    InputBox* anInput = reinterpret_cast<InputBox*>(a);
    ChatCommandParser* aParser = reinterpret_cast<ChatCommandParser*>(a);
    switch (inOp) {
    // EvGet's chat link of a WONGUI event (lobbyevt.rs reads the lobby events' members; hostops.rs picks the field)
    case Op::EvTlaLink: return (intptr_t)dynamic_cast<ChatLink*>(static_cast<TextLinkActivatedEvent*>(reinterpret_cast<ComponentEvent*>(a))->mLink.get());
    case Op::ChatLinkNew: return HeldRef(new ChatLink(*reinterpret_cast<const EeWonLobbyCtlLink*>(a)));
    case Op::LinkGetKind: return aLink->mKind;
    case Op::LinkGetClient: return (intptr_t)aLink->mClient.get();
    case Op::LinkGetFriend: return (intptr_t)aLink->mFriend.get();
    case Op::LinkGetNext: return (intptr_t)aLink->mNext.get();
    case Op::LinkGetPrevious: return (intptr_t)aLink->mPrevious;
    case Op::LinkSetColor: aLink->mColor = (int)b; return 0;
    case Op::LinkSetEnabled: aLink->mEnabled = b != 0; return 0;
    case Op::LinkSetNext: aLink->mNext = reinterpret_cast<ChatLink*>(b); return 0;
    case Op::LinkSetPrevious: aLink->mPrevious = reinterpret_cast<ChatLink*>(b); return 0;
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
    case Op::FormatTarget: return Held(LobbyFormatString(gLobbyResources[a].mString, reinterpret_cast<ChatCommandParser*>(b)->GetTargetName()));
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
    case Op::BrowserShowFiles: reinterpret_cast<BrowserComponent*>(a)->ShowFiles(std::string((const char*)reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->sys, reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->sys_len), std::string((const char*)reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->game, reinterpret_cast<const EeWonLobbyCtlFiles*>(b)->game_len), true); return 0;
    case Op::ScreenShow: reinterpret_cast<ScreenContainer*>(a)->ShowScreen(reinterpret_cast<Component*>(b), c != 0); return 0;
    case Op::ListSelGame: return (intptr_t)reinterpret_cast<ServerListCtrl*>(a)->GetSelGame();
    case Op::DeliverDirectConnect: inCtl->DeliverEvent(ChangeDirectConnectEvent((int)a, reinterpret_cast<LobbyGame*>(b)), false); return 0;
    case Op::LabelSetFormat3: aLabel->SetText(LobbyFormatString3(Txt(b), Txt(reinterpret_cast<const intptr_t*>(c)[0]), Txt(reinterpret_cast<const intptr_t*>(c)[1]), Txt(reinterpret_cast<const intptr_t*>(c)[2])), true); return 0;
    default: break;
    }
    return 0;
}

// ---- the config readers: the lobby controls' config kinds (R6 WON lobby/net round 40: ChatCtrlConfig ..
// CreditsDialogConfig, once nine classes in four units); their bodies are lobbycfgrd.rs's (R6 WON round 33) over
// these statements. The game list's header and column count are the config's two slots (compcfg.rs).

struct LobbyListCfg { MultiListHeader* mHeader; int mNumColumns; }; // ServerListCtrlConfig's members
static LobbyListCfg* LobbyListCfgOf(ComponentConfig* c) { return reinterpret_cast<LobbyListCfg*>(ee_won_cc_slots(c->mCfg)); }
template<class T> static T* LobbyCfgComp(ComponentConfig* inConfig) { return static_cast<T*>(inConfig->mComponent.get()); }
bool CfgKindV24(ComponentConfig* c, EeWonCfgKind k, const std::string& key); // the base kind's walk (WONCfgTable.inl)
bool CfgOwnNamed(ComponentConfig* c, const std::string& k, int& v);           // the base's named values (WONCompCfg.inl)
static intptr_t CfgAddColumn(ServerListCtrl* inList, const std::string& inName) { return inList->Cfg(EeWonLobbyCtlCfg::AddColumn, (intptr_t)inName.data(), (intptr_t)inName.size()); }
static intptr_t CreditsAddLine(LobbyRustDialog* inCredits, const GUIString& inLine, Component* inComponent, intptr_t inColor) { return inCredits->LCall(EeWonLobbyDlgCall::AddLine, (intptr_t)&inLine, (intptr_t)inComponent, (intptr_t)(uint32_t)inColor); }
static std::string sCfgRdPath; // HOMELOCATION's path
static intptr_t CfgRdHost(void* inCfg, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc)
{
    typedef EeWonLobbyCfgRdOp Op;
    ComponentConfig* c = static_cast<ComponentConfig*>(inCfg);
    LobbyListCfg* sl = LobbyListCfgOf(c);
    ServerListCtrl* aList = LobbyCfgComp<ServerListCtrl>(c);
    switch (inOp) {
    case Op::ReadString: return WON_GUARDED(Held(c->ReadString(false)));
    case Op::ReadColor: return WON_GUARDED(c->ReadColor());
    case Op::ReadInt: return WON_GUARDED(c->ReadInt());
    case Op::EnsureComma: return WON_GUARDED_V(c->EnsureComma());
    case Op::EndOfString: return WON_GUARDED(c->EndOfString());
    case Op::FindNamed: return WON_GUARDED(c->Fn386860(std::string((const char*)a)));
    case Op::FindNamedText: return WON_GUARDED(c->Fn386860((std::string)Txt(a)));
    case Op::TextRelease: ((GUIStringData*)a)->Release(); return 0;
    case Op::TextNameIndex: return (intptr_t)(ee_won_gui_cfg_name((int)b, ((GUIStringData*)a)->mText, ((GUIStringData*)a)->mLength, sizeof(wchar_t)));
    case Op::TextIsNoCase: return (Txt(a).CompareNoCase((const char*)b, 0) == 0) ? 1 : 0;
    case Op::Throw: return WON_GUARDED_V(throw ConfigObjectException(std::string((const char*)a)));
    case Op::ThrowWithText: return WON_GUARDED_V(throw ConfigObjectException(std::string((const char*)a) + (std::string)Txt(b)));
    case Op::Base: return WON_GUARDED(CfgKindV24(c, (EeWonCfgKind)b, *(const std::string*)a));
    case Op::ChatSetField: LobbyCfgComp<ChatCtrl>(c)->SetField((int)a, b); return 0;
    case Op::RoomSetPrefixRead: return WON_GUARDED_V(LobbyCfgComp<RoomLabelCtrl>(c)->SetField(0, Dat(c->ReadString(false))));
    case Op::RoomSetField: LobbyCfgComp<RoomLabelCtrl>(c)->SetField((int)a, b); return 0;
    case Op::RoomSetFieldBoolRead: return WON_GUARDED_V(LobbyCfgComp<RoomLabelCtrl>(c)->SetField((int)a, c->ReadBool()));
    case Op::ServerNamedBase: return WON_GUARDED(CfgOwnNamed(c, *(const std::string*)a, *(int*)b));
    case Op::ServerCfg: return (intptr_t)(aList->Cfg((EeWonLobbyCtlCfg)a, b));
    case Op::ServerPushColumn: return (intptr_t)(aList->Cfg(EeWonLobbyCtlCfg::PushColumn, a, b));
    case Op::ServerAddColumnText: return (intptr_t)(CfgAddColumn(aList, (std::string)Txt(a)));
    case Op::ServerAddGameOption: LobbyAddGameOption(Txt(a), Txt(b), false); return 0;
    case Op::AsMultiList: return (intptr_t)(dynamic_cast<MultiListArea*>((Component*)a));
    case Op::ServerHeaderFrom: return (intptr_t)(sl->mHeader = dynamic_cast<MultiListHeader*>((Component*)a));
    case Op::ServerHeaderIsNull: return (sl->mHeader == NULL) ? 1 : 0;
    case Op::ReadCombo: return WON_GUARDED(SafeConfigGetComponent<ComboBox*>(c->ReadComponent(), "ComboBox"));
    case Op::ServerColumnsInc: ((MultiListArea*)a)->SetNumColumns(++sl->mNumColumns); return 0;
    case Op::ServerNumColumns: return (intptr_t)(sl->mNumColumns);
    case Op::ListDynamicColumn: ((MultiListArea*)a)->SetDynamicSizeColumn((int)b); return 0;
    case Op::ListColumnWidth: ((MultiListArea*)a)->SetColumnWidth((int)b, (int)cc, false); return 0;
    case Op::ServerHeaderSetColumnItem: sl->mHeader->SetColumnItem(sl->mNumColumns - 1, (Component*)a); return 0;
    case Op::BrowserPathRead: return WON_GUARDED_V(sCfgRdPath = (std::string)c->ReadString(false));
    case Op::BrowserPathIsFile: return WON_GUARDED(ee_won_url_kind(reinterpret_cast<const uint8_t*>(sCfgRdPath.data()), sCfgRdPath.size()) == EE_WON_URL_FILE); // URL(path).GetType()
    case Op::BrowserPathResolve: return WON_GUARDED_V(sCfgRdPath = ComponentConfig::GetResourceFile(sCfgRdPath));
    case Op::BrowserSetHome: return WON_GUARDED_V(LobbyCfgComp<GameBrowserCtrl>(c)->SetHomeLocation(sCfgRdPath));
    case Op::CrossDefaultImageRead: return WON_GUARDED_V(LobbyCfgComp<CrossPromotionButton>(c)->SetDefaultImage(c->ReadImage(false)));
    case Op::CrossDefaultBrowseRead: return WON_GUARDED_V(LobbyCfgComp<CrossPromotionButton>(c)->SetDefaultBrowse(c->ReadString(false)));
    case Op::ImageTextFontRead: return WON_GUARDED_V(LobbyCfgComp<ImageTextButton>(c)->mFont = c->ReadFont());
    case Op::ImageTextTextRead: return WON_GUARDED_V(LobbyCfgComp<ImageTextButton>(c)->mText = c->ReadString(false));
    case Op::ImageTextBrowseRead: return WON_GUARDED_V(LobbyCfgComp<ImageTextButton>(c)->mBrowse = c->ReadString(false));
    case Op::CreditsAddLine: return WON_GUARDED(CreditsAddLine(LobbyCfgComp<LobbyRustDialog>(c), GUIString((GUIStringData*)a), c->Fn386860(std::string("DialogComponent")), b));
    }
    return 0;
}
// a lobby control kind's Virtual24 (WONCfgTable.inl) and the game list kind's GetNamedValue (WONCompCfg.inl)
bool LobbyCfgV24(ComponentConfig* c, const std::string& k) { return WonRethrow(ee_won_lobbycfgrd_call(c, CfgRdHost, EeWonLobbyCfgRdCall::OfKind, (intptr_t)k.data(), (intptr_t)k.size(), (intptr_t)&k, (intptr_t)c->mKind)) != 0; }
bool LobbyCfgNamed(ComponentConfig* c, const std::string& k, int& v) { return WonRethrow(ee_won_lobbycfgrd_call(c, CfgRdHost, EeWonLobbyCfgRdCall::ServerListNamed, (intptr_t)k.data(), (intptr_t)k.size(), (intptr_t)&k, (intptr_t)&v)) != 0; }


intptr_t LobbyNetCtrlHost(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    Component* aComp = reinterpret_cast<Component*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    Graphics* g = reinterpret_cast<Graphics*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
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
    case Op::FileDialogListSaves: reinterpret_cast<LobbyRustDialog*>(a)->LCall(EeWonLobbyDlgCall::FileListFiles, (intptr_t)std::string(EELobbyFn2988E9()).c_str(), (intptr_t)".ees");
        return 0;
    case Op::DoDialog: return LobbyDoDialog(reinterpret_cast<LobbyRustDialog*>(a));
    case Op::GfxDrawLine: g->DrawLine(Lo(b), Hi(b), Lo(c), Hi(c)); return 0;
    case Op::GfxDrawImage: g->DrawImage(reinterpret_cast<Image*>(b), Lo(c), Hi(c)); return 0;
    case Op::TextCompareNoCase: return Txt(a).CompareNoCase(Txt(b));
    case Op::TextFindNoCase: return Txt(a).findNoCase(Txt(b), 0);
    case Op::TextFindRes: return Txt(a).findNoCase(ResText(b), 0);
    case Op::TextLen: return Txt(a).length();
    case Op::TextAt: return Txt(a)[(int)b];
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
    case Op::ComboItem: return (intptr_t)aCombo->mListArea->GetItem((int)b);
    case Op::ComboCount: return aCombo->mListArea->GetNumItems();
    case Op::ComboItemPos: return aCombo->mListArea->GetItemPos(reinterpret_cast<ListItem*>(b));
    case Op::TextItemIs: return reinterpret_cast<TextListItem*>(a)->mText == Txt(b) ? 1 : 0;
    case Op::ComboUpdateItem: aCombo->UpdateItem(reinterpret_cast<TextListItem*>(b)); return 0;
    case Op::ComboSetText: aCombo->SetText(Txt(b), true); return 0;
    case Op::ComboAddString: return (intptr_t)aCombo->AddString(Txt(b), -1);
    case Op::ComboSelectEnd: aCombo->mEdit->SelectRange(-1, 0); return 0;
    case Op::AddFilterValue: LobbyAddFilterValue(Txt(a), Txt(b)); return 0;
    case Op::RemoveFilterValue: LobbyRemoveFilterValue(Txt(a), Txt(b)); return 0;
    // the statements of the compound cases hostops.rs composes in the game (R6 WON round 35)
    case Op::ItemGetHeight: return reinterpret_cast<ListItem*>(a)->mHeight;
    case Op::ItemGetTop: return reinterpret_cast<ListItem*>(a)->mTop;
    case Op::ItemGetWidth: return reinterpret_cast<ListItem*>(a)->mWidth;
    case Op::ItemGetSelected: return reinterpret_cast<ListItem*>(a)->mSelected ? 1 : 0;
    case Op::ItemGetSelectable: return reinterpret_cast<ListItem*>(a)->mSelectable ? 1 : 0;
    case Op::ItemGetColor: return (intptr_t)aText->mColor;
    case Op::ItemSetHeight: reinterpret_cast<ListItem*>(a)->mHeight = (int)b; return 0;
    case Op::ItemSetTop: reinterpret_cast<ListItem*>(a)->mTop = (int)b; return 0;
    case Op::ItemSetWidth: reinterpret_cast<ListItem*>(a)->mWidth = (int)b; return 0;
    case Op::ItemSetSelected: reinterpret_cast<ListItem*>(a)->mSelected = b != 0; return 0;
    case Op::ItemSetSelectable: reinterpret_cast<ListItem*>(a)->mSelectable = b != 0; return 0;
    case Op::ItemSetColor: aText->mColor = b; return 0;
    case Op::TextAlloc: return (intptr_t) new GUIStringData(reinterpret_cast<const char*>(a), (unsigned short)b);
    case Op::BytesOfText: sEELobbyBytes = (std::string)Txt(a); return 0;
    case Op::BytesOfHttpDocSys: sEELobbyBytes = reinterpret_cast<RustServerOp*>(a)->GetBytes(EE_WON_HTTPDOC_PATH, HTTPDocOwner_Sys); return 0;
    case Op::BytesOfHttpDocGame: sEELobbyBytes = reinterpret_cast<RustServerOp*>(a)->GetBytes(EE_WON_HTTPDOC_PATH, HTTPDocOwner_Game); return 0;
    case Op::BytesLen: return (intptr_t)sEELobbyBytes.length();
    case Op::BytesCopy: memcpy(reinterpret_cast<char*>(a), sEELobbyBytes.data(), (size_t)b); return 0;
    case Op::BytesDrop: std::string().swap(sEELobbyBytes); return 0;
    case Op::TextTake: sEELobbyText = GUIString((GUIStringData*)a); return 0;
    case Op::TextStartEmpty: sEELobbyText = GUIString(GetEmptyGUIStringData()); return 0;
    case Op::TextAppendText: sEELobbyText.append(Txt(a)); return 0;
    case Op::TextAppendRaw: sEELobbyText.append(GUIString(new GUIStringData(reinterpret_cast<const char*>(a), (unsigned short)b))); return 0;
    case Op::TextAppendParserPrefix: sEELobbyText.append(reinterpret_cast<ChatCommandParser*>(a)->GetPrefix()); return 0;
    case Op::TextAppendParserCommand: sEELobbyText.append(reinterpret_cast<ChatCommandParser*>(a)->GetCommandString((int)b)); return 0;
    case Op::TextAppendQuoteOpen: sEELobbyText.append(GUIString(" \"")); return 0;
    case Op::TextAppendParserTarget: sEELobbyText.append(reinterpret_cast<ChatCommandParser*>(a)->GetTargetName()); return 0;
    case Op::TextAppendQuoteClose: sEELobbyText.append(GUIString(new GUIStringData("\" "))); return 0;
    case Op::TextHeldOut: return Held(sEELobbyText);
    case Op::TextDrop: sEELobbyText = GUIString(); return 0;
    case Op::FmtSetString: sEELobbyFmtStrings[a] = Txt(b); return 0;
    case Op::FmtSetLink: sEELobbyFmtLinks[a] = reinterpret_cast<ChatLink*>(b); return 0;
    case Op::FmtSetFont: sEELobbyFmtFonts[a] = reinterpret_cast<Font*>(b); return 0;
    case Op::FmtTexts: return (intptr_t)sEELobbyFmtText;
    case Op::FmtLinks: return (intptr_t)sEELobbyFmtLinks;
    case Op::FmtAppend: LobbyAppendFormattedText(reinterpret_cast<TextArea*>(Fmt(a)->output), Txt(Fmt(a)->format), reinterpret_cast<const GUIString* const*>(b), Fmt(a)->num_strings, Fmt(a)->colors, 9, sEELobbyFmtFonts, 3, reinterpret_cast<TextSegmentBase* const*>(c), Fmt(a)->num_links, Fmt(a)->line_break != 0); return 0;
    case Op::EvCreateAccountNew: sEELobbyEvent = CreateAccountEvent(); return 0;
    case Op::EvSetWide: LobbyEvtSetStr(sEELobbyEvent.get(), (LobbyEvtField)a, (std::wstring)reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::EvSetNarrow: LobbyEvtSetStr(sEELobbyEvent.get(), (LobbyEvtField)a, (std::string)reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::EvSetNum: LobbyEvtSet(sEELobbyEvent.get(), (LobbyEvtField)a, b); return 0;
    case Op::EvDeliverHeld: inCtl->DeliverEvent(sEELobbyEvent.get(), false); return 0;
    case Op::EvDrop: sEELobbyEvent = NULL; return 0;
    case Op::ColumnTextSet: sEELobbyColumnText = Txt(a); return 0;
    case Op::ColumnText: return (intptr_t)&sEELobbyColumnText;
    default: return ChatOp(inCtl, inOp, a, b, c);
    }
    return 0;
}
}
