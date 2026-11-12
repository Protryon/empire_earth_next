// The lobby's game room (StagingLogic.cpp, LobbyModel.cpp's LobbyGame and LobbyPlayer): each room message built
// the way the original built it with WriteBuffer (the statements copied from the C++ before the port) and by
// ee_won's staging module through its C ABI must be the same bytes; the original HandleMessage's dispatch and its
// handlers' guards and reads (copied, the objects replaced by what they answered) and the Rust read and guards must
// do the same for every type, length, join state and role; the original UpdateReadyState, IsFull, GetJoinStatus,
// HandleJoinRequest's first checks and MatchesRoomSpec over their inputs, and WriteSummary / ReadSummary over
// summaries of each game type cut at every length, the same. Links WriteBuffer / ReadBuffer / ByteBuffer. Prints one
// line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

static void AppendUnits(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring* anOut = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        WideAppendUtf16Unit(*anOut, inUnits[i]);
}

static std::string Bytes(WriteBuffer& inBuffer)
{
    ByteBufferPtr aBuffer = inBuffer.ToByteBuffer(true);
    return std::string(aBuffer->data(), aBuffer->length());
}

static const uint8_t* U8(const std::string& inText)
{
    return reinterpret_cast<const uint8_t*>(inText.data());
}

static bool Same(const char* inWhat, const std::string& inGot, const std::string& inWant)
{
    if (inGot == inWant)
        return true;
    printf("  %s\n  got ", inWhat);
    for (size_t i = 0; i < inGot.size(); i++) printf(" %02x", (unsigned char)inGot[i]);
    printf("\n  want");
    for (size_t i = 0; i < inWant.size(); i++) printf(" %02x", (unsigned char)inWant[i]);
    printf("\n");
    return false;
}

static std::string RustMessage(unsigned char inKind, unsigned short inId, bool inFlag, unsigned short inPing)
{
    EeWonRoomMsg m;
    memset(&m, 0, sizeof(m));
    m.kind = inKind;
    m.id = inId;
    m.flag = inFlag;
    m.ping = inPing;
    std::string aGot;
    ee_won_room_message(&m, Append, &aGot);
    return aGot;
}

// ---- the original's statements (StagingLogic.cpp, LobbyModel.cpp before the port)

static std::string OriginalMessage(unsigned char inKind, unsigned short inId, bool inFlag, unsigned short inPing)
{
    WriteBuffer aBuffer(0, true);
    switch (inKind) {
    case 1: // LobbyGame::GetJoinRequest (before the game code's hook)
        aBuffer.AppendByte(1);
        aBuffer.AppendShort(inPing);
        break;
    case 3: // StagingLogic::HandleJoinRequest's announcement (before the player)
        aBuffer.AppendByte(3);
        aBuffer.AppendShort(inId);
        break;
    case 4: // SendReady
        aBuffer.AppendByte(4);
        aBuffer.AppendBool(inFlag);
        break;
    case 5: // HandleReadyRequest
        aBuffer.AppendByte(5);
        aBuffer.AppendShort(inId);
        aBuffer.AppendBool(inFlag);
        break;
    case 6: // SendGameDissolved
        aBuffer.AppendByte(6);
        break;
    case 7: // SendStartGame
        aBuffer.AppendByte(7);
        break;
    case 8: // NotifyPingChange
        aBuffer.AppendByte(8);
        aBuffer.AppendShort(inPing);
        break;
    case 9: // HandlePingReport
        aBuffer.AppendByte(9);
        aBuffer.AppendShort(inId);
        aBuffer.AppendShort(inPing);
        break;
    case 10: // KickClient
        aBuffer.AppendByte(10);
        aBuffer.AppendShort(inId);
        aBuffer.AppendBool(inFlag);
        break;
    }
    return Bytes(aBuffer);
}

struct Roles {
    bool mSenderCaptain, mICaptain, mSenderPlayer, mSenderReady;
};

