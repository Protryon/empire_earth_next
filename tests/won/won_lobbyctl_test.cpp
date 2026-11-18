// The lobby screens' controls as one Rust object (R6 WONLobby rounds 4 and 5): the original GameBrowserCtrl.cpp,
// GameStagingCtrl.cpp, LobbyOptionsCtrl.cpp, GameUserListCtrl.cpp, EELobbyGameOptionsCtrl.cpp and
// EELobbyPlayerOptionsCtrl.cpp (the statements copied from the C++ before the
// port, every call on a control, an event, the lobby's model, its settings or a global replaced by the host op that
// makes it: LobbyCtrlHost.cpp runs the same calls) against ee_won's lobbyctl.rs over one scripted host.
//
// Each of 70,000 sequences makes a control of a random kind, then 1 to 10 random calls (InitComponent, a second one
// now and then; every event type on the control's own members, on no component and on another one; the browser's
// paint and reset; the members read back), and frees it. The host answers each op from a seeded generator (the same
// stream for both sides); a check box set with its event, a combo box or a list cleared with its event re-enters
// the control with the matching event a third of the time (two deep at most), as does the saved game's file
// dialog with a random event. The traces must be the same.
//
// The control tables are checked name by name, line by line and type by type against the original
// SafeGetComponent arguments.
//
// The first 80 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won_lobby/src/golden_lobbyctl.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/lobbyctl.h"

typedef EeWonLobbyCtlOp Op;
typedef EeWonLobbyCtlCall Call;
typedef EeWonLobbyCtlKind Kind;
typedef EeWonLobbyCtlEvent Ev;
typedef EeWonLobbyCtlString S;
typedef EeWonLobbyCtlPref P;
typedef EeWonLobbyCtlClientTest T;
typedef EeWonLobbyCtlImage I;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::vector<std::string> sTrace;
static std::set<std::string> sSeen;

static void Line(const char* inFormat, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0)
{
    char aBuffer[160];
    snprintf(aBuffer, sizeof(aBuffer), inFormat, a, b, c, d, e);
    sTrace.push_back(aBuffer);
}

static const intptr_t kPlayer = 0x3700;
static const intptr_t kCtl = 0x1000, kKick = 0x2000, kGame = 0x3000, kClients = 0x3100, kClient = 0x3200,
                      kItem = 0x3300, kNewItem = 0x3400, kScreen = 0x3500, kSlot = 0x3600, kEvent = 0xE1,
                      kOther = 0x77;
static const int kControlIds[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0x30d55, 0x30d56, 99};

static bool sRust;
static EeWonLobbyCtl* sRustCtl;
// R6 WONLobby round 6: the game is a real ee_won game (eegame.rs) on both sides. The original's calls on it are the
// C++ forwarders' (EELobbyGame::GetOption and the others were ee_won_eegame_call on its mRust); the Rust control
// calls it directly. Its ops are traced as "gop"; the lobby's sync events it raises re-enter the control.
static EeWonEeGame* sGame;
typedef EeWonEeGameCall GC;
static intptr_t EG(GC inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ee_won_eegame_call(sGame, inCall, a, b, c, 0);
}
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

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::Find: r = R(12) ? kCtl + b * 0x10 : 0; break;
    case Op::FindOptional: r = R(3) ? kKick : 0; break;
    case Op::EventControlId: r = R(4) ? R(50) : R(18); break;
    case Op::EventFlags: case Op::EventRoomSpec: r = R(8); break;
    case Op::EventButton: r = R(3); break;
    case Op::MyGame: r = R(5) ? kGame : 0; break;
    case Op::GameClients: r = R(5) ? kClients : 0; break;
    case Op::GameCaptainId: case Op::ClientId: r = R(4); break;
    case Op::ClientsCount: case Op::ListNumItems: r = R(5); break;
    case Op::ClientsAt: r = kClient + b; break;
    case Op::ListGetSel: r = R(3) ? kItem + R(3) : 0; break;
    case Op::ListItem: r = kItem + b; break;
    case Op::ItemClient: case Op::ClientChangedClient: case Op::PlayerChangedClient:
        r = R(5) ? kClient + R(3) : 0; break;
    case Op::ItemNew: r = kNewItem; break;
    case Op::ButtonOf: r = R(5) ? a : 0; break;
    case Op::ControlId: r = kControlIds[R(16)]; break;
    case Op::PrefGet: r = a == (intptr_t)P::ConnectionSpeed ? (intptr_t)R(70) - 5 : R(2); break;
    case Op::ComboGetSel: r = (intptr_t)R(8) - 1; break;
    case Op::LanNameCount: r = R(4); break;
    case Op::LanBroadcastPort: r = R(70000); break;
    case Op::CurrentScreen: r = R(3) ? kScreen : 0; break;
    case Op::EventIsModify: case Op::EventIsAdd: case Op::EventIsDelete: case Op::EventIsSelectedItem:
    case Op::EventDoubleClick: case Op::BrowserBackEnabled: case Op::BrowserForwardEnabled:
    case Op::BrowserStopEnabled: case Op::ButtonIsChecked: case Op::GameIsHost: case Op::GameCanStart:
    case Op::GameCanReady: case Op::GameMatchesSpec: case Op::IAmReady: case Op::ClientHasPlayer:
    case Op::ItemClientIs: case Op::OnLoginScreen: case Op::OnLanScreen: case Op::FirewallTestDone:
    case Op::BehindFirewall: case Op::PublicAddressValid:
    case Op::IsButton: case Op::IsCombo: case Op::ComboHasSel: case Op::IAmCaptain:
        r = R(5) != 0; break;
    // the C++ game's Rust object: the side's real game (a field read, not traced)
    case Op::GameRust: return (intptr_t)sGame;
    case Op::MyPlayerReady: r = R(3) == 0; break;
    case Op::MyPlayer: r = R(5) ? kPlayer : 0; break;
    case Op::PlayerSlot: r = R(4) ? R(8) : R(256); break;
    case Op::ClientPlayer: r = R(4) ? kPlayer : 0; break;
    case Op::ColorGetColor: r = R(17); break;
    case Op::EventParam: r = (intptr_t)R(19) - 1; break;
    case Op::ChooseSavedGame: r = R(3) == 0 ? -1 : R(2) ? 100001 : 100002; break;
    default: break;
    }
    Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    // the events a notifying set or clear delivers to the control
    if (inOp == Op::ButtonSetCheck && c != 0) Reenter(Ev::ButtonPressed, a);
    if (inOp == Op::ComboClear && b != 0) Reenter(Ev::ComboSelChanged, a);
    if (inOp == Op::ListClear && b != 0) Reenter(Ev::ListSelChanged, kOther);
    if (inOp == Op::ChooseSavedGame) Reenter((Ev)R(20), kOther);
    return r;
}

static intptr_t GameHost(void*, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    if (inOp == EeWonEeGameOp::NumMapTypes) return 12; // read by a new game (untraced; the Rust tests answer 12)
    intptr_t r = 0;
    switch (inOp) {
    case EeWonEeGameOp::IsHost: case EeWonEeGameOp::IAmCaptain: case EeWonEeGameOp::ClientIsCaptain: r = R(3) != 0; break;
    case EeWonEeGameOp::NewBuffer: r = 0x7000; break;
    case EeWonEeGameOp::ClientName: r = R(2) ? 0x7100 + R(4) : 0; break;
    default: r = R(4); break;
    }
    Line("gop %lld %lld %lld %lld %lld", (long long)inOp, a, b, c, d);
    sTrace.back() += " = " + std::to_string((long long)r);
    if (inOp == EeWonEeGameOp::SyncOption) Reenter(Ev::SyncOption, kOther);
    if (inOp == EeWonEeGameOp::SyncPlayer) Reenter(Ev::SyncPlayer, kOther);
    return r;
}

// ---- the original classes, every call on another object replaced by its op

struct Orig {
    Kind mKind;
    // GameBrowserCtrl
    intptr_t mBrowser, mBackButton, mForwardButton, mStopButton, mHomeButton, mRefreshButton, mFindInput,
        mFindNextButton, mFindPrevButton, mCaseSensitiveCheck, mStatusLabel;
    bool mIsReset;
    // GameStagingCtrl
    intptr_t mLeaveGameButton, mStartGameButton;
    int mRoomSpecFlags;
    bool mInGame, mLastReady;
    // LobbyOptionsCtrl
    bool mIsLan;
    intptr_t mTabSelectCombo, mConnectionSpeedCombo, mCrossPromotionCheck, mLobbySoundEffectsCheck,
        mChatSoundEffectsCheck, mStagingSoundEffectsCheck, mLobbyMusicCheck, mShowConfirmationsCheck,
        mShowGamesInProgressCheck, mAnonymousToFriendsCheck, mAutoPingGamesCheck, mLocalAddressDetailLabel,
        mPublicAddressDetailLabel, mFirewalledIcon, mUserNameCombo, mLANBroadcastDetailLabel, mFilterDirtyWordsCheck,
        mDoAutoAFKCheck;
    // GameUserListCtrl
    intptr_t mUserList, mKickButton;
    // EELobbyGameOptionsCtrl (its mIsActive is mInGame)
    intptr_t mCampaignInfoLabel, mCampaignInfoBox1, mCampaignInfoBox2, mOptions[16];
    // EELobbyPlayerOptionsCtrl (its mIsActive is mInGame, mRoomSpec mRoomSpecFlags)
    intptr_t mPlayerNameCombo[8], mTeamCombo[8], mColorButton[8], mCitizenCombo[8], mObserveCheck[8],
        mFileTransferLabel[8];
    long mMySlot;
    bool mIsHost;
};
static Orig sO;

