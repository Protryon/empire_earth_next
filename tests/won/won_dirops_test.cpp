// The directory server's ops (WONDir: GetEntityRequest, GetDirOp, GetMultiDirOp, GetAliasDirOp, GetServiceOp,
// FindEntityOp, AddEntityOp / AddServiceOp, ModifyEntityOp, RemoveEntityOp, RenameEntityOp, RenewEntityOp,
// EntitySetDataObjectsOp, EntityModifyDataObjectsOp, EntityClearDataObjectsOp): the original's statements (copied
// from the C++ before the port, the members replaced by generated values) against ee_won's dirops module through
// its C ABI. Every request's bytes over generated fields; the status replies (generated, cut at every length,
// mutated); the multi-directory replies' accounting over generated status runs; FindEntityOp's parser choice;
// GetDirEntityMap's grouping over generated listings. The exit code is the number of failed checks.
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
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_rust.h"

using namespace WONAPI;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 4242;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

static std::string Bytes(WriteBuffer& inBuffer)
{
    ConstSmartPtr<ByteBuffer> aBuffer = inBuffer.ToByteBuffer(true);
    return std::string(aBuffer->data(), aBuffer->length());
}

// ---- generated fields

static std::wstring AWString()
{
    static const wchar_t* const kTexts[] = { L"", L"/", L"/EE", L"/EE/", L"Titan", L"n\x263a", L"\x00e9t\x00e9",
                                             L"a/b/c" };
    std::wstring s = kTexts[Pick(8)];
    if (Pick(4) == 0) s += std::wstring(Pick(300), L'x');
    return s;
}
static std::string AString()
{
    static const char* const kTexts[] = { "", "t", "type2", "\xff\x01", "HostName" };
    std::string s = kTexts[Pick(5)];
    if (Pick(6) == 0) s += std::string(Pick(300), 'y');
    return s;
}
static ByteBufferPtr ABuffer()
{
    if (Pick(4) == 0) return NULL;
    std::string s(Pick(Pick(8) ? 8 : 400), '\0');
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)Pick(256);
    return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
}
static DWORD AFlags()
{
    static const DWORD kFlags[] = { 0, 1, 2, 3, 4, 5, 6, 7, 0x10, 0x12, 0x06020837, 0xffffffff, 0x80000002 };
    return Pick(3) ? kFlags[Pick(13)] : (DWORD)(Next() * 0x10001u);
}

struct Fields {
    int mOp;
    bool mIsService, mIsUnique;
    DWORD mFlags;
    std::wstring mPath, mName, mDisplayName, mNewName, mNewDisplayName;
    ByteBufferPtr mNetAddr, mNewNetAddr;
    unsigned long mLifespan;
    unsigned char mMode, mFindFlags;
    DirDataObjectList mObjects;
    struct Mod { DirDataObject mObject; bool mIsInsert; unsigned short mOffset; };
    std::list<Mod> mMods;
    DirDataTypeSet mTypes;
    struct Query { DWORD mFlags; std::wstring mPath; DirDataTypeSet mTypes; };
    std::list<Query> mQueries;
    std::string mProduct;
    unsigned char mLength;
};

static DirDataTypeSet ATypes()
{
    DirDataTypeSet s;
    int n = Pick(5) == 0 ? Pick(300) : Pick(5);
    for (int i = 0; i < n; i++) {
        char aBuffer[16];
        snprintf(aBuffer, sizeof(aBuffer), "%d", Pick(1000));
        s.insert(Pick(2) ? AString() : std::string(aBuffer));
    }
    return s;
}

static Fields Generate()
{
    Fields f;
    f.mOp = Pick(13);
    f.mIsService = Pick(2) != 0;
    f.mIsUnique = Pick(2) != 0;
    f.mFlags = AFlags();
    f.mPath = AWString();
    f.mName = AWString();
    f.mDisplayName = AWString();
    f.mNewName = AWString();
    f.mNewDisplayName = AWString();
    f.mNetAddr = ABuffer();
    f.mNewNetAddr = ABuffer();
    f.mLifespan = Pick(2) ? Pick(1000) : (unsigned long)(Next() * 397u);
    f.mMode = (unsigned char)Pick(256);
    f.mFindFlags = (unsigned char)Pick(4);
    int n = Pick(6);
    for (int i = 0; i < n; i++) {
        DirDataObject o;
        o.mDataType = AString();
        o.mData = ABuffer();
        f.mObjects.push_back(o);
        Fields::Mod m;
        m.mObject = o;
        m.mIsInsert = Pick(2) != 0;
        m.mOffset = (unsigned short)(Pick(2) ? Pick(10) : Next());
        f.mMods.push_back(m);
    }
    f.mTypes = ATypes();
    n = Pick(5) == 0 ? 250 + Pick(10) : Pick(5);
    for (int i = 0; i < n; i++) {
        Fields::Query q;
        q.mFlags = AFlags();
        q.mPath = AWString();
        q.mTypes = Pick(2) ? ATypes() : DirDataTypeSet();
        f.mQueries.push_back(q);
    }
    f.mProduct = AString();
    static const unsigned char kLengths[] = { 0, 1, 2, 4 };
    f.mLength = Pick(4) ? 4 : kLengths[Pick(4)];
    return f;
}

