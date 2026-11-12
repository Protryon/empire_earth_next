// WONDB (ProfileData, DBProxyOp, CreateAccountOp, GetAccountOp, UpdateAccountOp, GetUserIdFromNameOp, the email
// requests, ContestCallOp, EventReportOp): the original's statements (copied from the C++ before the port, the
// members replaced by locals) against the new code: ProfileData's shim over ee_won's Rust profile (its unit
// linked here) and include/ee/wonapi/WONDBMsg.h over ee_won's db module. Profiles over generated edits, packed
// and unpacked from generated replies (cut, mutated); every request over generated members; every reply
// generated, cut at every length and mutated, the members it assigns, the return or the throw compared. The exit
// code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONDBMsg.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 5151;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static std::string Bytes(WriteBuffer& inBuffer)
{
    ConstSmartPtr<ByteBuffer> aBuffer = inBuffer.ToByteBuffer(true);
    return std::string(aBuffer->data(), aBuffer->length());
}
static std::string Str(const ByteBuffer* b) { return b == NULL ? std::string("<null>") : std::string(b->data(), b->length()); }

static std::wstring AWString()
{
    static const wchar_t* const kTexts[] = { L"", L"bob", L"p\x263a", L"\x00e9t\x00e9" };
    std::wstring s = kTexts[Pick(4)];
    if (Pick(8) == 0) s += std::wstring(Pick(300), L'x');
    return s;
}
static std::string AString()
{
    static const char* const kTexts[] = { "", "a@b.c", "\xff\x01" };
    std::string s = kTexts[Pick(3)];
    if (Pick(8) == 0) s += std::string(Pick(300), 'y');
    return s;
}
static ByteBufferPtr ABuffer()
{
    if (Pick(4) == 0) return NULL;
    std::string s(Pick(Pick(8) ? 8 : 300), '\0');
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)Pick(256);
    return new ByteBuffer(s.data(), (unsigned long)s.size(), false);
}
static unsigned short AShort() { return (unsigned short)(Pick(3) ? Pick(4) : Next()); }

// ---- the original ProfileData (its members and methods)

struct OrigProfile {
    OrigProfile() : mBirthMonth(0),mBirthDay(0),mBirthYear(0),mCustomActionId(0) {}
    unsigned char mBirthMonth;
    unsigned char mBirthDay;
    unsigned short mBirthYear;
    std::map<unsigned short,bool> mNewsletterSubs;
    unsigned short mCustomActionId;
    ByteBufferPtr mCustomActionData;
    void AddNewsletterSub(unsigned short inId,bool inSubscribe) { this->mNewsletterSubs[inId]=inSubscribe; }
    bool GetNewsletterSub(unsigned short inId)
    {
        if (this->mNewsletterSubs.find(inId)==this->mNewsletterSubs.end()) return false;
        return this->mNewsletterSubs[inId];
    }
    void ClearNewsletterSubs() { this->mNewsletterSubs.clear(); }
    void PackRequestData(WriteBuffer& inMsg,bool inIsSet)
    {
        unsigned short fieldCount=0;
        unsigned long fieldCountPos=inMsg.length();
        inMsg.AppendShort(0);
        std::map<unsigned short,bool>::iterator it=this->mNewsletterSubs.begin();
        while (it!=this->mNewsletterSubs.end()) {
            inMsg.AppendShort(1);
            inMsg.AppendShort(inIsSet ? 3 : 2);
            inMsg.AppendShort(it->first);
            if (inIsSet) inMsg.AppendBool(it->second);
            ++fieldCount;
            ++it;
        }
        if (this->mCustomActionId!=0) {
            inMsg.AppendShort(2);
            unsigned long dataLen=0;
            if (this->mCustomActionData.get()!=NULL) dataLen=this->mCustomActionData->length();
            inMsg.AppendShort(dataLen+2);
            inMsg.AppendShort(this->mCustomActionId);
            inMsg.AppendBuffer(this->mCustomActionData.get(),0);
            ++fieldCount;
        }
        inMsg.AppendShort(3);
        inMsg.AppendShort(4);
        inMsg.AppendByte(this->mBirthMonth);
        inMsg.AppendByte(this->mBirthDay);
        inMsg.AppendShort(this->mBirthYear);
        ++fieldCount;
        inMsg.SetShort(fieldCountPos,fieldCount);
    }
    void UnpackGetData(ReadBuffer& inMsg)
    {
        try {
            short fieldCount=inMsg.ReadShort();
            for (int i=0;i<static_cast<unsigned short>(fieldCount);++i) {
                unsigned short field=static_cast<unsigned short>(inMsg.ReadShort());
                unsigned short length=static_cast<unsigned short>(inMsg.ReadShort());
                switch (field) {
                case 1: {
                    unsigned short id=static_cast<unsigned short>(inMsg.ReadShort());
                    bool& subscription=this->mNewsletterSubs[id];
                    subscription=inMsg.ReadBool();
                    break;
                }
                case 2:
                    inMsg.ReadShort();
                    this->mCustomActionData=new ByteBuffer(inMsg.ReadBytes(static_cast<unsigned long>(length)-2),static_cast<unsigned long>(length)-2,false);
                    break;
                case 3:
                    this->mBirthMonth=inMsg.ReadByte();
                    this->mBirthDay=inMsg.ReadByte();
                    this->mBirthYear=inMsg.ReadShort();
                    break;
                default:
                    break;
                }
            }
        }
        catch (ReadBufferException&) {
        }
    }
    void SetBirthDate(unsigned char inMonth,unsigned char inDay,unsigned short inYear)
    { this->mBirthMonth=inMonth; this->mBirthDay=inDay; this->mBirthYear=inYear; }
    void ClearNewsletterSub(unsigned short theId) { this->mNewsletterSubs.erase(theId); }
    void SetCustomAction(unsigned short theActionId, const ByteBuffer *theData)
    { this->mCustomActionId = theActionId; this->mCustomActionData = theData; }
    void ClearCustomAction() { this->mCustomActionId = 0; this->mCustomActionData = NULL; }
    unsigned long GetBirthDate() { return (this->mBirthMonth) | (this->mBirthDay<<8) | (this->mBirthYear<<16); }
};

