// R6 WON round 33's network moves against the original statements (fb32b70d8, copied here), over the same real
// objects: CDKey whole in Rust (cdkeystore.rs EeWonCdKeyObj; the original class is OrigCDKey below), DirMakeEntity's
// decisions (dir.rs ee_won_dir_entity_fill), RustServerOp's settings and requests (hostbase.rs, reqhost.rs: the
// time limits, auth type, client key, NewRequest, the account requests, the peer authentication and certificate
// ops' constructors), RoutingOp's SetText / SetData / Recipients (routeop.rs), RoutingGetUserStatusOp's settings
// (udpops.rs) and ThreadBase::Stop (thread.rs). Each phase draws random inputs, runs them through the original
// statements and through the game's classes, and compares what each side's objects then hold. The Internet code
// none of the live gates reach is proven here (owner rule "WON goldens by reach").
//
// EE_W33_MUTATE=n changes the original side in one way (1..13, the Mutate(n) calls below): the run must then report
// mismatches. The exit code is the number of mismatches.
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
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "platform/PlatTime.h"
#include "rust/won_net/dir.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/udpops.h"

using namespace WONAPI;

static int sMutate;
static bool Mutate(int n) { return sMutate == n; }
static unsigned sRand = 12345;
static unsigned R(unsigned n) { sRand = sRand * 1103515245u + 12345u; return n ? (sRand >> 8) % n : 0; }
static std::string N(long long v) { return std::to_string(v); }
static std::string Hex(const std::string& b)
{
    static const char d[] = "0123456789abcdef";
    std::string s = N((long long)b.size()) + ":";
    for (unsigned char c : b) { s += d[c >> 4]; s += d[c & 15]; }
    return s;
}
static std::string Hex(const ByteBuffer* b) { return b == NULL ? "null" : Hex(std::string(b->data(), b->length())); }
static std::string W(const std::wstring& w) { std::string s; for (wchar_t c : w) s += N((long long)c) + ","; return s; }

static int sChecks, sFailures;
static std::map<std::string, int> sFailedPhases;
static void Compare(const char* inPhase, const std::string& inOrig, const std::string& inNew, const std::string& inWhat)
{
    sChecks++;
    if (inOrig == inNew) return;
    sFailedPhases[inPhase]++;
    if (sFailures++ < 10) printf("FAIL %s %s:\n  orig %s\n  new  %s\n", inPhase, inWhat.c_str(), inOrig.c_str(), inNew.c_str());
}

