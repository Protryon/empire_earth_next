// RoutingLogic's connection requests (R6 WONLobby round 14): the host cases RoutingLogic.cpp had for them (the
// statements copied from the C++ before the move, each a RoutingConnection request helper: tests/won/won_routehelpers.h,
// the helpers as they were) against routelogic.rs's conn_request, which answers them in the game since then. Both
// sides make a request kind and its values (RoutingRequestValues in the helper's parameter order); the original's
// are recorded instead of run (this driver's RoutingRequestValues::Run stand-in), the Rust's through
// ee_won_routelogic_conn_request with the same arguments and texts.
//
// 200,000 random requests: every op and ConnDo kind, arguments over the whole 64-bit range and near the 16- and
// 32-bit edges, texts of 0..12 units (ASCII, Latin-1, BMP, surrogates alone and paired) or none (0: the empty text).
// The kinds and values must be the same. The first 60 lines (and each request of a new op) are
// rust/crates/ee_won_lobby/src/golden_routeconn.txt, which the Rust tests replay; `--print` prints them. The exit
// code is the number of mismatches.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include "rust/won_lobby/routelogic.h"
#include "rust/won_net/routecpp.h"

// ---- the original's side: the helpers' value lists, recorded

struct Rec {
    int kind = -1;
    std::vector<std::pair<long long, std::string> > values;
};
static Rec sRec;

namespace WONAPI {
class RoutingConnection;
template <class T> class CompletionBase;
class AsyncOpPtr;
typedef CompletionBase<AsyncOpPtr> RouteCompletion;
class ByteBuffer;
}
using namespace WONAPI;

// WONRouting.h's RoutingRequestValues (verbatim but Run, which records) and WideAppendUtf16LE (UTF-16 units kept)
static void WideAppendUtf16LE(std::string& outBytes, const std::wstring& inText)
{
    for (size_t i = 0; i < inText.size(); i++) {
        uint32_t u = (uint32_t)inText[i];
        if (u > 0xffff) {
            uint32_t v = u - 0x10000;
            uint16_t hi = (uint16_t)(0xd800 + (v >> 10)), lo = (uint16_t)(0xdc00 + (v & 0x3ff));
            outBytes.append(1, (char)(hi & 0xff)).append(1, (char)(hi >> 8)).append(1, (char)(lo & 0xff)).append(1, (char)(lo >> 8));
        } else
            outBytes.append(1, (char)(u & 0xff)).append(1, (char)(u >> 8));
    }
}
class RoutingRequestValues {
public:
    RoutingRequestValues& N(long long inNum) { return this->Add(inNum, std::string()); }
    RoutingRequestValues& W(const std::wstring& inText) { std::string b; WideAppendUtf16LE(b, inText); return this->Add(0, b); }
    RoutingRequestValues& S(const std::string& inBytes) { return this->Add(0, inBytes); }
    RoutingRequestValues& L(const std::list<unsigned short>& inIds)
    {
        std::string b;
        for (std::list<unsigned short>::const_iterator i = inIds.begin(); i != inIds.end(); ++i)
            b.append(1, (char)(*i & 0xff)).append(1, (char)(*i >> 8));
        return this->Add(0, b);
    }
    void Run(RoutingConnection*, int inKind, RouteCompletion*)
    {
        sRec.kind = inKind;
        sRec.values.assign(this->mValues.begin(), this->mValues.end());
    }
private:
    RoutingRequestValues& Add(long long inNum, const std::string& inBytes)
    { this->mValues.push_back(std::make_pair(inNum, inBytes)); return *this; }
    std::list<std::pair<long long, std::string> > mValues;
};

