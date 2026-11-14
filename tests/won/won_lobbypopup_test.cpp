// The lobby's options popups as one Rust object (R6 WONLobby round 3): the original ClientOptionsPopup.cpp,
// GameOptionsPopup.cpp and FriendOptionsPopup.cpp (the statements copied from the C++ before the port, every call on
// the menu, the popup, its subject, a font, the lobby's config or an event replaced by the host op that makes it:
// LobbyDialogHost.cpp's LobbyPopupHost runs the same calls) against ee_won's lobbypopup.rs over one scripted host.
//
// Each of 30,000 sequences makes a popup of a random kind, its constructor's body, then 1 to 6 random calls (Init
// with every room flag combination, NULL subjects, the same subject again, a moderator, a captain, oneself; clicks
// on an entry or none, with either button, on an action 0 or not; other events), and frees it. The host answers each
// predicate from a seeded generator (the same stream for both sides). The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or call none before made are
// rust/crates/ee_won/src/golden_lobbypopup.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/ee_won.h"

typedef EeWonLobbyPopupOp Op;
typedef EeWonLobbyPopupCall Call;
typedef EeWonLobbyPopupKind Kind;
typedef EeWonLobbyPopupString S;

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

static const intptr_t kMenu = 0x4400, kFont = 0x4500, kBold = 0x4600, kConfig = 0x4700, kItem = 0x4800;

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::MenuNew: r = kMenu; break;
    case Op::MenuFont: r = R(4) ? kFont : 0; break;
    case Op::PopupFontMod: case Op::MenuFontMod: r = kBold; break;
    case Op::Width: case Op::Height: r = 10 * (intptr_t)R(30); break;
    case Op::ClientId: case Op::ClientGameGroupId: r = R(3); break;
    case Op::ClientWonUserId: r = R(100000); break;
    case Op::GameType: r = R(5); break;
    case Op::Config: r = R(3) ? kConfig : 0; break;
    case Op::EventItem: r = R(4) ? kItem : 0; break;
    case Op::EventButton: r = R(3) == 0; break;
    case Op::ItemMenuId: r = R(4) ? 1 + (intptr_t)R(17) : 0; break;
    case Op::ClientIsModerator: case Op::ClientIsCaptain: case Op::ClientIsInvited: case Op::ClientIsFriend:
    case Op::ClientIsAway: case Op::ClientIsIgnored: case Op::ClientIsBlocked: case Op::ClientIsMuted:
    case Op::GameCanJoin: case Op::GameInProgress: case Op::GameShowDetails: case Op::ConfigNoPingInProgress:
        r = R(2); break;
    default: break;
    }
    Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    return r;
}

// ---- the original classes, every call on another object replaced by its op

