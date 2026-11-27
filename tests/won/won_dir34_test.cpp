// R6 WON round 34: the directory's entities whole in Rust (ee_won_net dirlist.rs, ee_won_lobby dirread.rs; the C++
// DirList of ee/EEWonNet.h) against the deleted C++ as it was at ab1b33bcb (won_direntity_orig.h: DirEntity,
// DirMakeEntity, GetNetAddrAsIP, TakeEntity; the statements below: ServerContext's AddAddressesFromDir,
// AtomicClearAndAddAddressesFromDir and DirAddresses, WONPing.h's MultiPingAddServers, LobbyModelHost.cpp's
// LobbyServerList::UpdateFromDirectory and LobbyServer::ReadDirEntity over a logging stand-in model). Each phase
// draws random listings (kinds, names, addresses of every length, data objects) and compares what each side gives:
// - entity: each entity's type, name, display name, path and address (the DirList reads);
// - context: a server list's addresses and the answer after AddAddressesFromDir / AtomicClear.. with every filter;
// - ping: the multi-ping op's servers after AddServers;
// - model: the trace of the model's calls the update makes (servers made, the connected left, names, addresses,
//   populations, address names);
// - request: RoutingRequestValues (whole in Rust: routereq.rs) against the class as it was, over random chains of
//   numbers, texts, data types, buffers and recipient lists (the values a request is made of).
// EE_W34_MUTATE=n changes the original side in one way (1..10): the run must then report mismatches.
// The exit code is the number of mismatching cases.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBuffer.h"
#include "rust/won_net/status_codes.h"
#include "rust/won_net/dir.h"
#include "rust/won_net/dirlist.h"
#include "rust/won_net/ffi_parse.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/multiping.h"
#include "rust/won_net/servctx.h"
#include "rust/won_net/routereq.h"
#include "rust/won_net/admin.h"
#include "rust/won_lobby/dirread.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/modelobj.h"
#include "won_direntity_orig.h"

using namespace WONAPI;

static int sMutate;
static bool Mutate(int n) { return sMutate == n; }
static unsigned sRand = 4321;
static unsigned R(unsigned n) { sRand = sRand * 1103515245u + 12345u; return n ? (sRand >> 8) % n : 0; }
static std::string N(long long v) { return std::to_string(v); }
static std::string W(const std::wstring& w) { std::string s; for (wchar_t c : w) s += N((long long)c) + ","; return s; }
static std::string A(const IPAddr& a) { return (a.IsValid() ? "valid " : "invalid ") + a.GetHostAndPortString(); }

static int sCases, sFailures;
static std::map<std::string, int> sFailedPhases;
static void Compare(const char* inPhase, const std::string& inOrig, const std::string& inNew, const std::string& inWhat)
{
    sCases++;
    if (inOrig == inNew) return;
    sFailedPhases[inPhase]++;
    if (++sFailures <= 10)
        printf("MISMATCH %s %s\n  orig %s\n  new  %s\n", inPhase, inWhat.c_str(), inOrig.c_str(), inNew.c_str());
}