// ---- the original's statements (each op's GetNextRequest before the port, the members replaced by the fields)

static void OrigPack(WriteBuffer* message, DWORD inFlags, const std::wstring& inPath, const DirDataTypeSet& inTypes,
                     bool inMulti)
{
    DWORD flags = inFlags; // GetEntityRequest::Pack
    bool aNoDirs = (flags & 2) == 0;
    if (!aNoDirs && (flags & 5) != 0)
        flags |= 0x10;
    message->AppendLong(flags);
    if (!inMulti)
        message->AppendShort(0);
    else
        message->AppendByte(0x44);
    message->AppendWString(inPath, 2);
    message->AppendShort(static_cast<short>(inTypes.size()));
    for (DirDataTypeSet::const_iterator i = inTypes.begin(); i != inTypes.end(); ++i)
        message->AppendString(*i, 1);
}

static std::string Orig(const Fields& f)
{
    WriteBuffer aMsg(f.mLength, true);
    switch (f.mOp) {
    case 0: // GetDirOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(103);
        OrigPack(&aMsg, f.mFlags, f.mPath, f.mTypes, false);
        break;
    case 1: // GetMultiDirOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(113);
        aMsg.AppendShort(0);
        aMsg.AppendByte(static_cast<unsigned char>(f.mQueries.size()));
        for (std::list<Fields::Query>::const_iterator i = f.mQueries.begin(); i != f.mQueries.end(); ++i)
            OrigPack(&aMsg, i->mFlags, i->mPath, i->mTypes, true);
        break;
    case 2: // GetAliasDirOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(114);
        aMsg.AppendShort(0);
        aMsg.AppendString(f.mProduct);
        aMsg.AppendByte(f.mQueries.size());
        for (std::list<Fields::Query>::const_iterator i = f.mQueries.begin(); i != f.mQueries.end(); ++i)
            OrigPack(&aMsg, i->mFlags, i->mPath, i->mTypes, true);
        break;
    case 3: { // GetServiceOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(105);
        aMsg.AppendLong(f.mFlags);
        aMsg.AppendWString(f.mPath, 2);
        aMsg.AppendWString(f.mName, 2);
        aMsg.AppendBuffer(f.mNetAddr, 1);
        aMsg.AppendShort(f.mTypes.size());
        DirDataTypeSet::const_iterator anItr = f.mTypes.begin();
        while (anItr != f.mTypes.end()) {
            aMsg.AppendString(*anItr, 1);
            ++anItr;
        }
        break;
    }
    case 4: { // FindEntityOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 109 : 107);
        aMsg.AppendLong(f.mFlags);
        aMsg.AppendByte(f.mMode);
        aMsg.AppendByte(f.mFindFlags);
        aMsg.AppendShort(0);
        aMsg.AppendWString(f.mPath, 2);
        aMsg.AppendWString(f.mName, 2);
        if (f.mIsService)
            aMsg.AppendBuffer(f.mNetAddr, 1);
        aMsg.AppendWString(f.mDisplayName, 2);
        aMsg.AppendShort(f.mObjects.size());
        DirDataObjectList::const_iterator aFindItr = f.mObjects.begin();
        while (aFindItr != f.mObjects.end()) {
            const DirDataObject& aDataObject = *aFindItr;
            aMsg.AppendString(aDataObject.mDataType, 1);
            aMsg.AppendBuffer(aDataObject.mData, 2);
            ++aFindItr;
        }
        aMsg.AppendShort(f.mTypes.size());
        DirDataTypeSet::const_iterator aGetItr = f.mTypes.begin();
        while (aGetItr != f.mTypes.end()) {
            aMsg.AppendString(*aGetItr, 1);
            ++aGetItr;
        }
        break;
    }
    case 5: { // AddEntityOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 215 : 214);
        aMsg.AppendByte(f.mFlags);
        aMsg.AppendWString(f.mPath, 2);
        aMsg.AppendWString(f.mName, 2);
        if (f.mIsService)
            aMsg.AppendBuffer(f.mNetAddr, 1);
        aMsg.AppendWString(f.mDisplayName, 2);
        aMsg.AppendLong(f.mLifespan);
        aMsg.AppendShort(f.mObjects.size());
        DirDataObjectList::const_iterator anItr = f.mObjects.begin();
        while (anItr != f.mObjects.end()) {
            aMsg.AppendString(anItr->mDataType, 1);
            aMsg.AppendBuffer(anItr->mData, 2);
            ++anItr;
        }
        aMsg.AppendShort(0);
        break;
    }
    case 6: { // ModifyEntityOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 217 : 216);
        aMsg.AppendByte(f.mFlags);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        aMsg.AppendWString(f.mNewName, 2);
        if (f.mIsService)
            aMsg.AppendBuffer(f.mNewNetAddr, 1);
        aMsg.AppendWString(f.mNewDisplayName, 2);
        aMsg.AppendLong(f.mLifespan);
        aMsg.AppendShort(f.mObjects.size());
        DirDataObjectList::const_iterator anItr = f.mObjects.begin();
        while (anItr != f.mObjects.end()) {
            aMsg.AppendString(anItr->mDataType, 1);
            aMsg.AppendBuffer(anItr->mData, 2);
            ++anItr;
        }
        aMsg.AppendShort(0);
        break;
    }
    case 7: // RemoveEntityOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 209 : 208);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        break;
    case 8: // RenameEntityOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 207 : 206);
        aMsg.AppendBool(f.mIsUnique);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        aMsg.AppendWString(f.mNewDisplayName, 2);
        break;
    case 9: // RenewEntityOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 205 : 204);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        aMsg.AppendLong(f.mLifespan);
        break;
    case 10: { // EntitySetDataObjectsOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 301 : 300);
        aMsg.AppendByte(f.mMode);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        aMsg.AppendShort(f.mObjects.size());
        DirDataObjectList::const_iterator anItr = f.mObjects.begin();
        while (anItr != f.mObjects.end()) {
            aMsg.AppendString(anItr->mDataType, 1);
            aMsg.AppendBuffer(anItr->mData, 2);
            ++anItr;
        }
        break;
    }
    case 11: { // EntityModifyDataObjectsOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 305 : 304);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        aMsg.AppendShort(f.mMods.size());
        std::list<Fields::Mod>::const_iterator anItr = f.mMods.begin();
        while (anItr != f.mMods.end()) {
            aMsg.AppendShort(anItr->mOffset);
            aMsg.AppendBool(anItr->mIsInsert);
            aMsg.AppendString(anItr->mObject.mDataType, 1);
            aMsg.AppendBuffer(anItr->mObject.mData, 2);
            ++anItr;
        }
        break;
    }
    case 12: { // EntityClearDataObjectsOp
        aMsg.AppendByte(5);
        aMsg.AppendShort(2);
        aMsg.AppendShort(f.mIsService ? 303 : 302);
        aMsg.AppendWString(f.mPath, 2);
        if (f.mIsService) {
            aMsg.AppendWString(f.mName, 2);
            aMsg.AppendBuffer(f.mNetAddr, 1);
        }
        aMsg.AppendShort(f.mTypes.size());
        DirDataTypeSet::const_iterator anItr = f.mTypes.begin();
        while (anItr != f.mTypes.end()) {
            aMsg.AppendString(*anItr, 1);
            ++anItr;
        }
        break;
    }
    }
    return Bytes(aMsg);
}

