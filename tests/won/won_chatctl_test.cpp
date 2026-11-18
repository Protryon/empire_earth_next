// ChatCtrl as lobbyctl.rs kind Chat (R6 WONLobby round 13): the original ChatCtrl.cpp (the statements copied from the
// C++ before the port: the handlers, the link chain, the duration's split, the commands the panel runs itself) with
// every call on a control, an event, a link, a text, the chat parser or a lobby global replaced by the host op that
// makes it (LobbyNetCtrlHost.cpp and the model's Rust objects run the same calls), against ee_won_lobby's chatctl.rs
// over one scripted host.
//
// Each sequence makes a chat panel, sets its room kind and colours (the config reader), then makes 1 to 12 random
// calls (InitComponent; every event the panel handles, on its Send button, its input, no component or another one;
// the members read back) and frees it. The host answers each op from a seeded generator (the same stream for both
// sides). The traces must be the same.
//
// The first 60 sequences and every later one that makes an op or event none before made are
// rust/crates/ee_won_lobby/src/golden_chatctl.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/won_lobby/chatctl.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/resource.h"

typedef EeWonLobbyCtlOp Op;
typedef EeWonLobbyCtlCall Call;
typedef EeWonLobbyCtlEvent Ev;
typedef EeWonLobbyCtlEvField EF;
typedef EeWonLobbyCtlLinkField LF;
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

static const intptr_t kCtl = 0x1000, kEvent = 0xE1, kOther = 0x77, kClient = 0x3200, kFriend = 0x3300,
                      kServer = 0x3400, kParser = 0x3500, kLink = 0x3600, kText = 0x7100, kAddr = 0x7800,
                      kClients = 0x3700;
static const int kSlotChat = 108;

static intptr_t EvAnswer(EF f)
{
    switch (f) {
    case EF::AclHandled: case EF::AfHandled: case EF::SciHandled: return R(4) == 0;
    case EF::AclRoomId: case EF::AfLocation: case EF::RcRoomSpec: case EF::JrRoomSpec: case EF::LrRoomSpec:
    case EF::AlKind: case EF::SciRoomSpec: case EF::GiKind: case EF::GjaKind: case EF::CmRoomSpec:
        return R(3) ? (intptr_t)(1u << R(5)) & 0x13 : (intptr_t)R(32);
    case EF::AclClient: case EF::PtcClient: case EF::RcSender: case EF::RcTarget: case EF::GiClient:
    case EF::GjaClient: case EF::CmClient:
        return R(5) ? kClient + R(3) * 0x10 : 0;
    case EF::AfFriend: return R(6) ? kFriend + R(2) * 0x10 : 0;
    case EF::TlaLink: return R(5) ? kLink + R(4) : 0;
    case EF::AclRoom: case EF::AclServer: case EF::RcText: case EF::JrGroupName: case EF::CceText: case EF::AlText:
    case EF::SciText: case EF::GiGameName: case EF::GiComment: case EF::GjaComment: case EF::CmRoomName:
    case EF::CmComment:
        return kText + R(4);
    case EF::JrClientId: return R(4) ? R(5) : 0xffff;
    case EF::AlValue: case EF::CmKind: return R(5);
    case EF::AlSeverity: case EF::CmDuration: {
        static const unsigned kSeconds[] = {0, 1, 59, 60, 61, 119, 120, 121, 3539, 3540, 3541, 3599, 3600, 3601,
                                            7199, 7200, 82799, 82800, 82801, 86399, 86400, 86401, 172799, 172800,
                                            400000, 4000000000u};
        return (intptr_t)kSeconds[R(sizeof(kSeconds) / sizeof(kSeconds[0]))];
    }
    case EF::SciCommand: return R(25);
    case EF::GiGameId: return R(70000);
    default: return R(2);
    }
}

static bool sRust;
static EeWonLobbyCtl* sRustCtl;
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c);

