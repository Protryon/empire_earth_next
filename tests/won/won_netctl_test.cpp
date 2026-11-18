// The Internet lobby's other controls as lobbyctl.rs kinds (R6 WONLobby round 13): the original RoomCtrl.cpp,
// FriendsListCtrl.cpp, UserListCtrl.cpp, QuickPlayCtrl.cpp, LoginCtrl.cpp, CreateAccountCtrl.cpp,
// UpdateAccountCtrl.cpp, LostPasswordCtrl.cpp, LostUserNameCtrl.cpp, LoginHelpCtrl.cpp, LoginStatusCtrl.cpp,
// InitFailedCtrl.cpp, MOTDCtrl.cpp, LoginScreen.cpp, DirectConnectCtrl.cpp and RoomLabelCtrl.cpp (the controls'
// statements copied from the C++ before the port; their list rows stay C++) with every call on a control, a row, an
// event, a text, an address or a lobby global replaced by the host op that makes it (LobbyNetCtrlHost.cpp and the
// model's and addresses' Rust objects run the same calls), against ee_won_lobby's netctl.rs over one scripted host.
//
// Each sequence makes a control of a random kind, maybe stores its config (the room label's), then makes 1 to 12
// random calls (InitComponent; every event the control handles, on its own members, on no component and on another
// one; a page's paint; the login screen's pages; the members read back) and frees it. The host answers each op from
// a seeded generator (the same stream for both sides); a notifying clear, selection or text re-enters the control
// with the matching event a third of the time (two deep at most). The traces must be the same.
//
// The first 80 sequences and every later one that makes an op or event none before made are
// rust/crates/ee_won_lobby/src/golden_netctl.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/netctl.h"
#include "rust/won_lobby/resource.h"

typedef EeWonLobbyCtlOp Op;
typedef EeWonLobbyCtlCall Call;
typedef EeWonLobbyCtlEvent Ev;
typedef EeWonLobbyCtlEvField EF;
typedef EeWonLobbyCtlItemField F;
typedef EeWonLobbyCtlKind Kind;
typedef EeWonModelField MF;
typedef EeWonModelCall MC;
typedef EeWonLobbyRes Res;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;
static intptr_t sNextHeld;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0)
{
    char aBuffer[200];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e);
    sTrace.push_back(aBuffer);
}

static const intptr_t kCtl = 0x1000, kEvent = 0xE1, kOther = 0x77, kMe = 0x9090, kClient = 0x3200,
                      kFriend = 0x3300, kServer = 0x3400, kGroup = 0x3500, kItem = 0x5000, kNode = 0x5400,
                      kText = 0x7100, kAddr = 0x7800, kClients = 0x3700, kServers = 0x3800, kGame = 0x3000;
static const char* const kEmails[] = {"", "a@b.c", "@b.c", "ab@c", "ab@c.", "x@y.zz", "a.b@c", "ab.c@d", "a@.b",
                                      "q@w.e.r", "plain"};

static bool sRust;
static EeWonLobbyCtl* sRustCtl;
static Kind sKind;
static int sDepth;
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c);

static void Reenter(Ev inEvent, intptr_t inComponent)
{
    if (sDepth >= 2 || R(3) != 0) return;
    sDepth++;
    Line("reenter %lld %lld", (long long)inEvent, inComponent);
    if (sRust) ee_won_lobbyctl_call(sRustCtl, Call::Event, kEvent, (intptr_t)inEvent, inComponent);
    else OrigCall(Call::Event, kEvent, (intptr_t)inEvent, inComponent);
    Line("back");
    sDepth--;
}

static intptr_t EvAnswer(EF f)
{
    switch (f) {
    case EF::JrRoomSpec: case EF::LrRoomSpec: return R(3) ? (intptr_t)(1u << R(3)) : (intptr_t)R(8);
    case EF::JrServerName: case EF::JrGroupName: case EF::LisStatus: case EF::LisDescription: case EF::SlfUserName:
    case EF::SlfPassword: case EF::QarEmail:
        return kText + R(4);
    case EF::JrClients: return kClients;
    case EF::GcGroup: return R(6) ? kGroup + R(3) * 0x10 : 0;
    case EF::GcServer: case EF::ScServer: return R(6) ? kServer + R(3) * 0x10 : 0;
    case EF::FcFriend: return kFriend + R(3) * 0x10;
    case EF::LisKind: return R(5);
    case EF::GhdOp: return 0x6600;
    default: return R(2);
    }
}

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::Find: r = R(14) ? kCtl + b * 0x10 : 0; break;
    case Op::EvGet: r = EvAnswer((EF)b); break;
    case Op::MGet:
        switch ((MF)b) {
        case MF::ServerAddress: r = kAddr + R(3); break;
        case MF::GroupId: r = R(4); break;
        case MF::FriendFlags: r = R(8); break;
        case MF::FriendName: case MF::ClientName: r = kText + 8 + R(3); break;
        case MF::FriendUserId: r = R(3); break;
        case MF::ClientId: r = R(3); break;
        default: r = R(2); break;
        }
        break;
    case Op::MCall:
        switch ((MC)b) {
        case MC::ServerGroupCount: case MC::ServerListCount: case MC::ListCount: r = R(4); break;
        case MC::ServerGroupAt: r = kGroup + c * 0x10; break;
        case MC::ServerListAt: r = kServer + c * 0x10; break;
        case MC::ListAt: r = kClient + c * 0x10; break;
        case MC::GroupKind: r = R(16); break;
        default: r = R(2); break;
        }
        break;
    case Op::ItemGet: r = R(2); break;
    case Op::ListGetSel: r = R(3) ? (sKind == Kind::Room ? kNode + R(3) : kItem + R(3)) : 0; break;
    case Op::ListNumItems: r = R(5); break;
    case Op::ListItem: r = R(6) ? (sKind == Kind::Room ? kNode + b : kItem + b) : 0; break;
    case Op::ListItemPos: r = (intptr_t)R(5) - 1; break;
    case Op::ListVertOffset: r = R(100); break;
    case Op::TreeInsertChild: case Op::TreeInsertServer: case Op::TreeInsertLoading: r = kNode + 0x100 + R(8); break;
    case Op::TreeWrapped: r = kItem + (a & 0xff); break;
    case Op::TreeChildCount: r = R(4); break;
    case Op::TreeChild: r = kNode + 0x200 + b; break;
    case Op::TreeParent: r = R(3) ? kNode + R(3) : kNode + 0x100 + R(8); break;
    case Op::TreeIndex: r = R(6); break;
    case Op::RoomItemServer: case Op::RoomItemGroup: case Op::FriendRowNew: case Op::UserRowNew:
    case Op::TextNew: case Op::TextAppend: case Op::UserPassword: case Op::AuthPassword: case Op::AddrClone:
        r = sNextHeld++; break;
    case Op::RoomItemAddr: r = kAddr + R(3); break;
    case Op::RoomItemGroupId: r = R(4) ? R(4) : 0xffff; break;
    case Op::ItemFriend: r = R(4) ? kFriend + R(3) * 0x10 : 0; break;
    case Op::ItemUserClient: r = R(4) ? kClient + R(3) * 0x10 : 0; break;
    case Op::ServerListGet: r = R(5) ? kServers : 0; break;
    case Op::ChatGroup: r = R(2) ? kGroup : 0; break;
    case Op::CurrentServer: r = R(4) ? kServer : 0; break;
    case Op::CurrentUser: r = R(2) ? 0x6800 : 0; break;
    case Op::AuthContext: r = R(4) ? 0x6900 : 0; break;
    case Op::FriendCount: case Op::UserCount: r = R(4); break;
    case Op::FriendAt: r = kFriend + a * 0x10; break;
    case Op::ComboAddUserName: r = kItem + 0x40 + b; break;
    case Op::UserLastUseAt: r = (intptr_t)R(5) - 1; break;
    case Op::ComboItemPos: r = R(4); break;
    case Op::ComboEditText: case Op::InputText: r = kText + 20 + R(4); break;
    case Op::TextLen: r = R(4); break;
    case Op::TextAt: r = R(3) ? '@' : 'a'; break;
    case Op::TextToInt: { static const int kInts[] = {0, 1, 12, 13, 31, 32, 1889, 1890, 1999, 2000, -1}; r = kInts[R(11)]; break; }
    case Op::TextBytes: {
        const char* s = kEmails[R(sizeof(kEmails) / sizeof(kEmails[0]))];
        size_t n = strlen(s);
        memcpy((char*)b, s, n < (size_t)c ? n : (size_t)c);
        r = (intptr_t)n;
        sTrace.push_back("op " + std::to_string((long long)inOp) + " " + std::to_string((long long)a) + " '" + s +
                         "' = " + std::to_string((long long)r));
        return r;
    }
    case Op::HttpDocPath: {
        static const char* const kPaths[] = {"", "sys.txt", "game.txt", "motd/a b.txt"};
        const char* s = kPaths[R(4)];
        size_t n = strlen(s);
        memcpy((char*)c, s, n);
        r = (intptr_t)n;
        sTrace.push_back("op " + std::to_string((long long)inOp) + " " + std::to_string((long long)a) + " " +
                         std::to_string((long long)b) + " '" + s + "' = " + std::to_string((long long)r));
        return r;
    }
    case Op::ResText: r = 0x9000 + a; break;
    case Op::ControlId: { static const int kIds[] = {0, 200004, 200007, 7}; r = kIds[R(4)]; break; }
    case Op::EventItem: r = R(3) ? (sKind == Kind::Room ? kNode + R(3) : kItem + R(3)) : 0; break;
    case Op::EventButton: r = R(3); break;
    case Op::EventX: case Op::EventY: r = R(50); break;
    case Op::EventRoomSpec: r = R(3) ? (intptr_t)(1u << R(3)) : (intptr_t)R(8); break;
    case Op::ClientChangedClient: r = kClient + R(3) * 0x10; break;
    case Op::EventFlags: r = R(8); break;
    case Op::ListSelGame: r = R(2) ? kGame : 0; break;
    case Op::DefaultFont: r = 0x4400; break;
    case Op::DefaultFontMod: r = 0x4500 + b; break;
    case Op::ButtonIsChecked: r = R(3) == 0; break;
    case Op::AddrEquals: r = R(3) == 0; break;
    case Op::IsDisabled: case Op::ConfigIsPublic: case Op::HttpDocIsMotd: case Op::TreeIsOpen:
    case Op::TreeContainsPoint: case Op::TextEquals: case Op::AddrValid: case Op::EventIsAdd: case Op::EventIsDelete:
    case Op::EventDoubleClick: case Op::EventIsSelectedItem:
        r = R(5) != 0; break;
    default: break;
    }
    // the structs by their contents, the made texts by their bytes
    if (inOp == Op::TextNew)
        sTrace.push_back("op " + std::to_string((int)inOp) + " '" + std::string((const char*)a, (size_t)b) + "' = " +
                         std::to_string((long long)r));
    else if (inOp == Op::DeliverCreateAccount) {
        const intptr_t* f = (const intptr_t*)a;
        Line("op %lld create %lld %lld %lld %lld", (long long)inOp, f[0], f[1], f[2], f[3]);
        sTrace.back() += " " + std::to_string((long long)f[4]) + " " + std::to_string((long long)f[5]) + " " +
                         std::to_string((long long)f[6]) + " = 0";
    } else if (inOp == Op::BrowserShowFiles) {
        const EeWonLobbyCtlFiles* f = (const EeWonLobbyCtlFiles*)b;
        sTrace.push_back("op " + std::to_string((int)inOp) + " " + std::to_string((long long)a) + " '" +
                         std::string((const char*)f->sys, f->sys_len) + "' '" + std::string((const char*)f->game, f->game_len) +
                         "' = 0");
    } else if (inOp == Op::LabelSetFormat3) {
        const intptr_t* t = (const intptr_t*)c;
        Line("op %lld %lld %lld f3 %lld %lld", (long long)inOp, a, b, t[0], t[1]);
        sTrace.back() += " " + std::to_string((long long)t[2]) + " = 0";
    } else Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    if ((inOp == Op::ListClear || inOp == Op::TreeClear) && b != 0) Reenter(Ev::ListSelChanged, kOther);
    if (inOp == Op::ListSetSel && c != 0) Reenter(Ev::ListSelChanged, a);
    if (inOp == Op::ComboClear && b != 0) Reenter(Ev::ComboSelChanged, a);
    if (inOp == Op::InputSetText && c != 0) Reenter(Ev::InputTextChanged, a);
    if (inOp == Op::InputClear && b != 0) Reenter(Ev::InputTextChanged, a);
    if (inOp == Op::ComboSetTextFlag && c != 0) Reenter(Ev::InputTextChanged, a);
    return r;
}
static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }
static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }

// ---- the original classes, every call on another object replaced by its op

struct Orig {
    intptr_t m[16]; // the controls, in each class's member order
    bool mFlag;     // mCanLogin, mHaveLoadedHelp, mNeedGenHTML, mUpdating, mShowServer
    int mNumber;    // mModeFlags, mUserCt
    int mRoomSpecFlags;
    intptr_t mTexts[3]; // mActualOldPassword; mPrefix, mServer, mRoom
    std::string mPaths[2];
};
static Orig sO;

static void Assign(intptr_t& ioMember, intptr_t inNew)
{
    if (ioMember == inNew) return;
    if (ioMember != 0) O(Op::Release, ioMember);
    ioMember = inNew;
    if (inNew != 0) O(Op::AddRef, inNew);
}
static void ReleasePtr(intptr_t& ioPtr) { if (ioPtr != 0) O(Op::Release, ioPtr); ioPtr = 0; }
static intptr_t E(intptr_t inEvent, EF f) { return O(Op::EvGet, inEvent, (intptr_t)f); }
static intptr_t Get(intptr_t inObj, MF f) { return O(Op::MGet, inObj, (intptr_t)f); }
static intptr_t Call2(intptr_t inObj, MC c, intptr_t a = 0) { return O(Op::MCall, inObj, (intptr_t)c, a); }
static intptr_t IG(intptr_t inItem, F f) { return O(Op::ItemGet, inItem, (intptr_t)f); }
static void IS(intptr_t inItem, F f, intptr_t v) { O(Op::ItemSet, inItem, (intptr_t)f, v); }
static intptr_t& M(int i) { return sO.m[i]; }
static void SetEnabled(intptr_t c, bool on) { O(Op::SetEnabled, c, on); }
static intptr_t Pt(int x, int y) { return (intptr_t)(uint32_t)x | ((intptr_t)(uint32_t)y << 32); }
static int SlotOf(Kind k)
{
    switch (k) {
    case Kind::Room: return 111; case Kind::Friends: return 114; case Kind::Users: return 117;
    case Kind::QuickPlay: return 120; case Kind::Login: return 126; case Kind::CreateAccount: return 130;
    case Kind::UpdateAccount: return 144; case Kind::LostPassword: return 157; case Kind::LostUserName: return 159;
    case Kind::LoginHelp: return 161; case Kind::LoginStatus: return 162; case Kind::InitFailed: return 163;
    case Kind::Motd: return 166; case Kind::LoginScreen: return 167; case Kind::DirectConnect: return 173;
    case Kind::RoomLabel: return 176; default: return 0;
    }
}
// the slots' members (each original SafeGetComponent's)
static const int kMember[66] = {0, 1, 2, 0, 1, 2, 0, 3, 4, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3,
                                0, 8, 1, 9, 2, 10, 3, 11, 4, 5, 6, 12, 7, 13,
                                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                0, 1, 0, 1, 0, 0, 1, 2, 0, 0, 1, 2, 3, 4, 5, 0, 0, 1, 2, 0};
static void SafeGet(intptr_t inConfig, int inSlot) { Assign(M(kMember[inSlot - 111]), O(Op::Find, inConfig, inSlot)); }

