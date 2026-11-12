// WONMisc's GetVersionOp and FindPatchOp (with VersionBaseOp) as whole ops: the original's statements (copied from
// GetVersionOp.cpp, FindPatchOp.cpp and WONMiscOpsMsg.h's FindPatchMsg before the port, the op a stand-in with
// their members; its directory op's request made as RustServerOp::NewGetDir made it, its entities read from the
// reply by DirEntityReplyParser's statements over ee_won's dir module) against the Rust ops (ee_won verop.rs)
// through their C ABI. Each sequence: generated members (product, extra config, filter string, FindPatchOp's
// version sets), a run (the request), a generated listing reply (directory and service entities with version and
// patch data objects), the members after it, then version checks and patch lists.
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_verop.txt, recorded
// before the port; `verop::tests::golden` replays it). The exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONPatchMsg.h"
#include "won_version_msg.h"
#include "rust/ee_won.h"

#ifndef VEROP_RUST
#define VEROP_RUST 1
#endif

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 2727;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const string& s)
{
    static const char kHex[] = "0123456789abcdef";
    string r;
    for (size_t i = 0; i < s.size(); i++) { r += kHex[(unsigned char)s[i] >> 4]; r += kHex[s[i] & 15]; }
    return r.empty() ? "-" : r;
}
static string Utf16(const wstring& w)
{
    string b;
    WideAppendUtf16LE(b, w);
    return b;
}
static string Num(long long v) { return std::to_string(v); }

// ---- the entities of a listing (DirEntityReplyParser's statements over ee_won's dir module, as won_reqdir_test)

static void DirAssign(wstring& outText, const uint16_t* inUnits, size_t inCount)
{
    string aBytes;
    for (size_t i = 0; i < inCount; i++) {
        aBytes += (char)(inUnits[i] & 0xff);
        aBytes += (char)(inUnits[i] >> 8);
    }
    WideAssignUtf16LE(outText, aBytes.data(), (unsigned long)inCount);
}
static void TakeMulti(void* c, const EeWonDirEntity* inEntity)
{
    DirEntityPtr anEntity = new DirEntity;
    uint32_t f = inEntity->fields;
    if (f & 0x1) anEntity->mType = (char)inEntity->kind;
    if (f & 0x2) DirAssign(anEntity->mPath, inEntity->path, inEntity->path_len);
    if (f & 0x4) DirAssign(anEntity->mName, inEntity->name, inEntity->name_len);
    if (f & 0x8) anEntity->mNetAddr = new ByteBuffer(inEntity->net_addr, (unsigned long)inEntity->net_addr_len, false);
    if (f & 0x20) DirAssign(anEntity->mDisplayName, inEntity->display_name, inEntity->display_name_len);
    for (size_t i = 0; i < inEntity->object_count; i++) {
        const EeWonDirDataObject& o = inEntity->objects[i];
        DirDataObject aData;
        aData.mDataType.assign(reinterpret_cast<const char*>(o.data_type), o.data_type_len);
        if (o.has_data) aData.mData = new ByteBuffer(o.data, (unsigned long)o.data_len, false);
        anEntity->mDataObjects.push_back(aData);
    }
    static_cast<DirEntityList*>(c)->push_back(anEntity);
}

// ---- the original's statements over a stand-in with the ops' members

typedef std::set<string> VersionSet;
typedef std::map<string, string> VersionDescriptionMap;
enum VersionResult { Version_Invalid, Version_Valid_NotLatest, Version_Valid_Latest };
typedef std::map<string, ByteBufferPtr> PatchDataMap;
struct PatchInfo {
    string mFromVersion;
    string mToVersion;
    wstring mName;
    unsigned long mChecksum;
    unsigned long mSize;
    ByteBufferPtr mPatchLocation;
    PatchDataMap mData;
    PatchInfo() : mChecksum(0), mSize(0) {}
};
typedef std::list<PatchInfo> PatchList;

static string CString(const ByteBuffer* b)
{
    const char* d = b->data();
    if (d == NULL) return string();
    size_t n = 0;
    while (n < b->length() && d[n] != 0) n++;
    return string(d, n);
}