// ---- the same fields through the C ABI (as the ported ops hand them over)

struct Store {
    std::list<std::string> mTexts;
    EeWonDirBytes Text(const std::wstring& s)
    {
        mTexts.push_back(std::string());
        WideAppendUtf16LE(mTexts.back(), s);
        EeWonDirBytes b = { reinterpret_cast<const uint8_t*>(mTexts.back().data()), mTexts.back().size(), true };
        return b;
    }
    static EeWonDirBytes Narrow(const std::string& s)
    {
        EeWonDirBytes b = { reinterpret_cast<const uint8_t*>(s.data()), s.size(), true };
        return b;
    }
    static EeWonDirBytes Buffer(const ByteBuffer* inBuffer)
    {
        EeWonDirBytes b = { NULL, 0, false };
        if (inBuffer != NULL) {
            b.data = reinterpret_cast<const uint8_t*>(inBuffer->data());
            b.len = inBuffer->length();
            b.present = true;
        }
        return b;
    }
};

static std::string Rust(const Fields& f)
{
    Store s;
    std::vector<EeWonDirObject> anObjects;
    for (DirDataObjectList::const_iterator i = f.mObjects.begin(); i != f.mObjects.end(); ++i) {
        EeWonDirObject o = { Store::Narrow(i->mDataType), Store::Buffer(i->mData.get()) };
        anObjects.push_back(o);
    }
    std::vector<EeWonDirMod> aMods;
    for (std::list<Fields::Mod>::const_iterator i = f.mMods.begin(); i != f.mMods.end(); ++i) {
        EeWonDirMod m;
        m.offset = i->mOffset;
        m.insert = i->mIsInsert;
        m.object.data_type = Store::Narrow(i->mObject.mDataType);
        m.object.data = Store::Buffer(i->mObject.mData.get());
        aMods.push_back(m);
    }
    std::vector<EeWonDirBytes> aTypes;
    for (DirDataTypeSet::const_iterator i = f.mTypes.begin(); i != f.mTypes.end(); ++i)
        aTypes.push_back(Store::Narrow(*i));
    std::vector<std::vector<EeWonDirBytes> > aQueryTypes;
    std::vector<EeWonDirQuery> aQueries;
    for (std::list<Fields::Query>::const_iterator i = f.mQueries.begin(); i != f.mQueries.end(); ++i) {
        aQueryTypes.push_back(std::vector<EeWonDirBytes>());
        for (DirDataTypeSet::const_iterator t = i->mTypes.begin(); t != i->mTypes.end(); ++t)
            aQueryTypes.back().push_back(Store::Narrow(*t));
    }
    size_t k = 0;
    for (std::list<Fields::Query>::const_iterator i = f.mQueries.begin(); i != f.mQueries.end(); ++i, ++k) {
        EeWonDirQuery q = { i->mFlags, s.Text(i->mPath), aQueryTypes[k].empty() ? NULL : &aQueryTypes[k][0],
                            aQueryTypes[k].size() };
        aQueries.push_back(q);
    }
    EeWonDirRequest r;
    memset(&r, 0, sizeof(r));
    r.op = f.mOp;
    r.is_service = f.mIsService;
    r.flags = f.mFlags;
    r.path = s.Text(f.mPath);
    r.name = s.Text(f.mName);
    r.display_name = s.Text(f.mDisplayName);
    r.net_addr = Store::Buffer(f.mNetAddr.get());
    r.new_name = s.Text(f.mNewName);
    r.new_display_name = s.Text(f.mNewDisplayName);
    r.new_net_addr = Store::Buffer(f.mNewNetAddr.get());
    r.lifespan = (uint32_t)f.mLifespan;
    r.is_unique = f.mIsUnique;
    r.mode = f.mMode;
    r.find_flags = f.mFindFlags;
    r.objects = anObjects.empty() ? NULL : &anObjects[0];
    r.object_count = anObjects.size();
    r.mods = aMods.empty() ? NULL : &aMods[0];
    r.mod_count = aMods.size();
    r.types = aTypes.empty() ? NULL : &aTypes[0];
    r.type_count = aTypes.size();
    r.queries = aQueries.empty() ? NULL : &aQueries[0];
    r.query_count = aQueries.size();
    r.product = Store::Narrow(f.mProduct);
    std::string anOut;
    ee_won_dir_request(&r, f.mLength, Append, &anOut);
    return anOut;
}