static std::string Pack(OrigProfile& p, bool set) { WriteBuffer w(0, true); p.PackRequestData(w, set); return Bytes(w); }
static std::string Pack(ProfileData* p, bool set) { WriteBuffer w(0, true); p->PackRequestData(w, set); return Bytes(w); }

static void SameProfile(OrigProfile& a, ProfileData* b)
{
    CHECK(Pack(a, true) == Pack(b, true));
    CHECK(Pack(a, false) == Pack(b, false));
    CHECK(a.GetBirthDate() == b->GetBirthDate());
    for (int id = 0; id < 5; id++) CHECK(a.GetNewsletterSub((unsigned short)id) == b->GetNewsletterSub((unsigned short)id));
}

static void Edit(OrigProfile& a, ProfileData* b)
{
    switch (Pick(7)) {
    case 0: { unsigned short id = AShort(); bool on = Pick(2) != 0; a.AddNewsletterSub(id, on); b->AddNewsletterSub(id, on); break; }
    case 1: a.ClearNewsletterSubs(); b->ClearNewsletterSubs(); break;
    case 2: { unsigned short id = AShort(); a.ClearNewsletterSub(id); b->ClearNewsletterSub(id); break; }
    case 3: { unsigned short id = Pick(4) ? 1 + Pick(3) : AShort(); ByteBufferPtr d = ABuffer(); a.SetCustomAction(id, d.get()); b->SetCustomAction(id, d.get()); break; }
    case 4: a.ClearCustomAction(); b->ClearCustomAction(); break;
    default: {
        unsigned char m = (unsigned char)Pick(256), d = (unsigned char)Pick(256);
        unsigned short y = (unsigned short)(Pick(2) ? 1900 + Pick(200) : Next());
        a.SetBirthDate(m, d, y);
        b->SetBirthDate(m, d, y);
        break;
    }
    }
}

// a profile get reply's fields
static std::string ProfileReply()
{
    WriteBuffer w(0, true);
    int n = Pick(5);
    w.AppendShort((short)(Pick(10) ? n : Pick(0x10000)));
    for (int i = 0; i < n; i++) {
        int f = Pick(5) ? 1 + Pick(3) : Pick(10);
        w.AppendShort((short)f);
        if (f == 1) { w.AppendShort(3); w.AppendShort(AShort()); w.AppendByte((unsigned char)Pick(3)); }
        else if (f == 2) {
            int len = Pick(6);
            w.AppendShort((short)(Pick(6) ? len + 2 : Pick(3)));
            w.AppendShort(AShort());
            for (int j = 0; j < len; j++) w.AppendByte((unsigned char)Pick(256));
        } else if (f == 3) { w.AppendShort(4); w.AppendByte((unsigned char)Pick(13)); w.AppendByte((unsigned char)Pick(32)); w.AppendShort((short)Next()); }
        else w.AppendShort((short)Pick(5));
    }
    return Bytes(w);
}