// ---- a random listing: the records, the orig list and the Rust list made of them
struct Gen {
    std::vector<std::vector<uint16_t> > mTexts;
    std::vector<std::string> mBytes;
    std::vector<EeWonDirDataObject> mObjects[8];
    std::vector<EeWonDirEntity> mEntities;
};
static const wchar_t* const kNames[] = {L"Alpha", L"alpha", L"TitanRoutingServer", L"", L"B"};
static std::vector<uint16_t> Units(const wchar_t* inText)
{
    std::vector<uint16_t> u;
    for (const wchar_t* p = inText; *p; p++) u.push_back((uint16_t)*p);
    return u;
}
static std::string Six()
{
    std::string s;
    unsigned n = R(4) == 0 ? R(9) : 6 + R(2);
    static const unsigned char kHosts[][4] = {{10, 0, 0, 1}, {127, 0, 0, 1}, {192, 168, 1, 5}, {0, 0, 0, 0}};
    const unsigned char* h = kHosts[R(4)];
    unsigned char b[8] = {(unsigned char)R(3), (unsigned char)(R(2) ? 0x3b : 0x90), h[0], h[1], h[2], h[3], (unsigned char)R(256), 9};
    for (unsigned i = 0; i < n; i++) s += (char)b[i];
    return s;
}
static std::string ObjectData(const std::string& inType)
{
    static const char* const kLists[] = {"one, two", "x", "\"q,r\", s", "a,,b", " lead , trail ", "", "a\0b, c", "1,2,3,4"};
    if (inType == "_L") { int i = R(8); return i == 6 ? std::string("a\0b, c", 6) : std::string(kLists[i]); }
    std::string s;
    for (unsigned i = 0, n = R(2) ? 2 : R(5); i < n; i++) s += (char)R(256);
    return s;
}
static void Make(Gen& g, unsigned inCount)
{
    static const char* const kTypes[] = {"_C", "_L", "_X", "", "_CC"};
    g.mTexts.clear(); g.mBytes.clear(); g.mEntities.clear();
    g.mTexts.reserve(64); g.mBytes.reserve(64);
    for (unsigned q = 0; q < inCount && q < 8; q++) {
        EeWonDirEntity e;
        memset(&e, 0, sizeof(e));
        e.fields = R(4) == 0 ? R(0x1000) : (0x2f | R(0x1000));
        e.kind = R(4) == 0 ? 'D' : (R(3) == 0 ? (uint8_t)R(256) : 'S');
        g.mTexts.push_back(Units(kNames[R(5)]));
        e.name = g.mTexts.back().data(); e.name_len = g.mTexts.back().size();
        g.mTexts.push_back(Units(kNames[R(5)]));
        e.display_name = g.mTexts.back().data(); e.display_name_len = g.mTexts.back().size();
        g.mTexts.push_back(Units(kNames[R(5)]));
        e.path = g.mTexts.back().data(); e.path_len = g.mTexts.back().size();
        g.mBytes.push_back(Six());
        e.net_addr = reinterpret_cast<const uint8_t*>(g.mBytes.back().data()); e.net_addr_len = g.mBytes.back().size();
        g.mObjects[q].clear();
        for (unsigned k = 0, m = R(4); k < m; k++) {
            g.mBytes.push_back(kTypes[R(5)]);
            const std::string& t = g.mBytes.back();
            EeWonDirDataObject o;
            o.data_type = reinterpret_cast<const uint8_t*>(t.data()); o.data_type_len = t.size();
            g.mBytes.push_back(ObjectData(t));
            o.has_data = t == "_L" || R(4) != 0; // an _L without data: the original read NULL's bytes
            o.data = reinterpret_cast<const uint8_t*>(g.mBytes.back().data()); o.data_len = g.mBytes.back().size();
            g.mObjects[q].push_back(o);
        }
        e.objects = g.mObjects[q].data(); e.object_count = g.mObjects[q].size();
        g.mEntities.push_back(e);
    }
}
static DirEntityList OrigList(const Gen& g)
{
    DirEntityList l;
    for (const EeWonDirEntity& e : g.mEntities) DirEntityReplyParser::TakeEntity(&l, &e);
    return l;
}
static DirList NewList(const Gen& g)
{
    DirList l;
    for (const EeWonDirEntity& e : g.mEntities) ee_won_dirlist_push(l.get(), &e);
    return l;
}