static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }

// ConstSmartPtr::operator=: nothing for the same pointer; the old one released, the new one referenced
static void Assign(intptr_t& ioMember, intptr_t inNew)
{
    if (ioMember == inNew) return;
    if (ioMember != 0) O(Op::Release, ioMember);
    ioMember = inNew;
    if (inNew != 0) O(Op::AddRef, inNew);
}
static void ReleasePtr(intptr_t& ioPtr) { if (ioPtr != 0) O(Op::Release, ioPtr); ioPtr = 0; }

// the original SafeGetComponent arguments, by slot (the Find op's b): name, line, type
struct Orig_Slot { const char* mName; int mLine; EeWonLobbyCtlType mType; int mId; };
static const Orig_Slot kOrigSlots[] = {
    {"Browser", 27, EeWonLobbyCtlType::Browser, -1}, {"BackButton", 28, EeWonLobbyCtlType::Button, -1},
    {"ForwardButton", 29, EeWonLobbyCtlType::Button, -1}, {"StopButton", 30, EeWonLobbyCtlType::Button, -1},
    {"HomeButton", 31, EeWonLobbyCtlType::Button, -1}, {"RefreshButton", 32, EeWonLobbyCtlType::Button, -1},
    {"FindInput", 33, EeWonLobbyCtlType::Input, -1}, {"FindNextButton", 34, EeWonLobbyCtlType::Button, -1},
    {"FindPrevButton", 35, EeWonLobbyCtlType::Button, -1}, {"CaseSensitiveCheck", 36, EeWonLobbyCtlType::Button, -1},
    {"StatusLabel", 37, EeWonLobbyCtlType::Label, -1},
    {"LeaveGameButton", 33, EeWonLobbyCtlType::Button, -1}, {"StartGameButton", 34, EeWonLobbyCtlType::Button, -1},
    {"ChatSoundEffectsCheck", 95, EeWonLobbyCtlType::Button, 2}, {"ConnectionSpeedCombo", 96, EeWonLobbyCtlType::Combo, -1},
    {"CrossPromotionCheck", 97, EeWonLobbyCtlType::Button, 3}, {"FirewalledIcon", 98, EeWonLobbyCtlType::Image, -1},
    {"LobbyMusicCheck", 99, EeWonLobbyCtlType::Button, 5}, {"LobbySoundEffectsCheck", 100, EeWonLobbyCtlType::Button, 6},
    {"LocalAddressDetailLabel", 101, EeWonLobbyCtlType::Label, -1},
    {"PublicAddressDetailLabel", 102, EeWonLobbyCtlType::Label, -1},
    {"ResetFiltersButton", 103, EeWonLobbyCtlType::Button, 8}, {"RestoreDefaultsButton", 104, EeWonLobbyCtlType::Button, 9},
    {"ShowConfirmationsCheck", 105, EeWonLobbyCtlType::Button, 10},
    {"ShowGamesInProgressCheck", 106, EeWonLobbyCtlType::Button, 11},
    {"StagingSoundEffectsCheck", 107, EeWonLobbyCtlType::Button, 12}, {"TabSelectCombo", 108, EeWonLobbyCtlType::Combo, -1},
    {"LANBroadcastDetailLabel", 112, EeWonLobbyCtlType::Label, -1}, {"RemoveLanNameButton", 113, EeWonLobbyCtlType::Button, 7},
    {"UserNameCombo", 114, EeWonLobbyCtlType::Combo, -1},
    {"AnonymousToFriendsCheck", 118, EeWonLobbyCtlType::Button, 0x30d56},
    {"AutoPingGamesCheck", 119, EeWonLobbyCtlType::Button, 1}, {"DoAutoAFKCheck", 120, EeWonLobbyCtlType::Button, 4},
    {"FilterDirtyWordsCheck", 121, EeWonLobbyCtlType::Button, 0x30d55},
    {"UserList", 159, EeWonLobbyCtlType::MultiList, -1}, {"KickButton", 182, EeWonLobbyCtlType::Button, -1},
    {"CampaignInfoLabel", 74, EeWonLobbyCtlType::Label, -1}, {"CampaignInfoBox1", 75, EeWonLobbyCtlType::Container, -1},
    {"CampaignInfoBox2", 76, EeWonLobbyCtlType::Container, -1},
    // sOptionTable's names, each SafeGetComponent<Component> at line 80
    {"GameTypeCombo", 80, EeWonLobbyCtlType::Component, -1}, {"MapSizeCombo", 80, EeWonLobbyCtlType::Component, -1},
    {"MapTypeCombo", 80, EeWonLobbyCtlType::Component, -1}, {"ResourcesCombo", 80, EeWonLobbyCtlType::Component, -1},
    {"StartingEpochCombo", 80, EeWonLobbyCtlType::Component, -1}, {"EndingEpochCombo", 80, EeWonLobbyCtlType::Component, -1},
    {"GameVariantCombo", 80, EeWonLobbyCtlType::Component, -1},
    {"DifficultyLevelCombo", 80, EeWonLobbyCtlType::Component, -1}, {"GameSpeedCombo", 80, EeWonLobbyCtlType::Component, -1},
    {"GameUnitLimitCombo", 80, EeWonLobbyCtlType::Component, -1}, {"WondersCombo", 80, EeWonLobbyCtlType::Component, -1},
    {"RevealMapCheck", 80, EeWonLobbyCtlType::Component, -1}, {"UseCustomCivsCheck", 80, EeWonLobbyCtlType::Component, -1},
    {"LockTeamsCheck", 80, EeWonLobbyCtlType::Component, -1}, {"LockSpeedCheck", 80, EeWonLobbyCtlType::Component, -1},
    {"VictoryCheck", 80, EeWonLobbyCtlType::Component, -1},
    // EELobbyPlayerOptionsCtrl::InitComponent's sprintf'd names, ids slot * 6 + column + 1
    {"UserNameCombo1", 50, EeWonLobbyCtlType::Combo, 1}, {"TeamCombo1", 54, EeWonLobbyCtlType::Combo, 2}, {"ColorButton1", 58, EeWonLobbyCtlType::ColorButton, 3},
    {"CitizenCombo1", 62, EeWonLobbyCtlType::Combo, 4}, {"ObserveCheck1", 66, EeWonLobbyCtlType::Button, 5}, {"FileTransferLabel1", 70, EeWonLobbyCtlType::Label, 6},
    {"UserNameCombo2", 50, EeWonLobbyCtlType::Combo, 7}, {"TeamCombo2", 54, EeWonLobbyCtlType::Combo, 8}, {"ColorButton2", 58, EeWonLobbyCtlType::ColorButton, 9},
    {"CitizenCombo2", 62, EeWonLobbyCtlType::Combo, 10}, {"ObserveCheck2", 66, EeWonLobbyCtlType::Button, 11}, {"FileTransferLabel2", 70, EeWonLobbyCtlType::Label, 12},
    {"UserNameCombo3", 50, EeWonLobbyCtlType::Combo, 13}, {"TeamCombo3", 54, EeWonLobbyCtlType::Combo, 14}, {"ColorButton3", 58, EeWonLobbyCtlType::ColorButton, 15},
    {"CitizenCombo3", 62, EeWonLobbyCtlType::Combo, 16}, {"ObserveCheck3", 66, EeWonLobbyCtlType::Button, 17}, {"FileTransferLabel3", 70, EeWonLobbyCtlType::Label, 18},
    {"UserNameCombo4", 50, EeWonLobbyCtlType::Combo, 19}, {"TeamCombo4", 54, EeWonLobbyCtlType::Combo, 20}, {"ColorButton4", 58, EeWonLobbyCtlType::ColorButton, 21},
    {"CitizenCombo4", 62, EeWonLobbyCtlType::Combo, 22}, {"ObserveCheck4", 66, EeWonLobbyCtlType::Button, 23}, {"FileTransferLabel4", 70, EeWonLobbyCtlType::Label, 24},
    {"UserNameCombo5", 50, EeWonLobbyCtlType::Combo, 25}, {"TeamCombo5", 54, EeWonLobbyCtlType::Combo, 26}, {"ColorButton5", 58, EeWonLobbyCtlType::ColorButton, 27},
    {"CitizenCombo5", 62, EeWonLobbyCtlType::Combo, 28}, {"ObserveCheck5", 66, EeWonLobbyCtlType::Button, 29}, {"FileTransferLabel5", 70, EeWonLobbyCtlType::Label, 30},
    {"UserNameCombo6", 50, EeWonLobbyCtlType::Combo, 31}, {"TeamCombo6", 54, EeWonLobbyCtlType::Combo, 32}, {"ColorButton6", 58, EeWonLobbyCtlType::ColorButton, 33},
    {"CitizenCombo6", 62, EeWonLobbyCtlType::Combo, 34}, {"ObserveCheck6", 66, EeWonLobbyCtlType::Button, 35}, {"FileTransferLabel6", 70, EeWonLobbyCtlType::Label, 36},
    {"UserNameCombo7", 50, EeWonLobbyCtlType::Combo, 37}, {"TeamCombo7", 54, EeWonLobbyCtlType::Combo, 38}, {"ColorButton7", 58, EeWonLobbyCtlType::ColorButton, 39},
    {"CitizenCombo7", 62, EeWonLobbyCtlType::Combo, 40}, {"ObserveCheck7", 66, EeWonLobbyCtlType::Button, 41}, {"FileTransferLabel7", 70, EeWonLobbyCtlType::Label, 42},
    {"UserNameCombo8", 50, EeWonLobbyCtlType::Combo, 43}, {"TeamCombo8", 54, EeWonLobbyCtlType::Combo, 44}, {"ColorButton8", 58, EeWonLobbyCtlType::ColorButton, 45},
    {"CitizenCombo8", 62, EeWonLobbyCtlType::Combo, 46}, {"ObserveCheck8", 66, EeWonLobbyCtlType::Button, 47}, {"FileTransferLabel8", 70, EeWonLobbyCtlType::Label, 48},
};
static const char* const kOrigFiles[] = {"GameBrowserCtrl.cpp", "GameStagingCtrl.cpp", "LobbyOptionsCtrl.cpp",
                                         "GameUserListCtrl.cpp", "EELobbyGameOptionsCtrl.cpp",
                                         "EELobbyPlayerOptionsCtrl.cpp"};