struct Op {
    typedef int (*VersionCompare)(const string& s1, const string& s2);
    bool mFind;
    string mProduct, mExtraConfig;
    typedef std::set<string> FilterStringSet;
    FilterStringSet mFilterStringSet;
    VersionSet mValidVersionSet, mFilteredVersionSet;
    string mHighestValidVersion;
    VersionCompare mVersionCompare;
    VersionDescriptionMap mVersionDescriptionMap, mFilteredVersionDescriptionMap;
    PatchList mPatchList;
    static int DefaultVersionCompare(const string& s1, const string& s2) { return s1.compare(s2); }
    Op() : mFind(false) { this->mVersionCompare = DefaultVersionCompare; }

    wstring GetVersionDir()
    {
        wstring aPath = L"/" + StringToWString(this->mProduct) + L"/Version";
        if(!this->mExtraConfig.empty())
            aPath += L"/" + StringToWString(this->mExtraConfig);
        return aPath;
    }
    bool IsFiltered(const string&, const string& theVersionData) { return PatchIsFiltered(this->mFilterStringSet,theVersionData); }
    void BuildValidVersionSet(const DirEntity* theEntity)
    {
        this->mValidVersionSet.clear();
        this->mFilteredVersionSet.clear();
        this->mHighestValidVersion.erase();
        const DirDataObjectList& aList = theEntity->mDataObjects;
        DirDataObjectList::const_iterator anItr = aList.begin();
        while(anItr!=aList.end()) {
            const string& aVersion = anItr->mDataType;
            // the data as the original's std::string(data()) read it: to its first NUL (an empty buffer, whose
            // data() is NULL here, reads as empty; the original read past a buffer without a NUL)
            if ((anItr->mData.get()==NULL) || !this->IsFiltered(aVersion, CString(anItr->mData.get()))) {
                if(this->mHighestValidVersion.empty() || this->mVersionCompare(aVersion,this->mHighestValidVersion)>0)
                    this->mHighestValidVersion = aVersion;
                this->mValidVersionSet.insert(aVersion);
                string& aStr = this->mVersionDescriptionMap[aVersion];
                if(anItr->mData.get()!=NULL)
                    aStr.assign(anItr->mData->data(), anItr->mData->length());
            } else {
                this->mFilteredVersionSet.insert(aVersion);
                string& aStr = this->mFilteredVersionDescriptionMap[aVersion];
                if(anItr->mData.get()!=NULL)
                    aStr.assign(anItr->mData->data(), anItr->mData->length());
            }
            ++anItr;
        }
    }
    VersionResult CheckVersion(const string& theVersionId)
    {
        VersionSet::iterator anItr = this->mValidVersionSet.find(theVersionId);
        if(anItr!=this->mValidVersionSet.end()) {
            if(theVersionId==this->mHighestValidVersion)
                return Version_Valid_Latest;
            else
                return Version_Valid_NotLatest;
        }
        else
            return Version_Invalid;
    }
    void SetFilterString(const string& theFilterString)
    {
        string aFilterString = StringToLowerCase(theFilterString);
        PatchFilterSet(aFilterString, this->mFilterStringSet);
    }
    // GetVersionOp::DoFinish / FindPatchOp::DoFinish after a successful listing
    void DoFinish(const DirEntityList& aList)
    {
        if (!this->mFind) {
            if(!aList.empty())
                this->BuildValidVersionSet(aList.front());
            return;
        }
        DirEntityList::const_iterator anItr = aList.begin();
        while(anItr!=aList.end()) {
            const DirEntity* anEntity = anItr->get();
            if((anEntity->mType=='D'))
                this->BuildValidVersionSet(anEntity);
            else {
                PatchInfo anInfo;
                anInfo.mPatchLocation = anEntity->mNetAddr;
                const string& aPatchId = WStringToString(anEntity->mName);
                intptr_t aPos = ee_won_findpatch_id_split(reinterpret_cast<const uint8_t*>(aPatchId.data()), aPatchId.size());
                if (aPos < 0) anInfo.mToVersion = aPatchId;
                else { anInfo.mFromVersion = aPatchId.substr(0, aPos); anInfo.mToVersion = aPatchId.substr(aPos + 1); }
                anInfo.mName = anEntity->mDisplayName;
                DirDataObjectList::const_iterator anItr = anEntity->mDataObjects.begin();
                while(anItr!=anEntity->mDataObjects.end()) {
                    DirDataObject anObject = *anItr;
                    // (a NULL data the original dereferenced reads as empty)
                    const char* d = anObject.mData.get() != NULL ? anObject.mData->data() : "";
                    unsigned long n = anObject.mData.get() != NULL ? anObject.mData->length() : 0;
                    uint32_t v = 0;
                    if(anObject.mDataType=="_cs") { if (ee_won_findpatch_le32(reinterpret_cast<const uint8_t*>(d), n, &v)) anInfo.mChecksum = v; }
                    else if(anObject.mDataType=="_ps") { if (ee_won_findpatch_le32(reinterpret_cast<const uint8_t*>(d), n, &v)) anInfo.mSize = v; }
                    else anInfo.mData[anObject.mDataType] = anObject.mData;
                    ++anItr;
                }
                this->mPatchList.push_back(anInfo);
            }
            ++anItr;
        }
    }
    // FindPatchMsg::List over this op's compare and sets
    struct Ctx { Op* mOp; const std::set<string>* mSet; };
    static int32_t Compare(void* c, const uint8_t* a, size_t an, const uint8_t* b, size_t bn)
    {
        return static_cast<Ctx*>(c)->mOp->mVersionCompare(string(reinterpret_cast<const char*>(a), an), string(reinterpret_cast<const char*>(b), bn));
    }
    static bool InSet(void* c, const uint8_t* a, size_t n)
    {
        Ctx* x = static_cast<Ctx*>(c);
        return x->mSet->find(string(reinterpret_cast<const char*>(a), n)) != x->mSet->end();
    }
    static void Put(void* c, size_t i) { static_cast<std::vector<size_t>*>(c)->push_back(i); }
    std::vector<size_t> List(int inWhich, const string& from, const string& to)
    {
        std::vector<EeWonPatchRef> aRefs;
        for (PatchList::const_iterator i = this->mPatchList.begin(); i != this->mPatchList.end(); ++i) {
            EeWonPatchRef r;
            r.from = reinterpret_cast<const uint8_t*>(i->mFromVersion.data());
            r.from_len = i->mFromVersion.size();
            r.to = reinterpret_cast<const uint8_t*>(i->mToVersion.data());
            r.to_len = i->mToVersion.size();
            aRefs.push_back(r);
        }
        Ctx aCtx = {this, inWhich == 1 ? &this->mValidVersionSet : &this->mFilteredVersionSet};
        EeWonPatchHooks h;
        h.context = &aCtx;
        h.compare = Compare;
        h.in_set = InSet;
        std::vector<size_t> aChosen;
        ee_won_findpatch_list(inWhich == 3 ? 2 : inWhich, reinterpret_cast<const uint8_t*>(from.data()), from.size(),
                              reinterpret_cast<const uint8_t*>(to.data()), to.size(), aRefs.empty() ? NULL : &aRefs[0],
                              aRefs.size(), &h, Put, &aChosen);
        return aChosen;
    }
};