// RoomCtrl
enum { RM_LIST, RM_JOIN, RM_CREATE };
static void RoomEnableControls() { SetEnabled(M(RM_JOIN), O(Op::ListGetSel, M(RM_LIST)) != 0); }
static void RoomAddGroup(intptr_t inParent, intptr_t inServer, intptr_t inGroup)
{
    intptr_t aItem = O(Op::RoomItemGroup, inGroup, inServer);
    O(Op::TreeInsertChild, M(RM_LIST), inParent, aItem);
}
static void RoomAddServer(intptr_t inServer, bool inOpen, int inPos)
{
    O(Op::ListBeginUpdate, M(RM_LIST));
    intptr_t aItem = O(Op::RoomItemServer, inServer, 0);
    intptr_t aServer = O(Op::TreeInsertServer, M(RM_LIST), aItem, inPos);
    size_t it = 0;
    while ((intptr_t)it < Call2(inServer, MC::ServerGroupCount)) {
        intptr_t aGroup = Call2(inServer, MC::ServerGroupAt, it);
        if (Call2(aGroup, MC::GroupKind) & 8) RoomAddGroup(aServer, inServer, aGroup);
        ++it;
    }
    if (!Call2(inServer, MC::ServerConnected) && Call2(inServer, MC::ServerGroupCount) == 0) {
        O(Op::TreeRemoveAll, M(RM_LIST), aServer);
        intptr_t aLoading = O(Op::RoomItemServer, inServer, 1);
        intptr_t aLoadingTree = O(Op::TreeInsertLoading, M(RM_LIST), aServer, aLoading);
        IS(aLoadingTree, F::Selectable, 0);
    }
    if (inOpen) O(Op::TreeOpen, M(RM_LIST), aServer, 1);
    O(Op::ListEndUpdate, M(RM_LIST));
    RoomEnableControls();
}
static intptr_t RoomServerItemAt(intptr_t inAddress)
{
    for (int i = 0; i < (int)O(Op::ListNumItems, M(RM_LIST)); ++i) {
        intptr_t aTree = O(Op::ListItem, M(RM_LIST), i);
        intptr_t aItem = O(Op::TreeWrapped, aTree);
        if (O(Op::AddrEquals, O(Op::RoomItemAddr, aItem), inAddress)) return aTree;
    }
    return 0;
}
static intptr_t RoomGroupItemAt(intptr_t inAddress, int inGroupId)
{
    intptr_t aServer = RoomServerItemAt(inAddress);
    if (aServer == 0) return 0;
    for (int i = 0; i < (int)O(Op::TreeChildCount, aServer); ++i) {
        intptr_t aChild = O(Op::TreeChild, aServer, i);
        if ((unsigned long)O(Op::RoomItemGroupId, O(Op::TreeWrapped, aChild)) == (unsigned long)(unsigned)inGroupId) return aChild;
    }
    return 0;
}
static intptr_t RoomServerItem(intptr_t inServer)
{
    if (inServer != 0) return RoomServerItemAt(Get(inServer, MF::ServerAddress));
    return 0;
}
static intptr_t RoomGroupItem(intptr_t inServer, intptr_t inGroup)
{
    if (inServer != 0 && inGroup != 0) {
        int anId = (unsigned short)Get(inGroup, MF::GroupId);
        return RoomGroupItemAt(Get(inServer, MF::ServerAddress), anId);
    }
    return 0;
}
static void RoomJoin()
{
    intptr_t aSelected = O(Op::ListGetSel, M(RM_LIST));
    if (aSelected != 0) O(Op::DeliverJoinRoom, O(Op::TreeWrapped, aSelected));
}
static void RoomCreate()
{
    intptr_t aAddress = 0;
    intptr_t aSelected = O(Op::ListGetSel, M(RM_LIST));
    if (aSelected != 0) aAddress = O(Op::AddrClone, O(Op::RoomItemAddr, O(Op::TreeWrapped, aSelected)));
    O(Op::DeliverCreateRoom, aAddress);
    if (aAddress != 0) O(Op::AddrFree, aAddress);
}
static void RoomSetList()
{
    intptr_t aServers = O(Op::ServerListGet);
    if (aServers == 0) return;
    O(Op::ListBeginUpdate, M(RM_LIST));
    O(Op::TreeClear, M(RM_LIST), 1);
    for (size_t it = 0; (intptr_t)it < Call2(aServers, MC::ServerListCount); ++it)
        RoomAddServer(Call2(aServers, MC::ServerListAt, it), false, -1);
    O(Op::ListEndUpdate, M(RM_LIST));
    intptr_t aGroup = O(Op::ChatGroup);
    intptr_t aServer = O(Op::CurrentServer);
    if (aServer != 0) {
        intptr_t aItem = RoomServerItem(aServer);
        if (aItem != 0) {
            O(Op::TreeOpen, M(RM_LIST), aItem, 0);
            if (aGroup == 0 || (aItem = RoomGroupItem(aServer, aGroup)) != 0) {
                intptr_t aList = M(RM_LIST);
                O(Op::ListSetSel, aList, O(Op::ListItemPos, aList, aItem), 1);
                O(Op::ListEnsureVisible, M(RM_LIST), aItem);
            }
        }
    }
    RoomEnableControls();
}
static void RoomGroupChanged(intptr_t inEvent)
{
    intptr_t aGroup = E(inEvent, EF::GcGroup);
    intptr_t aServer = E(inEvent, EF::GcServer);
    if (aGroup == 0 || aServer == 0 || !(Call2(aGroup, MC::GroupKind) & 8)) return;
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t aServerItem = RoomServerItem(aServer);
        if (aServerItem != 0) RoomAddGroup(aServerItem, aServer, aGroup);
        RoomEnableControls();
        return;
    }
    if (O(Op::EventIsDelete, inEvent)) {
        intptr_t aGroupItem = RoomGroupItem(aServer, aGroup);
        O(Op::TreeRemove, M(RM_LIST), aGroupItem);
        RoomEnableControls();
        return;
    }
    intptr_t aGroupItem = RoomGroupItem(aServer, aGroup);
    if (aGroupItem != 0) {
        O(Op::RoomItemUpdate, O(Op::TreeWrapped, aGroupItem), aServer, aGroup);
        O(Op::RoomItemUpdate, O(Op::TreeWrapped, O(Op::TreeParent, aGroupItem)), aServer, 0);
        O(Op::ListRefresh, M(RM_LIST), O(Op::TreeParent, aGroupItem));
        O(Op::ListRefresh, M(RM_LIST), aGroupItem);
    }
    RoomEnableControls();
}
static void RoomItemOpened(intptr_t inEvent)
{
    intptr_t aTree = O(Op::EventItem, inEvent);
    if (O(Op::TreeIsOpen, aTree)) {
        intptr_t aItem = O(Op::TreeWrapped, aTree);
        O(Op::DeliverQueryGroups, O(Op::RoomItemAddr, aItem));
    }
}
static void RoomServerChanged(intptr_t inEvent)
{
    if (O(Op::EventIsAdd, inEvent)) { RoomAddServer(E(inEvent, EF::ScServer), false, -1); return; }
    if (O(Op::EventIsDelete, inEvent)) {
        intptr_t aServerItem = RoomServerItem(E(inEvent, EF::ScServer));
        if (aServerItem != 0) O(Op::TreeRemove, M(RM_LIST), aServerItem);
        RoomEnableControls();
        return;
    }
    intptr_t aServerItem = RoomServerItem(E(inEvent, EF::ScServer));
    if (aServerItem == 0) return;
    bool aOpen = O(Op::TreeIsOpen, aServerItem) != 0;
    intptr_t aSelected = O(Op::ListGetSel, M(RM_LIST));
    unsigned long aGroupId = 0xffff;
    intptr_t aAddress = 0;
    if (aSelected != 0 && O(Op::TreeParent, aSelected) == aServerItem) {
        intptr_t aItem = O(Op::TreeWrapped, aSelected);
        aGroupId = (unsigned long)O(Op::RoomItemGroupId, aItem);
        aAddress = O(Op::AddrClone, O(Op::RoomItemAddr, aItem));
    }
    int aPosition = (int)O(Op::TreeIndex, aServerItem);
    intptr_t aVertOffset = O(Op::ListVertOffset, M(RM_LIST));
    O(Op::ListBeginUpdate, M(RM_LIST));
    O(Op::TreeRemove, M(RM_LIST), aServerItem);
    RoomAddServer(E(inEvent, EF::ScServer), aOpen, aPosition);
    if (aGroupId != 0xffff && O(Op::AddrValid, aAddress)) {
        intptr_t aItem = RoomGroupItemAt(aAddress, (int)aGroupId);
        if (aItem != 0) {
            intptr_t aList = M(RM_LIST);
            O(Op::ListSetSel, aList, O(Op::ListItemPos, aList, aItem), 1);
        }
    }
    O(Op::ListEndUpdate, M(RM_LIST));
    O(Op::ListSetVertOffset, M(RM_LIST), aVertOffset);
    RoomEnableControls();
    if (aAddress != 0) O(Op::AddrFree, aAddress);
}
static void RoomEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::TreeItemOpened: RoomItemOpened(inEvent); return;
    case Ev::ListItemClicked: {
        RoomEnableControls();
        if (!O(Op::EventDoubleClick, inEvent) || inComponent != M(RM_LIST) || !O(Op::EventIsSelectedItem, inEvent)) return;
        intptr_t aItem = O(Op::EventItem, inEvent);
        int y = (int)O(Op::EventY, inEvent);
        int x = (int)O(Op::EventX, inEvent);
        if (O(Op::TreeContainsPoint, aItem, Pt(x, y))) RoomJoin();
        return;
    }
    case Ev::ButtonPressed:
        if (inComponent == M(RM_JOIN)) { RoomJoin(); return; }
        if (inComponent == M(RM_CREATE)) { RoomCreate(); return; }
        break;
    case Ev::GroupChanged: RoomGroupChanged(inEvent); return;
    case Ev::ServerChanged: RoomServerChanged(inEvent); return;
    case Ev::SetRoomList: RoomSetList(); return;
    default: break;
    }
    O(Op::BaseEvent, inEvent);
}