// ---- CDKey: the original class (WONAuthContext.h and AuthContext.cpp's stores at fb32b70d8), renamed
class OrigCDKey {
public:
    OrigCDKey() : mValid(false) {}
    OrigCDKey(const std::string& inProduct) : mProduct(inProduct), mValid(false) {}
    void Invalidate() { this->mValid = false; if (Mutate(1)) this->mKey = NULL; }
    void SetProductString(const std::string& inProduct) { this->Invalidate(); this->mProduct = inProduct; }
    static DWORD GetMachineId(bool* outSuccess = NULL)
    {
        int found = 0;
        DWORD serial = PlatSystemMachineId(&found);
        if (outSuccess != NULL) *outSuccess = found != 0;
        return serial;
    }
    bool InitEncrypted(const void* inData, unsigned long inLength)
    {
        this->mValid = false;
        if (!Mutate(2)) this->mKey = NULL;
        if (inData == NULL) return false;
        bool gotId = false;
        DWORD machineId = OrigCDKey::GetMachineId(&gotId);
        EeWonCdKeyDecoded d;
        ee_won_cdkey_decrypt(WonBytes(this->mProduct), this->mProduct.length(), machineId, gotId,
                             this->GetUseNewEncryptMethod(), static_cast<const uint8_t*>(inData), inLength, &d);
        if (d.has_raw) {
            this->mKey = new ByteBuffer(d.raw, 8, false);
            this->mValid = d.valid;
        }
        if (!d.used_machine) this->NotifyUsedOldMethodToDecrypt();
        return this->mValid;
    }
    bool Init(const std::string& inKey)
    {
        this->mValid = false;
        this->mKey = NULL;
        uint8_t raw[8];
        return ee_won_cdkey_parse(WonBytes(inKey), Mutate(4) ? inKey.length() / 2 : inKey.length(), raw) && this->CalcKeyFromBuffer((const char*)raw);
    }
    std::string GetString() const
    {
        if (this->mKey.get() == NULL || this->mKey->length() < 8 || (Mutate(3) && !this->mValid)) return "";
        char text[25];
        ee_won_cdkey_format(reinterpret_cast<const uint8_t*>(this->mKey->data()), text);
        return text;
    }
    ConstSmartPtr<ByteBuffer> GetEncrypted() const
    {
        if (!this->IsValid()) return NULL;
        bool useMachine = this->GetUseNewEncryptMethod();
        bool gotId = false;
        DWORD machineId = useMachine ? OrigCDKey::GetMachineId(&gotId) : 0;
        ConstSmartPtr<ByteBuffer> result;
        ee_won_cdkey_encrypt(WonBytes(this->mProduct), this->mProduct.length(), machineId, useMachine && gotId,
                             reinterpret_cast<const uint8_t*>(this->mKey->data()), WonByteBufferSink, &result);
        return result;
    }
    bool InitRaw(const void* inRaw)
    {
        this->mValid = false;
        this->mKey = NULL;
        return this->CalcKeyFromBuffer(static_cast<const char*>(inRaw));
    }
    bool SaveToFile(const std::string& inFileName)
    {
        ByteBufferPtr anEncrypted = this->GetEncrypted();
        return anEncrypted.get() != NULL
            && ee_won_cdkey_file_write(inFileName.c_str(), reinterpret_cast<const uint8_t*>(anEncrypted->data()),
                                       anEncrypted->length());
    }
    bool LoadFromFile(const std::string& inFileName)
    {
        std::string aBytes;
        return ee_won_cdkey_file_read(inFileName.c_str(), WonStringSink, &aBytes) && this->InitEncrypted(aBytes.data(), aBytes.size());
    }
    bool IsValid() const { return mValid; }
    bool CalcKeyFromBuffer(const char* inRaw)
    {
        this->mKey = new ByteBuffer(inRaw, 8, false);
        this->mValid = ee_won_cdkey_is_valid(WonBytes(this->mProduct), this->mProduct.length(),
                                             reinterpret_cast<const uint8_t*>(inRaw));
        return this->mValid;
    }
    virtual void NotifyUsedOldMethodToDecrypt() {}
    virtual bool GetUseNewEncryptMethod() const { return true; }
    bool Flag(int inWhich) const
    {
        return this->mKey.get() != NULL
            && ee_won_cdkey_flag(reinterpret_cast<const uint8_t*>(this->mKey->data()), this->mKey->length(), inWhich);
    }
    std::string mProduct;
    ConstSmartPtr<ByteBuffer> mKey;
    bool mValid;
};
static std::string State(const OrigCDKey& k)
{
    return "valid " + N(k.IsValid()) + " key " + (k.mKey.get() ? Hex(k.mKey.get()) : std::string("none")) + " text '" +
           k.GetString() + "' enc " + Hex(k.GetEncrypted().get()) + " beta " + N(k.Flag(0)) + " force " + N(k.Flag(1)) +
           " product " + k.mProduct;
}
static std::string State(const CDKey& k)
{
    return "valid " + N(k.IsValid()) + " key " + (k.HasKey() ? Hex(k.KeyBytes()) : std::string("none")) + " text '" +
           k.GetString() + "' enc " + Hex(k.GetEncrypted().get()) + " beta " + N(k.IsBeta()) + " force " +
           N(k.IsForceCheck()) + " product " + k.GetProduct();
}
static std::string RandomProduct()
{
    static const char* kProducts[] = {"", "EmpireEarth", "Homeworld", "TribesVengeance", "x"};
    if (R(4) == 0) {
        std::string s;
        for (unsigned i = 0, n = R(12); i < n; i++) s += (char)(' ' + R(95));
        return s;
    }
    return kProducts[R(5)];
}
static std::string RandomRaw(const std::string& inProduct)
{
    uint8_t raw[8];
    for (;;) {
        for (int i = 0; i < 8; i++) raw[i] = (uint8_t)R(256);
        if (R(3) == 0 || ee_won_cdkey_is_valid(WonBytes(inProduct), inProduct.size(), raw)) break;
    }
    return std::string(reinterpret_cast<const char*>(raw), 8);
}
static std::string RandomText(const std::string& inProduct)
{
    std::string raw = RandomRaw(inProduct);
    char text[25];
    ee_won_cdkey_format(reinterpret_cast<const uint8_t*>(raw.data()), text);
    std::string s = text;
    switch (R(5)) {
    case 0: { std::string t; for (char c : s) if (c != '-') t += c; return t; }          // no dashes
    case 1: { std::string t; for (char c : s) t += (c == '-') ? ' ' : c; return t; }     // spaces
    case 2: s[R((unsigned)s.size())] = (char)(' ' + R(95)); return s;                     // a character changed
    case 3: return s.substr(0, R((unsigned)s.size()));                                     // cut short
    default: return s;
    }
}
static void CdKeyPhase(int inSequences)
{
    for (int q = 0; q < inSequences; q++) {
        std::string aProduct = RandomProduct();
        OrigCDKey o(aProduct);
        CDKey n(aProduct);
        std::string aLastEncrypted;
        for (int step = 0; step < 10; step++) {
            int op = (int)R(8);
            std::string what = "seq " + N(q) + " step " + N(step) + " op " + N(op);
            std::string r0, r1;
            switch (op) {
            case 0: { std::string p = RandomProduct(); o.SetProductString(p); n.SetProductString(p); break; }
            case 1: { std::string t = RandomText(o.mProduct); r0 = N(o.Init(t)); r1 = N(n.Init(t)); break; }
            case 2: { std::string raw = RandomRaw(o.mProduct); r0 = N(o.InitRaw(raw.data())); r1 = N(n.InitRaw(raw.data())); break; }
            case 3: {
                std::string d;
                int k = (int)R(3);
                if (k == 0) d = aLastEncrypted;
                else for (unsigned i = 0, m = R(40); i < m; i++) d += (char)R(256);
                bool null = R(6) == 0;
                r0 = N(o.InitEncrypted(null ? NULL : d.data(), (unsigned long)d.size()));
                r1 = N(n.InitEncrypted(null ? NULL : d.data(), (unsigned long)d.size()));
                break;
            }
            case 4: o.Invalidate(); n.Invalidate(); break;
            case 5: {
                // the file store: each side's own file, then a fresh key of the product loaded from it
                r0 = N(o.SaveToFile("won_net33_orig.key"));
                r1 = N(n.SaveToFile("won_net33_new.key"));
                OrigCDKey lo(o.mProduct);
                CDKey ln(n.GetProduct());
                r0 += " load " + N(lo.LoadFromFile("won_net33_orig.key")) + " " + State(lo);
                r1 += " load " + N(ln.LoadFromFile("won_net33_new.key")) + " " + State(ln);
                PlatDeleteFile("won_net33_orig.key");
                PlatDeleteFile("won_net33_new.key");
                break;
            }
            case 6: { CDKey c(n); OrigCDKey oc(o); r0 = State(oc); r1 = State(c); CDKey a; a = n; r1 += State(a); r0 += State(oc); break; }
            default: {
                // the encrypted form decrypted again by a key of the same product
                ConstSmartPtr<ByteBuffer> e = o.GetEncrypted();
                aLastEncrypted = e.get() ? std::string(e->data(), e->length()) : std::string();
                break;
            }
            }
            Compare("cdkey", r0 + " | " + State(o), r1 + " | " + State(n), what);
        }
    }
}