static string SetText(const VersionSet& s)
{
    string r;
    for (VersionSet::const_iterator i = s.begin(); i != s.end(); ++i) r += (r.empty() ? "" : ",") + Hex(*i);
    return r;
}
static string MapText(const VersionDescriptionMap& m)
{
    string r;
    for (VersionDescriptionMap::const_iterator i = m.begin(); i != m.end(); ++i)
        r += (r.empty() ? "" : ",") + Hex(i->first) + "=" + Hex(i->second);
    return r;
}
static string Dump(const Op& o)
{
    string s = "v[" + SetText(o.mValidVersionSet) + "] f[" + SetText(o.mFilteredVersionSet) + "] h" +
               Hex(o.mHighestValidVersion) + " d[" + MapText(o.mVersionDescriptionMap) + "] fd[" +
               MapText(o.mFilteredVersionDescriptionMap) + "] p[";
    bool first = true;
    for (PatchList::const_iterator p = o.mPatchList.begin(); p != o.mPatchList.end(); ++p) {
        if (!first) s += ";";
        first = false;
        string data;
        for (PatchDataMap::const_iterator d = p->mData.begin(); d != p->mData.end(); ++d)
            data += (data.empty() ? "" : ",") + Hex(d->first) + "=" +
                    (d->second.get() ? Hex(string(d->second->data(), d->second->length())) : string("n"));
        s += Hex(p->mFromVersion) + "~" + Hex(p->mToVersion) + " " + Hex(Utf16(p->mName)) + " " + Num(p->mChecksum) +
             " " + Num(p->mSize) + " " +
             (p->mPatchLocation.get() ? Hex(string(p->mPatchLocation->data(), p->mPatchLocation->length())) : string("n")) +
             " [" + data + "]";
    }
    return s + "]";
}