// FriendsListCtrl
enum { N_LIST, N_REMOVE, N_LOCATE, N_SEPARATOR };
static void FriendsUpdateButtons()
{
    if (O(Op::ListGetSel, M(N_LIST)) == 0) { SetEnabled(M(N_LOCATE), false); SetEnabled(M(N_REMOVE), false); }
    else { SetEnabled(M(N_LOCATE), true); SetEnabled(M(N_REMOVE), true); }
}
static void FriendsUpdateSeparator()
{
    O(Op::ListRefresh, M(N_LIST), M(N_SEPARATOR));
    int pos = (int)O(Op::ListItemPos, M(N_LIST), M(N_SEPARATOR));
    bool show = pos > 0 && pos < (int)O(Op::ListNumItems, M(N_LIST)) - 1;
    if ((IG(M(N_SEPARATOR), F::FriendSeparator) != 0) != show) {
        IS(M(N_SEPARATOR), F::FriendSeparator, show);
        O(Op::ListRefresh, M(N_LIST), M(N_SEPARATOR));
    }
}
static void FriendsUpdate(intptr_t inFriend, intptr_t inItem)
{
    O(Op::ItemSetFriend, inItem, inFriend);
    if (((unsigned char)Get(inFriend, MF::FriendFlags) & 3) != 0 && !Get(inFriend, MF::FriendAnonymous)) IS(inItem, F::FriendSortGroup, 2);
    else IS(inItem, F::FriendSortGroup, 0);
    Res anImage;
    if (((unsigned char)Get(inFriend, MF::FriendFlags) & 4) != 0) anImage = Res::LobbyGlobal_Searching_Image;
    else if (!Get(inFriend, MF::FriendAnonymous)) {
        if (((unsigned char)Get(inFriend, MF::FriendFlags) & 2) != 0) anImage = Res::FriendsListCtrl_InGame_Image;
        else if (((unsigned char)Get(inFriend, MF::FriendFlags) & 1) != 0) anImage = Res::FriendsListCtrl_InChat_Image;
        else anImage = Res::FriendsListCtrl_Unknown_Image;
    } else anImage = Res::FriendsListCtrl_Unknown_Image;
    O(Op::ItemSetImageRes, inItem, (intptr_t)anImage);
    O(Op::ItemSetText, inItem, Get(inFriend, MF::FriendName));
    if (((unsigned char)Get(inFriend, MF::FriendFlags) & 3) != 0 && !Get(inFriend, MF::FriendAnonymous)) {
        IS(inItem, F::Color, 0x8000000e);
        O(Op::ItemSetFont, inItem, O(Op::DefaultFont, kMe));
    } else {
        IS(inItem, F::Color, 0x80000006);
        O(Op::ItemSetFont, inItem, O(Op::DefaultFontMod, kMe, 2, 0));
    }
}
static intptr_t FriendsFind(intptr_t inFriend)
{
    for (int i = 0; i < (int)O(Op::ListNumItems, M(N_LIST)); i++) {
        intptr_t item = O(Op::ListItem, M(N_LIST), i);
        if (O(Op::ItemFriend, item) != 0 && (unsigned long)Get(O(Op::ItemFriend, item), MF::FriendUserId) == (unsigned long)Get(inFriend, MF::FriendUserId))
            return item;
    }
    return 0;
}
static intptr_t FriendsSelected()
{
    intptr_t item = O(Op::ListGetSel, M(N_LIST));
    if (item != 0) return O(Op::ItemFriend, item);
    return 0;
}
static void FriendsChanged(intptr_t inEvent)
{
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t item = O(Op::FriendRowNew);
        O(Op::AddRef, item);
        FriendsUpdate(E(inEvent, EF::FcFriend), item);
        O(Op::ListInsert2, M(N_LIST), item);
        O(Op::Release, item);
    } else if (O(Op::EventIsDelete, inEvent)) {
        intptr_t item = FriendsFind(E(inEvent, EF::FcFriend));
        if (item == 0) return;
        O(Op::ListRemove, M(N_LIST), item);
    } else {
        intptr_t item = FriendsFind(E(inEvent, EF::FcFriend));
        if (item == 0) return;
        FriendsUpdate(E(inEvent, EF::FcFriend), item);
        O(Op::ListRefresh, M(N_LIST), item);
    }
    FriendsUpdateButtons();
    FriendsUpdateSeparator();
}
static void FriendsRefresh()
{
    O(Op::ListBeginUpdate, M(N_LIST));
    for (int i = 0; i < (int)O(Op::ListNumItems, M(N_LIST)); i++) {
        intptr_t item = O(Op::ListItem, M(N_LIST), i);
        if (item != 0 && O(Op::ItemFriend, item) != 0) FriendsUpdate(O(Op::ItemFriend, item), item);
    }
    O(Op::ListEndUpdate, M(N_LIST));
    FriendsUpdateButtons();
    FriendsUpdateSeparator();
}
static void FriendsFill()
{
    O(Op::ListBeginUpdate, M(N_LIST));
    O(Op::ListClear, M(N_LIST), 1);
    O(Op::ListInsert2, M(N_LIST), M(N_SEPARATOR));
    for (size_t i = 0; (intptr_t)i < O(Op::FriendCount); ++i) {
        intptr_t item = O(Op::FriendRowNew);
        O(Op::AddRef, item);
        FriendsUpdate(O(Op::FriendAt, i), item);
        O(Op::ListInsert2, M(N_LIST), item);
        O(Op::Release, item);
    }
    O(Op::ListEndUpdate, M(N_LIST));
    FriendsUpdateButtons();
    FriendsUpdateSeparator();
}
static void FriendsEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::ButtonPressed:
        if (inComponent == M(N_REMOVE)) {
            intptr_t aFriend = FriendsSelected();
            if (aFriend != 0) O(Op::DeliverFriendAction, 3, aFriend, 0);
        } else if (inComponent == M(N_LOCATE)) {
            intptr_t aFriend = FriendsSelected();
            if (aFriend != 0) O(Op::DeliverFriendAction, 1, aFriend, 0);
        } else break;
        return;
    case Ev::ListSelChanged: FriendsUpdateButtons(); return;
    case Ev::ListItemClicked: {
        intptr_t item = O(Op::ListGetSel, M(N_LIST));
        if (item != 0 && item == O(Op::EventItem, inEvent) && O(Op::ItemFriend, item) != 0) {
            if ((int)O(Op::EventButton, inEvent) == 2) {
                O(Op::DeliverFriendAction, 5, O(Op::ItemFriend, item), 0);
                return;
            }
            if ((int)O(Op::EventButton, inEvent) == 0 && O(Op::EventDoubleClick, inEvent))
                O(Op::DeliverFriendAction, 1, O(Op::ItemFriend, item), 0);
        }
        return;
    }
    case Ev::SyncOptions: FriendsFill(); return;
    case Ev::FriendChanged: FriendsChanged(inEvent); return;
    case Ev::RefreshFriends: FriendsRefresh(); return;
    default: break;
    }
    O(Op::BaseEvent, inEvent);
}