// ---- the entities: what the DirList reads against the DirEntity's members
static void EntityPhase(int inRuns)
{
    for (int q = 0; q < inRuns; q++) {
        Gen g;
        Make(g, 1 + R(8));
        DirEntityList o = OrigList(g);
        DirList n = NewList(g);
        std::string so, sn = "count " + N((long long)n.size());
        so = "count " + N((long long)o.size());
        size_t i = 0;
        for (DirEntityList::const_iterator it = o.begin(); it != o.end(); ++it, ++i) {
            const DirEntity* e = it->get();
            IPAddr a = e->GetNetAddrAsIP();
            if (Mutate(3) && e->mNetAddr.get() != NULL && e->mNetAddr->length() == 6) a = IPAddr();
            so += " | type " + N(e->mType) + " name " + W(e->mName) + " display " + W(e->mDisplayName) + " path " + W(e->mPath) + " " + A(a);
            std::wstring aPath;
            ee_won_dirlist_text(n.get(), i, EeWonDirText::Path, WonWideSink, &aPath);
            sn += " | type " + N((char)ee_won_dirlist_kind(n.get(), i)) + " name " + W(n.Name(i)) + " display " + W(n.DisplayName(i)) +
                  " path " + W(aPath) + " " + A(n.Addr(i));
        }
        Compare("entity", so, sn, "run " + N(q));
    }
}

// ---- ServerContext's directory calls (WONRouting.h at ab1b33bcb), over the Rust server list as the class was
static bool OrigAdd(EeWonServerList* inList, const IPAddr& inAddress) { EeWonAddr a = inAddress.ToRaw(); return ee_won_ctx_add(inList, &a); }
static std::vector<IPAddr> OrigDirAddresses(const DirEntityList& inDir, const wchar_t* inNameFilter)
{
    std::wstring aFilter;
    if (inNameFilter != NULL) aFilter = inNameFilter;
    std::vector<IPAddr> anAddrs;
    for (DirEntityList::const_iterator i = inDir.begin(); i != inDir.end(); ++i) {
        DirEntity* e = i->get();
        if ((!Mutate(1) && e->mType == 'D') || (!Mutate(2) && !aFilter.empty() && !(aFilter == e->mName))) continue;
        anAddrs.push_back(e->GetNetAddrAsIP());
    }
    return anAddrs;
}
static bool OrigAddAddressesFromDir(EeWonServerList* inList, const DirEntityList& inDir, const wchar_t* inNameFilter)
{
    std::vector<IPAddr> anAddrs = OrigDirAddresses(inDir, inNameFilter);
    bool added = false;
    for (size_t i = 0; i < anAddrs.size(); i++) if (OrigAdd(inList, anAddrs[i])) added = true;
    return added;
}
static bool OrigAtomicClearAndAddAddressesFromDir(EeWonServerList* inList, const DirEntityList& inDir, const wchar_t* inNameFilter)
{
    std::vector<IPAddr> anAddrs = OrigDirAddresses(inDir, inNameFilter);
    std::vector<EeWonAddr> aRaw;
    for (size_t i = 0; i < anAddrs.size(); i++) aRaw.push_back(anAddrs[i].ToRaw());
    if (Mutate(7)) { bool b = false; for (size_t i = 0; i < aRaw.size(); i++) b |= ee_won_ctx_add(inList, &aRaw[i]); return b; }
    return ee_won_ctx_replace(inList, aRaw.empty() ? NULL : &aRaw[0], aRaw.size());
}
static void DumpAddr(void* inOut, const EeWonAddr* inRaw)
{
    IPAddr a;
    a.FromRaw(*inRaw);
    *static_cast<std::string*>(inOut) += " " + A(a);
}
static std::string Dump(EeWonServerList* inList)
{
    std::string s = N((long long)ee_won_ctx_len(inList));
    ee_won_ctx_copy_sorted(inList, DumpAddr, &s);
    return s;
}
static void ContextPhase(int inRuns)
{
    static const wchar_t* const kFilters[] = {NULL, L"", L"Alpha", L"B", L"Gamma"};
    for (int q = 0; q < inRuns; q++) {
        Gen g;
        Make(g, R(9));
        DirEntityList o = OrigList(g);
        DirList n = NewList(g);
        const wchar_t* f = kFilters[R(5)];
        bool atomic = R(2) != 0;
        EeWonServerList* lo = ee_won_ctx_new();
        EeWonServerList* ln = ee_won_ctx_new();
        for (unsigned k = 0, m = R(3); k < m; k++) {
            IPAddr a((long)(0x0a000001 + R(3)), (unsigned short)(15100 + R(2)));
            OrigAdd(lo, a);
            OrigAdd(ln, a);
        }
        bool ro = atomic ? OrigAtomicClearAndAddAddressesFromDir(lo, o, f) : OrigAddAddressesFromDir(lo, o, f);
        bool rn = atomic ? ee_won_ctx_replace_from_dir(ln, n.get(), f, f != NULL ? wcslen(f) : 0, sizeof(wchar_t))
                         : ee_won_ctx_add_from_dir(ln, n.get(), f, f != NULL ? wcslen(f) : 0, sizeof(wchar_t));
        Compare("context", N(ro) + " " + Dump(lo), N(rn) + " " + Dump(ln), "run " + N(q) + (atomic ? " atomic" : " add"));
        ee_won_ctx_free(lo);
        ee_won_ctx_free(ln);
    }
    // the class itself over the Rust list (one statement each): the same as the call above
    for (int q = 0; q < inRuns / 10; q++) {
        Gen g;
        Make(g, R(9));
        DirEntityList o = OrigList(g);
        SmartPtr<ServerContext> c = new ServerContext;
        EeWonServerList* lo = ee_won_ctx_new();
        bool rn = q % 2 ? c->AtomicClearAndAddAddressesFromDir(NewList(g)) : c->AddAddressesFromDir(NewList(g));
        bool ro = q % 2 ? OrigAtomicClearAndAddAddressesFromDir(lo, o, NULL) : OrigAddAddressesFromDir(lo, o, NULL);
        Compare("context", N(ro) + " " + Dump(lo), N(rn) + " " + Dump(c->GetListObject()), "class run " + N(q));
        ee_won_ctx_free(lo);
    }
}