// ---- DirMakeEntity (DirEntityReplyParser.cpp at fb32b70d8)
static void DirAssign(std::wstring& outText, const uint16_t* inUnits, size_t inCount)
{
    std::string aBytes;
    aBytes.reserve(inCount * 2);
    for (size_t i = 0; i < inCount; i++) {
        aBytes += (char)(inUnits[i] & 0xff);
        aBytes += (char)(inUnits[i] >> 8);
    }
    WideAssignUtf16LE(outText, aBytes.data(), (unsigned long)inCount);
}
static DirEntityPtr DirMakeEntity(const EeWonDirEntity* inEntity)
{
    DirEntityPtr anEntity = new DirEntity;
    uint32_t f = inEntity->fields;
    if (f & 0x1) anEntity->mType = (char)inEntity->kind;
    if (f & 0x2) DirAssign(anEntity->mPath, inEntity->path, inEntity->path_len);
    if (f & 0x4) DirAssign(anEntity->mName, inEntity->name, inEntity->name_len);
    if (f & 0x8)
        anEntity->mNetAddr = new ByteBuffer(inEntity->net_addr, (unsigned long)inEntity->net_addr_len, false);
    if (f & 0x10) anEntity->mReqUnique = inEntity->req_unique;
    if (f & 0x20) DirAssign(anEntity->mDisplayName, inEntity->display_name, inEntity->display_name_len);
    if (f & 0x40) anEntity->mLifespan = inEntity->lifespan;
    if (f & 0x80) anEntity->mCreated = inEntity->created;
    if (f & 0x100) anEntity->mTouched = inEntity->touched;
    if (f & 0x200) anEntity->mCRC = inEntity->crc;
    if (f & 0x400) {
        anEntity->mCreateIP = inEntity->create_ip;
        if (!Mutate(5)) anEntity->mTouchIP = inEntity->touch_ip;
    }
    if (f & 0x800) {
        anEntity->mCreateId = inEntity->create_id;
        anEntity->mTouchId = inEntity->touch_id;
    }
    for (size_t i = 0; i < inEntity->object_count; i++) {
        const EeWonDirDataObject& anObject = inEntity->objects[i];
        DirDataObject aData;
        aData.mDataType.assign(reinterpret_cast<const char*>(anObject.data_type), anObject.data_type_len);
        if (anObject.has_data)
            aData.mData = new ByteBuffer(anObject.data, (unsigned long)anObject.data_len, false);
        anEntity->mDataObjects.push_back(aData);
    }
    for (size_t i = 0; i < inEntity->acl_count; i++) {
        const EeWonDirAcl& aRead = inEntity->acls[i];
        DirACL acl;
        acl.mType = (char)aRead.kind;
        long user = aRead.user;
        long comm = aRead.community;
        short trust = Mutate(6) ? (short)(aRead.trust + 1) : aRead.trust;
        acl.mUser = user; acl.mCommunity = comm; acl.mTrust = trust;
        anEntity->mACLs.push_back(acl);
    }
    return anEntity;
}
static std::string Describe(const DirEntity& e)
{
    std::string s = "type " + N(e.mType) + " path " + W(e.mPath) + " name " + W(e.mName) + " display " + W(e.mDisplayName) +
                    " life " + N(e.mLifespan) + " crc " + N((long long)e.mCRC) + " created " + N(e.mCreated) + " cip " +
                    N((long long)e.mCreateIP) + " cid " + N((long long)e.mCreateId) + " touched " + N(e.mTouched) + " tip " +
                    N((long long)e.mTouchIP) + " tid " + N((long long)e.mTouchId) + " unique " + N(e.mReqUnique) + " addr " +
                    Hex(e.mNetAddr.get());
    for (const DirDataObject& d : e.mDataObjects) s += " obj " + Hex(d.mDataType) + "=" + Hex(d.mData.get());
    for (const DirACL& a : e.mACLs)
        s += " acl " + N(a.mType) + "/" + N((long long)a.mUser) + "/" + N((long long)a.mCommunity) + "/" + N(a.mTrust);
    return s;
}
static std::vector<uint16_t> Units()
{
    std::vector<uint16_t> u;
    for (unsigned i = 0, n = R(8); i < n; i++) {
        switch (R(4)) {
        case 0: u.push_back((uint16_t)('a' + R(26))); break;
        case 1: u.push_back((uint16_t)R(0x10000)); break;
        case 2: u.push_back((uint16_t)(0xd800 + R(0x400))); u.push_back((uint16_t)(0xdc00 + R(0x400))); break; // a pair
        default: u.push_back((uint16_t)(0xd800 + R(0x800))); break; // a lone surrogate
        }
    }
    return u;
}
static int32_t AnyInt() { return R(3) == 0 ? (int32_t)(0x80000000u + R(0x1000)) : (int32_t)R(100000) - 50000; }
static void DirPhase(int inEntities)
{
    for (int q = 0; q < inEntities; q++) {
        std::vector<uint16_t> path = Units(), name = Units(), display = Units();
        std::string addr, types[4], datas[4];
        for (unsigned i = 0, m = R(9); i < m; i++) addr += (char)R(256);
        EeWonDirDataObject objects[4];
        EeWonDirAcl acls[4];
        EeWonDirEntity e;
        memset(&e, 0, sizeof(e));
        e.fields = R(0x1000);
        e.kind = (uint8_t)R(256);
        e.path = path.data(); e.path_len = path.size();
        e.name = name.data(); e.name_len = name.size();
        e.display_name = display.data(); e.display_name_len = display.size();
        e.net_addr = reinterpret_cast<const uint8_t*>(addr.data()); e.net_addr_len = addr.size();
        e.req_unique = R(2) != 0;
        e.lifespan = AnyInt(); e.created = AnyInt(); e.touched = AnyInt(); e.crc = AnyInt();
        e.create_ip = AnyInt(); e.touch_ip = AnyInt(); e.create_id = AnyInt(); e.touch_id = AnyInt();
        e.object_count = R(4);
        for (size_t i = 0; i < e.object_count; i++) {
            for (unsigned k = 0, m = R(6); k < m; k++) types[i] += (char)R(256);
            for (unsigned k = 0, m = R(6); k < m; k++) datas[i] += (char)R(256);
            objects[i].data_type = reinterpret_cast<const uint8_t*>(types[i].data());
            objects[i].data_type_len = types[i].size();
            objects[i].has_data = R(2) != 0;
            objects[i].data = reinterpret_cast<const uint8_t*>(datas[i].data());
            objects[i].data_len = datas[i].size();
        }
        e.objects = objects;
        e.acl_count = R(4);
        for (size_t i = 0; i < e.acl_count; i++) {
            acls[i].kind = (uint8_t)R(256);
            acls[i].user = AnyInt();
            acls[i].community = AnyInt();
            acls[i].trust = (int16_t)(R(0x10000) - 0x8000);
        }
        e.acls = acls;
        DirEntityList aList;
        DirEntityReplyParser::TakeEntity(&aList, &e);
        Compare("dir", Describe(*DirMakeEntity(&e).get()), Describe(*aList.front().get()), "entity " + N(q) + " fields " + N(e.fields));
    }
}

