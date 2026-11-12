// WONDir's entity changes as whole ops (AddEntityOp, ModifyEntityOp, RemoveEntityOp, RenameEntityOp, RenewEntityOp,
// EntitySetDataObjectsOp, EntityModifyDataObjectsOp, EntityClearDataObjectsOp): the original's Init, GetNextRequest
// and CheckResponse (copied from the C++ op classes before the port, over stand-ins with the ops' members, the
// messages through include/ee/wonapi/WONDirRequest.h, which won_dirops_test checks against the pre-port statements)
// against the Rust ops (ee_won dirchange.rs) through their C ABI. Each sequence: generated members, the request,
// then up to four generated status replies (headers right and wrong, a returned address or none, cut and mutated),
// each reply's verdict and the op's address after it.
//
// `--print N` prints N sequences with what the original did (rust/crates/ee_won/src/golden_dirop.txt, recorded
// before the port; `dirchange::tests::golden` replays it). The exit code is the number of failed checks.
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
#include "ee/wonapi/WONDirExtended.h"
#include "ee/wonapi/WONDirRequest.h"
#include "rust/ee_won.h"

// 0 records the golden from the original's half alone
#ifndef DIROP_RUST
#define DIROP_RUST 1
#endif

using namespace WONAPI;
using std::string;
using std::wstring;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7373;
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
static string HexW(const wstring& w)
{
    string b;
    WideAppendUtf16LE(b, w);
    return Hex(b);
}
static string Bytes(const ByteBuffer* b) { return b == NULL ? string() : string(b->data(), b->length()); }
static string Buf(const ByteBufferPtr& b) { return b.get() == NULL ? string("n") : "x" + Hex(Bytes(b.get())); }
static string Num(long long v) { return std::to_string(v); }

static wstring AWString()
{
    static const wchar_t* const kTexts[] = { L"", L"/", L"/EE", L"Titan", L"n\x263a", L"\x00e9t\x00e9" };
    wstring s = kTexts[Pick(6)];
    if (Pick(12) == 0) s += wstring(Pick(140), L'x');
    return s;
}
static string AString()
{
    static const char* const kTexts[] = { "", "t", "type2", "\xff\x01", "HostName" };
    string s = kTexts[Pick(5)];
    if (Pick(12) == 0) s += string(Pick(140), 'y');
    return s;
}
static ByteBufferPtr ABuffer()
{
    if (Pick(4) == 0) return NULL;
    string s(Pick(Pick(12) ? 8 : 300), '\0');
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)Pick(256);
    return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
}
static DWORD AFlags()
{
    static const DWORD kFlags[] = { 0, 1, 2, 0x10, 0x11, 0x12, 0x1f, 0xff, 0x110 };
    return Pick(3) ? kFlags[Pick(9)] : (DWORD)(Next() * 0x10001u);
}
static DirDataObject AObject()
{
    DirDataObject o;
    o.mDataType = AString();
    o.mData = ABuffer();
    return o;
}

// ---- the ops' members (one stand-in for the family: each op reads its own)

enum { K_ADD = 5, K_MODIFY, K_REMOVE, K_RENAME, K_RENEW, K_SET, K_MODIFY_OBJECTS, K_CLEAR };

struct Op {
    int mKind;
    unsigned char mLengthFieldSize;
    bool mIsService, mIsUnique;
    DWORD mFlags;
    wstring mPath, mName, mDisplayName, mNewName, mNewDisplayName;
    ByteBufferPtr mNetAddr, mNewNetAddr, mRequest, mResponse;
    unsigned long mLifespan, mNewLifespan;
    DirDataObjectSetMode mDataObjectMode;
    DirDataObjectList mDataObjects;
    struct Mod { DirDataObject mObject; bool mIsInsert; unsigned short mOffset; };
    std::list<Mod> mDataObjectMods;
    DirDataTypeSet mDataTypes;