struct Orig {
    Kind mKind;
    intptr_t mPopupMenu;
    intptr_t mSubject; // mClient, mGame or mFriend
    int mRoomSpecFlags;
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
static void ReleaseLocal(intptr_t inPtr) { if (inPtr != 0) O(Op::Release, inPtr); }
static void AddItem(S inString, int inAction) { O(Op::MenuAddItem, sO.mPopupMenu, (intptr_t)inString, inAction); }

// the constructors: this->mPopupMenu=new PopupMenu; this->AddChild(this->mPopupMenu.get())
static void OrigConstruct()
{
    Assign(sO.mPopupMenu, O(Op::MenuNew));
    O(Op::AddChild, sO.mPopupMenu);
}

// ClientOptionsPopup::Init
static void OrigClientInit(intptr_t inClient, intptr_t inMyClient, int inRoomSpec)
{
    if (inClient == 0 || inMyClient == 0) return;
    Assign(sO.mSubject, inClient);
    sO.mRoomSpecFlags = inRoomSpec;
    O(Op::MenuClear, sO.mPopupMenu);
    bool isLAN = (inRoomSpec & 2) != 0;
    bool isGame = (inRoomSpec & 4) != 0;
    bool isModerator = O(Op::ClientIsModerator, inMyClient) != 0;
    bool isCaptain = O(Op::ClientIsCaptain, inMyClient, isLAN) != 0;
    intptr_t aMine = O(Op::ClientId, inMyClient);
    bool isSelf = aMine == O(Op::ClientId, inClient);
    intptr_t anOldFont = O(Op::MenuFont, sO.mPopupMenu);     // SmartPtr<Font> anOldFont=this->mPopupMenu->mFont
    intptr_t aBoldFont = O(Op::PopupFontMod, anOldFont);     // SmartPtr<Font> aBoldFont=this->GetFontMod(old,1,0)
    O(Op::MenuSetFont, sO.mPopupMenu, aBoldFont);
    O(Op::MenuTitleClient, sO.mPopupMenu, inClient);         // AddItem(inClient->GetName(),0)->mSelectable=false
    if (isModerator) {
        intptr_t anId = O(Op::ClientWonUserId, inClient);
        O(Op::MenuTitleNumber, sO.mPopupMenu, anId);
    }
    O(Op::MenuAssignFont, sO.mPopupMenu, anOldFont);         // this->mPopupMenu->mFont=anOldFont
    AddItem(S::Whisper, 16);
    if (!isSelf && O(Op::ClientIsCaptain, inMyClient, 1) &&
        O(Op::ClientGameGroupId, inClient) != O(Op::ClientGameGroupId, inMyClient)) {
        if (!O(Op::ClientIsInvited, sO.mSubject)) AddItem(S::Invite, 6);
        else AddItem(S::Uninvite, 13);
    }
    if (isGame) {
        if (!isSelf) AddItem(S::Locate, 17);
        if (!O(Op::ClientIsFriend, inClient)) AddItem(S::AddToFriends, 1);
        else AddItem(S::RemoveFromFriends, 9);
        O(Op::MenuAddSeparator, sO.mPopupMenu);
    }
    if (isSelf && isGame) {
        if (O(Op::ClientIsAway, inClient)) AddItem(S::ClearAfk, 4);
        else AddItem(S::SetAfk, 10);
    } else {
        if (!O(Op::ClientIsIgnored, inClient)) AddItem(S::Ignore, 5);
        else AddItem(S::Unignore, 12);
        if (isGame) {
            if (!O(Op::ClientIsBlocked, inClient)) AddItem(S::Block, 3);
            else AddItem(S::Unblock, 11);
        }
    }
    if (isGame) {
        if (isModerator || isCaptain) {
            O(Op::MenuAddSeparator, sO.mPopupMenu);
            if (isModerator) AddItem(S::Warn, 15);
            if (O(Op::ClientIsMuted, inClient, isLAN)) AddItem(S::UnMute, 14);
            else AddItem(S::Mute, 8);
            if (!isSelf) {
                AddItem(S::Kick, 7);
                AddItem(S::Ban, 2);
            }
        }
    } else if (isCaptain && !isSelf) {
        O(Op::MenuAddSeparator, sO.mPopupMenu);
        AddItem(S::Kick, 7);
    }
    intptr_t w = O(Op::Width, sO.mPopupMenu);
    intptr_t h = O(Op::Height, sO.mPopupMenu);
    O(Op::SetSize, w, h);
    ReleaseLocal(aBoldFont);
    ReleaseLocal(anOldFont);
}

// GameOptionsPopup::Init
static void OrigGameInit(intptr_t inGame)
{
    O(Op::MenuClear, sO.mPopupMenu);
    Assign(sO.mSubject, inGame);
    intptr_t anOldFont = O(Op::MenuFont, sO.mPopupMenu);
    intptr_t aBoldFont = O(Op::MenuFontMod, sO.mPopupMenu, anOldFont); // this->mPopupMenu->GetFontMod(old,1,0)
    O(Op::MenuSetFont, sO.mPopupMenu, aBoldFont);
    O(Op::MenuTitleGame, sO.mPopupMenu, inGame);
    O(Op::MenuSetFont, sO.mPopupMenu, anOldFont);
    O(Op::MenuAddSeparator, sO.mPopupMenu);
    if (O(Op::GameCanJoin, inGame)) AddItem(S::GameJoin, 1);
    bool allowPing = true;
    if (O(Op::GameInProgress, inGame)) {
        intptr_t aConfig = O(Op::Config);
        if (aConfig != 0) allowPing = !O(Op::ConfigNoPingInProgress, aConfig);
    }
    if (allowPing) {
        if (O(Op::GameType, inGame) != 1) AddItem(S::GamePing, 2);
        if (allowPing) AddItem(S::GameQueryDetails, 3);
    }
    if (O(Op::GameShowDetails, inGame)) AddItem(S::GameHideDetails, 5);
    else AddItem(S::GameShowDetails, 4);
    if (O(Op::GameType, inGame) == 3) {
        O(Op::MenuAddSeparator, sO.mPopupMenu);
        AddItem(S::GameRemove, 6);
    }
    intptr_t w = O(Op::Width, sO.mPopupMenu);
    intptr_t h = O(Op::Height, sO.mPopupMenu);
    O(Op::SetSize, w, h);
    ReleaseLocal(aBoldFont);
    ReleaseLocal(anOldFont);
}

// FriendOptionsPopup::Init
static void OrigFriendInit(intptr_t inFriend, int inRoomSpec)
{
    if (inFriend == 0) return;
    Assign(sO.mSubject, inFriend);
    sO.mRoomSpecFlags = inRoomSpec;
    O(Op::MenuClear, sO.mPopupMenu);
    intptr_t anOldFont = O(Op::MenuFont, sO.mPopupMenu);
    intptr_t aBoldFont = O(Op::PopupFontMod, anOldFont);
    O(Op::MenuSetFont, sO.mPopupMenu, aBoldFont);
    O(Op::MenuTitleFriend, sO.mPopupMenu, inFriend);
    O(Op::MenuSetFont, sO.mPopupMenu, anOldFont);
    O(Op::MenuAddSeparator, sO.mPopupMenu);
    AddItem(S::Locate, 1);
    AddItem(S::FriendRefreshList, 2);
    O(Op::MenuAddSeparator, sO.mPopupMenu);
    AddItem(S::FriendRemove, 3);
    intptr_t w = O(Op::Width, sO.mPopupMenu);
    intptr_t h = O(Op::Height, sO.mPopupMenu);
    O(Op::SetSize, w, h);
    ReleaseLocal(aBoldFont);
    ReleaseLocal(anOldFont);
}

// the three HandleComponentEvents
static void OrigEvent(intptr_t inEvent, int inType)
{
    if (inType != (int)EeWonLobbyPopupEvent::ItemUpClicked) {
        O(Op::BaseEvent, inEvent); // Container::HandleComponentEvent(inEvent)
        return;
    }
    if (O(Op::EventItem, inEvent) == 0 || O(Op::EventButton, inEvent) != 0) return;
    intptr_t anAction = O(Op::ItemMenuId, kItem);
    if (anAction != 0) {
        if (sO.mKind == Kind::Client) O(Op::DeliverClient, anAction, sO.mSubject, sO.mRoomSpecFlags);
        else if (sO.mKind == Kind::Game) O(Op::DeliverGame, anAction, sO.mSubject);
        else O(Op::DeliverFriend, anAction, sO.mSubject, sO.mRoomSpecFlags);
    }
    O(Op::ClosePopup);
}

static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Construct: OrigConstruct(); return 0;
    case Call::Init:
        if (sO.mKind == Kind::Client) OrigClientInit(a, b, (int)c);
        else if (sO.mKind == Kind::Game) OrigGameInit(a);
        else OrigFriendInit(a, (int)b);
        return 0;
    case Call::Event: OrigEvent(a, (int)b); return 0;
    case Call::Field:
        switch (a) {
        case 0: return sO.mPopupMenu;
        case 1: return sO.mSubject;
        case 2: return sO.mRoomSpecFlags;
        }
        return 0;
    }
    return 0;
}