// UserListCtrl
enum { U_LIST, U_FIRST, U_SECOND, U_WHISPER, U_IGNORE };
static Res ClientIcon(intptr_t inClient, bool inMode)
{
    if (Get(inClient, MF::ClientIgnored)) return Res::UserListCtrl_Ignored_Image;
    if (Get(inClient, MF::ClientBlocked)) return Res::UserListCtrl_Blocked_Image;
    if (Call2(inClient, MC::ClientMuted, inMode)) return Res::UserListCtrl_Muted_Image;
    if (Call2(inClient, MC::ClientAway)) return Res::UserListCtrl_Away_Image;
    if (Call2(inClient, MC::ClientAdmin)) return Res::UserListCtrl_Admin_Image;
    if (Call2(inClient, MC::ClientModerator)) return Res::UserListCtrl_Moderator_Image;
    return Call2(inClient, MC::ClientIsCaptain, inMode) ? Res::UserListCtrl_Captain_Image : Res::UserListCtrl_Normal_Image;
}
static void UsersUpdateItem(intptr_t inClient, intptr_t inItem)
{
    O(Op::ItemSetUserClient, inItem, inClient);
    int sortKey;
    if (Call2(inClient, MC::ClientAdmin)) sortKey = 5;
    else if (Call2(inClient, MC::ClientModerator)) sortKey = 4;
    else sortKey = Get(inClient, MF::ClientIsFriend) && !Call2(inClient, MC::ClientAnonymous) ? 2 : 0;
    IS(inItem, F::UserSortKey, sortKey);
    O(Op::ItemSetText, inItem, Get(inClient, MF::ClientName));
    O(Op::ItemSetImageRes, inItem, inClient == 0 ? -1 : (intptr_t)ClientIcon(inClient, (sO.mNumber & 2) != 0));
    if (Get(inClient, MF::ClientIsFriend) && !Call2(inClient, MC::ClientAnonymous)) O(Op::ItemSetFont, inItem, O(Op::DefaultFontMod, kMe, 1, 0));
    else O(Op::ItemSetFont, inItem, O(Op::DefaultFont, kMe));
}
static void UsersRefreshSeparators()
{
    O(Op::ListRefresh, M(U_LIST), M(U_FIRST));
    O(Op::ListRefresh, M(U_LIST), M(U_SECOND));
    int first = (int)O(Op::ListItemPos, M(U_LIST), M(U_FIRST));
    int second = (int)O(Op::ListItemPos, M(U_LIST), M(U_SECOND));
    bool before = first > 0;
    bool middle = second > first + 1;
    bool after = second < (int)O(Op::ListNumItems, M(U_LIST)) - 1;
    bool showFirst = before && (middle || after);
    bool showSecond = middle && after;
    if ((IG(M(U_FIRST), F::UserShowSeparator) != 0) != showFirst) {
        IS(M(U_FIRST), F::UserShowSeparator, showFirst);
        O(Op::ListRefresh, M(U_LIST), M(U_FIRST));
    }
    if ((IG(M(U_SECOND), F::UserShowSeparator) != 0) != showSecond) {
        IS(M(U_SECOND), F::UserShowSeparator, showSecond);
        O(Op::ListRefresh, M(U_LIST), M(U_SECOND));
    }
}
static intptr_t UsersFind(intptr_t inClient)
{
    for (int i = 0; i < (int)O(Op::ListNumItems, M(U_LIST)); ++i) {
        intptr_t item = O(Op::ListItem, M(U_LIST), i);
        if (O(Op::ItemUserClient, item) != 0 &&
            (unsigned short)Get(O(Op::ItemUserClient, item), MF::ClientId) == (unsigned short)Get(inClient, MF::ClientId))
            return item;
    }
    return 0;
}
static intptr_t UsersSelectedClient()
{
    intptr_t item = O(Op::ListGetSel, M(U_LIST));
    return item == 0 ? 0 : O(Op::ItemUserClient, item);
}
static void UsersUpdateButtons()
{
    intptr_t client = UsersSelectedClient();
    if (client == 0) {
        O(Op::ButtonSetRes, M(U_IGNORE), (intptr_t)Res::UserListCtrl_Ignore_String);
        SetEnabled(M(U_IGNORE), false);
        SetEnabled(M(U_WHISPER), false);
    } else {
        SetEnabled(M(U_WHISPER), true);
        SetEnabled(M(U_IGNORE), true);
        if (Get(client, MF::ClientIgnored)) O(Op::ButtonSetRes, M(U_IGNORE), (intptr_t)Res::UserListCtrl_Unignore_String);
        else O(Op::ButtonSetRes, M(U_IGNORE), (intptr_t)Res::UserListCtrl_Ignore_String);
    }
}
static void UsersJoined(intptr_t inEvent)
{
    if ((sO.mNumber & (int)E(inEvent, EF::JrRoomSpec)) != (int)E(inEvent, EF::JrRoomSpec)) return;
    sO.mFlag = true;
    O(Op::ListBeginUpdate, M(U_LIST));
    O(Op::ListClear, M(U_LIST), 1);
    O(Op::ListInsert2, M(U_LIST), M(U_FIRST));
    O(Op::ListInsert2, M(U_LIST), M(U_SECOND));
    intptr_t clients = E(inEvent, EF::JrClients);
    for (size_t i = 0; (intptr_t)i < Call2(clients, MC::ListCount); ++i) {
        intptr_t client = Call2(clients, MC::ListAt, i);
        intptr_t item = O(Op::UserRowNew);
        O(Op::AddRef, item);
        UsersUpdateItem(client, item);
        O(Op::ListInsert2, M(U_LIST), item);
        O(Op::Release, item);
    }
    O(Op::ListEndUpdate, M(U_LIST));
    UsersUpdateButtons();
    UsersRefreshSeparators();
}
static void UsersLeft(intptr_t inEvent)
{
    if ((sO.mNumber & (int)E(inEvent, EF::LrRoomSpec)) != (int)E(inEvent, EF::LrRoomSpec)) return;
    sO.mFlag = false;
    O(Op::ListClear, M(U_LIST), 1);
    UsersUpdateButtons();
}
static void UsersClientChanged(intptr_t inEvent)
{
    if ((sO.mNumber & (int)O(Op::EventRoomSpec, inEvent)) != (int)O(Op::EventRoomSpec, inEvent)) return;
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t item = O(Op::UserRowNew);
        O(Op::AddRef, item);
        intptr_t client = O(Op::ClientChangedClient, inEvent);
        UsersUpdateItem(client, item);
        O(Op::ListInsert2, M(U_LIST), item);
        O(Op::Release, item);
    } else if (O(Op::EventIsDelete, inEvent)) {
        intptr_t item = UsersFind(O(Op::ClientChangedClient, inEvent));
        if (item == 0) return;
        O(Op::ListRemove, M(U_LIST), item);
    } else {
        intptr_t item = UsersFind(O(Op::ClientChangedClient, inEvent));
        if (item == 0) return;
        UsersUpdateItem(O(Op::ClientChangedClient, inEvent), item);
        O(Op::ListRefresh, M(U_LIST), item);
    }
    UsersUpdateButtons();
    UsersRefreshSeparators();
}
static void UsersEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::ClientChanged: UsersClientChanged(inEvent); return;
    case Ev::JoinedRoom: UsersJoined(inEvent); return;
    case Ev::LeftRoom: UsersLeft(inEvent); return;
    case Ev::ButtonPressed: {
        if (inComponent == M(U_IGNORE)) {
            intptr_t client = UsersSelectedClient();
            if (client != 0) O(Op::DeliverClientAction, Get(client, MF::ClientIgnored) ? 12 : 5, sO.mNumber, client);
        } else if (inComponent == M(U_WHISPER)) {
            intptr_t client = UsersSelectedClient();
            if (client != 0) O(Op::BroadcastWhisper, client, sO.mNumber);
        }
        return;
    }
    case Ev::ListSelChanged: UsersUpdateButtons(); return;
    case Ev::ListItemClicked: {
        intptr_t client = UsersSelectedClient();
        if (client == 0) return;
        intptr_t item = O(Op::EventItem, inEvent);
        if (O(Op::ListGetSel, M(U_LIST)) != item) return;
        if ((int)O(Op::EventButton, inEvent) == 2) O(Op::DeliverClientAction, 18, sO.mNumber, client);
        else if ((int)O(Op::EventButton, inEvent) == 0 && O(Op::EventDoubleClick, inEvent)) O(Op::BroadcastWhisper, client, sO.mNumber);
        return;
    }
    default: O(Op::BaseEvent, inEvent); return;
    }
}

// QuickPlayCtrl
static void QuickPlayEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::SyncOptions: {
        intptr_t user = O(Op::CurrentUser);
        if (user != 0) O(Op::LabelSetUserWelcome, M(0), user);
        else O(Op::LabelSetRes, M(0), (intptr_t)Res::InternetQuickPlay_Welcome_String);
        return;
    }
    case Ev::ButtonPressed:
        if (inComponent == M(5)) {
            int gameType = 0;
            if (O(Op::ButtonIsChecked, M(1))) gameType = 1;
            else if (O(Op::ButtonIsChecked, M(2))) gameType = 2;
            else if (O(Op::ButtonIsChecked, M(3))) gameType = 3;
            else if (O(Op::ButtonIsChecked, M(4))) gameType = 4;
            O(Op::DeliverQuickPlay, gameType);
        }
        break;
    default: break;
    }
    O(Op::DialogBaseEvent, inEvent);
}

// LoginCtrl
enum { LG_USER, LG_PASSWORD, LG_REMEMBER, LG_LOGIN };
static void LoginUpdateButton()
{
    SetEnabled(M(LG_LOGIN), sO.mFlag && O(Op::TextLen, O(Op::ComboEditText, M(LG_USER))) != 0 &&
                                O(Op::TextLen, O(Op::InputText, M(LG_PASSWORD))) != 0);
}
static void LoginLoadPassword()
{
    if (O(Op::ConfigIsPublic)) {
        O(Op::InputClear, M(LG_PASSWORD), 1);
        O(Op::SetVisible, M(LG_REMEMBER), 0);
        O(Op::ButtonSetCheck, M(LG_REMEMBER), 0, 0);
        return;
    }
    intptr_t password = O(Op::UserPassword, O(Op::ComboEditText, M(LG_USER)));
    O(Op::InputSetText, M(LG_PASSWORD), password, 1);
    O(Op::InputSelectRange, M(LG_PASSWORD), 0, -1);
    O(Op::ButtonSetCheck, M(LG_REMEMBER), O(Op::TextLen, password) != 0, 0);
    O(Op::SetVisible, M(LG_REMEMBER), 1);
    O(Op::Release, password);
}
static void LoginFillUserNames()
{
    O(Op::ComboClear, M(LG_USER), 1);
    long newest = 0;
    intptr_t latest = 0;
    size_t i = 0;
    while ((intptr_t)i < O(Op::UserCount)) {
        intptr_t item = O(Op::ComboAddUserName, M(LG_USER), i);
        long aLastUse = (long)O(Op::UserLastUseAt, i);
        if (aLastUse > newest) { newest = aLastUse; latest = item; }
        ++i;
    }
    if (latest != 0) {
        O(Op::ComboSetSel, M(LG_USER), O(Op::ComboItemPos, M(LG_USER), latest), 0);
        LoginLoadPassword();
    }
}
static void LoginSavePassword()
{
    intptr_t userName = O(Op::ComboEditText, M(LG_USER));
    O(Op::AddRef, userName);
    intptr_t password = 0;
    if (O(Op::ButtonIsChecked, M(LG_REMEMBER))) { password = O(Op::InputText, M(LG_PASSWORD)); O(Op::AddRef, password); }
    if (O(Op::TextLen, userName) != 0) O(Op::AddUser, userName, password);
    if (password) O(Op::Release, password);
    O(Op::Release, userName);
}
static void LoginEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::InputTextChanged:
        if (inComponent == M(LG_USER) && O(Op::TextAt, O(Op::ComboEditText, M(LG_USER)), 0) == '@' &&
            O(Op::TextLen, O(Op::InputText, M(LG_PASSWORD))) != 0)
            O(Op::InputClear, M(LG_PASSWORD), 0);
        LoginUpdateButton();
        return;
    case Ev::InputReturn:
        if (O(Op::IsDisabled, M(LG_LOGIN))) {
            if (inComponent == M(LG_USER)) O(Op::RequestFocus, M(LG_PASSWORD));
            else if (inComponent == M(LG_PASSWORD)) O(Op::RequestFocus, M(LG_USER));
            return;
        }
        // Enter with the login button enabled falls through to the button-press checks
    case Ev::ButtonPressed:
        if (inComponent == M(LG_LOGIN)) { O(Op::DeliverLogin, M(LG_PASSWORD), M(LG_REMEMBER), M(LG_USER)); return; }
        else if (inComponent == M(LG_REMEMBER)) { LoginSavePassword(); return; }
        break;
    case Ev::ComboSelChanged:
        if (inComponent == M(LG_USER)) LoginLoadPassword();
        return;
    case Ev::LoginInitStatus:
        if (E(inEvent, EF::LisKind) == 1) { sO.mFlag = false; LoginUpdateButton(); O(Op::SetVisible, kMe, 1); }
        else if (E(inEvent, EF::LisKind) == 2) { sO.mFlag = true; LoginUpdateButton(); }
        else if (E(inEvent, EF::LisKind) == 3) O(Op::SetVisible, kMe, 0);
        return;
    case Ev::SetLoginFields:
        O(Op::ComboSetTextFlag, M(LG_USER), E(inEvent, EF::SlfUserName), 0);
        O(Op::InputSetText, M(LG_PASSWORD), E(inEvent, EF::SlfPassword), 0);
        O(Op::ButtonSetCheck, M(LG_REMEMBER), 0, 0);
        LoginUpdateButton();
        return;
    case Ev::FillUserNames: LoginFillUserNames(); return;
    default: break;
    }
    O(Op::DialogBaseEvent, inEvent);
}