// the helpers RoutingLogic.cpp's cases called (tests/won/won_routehelpers.h's, as WONRouting.h had them)
class RC_ {
public:
    RoutingConnection* mConn = nullptr;
    void LeaveGroup(unsigned short inId, RouteCompletion* c) { RoutingRequestValues().N(inId).Run(mConn, EE_WON_RTQ_LEAVE, c); } // 2bd850
    void CancelJoinGroup(unsigned short inId, bool inLeave, RouteCompletion* c)
    { RoutingRequestValues().N(inId).N(inLeave).Run(mConn, EE_WON_RTQ_CANCEL_JOIN, c); } // 2bd6b0
    void ReplaceDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName, RouteCompletion* c)
    { RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).Run(mConn, EE_WON_RTQ_DELETE_DATA, c); }
    void SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inRecipient, RouteCompletion* c)
    { this->SendChatTo(inText, (unsigned short)inFlags, std::list<unsigned short>(1, inRecipient), c); } // 2be540
    void SendChatTo(const std::wstring& inText, unsigned short inFlags, const std::list<unsigned short>& inClients,
                    RouteCompletion* c)
    { RoutingRequestValues().W(inText).N(inFlags).L(inClients).Run(mConn, EE_WON_RTQ_CHAT, c); } // 2be6a0
    void SetClientFlags(unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask, unsigned long inAsyncFlags,
                        RouteCompletion* c)
    { RoutingRequestValues().N(inMask).N(inFlags).N(inAsyncMask).N(inAsyncFlags).Run(mConn, EE_WON_RTQ_SET_CLIENT_FLAGS, c); }
    void SetGroupFlags(unsigned short inId, unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask,
                       unsigned long inAsyncFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inId).N(inMask).N(inFlags).N(inAsyncMask).N(inAsyncFlags)
            .Run(mConn, EE_WON_RTQ_SET_GROUP_FLAGS, c);
    } // 2bdc10
    void BanClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                   unsigned short inClientId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN, inGroupId, inOn, inSeconds, inExplanation, inClientId, c); } // 2bf590
    void BanClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                          unsigned long inWONId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN_WON_ID, inGroupId, inOn, inSeconds, inExplanation, inWONId, c); } // 2bf6b0
    void MuteClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                    unsigned short inClientId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE, inGroupId, inOn, inSeconds, inExplanation, inClientId, c); } // 2bf7d0
    void MuteClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                           const std::wstring& inExplanation, unsigned long inWONId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE_WON_ID, inGroupId, inOn, inSeconds, inExplanation, inWONId, c); } // 2bf8f0
    void AcceptClient(unsigned short inGroupId, unsigned short inClientId, bool inOn, const std::wstring& inExplanation,
                      RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inClientId).N(inOn).W(inExplanation).Run(mConn, EE_WON_RTQ_ACCEPT, c); }
    void InviteClient(unsigned short inGroupId, unsigned short inClientId, bool inOn, const std::wstring& inExplanation,
                      RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inClientId).N(inOn).W(inExplanation).Run(mConn, EE_WON_RTQ_INVITE, c); }
    void BecomeModerator(bool inOn, RouteCompletion* c) { RoutingRequestValues().N(inOn).Run(mConn, EE_WON_RTQ_MODERATOR, c); } // 2bf3f0
    void SendServerAlert(const std::wstring& inText, RouteCompletion* c)
    { RoutingRequestValues().W(inText).Run(mConn, EE_WON_RTQ_ALERT, c); } // 2bddd0
    void StartServerShutdown(const std::wstring& inText, unsigned long inSeconds, RouteCompletion* c)
    { RoutingRequestValues().W(inText).N(inSeconds).Run(mConn, EE_WON_RTQ_START_SHUTDOWN, c); } // 2be010
    void AbortServerShutdown(RouteCompletion* c) { RoutingRequestValues().Run(mConn, EE_WON_RTQ_ABORT_SHUTDOWN, c); } // 2be200
private:
    void Moderate(int inKind, unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                  const std::wstring& inExplanation, unsigned long inId, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inOn).N(inSeconds).W(inExplanation).N(inId).Run(mConn, inKind, c); }
};

// the texts: index p (1-based) into this sequence's texts, 0 the empty text (TX(0) is gEmptyGUIString)
static std::vector<std::wstring> sTexts;
static wchar_t gEmptyWideString[1];
static const std::wstring& TX(intptr_t p) { static std::wstring e; return p == 0 ? e : sTexts[(size_t)p - 1]; }
struct Logic { std::string mGameDataType = "G"; };