// ---- the members and the reply

static const char* const kProducts[] = { "EmpireEarth", "", "AoC\xc3\xa9", "x" };
static const char* const kConfigs[] = { "", "en", "beta" };
static const char* const kFilters[] = { "", "opt", "OPT,res", "res,,beta" };
static const char* const kVersions[] = { "1.0", "1.1", "2.0", "10.0", "" };
static const char* const kData[] = { "", "pt opt", "pt opt,res\nother", "x 1\nPT beta", "desc\0pt zz", "pt res" };
static const size_t kDataLen[] = { 0, 6, 16, 11, 10, 6 };
static const wchar_t* const kPatchNames[] = { L"1.0~1.1", L"2.0", L"1.1~2.0", L"~2.0", L"1.0~10.0", L"" };

struct Values {
    std::vector<string> mTok;
    std::list<string> mKeep;
    std::vector<EeWonAdminValue> mV;
    void N(long long v) { this->mTok.push_back("n" + Num(v)); EeWonAdminValue x = {static_cast<int64_t>(v), NULL, 0}; this->mV.push_back(x); }
    void S(const string& s)
    {
        this->mKeep.push_back(s);
        this->mTok.push_back("b" + (s.empty() ? string() : Hex(s)));
        EeWonAdminValue x = {0, reinterpret_cast<const uint8_t*>(this->mKeep.back().data()), this->mKeep.back().size()};
        this->mV.push_back(x);
    }
    string Line() const { string s; for (size_t i = 0; i < this->mTok.size(); i++) s += " " + this->mTok[i]; return s; }
};

static void AStr(string& w, const string& s) { w += (char)s.size(); w += s; }
static void AShort(string& w, int v) { w += (char)(v & 0xff); w += (char)((v >> 8) & 0xff); }
static void ALong(string& w, unsigned long v) { AShort(w, (int)(v & 0xffff)); AShort(w, (int)(v >> 16)); }
static void AWStr(string& w, const wstring& t) { string u = Utf16(t); AShort(w, (int)(u.size() / 2)); w += u; }