static void TestProfiles()
{
    for (int i = 0; i < 20000; i++) {
        OrigProfile a;
        SmartPtr<ProfileData> b = new ProfileData;
        int n = Pick(8);
        for (int j = 0; j < n; j++) Edit(a, b.get());
        SameProfile(a, b.get());
        std::string r = ProfileReply();
        size_t cut = Pick(3) ? r.size() : Pick((int)r.size() + 1);
        std::string m = r.substr(0, cut);
        if (Pick(4) == 0 && !m.empty()) m[Pick((int)m.size())] = (char)Pick(256);
        std::string pre(Pick(3), 'z');
        m = pre + m;
        ReadBuffer ra(m.data(), (unsigned long)m.size()), rb(m.data(), (unsigned long)m.size());
        ra.SetPos((int)pre.size());
        rb.SetPos((int)pre.size());
        a.UnpackGetData(ra);
        b->UnpackGetData(rb);
        CHECK(ra.pos() == rb.pos());
        SameProfile(a, b.get());
    }
}

// ---- requests

struct Fields {
    unsigned char mLengthFieldSize;
    short mMessageType, mSubMessageType;
    ByteBufferPtr mData;
    std::wstring mUserName, mPassword;
    std::string mEmail;
    unsigned long mUserId;
    unsigned short mProcNum;
};