// HandleMessage and each handler up to where it acts: what it would do, "nothing" (a guard failed or a read threw:
// the handlers did nothing before their reads), "drop" (joining)
static std::string OriginalDispatch(const std::string& inBytes, short inJoinStatus, const Roles& r)
{
    ReadBuffer inBuffer(inBytes.data(), inBytes.length());
    char anOut[96];
    try {
        unsigned char aType = inBuffer.ReadByte();
        if (inJoinStatus == 1 && aType != 2) return "drop";
        switch (aType) {
        case 1:
            if (r.mSenderPlayer) return "nothing";
            snprintf(anOut, sizeof(anOut), "join-request rest=%d", (int)inBuffer.pos());
            return anOut;
        case 2:
            if (!r.mSenderCaptain) return "nothing";
            snprintf(anOut, sizeof(anOut), "join-reply rest=%d", (int)inBuffer.pos());
            return anOut;
        case 3: {
            if (!r.mSenderCaptain) return "nothing";
            unsigned short aClientId = inBuffer.ReadShort();
            snprintf(anOut, sizeof(anOut), "player-joined id=%u rest=%d", aClientId, (int)inBuffer.pos());
            return anOut;
        }
        case 4: {
            if (!r.mICaptain || !r.mSenderPlayer) return "nothing";
            bool aReady = inBuffer.ReadBool();
            if (r.mSenderReady == aReady) return "nothing";
            snprintf(anOut, sizeof(anOut), "ready-request ready=%d", (int)aReady);
            return anOut;
        }
        case 5: {
            if (!r.mSenderCaptain) return "nothing";
            unsigned short anId = inBuffer.ReadShort();
            bool aReady = inBuffer.ReadBool();
            snprintf(anOut, sizeof(anOut), "ready-changed id=%u ready=%d", anId, (int)aReady);
            return anOut;
        }
        case 6:
            if (r.mSenderCaptain && !r.mICaptain) return "dissolved";
            return "nothing";
        case 7:
            if (!r.mSenderCaptain) return "nothing";
            snprintf(anOut, sizeof(anOut), "start exit=%d", r.mICaptain ? 2 : 1);
            return anOut;
        case 8: {
            if (!r.mICaptain) return "nothing";
            unsigned short aPing = inBuffer.ReadShort();
            if (!r.mSenderPlayer) return "nothing";
            snprintf(anOut, sizeof(anOut), "ping-report ping=%u", aPing);
            return anOut;
        }
        case 9: {
            if (!r.mSenderCaptain) return "nothing";
            unsigned short anId = inBuffer.ReadShort();
            unsigned short aPing = inBuffer.ReadShort();
            snprintf(anOut, sizeof(anOut), "ping-changed id=%u ping=%u", anId, aPing);
            return anOut;
        }
        case 10: {
            if (!r.mSenderCaptain) return "nothing";
            unsigned short anId = inBuffer.ReadShort();
            bool aFlag = inBuffer.ReadBool();
            snprintf(anOut, sizeof(anOut), "kicked id=%u ban=%d", anId, (int)aFlag);
            return anOut;
        }
        default:
            snprintf(anOut, sizeof(anOut), "game type=%u rest=%d", aType, (int)inBuffer.pos());
            return anOut;
        }
    } catch (ReadBufferException&) {
        return "nothing";
    }
}