// a listing reply (GetDirOp's multi reply): header, status, the entity flags, the entities
static string AReply(unsigned long inFlags)
{
    string w;
    w += (char)(Pick(16) ? 5 : 3);
    AShort(w, 2);
    AShort(w, Pick(16) ? 3 : 2);
    static const short kStatus[] = { 0, 0, 0, 0, 0, -1, 7 };
    AShort(w, kStatus[Pick(7)]);
    w += (char)(0x80 | Pick(2));
    unsigned long f = Pick(6) ? inFlags : inFlags ^ (unsigned long)(1 << Pick(27));
    ALong(w, f);
    int n = Pick(5);
    AShort(w, n);
    for (int i = 0; i < n; i++) {
        char type = Pick(3) ? (Pick(2) ? 'D' : 'S') : 'X';
        if (f & 0x10) w += type;
        bool aService = type == 'S' || (f & 5) == 0;
        bool aDir = type == 'D' || (f & 2) == 0;
        if (aService) {
            if (f & 0x01000000) AWStr(w, L"Patches");
            if (f & 0x02000000) AWStr(w, kPatchNames[Pick(6)]);
            if (f & 0x04000000) { string a = Pick(4) ? string("\x01\x02\x03\x04\x05\x06", 6) : string("ftp://h/p"); AStr(w, a); }
        } else if (aDir) {
            if (f & 0x00010000) AWStr(w, L"/EE/Version");
            if (f & 0x00020000) AWStr(w, L"Version");
            if (f & 0x00040000) w += (char)Pick(3);
        }
        if (f & 0x20) AWStr(w, Pick(2) ? L"Patch \x263a" : L"");
        if (f & 0x100) ALong(w, Next());
        if (f & 0x40) ALong(w, Next());
        if (f & 0x80) ALong(w, Next());
        if (f & 0x2000) ALong(w, Next());
        if (f & 0x8000) { ALong(w, Next()); ALong(w, Next()); }
        if (f & 0x4000) { ALong(w, Next()); ALong(w, Next()); }
        if (f & 0x800) {
            int k = Pick(5);
            AShort(w, k);
            for (int j = 0; j < k; j++) {
                if (f & 0x200) {
                    string t = Pick(4) ? (aService && Pick(2) ? (Pick(2) ? string("_cs") : string("_ps")) : string(kVersions[Pick(5)]))
                                       : string("other");
                    AStr(w, t);
                }
                if (f & 0x400) {
                    int d = Pick(6);
                    string data(kData[d], kDataLen[d]);
                    if (Pick(4) == 0) data = string("\x04\x03\x02\x01", 4);
                    AShort(w, (int)data.size());
                    w += data;
                }
            }
        }
        if (f & 0x1000) AShort(w, 0);
    }
    if (Pick(8) == 0) w.resize(Pick((int)w.size() + 1));
    return w;
}