static void TestRequests()
{
    static const unsigned char kSizes[] = { 0, 1, 2, 4 };
    for (int i = 0; i < 30000; i++) {
        Fields f;
        f.mLengthFieldSize = kSizes[Pick(4)];
        f.mMessageType = (short)AShort();
        f.mSubMessageType = (short)AShort();
        f.mData = ABuffer();
        f.mUserName = AWString();
        f.mPassword = AWString();
        f.mEmail = AString();
        f.mUserId = (unsigned long)Next() * 391u;
        f.mProcNum = AShort();
        OrigProfile a;
        SmartPtr<ProfileData> b = new ProfileData;
        int n = Pick(6);
        for (int j = 0; j < n; j++) Edit(a, b.get());
        std::string want, got;
        int kind = Pick(11);
        DbRequest q(kind, f.mLengthFieldSize);
        q.mReq.message = f.mMessageType;
        q.mReq.sub = f.mSubMessageType;
        q.mReq.user_id = (uint32_t)f.mUserId;
        q.mReq.data = DbRequest::Buffer(f.mData.get());
        q.mReq.name = q.Text(f.mUserName);
        q.mReq.password = q.Text(f.mPassword);
        q.mReq.email = DbRequest::Narrow(f.mEmail);
        q.mReq.profile = b->GetProfile();
        switch (kind) {
        case EE_WON_DB_REQ_PROXY: {
            WriteBuffer aRequest(f.mLengthFieldSize,true);
            aRequest.AppendByte(5);
            aRequest.AppendShort(14);
            aRequest.AppendShort(f.mMessageType);
            aRequest.AppendShort(f.mSubMessageType);
            aRequest.AppendBuffer(f.mData.get(),0);
            want = Bytes(aRequest);
            break;
        }
        case EE_WON_DB_REQ_CREATE_KEY: {
            WriteBuffer request(f.mLengthFieldSize,true);
            request.AppendByte(5);
            request.AppendShort(14);
            request.AppendShort(2);
            request.AppendShort(1);
            want = Bytes(request);
            break;
        }
        case EE_WON_DB_REQ_CREATE_SESSION: {
            WriteBuffer request(f.mLengthFieldSize,true);
            request.AppendByte(5);
            request.AppendShort(14);
            request.AppendShort(4);
            request.AppendShort(1);
            request.AppendBuffer(f.mData.get(),2);
            want = Bytes(request);
            break;
        }
        case EE_WON_DB_REQ_CREATE_DATA: {
            WriteBuffer account(0,true);
            account.AppendByte(5);
            account.AppendShort(14);
            account.AppendShort(1002);
            account.AppendShort(2);
            account.AppendWString(f.mUserName,2);
            account.AppendWString(f.mPassword,2);
            account.AppendString(f.mEmail,2);
            a.PackRequestData(account,true);
            want = std::string(account.data(), account.length());
            break;
        }
        case EE_WON_DB_REQ_CREATE_ACCOUNT: {
            WriteBuffer request(f.mLengthFieldSize,true);
            request.AppendByte(5);
            request.AppendShort(14);
            request.AppendShort(5);
            request.AppendShort(1);
            request.AppendBuffer(f.mData.get(),0);
            want = Bytes(request);
            break;
        }
        case EE_WON_DB_REQ_GET_ACCOUNT: {
            WriteBuffer aRequest(f.mLengthFieldSize,true);
            aRequest.AppendByte(5);
            aRequest.AppendShort(14);
            aRequest.AppendShort(1002);
            aRequest.AppendShort(6);
            a.PackRequestData(aRequest,false);
            want = Bytes(aRequest);
            break;
        }
        case EE_WON_DB_REQ_UPDATE_ACCOUNT: {
            WriteBuffer aRequest(f.mLengthFieldSize,true);
            aRequest.AppendByte(5);
            aRequest.AppendShort(14);
            aRequest.AppendShort(1002);
            aRequest.AppendShort(4);
            aRequest.AppendWString(f.mPassword,2);
            aRequest.AppendString(f.mEmail,2);
            a.PackRequestData(aRequest,true);
            want = Bytes(aRequest);
            break;
        }
        case EE_WON_DB_REQ_USER_ID: {
            WriteBuffer request(0,true);
            request.AppendWString(f.mUserName,2);
            want = Bytes(request);
            break;
        }
        case EE_WON_DB_REQ_PASSWORD_EMAIL: {
            WriteBuffer msg(0,true);
            msg.AppendWString(f.mUserName,2);
            a.PackRequestData(msg,true);
            want = Bytes(msg);
            break;
        }
        case EE_WON_DB_REQ_INFO_EMAIL: {
            WriteBuffer msg(0,true);
            msg.AppendString(f.mEmail,2);
            a.PackRequestData(msg,true);
            want = Bytes(msg);
            break;
        }
        case EE_WON_DB_REQ_CONTEST: {
            q.mReq.message = (short)f.mProcNum;
            WriteBuffer aBuf(f.mLengthFieldSize,true);
            aBuf.AppendByte(3);
            aBuf.AppendByte(4);
            aBuf.AppendByte(1);
            aBuf.AppendLong(f.mUserId);
            aBuf.AppendShort(f.mProcNum);
            for(int k=0; k<16; k++)
                aBuf.AppendByte(0);
            aBuf.AppendBuffer(f.mData);
            want = Bytes(aBuf);
            break;
        }
        }
        got = Str(q.Build().get());
        CHECK(got == want);
        if (got != want && sFailures < 6) printf("request %d: %zu vs %zu bytes\n", kind, got.size(), want.size());
    }
    // the event report
    for (int i = 0; i < 20000; i++) {
        unsigned char aSize = kSizes[Pick(4)];
        unsigned short mActivityType = AShort(), mServerType = AShort();
        bool mHasRelatedServer = Pick(2) != 0, mHasRelatedClient = Pick(2) != 0, mHasRelatedUser = Pick(2) != 0;
        std::wstring mServerLogicalName = AWString(), mClientName = AWString(), mUserName = AWString();
        std::string mServerNetAddress = AString(), mClientNetAddress = AString();
        unsigned long mUserId = (unsigned long)Next() * 13u;
        struct Detail { unsigned short mDetailType; int mDataType; unsigned long mNumber; std::wstring mString; };
        struct Attachment { std::wstring mDescription; unsigned char mContentType; ByteBufferPtr mData; };
        std::list<Detail> mDetailList;
        std::list<Attachment> mAttachmentList;
        int n = Pick(5) == 0 ? 255 + Pick(3) : Pick(4);
        for (int j = 0; j < n; j++) { Detail d = { AShort(), Pick(5) ? Pick(2) : Pick(4), (unsigned long)Next(), AWString() }; mDetailList.push_back(d); }
        n = Pick(4);
        for (int j = 0; j < n; j++) { Attachment t = { AWString(), (unsigned char)Pick(256), ABuffer() }; mAttachmentList.push_back(t); }

        WriteBuffer aBuf(aSize,true);
        aBuf.AppendByte(5);
        aBuf.AppendShort(4);
        aBuf.AppendShort(1);
        unsigned char aContentFlags = 0;
        if (mHasRelatedServer) aContentFlags |= 0x02;
        if (mHasRelatedClient) aContentFlags |= 0x04;
        if (mHasRelatedUser)   aContentFlags |= 0x08;
        aBuf.AppendByte(aContentFlags);
        aBuf.AppendByte(mDetailList.size());
        aBuf.AppendByte(mAttachmentList.size());
        aBuf.AppendShort(mActivityType);
        if (mHasRelatedServer) { aBuf.AppendShort(mServerType); aBuf.AppendWString(mServerLogicalName,2); aBuf.AppendString(mServerNetAddress); }
        if (mHasRelatedClient) { aBuf.AppendWString(mClientName,2); aBuf.AppendString(mClientNetAddress); }
        if (mHasRelatedUser) { aBuf.AppendShort(1); aBuf.AppendLong(mUserId); aBuf.AppendWString(mUserName,2); }
        for (std::list<Detail>::iterator d = mDetailList.begin(); d != mDetailList.end(); ++d) {
            aBuf.AppendShort(d->mDetailType);
            aBuf.AppendByte(d->mDataType);
            switch(d->mDataType) {
                case 0: aBuf.AppendLong(d->mNumber); break;
                case 1: aBuf.AppendWString(d->mString,2); break;
            }
        }
        for (std::list<Attachment>::iterator t = mAttachmentList.begin(); t != mAttachmentList.end(); ++t) {
            aBuf.AppendWString(t->mDescription,2);
            aBuf.AppendByte(t->mContentType);
            aBuf.AppendBuffer(t->mData,4);
        }
        std::string want = Bytes(aBuf);

        DbRequest q(0, aSize);
        EeWonDbEvent e;
        memset(&e, 0, sizeof(e));
        e.field_size = aSize;
        e.activity_type = mActivityType;
        e.has_server = mHasRelatedServer;
        e.server_type = mServerType;
        e.server_name = q.Text(mServerLogicalName);
        e.server_addr = DbRequest::Narrow(mServerNetAddress);
        e.has_client = mHasRelatedClient;
        e.client_name = q.Text(mClientName);
        e.client_addr = DbRequest::Narrow(mClientNetAddress);
        e.has_user = mHasRelatedUser;
        e.user_id = (uint32_t)mUserId;
        e.user_name = q.Text(mUserName);
        std::vector<EeWonDbDetail> ds;
        for (std::list<Detail>::iterator d = mDetailList.begin(); d != mDetailList.end(); ++d) {
            EeWonDbDetail x = { d->mDetailType, (int32_t)d->mDataType, (uint32_t)d->mNumber, q.Text(d->mString) };
            ds.push_back(x);
        }
        std::vector<EeWonDbAttachment> as;
        for (std::list<Attachment>::iterator t = mAttachmentList.begin(); t != mAttachmentList.end(); ++t) {
            EeWonDbAttachment x = { q.Text(t->mDescription), t->mContentType, DbRequest::Buffer(t->mData.get()) };
            as.push_back(x);
        }
        e.details = ds.empty() ? NULL : &ds[0];
        e.detail_count = ds.size();
        e.attachments = as.empty() ? NULL : &as[0];
        e.attachment_count = as.size();
        ByteBufferPtr r;
        ee_won_db_event_request(&e, WonByteBufferSink, &r);
        CHECK(Str(r.get()) == want);
    }
}