    // the constructor (Init) and generated members (each set or left)
    void Generate()
    {
        this->mKind = K_ADD + Pick(8);
        this->mIsService = Pick(2) != 0;
        this->mLengthFieldSize = 4;
        this->mFlags = 0; this->mLifespan = 0; this->mNewLifespan = 0; this->mIsUnique = false;
        this->mDataObjectMode = (DirDataObjectSetMode)0;
        switch (this->mKind) {
        case K_ADD: this->mFlags = DIR_UF_UNIQUEDISPLAYNAME; this->mLifespan = 0; break;
        case K_MODIFY: this->mFlags = DIR_UF_UNIQUEDISPLAYNAME; this->mNewLifespan = 0; break;
        case K_RENAME: this->mIsUnique = true; break;
        case K_RENEW: this->mLifespan = 0; break;
        case K_SET: this->mDataObjectMode = DIR_DO_ADDREPLACE; break;
        default: break;
        }
        if (Pick(6) == 0) return;
        if (Pick(4)) this->mFlags = AFlags();
        this->mPath = AWString(); this->mName = AWString(); this->mDisplayName = AWString();
        this->mNewName = AWString(); this->mNewDisplayName = AWString();
        this->mNetAddr = ABuffer(); this->mNewNetAddr = ABuffer();
        this->mLifespan = Pick(3) ? Pick(5) : Next() * 977u; this->mNewLifespan = Pick(3) ? Pick(5) : Next() * 977u;
        if (Pick(3)) this->mIsUnique = Pick(2) != 0;
        if (Pick(2)) this->mDataObjectMode = (DirDataObjectSetMode)Pick(6);
        for (int n = Pick(4); n > 0; n--) this->mDataObjects.push_back(AObject());
        for (int n = Pick(4); n > 0; n--) {
            Mod m = {AObject(), Pick(2) != 0, (unsigned short)Next()};
            this->mDataObjectMods.push_back(m);
        }
        for (int n = Pick(4); n > 0; n--) this->mDataTypes.insert(AString());
    }

    // every member as a field (the Rust op reads its own)
    string Line() const
    {
        string s = "OP " + Num(this->mKind) + " " + Num(this->mIsService) + " " + Num((uint32_t)this->mFlags) + " " +
                   HexW(this->mPath) + " " + HexW(this->mName) + " " + HexW(this->mDisplayName) + " " + Buf(this->mNetAddr) +
                   " " + HexW(this->mNewName) + " " + HexW(this->mNewDisplayName) + " " + Buf(this->mNewNetAddr) + " " +
                   Num((uint32_t)(this->mKind == K_MODIFY ? this->mNewLifespan : this->mLifespan)) + " " + Num(this->mIsUnique) +
                   " " + Num((uint8_t)this->mDataObjectMode) + " " + Num((long long)this->mDataObjects.size());
        for (DirDataObjectList::const_iterator i = this->mDataObjects.begin(); i != this->mDataObjects.end(); ++i)
            s += " " + Hex(i->mDataType) + " " + Buf(i->mData);
        s += " " + Num((long long)this->mDataObjectMods.size());
        for (std::list<Mod>::const_iterator i = this->mDataObjectMods.begin(); i != this->mDataObjectMods.end(); ++i)
            s += " " + Num(i->mOffset) + " " + Num(i->mIsInsert) + " " + Hex(i->mObject.mDataType) + " " + Buf(i->mObject.mData);
        s += " " + Num((long long)this->mDataTypes.size());
        for (DirDataTypeSet::const_iterator i = this->mDataTypes.begin(); i != this->mDataTypes.end(); ++i) s += " " + Hex(*i);
        return s;
    }