// ---- RustServerOp's settings and requests (ServerOp.cpp at fb32b70d8, over the class's public members)
struct Objects {
    ServerContext* mContext;
    AuthContext* mAuth;
    BlockingSocket* mSocket;
};
// an op's members: the op's own line with the side's objects named, the base members, the objects' references
static std::string OpState(RustServerOp* inOp, const Objects& s)
{
    std::string d;
    ee_won_hostop_describe(inOp->GetRustOp(), WonStringSink, &d);
    auto name = [&](long long v) -> std::string {
        if (v == 0) return "null";
        if (v == (long long)(intptr_t) static_cast<RefCount*>(s.mContext)) return "context";
        if (v == (long long)(intptr_t) static_cast<RefCount*>(s.mAuth)) return "auth";
        if (v == (long long)(intptr_t) static_cast<RefCount*>(s.mSocket)) return "socket";
        if (s.mAuth != NULL && v == (long long)(intptr_t) static_cast<RefCount*>(s.mAuth->GetServerContext().get())) return "authservers";
        return "other";
    };
    std::string b;
    for (int w = 0; w <= 8; w++) {
        long long v = ee_won_hostop_base_get(inOp->GetRustOp(), w);
        b += " b" + N(w) + "=" + (w <= 1 ? name(v) : N(v));
    }
    // the describe line carries addresses of the side's objects: replace them by their names
    for (RefCount* o : {static_cast<RefCount*>(s.mContext), static_cast<RefCount*>(s.mAuth), static_cast<RefCount*>(s.mSocket)}) {
        char hex[40];
        snprintf(hex, sizeof hex, "%llx", (unsigned long long)(intptr_t)o);
        for (size_t at; o != NULL && (at = d.find(hex)) != std::string::npos;) d.replace(at, strlen(hex), "OBJ");
    }
    return d + b + " addr " + inOp->GetAddr().GetHostAndPortString() + " refs " + N(s.mContext->mRefCount) + " " +
           N(s.mAuth->mRefCount) + " " + N(s.mSocket->mRefCount);
}
static EeWonReqOp* RandomRequest(int k)
{
    static const std::wstring kPath = L"/TitanServers/Auth";
    switch (k) {
    case 0: return ee_won_reqmake_get_account();
    case 1: return ee_won_reqmake_group_list((unsigned char)R(256));
    default: return ee_won_reqmake_get_dir(R(0x10000), kPath.data(), kPath.size(), sizeof(wchar_t), NULL, 0);
    }
}
static void ServerOpPhase(int inCases)
{
    for (int q = 0; q < inCases; q++) {
        int kind = (int)R(9);
        int req = (int)R(3);
        unsigned seed = sRand;
        std::string states[2];
        int connect = (int)R(100000) - 1000, send = (int)R(100000), recv = (int)(R(2) ? 0x80000000u + R(10) : R(1000));
        AuthType type = (AuthType)R(4);
        unsigned char keySize = (unsigned char)R(256);
        bool useKey = R(2) != 0;
        bool nullContext = R(4) == 0, nullAuth = R(4) == 0;
        IPAddr anAddr((long)(0x0a000000u + R(0x1000)), (unsigned short)R(0x10000));
        std::wstring aPassword = L"pw" + std::to_wstring(R(1000));
        std::string anEmail = "e" + N(R(1000)) + "@x.org";
        for (int side = 0; side < 2; side++) {
            sRand = seed;
            Objects s = {new ServerContext, new AuthContext, new BlockingSocket(0)};
            s.mContext->AddRef(); s.mAuth->AddRef(); s.mSocket->AddRef();
            ServerContext* ctx = nullContext ? NULL : s.mContext;
            AuthContext* auth = nullAuth ? NULL : s.mAuth;
            RustServerOp* op = NULL;
            bool orig = side == 0;
            switch (kind) {
            case 0: // NewRequest(op, context)
                if (orig) { op = new RustServerOp(ee_won_hostop_request(RandomRequest(req))); op->Set(EE_WON_REQHOST_CONTEXT, static_cast<RefCount*>(ctx)); }
                else op = RustServerOp::NewRequest(RandomRequest(req), ctx);
                break;
            case 1: // NewRequest(op, address)
                if (orig) {
                    op = new RustServerOp(ee_won_hostop_request(RandomRequest(req)));
                    ee_won_hostop_set(op->GetRustOp(), EE_WON_REQHOST_ADDR, 0, NULL, 0, reinterpret_cast<const uint8_t*>(anAddr.GetSixByte()), NULL);
                } else op = RustServerOp::NewRequest(RandomRequest(req), anAddr);
                break;
            case 2: // NewGetAccount
                if (orig) {
                    op = new RustServerOp(ee_won_hostop_request(ee_won_reqmake_get_account()));
                    op->Set(EE_WON_REQHOST_CONTEXT, static_cast<RefCount*>(ctx));
                    op->SetAuthType(Mutate(7) ? AUTH_TYPE_PERSISTENT : AUTH_TYPE_SESSION, auth);
                } else op = RustServerOp::NewGetAccount(ctx, auth);
                break;
            case 3: // NewUpdateAccount
                if (orig) {
                    op = new RustServerOp(ee_won_hostop_request(ee_won_reqmake_update_account(aPassword.data(), aPassword.size(), sizeof(wchar_t),
                                                                                              WonBytes(anEmail), anEmail.size())));
                    op->Set(EE_WON_REQHOST_CONTEXT, static_cast<RefCount*>(ctx));
                    op->SetAuthType(AUTH_TYPE_SESSION, auth);
                } else op = RustServerOp::NewUpdateAccount(ctx, auth, aPassword, anEmail);
                break;
            case 4: // SetMaxTimes
                op = new RustServerOp(ee_won_hostop_request(RandomRequest(req)));
                if (orig) {
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_MAX_CONNECT, (DWORD)connect, NULL);
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_MAX_SEND, (DWORD)(Mutate(8) ? recv : send), NULL);
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_MAX_RECV, (DWORD)(Mutate(8) ? send : recv), NULL);
                } else op->SetMaxTimes(connect, send, recv);
                break;
            case 5: // SetAuthType, SetClientKeySize
                op = new RustServerOp(ee_won_hostop_request(RandomRequest(req)));
                if (orig) {
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_AUTH_TYPE, type, NULL);
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_AUTH_CONTEXT, 0, static_cast<RefCount*>(auth));
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_CLIENT_KEY_SIZE, keySize, NULL);
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_USE_CLIENT_KEY, useKey, NULL);
                } else {
                    op->SetAuthType(type, auth);
                    op->SetClientKeySize(keySize, useKey);
                }
                break;
            case 6: // PeerAuthOp's constructor
                if (orig) {
                    op = new RustServerOp(ee_won_hostop_peerauth(reinterpret_cast<const uint8_t*>(anAddr.GetSixByte()), useKey));
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_AUTH_TYPE, type, NULL);
                    ee_won_hostop_base(op->GetRustOp(), EE_WON_HOSTBASE_AUTH_CONTEXT, 0, static_cast<RefCount*>(auth));
                    if (!Mutate(9)) op->SetSocket(nullContext ? NULL : s.mSocket);
                } else op = new PeerAuthOp(anAddr, auth, type, nullContext ? NULL : s.mSocket, useKey);
                break;
            case 7: // PeerAuthServerOp's constructor
                if (orig) {
                    op = new RustServerOp(ee_won_hostop_peerauth_server());
                    op->SetAuthType(AUTH_TYPE_NONE, auth);
                    op->SetSocket(nullContext ? NULL : s.mSocket);
                } else op = new PeerAuthServerOp(auth, nullContext ? NULL : s.mSocket);
                break;
            default: // GetCertOp's constructor (an auth context of its own servers)
                if (orig) {
                    op = new RustServerOp(ee_won_hostop_getcert());
                    op->Set(EE_WON_REQHOST_CONTEXT, Mutate(10) ? static_cast<RefCount*>(s.mContext) : s.mAuth->GetServerContext().get());
                    op->Set(EE_WON_GETCERT_AUTH_CONTEXT, s.mAuth);
                } else op = new GetCertOp(s.mAuth);
                break;
            }
            op->AddRef();
            states[side] = OpState(op, s);
            op->Release();
            s.mContext->Release(); s.mAuth->Release(); s.mSocket->Release();
        }
        Compare("serverop", states[0], states[1], "case " + N(q) + " kind " + N(kind));
    }
}