// CreateAccountCtrl
static bool IsValidEmail(intptr_t inText)
{
    char aBytes[1024];
    intptr_t n = O(Op::TextBytes, inText, (intptr_t)aBytes, sizeof(aBytes));
    std::string inEmail(aBytes, (size_t)n);
    if (inEmail.empty()) return false;
    std::string::size_type at = inEmail.find('@', 1);
    if (at == std::string::npos) return false;
    std::string::size_type dot = inEmail.find('.', at);
    if (dot == std::string::npos) return false;
    std::string::size_type length = inEmail.length();
    if (length > dot + 1) return true;
    return false;
}
static bool ShowErrorLabel(bool inVisible, intptr_t inLabel) { O(Op::SetVisible, inLabel, inVisible); return inVisible; }
static int ToInt(intptr_t inInput) { return (int)O(Op::TextToInt, O(Op::InputText, inInput)); }
static void CreateUpdateButton()
{
    bool invalid = false;
    if (ShowErrorLabel(O(Op::TextLen, O(Op::InputText, M(0))) < 2, M(8))) invalid = true;
    if (ShowErrorLabel(O(Op::TextLen, O(Op::InputText, M(1))) < 4, M(9))) invalid = true;
    {
        intptr_t password = O(Op::InputText, M(1));
        intptr_t confirm = O(Op::InputText, M(2));
        if (ShowErrorLabel(!O(Op::TextEquals, password, confirm), M(10))) invalid = true;
    }
    bool emailInvalid = ShowErrorLabel(!IsValidEmail(O(Op::InputText, M(3))), M(11));
    if (emailInvalid) invalid = true;
    int month = ToInt(M(4)), day = ToInt(M(5)), year = ToInt(M(6));
    bool valid = !(month <= 0 || day <= 0 || year <= 1889 || month > 12 || day > 31 || year > 1999);
    if (ShowErrorLabel(!valid, M(12))) invalid = true;
    SetEnabled(M(13), !invalid);
}
static void CreateEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    if (inType == Ev::InputTextChanged) { CreateUpdateButton(); return; }
    if (inType == Ev::ButtonPressed && inComponent == M(13)) {
        intptr_t f[7] = {M(0), M(1), M(3), M(5), M(4), M(6), M(7)};
        O(Op::DeliverCreateAccount, (intptr_t)f);
    } else O(Op::BaseEvent, inEvent);
}

// UpdateAccountCtrl
static void UpdateCheckInput()
{
    bool error = false, queryAllowed = true;
    if (ShowErrorLabel(!O(Op::TextEquals, O(Op::InputText, M(1)), sO.mTexts[0]), M(7))) { error = true; queryAllowed = false; }
    if (ShowErrorLabel(O(Op::TextLen, O(Op::InputText, M(2))) != 0 && O(Op::TextLen, O(Op::InputText, M(2))) < 4, M(8))) error = true;
    {
        intptr_t password = O(Op::InputText, M(2));
        intptr_t confirm = O(Op::InputText, M(3));
        if (ShowErrorLabel(!O(Op::TextEquals, password, confirm), M(9))) error = true;
    }
    if (ShowErrorLabel(O(Op::TextLen, O(Op::InputText, M(4))) != 0 && !IsValidEmail(O(Op::InputText, M(4))), M(10))) error = true;
    SetEnabled(M(12), !error);
    SetEnabled(M(11), queryAllowed);
}
static void UpdateEnteredScreen()
{
    intptr_t context = O(Op::AuthContext);
    if (context == 0) return;
    O(Op::InputSetAuthName, M(0), context);
    O(Op::InputClear, M(1), 1);
    O(Op::InputClear, M(2), 1);
    O(Op::InputClear, M(3), 1);
    O(Op::InputClear, M(4), 1);
    O(Op::ButtonSetCheck, M(5), 0, 0);
    intptr_t aPassword = O(Op::AuthPassword, context);
    intptr_t anOld = sO.mTexts[0];
    sO.mTexts[0] = aPassword;
    if (anOld) O(Op::Release, anOld);
}
static void UpdateEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::InputTextChanged: UpdateCheckInput(); return;
    case Ev::ButtonPressed:
        if (inComponent == M(11)) { O(Op::FireEvent, 100040, 0); return; }
        if (inComponent == M(12)) O(Op::DeliverUpdateAccount, M(4), M(2), M(5));
        return;
    case Ev::SyncOptions: UpdateEnteredScreen(); return;
    case Ev::QueryAccountResult:
        O(Op::InputSetText, M(4), E(inEvent, EF::QarEmail), 1);
        O(Op::ButtonSetCheck, M(5), E(inEvent, EF::QarNewsletter), 0);
        return;
    default: O(Op::BaseEvent, inEvent); return;
    }
}