// ---- replies: the original CheckResponse statements over locals

static int sInvalid;
static WONStatus InvalidReplyHeader() { sInvalid++; return WS_ServerReq_InvalidReplyHeader; }

struct Reply {
    short mMessageReplyType, mSubMessageReplyType;
    std::string mErrorString, mEmail;
    std::string mReceived;
    std::string mReplyData;
    int mState;
    std::string mKey;
    unsigned long mUserId;
    int mLastError;
    std::string Dump() const
    {
        char b[128];
        snprintf(b, sizeof(b), "%d %d %d %lu %d|", mMessageReplyType, mSubMessageReplyType, mState, mUserId, mLastError);
        return b + mErrorString + "|" + mEmail + "|" + mReceived + "|" + mReplyData + "|" + mKey;
    }
};

static bool SetPublicKey(Reply& r, const void* data, unsigned long len)
{
    r.mKey.assign(static_cast<const char*>(data), len);
    return len > 0 && static_cast<const unsigned char*>(data)[0] != 0xee;
}

enum { kProxy, kCreateKey, kCreate, kGetAccount, kUpdate, kContest, kEvent, kUserId, kKinds };

static WONStatus OrigProxy(Reply& o, const ByteBuffer* mResponse, WriteBuffer& mReceivedData, ByteBufferPtr& mReplyData)
{
    ReadBuffer aReply(mResponse->data(),mResponse->length());
    unsigned char aHeader=aReply.ReadByte();
    unsigned short aService=aReply.ReadShort();
    o.mMessageReplyType=aReply.ReadShort();
    o.mSubMessageReplyType=aReply.ReadShort();
    WONStatus aStatus=aReply.ReadShort();
    aReply.ReadString(o.mErrorString,2);
    unsigned char aFlags=aReply.ReadByte();
    if (aHeader!=5 || aService!=14)
        return InvalidReplyHeader();
    if ((unsigned long)aReply.pos()<aReply.length())
        mReceivedData.AppendBytes(aReply.data()+aReply.pos(),aReply.length()-aReply.pos());
    if ((aFlags&0x80)!=0) {
        mReplyData=mReceivedData.ToByteBuffer(true);
        return aStatus;
    }
    return WS_ServerReq_Recv;
}