    // each op's GetNextRequest
    WONStatus GetNextRequest()
    {
        DirRequestFields aFields(this->mKind, this->mIsService);
        aFields.mReq.path = aFields.Text(this->mPath);
        aFields.mReq.name = aFields.Text(this->mName);
        aFields.mReq.net_addr = DirRequestFields::Buffer(this->mNetAddr.get());
        switch (this->mKind) {
        case K_ADD:
            aFields.mReq.flags = this->mFlags;
            aFields.mReq.display_name = aFields.Text(this->mDisplayName);
            aFields.mReq.lifespan = (uint32_t)this->mLifespan;
            aFields.Objects(this->mDataObjects);
            break;
        case K_MODIFY:
            aFields.mReq.flags = this->mFlags;
            aFields.mReq.new_name = aFields.Text(this->mNewName);
            aFields.mReq.new_net_addr = DirRequestFields::Buffer(this->mNewNetAddr.get());
            aFields.mReq.new_display_name = aFields.Text(this->mNewDisplayName);
            aFields.mReq.lifespan = (uint32_t)this->mNewLifespan;
            aFields.Objects(this->mDataObjects);
            break;
        case K_RENAME:
            aFields.mReq.is_unique = this->mIsUnique;
            aFields.mReq.new_display_name = aFields.Text(this->mNewDisplayName);
            break;
        case K_RENEW: aFields.mReq.lifespan = (uint32_t)this->mLifespan; break;
        case K_SET:
            aFields.mReq.mode = (uint8_t)this->mDataObjectMode;
            aFields.Objects(this->mDataObjects);
            break;
        case K_MODIFY_OBJECTS:
            for (std::list<Mod>::iterator anItr = this->mDataObjectMods.begin(); anItr != this->mDataObjectMods.end(); ++anItr)
                aFields.AddMod(anItr->mOffset, anItr->mIsInsert, anItr->mObject);
            break;
        case K_CLEAR: aFields.Types(this->mDataTypes); break;
        default: break;   // the removal: the key only
        }
        this->mRequest = aFields.Build(this->mLengthFieldSize);
        return (WONStatus)WS_ServerReq_Recv;
    }

    // each op's CheckResponse: the status, or inv / cut
    string CheckResponse()
    {
        try {
            bool aAddr = (this->mKind == K_ADD || this->mKind == K_MODIFY) && this->mIsService &&
                         (this->mFlags & DIR_UF_SERVRETURNADDR) != 0;
            WONStatus aStatus = aAddr ? DirStatusReply(this->mResponse.get(), true, &this->mNetAddr)
                                      : DirStatusReply(this->mResponse.get(), false, NULL);
            if (aStatus == (WONStatus)WS_ServerReq_InvalidReplyHeader) return "inv";
            return Num(aStatus);
        } catch (ReadBufferException&) {
            return "cut";
        }
    }

    string Dump() const { return this->mNetAddr.get() == NULL ? string("a-n") : "a" + Hex(Bytes(this->mNetAddr.get())); }
};

static string AReply()
{
    WriteBuffer w(0, true);
    w.AppendByte(Pick(10) ? 5 : (BYTE)Pick(7));
    w.AppendShort(Pick(10) ? 2 : (short)Pick(4));
    w.AppendShort(Pick(10) ? 1 : (short)Pick(3));
    w.AppendShort(Pick(3) ? 0 : (short)(Pick(9) - 5));
    if (Pick(3)) {
        string a = Pick(8) ? string("\x0a\x00\x00\x01\x1f\x90", Pick(7)) : string(Pick(256), 'z');
        w.AppendByte((BYTE)a.size());
        w.AppendBytes(a.data(), (unsigned long)a.size());
    }
    ConstSmartPtr<ByteBuffer> b = w.ToByteBuffer(true);
    string r(b->data(), b->length());
    if (Pick(6) == 0) r.resize(Pick((int)r.size() + 1));
    else if (Pick(10) == 0 && !r.empty()) r[Pick((int)r.size())] = (char)Pick(256);
    return r;
}