// the pages and the rest
static void LostPasswordUpdate() { SetEnabled(M(1), O(Op::TextLen, O(Op::ComboEditText, M(0))) >= 2); }
static void LostUserNameUpdate() { SetEnabled(M(1), IsValidEmail(O(Op::InputText, M(0)))); }
static void RoomLabelCalc()
{
    intptr_t aFormat;
    bool aHeld = false;
    std::string aCount;
    if (O(Op::TextLen, sO.mTexts[2]) == 0) aFormat = O(Op::ResText, (intptr_t)Res::RoomLabelCtrl_LabelDisconnected_String);
    else {
        aFormat = O(Op::TextNew, (intptr_t)"", 0);
        aHeld = true;
        if (O(Op::TextLen, sO.mTexts[0]) != 0) aFormat = O(Op::TextAppend, aFormat, O(Op::ResText, (intptr_t)Res::RoomLabelCtrl_LabelPrefix_String));
        if (sO.mFlag && O(Op::TextLen, sO.mTexts[1]) != 0) aFormat = O(Op::TextAppend, aFormat, O(Op::ResText, (intptr_t)Res::RoomLabelCtrl_LabelServer_String));
        aFormat = O(Op::TextAppend, aFormat, sO.mTexts[2]);
        if (sO.mNumber > 0) {
            aCount = std::to_string(sO.mNumber);
            intptr_t aSuffix = sO.mNumber == 1 ? O(Op::ResText, (intptr_t)Res::RoomLabelCtrl_LabelUser_String) : O(Op::ResText, (intptr_t)Res::RoomLabelCtrl_LabelUsers_String);
            aFormat = O(Op::TextAppend, aFormat, aSuffix);
        }
    }
    intptr_t aCountText = O(Op::TextNew, (intptr_t)aCount.c_str(), (intptr_t)aCount.size());
    intptr_t aArgs[3] = {sO.mTexts[0], sO.mTexts[1], aCountText};
    O(Op::LabelSetFormat3, M(0), aFormat, (intptr_t)aArgs);
    O(Op::Release, aCountText);
    if (aHeld) O(Op::Release, aFormat);
}
static void RoomLabelEvent(intptr_t inEvent, Ev inType)
{
    switch (inType) {
    case Ev::JoinedRoom:
        if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::JrRoomSpec)) != (int)E(inEvent, EF::JrRoomSpec)) return;
        Assign(sO.mTexts[1], E(inEvent, EF::JrServerName));
        Assign(sO.mTexts[2], E(inEvent, EF::JrGroupName));
        if (sO.mNumber >= 0) sO.mNumber = (int)Call2(E(inEvent, EF::JrClients), MC::ListCount);
        RoomLabelCalc();
        return;
    case Ev::LeftRoom:
        if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::LrRoomSpec)) != (int)E(inEvent, EF::LrRoomSpec)) return;
        Assign(sO.mTexts[1], 0);
        Assign(sO.mTexts[2], 0);
        if (sO.mNumber > 0) sO.mNumber = 0;
        RoomLabelCalc();
        return;
    case Ev::ClientChanged:
        if (sO.mNumber < 0) return;
        if ((sO.mRoomSpecFlags & (int)O(Op::EventRoomSpec, inEvent)) != (int)O(Op::EventRoomSpec, inEvent)) return;
        if (O(Op::EventIsAdd, inEvent)) { ++sO.mNumber; RoomLabelCalc(); return; }
        if (sO.mNumber > 0 && O(Op::EventIsDelete, inEvent)) --sO.mNumber;
        RoomLabelCalc();
        return;
    case Ev::SetRoomSpecFlags: sO.mRoomSpecFlags |= (int)O(Op::EventFlags, inEvent); return;
    default: O(Op::BaseEvent, inEvent); return;
    }
}
static void ShowPage(intptr_t inPage)
{
    static const int kPages[4] = {1, 2, 4, 3};
    O(Op::ScreenShow, M(0), M(kPages[inPage < 0 || inPage > 3 ? 3 : inPage]), 1);
}
static void Init(intptr_t inConfig)
{
    int first = SlotOf(sKind);
    switch (sKind) {
    case Kind::Room:
        for (int i = 0; i < 3; i++) SafeGet(inConfig, first + i);
        O(Op::SubscribeBroadcast, 1);
        RoomEnableControls();
        return;
    case Kind::Friends:
        for (int i = 0; i < 3; i++) SafeGet(inConfig, first + i);
        Assign(M(N_SEPARATOR), O(Op::FriendRowNew));
        IS(M(N_SEPARATOR), F::FriendSortGroup, 1);
        IS(M(N_SEPARATOR), F::Selectable, 0);
        O(Op::SubscribeBroadcast, 1);
        FriendsUpdateButtons();
        return;
    case Kind::Users:
        for (int i = 0; i < 3; i++) SafeGet(inConfig, first + i);
        O(Op::SubscribeBroadcast, 1);
        Assign(M(U_FIRST), O(Op::UserRowNew));
        IS(M(U_FIRST), F::Selectable, 0);
        IS(M(U_FIRST), F::UserSortKey, 3);
        Assign(M(U_SECOND), O(Op::UserRowNew));
        IS(M(U_SECOND), F::Selectable, 0);
        IS(M(U_SECOND), F::UserSortKey, 1);
        UsersUpdateButtons();
        return;
    case Kind::QuickPlay: for (int i = 0; i < 6; i++) SafeGet(inConfig, first + i); O(Op::SubscribeBroadcast, 1); return;
    case Kind::LoginStatus: SafeGet(inConfig, first); O(Op::SubscribeBroadcast, 1); return;
    case Kind::DirectConnect: for (int i = 0; i < 3; i++) SafeGet(inConfig, first + i); O(Op::SubscribeBroadcast, 1); return;
    case Kind::Login: for (int i = 0; i < 4; i++) SafeGet(inConfig, first + i); LoginUpdateButton(); O(Op::SubscribeBroadcast, 1); return;
    case Kind::CreateAccount: for (int i = 0; i < 14; i++) SafeGet(inConfig, first + i); CreateUpdateButton(); return;
    case Kind::UpdateAccount:
        for (int i = 0; i < 13; i++) SafeGet(inConfig, first + i);
        SetEnabled(M(0), false);
        UpdateCheckInput();
        O(Op::SubscribeBroadcast, 1);
        return;
    case Kind::LostPassword: SafeGet(inConfig, first); SafeGet(inConfig, first + 1); LostPasswordUpdate(); O(Op::SubscribeBroadcast, 1); return;
    case Kind::LostUserName: SafeGet(inConfig, first); SafeGet(inConfig, first + 1); LostUserNameUpdate(); return;
    case Kind::LoginHelp: SafeGet(inConfig, first); return;
    case Kind::InitFailed:
        for (int i = 0; i < 3; i++) SafeGet(inConfig, first + i);
        O(Op::SetVisible, kMe, 0);
        O(Op::SubscribeBroadcast, 1);
        return;
    case Kind::Motd: SafeGet(inConfig, first); O(Op::SubscribeBroadcast, 1); return;
    case Kind::LoginScreen:
        for (int i = 0; i < 6; i++) SafeGet(inConfig, first + i);
        O(Op::ScreenShow, M(0), M(1), 0);
        return;
    case Kind::RoomLabel: SafeGet(inConfig, first); RoomLabelCalc(); O(Op::SubscribeBroadcast, 1); return;
    default: return;
    }
}
static void Event(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (sKind) {
    case Kind::Room: RoomEvent(inEvent, inType, inComponent); return;
    case Kind::Friends: FriendsEvent(inEvent, inType, inComponent); return;
    case Kind::Users: UsersEvent(inEvent, inType, inComponent); return;
    case Kind::QuickPlay: QuickPlayEvent(inEvent, inType, inComponent); return;
    case Kind::Login: LoginEvent(inEvent, inType, inComponent); return;
    case Kind::CreateAccount: CreateEvent(inEvent, inType, inComponent); return;
    case Kind::UpdateAccount: UpdateEvent(inEvent, inType, inComponent); return;
    case Kind::LostPassword:
        if (inType == Ev::InputTextChanged) LostPasswordUpdate();
        else if (inType == Ev::ButtonPressed && inComponent == M(1)) O(Op::DeliverLostPassword, M(0));
        else if (inType == Ev::FillUserNames) {
            O(Op::ComboClear, M(0), 1);
            for (size_t i = 0; (intptr_t)i < O(Op::UserCount); ++i) O(Op::ComboAddUserName, M(0), i);
        } else O(Op::BaseEvent, inEvent);
        return;
    case Kind::LostUserName:
        if (inType == Ev::InputTextChanged) LostUserNameUpdate();
        else if (inType == Ev::ButtonPressed && inComponent == M(1)) O(Op::DeliverLostUserName, M(0));
        else O(Op::BaseEvent, inEvent);
        return;
    case Kind::LoginStatus:
        if (inType == Ev::LoginInitStatus) O(Op::LabelSetData, M(0), E(inEvent, EF::LisStatus));
        else O(Op::BaseEvent, inEvent);
        return;
    case Kind::InitFailed:
        if (inType == Ev::LoginInitStatus) {
            if (E(inEvent, EF::LisKind) == 1) O(Op::SetVisible, kMe, 0);
            else if (E(inEvent, EF::LisKind) == 3) {
                O(Op::LabelSetData, M(0), E(inEvent, EF::LisDescription));
                O(Op::SetVisible, M(2), E(inEvent, EF::LisAllowIgnore));
                O(Op::EnableLayout, kMe, 1, E(inEvent, EF::LisAllowIgnore));
                O(Op::SetVisible, kMe, 1);
                O(Op::Relayout, kMe);
            }
            return;
        }
        if (inType == Ev::ButtonPressed) {
            if (inComponent == M(1)) { O(Op::FireEvent, 100045, 0); return; }
            if (inComponent == M(2)) { O(Op::FireEvent, 100025, 0); return; }
        }
        O(Op::BaseEvent, inEvent);
        return;
    case Kind::Motd:
        if (inType == Ev::GotHttpDocument) {
            intptr_t anOp = E(inEvent, EF::GhdOp);
            if (!O(Op::HttpDocIsMotd, anOp)) return;
            sO.mFlag = true;
            char aBytes[1024];
            intptr_t n = O(Op::HttpDocPath, anOp, 0, (intptr_t)aBytes);
            sO.mPaths[0] = std::string(aBytes, (size_t)n);
            n = O(Op::HttpDocPath, anOp, 1, (intptr_t)aBytes);
            sO.mPaths[1] = std::string(aBytes, (size_t)n);
            O(Op::Invalidate, kMe);
            return;
        }
        O(Op::BaseEvent, inEvent);
        return;
    case Kind::LoginScreen:
        if (inType == Ev::ButtonPressed) {
            switch ((int)O(Op::ControlId, inComponent)) {
            case 200007: O(Op::ScreenShow, M(0), M(5), 1); return;
            case 200004: ShowPage(0); return;
            }
        }
        O(Op::BaseEvent, inEvent);
        return;
    case Kind::DirectConnect:
        if (inType == Ev::ButtonPressed) {
            if (inComponent == M(1)) { O(Op::DeliverDirectConnect, 0, 0); return; }
            else if (inComponent == M(2)) {
                intptr_t game = O(Op::ListSelGame, M(0));
                if (game != 0) O(Op::DeliverDirectConnect, 1, game);
                return;
            }
        }
        O(Op::BaseEvent, inEvent);
        return;
    case Kind::RoomLabel: RoomLabelEvent(inEvent, inType); return;
    default: O(Op::BaseEvent, inEvent); return;
    }
}
static void Paint(intptr_t g)
{
    if (sKind == Kind::LoginHelp && !sO.mFlag) { O(Op::BrowserGoLoginHelp, M(0)); sO.mFlag = true; }
    if (sKind == Kind::Motd && sO.mFlag) {
        sO.mFlag = false;
        if (!sO.mPaths[0].empty() && !sO.mPaths[1].empty()) {
            EeWonLobbyCtlFiles f = {(const uint8_t*)sO.mPaths[0].data(), sO.mPaths[0].size(), (const uint8_t*)sO.mPaths[1].data(), sO.mPaths[1].size()};
            O(Op::BrowserShowFiles, M(0), (intptr_t)&f);
            sO.mPaths[0].erase();
            sO.mPaths[1].erase();
        }
    }
    O(Op::BasePaint, g);
}
static void SetField(intptr_t f, intptr_t v)
{
    if (sKind != Kind::RoomLabel) return;
    switch (f) {
    case 0: Assign(sO.mTexts[0], v); return;
    case 1: sO.mRoomSpecFlags = (int)v; return;
    case 2: if (!v) sO.mNumber = -1; else if (sO.mNumber < 0) sO.mNumber = 0; return;
    case 3: sO.mFlag = v != 0; return;
    }
}
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Init: Init(a); return 0;
    case Call::Event: Event(a, (Ev)b, c); return 0;
    case Call::Paint: if (sKind == Kind::LoginHelp || sKind == Kind::Motd) Paint(a); return 0;
    case Call::SetField: SetField(a, b); return 0;
    case Call::Page: if (sKind == Kind::LoginScreen) ShowPage(a); return 0;
    case Call::Field:
        if (a >= 0 && a < 16) return sO.m[a];
        switch (a) {
        case 100: return sO.mRoomSpecFlags;
        case 150: return sO.mFlag;
        case 151: return sO.mNumber;
        case 152: case 153: case 154: return sO.mTexts[a - 152];
        }
        return 0;
    default: return 0;
    }
}
static int MemberCount(Kind k)
{
    switch (k) {
    case Kind::Room: case Kind::DirectConnect: case Kind::InitFailed: return 3;
    case Kind::Friends: case Kind::Login: return 4;
    case Kind::Users: return 5;
    case Kind::QuickPlay: case Kind::LoginScreen: return 6;
    case Kind::CreateAccount: return 14;
    case Kind::UpdateAccount: return 13;
    case Kind::LostPassword: case Kind::LostUserName: return 2;
    default: return 1;
    }
}
static void OrigFree()
{
    for (int i = 2; i >= 0; i--) ReleasePtr(sO.mTexts[i]);
    for (int i = MemberCount(sKind) - 1; i >= 0; i--) ReleasePtr(sO.m[i]);
}