static WONStatus Orig(int kind, Reply& o, const ByteBuffer* mResponse, OrigProfile* profile)
{
    switch (kind) {
    case kProxy:
    case kUserId: {
        WriteBuffer rec(0, true);
        ByteBufferPtr data;
        WONStatus s = OrigProxy(o, mResponse, rec, data);
        o.mReceived.assign(rec.data(), rec.length());
        o.mReplyData = Str(data.get());
        if (kind == kProxy) return s;
        if (s!=WS_Success) return s;
        if (o.mSubMessageReplyType!=17) return InvalidReplyHeader();
        ReadBuffer reply(data->data(),data->length());
        o.mUserId=reply.ReadLong();
        return WS_Success;
    }
    case kCreateKey:
    case kCreate: {
        ReadBuffer reply(mResponse->data(),mResponse->length());
        unsigned char header=reply.ReadByte();
        unsigned short service=reply.ReadShort();
        unsigned short message=reply.ReadShort();
        unsigned short subMessage=reply.ReadShort();
        WONStatus status=reply.ReadShort();
        reply.ReadString(o.mErrorString,2);
        reply.ReadByte();
        if (header!=5 || service!=14) return InvalidReplyHeader();
        if (status!=0) return status;
        if (o.mState==0) {
            if (message!=3) return InvalidReplyHeader();
            unsigned short keyLength=reply.ReadShort();
            if (!SetPublicKey(o,reply.ReadBytes(keyLength),keyLength)) {
                o.mLastError=WS_InvalidPublicKey;
                return WS_ServerReq_TryNextServer;
            }
            o.mState=1;
            return WS_ServerReq_Send;
        } else {
            if (message!=1002 || subMessage!=3) return InvalidReplyHeader();
            return status;
        }
    }
    case kGetAccount:
    case kUpdate: {
        ReadBuffer aReply(mResponse->data(),mResponse->length());
        unsigned char aHeader=aReply.ReadByte();
        unsigned short aService=aReply.ReadShort();
        unsigned short aMessage=aReply.ReadShort();
        unsigned short aSubMessage=aReply.ReadShort();
        WONStatus aStatus=aReply.ReadShort();
        aReply.ReadString(o.mErrorString,2);
        aReply.ReadByte();
        if (aHeader!=5 || aService!=14)
            return InvalidReplyHeader();
        if (aStatus!=0)
            return aStatus;
        if (kind == kUpdate) {
            if (aMessage!=1002 || aSubMessage!=5)
                return InvalidReplyHeader();
            return WS_Success;
        }
        if (aMessage!=1002 || aSubMessage!=7)
            return InvalidReplyHeader();
        aReply.ReadString(o.mEmail,1);
        profile->UnpackGetData(aReply);
        return WS_Success;
    }
    case kContest: {
        ReadBuffer aMsg(mResponse->data(),mResponse->length());
        unsigned char aHeaderType = aMsg.ReadByte();
        unsigned short aServiceType = aMsg.ReadByte();
        unsigned short aMessageType = aMsg.ReadByte();
        if(aHeaderType!=3 || aServiceType!=4 || aMessageType!=2)
            return InvalidReplyHeader();
        short aStatus = aMsg.ReadShort();
        o.mReplyData = Str(new ByteBuffer(aMsg.data() + aMsg.pos(), aMsg.Available(),false));
        return (WONStatus)aStatus;
    }
    case kEvent: {
        ReadBuffer aMsg(mResponse->data(),mResponse->length());
        unsigned char aHeaderType = aMsg.ReadByte();
        unsigned short aServiceType = aMsg.ReadShort();
        unsigned short aMessageType = aMsg.ReadShort();
        if(aHeaderType!=5 || aServiceType!=4 || aMessageType!=2)
            return InvalidReplyHeader();
        short aStatus = aMsg.ReadShort();
        return (WONStatus)aStatus;
    }
    }
    return (WONStatus)-1;
}