static int OrigFileOf(int inSlot)
{
    return inSlot <= 10 ? 0 : inSlot <= 12 ? 1 : inSlot <= 33 ? 2 : inSlot <= 35 ? 3 : inSlot <= 54 ? 4 : 5;
}

// SafeGetComponent(config, outPtr, name, [id,] file, line), the name given by its slot
static void SafeGetComponent(intptr_t inConfig, intptr_t& outPtr, int inSlot)
{
    Assign(outPtr, O(Op::Find, inConfig, inSlot));
    if (kOrigSlots[inSlot].mId >= 0) O(Op::SetControlId, outPtr, kOrigSlots[inSlot].mId);
}

// GameBrowserCtrl
static void BrowserEnableButtons()
{
    intptr_t button = sO.mBackButton;
    O(Op::SetEnabled, button, O(Op::BrowserBackEnabled, sO.mBrowser));
    button = sO.mForwardButton;
    O(Op::SetEnabled, button, O(Op::BrowserForwardEnabled, sO.mBrowser));
    intptr_t browser = sO.mBrowser;
    intptr_t stop = sO.mStopButton;
    O(Op::SetEnabled, stop, O(Op::BrowserStopEnabled, browser));
}
static void BrowserDoFind(bool inNext)
{
    intptr_t check = sO.mCaseSensitiveCheck;
    intptr_t input = sO.mFindInput;
    intptr_t browser = sO.mBrowser;
    bool aCase = O(Op::ButtonIsChecked, check) != 0; // browser->FindStr(input->GetText(),inNext,check->IsChecked())
    O(Op::BrowserFind, browser, input, (inNext ? 1 : 0) | (aCase ? 2 : 0));
}
static void BrowserGoHome() { O(Op::BrowserGoHome, sO.mBrowser); }
static void BrowserInit(intptr_t config)
{
    SafeGetComponent(config, sO.mBrowser, 0);
    SafeGetComponent(config, sO.mBackButton, 1);
    SafeGetComponent(config, sO.mForwardButton, 2);
    SafeGetComponent(config, sO.mStopButton, 3);
    SafeGetComponent(config, sO.mHomeButton, 4);
    SafeGetComponent(config, sO.mRefreshButton, 5);
    SafeGetComponent(config, sO.mFindInput, 6);
    SafeGetComponent(config, sO.mFindNextButton, 7);
    SafeGetComponent(config, sO.mFindPrevButton, 8);
    SafeGetComponent(config, sO.mCaseSensitiveCheck, 9);
    SafeGetComponent(config, sO.mStatusLabel, 10);
    O(Op::SetControlId, sO.mBackButton, 2);
    O(Op::SetControlId, sO.mForwardButton, 3);
    O(Op::SetControlId, sO.mStopButton, 5);
    O(Op::SetControlId, sO.mRefreshButton, 1);
    O(Op::SetControlId, sO.mHomeButton, 4);
    O(Op::SetControlId, sO.mFindNextButton, 7);
    O(Op::SetControlId, sO.mFindPrevButton, 8);
    BrowserEnableButtons();
}
static void BrowserEvent(intptr_t inEvent, Ev inType)
{
    switch (inType) {
    case Ev::BrowserButtonChange: BrowserEnableButtons(); return;
    case Ev::BrowserStatusChange: O(Op::LabelSetBrowserStatus, sO.mStatusLabel, sO.mBrowser); return;
    case Ev::ButtonPressed:
        switch (O(Op::EventControlId, inEvent)) {
        case 2: O(Op::BrowserBack, sO.mBrowser); break;
        case 3: O(Op::BrowserForward, sO.mBrowser); break;
        case 5: O(Op::BrowserStop, sO.mBrowser); break;
        case 1: O(Op::BrowserRefresh, sO.mBrowser); break;
        case 4: BrowserGoHome(); break;
        case 7: BrowserDoFind(true); break;
        case 8: BrowserDoFind(false); break;
        }
        return;
    default: return;
    }
}
static void BrowserPaint(intptr_t inGraphics)
{
    if (sO.mIsReset) { sO.mIsReset = false; BrowserGoHome(); } // GoHomeIfReset
    O(Op::BasePaint, inGraphics);
}

// GameStagingCtrl
static void StagingInit(intptr_t config)
{
    SafeGetComponent(config, sO.mLeaveGameButton, 11);
    SafeGetComponent(config, sO.mStartGameButton, 12);
    O(Op::SetEnabled, sO.mStartGameButton, 0);
    O(Op::SubscribeBroadcast, 1);
}
static void StagingUpdateStartButton()
{
    if (!sO.mInGame) return;
    intptr_t game = O(Op::MyGame);
    if (game == 0) return;
    bool enabled;
    if (O(Op::GameIsHost, game)) enabled = O(Op::GameCanStart, game) != 0;
    else {
        if (sO.mLastReady != (O(Op::IAmReady) != 0)) {
            sO.mLastReady = O(Op::IAmReady) != 0;
            O(Op::ButtonSetText, sO.mStartGameButton, (intptr_t)(sO.mLastReady ? S::NotReady : S::Ready));
        }
        if (!sO.mLastReady) enabled = O(Op::GameCanReady, game) != 0;
        else enabled = true;
    }
    O(Op::SetEnabled, sO.mStartGameButton, enabled);
}
static void StagingJoinedGame()
{
    intptr_t game = O(Op::MyGame);
    if (game == 0 || !O(Op::GameMatchesSpec, game, sO.mRoomSpecFlags)) return;
    sO.mInGame = true;
    if (O(Op::GameIsHost, game)) {
        O(Op::ButtonSetText, sO.mLeaveGameButton, (intptr_t)S::DissolveGame);
        O(Op::ButtonSetText, sO.mStartGameButton, (intptr_t)S::StartGame);
    } else {
        O(Op::ButtonSetText, sO.mLeaveGameButton, (intptr_t)S::LeaveGame);
        sO.mLastReady = O(Op::IAmReady) != 0;
        O(Op::ButtonSetText, sO.mStartGameButton, (intptr_t)(sO.mLastReady ? S::NotReady : S::Ready));
    }
    StagingUpdateStartButton();
}
static void StagingEvent(intptr_t inEvent, Ev inType)
{
    switch (inType) {
    case Ev::JoinedGame: StagingJoinedGame(); break;
    case Ev::LeftGame: sO.mInGame = false; break;
    case Ev::SetRoomSpecFlags: sO.mRoomSpecFlags |= (int)O(Op::EventFlags, inEvent); break;
    case Ev::ReadyChanged: StagingUpdateStartButton(); break;
    default: break;
    }
    O(Op::BaseEvent, inEvent); // Component::HandleComponentEvent(inEvent)
}