// ---- RoutingOp's members and RoutingGetUserStatusOp's settings (WONRouting.h at fb32b70d8)
static std::wstring RandomWide()
{
    std::wstring w;
    for (unsigned i = 0, n = R(10); i < n; i++) {
        unsigned k = R(4);
        if (k == 0) w += (wchar_t)('A' + R(26));
        else if (k == 1) w += (wchar_t)R(0xd800);
        else if (k == 2 && sizeof(wchar_t) == 4) w += (wchar_t)(0x10000 + R(0x100000));
        else w += (wchar_t)(0xe000 + R(0x2000));
    }
    return w;
}
static void RoutingPhase(int inCases)
{
    for (int q = 0; q < inCases; q++) {
        std::string states[2];
        unsigned seed = sRand;
        for (int side = 0; side < 2; side++) {
            sRand = seed;
            bool orig = side == 0;
            RoutingOp* op = new RoutingOp(NULL, (RoutingOpType)R(100));
            op->AddRef();
            std::string st;
            int which = (int)R(4);
            std::wstring text = RandomWide();
            if (orig) {
                std::string b;
                WideAppendUtf16LE(b, Mutate(11) ? text.substr(0, text.size() / 2) : text);
                ee_won_routeop_set_text(op->mRust, which, reinterpret_cast<const uint16_t*>(b.data()), b.size() / 2);
            } else op->SetText(which, text);
            st += "text " + W(op->Text(which));
            std::string data;
            for (unsigned i = 0, m = R(12); i < m; i++) data += (char)R(256);
            bool null = R(3) == 0;
            ByteBufferPtr buffer = null ? NULL : new ByteBuffer(data.data(), (unsigned long)data.size(), false);
            if (orig) {
                if (buffer.get() == NULL) ee_won_routeop_set_data(op->mRust, false, NULL, 0);
                else ee_won_routeop_set_data(op->mRust, true, reinterpret_cast<const uint8_t*>(buffer->data()), buffer->length());
            } else op->SetData(buffer.get());
            st += " data " + Hex(op->GetData().get());
            std::vector<uint16_t> ids;
            for (unsigned i = 0, m = R(7); i < m; i++) ids.push_back((uint16_t)R(0x10000));
            ee_won_routeop_set_ids(op->mRust, ids.empty() ? NULL : ids.data(), ids.size());
            std::list<unsigned short> got;
            if (orig) {
                std::vector<uint16_t> anIds(ee_won_routeop_ids(op->mRust, NULL, 0) + 1);
                size_t n = ee_won_routeop_ids(op->mRust, &anIds[0], anIds.size() - 1);
                got = std::list<unsigned short>(anIds.begin(), anIds.begin() + n);
            } else got = op->Recipients();
            st += " ids";
            for (unsigned short id : got) st += " " + N(id);
            op->Release();
            // the user status op's settings (the original constructor's statements on a plain send-and-receive op)
            UDPManager* aManager = R(2) ? new UDPManager : NULL;
            if (aManager) aManager->AddRef();
            IPAddr anAddr((long)(0x0a000000u + R(0x1000)), (unsigned short)R(0x10000));
            UDPManagerSendRecvOp* u;
            if (orig) {
                u = new UDPManagerSendRecvOp;
                u->SetDestAddr(anAddr);
                u->SetUDPManager(aManager);
                u->SetNumRetransmits(Mutate(12) ? 2 : 3);
                u->SetMsgTimeout(1000);
            } else u = new RoutingGetUserStatusOp(anAddr, aManager);
            u->AddRef();
            st += " status " + u->GetDestAddr().GetHostAndPortString() + " " + N(u->GetNumRetransmits()) + " " +
                  N(u->GetMsgTimeout()) + " " + N(ee_won_udpsendrecvop_manager(u->GetUdpObject()) == static_cast<RefCount*>(aManager));
            u->Release();
            if (aManager) aManager->Release();
            states[side] = st;
        }
        Compare("routing", states[0], states[1], "case " + N(q));
    }
}