// the new code (the ops' CheckResponse bodies, the members as locals)
static WONStatus New(int kind, Reply& o, const ByteBuffer* mResponse, ProfileData* profile)
{
    switch (kind) {
    case kProxy:
    case kUserId: {
        WriteBuffer rec(0, true);
        ByteBufferPtr data;
        bool anInvalid = false;
        WONStatus s;
        try {
            s = DbProxyReply(mResponse, o.mMessageReplyType, o.mSubMessageReplyType, o.mErrorString, rec, data, &anInvalid);
        } catch (ReadBufferException&) {
            o.mReceived.assign(rec.data(), rec.length());
            throw;
        }
        if (anInvalid) s = InvalidReplyHeader();
        o.mReceived.assign(rec.data(), rec.length());
        o.mReplyData = Str(data.get());
        if (kind == kProxy) return s;
        if (s!=WS_Success) return s;
        if (!ee_won_db_answers(o.mSubMessageReplyType,16)) return InvalidReplyHeader();
        o.mUserId=DbUserId(data.get());
        return WS_Success;
    }
    case kCreateKey:
    case kCreate: {
        EeWonDbReply r=DbAccountReply(o.mState==0 ? EE_WON_DB_CREATE_KEY : EE_WON_DB_CREATE, mResponse, o.mErrorString);
        if (r.verdict==EE_WON_DB_INVALID_HEADER) return InvalidReplyHeader();
        if (r.verdict==EE_WON_DB_PUBLIC_KEY) {
            if (!SetPublicKey(o,mResponse->data()+r.key_at,r.key_len)) {
                o.mLastError=WS_InvalidPublicKey;
                return WS_ServerReq_TryNextServer;
            }
            o.mState=1;
            return WS_ServerReq_Send;
        }
        return (WONStatus)r.result;
    }
    case kGetAccount: {
        EeWonDbReply r=DbAccountReply(EE_WON_DB_GET_ACCOUNT,mResponse,o.mErrorString,&o.mEmail);
        if (r.verdict==EE_WON_DB_INVALID_HEADER)
            return InvalidReplyHeader();
        if (r.email!=2)
            return (WONStatus)r.result;
        ReadBuffer aReply(mResponse->data(),mResponse->length());
        aReply.SetPos((int)r.profile_at);
        profile->UnpackGetData(aReply);
        return WS_Success;
    }
    case kUpdate: {
        EeWonDbReply r=DbAccountReply(EE_WON_DB_UPDATE_ACCOUNT,mResponse,o.mErrorString);
        if (r.verdict==EE_WON_DB_INVALID_HEADER)
            return InvalidReplyHeader();
        return (WONStatus)r.result;
    }
    case kContest:
    case kEvent: {
        bool anInvalid=false;
        ByteBufferPtr data;
        WONStatus aStatus=DbContestReply(mResponse,kind == kContest ? &data : NULL,&anInvalid);
        if(anInvalid)
            return InvalidReplyHeader();
        if (kind == kContest) o.mReplyData = Str(data.get());
        return aStatus;
    }
    }
    return (WONStatus)-1;
}