// RoutingLogic.cpp's cases (the statements as they were before the move; RC(a) the recorder, L the logic)
static bool Original(EeWonRouteLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e)
{
    typedef EeWonRouteLogicOp O;
    RC_ aRc;
    RC_* RCa = &aRc;
    Logic aLogic;
    Logic* L = &aLogic;
    (void)a;
#define RC(x) RCa
    switch (inOp) {
    case O::ConnDo:
        switch (b) {
        case 6: RC(a)->AbortServerShutdown(NULL); return true;
        case 7: RC(a)->BecomeModerator(c != 0, NULL); return true;
        case 8: RC(a)->LeaveGroup((unsigned short)c, NULL); return true;
        case 9: RC(a)->CancelJoinGroup((unsigned short)c, true, NULL); return true;
        case 10: RC(a)->ReplaceDataObject((unsigned short)c, L->mGameDataType, gEmptyWideString, NULL); return true;
        }
        return false;
    case O::ConnSetClientFlags:
        RC(a)->SetClientFlags((unsigned long)b, (unsigned long)c, (unsigned long)d, (unsigned long)e, NULL);
        return true;
    case O::ConnSetGroupFlags:
        RC(a)->SetGroupFlags((unsigned short)b, (unsigned long)c, (unsigned long)d, 0, 0, NULL);
        return true;
    case O::ConnBan:
        RC(a)->BanClient((unsigned short)b, (b >> 16) != 0, (unsigned long)c, TX(d), (unsigned short)e, NULL);
        return true;
    case O::ConnMute:
        RC(a)->MuteClient((unsigned short)b, (b >> 16) != 0, (unsigned long)c, TX(d), (unsigned short)e, NULL);
        return true;
    case O::ConnBanByWonId:
        RC(a)->BanClientByWONId((unsigned short)b, false, 0, std::wstring(gEmptyWideString), (unsigned long)c, NULL);
        return true;
    case O::ConnMuteByWonId:
        RC(a)->MuteClientByWONId((unsigned short)b, false, 0, std::wstring(gEmptyWideString), (unsigned long)c, NULL);
        return true;
    case O::ConnAccept: RC(a)->AcceptClient((unsigned short)b, (unsigned short)c, d != 0, gEmptyWideString, NULL); return true;
    case O::ConnInvite: RC(a)->InviteClient((unsigned short)b, (unsigned short)c, d != 0, TX(e), NULL); return true;
    case O::ConnSendChat: RC(a)->SendChat(TX(b), (unsigned long)c, (unsigned short)d, NULL); return true;
    case O::ConnSendChatTo: {
        const uint16_t* anIds = reinterpret_cast<const uint16_t*>(e);
        std::list<unsigned short> aRecipients(anIds, anIds + d);
        RC(a)->SendChatTo(TX(b), (unsigned short)c, aRecipients, NULL);
        return true;
    }
    case O::ConnAlert: RC(a)->SendServerAlert(TX(b), NULL); return true;
    case O::ConnStartShutdown: RC(a)->StartServerShutdown(TX(b), (unsigned long)c, NULL); return true;
    default: return false;
    }
#undef RC
}

// ---- the port's side

static Rec sPort;
static size_t PortText(void*, intptr_t p, uint32_t* outUnits)
{
    const std::wstring& t = TX(p);
    if (outUnits != NULL)
        for (size_t i = 0; i < t.size(); i++) outUnits[i] = (uint32_t)t[i];
    return t.size();
}
static void PortValue(void*, int64_t num, const uint8_t* bytes, size_t length)
{
    sPort.values.push_back(std::make_pair((long long)num, std::string((const char*)bytes, length)));
}

// ---- the sequences

static uint64_t sSeed = 0x9e3779b97f4a7c15ull;
static uint64_t Next() { sSeed ^= sSeed << 13; sSeed ^= sSeed >> 7; sSeed ^= sSeed << 17; return sSeed; }
static unsigned Rand(unsigned n) { return (unsigned)(Next() % n); }
static intptr_t Arg()
{
    static const intptr_t kEdges[] = {0, 1, -1, 2, 0x7fff, 0x8000, 0xffff, 0x10000, 0x1ffff, 0x7fffffff, (intptr_t)0x80000000ll,
                                      (intptr_t)0xffffffffll, (intptr_t)0x100000000ll, INTPTR_MIN, INTPTR_MAX, 300, 0x105};
    switch (Rand(4)) {
    case 0: return kEdges[Rand(sizeof kEdges / sizeof kEdges[0])];
    case 1: return (intptr_t)Rand(70000);
    case 2: return (intptr_t)Next();
    default: return (intptr_t)(Rand(2) ? 1 : 0) << 16 | (intptr_t)Rand(0x10000);
    }
}
static std::wstring Text()
{
    static const unsigned kUnits[] = {'a', 'Z', '0', ' ', 0xe9, 0xff, 0x263a, 0xd83d, 0xde00, 0xfffd, 0x7f, 0x100};
    std::wstring t;
    unsigned n = Rand(13);
    for (unsigned i = 0; i < n; i++) t += (wchar_t)kUnits[Rand(sizeof kUnits / sizeof kUnits[0])];
    return t;
}