// LobbyOptionsCtrl
static int GetConnectionSpeed(int inSelection)
{
    if (inSelection < 0) return 5;
    switch (inSelection) {
    case 0: return 5;
    case 1: return 15;
    case 2: return 25;
    case 3: return 35;
    case 4: return 45;
    default: return 55;
    }
}
static int GetConnectionSpeedIndex(int inSpeed)
{
    int i;
    for (i = 1; i < 6; i++)
        if (GetConnectionSpeed(i) > inSpeed) break;
    return i - 1;
}
static void OptionsInit(intptr_t config)
{
    intptr_t button = 0; // SmartPtr<Button> button
    SafeGetComponent(config, sO.mChatSoundEffectsCheck, 13);
    SafeGetComponent(config, sO.mConnectionSpeedCombo, 14);
    SafeGetComponent(config, sO.mCrossPromotionCheck, 15);
    SafeGetComponent(config, sO.mFirewalledIcon, 16);
    SafeGetComponent(config, sO.mLobbyMusicCheck, 17);
    SafeGetComponent(config, sO.mLobbySoundEffectsCheck, 18);
    SafeGetComponent(config, sO.mLocalAddressDetailLabel, 19);
    SafeGetComponent(config, sO.mPublicAddressDetailLabel, 20);
    SafeGetComponent(config, button, 21);
    SafeGetComponent(config, button, 22);
    SafeGetComponent(config, sO.mShowConfirmationsCheck, 23);
    SafeGetComponent(config, sO.mShowGamesInProgressCheck, 24);
    SafeGetComponent(config, sO.mStagingSoundEffectsCheck, 25);
    SafeGetComponent(config, sO.mTabSelectCombo, 26);
    if (sO.mIsLan) {
        SafeGetComponent(config, sO.mLANBroadcastDetailLabel, 27);
        SafeGetComponent(config, button, 28);
        SafeGetComponent(config, sO.mUserNameCombo, 29);
    } else {
        SafeGetComponent(config, sO.mAnonymousToFriendsCheck, 30);
        SafeGetComponent(config, sO.mAutoPingGamesCheck, 31);
        SafeGetComponent(config, sO.mDoAutoAFKCheck, 32);
        SafeGetComponent(config, sO.mFilterDirtyWordsCheck, 33);
    }
    O(Op::LabelSetLocalAddress, sO.mLocalAddressDetailLabel);
    O(Op::SubscribeBroadcast, 1);
    ReleasePtr(button);
}
static void OptionsRefreshLanOptions()
{
    if (!sO.mIsLan) return;
    O(Op::ComboClear, sO.mUserNameCombo, 1);
    size_t i = 0;
    while ((intptr_t)i < O(Op::LanNameCount)) {
        O(Op::ComboAddLanName, sO.mUserNameCombo, (intptr_t)i);
        ++i;
    }
    intptr_t combo = sO.mUserNameCombo;
    O(Op::ComboShowLanName, combo);
    intptr_t port = O(Op::LanBroadcastPort);
    O(Op::LabelSetNumber, sO.mLANBroadcastDetailLabel, port);
}
static void SetCheckFrom(intptr_t inCheck, P inPref, bool inNotify)
{
    intptr_t v = O(Op::PrefGet, (intptr_t)inPref);
    O(Op::ButtonSetCheck, inCheck, v, inNotify);
}
static void OptionsRefreshOptions()
{
    if (sO.mIsLan) OptionsRefreshLanOptions();
    else {
        SetCheckFrom(sO.mFilterDirtyWordsCheck, P::FilterDirtyWords, false);
        SetCheckFrom(sO.mDoAutoAFKCheck, P::DoAutoAfk, false);
        SetCheckFrom(sO.mAnonymousToFriendsCheck, P::AnonymousToFriends, true);
        SetCheckFrom(sO.mAutoPingGamesCheck, P::AutoPingGames, true);
    }
    SetCheckFrom(sO.mChatSoundEffectsCheck, P::ChatSoundEffects, true);
    O(Op::ComboSetSel, sO.mConnectionSpeedCombo, GetConnectionSpeedIndex((int)O(Op::PrefGet, (intptr_t)P::ConnectionSpeed)), 0);
    O(Op::ComboShowTabSelect, sO.mTabSelectCombo);
    SetCheckFrom(sO.mLobbyMusicCheck, P::LobbyMusic, true);
    SetCheckFrom(sO.mLobbySoundEffectsCheck, P::LobbySoundEffects, true);
    SetCheckFrom(sO.mShowConfirmationsCheck, P::ShowConfirmations, true);
    SetCheckFrom(sO.mCrossPromotionCheck, P::CrossPromotion, true);
    SetCheckFrom(sO.mShowGamesInProgressCheck, P::ShowGamesInProgress, true);
    SetCheckFrom(sO.mStagingSoundEffectsCheck, P::StagingSoundEffects, true);
}
static void OptionsSyncOptions()
{
    if (!O(Op::OnLoginScreen) && (O(Op::OnLanScreen) != 0) == sO.mIsLan) OptionsRefreshOptions();
}
static bool OptionsHandleButton(intptr_t inComponent)
{
    if (inComponent == 0) return false;
    intptr_t button = O(Op::ButtonOf, inComponent);
    if (button == 0) return false;
    intptr_t checked = O(Op::ButtonIsChecked, button);
    switch (O(Op::ControlId, inComponent)) {
    case 3: O(Op::PrefSet, (intptr_t)P::CrossPromotion, checked); break;
    case 4: O(Op::PrefSet, (intptr_t)P::DoAutoAfk, checked); return true;
    case 1: O(Op::PrefSet, (intptr_t)P::AutoPingGames, checked); return true;
    case 11:
        O(Op::PrefSet, (intptr_t)P::ShowGamesInProgress, checked);
        O(Op::BroadcastLobbyEvent, 100059, 1);
        return true;
    case 9:
        O(Op::RestoreDefaults);
        O(Op::ResetNetworkAdapter);
        OptionsRefreshOptions();
        return true;
    case 8: {
        O(Op::ResetFilters);
        intptr_t screen = O(Op::CurrentScreen);
        if (screen != 0) O(Op::BroadcastDown, screen, 100034);
        return true;
    }
    case 5:
        O(Op::PrefSet, (intptr_t)P::LobbyMusic, checked);
        if (checked) O(Op::MusicResume);
        else O(Op::MusicStop);
        return true;
    case 6:
        O(Op::PrefSet, (intptr_t)P::LobbySoundEffects, checked);
        O(Op::ClickSoundMute, !checked);
        if (!checked) O(Op::LogonSoundStop);
        return true;
    case 2: O(Op::PrefSet, (intptr_t)P::ChatSoundEffects, checked); return true;
    case 12: O(Op::PrefSet, (intptr_t)P::StagingSoundEffects, checked); return true;
    case 10: O(Op::PrefSet, (intptr_t)P::ShowConfirmations, checked); return true;
    case 7: O(Op::RemoveLanName); OptionsRefreshLanOptions(); return true;
    case 0x30d55: O(Op::PrefSet, (intptr_t)P::FilterDirtyWords, checked); break;
    case 0x30d56: O(Op::PrefSet, (intptr_t)P::AnonymousToFriends, checked); break;
    }
    return false;
}
static void OptionsRefreshPublicAddress()
{
    if (O(Op::FirewallTestDone)) {
        if (O(Op::BehindFirewall)) {
            O(Op::IconSetImage, sO.mFirewalledIcon, (intptr_t)I::Firewall);
            O(Op::Invalidate, sO.mFirewalledIcon);
            O(Op::SetVisible, sO.mFirewalledIcon, 1);
        } else O(Op::SetVisible, sO.mFirewalledIcon, 0);
    } else {
        O(Op::IconSetImage, sO.mFirewalledIcon, (intptr_t)I::Searching);
        O(Op::Invalidate, sO.mFirewalledIcon);
        O(Op::SetVisible, sO.mFirewalledIcon, 1);
    }
    if (!O(Op::PublicAddressValid)) { // IPAddr address=LobbyGetPublicAddress(); if(!address.IsValid())
        if (!O(Op::FirewallTestDone))
            O(Op::LabelSetText, sO.mPublicAddressDetailLabel, (intptr_t)S::CheckingAddress);
        else O(Op::LabelSetText, sO.mPublicAddressDetailLabel, (intptr_t)S::AddressUnavailable);
    } else {
        O(Op::LabelSetPublicAddress, sO.mPublicAddressDetailLabel);
    }
}
static void OptionsEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::ComboSelChanged:
        if (inComponent == sO.mTabSelectCombo) {
            O(Op::SaveTabSelect, sO.mTabSelectCombo); return;
        } else if (inComponent == sO.mConnectionSpeedCombo) {
            O(Op::PrefSet, (intptr_t)P::ConnectionSpeed,
              GetConnectionSpeed((int)O(Op::ComboGetSel, sO.mConnectionSpeedCombo))); return;
        } else if (inComponent == sO.mUserNameCombo) {
            O(Op::SaveLanName, sO.mUserNameCombo); return;
        }
        break;
    case Ev::ButtonPressed:
        if (OptionsHandleButton(inComponent)) return;
        break;
    case Ev::SyncOptions: OptionsSyncOptions(); return;
    case Ev::LanOptions: OptionsRefreshLanOptions(); return;
    case Ev::PublicAddress: OptionsRefreshPublicAddress(); return;
    default: break;
    }
    O(Op::BaseEvent, inEvent);
}

