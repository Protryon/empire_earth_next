// The lobby model reading the routing server's records in Rust (R6 WONLobby round 12): the game's model answers
// RoutingLogic's record ops itself (modelobj.rs route_record_op, through ee_won_model_route_record_op), reading the
// routing op's Rust object. The golden half runs the original statements on the same kind of model: RoutingOp.cpp's
// copies into C++ records (tests/won/won_routerecords.h), RoutingLogic.cpp's cases (ServerAddGroup, ServerAddClient,
// ServerSyncClients, ServerSyncGroups, GameReadSummary, RDataObjects, DataObjectLink, GameReadDataObject, the data
// objects' ListFree) and LobbyModelHost.cpp's ReadRoutingInfo (client and room), SyncClients, SyncGroups and
// SetMembers as they were before the move, each C++ object the model's host made a stand-in (the GUIStringData a
// text, LobbyGame::ReadSummaryBuffer a traced answer). The bad user list's rows (LobbyDialogHost.cpp's BadUserItem)
// read the records instead of RoutingBadUser copies: both readings are printed.
//
// The routing ops are real C++ RoutingOps over ee_won's routing ops, each fed one generated server message (client
// lists plain and extended, group lists, the group created and client joined server notices, data objects, peer
// data, bad user lists; some cut short or failing). Each of 30,000 sequences makes a server and a game in both worlds
// and applies 1 to 12 random record ops; after each, the host's trace (the objects made and deleted, the texts, the
// ignore and block questions, the summaries read, the answer) and a dump of the server (its clients, rooms and
// members with every field read, the population) and of every live object's reference count must be the same.
//
// The first 40 sequences and every later one that reaches a line kind none before did are
// rust/crates/ee_won_lobby/src/golden_routerec.txt (the golden half's lines), which the Rust tests replay;
// `--print` prints them. `--mutate` is for the mutation runs (the count of differing sequences, exit 0).
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "won_routerecords.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_net/routeop.h"