static void OrigFree()
{
    // the SmartPtr members destroyed in reverse order (GameOptionsPopup: mGame at 0x84, mPopupMenu at 0x88)
    if (sO.mKind == Kind::Game) { ReleaseLocal(sO.mPopupMenu); ReleaseLocal(sO.mSubject); }
    else { ReleaseLocal(sO.mSubject); ReleaseLocal(sO.mPopupMenu); }
}

// ---- the Rust side and the sequences

static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }

struct Step { Call mCall; intptr_t a, b, c; };

static std::vector<std::string> RunSide(bool inRust, Kind inKind, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    EeWonLobbyPopup* aRust = NULL;
    if (inRust) aRust = ee_won_lobbypopup_new((void*)0x9090, inKind, RustHost);
    else { memset(&sO, 0, sizeof(sO)); sO.mKind = inKind; }
    for (size_t i = 0; i < inSteps.size(); i++) {
        const Step& s = inSteps[i];
        Line("call %lld %lld %lld %lld", (long long)s.mCall, s.a, s.b, s.c);
        intptr_t r = inRust ? ee_won_lobbypopup_call(aRust, s.mCall, s.a, s.b, s.c) : OrigCall(s.mCall, s.a, s.b, s.c);
        Line("ret %lld", r);
    }
    Line("free");
    if (inRust) ee_won_lobbypopup_free(aRust);
    else OrigFree();
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 1618;
    static const intptr_t kSubjects[] = {0, 0x5100, 0x5200};
    for (int n = 0; n < 30000; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        Kind aKind = (Kind)R(3);
        std::vector<Step> aSteps;
        aSteps.push_back(Step{Call::Construct, 0, 0, 0});
        int aCount = 1 + (int)R(6);
        for (int i = 0; i < aCount; i++) {
            Step s = {Call::Init, kSubjects[R(4) ? 1 + R(2) : 0], 0, 0};
            switch (R(6)) {
            case 0: case 1: case 2:
                if (aKind == Kind::Client) { s.b = R(6) ? 0x5300 : 0; s.c = (intptr_t)R(8); }
                else if (aKind == Kind::Friend) s.b = (intptr_t)R(8);
                break;
            case 3: case 4: s.mCall = Call::Event; s.a = 0xE1; s.b = R(4) ? 1 : 0; s.c = 0; break;
            default: s.mCall = Call::Field; s.a = R(4); s.b = 0; break;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 101u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aKind, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aKind, aSeed, aSteps);
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
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            std::string aKey = a[i].substr(0, a[i].find(' ', a[i].find(' ') + 1));
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d kind %d\n", n, (int)aKind);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of 30000 sequences differ\n", aDiffer); }
    if (!aPrint)
        printf("won_lobbypopup_test: 30000 sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