static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<string*>(c)->assign(reinterpret_cast<const char*>(d), n); }
#if VEROP_RUST
static string RDescribe(EeWonReqOp* r)
{
    string d;
    ee_won_reqop_describe(r, StrSink, &d);
    return d;
}
#endif

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i + 1 < argc; i += 2)
        if (!strcmp(argv[i], "--print")) aPrint = atoi(argv[i + 1]);
    int aSequences = aPrint > 0 ? aPrint : 20000;
    for (int q = 0; q < aSequences; q++) {
        Op o;
        Values v;
        o.mFind = Pick(2) != 0;
        o.mProduct = kProducts[Pick(4)];
        o.mExtraConfig = kConfigs[Pick(3)];
        string aFilter = kFilters[Pick(4)];
        v.S(o.mProduct);
        v.S(o.mExtraConfig);
        v.S(aFilter);
        o.SetFilterString(aFilter);
        if (o.mFind) {
            // FindPatchOp(GetVersionOp*): the version op's sets, at times
            int n = Pick(3) ? 0 : Pick(4);
            v.N(n);
            for (int i = 0; i < n; i++) {
                string ver = kVersions[Pick(5)];
                string desc = Pick(2) ? string("d") : string();
                v.S(ver);
                v.S(desc);
                o.mValidVersionSet.insert(ver);
                o.mVersionDescriptionMap[ver] = desc;
            }
            int m = Pick(3);
            v.N(m);
            for (int i = 0; i < m; i++) {
                string ver = kVersions[Pick(5)];
                v.S(ver);
                o.mFilteredVersionSet.insert(ver);
            }
        }
        int aKind = o.mFind ? 17 : 16;
        if (aPrint) printf("OP %d%s\n", aKind, v.Line().c_str());
#if VEROP_RUST
        EeWonReqOp* r = ee_won_reqops_new(aKind, v.mV.empty() ? NULL : &v.mV[0], v.mV.size());
#endif
        // RunHook: the sets cleared (FindPatchOp: its patches), the directory op made, its request
        unsigned long aFlags;
        if (!o.mFind) {
            o.mValidVersionSet.clear();
            o.mFilteredVersionSet.clear();
            o.mHighestValidVersion.erase();
            aFlags = 0x1 | 0x800 | 0x200 | 0x400;
        } else {
            o.mPatchList.clear();
            aFlags = 0x2 | 0x02000000 | 0x20 | 0x04000000 | 0x800 | 0x200 | 0x400;
            if (o.mValidVersionSet.empty()) aFlags |= 0x1;
        }
        string aPath = Utf16(o.GetVersionDir());
        EeWonAdminValue dv[3] = {{(int64_t)aFlags, NULL, 0}, {0, reinterpret_cast<const uint8_t*>(aPath.data()), aPath.size()}, {0, NULL, 0}};
        EeWonReqOp* aDir = ee_won_reqops_new(7, dv, 3);
        string aReq;
        long aReqStatus = ee_won_reqop_next_request(aDir, 4, StrSink, &aReq);
        string aQ = Num(aReqStatus) + " " + Hex(aReq);
        if (aPrint) printf("Q %s\n", aQ.c_str());
#if VEROP_RUST
        int32_t aFinish = 0;
        ee_won_reqop_run_hook(r, 0, &aFinish);
        string aRustReq;
        long aRs = ee_won_reqop_next_request(r, 4, StrSink, &aRustReq);
        CHECK(Num(aRs) + " " + Hex(aRustReq) == aQ);
#endif
        for (int n = 1 + Pick(2); n > 0; n--) {
            string aReply = AReply(aFlags);
            int32_t aVerdict = 0;
            long s = ee_won_reqop_check_response(aDir, reinterpret_cast<const uint8_t*>(aReply.data()), aReply.size(), &aVerdict);
            string st = aVerdict == 1 ? "inv" : aVerdict == 2 ? "cut" : Num(s);
            if (aVerdict == 0 && s == 0) {
                // the listing succeeded: DoFinish over its entities
                DirEntityList aList;
                ee_won_reqop_entities(aDir, 42, 0, TakeMulti, &aList);
                o.DoFinish(aList);
            }
            string aOrig = st + " " + Dump(o);
            if (aPrint) printf("R %s => %s\n", Hex(aReply).c_str(), aOrig.c_str());
#if VEROP_RUST
            int32_t rv = 0;
            long rs = ee_won_reqop_check_response(r, reinterpret_cast<const uint8_t*>(aReply.data()), aReply.size(), &rv);
            string aNew = (rv == 1 ? string("inv") : rv == 2 ? string("cut") : Num(rs)) + " " + RDescribe(r);
            CHECK(aNew == aOrig);
            if (aNew != aOrig && sFailures < 6) printf("R %s\n  orig %s\n  new  %s\n", Hex(aReply).c_str(), aOrig.c_str(), aNew.c_str());
#endif
            if (st != "100059") break;
        }
        // the queries: a version check, the patch lists
        for (int k = 0; k < 3; k++) {
            string id = kVersions[Pick(5)];
            string aOrig = Num(o.CheckVersion(id));
            int w = Pick(4);
            string from = kVersions[Pick(5)], to = kVersions[Pick(5)];
            std::vector<size_t> l = o.List(w, from, w == 3 ? o.mHighestValidVersion : to);
            aOrig += " [";
            for (size_t i = 0; i < l.size(); i++) aOrig += (i ? "," : "") + Num((long long)l[i]);
            aOrig += "]";
            if (aPrint) printf("C %s %d %s %s => %s\n", Hex(id).c_str(), w, Hex(from).c_str(), Hex(to).c_str(), aOrig.c_str());
#if VEROP_RUST
            ee_won_reqop_set(r, 64, 0, reinterpret_cast<const uint8_t*>(id.data()), id.size());
            string q = from + string(1, '\0') + to;
            ee_won_reqop_set(r, 72, w, reinterpret_cast<const uint8_t*>(q.data()), q.size());
            string aNew = Num(ee_won_reqop_num_at(r, 64, 0)) + " [";
            for (int i = 0; ee_won_reqop_num_at(r, 72, i) >= 0; i++) aNew += (i ? "," : "") + Num(ee_won_reqop_num_at(r, 72, i));
            aNew += "]";
            CHECK(aNew == aOrig);
#endif
        }
        ee_won_reqop_free(aDir);
#if VEROP_RUST
        ee_won_reqop_free(r);
#endif
    }
    if (!aPrint) printf("won_verop_test: %lld checks, %d failures (%d sequences)\n", sChecks, sFailures, aSequences);
    return sFailures;
}