using namespace WONAPI;
typedef EeWonModelField MF;
typedef EeWonModelCall MC;
typedef EeWonModelKind MK;
typedef EeWonRouteLogicOp Op;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand(unsigned& ioSeed) { ioSeed = ioSeed * 1103515245u + 12345u; return (ioSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand(sSeed) % n; }
static std::string N(long long v) { return std::to_string(v); }
static std::string Hex(const void* inBytes, size_t inLength)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inLength; i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", static_cast<const unsigned char*>(inBytes)[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}
static std::string Units(const std::wstring& inText)
{
    std::string anOut;
    char aBuffer[8];
    for (size_t i = 0; i < inText.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%04x", (unsigned)(uint16_t)inText[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}
static std::string Units16(const uint16_t* inUnits, size_t inCount)
{
    std::wstring aText;
    for (size_t i = 0; i < inCount; i++) aText += (wchar_t)inUnits[i];
    return Units(aText);
}

// ---- one world of model objects over stand-in shells (the model at the shell's third word)

struct Ref { intptr_t mVt; int32_t mRc; int32_t mPad; EeWonModel* mModel; int mHandle; int mKind; std::string mText; };
static const int kText = 100;
struct World {
    std::map<int, Ref*> mRefs;
    std::map<const void*, int> mRefOf;
    int mNext;
    std::vector<std::string> mTrace;
};
static World* sW;
static Ref* sEmpty;
static void Line(const std::string& inLine) { sW->mTrace.push_back(inLine); }
static intptr_t ModelHost(EeWonModelOp op, intptr_t a, intptr_t b, intptr_t c);
static Ref* NewRef(int inKind, const std::string& inText = std::string())
{
    Ref* r = new Ref;
    r->mVt = 0; r->mRc = 0; r->mPad = 0; r->mKind = inKind; r->mText = inText; r->mModel = NULL; r->mHandle = sW->mNext++;
    sW->mRefs[r->mHandle] = r;
    sW->mRefOf[r] = r->mHandle;
    if (inKind != kText) r->mModel = ee_won_model_new(r, (MK)inKind, ModelHost, sEmpty);
    return r;
}
static void RefAdd(Ref* r) { if (r) r->mRc++; }
static void RefRel(Ref* r)
{
    if (r == NULL || r == sEmpty || --r->mRc > 0) return;
    Line("delete h" + N(r->mHandle));
    sW->mRefs.erase(r->mHandle);
    sW->mRefOf.erase(r);
    if (r->mModel != NULL) ee_won_model_free(r->mModel);
    delete r;
}
static Ref* RR(intptr_t p) { return reinterpret_cast<Ref*>(p); }
static std::string H(intptr_t p)
{
    if (p == 0) return "0";
    if (RR(p) == sEmpty) return "e";
    std::map<const void*, int>::iterator it = sW->mRefOf.find(RR(p));
    return it == sW->mRefOf.end() ? "?" : "h" + N(it->second);
}
static Ref* Held(int inKind, const char* inName)
{
    Ref* r = NewRef(inKind);
    RefAdd(r);
    Line(std::string("new ") + inName + " h" + N(r->mHandle));
    return r;
}
static intptr_t Call(Ref* r, MC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0)
{
    return ee_won_model_call(r->mModel, c, a, b, cc, (intptr_t)sEmpty);
}
static intptr_t Get(Ref* r, MF f) { return ee_won_model_get(r->mModel, f); }
// a text (GUIStringData stand-in) of UTF-16 units, held
static Ref* Text(const std::string& inUnits)
{
    Ref* t = NewRef(kText, inUnits);
    RefAdd(t);
    Line("text h" + N(t->mHandle) + " " + inUnits);
    return t;
}
static std::string TextOf(intptr_t p) { return p == 0 ? "0" : RR(p) == sEmpty ? "e" : RR(p)->mText; }
// LobbyGame::ReadSummaryBuffer's stand-in: the bytes it would read and an answer from their length
static intptr_t Summary(Ref* inGame, const char* inData, unsigned long inLength)
{
    intptr_t anAnswer = (intptr_t)(inLength % 2);
    Line("summary " + H((intptr_t)inGame) + " " + Hex(inData, inLength) + " -> " + N(anAnswer));
    return anAnswer;
}

// ---- the golden half: the original statements

static void OrigSetMembers(Ref* inGroup, const RoutingMemberMap& inMembers, Ref* inClients)
{
    std::vector<uint16_t> anIds;
    std::vector<uint8_t> aFlags;
    for (RoutingMemberMap::const_iterator it = inMembers.begin(); it != inMembers.end(); ++it) {
        anIds.push_back(it->first);
        aFlags.push_back(it->second->mFlags);
    }
    Call(inGroup, MC::GroupSetMembersList, (intptr_t)inClients);
    Call(inGroup, MC::GroupSetMembers, anIds.empty() ? 0 : (intptr_t)&anIds[0], aFlags.empty() ? 0 : (intptr_t)&aFlags[0], anIds.size());
}
// LobbyClient::ReadRoutingInfo
static void OrigClientRead(Ref* inClient, RoutingClientInfo* inInfo)
{
    if (inInfo == NULL) return; // (the original faulted: GetClientInfo found no client)
    intptr_t aValues[4] = { (intptr_t)inInfo->mFlags, inInfo->mClientId, (intptr_t)inInfo->mUserId, (intptr_t)inInfo->mAddress };
    Ref* aName = Text(Units(inInfo->mName));
    Call(inClient, MC::ClientSetRouting, (intptr_t)aName, (intptr_t)aValues);
    RefRel(aName);
}
// LobbyGroup::ReadRoutingInfo
static void OrigGroupRead(Ref* inGroup, RoutingGroupInfo* inInfo, Ref* inClients)
{
    intptr_t aValues[5] = { inInfo->mId, inInfo->mMaxPlayers, (intptr_t)inInfo->mFlags, inInfo->mCaptainId, inInfo->mMaxClients };
    Ref* aName = Text(Units(inInfo->mName));
    Call(inGroup, MC::GroupSetRouting, (intptr_t)aName, (intptr_t)aValues);
    if (inClients != NULL) OrigSetMembers(inGroup, inInfo->mMembers, inClients);
    RefRel(aName);
}
template <class Map> static void SyncArgs(const Map& inMap, std::vector<uint16_t>& outIds, std::vector<intptr_t>& outInfos)
{
    for (typename Map::const_iterator it = inMap.begin(); it != inMap.end(); ++it) {
        outIds.push_back(it->first);
        outInfos.push_back((intptr_t)it->second.get());
    }
}
typedef std::vector<RoutingDataObjectPtr> RoutingDataObjectVector;
// RoutingLogic.cpp's cases as they were
static intptr_t Orig(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    RoutingOp* RO = reinterpret_cast<RoutingOp*>(b);
    Ref* S = RR(a);
    switch (inOp) {
    case Op::ServerAddGroup: {
        RoutingGroupInfoPtr anInfo = RouteOpRecords(RO).GetGroupInfo();
        return Call(S, MC::ServerAddGroup, (intptr_t)anInfo.get());
    }
    case Op::ServerAddClient: {
        RoutingClientInfoPtr anInfo = RouteOpRecords(RO).GetClientInfo();
        return Call(S, MC::ServerAddClient, (intptr_t)anInfo.get());
    }
    case Op::ServerSyncClients: {
        RoutingClientMap m;
        RouteOpRecords(RO).GetClients(m);
        std::vector<uint16_t> anIds;
        std::vector<intptr_t> anInfos;
        SyncArgs(m, anIds, anInfos);
        Call(S, MC::ServerSyncClients, anIds.empty() ? 0 : (intptr_t)&anIds[0], anInfos.empty() ? 0 : (intptr_t)&anInfos[0], anIds.size());
        return 0;
    }
    case Op::ServerSyncGroups: {
        RoutingGroupMap m;
        RouteOpRecords(RO).GetGroups(m);
        std::vector<uint16_t> anIds;
        std::vector<intptr_t> anInfos;
        SyncArgs(m, anIds, anInfos);
        Call(S, MC::ServerSyncGroups, anIds.empty() ? 0 : (intptr_t)&anIds[0], anInfos.empty() ? 0 : (intptr_t)&anInfos[0],
             (intptr_t)((uint64_t)anIds.size() | (uint64_t)(c != 0) << 32));
        return 0;
    }
    case Op::GameReadSummary: {
        // ReadSummaryBuffer(RO(b)->GetData().get())
        ByteBufferPtr aData = RO->GetData();
        if (aData.get() == NULL) return false;
        return Summary(RR(a), aData->data(), aData->length());
    }
    case Op::RDataObjects: {
        RoutingDataObjectList aList;
        RouteOpRecords(reinterpret_cast<RoutingOp*>(a)).GetDataObjects(aList);
        RoutingDataObjectVector* aVector = new RoutingDataObjectVector(aList.begin(), aList.end());
        *reinterpret_cast<intptr_t*>(b) = (intptr_t)aVector->size();
        return (intptr_t)aVector;
    }
    case Op::DataObjectLink: return (*reinterpret_cast<RoutingDataObjectVector*>(a))[(size_t)b]->mLinkId;
    case Op::GameReadDataObject: {
        const ByteBuffer* aData = (*reinterpret_cast<RoutingDataObjectVector*>(b))[(size_t)c]->mData.get();
        if (aData == NULL) return false;
        return Summary(RR(a), aData->data(), aData->length());
    }
    case Op::ListFree: delete reinterpret_cast<RoutingDataObjectVector*>(a); return 0;
    default: return 0;
    }
}

// ---- the model's host (both worlds): the golden half answers the read ops with the original's statements

static bool sGolden;
static intptr_t ModelHost(EeWonModelOp op, intptr_t a, intptr_t b, intptr_t c)
{
    switch (op) {
    case EeWonModelOp::AddRef: RefAdd(RR(a)); return 0;
    case EeWonModelOp::Release: RefRel(RR(a)); return 0;
    case EeWonModelOp::NewClient: return (intptr_t)Held((int)MK::Client, "client");
    case EeWonModelOp::NewClientList: return (intptr_t)Held((int)MK::ClientList, "list");
    case EeWonModelOp::NewGroup: return (intptr_t)Held((int)MK::Group, "group");
    case EeWonModelOp::NameIgnored: {
        intptr_t anAnswer = (intptr_t)((TextOf(a).size() / 4) % 2);
        Line("ignored? " + H(a) + " -> " + N(anAnswer));
        return anAnswer;
    }
    case EeWonModelOp::CdKeyBlocked: {
        intptr_t anAnswer = (unsigned long)a % 3 == 0;
        Line("blocked? " + N((unsigned long)a) + " -> " + N(anAnswer));
        return anAnswer;
    }
    case EeWonModelOp::ClientReadRouting:
        if (!sGolden) Line("unexpected ClientReadRouting");
        OrigClientRead(RR(a), reinterpret_cast<RoutingClientInfo*>(b));
        return 0;
    case EeWonModelOp::GroupReadRouting:
        if (!sGolden) Line("unexpected GroupReadRouting");
        OrigGroupRead(RR(a), reinterpret_cast<RoutingGroupInfo*>(b), RR(c));
        return 0;
    case EeWonModelOp::TextOfUnits:
        if (sGolden) Line("unexpected TextOfUnits");
        return (intptr_t)Text(Units16(reinterpret_cast<const uint16_t*>(a), (size_t)b));
    case EeWonModelOp::GameReadSummaryBytes:
        if (sGolden) Line("unexpected GameReadSummaryBytes");
        return Summary(RR(a), reinterpret_cast<const char*>(b), (unsigned long)c);
    default: Line("model op " + N((int)op)); return 0;
    }
}

// ---- the dump: the server's clients, rooms and members, the population, every live count

static void DumpClient(const char* inTag, intptr_t inKey, Ref* c)
{
    Line(std::string("D ") + inTag + " " + N(inKey) + " " + H((intptr_t)c) + " id=" + N(Get(c, MF::ClientId)) + " name=" +
         TextOf(Get(c, MF::ClientName)) + " flags=" + N((unsigned long)Get(c, MF::ClientFlags)) + " won=" +
         N((unsigned long)Get(c, MF::ClientWonUserId)) + " cd=" + N((unsigned long)Get(c, MF::ClientCdKeyId)) + " ign=" +
         N(Get(c, MF::ClientIgnored)) + " blk=" + N(Get(c, MF::ClientBlocked)) + " chat=" + N(Get(c, MF::ClientChatGroupId)) +
         " game=" + N(Get(c, MF::ClientGameGroupId)) + " cm=" + N(Get(c, MF::ClientChatMemberFlags)) + " gm=" +
         N(Get(c, MF::ClientGameMemberFlags)) + " cc=" + N(Get(c, MF::ClientChatCaptain)) + " gc=" +
         N(Get(c, MF::ClientGameCaptain)));
}
static void Dump(Ref* inServer)
{
    Ref* aClients = RR(Get(inServer, MF::ServerClients));
    intptr_t n = Call(aClients, MC::ListCount);
    Line("D clients " + N(n));
    for (intptr_t i = 0; i < n; i++) {
        Ref* c = RR(Call(aClients, MC::ListAt, i));
        DumpClient("c", Get(c, MF::ClientId), c);
    }
    intptr_t g = Call(inServer, MC::ServerGroupCount);
    Line("D groups " + N(g));
    for (intptr_t i = 0; i < g; i++) {
        Ref* r = RR(Call(inServer, MC::ServerGroupAt, i));
        Ref* m = RR(Get(r, MF::GroupClients));
        intptr_t k = Call(m, MC::ListCount);
        std::string aMembers;
        for (intptr_t j = 0; j < k; j++) aMembers += " " + H(Call(m, MC::ListAt, j));
        Line("D g " + H((intptr_t)r) + " id=" + N(Get(r, MF::GroupId)) + " name=" + TextOf(Get(r, MF::GroupName)) + " num=" +
             N((unsigned long)Get(r, MF::GroupNumClients)) + " flags=" + N((unsigned long)Get(r, MF::GroupFlags)) + " cap=" +
             N(Get(r, MF::GroupCaptainId)) + " max=" + N((unsigned long)Get(r, MF::GroupMaxClients)) + " members" +
             N(k) + ":" + aMembers);
    }
    Line("D pop chat=" + N(Get(inServer, MF::ServerNumChat)) + " total=" + N(Get(inServer, MF::ServerNumClients)));
    std::string aCounts = "D rc";
    for (std::map<int, Ref*>::iterator it = sW->mRefs.begin(); it != sW->mRefs.end(); ++it)
        aCounts += " h" + N(it->first) + "=" + N(it->second->mRc);
    Line(aCounts);
}

// ---- the generated server messages

struct W {
    std::string b;
    W& B(int v) { b += (char)(v & 0xff); return *this; }
    W& S(int v) { B(v); B(v >> 8); return *this; }
    W& L(unsigned long v) { S((int)(v & 0xffff)); S((int)(v >> 16)); return *this; }
    W& Wide(int inCountBytes, const std::vector<uint16_t>& inUnits)
    {
        if (inCountBytes == 2) S((int)inUnits.size()); else B((int)inUnits.size());
        for (size_t i = 0; i < inUnits.size(); i++) S(inUnits[i]);
        return *this;
    }
    W& N1(const std::string& s) { B((int)s.size()); b += s; return *this; }
    W& Buf2(const std::string& d) { S((int)d.size()); b += d; return *this; }
    W& Entry(const W& e) { S((int)e.b.size()); b += e.b; return *this; }
};
static W Msg(int inType) { W w; w.B(3).B(7).B(inType); return w; }
static std::vector<uint16_t> Name()
{
    std::vector<uint16_t> u;
    unsigned n = R(5);
    for (unsigned i = 0; i < n; i++) u.push_back(R(8) == 0 ? (uint16_t)0x263a : (uint16_t)('a' + R(5)));
    return u;
}
static int Id() { return 1 + (int)R(6); }
static unsigned long Flags() { static const unsigned long k[] = {0, 1, 4, 0x20, 0x800, 0x801, 0x810, 0x1004, 0x40000, 0xffffffff}; return k[R(10)]; }
static std::string Bytes()
{
    std::string d;
    unsigned n = R(5);
    for (unsigned i = 0; i < n; i++) d += (char)R(256);
    return d;
}
// the extended client's extras: the WON user id (1), the CD key id (2), another (9)
static void Extras(W& w)
{
    unsigned k = R(4);
    w.B((int)k);
    for (unsigned i = 0; i < k; i++) {
        unsigned t = R(3);
        if (t == 0) w.B(1).B(4).L(1000 + R(30000));
        else if (t == 1) w.B(2).B(4).L(R(40));
        else w.B(9).B(1).B(R(256));
    }
}
static void Cut(W& w)
{
    if (R(10) == 0 && w.b.size() > 3) w.b.resize(3 + R((unsigned)w.b.size() - 3));
}
struct Made { int mType; std::string mBytes; };
// a routing op of the step's kind and the message it was fed
static Made MakeMessage(int inKind)
{
    Made m;
    W w;
    switch (inKind) {
    case 0: { // the group created notice
        m.mType = RoutingOp_GroupCreated;
        w = Msg(32);
        w.S(Id()).Wide(2, Name()).S(R(20)).L(Flags()).L(Flags());
        break;
    }
    case 1: { // the client joined server notice, plain or extended
        m.mType = RoutingOp_ClientJoinedServer;
        w = Msg(8);
        w.S(Id()).Wide(2, Name()).L(Flags());
        if (R(2)) Extras(w);
        break;
    }
    case 2: { // a client list, plain (23) or extended (74)
        m.mType = RoutingOp_GetClientList;
        bool ex = R(2) != 0;
        w = Msg(ex ? 74 : 23);
        if (R(12) == 0) { w.S(-1); break; }
        unsigned n = R(5);
        w.S(0).S((int)n);
        for (unsigned i = 0; i < n; i++) {
            w.S(Id()).Wide(2, Name()).L(Flags());
            if (ex) Extras(w);
        }
        break;
    }
    case 3: { // a group list (all fields: flags 0xff), its members
        m.mType = RoutingOp_GetGroupList;
        w = Msg(25);
        if (R(12) == 0) { w.S(-3); break; }
        unsigned n = R(4);
        w.S(0).S(0xff).S((int)n);
        for (unsigned i = 0; i < n; i++) {
            W g;
            unsigned k = R(4);
            g.S(Id() * 10).Wide(2, Name()).S(Id()).S(R(60)).L(Flags()).L(Flags()).S(R(8)).S((int)k);
            for (unsigned j = 0; j < k; j++) g.S(Id()).B(R(8));
            w.Entry(g);
        }
        break;
    }
    case 4: { // data objects read
        m.mType = RoutingOp_ReadDataObject;
        w = Msg(49);
        if (R(12) == 0) { w.S(-4); break; }
        unsigned n = R(4);
        w.S(0).S((int)n);
        for (unsigned i = 0; i < n; i++) w.S(Id() * 10).N1(R(2) ? "Game" : "G").Wide(1, Name()).Buf2(Bytes());
        break;
    }
    case 5: { // a peer's data (the op's own buffer)
        m.mType = RoutingOp_PeerData;
        w = Msg(48);
        w.S(Id()).Buf2(Bytes());
        break;
    }
    default: { // a bad user list, every optional field or none
        m.mType = RoutingOp_GetBadUserList;
        w = Msg(21);
        bool all = R(2) != 0;
        unsigned n = R(4);
        w.S(0).B(1 + R(2)).S(100).B(all ? 0x0f : 0).S((int)n);
        for (unsigned i = 0; i < n; i++) {
            W u;
            u.L(5000 + R(100)).Wide(2, Name());
            if (all) u.L(R(4) == 0 ? 0 : R(200000)).L(R(100)).Wide(2, Name()).Wide(2, Name());
            w.Entry(u);
        }
        break;
    }
    }
    Cut(w);
    m.mBytes = w.b;
    return m;
}
static RoutingOp* FeedOp(const Made& inMade)
{
    RoutingOp* anOp = new RoutingOp(NULL, inMade.mType);
    anOp->AddRef();
    EeWonRouteOpRead aRead;
    ee_won_routeop_reply(anOp->mRust, (uint8_t)inMade.mBytes[2], reinterpret_cast<const uint8_t*>(inMade.mBytes.data()),
                         inMade.mBytes.size(), 3, true, &aRead);
    return anOp;
}

// ---- the bad user rows: BadUserItem's reads of a RoutingBadUser copy against the record's

static std::string BadLine(const std::wstring& inName, unsigned long inId, unsigned long inExp, unsigned long inMod,
                           const std::wstring& inModName, const std::wstring& inComment)
{
    return "bad name=" + Units(inName) + " id=" + N(inId) + " exp=" + N(inExp) + " mod=" + N(inMod) + " modname=" +
           Units(inModName) + " comment=" + Units(inComment);
}
static void OnBad(void* inLines, const EeWonRouteRecord* r)
{
    std::wstring aName, aModName, aComment;
    WonWideSink(&aName, r->name, r->name_len);
    WonWideSink(&aModName, r->moderator_name, r->moderator_name_len);
    WonWideSink(&aComment, r->comment, r->comment_len);
    static_cast<std::vector<std::string>*>(inLines)->push_back(
        BadLine(aName, r->user_id, r->expiration, r->moderator_id, aModName, aComment));
}

// ---- a sequence in one world

static bool sMutate;
static World sWorlds[2];
static Ref* sServers[2];
static Ref* sGames[2];
static void Begin(int inWorld)
{
    sW = &sWorlds[inWorld];
    sGolden = inWorld == 0;
}
static intptr_t Port(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    intptr_t anOut = 0;
    if (!ee_won_model_route_record_op((uint32_t)inOp, a, b, c, &anOut)) Line("not answered " + N((int)inOp));
    return anOut;
}
static intptr_t Do(Op inOp, intptr_t a, intptr_t b, intptr_t c)
{
    return sGolden ? Orig(inOp, a, b, c) : Port(inOp, a, b, c);
}
static void Step(int inWorld, int inKind, RoutingOp* inOp, unsigned inFlag)
{
    Begin(inWorld);
    Ref* S = sServers[inWorld];
    Ref* G = sGames[inWorld];
    intptr_t r = 0;
    switch (inKind) {
    case 0: r = Do(Op::ServerAddGroup, (intptr_t)S, (intptr_t)inOp, 0); Line("=> " + H(r)); break;
    case 1: r = Do(Op::ServerAddClient, (intptr_t)S, (intptr_t)inOp, 0); Line("=> " + H(r)); break;
    case 2: Do(Op::ServerSyncClients, (intptr_t)S, (intptr_t)inOp, 0); break;
    case 3: Do(Op::ServerSyncGroups, (intptr_t)S, (intptr_t)inOp, inFlag); break;
    case 4: {
        intptr_t aCount = 0;
        intptr_t aList = Do(Op::RDataObjects, (intptr_t)inOp, (intptr_t)&aCount, 0);
        Line("objects " + N(aCount));
        for (intptr_t i = 0; i < aCount; i++) {
            Line("link " + N(Do(Op::DataObjectLink, aList, i, 0)));
            Line("=> " + N(Do(Op::GameReadDataObject, (intptr_t)G, aList, i)));
        }
        Do(Op::ListFree, aList, 1, 0);
        break;
    }
    case 5: Line("=> " + N(Do(Op::GameReadSummary, (intptr_t)G, (intptr_t)inOp, 0))); break;
    default: {
        std::vector<std::string> aLines;
        if (sGolden) {
            RoutingBadUserList aList;
            RouteOpRecords(inOp).GetBadUsers(aList);
            for (RoutingBadUserList::iterator i = aList.begin(); i != aList.end(); ++i)
                aLines.push_back(BadLine(i->mName, i->mWONId, i->mExpirationDiff, i->mModeratorWONId, i->mModeratorName,
                                         i->mModeratorComment));
        } else {
            ee_won_routeop_records(inOp->mRust, EE_WON_RTR_BAD_USER, OnBad, &aLines);
        }
        for (size_t i = 0; i < aLines.size(); i++) Line(aLines[i]);
        break;
    }
    }
    Dump(S);
}
static void Setup(int inWorld)
{
    Begin(inWorld);
    sW->mRefs.clear();
    sW->mRefOf.clear();
    sW->mTrace.clear();
    sW->mNext = 1;
    sServers[inWorld] = Held((int)MK::Server, "server");
    sGames[inWorld] = Held((int)MK::Game, "game");
}
static void Teardown(int inWorld)
{
    Begin(inWorld);
    RefRel(sGames[inWorld]);
    RefRel(sServers[inWorld]);
    Line("left " + N((long long)sW->mRefs.size()));
}

// the sequence's lines (the golden half's) and whether both halves agree
static bool Sequence(int inSeq, std::vector<std::string>& outLines)
{
    sSeed = 0x5eed0000u + (unsigned)inSeq * 7919u;
    Setup(0);
    Setup(1);
    std::vector<std::string> aHead;
    aHead.push_back("seq " + N(inSeq));
    unsigned aSteps = 1 + R(12);
    std::vector<std::string> aWant, aGot;
    for (int w = 0; w < 2; w++) {
        std::vector<std::string>& t = sWorlds[w].mTrace;
        (w == 0 ? aWant : aGot).insert((w == 0 ? aWant : aGot).end(), t.begin(), t.end());
        t.clear();
    }
    outLines = aHead;
    outLines.insert(outLines.end(), aWant.begin(), aWant.end());
    for (unsigned s = 0; s < aSteps; s++) {
        // the clients and rooms ops most often, so the merges meet earlier records
        static const int kKinds[] = {0, 1, 2, 2, 3, 3, 3, 4, 5, 6};
        int aKind = kKinds[R(10)];
        Made aMade = MakeMessage(aKind);
        unsigned aFlag = R(2);
        RoutingOp* anOp = FeedOp(aMade);
        std::string aStep = "step " + N(aKind) + " " + N(aMade.mType) + " " + N(aFlag) + " " + Hex(aMade.mBytes.data(), aMade.mBytes.size());
        outLines.push_back(aStep);
        aWant.push_back(aStep);
        aGot.push_back(aStep);
        for (int w = 0; w < 2; w++) {
            Step(w, aKind, anOp, aFlag);
            std::vector<std::string>& t = sWorlds[w].mTrace;
            (w == 0 ? aWant : aGot).insert((w == 0 ? aWant : aGot).end(), t.begin(), t.end());
            if (w == 0) outLines.insert(outLines.end(), t.begin(), t.end());
            t.clear();
        }
        anOp->Release();
    }
    for (int w = 0; w < 2; w++) {
        Teardown(w);
        std::vector<std::string>& t = sWorlds[w].mTrace;
        (w == 0 ? aWant : aGot).insert((w == 0 ? aWant : aGot).end(), t.begin(), t.end());
        if (w == 0) outLines.insert(outLines.end(), t.begin(), t.end());
        t.clear();
    }
    bool aSame = aWant == aGot;
    sChecks++;
    if (!aSame && !sMutate) {
        sFailures++;
        if (sFailures < 8) {
            printf("FAIL sequence %d\n", inSeq);
            for (size_t i = 0; i < aWant.size() || i < aGot.size(); i++) {
                std::string a = i < aWant.size() ? aWant[i] : "<none>", b = i < aGot.size() ? aGot[i] : "<none>";
                if (a != b) { printf(" line %u:\n  want %s\n   got %s\n", (unsigned)i, a.c_str(), b.c_str()); break; }
            }
        }
    }
    return aSame;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    sMutate = argc > 1 && strcmp(argv[1], "--mutate") == 0;
    // the routing classes' core table is bound by the first connection made
    SmartPtr<RoutingConnection> aConnection = new RoutingConnection;
    static World sEmptyWorld;
    sW = &sEmptyWorld;
    sEmpty = NewRef(kText, "e");
    sEmpty->mRc = 1000000;
    const int kSequences = 30000;
    std::set<std::string> aSeen;
    size_t aLines = 0;
    int aDiffering = 0;
    for (int s = 0; s < kSequences; s++) {
        std::vector<std::string> aOut;
        if (!Sequence(s, aOut)) aDiffering++;
        aLines += aOut.size();
        bool aNew = s < 40;
        for (size_t i = 0; i < aOut.size(); i++) {
            const std::string& l = aOut[i];
            std::string aKind = l.substr(0, l.find(' ', l.compare(0, 2, "D ") == 0 ? 2 : 0));
            if (l.compare(0, 5, "step ") == 0) aKind = l.substr(0, 7);
            if (aSeen.insert(aKind).second) aNew = true;
        }
        if (aPrint && aNew)
            for (size_t i = 0; i < aOut.size(); i++) printf("%s\n", aOut[i].c_str());
        CHECK(aOut.back() == "left 0");
    }
    if (sMutate) {
        printf("won_routemodel_test --mutate: %d of %d sequences differ\n", aDiffering, kSequences);
        return 0;
    }
    if (!aPrint)
        printf("won_routemodel_test: %d sequences, %u golden lines, %u line kinds, %d checks, %d failures\n", kSequences,
               (unsigned)aLines, (unsigned)aSeen.size(), sChecks, sFailures);
    return aPrint ? 0 : sFailures;
}