#if DIROP_RUST
static void StrSink(void* c, const uint8_t* d, size_t n) { static_cast<string*>(c)->assign(reinterpret_cast<const char*>(d), n); }
struct RustOp {
    EeWonReqOp* mOp;
    // every member into the fields (the op reads its own)
    RustOp(Op& o)
    {
        DirRequestFields f(o.mKind, o.mIsService);
        f.mReq.flags = o.mFlags;
        f.mReq.path = f.Text(o.mPath);
        f.mReq.name = f.Text(o.mName);
        f.mReq.display_name = f.Text(o.mDisplayName);
        f.mReq.net_addr = DirRequestFields::Buffer(o.mNetAddr.get());
        f.mReq.new_name = f.Text(o.mNewName);
        f.mReq.new_display_name = f.Text(o.mNewDisplayName);
        f.mReq.new_net_addr = DirRequestFields::Buffer(o.mNewNetAddr.get());
        f.mReq.lifespan = (uint32_t)(o.mKind == K_MODIFY ? o.mNewLifespan : o.mLifespan);
        f.mReq.is_unique = o.mIsUnique;
        f.mReq.mode = (uint8_t)o.mDataObjectMode;
        f.Objects(o.mDataObjects);
        for (std::list<Op::Mod>::iterator i = o.mDataObjectMods.begin(); i != o.mDataObjectMods.end(); ++i)
            f.AddMod(i->mOffset, i->mIsInsert, i->mObject);
        f.Types(o.mDataTypes);
        f.Build(4);   // fills the arrays
        this->mOp = ee_won_dirop_new(&f.mReq);
    }
    ~RustOp() { ee_won_reqop_free(this->mOp); }
    string Request()
    {
        int32_t aFinish = 0;
        ee_won_reqop_run_hook(this->mOp, 0, &aFinish);
        string r;
        long s = ee_won_reqop_next_request(this->mOp, ee_won_reqop_length_field_size(this->mOp), StrSink, &r);
        return Num(s) + " " + Hex(r);
    }
    string Check(const string& inReply)
    {
        int32_t v = 0;
        long s = ee_won_reqop_check_response(this->mOp, reinterpret_cast<const uint8_t*>(inReply.data()), inReply.size(), &v);
        string d;
        ee_won_reqop_describe(this->mOp, StrSink, &d);
        string st = v == EE_WON_REQOP_INVALID_HEADER ? string("inv") : v == EE_WON_REQOP_CUT ? string("cut") : Num(s);
        return st + " " + d;
    }
};
#endif

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    int aSequences = aPrint > 0 ? aPrint : 100000;
    long aSteps = 0;
    for (int q = 0; q < aSequences; q++) {
        Op o;
        o.Generate();
        string aLine = o.Line();
#if DIROP_RUST
        RustOp r(o);
#endif
        WONStatus aReqStatus = o.GetNextRequest();
        string aReq = Num(aReqStatus) + " " + Hex(Bytes(o.mRequest.get()));
#if DIROP_RUST
        string aRustReq = r.Request();
        CHECK(aRustReq == aReq);
        if (aRustReq != aReq && sFailures < 10) printf("%s\n  orig %s\n  new  %s\n", aLine.c_str(), aReq.c_str(), aRustReq.c_str());
#endif
        if (aPrint) printf("%s\nQ %s\n", aLine.c_str(), aReq.c_str());
        for (int n = 1 + Pick(4); n > 0; n--) {
            string aReply = AReply();
            o.mResponse = new ByteBuffer(aReply.data(), (unsigned long)aReply.size(), false);
            string st = o.CheckResponse();
            string aOrig = st + " " + o.Dump();
            aSteps++;
            if (aPrint) printf("R %s => %s\n", Hex(aReply).c_str(), aOrig.c_str());
#if DIROP_RUST
            string aNew = r.Check(aReply);
            CHECK(aNew == aOrig);
            if (aNew != aOrig && sFailures < 10) printf("%s\nR %s\n  orig %s\n  new  %s\n", aLine.c_str(), Hex(aReply).c_str(), aOrig.c_str(), aNew.c_str());
#endif
        }
    }
    if (!aPrint) printf("won_dirop_test: %lld checks, %d failures (%ld replies of %d sequences)\n", sChecks, sFailures, aSteps, aSequences);
    return sFailures;
}