// ---- replies

struct StatusOut {
    int mEnd, mStatus;
    bool mHasAddr;
    std::string mAddr;
};

// AddEntityOp / ModifyEntityOp::CheckResponse (the others the same without the address)
static StatusOut OrigStatus(const std::string& inReply, bool inIsService, DWORD inFlags)
{
    StatusOut o = { 0, 0, false, std::string() };
    try {
        ReadBuffer aMsg(inReply.data(), inReply.size());
        unsigned char aHeaderType = aMsg.ReadByte();
        unsigned short aServiceType = aMsg.ReadShort();
        unsigned short aMessageType = aMsg.ReadShort();
        if (aHeaderType != 5 || aServiceType != 2 || aMessageType != 1) {
            o.mEnd = 1;
            return o;
        }
        short aStatus = aMsg.ReadShort();
        if (aStatus >= 0 && inIsService) {
            if ((inFlags & 0x10) != 0) {
                unsigned char aLen = aMsg.ReadByte();
                o.mAddr.assign(static_cast<const char*>(aMsg.ReadBytes(aLen)), aLen);
                o.mHasAddr = true;
            }
        }
        o.mStatus = aStatus;
    } catch (ReadBufferException&) {
        o.mEnd = 2;
    }
    return o;
}

static void TestReplies()
{
    for (int i = 0; i < 20000; i++) {
        WriteBuffer aBuf(0, true);
        aBuf.AppendByte(Pick(10) ? 5 : (unsigned char)Pick(256));
        aBuf.AppendShort(Pick(10) ? 2 : (short)Pick(0x10000));
        aBuf.AppendShort(Pick(10) ? 1 : (short)Pick(4));
        static const short kStatus[] = { 0, 1, -1, -1201, 32767, -32768, 100 };
        aBuf.AppendShort(kStatus[Pick(7)]);
        if (Pick(3)) {
            int n = Pick(4) ? Pick(8) : 255;
            aBuf.AppendByte((unsigned char)n);
            for (int k = 0; k < n; k++) aBuf.AppendByte((unsigned char)Pick(256));
        }
        std::string aReply = Bytes(aBuf);
        if (Pick(6) == 0) aReply += std::string(Pick(5), 'z');
        bool aService = Pick(2) != 0;
        DWORD aFlags = Pick(2) ? 0x10 : (DWORD)Pick(0x40);
        for (size_t aCut = 0; aCut <= aReply.size(); aCut++) {
            std::string aPart = aReply.substr(0, aCut);
            StatusOut o = OrigStatus(aPart, aService, aFlags);
            EeWonDirStatusReply r;
            ee_won_dir_status_reply(reinterpret_cast<const uint8_t*>(aPart.data()), aPart.size(),
                                    aService && (aFlags & 0x10) != 0, &r);
            CHECK(r.end == o.mEnd);
            if (o.mEnd == 0) {
                CHECK(r.status == o.mStatus);
                CHECK(r.has_addr == o.mHasAddr);
                if (o.mHasAddr) CHECK(aPart.substr(r.addr_at, r.addr_len) == o.mAddr);
            }
        }
    }
}