// GameUserListCtrl (and GameUserListItem::UpdateIcon)
static void ItemUpdateIcon(intptr_t inItem)
{
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsIgnored)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Ignored); return; }
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsBlocked)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Blocked); return; }
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsMuted)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Muted); return; }
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsAway)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Away); return; }
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsAdmin)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Admin); return; }
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsModerator)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Moderator); return; }
    if (O(Op::ItemClientIs, inItem, (intptr_t)T::IsCaptain)) { O(Op::ItemSetImage, inItem, (intptr_t)I::Captain); return; }
    O(Op::ItemSetImage, inItem, (intptr_t)I::Normal);
}
static intptr_t ListGetSelectedClient()
{
    intptr_t item = O(Op::ListGetSel, sO.mUserList);
    return item == 0 ? 0 : O(Op::ItemClient, item);
}
static intptr_t ListGetListItem(intptr_t inClient)
{
    for (int i = 0; i < (int)O(Op::ListNumItems, sO.mUserList); ++i) {
        intptr_t item = O(Op::ListItem, sO.mUserList, i);
        intptr_t client = O(Op::ItemClient, item);
        if (client != 0 && O(Op::ClientId, client) == O(Op::ClientId, inClient)) return item;
    }
    return 0;
}
static void ListHandleKickPlayer(bool inBan)
{
    intptr_t client = ListGetSelectedClient();
    if (client != 0) O(Op::KickClient, client, inBan);
}
static void ListUpdateKickButton()
{
    if (!sO.mInGame || sO.mKickButton == 0) return;
    intptr_t game = O(Op::MyGame);
    if (game == 0) return;
    if (O(Op::GameIsHost, game)) {
        intptr_t client = ListGetSelectedClient();
        if (client != 0 && O(Op::ClientId, client) != O(Op::GameCaptainId, game)) O(Op::SetEnabled, sO.mKickButton, 1);
        else O(Op::SetEnabled, sO.mKickButton, 0);
    } else O(Op::SetEnabled, sO.mKickButton, 0);
}
static void ListHandleClientChanged(intptr_t inEvent)
{
    if (!sO.mInGame) return;
    int spec = (int)O(Op::EventRoomSpec, inEvent);
    if ((sO.mRoomSpecFlags & spec) != spec) return;
    if (!O(Op::EventIsModify, inEvent)) return;
    intptr_t item = ListGetListItem(O(Op::ClientChangedClient, inEvent));
    if (item == 0) return;
    ItemUpdateIcon(item);
    O(Op::ListRefresh, sO.mUserList, item);
    ListUpdateKickButton();
}
static void ListHandlePlayerChanged(intptr_t inEvent)
{
    if (!sO.mInGame) return;
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t item = O(Op::ItemNew, O(Op::PlayerChangedClient, inEvent)); // SmartPtr<GameUserListItem> item=new ...
        ItemUpdateIcon(item);
        O(Op::ListInsert, sO.mUserList, item);
        ReleasePtr(item);
    } else if (O(Op::EventIsDelete, inEvent)) {
        intptr_t item = ListGetListItem(O(Op::PlayerChangedClient, inEvent));
        if (item == 0) return;
        O(Op::ListRemove, sO.mUserList, item);
    } else {
        intptr_t item = ListGetListItem(O(Op::PlayerChangedClient, inEvent));
        if (item == 0) return;
        ItemUpdateIcon(item);
        O(Op::ListRefresh, sO.mUserList, item);
    }
}
static void ListHandleLeftGame()
{
    if (sO.mInGame) {
        sO.mInGame = false;
        O(Op::ListClear, sO.mUserList, 1);
    }
}
static void ListInit(intptr_t config)
{
    SafeGetComponent(config, sO.mUserList, 34);
    Assign(sO.mKickButton, O(Op::FindOptional, config, 35)); // dynamic_cast<Button*>(config->Fn386860("KickButton"))
    if (sO.mKickButton != 0) O(Op::SetEnabled, sO.mKickButton, 0);
    O(Op::SubscribeBroadcast, 1);
}
static void ListHandleJoinedGame()
{
    intptr_t game = O(Op::MyGame);
    if (game == 0 || !O(Op::GameMatchesSpec, game, sO.mRoomSpecFlags)) return;
    intptr_t clients = O(Op::GameClients, game);
    if (clients == 0) return;
    if (sO.mKickButton != 0) {
        if (sO.mRoomSpecFlags & 4) O(Op::ButtonSetText, sO.mKickButton, (intptr_t)S::Ban);
        else O(Op::ButtonSetText, sO.mKickButton, (intptr_t)S::Kick);
    }
    O(Op::ListBeginUpdate, sO.mUserList);
    O(Op::ListClear, sO.mUserList, 1);
    for (size_t i = 0; (intptr_t)i < O(Op::ClientsCount, clients); ++i) {
        intptr_t client = O(Op::ClientsAt, clients, (intptr_t)i);
        if (!O(Op::ClientHasPlayer, client))
            continue;
        intptr_t item = O(Op::ItemNew, client);
        ItemUpdateIcon(item);
        O(Op::ListInsert, sO.mUserList, item);
        ReleasePtr(item);
    }
    O(Op::ListEndUpdate, sO.mUserList);
    sO.mInGame = true;
    ListUpdateKickButton();
}
static void ListEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::ListSelChanged: ListUpdateKickButton(); return;
    case Ev::ButtonPressed:
        if (inComponent == sO.mKickButton) {
            ListHandleKickPlayer((sO.mRoomSpecFlags & 4) != 0);
            return;
        }
        break;
    case Ev::ListItemClicked: {
        intptr_t client = ListGetSelectedClient();
        if (client == 0) return;
        if (!O(Op::EventIsSelectedItem, inEvent)) return;
        intptr_t button = O(Op::EventButton, inEvent);
        if (button == 2)
            O(Op::DeliverClientAction, 18, sO.mRoomSpecFlags, client);
        else if (button == 0 && O(Op::EventDoubleClick, inEvent))
            O(Op::BroadcastWhisper, client, sO.mRoomSpecFlags);
        return;
    }
    case Ev::ClientChanged: ListHandleClientChanged(inEvent); return;
    case Ev::JoinedGame: ListHandleJoinedGame(); return;
    case Ev::LeftGame: ListHandleLeftGame(); return;
    case Ev::PlayerChanged: ListHandlePlayerChanged(inEvent); return;
    case Ev::SetRoomSpecFlags: sO.mRoomSpecFlags |= (int)O(Op::EventFlags, inEvent); return;
    default: break;
    }
    O(Op::BaseEvent, inEvent);
}