// ---- the sequences

struct Step { Call mCall; intptr_t a, b, c; };

static std::vector<std::string> RunSide(bool inRust, Kind inKind, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sKind = inKind;
    sNextHeld = 0xA000;
    if (inRust) sRustCtl = ee_won_lobbyctl_new((void*)kMe, inKind, RustHost);
    else {
        sO = Orig();
        memset(sO.m, 0, sizeof(sO.m));
        sO.mFlag = inKind == Kind::RoomLabel;
        sO.mNumber = inKind == Kind::Users ? 5 : 0;
        sO.mRoomSpecFlags = 0;
        sO.mTexts[0] = sO.mTexts[1] = sO.mTexts[2] = 0;
    }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        Line("call %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.c);
        intptr_t r = inRust ? ee_won_lobbyctl_call(sRustCtl, s.mCall, s.a, s.b, s.c) : OrigCall(s.mCall, s.a, s.b, s.c);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_lobbyctl_free(sRustCtl);
    else OrigFree();
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the look-ups' members against the originals' SafeGetComponent targets
    CHECK(ee_won_lobbyctl_slot_count() == 177);
    for (int i = 111; i < 177; i++) CHECK(ee_won_lobbyctl_slot(i)->member == kMember[i - 111]);
    CHECK(strcmp(ee_won_lobbyctl_slot(111)->name, "RoomList") == 0 && ee_won_lobbyctl_slot(111)->line == 168);
    CHECK(strcmp(ee_won_lobbyctl_slot(176)->name, "Label") == 0 && strcmp(ee_won_lobbyctl_slot(176)->file, "RoomLabelCtrl.cpp") == 0);
    // the e-mail rule against the original's
    for (size_t k = 0; k < sizeof(kEmails) / sizeof(kEmails[0]); k++) {
        std::string inEmail = kEmails[k];
        bool aOrig;
        {
            std::string::size_type at = inEmail.find('@', 1), dot;
            aOrig = !inEmail.empty() && at != std::string::npos && (dot = inEmail.find('.', at)) != std::string::npos &&
                    inEmail.length() > dot + 1;
        }
        CHECK(aOrig == ee_won_lobbyctl_valid_email((const uint8_t*)inEmail.data(), inEmail.size()));
    }
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 3083;
    int kSequences = 80000;
    if (argc > 2) kSequences = atoi(argv[2]);
    static const Ev kEvents[] = {Ev::ButtonPressed, Ev::ButtonPressed, Ev::ListSelChanged, Ev::ListItemClicked,
                                 Ev::InputTextChanged, Ev::InputReturn, Ev::ComboSelChanged, Ev::TreeItemOpened,
                                 Ev::GroupChanged, Ev::ServerChanged, Ev::SetRoomList, Ev::FriendChanged,
                                 Ev::RefreshFriends, Ev::SyncOptions, Ev::ClientChanged, Ev::JoinedRoom, Ev::LeftRoom,
                                 Ev::LoginInitStatus, Ev::SetLoginFields, Ev::FillUserNames, Ev::QueryAccountResult,
                                 Ev::GotHttpDocument, Ev::SetRoomSpecFlags, Ev::Other};
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        Kind aKind = (Kind)(9 + R(16));
        int aFirst = SlotOf(aKind), aCount = MemberCount(aKind);
        if (aKind == Kind::Friends || aKind == Kind::Users) aCount = 3;
        std::vector<Step> aSteps;
        if (aKind == Kind::RoomLabel) {
            int k = (int)R(4);
            for (int i = 0; i < k; i++) { Step s = {Call::SetField, (intptr_t)R(5), (intptr_t)(R(3) ? R(3) : kText + R(3)), 0}; aSteps.push_back(s); }
        }
        if (R(6)) { Step s = {Call::Init, 0xC0, 0, 0}; aSteps.push_back(s); }
        int aCalls = 1 + (int)R(12);
        for (int i = 0; i < aCalls; i++) {
            Step s = {Call::Event, kEvent, (intptr_t)kEvents[R(sizeof(kEvents) / sizeof(kEvents[0]))], 0};
            switch (R(12)) {
            case 0: s.mCall = Call::Init; s.a = 0xC0; s.b = 0; break;
            case 1: if (aKind == Kind::LoginHelp || aKind == Kind::Motd) { s.mCall = Call::Paint; s.a = 0x6100; s.b = 0; } break;
            case 2: s.mCall = Call::Field; s.a = R(2) ? (intptr_t)R(16) : R(2) ? 100 : 150 + (intptr_t)R(5); s.b = 0; break;
            case 3: if (aKind == Kind::LoginScreen) { s.mCall = Call::Page; s.a = (intptr_t)R(4); s.b = 0; } break;
            default: break;
            }
            if (s.mCall == Call::Event) {
                unsigned w = R(6);
                if (w == 0) s.c = 0;
                else if (w == 1) s.c = kOther;
                else s.c = kCtl + (aFirst + (intptr_t)R((unsigned)aCount)) * 0x10;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 131u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aKind, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aKind, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d (kind %d) differs\n", n, (int)aKind);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    if (i >= a.size() || i >= b.size() || a[i] != b[i]) {
                        printf("  line %zu: C++ '%s' Rust '%s'\n", i, i < a.size() ? a[i].c_str() : "-", i < b.size() ? b[i].c_str() : "-");
                        for (size_t k = i > 6 ? i - 6 : 0; k < i; k++) printf("    %s\n", a[k].c_str());
                        break;
                    }
                }
            }
            continue;
        }
        bool aNew = n < 80;
        for (size_t i = 0; i < a.size(); i++) {
            long long t[4] = {0, 0, 0, 0};
            char aWord[16] = "";
            int k = sscanf(a[i].c_str(), "%15s %lld %lld %lld %lld", aWord, &t[0], &t[1], &t[2], &t[3]);
            std::string aKey = aWord;
            if (k >= 2 && strcmp(aWord, "ret") != 0) aKey += " " + std::to_string(t[0]);
            if (strcmp(aWord, "op") == 0 && k >= 4 && (t[0] == (long long)Op::EvGet || t[0] == (long long)Op::MGet ||
                                                       t[0] == (long long)Op::MCall || t[0] == (long long)Op::ItemGet ||
                                                       t[0] == (long long)Op::ItemSet))
                aKey += " " + std::to_string(t[2]);
            if (strcmp(aWord, "call") == 0 && k >= 4 && t[0] == (long long)Call::Event) aKey += " " + std::to_string(t[2]) + " k" + std::to_string((int)aKind);
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d kind %d\n", n, (int)aKind);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_netctl_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