static std::string RustDispatch(const std::string& inBytes, short inJoinStatus, const Roles& r)
{
    EeWonRoomMsg m;
    memset(&m, 0, sizeof(m));
    int aRead = ee_won_room_read(U8(inBytes), inBytes.length(), inJoinStatus == 1, &m);
    if (aRead == 1) return "drop";
    if (aRead != 0) return "nothing";
    EeWonRoomRoles aRoles = {r.mSenderCaptain, r.mICaptain, r.mSenderPlayer, r.mSenderReady};
    if (!ee_won_room_accepts(&m, &aRoles)) return "nothing";
    char anOut[96];
    switch (m.kind) {
    case 1: snprintf(anOut, sizeof(anOut), "join-request rest=%d", (int)m.rest); break;
    case 2: snprintf(anOut, sizeof(anOut), "join-reply rest=%d", (int)m.rest); break;
    case 3: snprintf(anOut, sizeof(anOut), "player-joined id=%u rest=%d", m.id, (int)m.rest); break;
    case 4: snprintf(anOut, sizeof(anOut), "ready-request ready=%d", (int)m.flag); break;
    case 5: snprintf(anOut, sizeof(anOut), "ready-changed id=%u ready=%d", m.id, (int)m.flag); break;
    case 6: return "dissolved";
    case 7: snprintf(anOut, sizeof(anOut), "start exit=%d", ee_won_room_start_exit_code(r.mICaptain)); break;
    case 8: snprintf(anOut, sizeof(anOut), "ping-report ping=%u", m.ping); break;
    case 9: snprintf(anOut, sizeof(anOut), "ping-changed id=%u ping=%u", m.id, m.ping); break;
    case 10: snprintf(anOut, sizeof(anOut), "kicked id=%u ban=%d", m.id, (int)m.flag); break;
    default: snprintf(anOut, sizeof(anOut), "game type=%u rest=%d", m.kind, (int)m.rest); break;
    }
    return anOut;
}

// UpdateReadyState over plain values: the sound (0 none, 1 all ready, 2 not ready; the staging sounds on) and the
// remembered state
struct ReadyCase {
    bool mAllReady, mLastAllReady, mIsHost, mLastReady, mIAmReady;
    long long mNow, mLastReadyTime;
};

static void OriginalReady(ReadyCase& c, int& outSound)
{
    outSound = 0;
    bool isReady = c.mAllReady;
    if (isReady) {
        if (c.mLastAllReady != isReady) {
            long long aTime = c.mNow;
            if (aTime - c.mLastReadyTime >= 15) {
                c.mLastReadyTime = aTime;
                outSound = 1;
            }
        }
    } else if (!c.mIsHost && c.mLastReady != c.mIAmReady) {
        c.mLastReady = c.mIAmReady;
        if (!c.mLastReady) outSound = 2;
    }
    c.mLastAllReady = isReady;
}

static bool OriginalIsFull(long inPlayers, long inSlots) { return inSlots > 0 && inPlayers >= inSlots; }

// GetJoinStatus, the hook answering 77
static short OriginalJoinStatus(bool inFull, bool inInviteOnly, bool inInvited, bool inIsHost, bool inInProgress)
{
    if (inFull) return 2;
    if (inInviteOnly && !inInvited && !inIsHost) return 5;
    if (inInProgress) return 3;
    return 77;
}

static short OriginalHostJoinCheck(bool inFull, bool inInProgress)
{
    if (inFull) return 2;
    if (inInProgress) return 3;
    return 0;
}

static bool OriginalMatchesRoomSpec(long inFlags, long inGameType)
{
    if (inFlags & 4) {
        if (inGameType == 2) return true;
    } else if (inFlags & 8) {
        if (inGameType == 1 || inGameType == 3) return true;
    }
    return false;
}

// the summary's part of a LobbyGame
struct Game {
    long mGameType;
    unsigned char mAddress[6];
    bool mInProgress;
    std::wstring mName;
    std::wstring mPassword;
    long mSkill;
    bool mHasPassword, mInviteOnly, mAskToJoin;
    long mNumPlayers, mNumOpenSlots;
};

static const unsigned short kProductId = 0x1e0;

static void OriginalWriteSummary(const Game& g, WriteBuffer& outBuffer)
{
    if (g.mGameType == 2)
        outBuffer.AppendBytes(g.mAddress, 6);
    else
        outBuffer.AppendShort(kProductId);
    outBuffer.AppendBool(g.mInProgress);
    if (g.mGameType != 2)
        outBuffer.AppendWString(g.mName, 2);
    outBuffer.AppendByte(g.mSkill);
    if (g.mGameType != 2) {
        unsigned char aFlags = 0;
        if (g.mPassword.length() != 0) aFlags = 1;
        if (g.mInviteOnly) aFlags |= 2;
        if (g.mAskToJoin) aFlags |= 4;
        outBuffer.AppendByte(aFlags);
    }
    outBuffer.AppendShort(g.mNumPlayers);
    outBuffer.AppendShort(g.mNumOpenSlots);
}