// ---- MultiPingOp's AddServers (WONPing.h at ab1b33bcb) on a multi-ping op
static void OrigAddServers(EeWonHostOp* inOp, const DirEntityList& inDir, const wchar_t* inNameFilter)
{
    std::wstring aFilter = inNameFilter != NULL ? inNameFilter : L"";
    for (DirEntityList::const_iterator i = inDir.begin(); i != inDir.end(); ++i) {
        const DirEntity* anEntity = *i;
        if (anEntity->mType == 'D' || (!aFilter.empty() && aFilter != anEntity->mName)) continue;
        IPAddr anAddr = anEntity->GetNetAddrAsIP();
        if (Mutate(8) || anAddr.IsValid())
            ee_won_hostop_set(inOp, EE_WON_MULTIPING_ADD, 0, NULL, 0, reinterpret_cast<const uint8_t*>(anAddr.GetSixByte()),
                              NULL); // (the original handed the entity: the Rust list has no C++ object, the element none)
    }
}
static void TextSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
// the op's pending list (its describe line: the elements' addresses, times, failures)
static std::string Pings(EeWonHostOp* inOp)
{
    std::string s;
    ee_won_hostop_describe(inOp, TextSink, &s);
    return s;
}
static void PingPhase(int inRuns)
{
    static const wchar_t* const kFilters[] = {NULL, L"", L"Alpha", L"B"};
    for (int q = 0; q < inRuns; q++) {
        Gen g;
        Make(g, R(9));
        DirEntityList o = OrigList(g);
        DirList n = NewList(g);
        const wchar_t* f = kFilters[R(4)];
        EeWonHostOp* po = ee_won_multiping_new();
        EeWonHostOp* pn = ee_won_multiping_new();
        OrigAddServers(po, o, f);
        ee_won_multiping_add_servers(pn, n.get(), f, f != NULL ? wcslen(f) : 0, sizeof(wchar_t));
        Compare("ping", Pings(po), Pings(pn), "run " + N(q));
        ee_won_hostop_free(po);
        ee_won_hostop_free(pn);
    }
}