// EELobbyGameOptionsCtrl (sOptionTable's types: 0 combo box, 1 check box)
static const int kOptionType[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1};
static intptr_t GoGetControl(int inOption) { return inOption >= 0 && inOption < 16 ? sO.mOptions[inOption] : 0; }
static intptr_t GoGetComboBox(int inOption)
{
    if (inOption >= 0 && inOption < 16 && kOptionType[inOption] == 0) return sO.mOptions[inOption];
    return 0;
}
static intptr_t GoGetCheckBox(int inOption)
{
    if (inOption >= 0 && inOption < 16 && kOptionType[inOption] == 1) return sO.mOptions[inOption];
    return 0;
}
static intptr_t GoGetGame() { return O(Op::MyGame); }
static void GoInit(intptr_t theConfig)
{
    SafeGetComponent(theConfig, sO.mCampaignInfoLabel, 36);
    SafeGetComponent(theConfig, sO.mCampaignInfoBox1, 37);
    SafeGetComponent(theConfig, sO.mCampaignInfoBox2, 38);
    int i;
    for (i = 0; i < 16; i++) {
        SafeGetComponent(theConfig, sO.mOptions[i], 39 + i);
        bool theTypeOK = true;
        switch (kOptionType[i]) {
        case 1: theTypeOK = O(Op::IsButton, sO.mOptions[i]) != 0; break;
        case 0: theTypeOK = O(Op::IsCombo, sO.mOptions[i]) != 0; break;
        }
        if (!theTypeOK)
            O(Op::ReportInvalidType, 39 + i); // ComponentConfigMissing("Invalid type for "+name, __FILE__, 89)
        O(Op::SetControlId, sO.mOptions[i], i + 1);
    }
    intptr_t theCombo = GoGetComboBox(9);
    if (theCombo != 0) {
        for (int theLimit = 50; theLimit <= 1200; theLimit += 50)
            O(Op::ComboAddNumber, theCombo, theLimit);
    }
    O(Op::FillMapTypes, GoGetComboBox(2));
    O(Op::SubscribeBroadcast, 1);
    O(Op::SetVisible, GoGetCheckBox(15), 0);
}
static void GoUpdateOption(int inOption, intptr_t inGame)
{
    if (inGame == 0 || inOption < 0 || inOption > 16)
        return;
    if (inOption != 9) {
        // sOptionTable[16] was past the table; its getters answer NULL for 16 whatever it held
        switch (inOption < 16 ? kOptionType[inOption] : -1) {
        case 1: {
            intptr_t theCheck = GoGetCheckBox(inOption);
            if (theCheck != 0)
                O(Op::ButtonSetCheck, theCheck, (long)EG(GC::GetOption, inOption) != 0, 0);
            break;
        }
        case 0: {
            intptr_t theCombo = GoGetComboBox(inOption);
            if (theCombo != 0)
                O(Op::ComboSetSel, theCombo, (int)(long)EG(GC::GetOption, inOption), 0);
            break;
        }
        }
    } else {
        intptr_t theCombo = GoGetComboBox(9);
        if (theCombo != 0)
            O(Op::ComboSetSel, theCombo, (int)((uint32_t)(long)EG(GC::GetOption, 9) / 50 - 1), 0);
    }
}
static void GoUpdateAll()
{
    intptr_t theGame = GoGetGame();
    if (theGame == 0)
        return;
    int i;
    for (i = 0; i < 16; i++)
        GoUpdateOption(i, theGame);
    bool theNotRandomMap = (long)EG(GC::GetOption, 0) != 0;
    bool theFlag = O(Op::GameIsHost, theGame) != 0;
    for (int j = 0; j < 16; j++) {
        intptr_t theControl = GoGetControl(j);
        bool theEnabled;
        if (j == 0)
            theEnabled = theFlag;
        else if (theFlag && !theNotRandomMap)
            theEnabled = true;
        else
            theEnabled = false;
        if (theControl != 0)
            O(Op::SetEnabled, theControl, theEnabled);
    }
    if (EG(GC::HasGameFile) != 0) {
        O(Op::SetVisible, sO.mCampaignInfoBox1, 0);
        O(Op::SetVisible, sO.mCampaignInfoBox2, 1);
        // SetText(theGame->GetGameFileName()): the transfer's name
        O(Op::LabelSetName, sO.mCampaignInfoLabel, EG(GC::GetGameFileName));
    } else {
        O(Op::SetVisible, sO.mCampaignInfoBox1, 1);
        O(Op::SetVisible, sO.mCampaignInfoBox2, 0);
    }
}
static void GoActivate()
{
    intptr_t theGame = GoGetGame();
    if (theGame == 0 || !O(Op::GameMatchesSpec, theGame, sO.mRoomSpecFlags))
        return;
    sO.mInGame = true;
    GoUpdateAll();
    bool theFlag = O(Op::GameIsHost, theGame) != 0;
    for (int i = 0; i < 16; i++)
        O(Op::SetEnabled, sO.mOptions[i], theFlag);
}
static void GoChangeGameType()
{
    intptr_t theCombo = GoGetComboBox(0);
    if (theCombo == 0 || !O(Op::ComboHasSel, theCombo))
        return;
    intptr_t theGame = GoGetGame();
    if (theGame == 0)
        return;
    long theOldType = (long)EG(GC::GetOption, 0);
    if (O(Op::ComboGetSel, theCombo) == 1) {
        int theResult = (int)O(Op::ChooseSavedGame);
        theGame = GoGetGame();
        if (theGame != 0) {
            if (theResult == 100001) {
                EG(GC::SetOption, 0, (intptr_t)(uint32_t)1);
                EG(GC::SendGameState);
            } else
                O(Op::ComboSetSel, theCombo, (int)theOldType, 0);
        }
    } else {
        O(Op::ComboSetSel, theCombo, 0, 0);
        EG(GC::SetOption, 0, 0);
        for (int i = 0; i < 8; i++) {
            // theGame->GetPlayerSlot(i) (&mSlots[i]); Option(0); SetOpen()
            if ((unsigned char)EG(GC::SlotRawOption, i, 0) == 4)
                EG(GC::SlotSetOpen, i);
        }
        EG(GC::SendGameState);
    }
}
static void GoOptionChanged(int inOption)
{
    if (inOption < 0 || inOption >= 16)
        return;
    if (!O(Op::IAmCaptain)) {
        GoUpdateOption(inOption, GoGetGame()); // UpdateOption(int)
    } else {
        intptr_t theGame = GoGetGame();
        if (theGame == 0)
            return;
        if (inOption != 0) {
            if (inOption != 9) {
                switch (kOptionType[inOption]) {
                case 1: {
                    intptr_t theCheck = GoGetCheckBox(inOption);
                    if (theCheck != 0)
                        EG(GC::ChangeOption, inOption, (intptr_t)(uint32_t)(unsigned long)O(Op::ButtonIsChecked, theCheck), 1);
                    break;
                }
                case 0: {
                    intptr_t theCombo = GoGetComboBox(inOption);
                    if (theCombo != 0 && O(Op::ComboHasSel, theCombo))
                        EG(GC::ChangeOption, inOption, (intptr_t)(uint32_t)(unsigned long)(int)O(Op::ComboGetSel, theCombo), 1);
                    break;
                }
                }
            } else {
                intptr_t theCombo = GoGetComboBox(9);
                if (theCombo != 0 && O(Op::ComboHasSel, theCombo))
                    EG(GC::ChangeOption, 9, (intptr_t)(uint32_t)(unsigned long)(((int)O(Op::ComboGetSel, theCombo) + 1) * 50), 1);
            }
        } else
            GoChangeGameType();
    }
}
static void GoEvent(intptr_t inEvent, Ev inType)
{
    switch (inType) {
    case Ev::AllOptions:
        if (sO.mInGame) GoUpdateAll();
        break;
    case Ev::SyncOption:
        if (sO.mInGame) {
            int theOption = (int)O(Op::EventParam, inEvent);
            GoUpdateOption(theOption, GoGetGame());
        }
        break;
    case Ev::SetRoomSpecFlags: sO.mRoomSpecFlags |= (int)O(Op::EventFlags, inEvent); break;
    case Ev::LeftGame: if (sO.mInGame) sO.mInGame = false; break;
    case Ev::JoinedGame: GoActivate(); break;
    case Ev::ComboSelChanged:
    case Ev::ButtonPressed:
        if (sO.mInGame) GoOptionChanged((int)O(Op::EventControlId, inEvent) - 1);
        break;
    default: break;
    }
}