// ReadSummary (the hook not called); outPos: where the hook would read
static bool OriginalReadSummary(Game& g, ReadBuffer& inBuffer)
{
    try {
        if (g.mGameType == 2)
            memcpy(g.mAddress, inBuffer.ReadBytes(6), 6);
        else {
            short aProductId = inBuffer.ReadShort();
            if (aProductId != (short)kProductId) return false;
        }
        g.mInProgress = inBuffer.ReadBool();
        if (g.mGameType != 2) {
            std::wstring aName;
            inBuffer.ReadWString(aName, 2);
            g.mName = aName;
        }
        g.mSkill = (char)inBuffer.ReadByte();
        if (g.mSkill < 0 || g.mSkill >= 5) g.mSkill = 0;
        if (g.mGameType != 2) {
            unsigned char aFlags = inBuffer.ReadByte();
            g.mHasPassword = (aFlags & 1) != 0;
            g.mInviteOnly = ((aFlags >> 1) & 1) != 0;
            g.mAskToJoin = ((aFlags >> 2) & 1) != 0;
        }
        g.mNumPlayers = inBuffer.ReadShort();
        g.mNumOpenSlots = inBuffer.ReadShort();
        return true;
    } catch (ReadBufferException&) {
        return false;
    }
}

// ---- the ported forms

static std::string Utf16(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    return aBytes;
}

static std::string RustWriteSummary(const Game& g)
{
    EeWonGameSummary s;
    memset(&s, 0, sizeof(s));
    s.game_type = g.mGameType;
    memcpy(s.address, g.mAddress, 6);
    s.product_id = kProductId;
    s.in_progress = g.mInProgress;
    s.has_password = g.mPassword.length() != 0;
    s.invite_only = g.mInviteOnly;
    s.ask_to_join = g.mAskToJoin;
    s.skill = g.mSkill;
    s.players = g.mNumPlayers;
    s.open_slots = g.mNumOpenSlots;
    std::string aName = Utf16(g.mName), aGot;
    ee_won_game_write_summary(&s, U8(aName), aName.length(), Append, &aGot);
    return aGot;
}

// the forwarder's assignments (LobbyGame::ReadSummary)
static bool RustReadSummary(Game& g, const std::string& inBytes, size_t& outRest)
{
    EeWonGameSummary s;
    std::wstring aName;
    ee_won_game_read_summary(U8(inBytes), inBytes.length(), g.mGameType, kProductId, &s, AppendUnits, &aName);
    if (s.read & 1) memcpy(g.mAddress, s.address, 6);
    if (s.read & 2) g.mInProgress = s.in_progress;
    if (s.read & 4) g.mName = aName;
    if (s.read & 8) g.mSkill = s.skill;
    if (s.read & 0x10) {
        g.mHasPassword = s.has_password;
        g.mInviteOnly = s.invite_only;
        g.mAskToJoin = s.ask_to_join;
    }
    if (s.read & 0x20) g.mNumPlayers = s.players;
    if (s.read & 0x40) g.mNumOpenSlots = s.open_slots;
    outRest = s.rest;
    return s.ok;
}

static bool SameGame(const Game& a, const Game& b)
{
    return a.mGameType == b.mGameType && memcmp(a.mAddress, b.mAddress, 6) == 0 && a.mInProgress == b.mInProgress &&
           a.mName == b.mName && a.mSkill == b.mSkill && a.mHasPassword == b.mHasPassword &&
           a.mInviteOnly == b.mInviteOnly && a.mAskToJoin == b.mAskToJoin && a.mNumPlayers == b.mNumPlayers &&
           a.mNumOpenSlots == b.mNumOpenSlots;
}