static intptr_t Host(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t r = 0;
    switch (inOp) {
    case Op::Find: r = R(12) ? kCtl + b * 0x10 : 0; break;
    case Op::EvGet: r = EvAnswer((EF)b); break;
    case Op::MGet:
        switch ((MF)b) {
        case MF::ClientId: r = R(4); break;
        case MF::ClientName: r = kText + 8 + R(3); break;
        case MF::FriendAddress: r = kAddr; break;
        case MF::FriendLocation: r = kText + 12 + R(2); break;
        case MF::FriendFlags: r = R(8); break;
        default: r = R(70000); break;
        }
        break;
    case Op::MCall:
        if ((MC)b == MC::ListCount) r = R(5);
        else if ((MC)b == MC::ListAt) r = kClient + c * 0x10;
        else r = R(2);
        break;
    case Op::LinkGet:
        switch ((LF)b) {
        case LF::Kind: r = (intptr_t)R(7); break;
        case LF::Next: case LF::Previous: r = R(3) ? kLink + R(4) : 0; break;
        default: r = kClient + R(3) * 0x10; break;
        }
        break;
    case Op::ChatLinkNew: case Op::TextNew: case Op::ParserCommandText: case Op::FormatTarget: case Op::ParserHelp:
    case Op::ParserText: case Op::IgnoredAt: case Op::TextAppend: case Op::TextAppendBytes:
        r = sNextHeld++; break;
    case Op::ResText: r = R(9) ? 0x9000 + a : 0; break;
    case Op::TextLen: r = R(3); break;
    case Op::TextAt: r = R(2) ? '/' : 'h'; break;
    case Op::TextFind: r = (intptr_t)R(3) - 1; break;
    case Op::InputText: r = kText + 20 + R(3); break;
    case Op::TextAreaFont: r = 0x4400; break;
    case Op::TextAreaLinkFont: r = 0x4410; break;
    case Op::FontMod: r = R(5) ? 0x4500 + c : 0; break;
    case Op::CurrentServer: r = R(4) ? kServer : 0; break;
    case Op::MyClient: r = R(4) ? kClient : 0; break;
    case Op::ChatParser: case Op::RoomChatParser: r = R(6) ? kParser : 0; break;
    case Op::ParserNumber: r = a == kParser && b == 0 ? '/' : (b == 1 ? (intptr_t)R(9) : (intptr_t)R(23)); break;
    case Op::ParserClients: r = R(4) ? kClients : 0; break;
    case Op::IgnoredCount: r = R(4); break;
    case Op::IsRootVisible: case Op::IsDisabled: case Op::ParserHandleInput: case Op::EventIsAdd:
    case Op::EventIsDelete: case Op::AddrValid:
        r = R(5) != 0; break;
    case Op::EventFlags: r = R(32); break;
    default: break;
    }
    // the structs by their fields, the made texts by their bytes
    if (inOp == Op::ChatLinkNew) {
        const EeWonLobbyCtlLink* l = (const EeWonLobbyCtlLink*)a;
        char aBuffer[200];
        snprintf(aBuffer, sizeof(aBuffer), "op %d link %d %lld %lld %d %lld %lld %d = %lld", (int)inOp, l->kind,
                 (long long)l->obj, (long long)l->text, l->id, (long long)l->addr, (long long)l->font, l->color, (long long)r);
        sTrace.push_back(aBuffer);
    } else if (inOp == Op::ChatAddFormatted) {
        const EeWonLobbyCtlFormat* f = (const EeWonLobbyCtlFormat*)a;
        std::string s = "op " + std::to_string((int)inOp) + " fmt " + std::to_string((long long)f->output) + " " +
                        std::to_string((long long)f->format) + " s";
        for (int i = 0; i < f->num_strings; i++) s += " " + std::to_string((long long)f->strings[i]);
        s += " l";
        for (int i = 0; i < f->num_links; i++) s += " " + std::to_string((long long)f->links[i]);
        s += " c";
        for (int i = 0; i < 9; i++) s += " " + std::to_string(f->colors[i]);
        s += " f";
        for (int i = 0; i < 3; i++) s += " " + std::to_string((long long)f->fonts[i]);
        s += " " + std::to_string(f->line_break) + " = 0";
        sTrace.push_back(s);
    } else if (inOp == Op::TextNew) {
        sTrace.push_back("op " + std::to_string((int)inOp) + " '" + std::string((const char*)a, (size_t)b) + "' = " +
                         std::to_string((long long)r));
    } else if (inOp == Op::TextAppendBytes) {
        sTrace.push_back("op " + std::to_string((int)inOp) + " " + std::to_string((long long)a) + " '" +
                         std::string((const char*)b, (size_t)c) + "' = " + std::to_string((long long)r));
    } else Line("op %lld %lld %lld %lld = %lld", (long long)inOp, a, b, c, r);
    return r;
}
static intptr_t O(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return Host(inOp, a, b, c); }
static intptr_t RustHost(void*, Op inOp, intptr_t a, intptr_t b, intptr_t c) { return Host(inOp, a, b, c); }

// ---- the original class, every call on another object replaced by its op