// ---- ThreadBase::Stop: the original statements on a thread waiting on its signal
class Waiter : public Thread {
public:
    volatile int mLoops = 0, mEntered = 0;
    virtual void ThreadFunc() { while (!this->IsStopped()) { mEntered = 1; this->WaitForSignal(8000); mLoops = mLoops + 1; } }
    void OrigStop(bool waitForStop)
    {
        if (!ee_won_thread_mark_stopped(this->mThread))
            return;
        if (!Mutate(13)) this->Signal();
        if (waitForStop)
            this->WaitForStop();
    }
};
static void ThreadPhase(int inCases)
{
    for (int q = 0; q < inCases; q++) {
        std::string states[2];
        bool wait = q % 2 == 0, twice = q % 3 == 0, start = q % 5 != 4;
        for (int side = 0; side < 2; side++) {
            Waiter* t = new Waiter;
            if (start) t->Start();
            // the thread waiting on its signal before the stop (else the stop's timing decides the loops)
            for (int i = 0; start && !t->mEntered && i < 5000; i++) PlatSleep(1);
            PlatSleep(5);
            uint64_t t0 = PlatTimeMilliseconds64();
            if (side == 0) t->OrigStop(wait); else t->Stop(wait);
            if (twice) { if (side == 0) t->OrigStop(true); else t->Stop(true); }
            uint64_t ms = PlatTimeMilliseconds64() - t0;
            t->WaitForStop(); // (a second stop returns at once, the first without a wait did not wait)
            // the signal wakes the thread at once: well under its 8 s wait
            states[side] = "quick " + N(ms < 6000) + " loops " + N(t->mLoops);
            delete t;
        }
        Compare("thread", states[0], states[1], "case " + N(q));
    }
}

int main(int argc, char** argv)
{
    const char* m = getenv("EE_W33_MUTATE");
    sMutate = m != NULL ? atoi(m) : 0;
    int scale = sMutate ? 1 : 4;
    AsyncSocket::StartWinsock();
    WONAPICoreEx aCore;
    aCore.SetDefaultFileDirectory("./");
    aCore.Startup();
    CdKeyPhase(500 * scale);
    printf("cdkey: %d checks\n", sChecks);
    int c = sChecks;
    DirPhase(5000 * scale);
    printf("dir: %d checks\n", sChecks - c);
    c = sChecks;
    ServerOpPhase(500 * scale);
    printf("serverop: %d checks\n", sChecks - c);
    c = sChecks;
    RoutingPhase(1000 * scale);
    printf("routing: %d checks\n", sChecks - c);
    c = sChecks;
    ThreadPhase(sMutate ? 4 : 10);
    printf("thread: %d checks\n", sChecks - c);
    for (auto& f : sFailedPhases) printf("  %s: %d mismatches\n", f.first.c_str(), f.second);
    printf("won_net33_test%s: %d checks, %d mismatches\n", sMutate ? (" (mutation " + N(sMutate) + ")").c_str() : "", sChecks, sFailures);
    return sFailures;
}