static std::string Line(const char* inSide, EeWonRouteLogicOp op, const intptr_t* g, const Rec& r)
{
    char buf[256];
    std::string s;
    snprintf(buf, sizeof buf, "%s op=%u b=%lld c=%lld d=%lld e=%lld kind=%d", inSide, (unsigned)op, (long long)g[1],
             (long long)g[2], (long long)g[3], (long long)g[4], r.kind);
    s = buf;
    for (size_t i = 0; i < r.values.size(); i++) {
        snprintf(buf, sizeof buf, " %lld:", r.values[i].first);
        s += buf;
        for (unsigned char ch : r.values[i].second) { snprintf(buf, sizeof buf, "%02x", ch); s += buf; }
    }
    return s;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && !strcmp(argv[1], "--print");
    int count = argc > 2 ? atoi(argv[2]) : 200000;
    typedef EeWonRouteLogicOp O;
    static const O kOps[] = {O::ConnDo, O::ConnSetClientFlags, O::ConnSetGroupFlags, O::ConnBan, O::ConnMute,
                             O::ConnBanByWonId, O::ConnMuteByWonId, O::ConnAccept, O::ConnInvite, O::ConnSendChat,
                             O::ConnSendChatTo, O::ConnAlert, O::ConnStartShutdown, O::ConnSendData, O::ConnAdvertise,
                             O::ConnNew, O::TextLength};
    int failures = 0, made = 0, host = 0, printed = 0;
    std::set<std::string> seen;
    for (int n = 0; n < count; n++) {
        O op = kOps[Rand(sizeof kOps / sizeof kOps[0])];
        sTexts.clear();
        for (int i = 0; i < 3; i++) sTexts.push_back(Text());
        std::vector<uint16_t> ids;
        intptr_t g[5] = {0x1000, Arg(), Arg(), Arg(), Arg()};
        if (op == O::ConnDo) g[1] = (intptr_t)Rand(12);
        // a text argument: 0 (the empty text) or one of the three
        intptr_t aText = (intptr_t)Rand(4);
        if (op == O::ConnBan || op == O::ConnMute) g[3] = aText;
        if (op == O::ConnInvite) g[4] = aText;
        if (op == O::ConnSendChat || op == O::ConnSendChatTo || op == O::ConnAlert || op == O::ConnStartShutdown) g[1] = aText;
        if (op == O::ConnSendChatTo) {
            unsigned k = Rand(5);
            for (unsigned i = 0; i < k; i++) ids.push_back((uint16_t)Arg());
            g[3] = (intptr_t)ids.size();
            g[4] = (intptr_t)ids.data();
        }
        sRec = Rec();
        bool orig = Original(op, g[0], g[1], g[2], g[3], g[4]);
        sPort = Rec();
        int32_t aKind = -1;
        bool port = ee_won_routelogic_conn_request(op, g, PortText, PortValue, nullptr, &aKind);
        sPort.kind = port ? aKind : -1;
        if (!orig) { host++; sRec = Rec(); }
        else made++;
        std::string lo = Line("req", op, g, sRec), lp = Line("req", op, g, sPort);
        if (orig != port || lo != lp) {
            if (failures < 20) printf("FAIL: %s (original %d)\n  port   %s (port %d)\n", lo.c_str(), orig, lp.c_str(), port);
            failures++;
        }
        char key[32];
        snprintf(key, sizeof key, "%u/%d", (unsigned)op, op == O::ConnDo ? (int)g[1] : -1);
        if (print && (printed < 60 || seen.insert(key).second)) {
            printed++;
            seen.insert(key);
            // the replayed form: the op, its five arguments, the texts by index, the ids, the result
            std::string s = lo;
            char buf[64];
            snprintf(buf, sizeof buf, " made=%d", orig ? 1 : 0);
            s += buf;
            s += " texts=";
            for (size_t i = 0; i < sTexts.size(); i++) {
                if (i) s += ",";
                for (wchar_t ch : sTexts[i]) { snprintf(buf, sizeof buf, "%04x", (unsigned)ch); s += buf; }
            }
            s += " ids=";
            for (size_t i = 0; i < ids.size(); i++) { snprintf(buf, sizeof buf, "%s%u", i ? "," : "", ids[i]); s += buf; }
            printf("%s\n", s.c_str());
        }
    }
    if (!print)
        printf("won_routeconn_req_test: %d requests (%d made, %d the host's), %d failures\n", count, made, host, failures);
    return failures;
}