// ---- the model's directory reads (LobbyModelHost.cpp at ab1b33bcb) over a logging stand-in
static std::string sTrace;
static std::map<std::string, int> sConnected; // the servers' answers by address
typedef EeWonModelField LobbyMF;
typedef EeWonModelCall LobbyMC;
struct GUIString { std::wstring mText; GUIString(const std::wstring& t) : mText(t) {} };
// the original's StringParser as it is (ee/wonapi/WONLobbyParser.h: the narrow reads over ee_won's parser)
class StringParser {
public:
    StringParser(const char* inText) : mNarrow(inText), mPos(0) {}
    bool CheckNextChar(int inChar) { return ee_won_parser_check_next_char(this->mNarrow, 1, &this->mPos, inChar); }
    bool ReadString(std::string& outString, bool inStopAtWhitespace)
    {
        outString.erase();
        ee_won_parser_read_string(this->mNarrow, 1, &this->mPos, inStopAtWhitespace, AppendBytes, &outString);
        return true;
    }
    bool ReadValue(std::list<std::string>& theList)
    {
        do { theList.push_back(std::string()); this->ReadString(theList.back(), false); } while (this->CheckNextChar(','));
        return true;
    }
    const char* mNarrow;
    int mPos;
private:
    static void AppendBytes(void* inOut, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inOut)->append(reinterpret_cast<const char*>(inBytes), inLength); }
};
struct LobbyServer {
    std::string mKey;
    void MSetText(LobbyMF inField, const GUIString& inText) { sTrace += " " + mKey + " text " + N((int)inField) + " " + W(inText.mText); }
    void MSetAddr(LobbyMF inField, const IPAddr& inAddr) { sTrace += " " + mKey + " addr " + N((int)inField) + " " + A(inAddr); }
    intptr_t M(LobbyMC inCall, intptr_t a = 0, intptr_t b = 0)
    {
        std::string s = " " + mKey + " call " + N((int)inCall);
        if (inCall == LobbyMC::ServerAddressAdd) s += " " + std::string(reinterpret_cast<const char*>(a), (size_t)b);
        else s += " " + N((long long)a);
        sTrace += s;
        return 0;
    }
    bool IsConnected() { return sConnected[mKey] != 0; }
    void ReadDirEntity(DirEntity* inEntity);
};
static std::map<std::string, LobbyServer> sServers;
static LobbyServer* Insert(const EeWonIpAddr* inAddr)
{
    IPAddr a = IPAddr::FromValue(ee_won_ipaddr_clone(inAddr));
    std::string k = A(a);
    if (sServers.find(k) == sServers.end()) { sServers[k].mKey = k; sConnected[k] = R(3) == 0; sTrace += " new " + k; }
    sTrace += " insert " + k;
    return &sServers[k];
}
struct LobbyServerList {
    intptr_t M(LobbyMC, intptr_t a) { return (intptr_t)Insert(reinterpret_cast<const EeWonIpAddr*>(a)); }
    void UpdateFromDirectory(const DirEntityList& inEntities);
};
void LobbyServer::ReadDirEntity(DirEntity* inEntity)
{
    this->MSetText(LobbyMF::ServerName, GUIString(inEntity->mDisplayName));
    this->MSetAddr(LobbyMF::ServerAddress, inEntity->GetNetAddrAsIP());
    const DirDataObjectList& aList = inEntity->mDataObjects;
    for (DirDataObjectList::const_iterator anItr = aList.begin(); anItr != aList.end(); ++anItr) {
        const DirDataObject& anObject = *anItr;
        uint16_t aCount = 0;
        const ByteBuffer* anData = anObject.mData.get();
        int aKind = ee_won_model_dir_object(reinterpret_cast<const uint8_t*>(anObject.mDataType.data()),
                                            anObject.mDataType.length(),
                                            anData != NULL ? reinterpret_cast<const uint8_t*>(anData->data()) : NULL,
                                            anData != NULL ? (size_t)anData->length() : 0, &aCount);
        if (aKind == 1) {
            this->M(LobbyMC::ServerSetListed, Mutate(6) ? aCount + 1 : aCount);
        } else if (aKind == 2) {
            this->M(LobbyMC::ServerAddressesClear);
            std::string aText(anObject.mData->data(), anObject.mData->length());
            StringParser aParser(aText.c_str());
            std::list<std::string> aNames;
            aParser.ReadValue(aNames);
            if (Mutate(5) && aNames.size() > 1) aNames.pop_back();
            for (std::list<std::string>::iterator aName = aNames.begin(); aName != aNames.end(); ++aName)
                this->M(LobbyMC::ServerAddressAdd, (intptr_t)aName->data(), (intptr_t)aName->length());
        }
    }
}
void LobbyServerList::UpdateFromDirectory(const DirEntityList& inEntities)
{
    for (DirEntityList::const_iterator it = inEntities.begin(); it != inEntities.end(); ++it) {
        DirEntity* entity = it->get();
        IPAddr anAddress = entity->GetNetAddrAsIP();
        LobbyServer* aServer = (LobbyServer*)this->M(LobbyMC::ServerListInsert, (intptr_t)anAddress.GetValue());
        if (Mutate(4) ? aServer->IsConnected() : !aServer->IsConnected()) aServer->ReadDirEntity(entity);
    }
}
// the host's statements (LobbyModelHost.cpp's DirReadHost) over the same stand-in
static intptr_t DirReadHost(EeWonDirReadOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    typedef EeWonDirReadOp O;
    LobbyServer* aServer = static_cast<LobbyServer*>(inObj);
    switch (inOp) {
    case O::ServerListInsert: return static_cast<LobbyServerList*>(inObj)->M(LobbyMC::ServerListInsert, a);
    case O::ServerConnected: return aServer->IsConnected();
    case O::ServerSetName: { std::wstring t; WonWideSink(&t, reinterpret_cast<const uint16_t*>(a), (size_t)b); aServer->MSetText(LobbyMF::ServerName, GUIString(t)); return 0; }
    case O::ServerSetAddr: aServer->MSetAddr(LobbyMF::ServerAddress, IPAddr::FromValue(ee_won_ipaddr_clone(reinterpret_cast<const EeWonIpAddr*>(a)))); return 0;
    case O::ServerSetListed: aServer->M(LobbyMC::ServerSetListed, a); return 0;
    case O::ServerAddressesClear: aServer->M(LobbyMC::ServerAddressesClear); return 0;
    case O::ServerAddressAdd: aServer->M(LobbyMC::ServerAddressAdd, a, b); return 0;
    }
    return 0;
}
static void ModelPhase(int inRuns)
{
    for (int q = 0; q < inRuns; q++) {
        Gen g;
        Make(g, R(9));
        DirEntityList o = OrigList(g);
        DirList n = NewList(g);
        LobbyServerList aList;
        unsigned aSeed = sRand;
        std::string so, sn;
        sServers.clear(); sConnected.clear(); sTrace.clear();
        aList.UpdateFromDirectory(o);
        so = sTrace;
        sRand = aSeed; // the same connected answers for the new side
        sServers.clear(); sConnected.clear(); sTrace.clear();
        ee_won_lobby_dir_update(&aList, n.get(), DirReadHost);
        sn = sTrace;
        Compare("model", so, sn, "run " + N(q));
    }
}