// EELobbyPlayerOptionsCtrl (the original EELobbyPlayerOptionsCtrl.cpp; its mIsActive is mInGame, mRoomSpec mRoomSpecFlags)
static void PlayerInit(intptr_t config)
{
    for (int i = 0; i < 8; i++) {
        SafeGetComponent(config, sO.mPlayerNameCombo[i], 55 + i * 6);
        SafeGetComponent(config, sO.mTeamCombo[i], 56 + i * 6);
        SafeGetComponent(config, sO.mColorButton[i], 57 + i * 6);
        SafeGetComponent(config, sO.mCitizenCombo[i], 58 + i * 6);
        SafeGetComponent(config, sO.mObserveCheck[i], 59 + i * 6);
        SafeGetComponent(config, sO.mFileTransferLabel[i], 60 + i * 6);
    }
    O(Op::SubscribeBroadcast, 1);
}
// the slot view's Option(k): (unsigned char) of the game's raw option
static intptr_t SlotOpt(int inSlot, int inOption) { return (unsigned char)EG(GC::SlotRawOption, inSlot, inOption); }
static void PlayerUpdate(int inSlot, intptr_t inGame)
{
    if (inGame == 0 || inSlot < 0 || inSlot >= 8) return;
    int theSlot = inSlot; // inGame->GetPlayerSlot(inSlot): &mSlots[inSlot], never NULL here
    intptr_t theTeam = sO.mTeamCombo[inSlot];
    O(Op::ComboSetSel, theTeam, SlotOpt(theSlot, 1));
    intptr_t theCitizen = sO.mCitizenCombo[inSlot];
    O(Op::ComboSetSel, theCitizen, SlotOpt(theSlot, 3) - 1);
    bool theTaken = false;
    bool theShowTeam = false;
    bool theNotRandomMap = (long)EG(GC::GetOption, 0) != 0;
    bool theIsMe = false;
    if (inSlot == sO.mMySlot && !O(Op::MyPlayerReady)) theIsMe = true;
    if (SlotOpt(theSlot, 0) == 1) theShowTeam = true;
    intptr_t* theName;
    if (SlotOpt(theSlot, 0) != 3 && SlotOpt(theSlot, 0) != 4) {
        theName = &sO.mPlayerNameCombo[inSlot];
        O(Op::ComboSetSel, *theName, SlotOpt(theSlot, 0));
        O(Op::ComboSetTextColor, *theName, 0x8000000e);
        O(Op::SetVisible, sO.mObserveCheck[inSlot], 0);
        O(Op::SetVisible, sO.mColorButton[inSlot], 0);
        O(Op::SetVisible, sO.mFileTransferLabel[inSlot], 0);
    } else {
        theShowTeam = SlotOpt(theSlot, 4) == 0;
        theName = &sO.mPlayerNameCombo[inSlot];
        theTaken = true;
        O(Op::ComboSetName, *theName, EG(GC::SlotGetName, theSlot)); // SetText(theSlot->GetName())
        if (SlotOpt(theSlot, 0) == 4) O(Op::ComboSetTextColor, *theName, 0xff0000);
        else O(Op::ComboSetTextColor, *theName, 0x8000000e);
        O(Op::SetVisible, sO.mObserveCheck[inSlot], 1);
        O(Op::SetEnabled, sO.mObserveCheck[inSlot], theIsMe && !theNotRandomMap);
        O(Op::ButtonSetCheck, sO.mObserveCheck[inSlot], SlotOpt(theSlot, 4) != 0, 0);
        O(Op::SetVisible, sO.mColorButton[inSlot], SlotOpt(theSlot, 4) == 0);
        O(Op::SetEnabled, sO.mColorButton[inSlot], theIsMe && !theNotRandomMap);
        O(Op::ColorSetColor, sO.mColorButton[inSlot], SlotOpt(theSlot, 2));
        if (SlotOpt(theSlot, 5) >= 100)
            O(Op::SetVisible, sO.mFileTransferLabel[inSlot], 0);
        else {
            O(Op::SetVisible, sO.mFileTransferLabel[inSlot], 1);
            O(Op::LabelSetPercent, sO.mFileTransferLabel[inSlot], SlotOpt(theSlot, 5));
        }
    }
    O(Op::SetEnabled, *theName, sO.mIsHost && inSlot != sO.mMySlot && !theNotRandomMap);
    O(Op::SetEnabled, theCitizen, sO.mIsHost && !theNotRandomMap);
    O(Op::SetEnabled, theTeam, (theIsMe || (sO.mIsHost && !theTaken)) && !theNotRandomMap);
    O(Op::SetVisible, theCitizen, theShowTeam);
    O(Op::SetVisible, theTeam, theShowTeam);
}
static void PlayerUpdateMine(int inSlot) { PlayerUpdate(inSlot, O(Op::MyGame)); }
static void PlayerUpdateAll()
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame != 0)
        for (int i = 0; i < 8; i++) PlayerUpdate(i, theGame);
}
static void PlayerActivate()
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame == 0 || !O(Op::GameMatchesSpec, theGame, sO.mRoomSpecFlags)) return;
    sO.mInGame = true;
    sO.mIsHost = O(Op::IAmCaptain) != 0;
    intptr_t thePlayer = O(Op::MyPlayer);
    if (thePlayer != 0) sO.mMySlot = (unsigned char)O(Op::PlayerSlot, thePlayer);
    PlayerUpdateAll();
}
static void PlayerDeactivate()
{
    if (sO.mInGame) {
        sO.mMySlot = -1;
        sO.mInGame = false;
        sO.mIsHost = false;
    }
}
static void PlayerOptionChanged(int inSlot, int inWhat)
{
    intptr_t theGame = O(Op::MyGame);
    if (theGame == 0 || inSlot < 0 || inSlot >= 8 || inWhat < 0 || inWhat >= 6) return;
    unsigned char theValue = 0;
    switch (inWhat) {
    case 4: theValue = (unsigned char)O(Op::ButtonIsChecked, sO.mObserveCheck[inSlot]); break;
    case 3: theValue = (unsigned char)(O(Op::ComboGetSel, sO.mCitizenCombo[inSlot]) + 1); break;
    case 2: theValue = (unsigned char)O(Op::ColorGetColor, sO.mColorButton[inSlot]); break;
    case 1: theValue = (unsigned char)O(Op::ComboGetSel, sO.mTeamCombo[inSlot]); break;
    case 0: theValue = (unsigned char)O(Op::ComboGetSel, sO.mPlayerNameCombo[inSlot]); break;
    }
    EG(GC::ChangePlayerOption, inSlot, inWhat, theValue);
}
static void PlayerChanged(intptr_t inEvent)
{
    if (O(Op::EventIsModify, inEvent)) {
        intptr_t aClient = O(Op::PlayerChangedClient, inEvent);
        if (O(Op::ClientPlayer, aClient) != 0 && (int)(unsigned char)O(Op::PlayerSlot, O(Op::ClientPlayer, aClient)) == sO.mMySlot)
            PlayerUpdateMine(sO.mMySlot);
    }
}
static void PlayerEvent(intptr_t inEvent, Ev inType)
{
    switch (inType) {
    case Ev::LeftGame: PlayerDeactivate(); break;
    case Ev::JoinedGame: PlayerActivate(); break;
    case Ev::PlayerChanged:
        PlayerChanged(inEvent);
        // falls through (the change kind read as the slot)
    case Ev::SyncPlayer:
        if (sO.mInGame) PlayerUpdateMine((int)O(Op::EventParam, inEvent));
        break;
    case Ev::AllOptions:
        if (sO.mInGame) PlayerUpdateAll();
        break;
    case Ev::ButtonPressed: case Ev::ComboSelChanged: case Ev::ColorChanged:
        if (sO.mInGame) {
            int theId = (int)O(Op::EventControlId, inEvent) - 1;
            PlayerOptionChanged(theId / 6, theId % 6);
        }
        break;
    case Ev::SetRoomSpecFlags: sO.mRoomSpecFlags |= (int)O(Op::EventFlags, inEvent); break;
    default: break;
    }
}

static intptr_t OrigMember(int inIndex)
{
    switch (sO.mKind) {
    case Kind::Browser: {
        const intptr_t m[] = {sO.mBrowser, sO.mBackButton, sO.mForwardButton, sO.mStopButton, sO.mHomeButton,
                              sO.mRefreshButton, sO.mFindInput, sO.mFindNextButton, sO.mFindPrevButton,
                              sO.mCaseSensitiveCheck, sO.mStatusLabel};
        return inIndex < 11 ? m[inIndex] : 0;
    }
    case Kind::Staging: return inIndex == 0 ? sO.mLeaveGameButton : inIndex == 1 ? sO.mStartGameButton : 0;
    case Kind::UserList: return inIndex == 0 ? sO.mUserList : inIndex == 1 ? sO.mKickButton : 0;
    case Kind::GameOptions:
        if (inIndex == 0) return sO.mCampaignInfoLabel;
        if (inIndex == 1) return sO.mCampaignInfoBox1;
        if (inIndex == 2) return sO.mCampaignInfoBox2;
        return inIndex < 19 ? sO.mOptions[inIndex - 3] : 0;
    case Kind::PlayerOptions: {
        intptr_t* m[] = {sO.mPlayerNameCombo, sO.mTeamCombo, sO.mColorButton, sO.mCitizenCombo, sO.mObserveCheck,
                         sO.mFileTransferLabel};
        return m[inIndex / 8][inIndex % 8];
    }
    default: {
        const intptr_t m[] = {sO.mTabSelectCombo, sO.mConnectionSpeedCombo, sO.mCrossPromotionCheck,
                              sO.mLobbySoundEffectsCheck, sO.mChatSoundEffectsCheck, sO.mStagingSoundEffectsCheck,
                              sO.mLobbyMusicCheck, sO.mShowConfirmationsCheck, sO.mShowGamesInProgressCheck,
                              sO.mAnonymousToFriendsCheck, sO.mAutoPingGamesCheck, sO.mLocalAddressDetailLabel,
                              sO.mPublicAddressDetailLabel, sO.mFirewalledIcon, sO.mUserNameCombo,
                              sO.mLANBroadcastDetailLabel, sO.mFilterDirtyWordsCheck, sO.mDoAutoAFKCheck};
        return inIndex < 18 ? m[inIndex] : 0;
    }
    }
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Init:
        switch (sO.mKind) {
        case Kind::Browser: BrowserInit(a); break;
        case Kind::Staging: StagingInit(a); break;
        case Kind::UserList: ListInit(a); break;
        case Kind::GameOptions: GoInit(a); break;
        case Kind::PlayerOptions: PlayerInit(a); break;
        default: OptionsInit(a); break;
        }
        return 0;
    case Call::Event:
        switch (sO.mKind) {
        case Kind::Browser: BrowserEvent(a, (Ev)b); break;
        case Kind::Staging: StagingEvent(a, (Ev)b); break;
        case Kind::UserList: ListEvent(a, (Ev)b, c); break;
        case Kind::GameOptions: GoEvent(a, (Ev)b); break;
        case Kind::PlayerOptions: PlayerEvent(a, (Ev)b); break;
        default: OptionsEvent(a, (Ev)b, c); break;
        }
        return 0;
    case Call::Paint: BrowserPaint(a); return 0;
    case Call::ResetBrowser: O(Op::BrowserReset, sO.mBrowser); sO.mIsReset = true; return 0;
    case Call::Field:
        if (a >= 0 && a < 48) return OrigMember((int)a);
        switch (a) {
        case 100: return sO.mRoomSpecFlags;
        case 101: return sO.mInGame;
        case 102: return sO.mLastReady;
        case 103: return sO.mIsReset;
        case 104: return sO.mMySlot;
        case 105: return sO.mIsHost;
        }
        return 0;
    }
    return 0;
}