struct Orig {
    intptr_t mBoldFont, mItalicFont;
    int mDefaultColor, mUserColor, mWhisperColor, mEmoteColor, mWarningColor, mErrorColor, mHelpColor, mGroupColor,
        mMessageColor, mAcceptColor, mRejectColor;
    intptr_t mChatInput, mSendButton, mChatOutput;
    long mRoomSpecFlags;
    unsigned short mClientId;
    intptr_t mRoomName; // a GUIStringData with one reference (0: the empty string)
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
static void Rel(intptr_t p) { if (p != 0) O(Op::Release, p); }
static intptr_t E(intptr_t inEvent, EF f) { return O(Op::EvGet, inEvent, (intptr_t)f); }
static intptr_t S(Res inRes) { return O(Op::ResText, (intptr_t)inRes); }
static intptr_t Get(intptr_t inObj, MF f) { return O(Op::MGet, inObj, (intptr_t)f); }
static bool IsTeamRoom() { return (sO.mRoomSpecFlags & 16) != 0 ? true : false; }

static void AddFormattedText(intptr_t inFormat, const intptr_t* inStrings, int inNumStrings, const intptr_t* inSegments,
                             int inNumSegments)
{
    EeWonLobbyCtlFormat f;
    memset(&f, 0, sizeof(f));
    f.output = sO.mChatOutput;
    f.format = inFormat;
    for (int i = 0; i < inNumStrings; i++) f.strings[i] = inStrings[i];
    f.num_strings = inNumStrings;
    for (int i = 0; i < inNumSegments; i++) f.links[i] = inSegments[i];
    f.num_links = inNumSegments;
    int aColors[9] = {sO.mDefaultColor, sO.mUserColor, sO.mWhisperColor, sO.mEmoteColor, sO.mWarningColor,
                      sO.mErrorColor, sO.mHelpColor, sO.mGroupColor, sO.mMessageColor};
    memcpy(f.colors, aColors, sizeof(aColors));
    f.fonts[0] = O(Op::TextAreaFont, sO.mChatOutput);
    f.fonts[1] = sO.mBoldFont;
    f.fonts[2] = sO.mItalicFont;
    f.line_break = 1;
    O(Op::ChatAddFormatted, (intptr_t)&f);
}
static void AddTextLine(intptr_t inFormat, intptr_t inText) { AddFormattedText(inFormat, &inText, 1, NULL, 0); }
static void AddLinkLine(intptr_t inFormat, intptr_t inSegment) { AddFormattedText(inFormat, NULL, 0, &inSegment, 1); }
static intptr_t NewLink(int inKind, intptr_t inObj, intptr_t inText, int inId, intptr_t inAddr, intptr_t inFont, int inColor)
{
    EeWonLobbyCtlLink l;
    memset(&l, 0, sizeof(l));
    l.kind = inKind; l.obj = inObj; l.text = inText; l.id = inId; l.addr = inAddr; l.font = inFont; l.color = inColor;
    return O(Op::ChatLinkNew, (intptr_t)&l);
}
// the constructors' arguments right to left (MSVC's order): the colour, the font, then the rest
static intptr_t MakeClientLink(intptr_t inClient)
{
    int aColor = sO.mUserColor;
    intptr_t aFont = O(Op::TextAreaFont, sO.mChatOutput);
    return NewLink(3, inClient, 0, 0, 0, aFont, aColor);
}
static intptr_t RoomLink(intptr_t inText, int inId, intptr_t inServer)
{
    int aColor = sO.mGroupColor;
    intptr_t aFont = O(Op::TextAreaLinkFont, sO.mChatOutput);
    return NewLink(0, 0, inText, inId, inServer, aFont, aColor);
}
static intptr_t FriendLink(intptr_t inFriend)
{
    int aColor = sO.mUserColor;
    intptr_t aFont = O(Op::TextAreaFont, sO.mChatOutput);
    return NewLink(4, inFriend, 0, 0, 0, aFont, aColor);
}
static void DisableLinkChain(intptr_t inLink, int inColor, int inLinkedColor)
{
    O(Op::LinkSet, inLink, (intptr_t)LF::Color, inColor);
    O(Op::LinkSet, inLink, (intptr_t)LF::Enabled, 0);
    intptr_t aLink = O(Op::LinkGet, inLink, (intptr_t)LF::Next);
    while (aLink != 0) {
        O(Op::LinkSet, aLink, (intptr_t)LF::Enabled, 0);
        O(Op::LinkSet, aLink, (intptr_t)LF::Color, inLinkedColor);
        aLink = O(Op::LinkGet, aLink, (intptr_t)LF::Next);
    }
    aLink = O(Op::LinkGet, inLink, (intptr_t)LF::Previous);
    while (aLink != 0) {
        O(Op::LinkSet, aLink, (intptr_t)LF::Enabled, 0);
        O(Op::LinkSet, aLink, (intptr_t)LF::Color, inLinkedColor);
        aLink = O(Op::LinkGet, aLink, (intptr_t)LF::Previous);
    }
    O(Op::Invalidate, sO.mChatOutput);
}
static void InitComponent(intptr_t inConfig)
{
    Assign(sO.mChatInput, O(Op::Find, inConfig, kSlotChat));
    Assign(sO.mSendButton, O(Op::Find, inConfig, kSlotChat + 1));
    Assign(sO.mChatOutput, O(Op::Find, inConfig, kSlotChat + 2));
    intptr_t aFont = O(Op::TextAreaFont, sO.mChatOutput);
    Assign(sO.mBoldFont, O(Op::FontMod, 0x9090, aFont, 1));
    aFont = O(Op::TextAreaFont, sO.mChatOutput);
    Assign(sO.mItalicFont, O(Op::FontMod, 0x9090, aFont, 2));
    O(Op::SubscribeBroadcast, 1);
}
static void HandleAnnounceClientLocation(intptr_t inEvent)
{
    if (E(inEvent, EF::AclHandled) || (sO.mRoomSpecFlags & (int)E(inEvent, EF::AclRoomId)) != (int)E(inEvent, EF::AclRoomId)) return;
    intptr_t initialClient = E(inEvent, EF::AclClient);
    if (initialClient == 0 || !O(Op::IsRootVisible)) return;
    O(Op::EvSetHandled, inEvent, (intptr_t)EF::AclHandled);
    intptr_t aServer = O(Op::CurrentServer);
    intptr_t aClientLink = MakeClientLink(E(inEvent, EF::AclClient));
    intptr_t aRoom = E(inEvent, EF::AclRoom);
    intptr_t aRoomLink = RoomLink(aRoom, (unsigned short)Get(initialClient, MF::ClientGameGroupId), aServer);
    intptr_t aChat = E(inEvent, EF::AclServer);
    intptr_t aServerLink = RoomLink(aChat, (unsigned short)Get(initialClient, MF::ClientChatGroupId), aServer);
    intptr_t aLinks[3] = {aClientLink, aRoomLink, aServerLink};
    bool hasRoom = O(Op::TextLen, E(inEvent, EF::AclRoom)) != 0;
    bool hasServer = O(Op::TextLen, E(inEvent, EF::AclServer)) != 0;
    intptr_t aFormat;
    if (O(Op::MCall, initialClient, (intptr_t)MC::ClientAnonymous)) aFormat = S(Res::ChatCtrl_ClientIsAnonymous_String);
    else if (hasRoom && hasServer) aFormat = S(Res::ChatCtrl_ClientInGameAndChat_String);
    else if (hasServer) aFormat = S(Res::ChatCtrl_ClientInChat_String);
    else if (hasRoom) aFormat = S(Res::ChatCtrl_ClientInGame_String);
    else aFormat = S(Res::ChatCtrl_ClientNotOnServer_String);
    AddFormattedText(aFormat, NULL, 0, aLinks, 3);
    Rel(aServerLink);
    Rel(aRoomLink);
    Rel(aClientLink);
}
static void HandleAnnounceFriend(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::AfLocation)) != (int)E(inEvent, EF::AfLocation) || E(inEvent, EF::AfFriend) == 0) return;
    intptr_t aFriend = E(inEvent, EF::AfFriend);
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t aLink = FriendLink(aFriend);
        AddLinkLine(S(Res::ChatCtrl_FriendJoined_String), aLink);
        Rel(aLink);
        return;
    }
    if (O(Op::EventIsDelete, inEvent)) {
        intptr_t aLink = FriendLink(aFriend);
        AddLinkLine(S(Res::ChatCtrl_FriendLeft_String), aLink);
        Rel(aLink);
        return;
    }
    if (E(inEvent, EF::AfHandled) || !O(Op::IsRootVisible)) return;
    O(Op::EvSetHandled, inEvent, (intptr_t)EF::AfHandled);
    intptr_t aAddress = Get(aFriend, MF::FriendAddress);
    intptr_t aLocation = Get(aFriend, MF::FriendLocation);
    O(Op::AddRef, aLocation);
    bool hasLocation = ((unsigned char)Get(aFriend, MF::FriendFlags) & 3) != 0 && O(Op::AddrValid, aAddress) &&
                       O(Op::TextLen, aLocation) != 0;
    intptr_t aFriendLink = FriendLink(aFriend);
    intptr_t aLocationLink = 0;
    if (hasLocation) {
        int aColor = sO.mUserColor;
        intptr_t aFont = O(Op::TextAreaFont, sO.mChatOutput);
        aLocationLink = NewLink(5, aFriend, aLocation, 0, aAddress, aFont, aColor);
    }
    intptr_t aLinks[2] = {aFriendLink, aLocationLink};
    intptr_t aFormat;
    if (!hasLocation) aFormat = S(Res::ChatCtrl_FriendIsNotOnline_String);
    else if (((unsigned char)Get(aFriend, MF::FriendFlags) & 1) != 0) {
        if (((unsigned char)Get(aFriend, MF::FriendFlags) & 2) != 0) aFormat = S(Res::ChatCtrl_FriendIsInGameStaging_String);
        else aFormat = S(Res::ChatCtrl_FriendIsChatting_String);
    } else if (((unsigned char)Get(aFriend, MF::FriendFlags) & 2) != 0) aFormat = S(Res::ChatCtrl_FriendIsPlaying_String);
    else aFormat = S(Res::ChatCtrl_FriendIsOn_String);
    AddFormattedText(aFormat, NULL, 0, aLinks, 2);
    Rel(aLocationLink);
    Rel(aFriendLink);
    O(Op::Release, aLocation);
}
static void HandleTextLinkActivated(intptr_t inEvent)
{
    intptr_t aLink = E(inEvent, EF::TlaLink);
    if (aLink == 0) return;
    switch ((int)O(Op::LinkGet, aLink, (intptr_t)LF::Kind)) {
    case 3: O(Op::DeliverClientAction, 18, sO.mRoomSpecFlags, O(Op::LinkGet, aLink, (intptr_t)LF::Client)); return;
    case 4: O(Op::DeliverFriendAction, 5, O(Op::LinkGet, aLink, (intptr_t)LF::Friend), sO.mRoomSpecFlags); return;
    case 5: O(Op::DeliverJoinFriendsRoom, O(Op::LinkGet, aLink, (intptr_t)LF::Friend)); return;
    case 0: O(Op::DeliverJumpToRoom, aLink); return;
    case 1:
        DisableLinkChain(aLink, sO.mAcceptColor, sO.mRejectColor);
        O(Op::DeliverAcceptClient, O(Op::LinkGet, aLink, (intptr_t)LF::Client), 1);
        return;
    case 2:
        DisableLinkChain(aLink, sO.mAcceptColor, sO.mRejectColor);
        O(Op::DeliverAcceptClient, O(Op::LinkGet, aLink, (intptr_t)LF::Client), 0);
        return;
    default: return;
    }
}
static void HandlePlayerTeamChanged(intptr_t inEvent)
{
    if (!IsTeamRoom()) return;
    // AddLinkLine's arguments right to left (MSVC's order): the link, then the format
    if (O(Op::EventIsAdd, inEvent)) {
        intptr_t aLink = MakeClientLink(E(inEvent, EF::PtcClient));
        AddLinkLine(S(Res::ChatCtrl_TeamMemberJoined_String), aLink);
        Rel(aLink);
    } else if (O(Op::EventIsDelete, inEvent)) {
        intptr_t aLink = MakeClientLink(E(inEvent, EF::PtcClient));
        AddLinkLine(S(Res::ChatCtrl_TeamMemberLeft_String), aLink);
        Rel(aLink);
    }
}
static void HandleRecvChat(intptr_t inEvent)
{
    if (E(inEvent, EF::RcSender) == 0 || sO.mClientId == 0xffff) return;
    if (E(inEvent, EF::RcIsTeam)) {
        if (((sO.mRoomSpecFlags | 16) & (int)E(inEvent, EF::RcRoomSpec)) != (int)E(inEvent, EF::RcRoomSpec)) return;
    } else {
        if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::RcRoomSpec)) != (int)E(inEvent, EF::RcRoomSpec)) return;
        if (IsTeamRoom() && !E(inEvent, EF::RcIsWhisper) && !E(inEvent, EF::RcIsWarning)) return;
    }
    bool crossRoom = E(inEvent, EF::RcIsTeam) && !IsTeamRoom();
    intptr_t aSender = MakeClientLink(E(inEvent, EF::RcSender));
    intptr_t aTarget = 0;
    intptr_t aLinks[2] = {aSender, 0};
    int count = 1;
    if (E(inEvent, EF::RcTarget) != 0) {
        aLinks[1] = (aTarget = MakeClientLink(E(inEvent, EF::RcTarget)));
        count = 2;
    }
    intptr_t aText = E(inEvent, EF::RcText);
    intptr_t aFormat;
    if (E(inEvent, EF::RcIsEmote)) aFormat = crossRoom ? S(Res::ChatCtrl_TeamEmote_String) : S(Res::ChatCtrl_Emote_String);
    else if (E(inEvent, EF::RcIsWhisper) && E(inEvent, EF::RcTarget) != 0) {
        if (Get(E(inEvent, EF::RcSender), MF::ClientId) == sO.mClientId)
            if (Get(E(inEvent, EF::RcTarget), MF::ClientId) == sO.mClientId) aFormat = S(Res::ChatCtrl_YouWhisperToYourself_String);
            else aFormat = S(Res::ChatCtrl_YouWhisper_String);
        else aFormat = S(Res::ChatCtrl_SomeoneWhispersToYou_String);
    } else if (E(inEvent, EF::RcIsWarning) && E(inEvent, EF::RcTarget) != 0) {
        if (Get(E(inEvent, EF::RcSender), MF::ClientId) == sO.mClientId)
            if (Get(E(inEvent, EF::RcTarget), MF::ClientId) == sO.mClientId) aFormat = S(Res::ChatCtrl_YouWarnYourself_String);
            else aFormat = S(Res::ChatCtrl_YouWarnSomeone_String);
        else if (Get(E(inEvent, EF::RcTarget), MF::ClientId) == sO.mClientId) aFormat = S(Res::ChatCtrl_SomeoneWarnsYou_String);
        else aFormat = S(Res::ChatCtrl_SomeoneWarnsSomeone_String);
    } else aFormat = crossRoom ? S(Res::ChatCtrl_TeamChat_String) : S(Res::ChatCtrl_NormalChat_String);
    if (O(Op::TextLen, aFormat) != 0) AddFormattedText(aFormat, &aText, 1, aLinks, count);
    Rel(aTarget);
    Rel(aSender);
}
static void HandleJoinedRoom(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::JrRoomSpec)) != (int)E(inEvent, EF::JrRoomSpec)) return;
    sO.mClientId = (unsigned short)E(inEvent, EF::JrClientId);
    Assign(sO.mRoomName, E(inEvent, EF::JrGroupName));
    O(Op::SetEnabled, sO.mSendButton, 1);
    intptr_t aFormat = S(Res::ChatCtrl_EnteringRoom_String);
    AddTextLine(aFormat, E(inEvent, EF::JrGroupName));
    O(Op::TextAreaLineBreak, sO.mChatOutput, 10);
}
static void HandleLeftRoom(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::LrRoomSpec)) != (int)E(inEvent, EF::LrRoomSpec)) return;
    sO.mClientId = 0xffff;
    O(Op::SetEnabled, sO.mSendButton, 0);
    if (!E(inEvent, EF::LrFlag)) return;
    if (E(inEvent, EF::LrForced)) AddTextLine(S(Res::ChatCtrl_Disconnected_String), sO.mRoomName);
    else AddTextLine(S(Res::ChatCtrl_LeavingRoom_String), sO.mRoomName);
    O(Op::TextAreaLineBreak, sO.mChatOutput, 10);
}
static unsigned char LobbySplitDuration(unsigned long inSeconds, bool inRoundUp, unsigned long* outValue)
{
    if (inSeconds == 0) { *outValue = 0; return 6; }
    if (inRoundUp) {
        if (inSeconds <= 60) { *outValue = 1; return 0; }
        if (inSeconds <= 3540) { *outValue = inSeconds / 60; if (inSeconds % 60 > 0) ++*outValue; return 1; }
        if (inSeconds <= 3600) { *outValue = 1; return 2; }
        if (inSeconds <= 82800) { *outValue = inSeconds / 3600; if (inSeconds % 3600 > 0) ++*outValue; return 3; }
        if (inSeconds <= 86400) { *outValue = 1; return 4; }
        *outValue = inSeconds / 86400;
        if (inSeconds % 86400 > 0) ++*outValue;
        return 5;
    }
    if (inSeconds < 120) { *outValue = 1; return 0; }
    if (inSeconds < 3600) { *outValue = inSeconds / 60; return 1; }
    if (inSeconds < 7200) { *outValue = 1; return 2; }
    if (inSeconds < 86400) { *outValue = inSeconds / 3600; return 3; }
    if (inSeconds < 172800) { *outValue = 1; return 4; }
    *outValue = inSeconds / 86400;
    return 5;
}
static intptr_t Number(unsigned long v)
{
    char text[20];
    snprintf(text, sizeof(text), "%lu", (unsigned long)(uint32_t)v);
    return O(Op::TextNew, (intptr_t)text, (intptr_t)strlen(text));
}
static void HandleAlert(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::AlKind)) != (int)E(inEvent, EF::AlKind)) return;
    intptr_t aFormat;
    intptr_t aText = E(inEvent, EF::AlText);
    switch ((int)E(inEvent, EF::AlValue)) {
    case 0: aFormat = S(Res::ChatCtrl_ServerAlert_String); break;
    case 1: {
        unsigned long value = 0;
        aFormat = O(Op::ResText, LobbySplitDuration((uint32_t)E(inEvent, EF::AlSeverity), false, &value) + 0x86);
        if (value != 0) {
            intptr_t number = Number(value);
            AddFormattedText(aFormat, &number, 1, NULL, 0);
            O(Op::Release, number);
            aFormat = S(Res::ChatCtrl_AlertComment_String);
        }
        break;
    }
    default:
        aFormat = S(Res::ChatCtrl_ServerShutdownAborted_String);
        AddFormattedText(aFormat, NULL, 0, NULL, 0);
        aFormat = S(Res::ChatCtrl_AlertComment_String);
        break;
    }
    if (O(Op::TextLen, aText) != 0) AddFormattedText(aFormat, &aText, 1, NULL, 0);
}
static void HandleSetChatInput(intptr_t inEvent)
{
    if (E(inEvent, EF::SciHandled) || (sO.mRoomSpecFlags & (int)E(inEvent, EF::SciRoomSpec)) != (int)E(inEvent, EF::SciRoomSpec) ||
        !O(Op::IsRootVisible)) return;
    O(Op::EvSetHandled, inEvent, (intptr_t)EF::SciHandled);
    intptr_t parser = O(Op::ChatParser);
    if (parser == 0) return;
    O(Op::RequestFocus, sO.mChatInput);
    intptr_t aCommand = E(inEvent, EF::SciCommand);
    intptr_t input = O(Op::ParserCommandText, parser, aCommand, E(inEvent, EF::SciText));
    O(Op::InputSetText, sO.mChatInput, input, 0);
    O(Op::InputSelectRange, sO.mChatInput, -1, -1);
    if ((int)O(Op::TextFind, O(Op::InputText, sO.mChatInput), input) != -1) { O(Op::Release, input); return; }
    if (O(Op::TextAt, O(Op::InputText, sO.mChatInput), 0) != O(Op::ParserNumber, parser, 0))
        input = O(Op::TextAppend, input, O(Op::InputText, sO.mChatInput));
    O(Op::InputSetText, sO.mChatInput, input, 0);
    O(Op::InputSelectRange, sO.mChatInput, -1, -1);
    O(Op::Release, input);
}
static void HandleGroupInvitation(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::GiKind)) != (int)E(inEvent, EF::GiKind)) return;
    intptr_t aClient = MakeClientLink(E(inEvent, EF::GiClient));
    intptr_t aServer = O(Op::CurrentServer);
    intptr_t aName = E(inEvent, EF::GiGameName);
    intptr_t aRoom = RoomLink(aName, (unsigned short)E(inEvent, EF::GiGameId), aServer);
    intptr_t aLinks[2] = {aClient, aRoom};
    intptr_t aText[2] = {E(inEvent, EF::GiGameName), E(inEvent, EF::GiComment)};
    int id = !E(inEvent, EF::GiFlag) ? (E(inEvent, EF::GiInvite) ? 0x8d : 0x8e) : (E(inEvent, EF::GiInvite) ? 0x8f : 0x90);
    if (O(Op::TextLen, E(inEvent, EF::GiComment)) != 0) id += 4;
    AddFormattedText(O(Op::ResText, id), aText, 2, aLinks, 2);
    Rel(aRoom);
    Rel(aClient);
}
static void HandleGroupJoinAttempt(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::GjaKind)) != (int)E(inEvent, EF::GjaKind) || IsTeamRoom()) return;
    intptr_t aClient = MakeClientLink(E(inEvent, EF::GjaClient));
    int aColor = sO.mMessageColor;
    intptr_t aFont = O(Op::TextAreaLinkFont, sO.mChatOutput);
    intptr_t aT = S(Res::ChatCtrl_Accept_String);
    intptr_t aAccept = NewLink(1, E(inEvent, EF::GjaClient), aT, 0, 0, aFont, aColor);
    aColor = sO.mMessageColor;
    aFont = O(Op::TextAreaLinkFont, sO.mChatOutput);
    aT = S(Res::ChatCtrl_Reject_String);
    intptr_t aDecline = NewLink(2, E(inEvent, EF::GjaClient), aT, 0, 0, aFont, aColor);
    O(Op::LinkSet, aAccept, (intptr_t)LF::Previous, aDecline);
    O(Op::LinkSet, aDecline, (intptr_t)LF::Next, aAccept);
    intptr_t aText = E(inEvent, EF::GjaComment);
    intptr_t aLinks[3] = {aClient, aAccept, aDecline};
    intptr_t aFormat = O(Op::TextLen, E(inEvent, EF::GjaComment)) == 0 ? S(Res::ChatCtrl_SomeoneAsksToJoin_String) : S(Res::ChatCtrl_SomeoneAsksToJoinExpl_String);
    AddFormattedText(aFormat, &aText, 1, aLinks, 3);
    Rel(aDecline);
    Rel(aAccept);
    Rel(aClient);
}
static void ShowParserError(intptr_t inParser)
{
    intptr_t error = 0, held = 0;
    switch ((int)O(Op::ParserNumber, inParser, 1)) {
    case 3: held = error = O(Op::FormatTarget, (intptr_t)Res::RoutingLogic_ClientNotFound_String, inParser); break;
    case 4: held = error = O(Op::FormatTarget, (intptr_t)Res::RoutingLogic_ClientAmbiguous_String, inParser); break;
    case 1: error = S(Res::RoutingLogic_InvalidCommand_String); break;
    case 2: error = S(Res::RoutingLogic_RequireName_String); break;
    case 7: error = S(Res::RoutingLogic_BadTimeSpec_String); break;
    }
    if (O(Op::TextLen, error) == 0) { Rel(held); held = 0; error = S(Res::RoutingLogic_Error_String); }
    O(Op::TextAreaAddSegment, sO.mChatOutput, error, sO.mErrorColor);
    Rel(held);
}
static void HandleClientModerated(intptr_t inEvent)
{
    if ((sO.mRoomSpecFlags & (int)E(inEvent, EF::CmRoomSpec)) != (int)E(inEvent, EF::CmRoomSpec)) return;
    intptr_t format;
    intptr_t client = MakeClientLink(E(inEvent, EF::CmClient));
    intptr_t link = client;
    unsigned long value = 0;
    if (E(inEvent, EF::CmFlag)) {
        switch ((int)E(inEvent, EF::CmKind)) {
        case 0: format = O(Op::ResText, LobbySplitDuration((uint32_t)E(inEvent, EF::CmDuration), true, &value) + 0x64); break;
        case 2: format = O(Op::ResText, LobbySplitDuration((uint32_t)E(inEvent, EF::CmDuration), true, &value) + 0x74); break;
        case 3: format = S(Res::ChatCtrl_YouWereUnmuted_String); break;
        default: format = S(Res::ChatCtrl_YouWereKicked_String); break;
        }
    } else {
        switch ((int)E(inEvent, EF::CmKind)) {
        case 0: format = O(Op::ResText, LobbySplitDuration((uint32_t)E(inEvent, EF::CmDuration), true, &value) + 0x6c); break;
        case 2: format = O(Op::ResText, LobbySplitDuration((uint32_t)E(inEvent, EF::CmDuration), true, &value) + 0x7c); break;
        case 3: format = S(Res::ChatCtrl_SomeoneWasUnmuted_String); break;
        default: format = S(Res::ChatCtrl_SomeoneWasKicked_String); break;
        }
    }
    intptr_t aRoom = E(inEvent, EF::CmRoomName);
    if (value == 0) AddFormattedText(format, &aRoom, 1, &link, 1);
    else {
        intptr_t number = Number(value);
        intptr_t args[2] = {aRoom, number};
        AddFormattedText(format, args, 2, &link, 1);
        O(Op::Release, number);
    }
    intptr_t aComment = E(inEvent, EF::CmComment);
    if (O(Op::TextLen, aComment) != 0) {
        format = S(Res::ChatCtrl_ModeratorComment_String);
        AddFormattedText(format, &aComment, 1, NULL, 0);
    }
    Rel(client);
}
static void SendInput()
{
    if (O(Op::TextLen, O(Op::InputText, sO.mChatInput)) == 0 || O(Op::IsDisabled, sO.mSendButton)) return;
    intptr_t parser = O(Op::RoomChatParser, sO.mRoomSpecFlags);
    if (parser == 0) return;
    intptr_t input = O(Op::InputText, sO.mChatInput);
    O(Op::AddRef, input);
    O(Op::InputAddHistory, sO.mChatInput, input);
    O(Op::InputClear, sO.mChatInput, 1);
    if (!O(Op::ParserHandleInput, parser, input)) {
        ShowParserError(parser);
        O(Op::Release, input);
        return;
    }
    switch ((int)O(Op::ParserNumber, parser, 2)) {
    case 20: {
        intptr_t help = O(Op::ParserHelp, parser, sO.mClientId);
        AddFormattedText(help, NULL, 0, NULL, 0);
        O(Op::Release, help);
        O(Op::Release, input);
        return;
    }
    case 19:
        O(Op::TextAreaReset, sO.mChatOutput, 0);
        O(Op::Release, input);
        return;
    case 18: {
        intptr_t aText = O(Op::ParserText, parser, 1);
        bool aHas = O(Op::TextLen, aText) != 0;
        O(Op::Release, aText);
        if (aHas) break;
        intptr_t command = O(Op::ParserCommandText, parser, 2, 0);
        O(Op::InputSetText, sO.mChatInput, command, 0);
        O(Op::InputSelectRange, sO.mChatInput, -1, -1);
        O(Op::Release, command);
        O(Op::Release, input);
        return;
    }
    case 21: {
        intptr_t aClients = O(Op::ParserClients, parser);
        intptr_t me = O(Op::MyClient);
        if (me != 0) {
            intptr_t names = O(Op::TextNew, (intptr_t)"", 0);
            // LobbyClientIterator over the list: HasMore (the count), Next (the client at the cursor)
            intptr_t i = 0;
            while (aClients != 0 && i < O(Op::MCall, aClients, (intptr_t)MC::ListCount, 0)) {
                intptr_t client = O(Op::MCall, aClients, (intptr_t)MC::ListAt, i++);
                if (O(Op::MCall, me, (intptr_t)MC::ClientIsTeammate, client)) {
                    if (O(Op::TextLen, names) != 0) names = O(Op::TextAppendBytes, names, (intptr_t)", ", 2);
                    names = O(Op::TextAppend, names, Get(client, MF::ClientName));
                }
            }
            AddTextLine(S(Res::ChatCtrl_TeamMembers_String), names);
            O(Op::Release, names);
        }
        O(Op::Release, input);
        return;
    }
    case 5: {
        intptr_t aTarget = O(Op::ParserText, parser, 0);
        bool aHas = O(Op::TextLen, aTarget) != 0;
        O(Op::Release, aTarget);
        if (aHas) break;
        intptr_t names = O(Op::TextNew, (intptr_t)"", 0);
        for (intptr_t i = 0; i < O(Op::IgnoredCount); ++i) {
            if (O(Op::TextLen, names) != 0) names = O(Op::TextAppendBytes, names, (intptr_t)", ", 2);
            intptr_t aName = O(Op::IgnoredAt, i);
            names = O(Op::TextAppend, names, aName);
            O(Op::Release, aName);
        }
        AddTextLine(S(Res::ChatCtrl_IgnoreList_String), names);
        O(Op::Release, names);
        O(Op::Release, input);
        return;
    }
    }
    O(Op::FireEvent, 100012, 1);
    O(Op::Release, input);
}
static void HandleComponentEvent(intptr_t inEvent, Ev inType, intptr_t inComponent)
{
    switch (inType) {
    case Ev::TextLinkActivated: HandleTextLinkActivated(inEvent); return;
    case Ev::ButtonPressed: if (inComponent == sO.mSendButton) { SendInput(); return; } break;
    case Ev::InputReturn: if (inComponent == sO.mChatInput) { SendInput(); return; } break;
    case Ev::Alert: HandleAlert(inEvent); return;
    case Ev::AnnounceFriend: HandleAnnounceFriend(inEvent); return;
    case Ev::AnnounceClientLocation: HandleAnnounceClientLocation(inEvent); return;
    case Ev::ClientModerated: HandleClientModerated(inEvent); return;
    case Ev::ChatCommandError: O(Op::TextAreaAddSegment, sO.mChatOutput, E(inEvent, EF::CceText), sO.mErrorColor); return;
    case Ev::GroupInvitation: HandleGroupInvitation(inEvent); return;
    case Ev::GroupJoinAttempt: HandleGroupJoinAttempt(inEvent); return;
    case Ev::JoinedRoom: HandleJoinedRoom(inEvent); return;
    case Ev::LeftRoom: HandleLeftRoom(inEvent); return;
    case Ev::PlayerTeamChanged: HandlePlayerTeamChanged(inEvent); return;
    case Ev::RecvChat: HandleRecvChat(inEvent); return;
    case Ev::SetChatInput: HandleSetChatInput(inEvent); return;
    case Ev::SetRoomSpecFlags: sO.mRoomSpecFlags |= (int)O(Op::EventFlags, inEvent); return;
    default: break;
    }
    O(Op::BaseEvent, inEvent);
}
// ChatCtrlConfig's stores (GROUPTYPE's room kind, the colours)
static void SetField(intptr_t inField, intptr_t inValue)
{
    int* aColors[11] = {&sO.mDefaultColor, &sO.mUserColor, &sO.mWhisperColor, &sO.mEmoteColor, &sO.mWarningColor,
                        &sO.mErrorColor, &sO.mHelpColor, &sO.mGroupColor, &sO.mMessageColor, &sO.mAcceptColor,
                        &sO.mRejectColor};
    if (inField >= 0 && inField < 11) *aColors[inField] = (int)inValue;
    else if (inField == 20) sO.mRoomSpecFlags = (long)inValue;
}
static intptr_t OrigCall(Call inCall, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inCall) {
    case Call::Init: InitComponent(a); return 0;
    case Call::Event: HandleComponentEvent(a, (Ev)b, c); return 0;
    case Call::SetField: SetField(a, b); return 0;
    case Call::Field:
        switch (a) {
        case 0: return sO.mBoldFont;
        case 1: return sO.mItalicFont;
        case 2: return sO.mChatInput;
        case 3: return sO.mSendButton;
        case 4: return sO.mChatOutput;
        case 100: return sO.mRoomSpecFlags;
        case 120: return sO.mClientId;
        case 121: return sO.mRoomName;
        }
        if (a >= 130 && a < 141) {
            int* aColors[11] = {&sO.mDefaultColor, &sO.mUserColor, &sO.mWhisperColor, &sO.mEmoteColor, &sO.mWarningColor,
                                &sO.mErrorColor, &sO.mHelpColor, &sO.mGroupColor, &sO.mMessageColor, &sO.mAcceptColor,
                                &sO.mRejectColor};
            return *aColors[a - 130];
        }
        return 0;
    default: return 0;
    }
}
static void OrigFree()
{
    ReleasePtr(sO.mRoomName);
    ReleasePtr(sO.mChatOutput);
    ReleasePtr(sO.mSendButton);
    ReleasePtr(sO.mChatInput);
    ReleasePtr(sO.mItalicFont);
    ReleasePtr(sO.mBoldFont);
}