static std::string GenReply(int kind)
{
    WriteBuffer w(0, true);
    if (kind == kContest) {
        w.AppendByte(Pick(8) ? 3 : 2); w.AppendByte(Pick(8) ? 4 : 5); w.AppendByte(Pick(8) ? 2 : 1);
        w.AppendShort((short)(Pick(2) ? 0 : Next()));
        int n = Pick(6);
        for (int i = 0; i < n; i++) w.AppendByte((unsigned char)Pick(256));
        return Bytes(w);
    }
    if (kind == kEvent) {
        w.AppendByte(Pick(8) ? 5 : 3); w.AppendShort(Pick(8) ? 4 : 14); w.AppendShort(Pick(8) ? 2 : 1);
        w.AppendShort((short)(Pick(2) ? 0 : Next()));
        return Bytes(w);
    }
    static const short kMessages[] = { 3, 1002, 1002, 5 };
    static const short kSubs[] = { 3, 7, 5, 17, 9, 15 };
    w.AppendByte(Pick(10) ? 5 : 3);
    w.AppendShort(Pick(10) ? 14 : 4);
    w.AppendShort(Pick(4) ? kMessages[Pick(4)] : (short)Next());
    w.AppendShort(Pick(4) ? kSubs[Pick(6)] : (short)Next());
    w.AppendShort((short)(Pick(3) ? 0 : (Pick(2) ? -5 : Next())));
    w.AppendString(AString(), 2);
    w.AppendByte((unsigned char)(Pick(2) ? 0x80 : Pick(256)));
    switch (kind) {
    case kCreateKey: { int n = Pick(5); w.AppendShort((short)n); for (int i = 0; i < n; i++) w.AppendByte((unsigned char)(Pick(5) ? Pick(256) : 0xee)); break; }
    case kGetAccount: w.AppendString(AString().substr(0, 100), 1); w.AppendBytes(ProfileReply().data(), 0); { std::string p = ProfileReply(); w.AppendBytes(p.data(), (unsigned long)p.size()); } break;
    case kUserId: w.AppendLong((int)Next()); break;
    default: { int n = Pick(6); for (int i = 0; i < n; i++) w.AppendByte((unsigned char)Pick(256)); break; }
    }
    return Bytes(w);
}

struct Out {
    bool threw;
    int status;
    int invalid;
    std::string dump;
    bool operator==(const Out& o) const { return threw == o.threw && (threw || status == o.status) && invalid == o.invalid && dump == o.dump; }
};

static void TestReplyOn(int kind, const std::string& msg)
{
    ByteBufferPtr resp = new ByteBuffer(msg.data(), (unsigned long)msg.size(), false);
    Reply a, b;
    a.mMessageReplyType = b.mMessageReplyType = 7;
    a.mSubMessageReplyType = b.mSubMessageReplyType = 8;
    a.mErrorString = b.mErrorString = "old";
    a.mEmail = b.mEmail = "old@";
    a.mState = b.mState = kind == kCreateKey ? 0 : 2;
    a.mUserId = b.mUserId = 99;
    a.mLastError = b.mLastError = 0;
    OrigProfile pa;
    SmartPtr<ProfileData> pb = new ProfileData;
    if (Pick(2)) { pa.AddNewsletterSub(1, true); pb->AddNewsletterSub(1, true); }
    Out oa, ob;
    sInvalid = 0;
    oa.threw = false; oa.status = 0;
    try { oa.status = Orig(kind, a, resp.get(), &pa); } catch (ReadBufferException&) { oa.threw = true; }
    oa.invalid = sInvalid;
    oa.dump = a.Dump() + Pack(pa, true);
    sInvalid = 0;
    ob.threw = false; ob.status = 0;
    try { ob.status = New(kind, b, resp.get(), pb.get()); } catch (ReadBufferException&) { ob.threw = true; }
    ob.invalid = sInvalid;
    ob.dump = b.Dump() + Pack(pb.get(), true);
    CHECK(oa == ob);
    if (!(oa == ob) && sFailures < 8)
        printf("kind %d len %zu: threw %d/%d status %d/%d invalid %d/%d\n", kind, msg.size(), oa.threw, ob.threw,
               oa.status, ob.status, oa.invalid, ob.invalid);
}

static void TestReplies()
{
    for (int i = 0; i < 8000 * kKinds; i++) {
        int kind = i % kKinds;
        std::string m = GenReply(kind);
        TestReplyOn(kind, m);
        for (size_t n = 0; n < m.size(); n++) TestReplyOn(kind, m.substr(0, n));
        for (int j = 0; j < 3 && !m.empty(); j++) {
            std::string x = m;
            x[Pick((int)x.size())] = (char)Pick(256);
            TestReplyOn(kind, x);
        }
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    TestProfiles();
    TestRequests();
    TestReplies();
    printf("won_db_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