static void OrigFree()
{
    // the SmartPtr members destroyed in reverse order
    switch (sO.mKind) {
    case Kind::Browser:
        ReleasePtr(sO.mStatusLabel); ReleasePtr(sO.mCaseSensitiveCheck); ReleasePtr(sO.mFindPrevButton);
        ReleasePtr(sO.mFindNextButton); ReleasePtr(sO.mFindInput); ReleasePtr(sO.mRefreshButton);
        ReleasePtr(sO.mHomeButton); ReleasePtr(sO.mStopButton); ReleasePtr(sO.mForwardButton);
        ReleasePtr(sO.mBackButton); ReleasePtr(sO.mBrowser);
        break;
    case Kind::Staging: ReleasePtr(sO.mStartGameButton); ReleasePtr(sO.mLeaveGameButton); break;
    case Kind::UserList: ReleasePtr(sO.mKickButton); ReleasePtr(sO.mUserList); break;
    case Kind::PlayerOptions:
        for (int i = 7; i >= 0; i--) ReleasePtr(sO.mFileTransferLabel[i]);
        for (int i = 7; i >= 0; i--) ReleasePtr(sO.mObserveCheck[i]);
        for (int i = 7; i >= 0; i--) ReleasePtr(sO.mCitizenCombo[i]);
        for (int i = 7; i >= 0; i--) ReleasePtr(sO.mColorButton[i]);
        for (int i = 7; i >= 0; i--) ReleasePtr(sO.mTeamCombo[i]);
        for (int i = 7; i >= 0; i--) ReleasePtr(sO.mPlayerNameCombo[i]);
        break;
    case Kind::GameOptions:
        for (int i = 15; i >= 0; i--) ReleasePtr(sO.mOptions[i]);
        ReleasePtr(sO.mCampaignInfoBox2); ReleasePtr(sO.mCampaignInfoBox1); ReleasePtr(sO.mCampaignInfoLabel);
        break;
    default:
        ReleasePtr(sO.mDoAutoAFKCheck); ReleasePtr(sO.mFilterDirtyWordsCheck); ReleasePtr(sO.mLANBroadcastDetailLabel);
        ReleasePtr(sO.mUserNameCombo); ReleasePtr(sO.mFirewalledIcon); ReleasePtr(sO.mPublicAddressDetailLabel);
        ReleasePtr(sO.mLocalAddressDetailLabel); ReleasePtr(sO.mAutoPingGamesCheck);
        ReleasePtr(sO.mAnonymousToFriendsCheck); ReleasePtr(sO.mShowGamesInProgressCheck);
        ReleasePtr(sO.mShowConfirmationsCheck); ReleasePtr(sO.mLobbyMusicCheck); ReleasePtr(sO.mStagingSoundEffectsCheck);
        ReleasePtr(sO.mChatSoundEffectsCheck); ReleasePtr(sO.mLobbySoundEffectsCheck); ReleasePtr(sO.mCrossPromotionCheck);
        ReleasePtr(sO.mConnectionSpeedCombo); ReleasePtr(sO.mTabSelectCombo);
        break;
    }
}

// ---- the Rust side and the sequences

static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }

struct Step { Call mCall; intptr_t a, b, c; };
// the game's set-up: an option's value ("gset", SetOption: below its limit) or a slot's raw option ("gslot")
struct Setup { int mSlot, mOption; intptr_t mValue; };
static intptr_t GameHostFn(void* p, EeWonEeGameOp o, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return GameHost(p, o, a, b, c, d);
}

static std::vector<std::string> RunSide(bool inRust, Kind inKind, unsigned inSeed, const std::vector<Step>& inSteps,
                                        const std::vector<Setup>& inSetup)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sDepth = 0;
    sGame = ee_won_eegame_new((void*)0x6000, GameHostFn);
    for (size_t i = 0; i < inSetup.size(); i++) {
        const Setup& u = inSetup[i];
        if (u.mSlot < 0) {
            Line("gset %lld %lld", u.mOption, u.mValue);
            ee_won_eegame_call(sGame, GC::SetOption, u.mOption, u.mValue, 0, 0);
        } else {
            Line("gslot %lld %lld %lld", u.mSlot, u.mOption, u.mValue);
            ee_won_eegame_call(sGame, GC::SlotSetRawOption, u.mSlot, u.mOption, u.mValue, 0);
        }
    }
    if (inRust) sRustCtl = ee_won_lobbyctl_new((void*)0x9090, inKind, RustHost);
    else {
        memset(&sO, 0, sizeof(sO));
        sO.mKind = inKind;
        // the constructors
        sO.mIsReset = inKind == Kind::Browser;
        sO.mRoomSpecFlags = (inKind == Kind::Staging || inKind == Kind::UserList || inKind == Kind::GameOptions ||
                             inKind == Kind::PlayerOptions) ? 2 : 0;
        sO.mMySlot = -1;
        sO.mIsLan = inKind == Kind::LanOptions;
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
    Line("gfree");
    ee_won_eegame_free(sGame);
    sGame = NULL;
    return sTrace;
}

static int SlotsOf(Kind inKind, int* outSlots)
{
    int n = 0;
    switch (inKind) {
    case Kind::Browser: for (int i = 0; i <= 10; i++) outSlots[n++] = i; break;
    case Kind::Staging: outSlots[n++] = 11; outSlots[n++] = 12; break;
    case Kind::LanOptions: for (int i = 13; i <= 29; i++) outSlots[n++] = i; break;
    case Kind::NetOptions: for (int i = 13; i <= 26; i++) outSlots[n++] = i; for (int i = 30; i <= 33; i++) outSlots[n++] = i; break;
    case Kind::UserList: outSlots[n++] = 34; break;
    case Kind::GameOptions: for (int i = 36; i <= 54; i++) outSlots[n++] = i; break;
    case Kind::PlayerOptions: for (int i = 55; i <= 102; i++) outSlots[n++] = i; break;
    }
    return n;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the tables against the original SafeGetComponent arguments
    // (round 13's kinds' look-ups follow these; won_srvlist_test and won_netctl_test check them)
    CHECK(ee_won_lobbyctl_slot_count() >= sizeof(kOrigSlots) / sizeof(kOrigSlots[0]));
    for (unsigned i = 0; i < sizeof(kOrigSlots) / sizeof(kOrigSlots[0]); i++) {
        const EeWonLobbyCtlSlot* s = ee_won_lobbyctl_slot(i);
        CHECK(strcmp(s->name, kOrigSlots[i].mName) == 0);
        CHECK(strcmp(s->file, kOrigFiles[OrigFileOf((int)i)]) == 0);
        CHECK(s->line == kOrigSlots[i].mLine);
        CHECK(s->ty == kOrigSlots[i].mType);
        CHECK(s->control_id == kOrigSlots[i].mId);
    }
    CHECK(ee_won_lobbyctl_slot(ee_won_lobbyctl_slot_count()) == NULL);
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 4711;
    const int kSequences = 70000;
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        Kind aKind = (Kind)R(7);
        int aSlots[64];
        int aSlotCount = SlotsOf(aKind, aSlots);
        std::vector<Step> aSteps;
        if (R(6)) aSteps.push_back(Step{Call::Init, 0xC0, 0, 0});
        // the options panel acts only once a game room is joined: half of its sequences join first
        if ((aKind == Kind::GameOptions || aKind == Kind::PlayerOptions) && R(2))
            aSteps.push_back(Step{Call::Event, kEvent, (intptr_t)Ev::JoinedGame, 0});
        int aCount = 1 + (int)R(10);
        for (int i = 0; i < aCount; i++) {
            Step s = {Call::Event, kEvent, (intptr_t)R(20), 0};
            switch (R(10)) {
            case 0: s.mCall = Call::Init; s.a = 0xC0; s.b = 0; break;
            case 1:
                if (aKind == Kind::Browser) { s.mCall = R(2) ? Call::Paint : Call::ResetBrowser; s.a = 0x6600; s.b = 0; }
                break;
            case 2: s.mCall = Call::Field; s.a = R(3) ? (intptr_t)R(48) : 100 + (intptr_t)R(6); s.b = 0; break;
            default: break;
            }
            if (s.mCall == Call::Event) {
                unsigned w = R(6);
                if (w == 0) s.c = 0;
                else if (w == 1) s.c = kOther;
                else if (w == 2 && aKind == Kind::UserList) s.c = kKick;
                else s.c = kCtl + aSlots[R((unsigned)aSlotCount)] * 0x10;
            }
            aSteps.push_back(s);
        }
        // the game's options and its slots' raw options (only the game room's kinds read them)
        std::vector<Setup> aSetup;
        if (aKind == Kind::GameOptions || aKind == Kind::PlayerOptions) {
            for (int o = 0; o < 16; o++)
                if (R(2)) aSetup.push_back(Setup{-1, o, R(4) == 0 ? (intptr_t)R(1300) : (intptr_t)R(3)});
            static const unsigned kSlotRange[6] = {6, 6, 17, 6, 2, 120};
            for (int i = 0; i < 8; i++)
                for (int o = 0; o < 6; o++)
                    if (R(2)) aSetup.push_back(Setup{i, o, (intptr_t)(o == 5 && R(3) ? 100 : R(kSlotRange[o]))});
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aKind, aSeed, aSteps, aSetup);
        std::vector<std::string> b = RunSide(true, aKind, aSeed, aSteps, aSetup);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d (kind %d) differs\n", n, (int)aKind);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (strcmp(x, y) != 0) { printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y); break; }
                }
            }
            continue;
        }
        bool aNew = n < 80;
        for (size_t i = 0; i < a.size(); i++) {
            std::string aKey = a[i].substr(0, a[i].find(' ', a[i].find(' ') + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d kind %d\n", n, (int)aKind);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_lobbyctl_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