static void TestMulti()
{
    // GetMultiDirOp::CheckResponse and GetAliasDirOp::CheckResponse over runs of statuses
    for (int t = 0; t < 20000; t++) {
        int aCount = Pick(6);
        std::vector<int> aStatuses;
        int n = Pick(20);
        for (int i = 0; i < n; i++) {
            static const int kStatus[] = { 100059, 100059, 0, -1201, 100063, 5 };
            aStatuses.push_back(kStatus[Pick(6)]);
        }
        // the original: mCurRequest runs over aCount requests
        std::vector<int> aWant, aGot, aWantAlias;
        {
            int mCur = 0, mNumFailedRequests = 0;
            for (size_t i = 0; i < aStatuses.size() && mCur < aCount; i++) {
                if (aStatuses[i] != 100059) {
                    if (aStatuses[i] != 0) ++mNumFailedRequests;
                    ++mCur;
                }
                int r;
                if (mCur == aCount) {
                    if (mNumFailedRequests == 0) r = 0;
                    else r = mNumFailedRequests == static_cast<int>(aCount) ? -1224 : -1223;
                } else r = 100059;
                aWant.push_back(r);
            }
        }
        {
            int mCur = 0, mNumFailedRequests = 0;
            for (size_t i = 0; i < aStatuses.size() && mCur < aCount; i++) {
                if (aStatuses[i] != 100059) {
                    if (aStatuses[i] != 0) ++mNumFailedRequests;
                    ++mCur;
                }
                int r;
                if (mCur == aCount) {
                    if (mNumFailedRequests == 0) r = 0;
                    else if (mNumFailedRequests != (int)(size_t)aCount) r = -1223;
                    else r = -1224;
                } else r = 100059;
                aWantAlias.push_back(r);
            }
        }
        {
            int aCur = 0;
            int32_t aFailed = 0;
            for (size_t i = 0; i < aStatuses.size() && aCur < aCount; i++) {
                if (ee_won_dir_multi_step(aStatuses[i], &aFailed)) ++aCur;
                aGot.push_back(aCur == aCount ? ee_won_dir_multi_end(aFailed, aCount) : 100059);
            }
        }
        CHECK(aGot == aWant);
        CHECK(aGot == aWantAlias);
    }
    for (int s = 0; s < 2; s++)
        for (int f = 0; f < 256; f++)
            CHECK(ee_won_dir_find_single(s != 0, (unsigned char)f) == (s && !(f & 1)));
}