static Game Sentinel(long inType)
{
    Game g;
    g.mGameType = inType;
    memset(g.mAddress, 0xee, 6);
    g.mInProgress = true;
    g.mName = L"old";
    g.mSkill = 99;
    g.mHasPassword = g.mInviteOnly = g.mAskToJoin = true;
    g.mNumPlayers = g.mNumOpenSlots = -7;
    return g;
}

int main()
{
    // the messages
    const unsigned short kIds[] = {0, 1, 7, 0x1234, 0xfffe, 0xffff};
    const unsigned short kPings[] = {0, 40, 0x7fff, 0x8000, 0xfffc, 0xfffe};
    const unsigned char kKinds[] = {1, 3, 4, 5, 6, 7, 8, 9, 10};
    for (size_t k = 0; k < sizeof(kKinds); k++)
        for (size_t i = 0; i < sizeof(kIds) / sizeof(kIds[0]); i++)
            for (size_t p = 0; p < sizeof(kPings) / sizeof(kPings[0]); p++)
                for (int f = 0; f < 2; f++)
                    CHECK(Same("message", RustMessage(kKinds[k], kIds[i], f != 0, kPings[p]),
                               OriginalMessage(kKinds[k], kIds[i], f != 0, kPings[p])));
    for (size_t p = 0; p < sizeof(kPings) / sizeof(kPings[0]); p++)
        for (int f = 0; f < 2; f++) {
            // LobbyPlayer::WriteToBuffer before the game code's data
            WriteBuffer aBuffer(0, true);
            aBuffer.AppendShort(kPings[p]);
            aBuffer.AppendBool(f != 0);
            std::string aGot;
            ee_won_room_player_head(kPings[p], f != 0, Append, &aGot);
            CHECK(Same("player", aGot, Bytes(aBuffer)));
        }

    // the dispatch: every type byte, the fields' bytes cut at every length, both join states, every role
    const unsigned char kBody[] = {0x02, 0x01, 0x00, 0x2c, 0x01, 0x05};
    const unsigned char kOther[] = {0x02, 0x01, 0x01, 0xff, 0xff, 0x00};
    for (int aType = 0; aType < 256; aType++)
        for (int aBody = 0; aBody < 2; aBody++)
            for (size_t n = 0; n <= sizeof(kBody); n++)
                for (int aJoin = 0; aJoin < 3; aJoin++)
                    for (int aRoles = 0; aRoles < 16; aRoles++) {
                        std::string aBytes(1, (char)aType);
                        aBytes.append(reinterpret_cast<const char*>(aBody ? kOther : kBody), n);
                        Roles r = {(aRoles & 1) != 0, (aRoles & 2) != 0, (aRoles & 4) != 0, (aRoles & 8) != 0};
                        std::string aWant = OriginalDispatch(aBytes, (short)aJoin, r);
                        std::string aGot = RustDispatch(aBytes, (short)aJoin, r);
                        CHECK(aGot == aWant);
                        if (aGot != aWant && sFailures < 40)
                            printf("  type %d body %d n %d join %d roles %d: got %s want %s\n", aType, aBody, (int)n,
                                   aJoin, aRoles, aGot.c_str(), aWant.c_str());
                    }
    {
        Roles r = {false, false, false, false};
        CHECK(OriginalDispatch("", 0, r) == RustDispatch("", 0, r));
        CHECK(OriginalDispatch("", 1, r) == RustDispatch("", 1, r));
    }

    // UpdateReadyState
    const long long kTimes[][2] = {{100, 85}, {100, 86}, {100, 0}, {0, 0}, {15, 0}, {14, 0}, {5, 100}, {2000000000, -2000000000}};
    for (int aBits = 0; aBits < 32; aBits++)
        for (size_t t = 0; t < sizeof(kTimes) / sizeof(kTimes[0]); t++) {
            ReadyCase c = {(aBits & 1) != 0, (aBits & 2) != 0, (aBits & 4) != 0, (aBits & 8) != 0, (aBits & 16) != 0,
                           kTimes[t][0], kTimes[t][1]};
            EeWonReadyState s;
            s.all_ready = c.mAllReady;
            s.last_all_ready = c.mLastAllReady;
            s.is_host = c.mIsHost;
            s.last_ready = c.mLastReady;
            s.i_am_ready = c.mIAmReady;
            s.now = c.mNow;
            s.last_ready_time = c.mLastReadyTime;
            ee_won_room_ready_state(&s);
            int aSound = 0;
            OriginalReady(c, aSound);
            CHECK((int)s.sound == aSound && s.last_all_ready == c.mLastAllReady && s.last_ready == c.mLastReady &&
                  s.last_ready_time == c.mLastReadyTime);
        }

    // the join rules
    for (long aPlayers = -2; aPlayers <= 10; aPlayers++)
        for (long aSlots = -1; aSlots <= 9; aSlots++)
            CHECK(ee_won_game_is_full(aPlayers, aSlots) == OriginalIsFull(aPlayers, aSlots));
    for (int aBits = 0; aBits < 32; aBits++) {
        bool b[5];
        for (int i = 0; i < 5; i++) b[i] = (aBits >> i) & 1;
        short aWant = OriginalJoinStatus(b[0], b[1], b[2], b[3], b[4]);
        short aGot = ee_won_game_join_status(b[0], b[1], b[2], b[3], b[4]);
        CHECK(aGot == (aWant == 77 ? -1 : aWant));
        CHECK(ee_won_game_host_join_check(b[0], b[1]) == OriginalHostJoinCheck(b[0], b[1]));
    }
    for (long aFlags = 0; aFlags < 32; aFlags++)
        for (long aType = 0; aType <= 4; aType++)
            CHECK(ee_won_game_matches_room_spec(aFlags, aType) == OriginalMatchesRoomSpec(aFlags, aType));

    // the summaries: each game type, odd skills and counts, cut at every length
    const long kSkills[] = {0, 3, 4, 5, -1, 0x102, 0x7f, 0x80};
    for (long aType = 1; aType <= 3; aType++)
        for (size_t k = 0; k < sizeof(kSkills) / sizeof(kSkills[0]); k++) {
            Game g = Sentinel(aType);
            unsigned char anAddress[6] = {0x1e, 0x61, 10, 0, 0, 7};
            memcpy(g.mAddress, anAddress, 6);
            g.mInProgress = (k & 1) != 0;
            g.mName = k & 2 ? L"Bob's \xe9 game \x263a" : L"";
            g.mPassword = k & 1 ? L"pw" : L"";
            g.mSkill = kSkills[k];
            g.mInviteOnly = (k & 2) != 0;
            g.mAskToJoin = (k & 4) != 0;
            g.mNumPlayers = k == 7 ? 0x10005 : (long)k;
            g.mNumOpenSlots = k == 6 ? -3 : 8;
            WriteBuffer aBuffer(0, true);
            OriginalWriteSummary(g, aBuffer);
            std::string aWant = Bytes(aBuffer);
            CHECK(Same("summary", RustWriteSummary(g), aWant));
            // a wrong product id
            std::string aWrong = aWant;
            if (aType != 2) aWrong[0] ^= 1;
            for (int aCase = 0; aCase < 2; aCase++) {
                const std::string& aFull = aCase ? aWrong : aWant;
                for (size_t n = 0; n <= aFull.length() + 1; n++) {
                    std::string aBytes = aFull.substr(0, n);
                    if (n == aFull.length() + 1) aBytes += "\x09";
                    Game a = Sentinel(aType), b = Sentinel(aType);
                    ReadBuffer anIn(aBytes.data(), aBytes.length());
                    bool aWantOk = OriginalReadSummary(a, anIn);
                    size_t aRest = 0;
                    bool aGotOk = RustReadSummary(b, aBytes, aRest);
                    CHECK(aGotOk == aWantOk && SameGame(a, b));
                    CHECK(!aWantOk || aRest == (size_t)anIn.pos());
                }
            }
        }

    printf("won_staging_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