// ---- RoutingRequestValues (WONRouting.h at ab1b33bcb, Run recording the values instead of requesting)
class OrigRequestValues {
public:
    OrigRequestValues& N(long long inNum) { return this->Add(inNum, std::string()); }
    OrigRequestValues& W(const std::wstring& inText) { std::string b; WideAppendUtf16LE(b, inText); return this->Add(0, b); }
    OrigRequestValues& S(const std::string& inBytes) { return this->Add(0, inBytes); }
    OrigRequestValues& D(const ByteBuffer* inData)
    { return inData == NULL ? this->Add(Mutate(9) ? 1 : 0, std::string()) : this->Add(1, std::string(inData->data(), inData->length())); }
    OrigRequestValues& L(const std::list<unsigned short>& inIds)
    {
        std::string b;
        for (std::list<unsigned short>::const_iterator i = inIds.begin(); i != inIds.end(); ++i)
            b.append(1, (char)(*i & 0xff)).append(1, (char)(Mutate(10) ? 0 : *i >> 8));
        return this->Add(0, b);
    }
    std::string Run()
    {
        std::vector<EeWonAdminValue> v;
        for (std::list<std::pair<long long, std::string> >::iterator i = mValues.begin(); i != mValues.end(); ++i) {
            EeWonAdminValue x = {i->first, reinterpret_cast<const uint8_t*>(i->second.data()), i->second.size()};
            v.push_back(x);
        }
        std::string s;
        for (const EeWonAdminValue& x : v) ValueOut(&s, x.num, x.data, x.len);
        return s;
    }
    static void ValueOut(void* inOut, int64_t inNum, const uint8_t* inData, size_t inLength)
    {
        std::string& s = *static_cast<std::string*>(inOut);
        s += " " + ::N(inNum) + ":";
        for (size_t i = 0; i < inLength; i++) s += ::N(inData[i]) + ".";
    }
private:
    OrigRequestValues& Add(long long inNum, const std::string& inBytes)
    { this->mValues.push_back(std::make_pair(inNum, inBytes)); return *this; }
    std::list<std::pair<long long, std::string> > mValues;
};
static void RequestPhase(int inRuns)
{
    static const wchar_t* const kTexts[] = {L"", L"Room", L"\x00e9t\x00e9", L"a\xd800" L"b", L"\xd83d\xde00"};
    static const char* const kTypes[] = {"", "G", "_C", "\xff\x00x"};
    for (int q = 0; q < inRuns; q++) {
        OrigRequestValues o;
        RoutingRequestValues n;
        for (unsigned k = 0, m = R(9); k < m; k++) {
            switch (R(5)) {
            case 0: { long long v = (long long)R(0x20000) - 0x10000; if (R(4) == 0) v *= 0x100000; o.N(v); n.N(v); break; }
            case 1: { std::wstring t = kTexts[R(5)]; o.W(t); n.W(t); break; }
            case 2: { std::string t = kTypes[R(4)]; o.S(t); n.S(t); break; }
            case 3: {
                if (R(3) == 0) { o.D(NULL); n.D(NULL); break; }
                std::string b;
                for (unsigned i = 0, c = R(6); i < c; i++) b += (char)R(256);
                ByteBufferPtr p = new ByteBuffer(b.data(), (unsigned long)b.size(), false);
                o.D(p.get()); n.D(p.get());
                break;
            }
            default: {
                std::list<unsigned short> ids;
                for (unsigned i = 0, c = R(4); i < c; i++) ids.push_back((unsigned short)(R(2) ? R(0x10000) : R(8)));
                o.L(ids); n.L(ids);
            }
            }
        }
        std::string sn;
        ee_won_routereq_each(n.GetReq(), OrigRequestValues::ValueOut, &sn);
        Compare("request", o.Run(), sn, "run " + N(q));
    }
}

int main(int argc, char** argv)
{
    const char* m = getenv("EE_W34_MUTATE");
    sMutate = m != NULL ? atoi(m) : 0;
    int scale = sMutate ? 1 : 5;
    int c = 0;
    EntityPhase(4000 * scale);
    printf("entity: %d cases\n", sCases - c); c = sCases;
    ContextPhase(4000 * scale);
    printf("context: %d cases\n", sCases - c); c = sCases;
    PingPhase(3000 * scale);
    printf("ping: %d cases\n", sCases - c); c = sCases;
    ModelPhase(4000 * scale);
    printf("model: %d cases\n", sCases - c); c = sCases;
    RequestPhase(4000 * scale);
    printf("request: %d cases\n", sCases - c);
    for (auto& f : sFailedPhases) printf("  %s: %d mismatches\n", f.first.c_str(), f.second);
    printf("won_dir34_test%s: %d cases, %d mismatches\n", sMutate ? (" (mutation " + N(sMutate) + ")").c_str() : "", sCases, sFailures);
    return sFailures;
}