// GetEntityRequest::GetDirEntityMap's statements over (type, path, name) entities: the key of each entity
static std::vector<std::wstring> OrigGroups(const std::wstring& inPath, const std::vector<char>& inTypes,
                                            const std::vector<std::wstring>& inPaths,
                                            const std::vector<std::wstring>& inNames)
{
    typedef std::map<std::wstring, std::vector<int> > Map;
    Map aMap;
    Map::iterator aMapItr = aMap.insert(Map::value_type(inPath, std::vector<int>())).first;
    std::wstring currentDir = inPath;
    for (size_t i = 0; i < inTypes.size(); i++) {
        const std::wstring& aPath = inPaths[i];
        bool mapInsert = false;
        if (inTypes[i] == 'D') {
            if (aPath.empty())
                currentDir = inPath;
            else
                currentDir = aPath;
            if (currentDir.empty() || currentDir[currentDir.length() - 1] != L'/')
                currentDir += L'/';
            currentDir += inNames[i];
            mapInsert = true;
        } else if (!aPath.empty() && aPath != currentDir) {
            currentDir = aPath;
            mapInsert = true;
        }
        if (mapInsert)
            aMapItr = aMap.insert(aMapItr, Map::value_type(currentDir, std::vector<int>()));
        aMapItr->second.push_back((int)i);
    }
    std::vector<std::wstring> aKeys(inTypes.size());
    for (Map::iterator i = aMap.begin(); i != aMap.end(); ++i)
        for (size_t k = 0; k < i->second.size(); k++) aKeys[i->second[k]] = i->first;
    return aKeys;
}

static void AppendKey(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    std::vector<std::wstring>* aKeys = static_cast<std::vector<std::wstring>*>(inContext);
    aKeys->push_back(std::wstring());
    for (size_t i = 0; i + 1 < inLength; i += 2) WideAppendUtf16Unit(aKeys->back(), inBytes[i] | (inBytes[i + 1] << 8));
}

static void TestGroups()
{
    static const wchar_t* const kPaths[] = { L"", L"", L"/EE", L"/EE/", L"/EE/sub", L"/x", L"/" };
    static const wchar_t* const kNames[] = { L"", L"sub", L"a", L"\x263a" };
    for (int t = 0; t < 30000; t++) {
        std::wstring aPath = kPaths[Pick(7)];
        int n = Pick(10);
        std::vector<char> aTypes;
        std::vector<std::wstring> aPaths, aNames;
        for (int i = 0; i < n; i++) {
            aTypes.push_back(Pick(3) == 0 ? 'D' : Pick(4) == 0 ? 0 : 'S');
            aPaths.push_back(kPaths[Pick(7)]);
            aNames.push_back(kNames[Pick(4)]);
        }
        std::vector<std::wstring> aWant = OrigGroups(aPath, aTypes, aPaths, aNames), aGot;
        Store s;
        std::vector<EeWonDirListed> aList;
        for (int i = 0; i < n; i++) {
            EeWonDirListed e = { (uint8_t)aTypes[i], s.Text(aPaths[i]), s.Text(aNames[i]) };
            aList.push_back(e);
        }
        ee_won_dir_group_keys(s.Text(aPath), aList.empty() ? NULL : &aList[0], aList.size(), AppendKey, &aGot);
        CHECK(aGot == aWant);
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    for (int i = 0; i < 30000; i++) {
        Fields f = Generate();
        std::string aWant = Orig(f), aGot = Rust(f);
        CHECK(aGot == aWant);
        if (aGot != aWant && sFailures < 4) printf("op %d: %zu vs %zu bytes\n", f.mOp, aGot.size(), aWant.size());
    }
    for (int p = 0; p < 0x10000; p += 1 + Pick(40)) {
        WriteBuffer theBuf(0, true); // AddServiceOp::SetNetAddrPort
        theBuf.AppendByte(static_cast<unsigned char>(p >> 8));
        theBuf.AppendByte(static_cast<unsigned char>(p & 0xff));
        std::string aWant = Bytes(theBuf), aGot;
        ee_won_dir_port_net_addr((uint16_t)p, Append, &aGot);
        CHECK(aGot == aWant);
    }
    TestReplies();
    TestMulti();
    TestGroups();
    printf("won_dirops_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