// ---- the sequences

struct Step { Call mCall; intptr_t a, b, c; };

static std::vector<std::string> RunSide(bool inRust, unsigned inSeed, const std::vector<Step>& inSteps)
{
    sTrace.clear();
    sSeed = inSeed;
    sRust = inRust;
    sNextHeld = 0xA000;
    if (inRust) sRustCtl = ee_won_lobbyctl_new((void*)0x9090, EeWonLobbyCtlKind::Chat, RustHost);
    else {
        memset(&sO, 0, sizeof(sO));
        // the constructor's colours, no room kind, no client
        sO.mErrorColor = 0xff0000; sO.mHelpColor = 0x404040; sO.mUserColor = 0x800000; sO.mEmoteColor = 0x800080;
        sO.mWhisperColor = 0x8000; sO.mWarningColor = 0xff8000; sO.mGroupColor = 0x80; sO.mMessageColor = 0xff;
        sO.mRejectColor = 0xa0a0a0; sO.mClientId = 0xffff;
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
    // the look-ups against the original SafeGetComponent arguments
    {
        const char* kNames[] = {"ChatInput", "SendButton", "ChatOutput"};
        const EeWonLobbyCtlType kTypes[] = {EeWonLobbyCtlType::Input, EeWonLobbyCtlType::Button, EeWonLobbyCtlType::TextArea};
        for (int i = 0; i < 3; i++) {
            const EeWonLobbyCtlSlot* s = ee_won_lobbyctl_slot(kSlotChat + i);
            CHECK(s != NULL && strcmp(s->name, kNames[i]) == 0 && strcmp(s->file, "ChatCtrl.cpp") == 0);
            CHECK(s != NULL && s->line == 160 + i && s->ty == kTypes[i] && s->control_id == -1);
        }
    }
    // the duration's split against the original's
    for (unsigned long sec = 0; sec < 400000; sec += (sec < 200000 ? 1 : 37)) {
        for (int up = 0; up < 2; up++) {
            unsigned long v;
            unsigned char u = LobbySplitDuration(sec, up != 0, &v);
            uint32_t rv;
            uint8_t ru = ee_won_lobbyctl_split_duration((uint32_t)sec, up != 0, &rv);
            sChecks++;
            if (u != ru || v != rv) { sFailures++; if (sFailures < 40) printf("FAIL split %lu %d\n", sec, up); }
        }
    }
    unsigned long long aLines = 0;
    int aDiffer = 0;
    unsigned aStepSeed = 2027;
    int kSequences = 60000;
    if (argc > 2) kSequences = atoi(argv[2]);
    static const Ev kEvents[] = {Ev::TextLinkActivated, Ev::ButtonPressed, Ev::InputReturn, Ev::Alert,
                                 Ev::AnnounceFriend, Ev::AnnounceClientLocation, Ev::ClientModerated,
                                 Ev::ChatCommandError, Ev::GroupInvitation, Ev::GroupJoinAttempt, Ev::JoinedRoom,
                                 Ev::LeftRoom, Ev::PlayerTeamChanged, Ev::RecvChat, Ev::RecvChat, Ev::SetChatInput,
                                 Ev::SetRoomSpecFlags, Ev::Other, Ev::ButtonPressed, Ev::InputReturn};
    for (int n = 0; n < kSequences; n++) {
        unsigned aSave = sSeed;
        sSeed = aStepSeed;
        std::vector<Step> aSteps;
        // the config reader's stores
        if (R(5)) { Step s = {Call::SetField, 20, (intptr_t)(R(4) == 0 ? 0x12 : R(2) ? 1 : 2), 0}; aSteps.push_back(s); }
        int aColors = (int)R(3);
        for (int i = 0; i < aColors; i++) { Step s = {Call::SetField, (intptr_t)R(11), (intptr_t)R(0x1000000), 0}; aSteps.push_back(s); }
        if (R(8)) { Step s = {Call::Init, 0xC0, 0, 0}; aSteps.push_back(s); }
        int aCount = 1 + (int)R(12);
        for (int i = 0; i < aCount; i++) {
            Step s = {Call::Event, kEvent, (intptr_t)kEvents[R(sizeof(kEvents) / sizeof(kEvents[0]))], 0};
            switch (R(14)) {
            case 0: s.mCall = Call::Init; s.a = 0xC0; s.b = 0; break;
            case 1: s.mCall = Call::Field; s.a = R(2) ? (intptr_t)R(5) : R(2) ? (R(3) ? 120 + (intptr_t)R(2) : 100) : 130 + (intptr_t)R(11); s.b = 0; break;
            default: break;
            }
            if (s.mCall == Call::Event) {
                unsigned w = R(5);
                if (w == 0) s.c = 0;
                else if (w == 1) s.c = kOther;
                else s.c = kCtl + (kSlotChat + (intptr_t)R(3)) * 0x10;
            }
            aSteps.push_back(s);
        }
        aStepSeed = sSeed;
        sSeed = aSave;
        unsigned aSeed = 91u + (unsigned)n * 7919u;
        std::vector<std::string> a = RunSide(false, aSeed, aSteps);
        std::vector<std::string> b = RunSide(true, aSeed, aSteps);
        aLines += a.size();
        sChecks++;
        if (a != b) {
            if (++aDiffer <= 3) {
                printf("sequence %d differs\n", n);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "-";
                    const char* y = i < b.size() ? b[i].c_str() : "-";
                    if (i >= a.size() || i >= b.size() || a[i] != b[i]) {
                        printf("  line %zu: C++ '%s' Rust '%s'\n", i, x, y);
                        for (size_t k = i > 6 ? i - 6 : 0; k < i; k++) printf("    %s\n", a[k].c_str());
                        break;
                    }
                }
            }
            continue;
        }
        bool aNew = n < 60;
        for (size_t i = 0; i < a.size(); i++) {
            long long t[4] = {0, 0, 0, 0};
            char aWord[16] = "";
            int k = sscanf(a[i].c_str(), "%15s %lld %lld %lld %lld", aWord, &t[0], &t[1], &t[2], &t[3]);
            std::string aKey = aWord;
            if (k >= 2 && strcmp(aWord, "ret") != 0) aKey += " " + std::to_string(t[0]);
            if (strcmp(aWord, "op") == 0 && k >= 4 && (t[0] == (long long)Op::EvGet || t[0] == (long long)Op::MGet ||
                                                       t[0] == (long long)Op::MCall || t[0] == (long long)Op::LinkGet))
                aKey += " " + std::to_string(t[2]);
            if (strcmp(aWord, "call") == 0 && k >= 4 && t[0] == (long long)Call::Event) aKey += " " + std::to_string(t[2]);
            if (sSeen.insert(aKey).second) aNew = true;
        }
        if (aPrint && aNew) {
            printf("seq %d\n", n);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (aDiffer) { sFailures++; printf("FAIL: %d of %d sequences differ\n", aDiffer, kSequences); }
    if (!aPrint)
        printf("won_chatctl_test: %d sequences, %llu trace lines, %zu distinct ops and calls; %d checks, %d failures\n",
               kSequences, aLines, sSeen.size(), sChecks, sFailures);
    return sFailures ? 1 : 0;
}
