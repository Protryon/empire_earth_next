// WONMisc's FTP and patch code (FTPGetOp's URL, path, status, commands, passive reply and reply steps; VersionData's
// and PatchData's wire forms and the version type split; the version filter; CheckValidVersionOp's,
// GetPatchListOp's and ReportPatchStatusOp's request and reply data): the original's statements (copied from
// FTPGetOp.cpp, PatchTypes.cpp, GetVersionOp.cpp, CheckValidVersionOp.cpp, GetPatchListOp.cpp and
// ReportPatchStatusOp.cpp before the port, over stand-ins) against tests/won/won_ftp_msg.h and
// WONPatchMsg.h over ee_won's ftp and patch modules, over the same stand-ins and generated input, cut at every
// length and mutated. The exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <stdexcept>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONHttpMsg.h"
#include "won_ftp_msg.h"
#include "ee/wonapi/WONPatchMsg.h"
#include "rust/ee_rust.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 3131;
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
static string FromSet(const char* inSet, int n)
{
    string s;
    size_t k = strlen(inSet);
    for (int i = 0; i < n; i++) s += inSet[Pick((int)k)];
    return s;
}

// ---- FTP

struct SAddr {
    string mLog;
    void Fn2B2C20(long h, unsigned short p)
    {
        char b[64];
        snprintf(b, sizeof(b), "[%ld:%u]", h, (unsigned)p);
        this->mLog += b;
    }
};

struct SFtp {
    enum RequestState { RequestState_Start, RequestState_User, RequestState_Password, RequestState_ChangeDir,
                        RequestState_ChangeType, RequestState_Passive, RequestState_Restart, RequestState_Retrieve };
    string mHost, mRemotePath, mUserName, mPassword, mActualPath, mActualFile;
    int mFTPStatus;
    bool mIsBinary, mDoResume;
    DWORD mLocalFileSize;
    RequestState mRequestState;
    SAddr mDataAddr;
    bool mAsync, mSendOk, mDataOk;
    string mLog;
    SFtp() : mUserName("anonymous"), mPassword("anonymous"), mFTPStatus(-9), mIsBinary(true), mDoResume(false),
             mLocalFileSize(0), mRequestState(RequestState_Start), mAsync(false), mSendOk(true), mDataOk(true) {}
    void SetUserName(const string& s) { this->mUserName = s; }
    void SetPassword(const string& s) { this->mPassword = s; }
    void Finish(int s) { this->mLog += "finish " + std::to_string(s) + ";"; }
    bool SendRequest() { this->mLog += "send " + std::to_string((int)this->mRequestState) + ";"; return this->mSendOk; }
    bool DoDataConnection() { this->mLog += "data;"; return this->mDataOk; }
    void RecvContent() { this->mLog += "recv;"; }
    void RecvLineAsync() { this->mLog += "line;"; }
    bool IsAsync() const { return this->mAsync; }
    string Dump() const
    {
        return Hex(this->mHost) + " " + Hex(this->mRemotePath) + " " + Hex(this->mUserName) + " " + Hex(this->mPassword) +
               " " + Hex(this->mActualPath) + " " + Hex(this->mActualFile) + " " + std::to_string(this->mFTPStatus) + " " +
               std::to_string(this->mDoResume) + " " + std::to_string((int)this->mRequestState) + " " +
               this->mDataAddr.mLog + " " + this->mLog;
    }
};

namespace OrigFtp {
static void SetRemotePath(SFtp& o, const std::string& thePath)
{
    o.mRemotePath = thePath;
    int aSlashPos = thePath.find_last_of('/');
    if (aSlashPos == string::npos) {
        o.mActualPath = "";
        o.mActualFile = thePath;
    } else {
        o.mActualPath = thePath.substr(0, aSlashPos);
        o.mActualFile = thePath.substr(aSlashPos + 1);
    }
}
static void ExtractUserNameAndPasswordFromURL(SFtp& o, std::string& theURL)
{
    int aColenPos = theURL.find(':');
    if (aColenPos != string::npos) {
        o.SetUserName(theURL.substr(0, aColenPos));
        theURL = theURL.substr(aColenPos + 1);
    }
    int aSlashPos = theURL.find('/');
    int aAtPos = theURL.rfind('@', aSlashPos);
    if (aAtPos != string::npos) {
        o.SetPassword(theURL.substr(0, aAtPos));
        theURL = theURL.substr(aAtPos + 1);
    }
}
static void FromURL(SFtp& o, const std::string& theURL, bool doExtractUserAndPassword)
{
    string aStr = theURL;
    int aPos = aStr.find("://");
    if (aPos != string::npos)
        aStr = aStr.substr(aPos + 3);
    if (doExtractUserAndPassword)
        ExtractUserNameAndPasswordFromURL(o, aStr);
    aPos = aStr.find('/');
    o.mHost = aStr.substr(0, aPos);
    if (aPos != string::npos)
        SetRemotePath(o, aStr.substr(aPos + 1));
}
static bool ExtractStatus(SFtp& o, const char* theStatusLine)
{
    if (sscanf(theStatusLine, "%d", &o.mFTPStatus) != 1) {
        o.Finish(WS_FTP_InvalidResponse);
        return false;
    }
    if (o.mFTPStatus >= 200 && o.mFTPStatus < 300) return true;
    if (o.mFTPStatus == 331) return true;
    if (o.mFTPStatus == 125 || o.mFTPStatus == 150) return true;
    if (o.mFTPStatus == 350) return true;
    if (o.mFTPStatus == 504) return true;
    o.Finish(WS_FTP_StatusError);
    return false;
}
static string Command(SFtp& o)
{
    std::string aReq;
    switch (o.mRequestState) {
    case SFtp::RequestState_User: aReq = "USER " + o.mUserName; break;
    case SFtp::RequestState_Password: aReq = "PASS " + o.mPassword; break;
    case SFtp::RequestState_ChangeDir: aReq = "CWD " + o.mActualPath; break;
    case SFtp::RequestState_ChangeType: aReq = string("TYPE ") + (o.mIsBinary ? 'I' : 'A'); break;
    case SFtp::RequestState_Passive: aReq = "PASV"; break;
    case SFtp::RequestState_Restart: {
        char aBuf[50];
        sprintf(aBuf, "REST %d", o.mLocalFileSize);
        aReq = aBuf;
    } break;
    case SFtp::RequestState_Retrieve: aReq = "RETR " + o.mActualFile; break;
    default: break;
    }
    aReq += "\r\n";
    return aReq;
}
static bool ParsePasv(SFtp& o, const char* theStatusLine)
{
    const char* aBufP = strchr(theStatusLine, '(');
    if (aBufP == NULL) {
        o.Finish(WS_FTP_InvalidPasvResponse);
        return false;
    }
    int aNum[6], aResult;
    aResult = sscanf(aBufP, "(%d,%d,%d,%d,%d,%d)", aNum, aNum + 1, aNum + 2, aNum + 3, aNum + 4, aNum + 5);
    if (aResult != 6) {
        o.Finish(WS_FTP_InvalidPasvResponse);
        return false;
    }
    long aHost = (aNum[0] << 24) | (aNum[1] << 16) | (aNum[2] << 8) | aNum[3];
    unsigned short aPort = (aNum[4] << 8) | aNum[5];
    o.mDataAddr.Fn2B2C20(aHost, aPort);
    return true;
}
static bool HandleReply(SFtp& o, const char* aLine, int aLen)
{
    if (aLen >= 4 && aLine[3] == '-') {
        if (o.IsAsync())
            o.RecvLineAsync();
        return true;
    }
    if (!ExtractStatus(o, aLine))
        return false;
    switch (o.mRequestState) {
    case SFtp::RequestState_Start:
        o.mRequestState = SFtp::RequestState_User;
        return o.SendRequest();
    case SFtp::RequestState_User:
        if (o.mFTPStatus == 331) {
            o.mRequestState = SFtp::RequestState_Password;
            return o.SendRequest();
        }
    case SFtp::RequestState_Password:
        if (!o.mActualPath.empty()) {
            o.mRequestState = SFtp::RequestState_ChangeDir;
            return o.SendRequest();
        }
    case SFtp::RequestState_ChangeDir:
        o.mRequestState = SFtp::RequestState_ChangeType;
        return o.SendRequest();
    case SFtp::RequestState_ChangeType:
        o.mRequestState = SFtp::RequestState_Passive;
        return o.SendRequest();
    case SFtp::RequestState_Passive:
        if (!ParsePasv(o, aLine))
            return false;
        if (o.mDoResume)
            o.mRequestState = SFtp::RequestState_Restart;
        else
            o.mRequestState = SFtp::RequestState_Retrieve;
        if (!o.DoDataConnection())
            return false;
        return o.SendRequest();
    case SFtp::RequestState_Restart:
        if (o.mFTPStatus == 504)
            o.mDoResume = false;
        o.mRequestState = SFtp::RequestState_Retrieve;
        return o.SendRequest();
    case SFtp::RequestState_Retrieve:
        o.RecvContent();
        return false;
    }
    return false;
}
} // namespace OrigFtp

static string AFtpUrl()
{
    string s = Pick(3) ? "ftp://" : Pick(2) ? "" : "x://";
    s += FromSet("ab:@/.", Pick(16));
    return s;
}
static string AReplyLine(int inState)
{
    static const char* kCodes[] = { "220", "230", "331", "250", "200", "227", "350", "504", "150", "125", "530",
                                    "550", "x", "", "2147483648", "-5", " 226", "+331" };
    string s = kCodes[Pick(18)];
    if (inState == SFtp::RequestState_Passive && Pick(3)) {
        s = "227 Entering Passive Mode " + string(Pick(6) ? "(" : "");
        for (int i = 0; i < 6; i++) {
            if (i) s += Pick(15) ? "," : " ,";
            s += Pick(8) ? std::to_string(Pick(300)) : FromSet(" -+09x", Pick(4));
        }
        if (Pick(2)) s += ")";
        return s;
    }
    if (Pick(6) == 0) s += "-";
    s += FromSet(" text-", Pick(10));
    if (Pick(10) == 0) s = FromSet("0123456789-( ,)", Pick(12));
    return s;
}

static void CheckFtp()
{
    for (int i = 0; i < 200000; i++) {
        string u = AFtpUrl();
        bool extract = Pick(3) != 0;
        SFtp a, b;
        OrigFtp::FromURL(a, u, extract);
        FtpMsg<SFtp>::FromURL(b, u, extract);
        CHECK(a.Dump() == b.Dump());
        if (a.Dump() != b.Dump() && sFailures < 8) printf("url [%s] %d: %s / %s\n", u.c_str(), extract, a.Dump().c_str(), b.Dump().c_str());
        string x = u, y = u;
        SFtp c, d;
        OrigFtp::ExtractUserNameAndPasswordFromURL(c, x);
        FtpMsg<SFtp>::ExtractUserNameAndPassword(d, y);
        CHECK(x == y && c.Dump() == d.Dump());
    }
    for (int i = 0; i < 400000; i++) {
        SFtp a;
        a.mRequestState = (SFtp::RequestState)Pick(9);
        a.mActualPath = Pick(2) ? "" : "dir";
        a.mActualFile = FromSet("fx.", Pick(5));
        a.mUserName = FromSet("u", Pick(3));
        a.mPassword = FromSet("p", Pick(3));
        a.mIsBinary = Pick(2) != 0;
        a.mDoResume = Pick(2) != 0;
        a.mLocalFileSize = Pick(3) ? (DWORD)Pick(100000) : (DWORD)(Next() << 8);
        a.mAsync = Pick(2) != 0;
        a.mSendOk = Pick(4) != 0;
        a.mDataOk = Pick(4) != 0;
        SFtp b = a;
        CHECK(OrigFtp::Command(a) == FtpMsg<SFtp>::Command(b));
        string line = AReplyLine(a.mRequestState);
        bool ra = OrigFtp::HandleReply(a, line.c_str(), (int)line.size());
        bool rb = FtpMsg<SFtp>::HandleReply(b, line.c_str(), (int)line.size());
        CHECK(ra == rb && a.Dump() == b.Dump());
        if (!(ra == rb && a.Dump() == b.Dump()) && sFailures < 8)
            printf("reply [%s]: %d %s / %d %s\n", line.c_str(), ra, a.Dump().c_str(), rb, b.Dump().c_str());
        SFtp c, d;
        bool sa = OrigFtp::ExtractStatus(c, line.c_str()), sb = FtpMsg<SFtp>::ExtractStatus(d, line.c_str());
        bool pa = OrigFtp::ParsePasv(c, line.c_str()), pb = FtpMsg<SFtp>::ParsePasv(d, line.c_str());
        CHECK(sa == sb && pa == pb && c.Dump() == d.Dump());
    }
}

// ---- the version and patch records

struct SVersion : public RefCount {
    string mConfigName, mVersion, mDescriptionURL;
    VersionState mState;
    VersionTypeList mVersionTypeList;
    SVersion() : mState(VersionState_Invalid) {}
    void SetConfigName(const string& s) { this->mConfigName = s; }
    void SetVersion(const string& s) { this->mVersion = s; }
    void SetDescriptionURL(const string& s) { this->mDescriptionURL = s; }
    void SetState(VersionState s) { this->mState = s; }
    void AddVersionType(const string& s) { PatchAddVersionType(this->mVersionTypeList, s); }
    const string& GetConfigName() const { return this->mConfigName; }
    const string& GetVersion() const { return this->mVersion; }
    const string& GetDescriptionURL() const { return this->mDescriptionURL; }
    VersionState GetState() const { return this->mState; }
    const VersionTypeList& GetVersionTypeList() const { return this->mVersionTypeList; }
    string Dump() const
    {
        string s = Hex(this->mConfigName) + " " + Hex(this->mVersion) + " " + Hex(this->mDescriptionURL) + " " +
                   std::to_string((int)this->mState);
        for (VersionTypeList::const_iterator i = this->mVersionTypeList.begin(); i != this->mVersionTypeList.end(); ++i)
            s += " t" + Hex(*i);
        return s;
    }
};

struct SPatch : public RefCount {
    string mConfigName, mFromVersion, mToVersion, mDisplayName, mNetAddress, mHostName, mHostURL, mHostText, mHostImageURL;
    unsigned long mCheckSum, mPatchSize;
    bool mMustVisitHost, mIsActive;
    SPatch() : mCheckSum(0), mPatchSize(0), mMustVisitHost(false), mIsActive(false) {}
    void SetConfigName(const string& s) { this->mConfigName = s; }
    void SetFromVersion(const string& s) { this->mFromVersion = s; }
    void SetToVersion(const string& s) { this->mToVersion = s; }
    void SetDisplayName(const string& s) { this->mDisplayName = s; }
    void SetNetAddress(const string& s) { this->mNetAddress = s; }
    void SetHostName(const string& s) { this->mHostName = s; }
    void SetHostURL(const string& s) { this->mHostURL = s; }
    void SetHostText(const string& s) { this->mHostText = s; }
    void SetHostImageURL(const string& s) { this->mHostImageURL = s; }
    void SetCheckSum(unsigned long v) { this->mCheckSum = v; }
    void SetPatchSize(unsigned long v) { this->mPatchSize = v; }
    void SetMustVisitHost(bool v) { this->mMustVisitHost = v; }
    void SetIsActive(bool v) { this->mIsActive = v; }
    const string& GetConfigName() const { return this->mConfigName; }
    const string& GetFromVersion() const { return this->mFromVersion; }
    const string& GetToVersion() const { return this->mToVersion; }
    const string& GetDisplayName() const { return this->mDisplayName; }
    const string& GetNetAddress() const { return this->mNetAddress; }
    const string& GetHostName() const { return this->mHostName; }
    const string& GetHostURL() const { return this->mHostURL; }
    const string& GetHostText() const { return this->mHostText; }
    const string& GetHostImageURL() const { return this->mHostImageURL; }
    unsigned long GetCheckSum() const { return this->mCheckSum; }
    unsigned long GetPatchSize() const { return this->mPatchSize; }
    bool GetMustVisitHost() const { return this->mMustVisitHost; }
    bool GetIsActive() const { return this->mIsActive; }
    void ReadFromBuffer(ReadBuffer& b, bool inList) { PatchReadPatch(*this, b, inList); }
    string Dump() const
    {
        return Hex(this->mConfigName) + " " + Hex(this->mFromVersion) + " " + Hex(this->mToVersion) + " " +
               Hex(this->mDisplayName) + " " + Hex(this->mNetAddress) + " " + Hex(this->mHostName) + " " +
               Hex(this->mHostURL) + " " + Hex(this->mHostText) + " " + Hex(this->mHostImageURL) + " " +
               std::to_string(this->mCheckSum) + " " + std::to_string(this->mPatchSize) + " " +
               std::to_string(this->mMustVisitHost) + std::to_string(this->mIsActive);
    }
};
typedef std::list<SmartPtr<SPatch> > SPatchList;

namespace OrigPatch {
static void AddVersionType(SVersion& v, const std::string& theVersionType)
{
    int lastPos = 0;
    int aCommaPos = theVersionType.find(',');
    std::string versionString;
    for (; aCommaPos != -1; aCommaPos = theVersionType.find(',', lastPos)) {
        versionString = theVersionType.substr(lastPos, aCommaPos - lastPos);
        lastPos = aCommaPos + 1;
        v.mVersionTypeList.push_back(versionString);
    }
    versionString = theVersionType.substr(lastPos, aCommaPos - lastPos);
    v.mVersionTypeList.push_back(versionString);
}
static void ReadVersion(SVersion& v, ReadBuffer& theBuffer)
{
    theBuffer.ReadString(v.mConfigName);
    theBuffer.ReadString(v.mVersion);
    int numVersionTypeStrings = theBuffer.ReadByte();
    std::string versionTypeString;
    for (int k = 0; k < numVersionTypeStrings; ++k) {
        theBuffer.ReadString(versionTypeString);
        AddVersionType(v, versionTypeString);
    }
    theBuffer.ReadString(v.mDescriptionURL);
    v.mState = (VersionState)theBuffer.ReadByte();
}
static void WriteVersion(const SVersion& v, WriteBuffer& outBuffer, DWORD inFlags)
{
    outBuffer.AppendString(v.mConfigName);
    outBuffer.AppendString(v.mVersion);
    outBuffer.AppendByte(v.mVersionTypeList.size());
    if (!v.mVersionTypeList.empty()) {
        VersionTypeList::const_iterator i = v.mVersionTypeList.begin();
        while (true) {
            if (i == v.mVersionTypeList.end()) break;
            outBuffer.AppendString(*i);
            ++i;
        }
    }
    if (inFlags & 2) outBuffer.AppendString(v.mDescriptionURL);
    if (inFlags & 1) outBuffer.AppendByte(v.mState);
}
static void ReadPatch(SPatch& p, ReadBuffer& inBuffer, bool inGetPatchList)
{
    if (inGetPatchList) {
        inBuffer.ReadString(p.mConfigName);
        inBuffer.ReadString(p.mFromVersion);
        inBuffer.ReadString(p.mToVersion);
        p.mIsActive = inBuffer.ReadBool();
    }
    inBuffer.ReadString(p.mDisplayName);
    inBuffer.ReadString(p.mNetAddress);
    p.mCheckSum = inBuffer.ReadLong();
    p.mPatchSize = inBuffer.ReadLong();
    inBuffer.ReadString(p.mHostName);
    inBuffer.ReadString(p.mHostURL);
    inBuffer.ReadString(p.mHostText);
    inBuffer.ReadString(p.mHostImageURL);
    p.mMustVisitHost = inBuffer.ReadBool();
}
static void WritePatch(const SPatch& p, WriteBuffer& theWriteBuffer, bool bUpdateOnly)
{
    theWriteBuffer.AppendString(p.mConfigName);
    theWriteBuffer.AppendString(p.mFromVersion);
    theWriteBuffer.AppendString(p.mToVersion);
    theWriteBuffer.AppendByte(bUpdateOnly);
    theWriteBuffer.AppendString(p.mDisplayName);
    theWriteBuffer.AppendString(p.mNetAddress);
    theWriteBuffer.AppendLong(p.mCheckSum);
    theWriteBuffer.AppendLong(p.mPatchSize);
    theWriteBuffer.AppendString(p.mHostName);
    theWriteBuffer.AppendString(p.mHostURL);
    theWriteBuffer.AppendString(p.mHostText);
    theWriteBuffer.AppendString(p.mHostImageURL);
    theWriteBuffer.AppendByte(p.mMustVisitHost);
    theWriteBuffer.AppendByte(p.mIsActive);
}
// CheckValidVersionOp::CheckResponse after the header checks (and GetPatchListOp's: inVersion NULL)
static void ReplyData(const ByteBuffer* inData, const SVersion* inVersion, string& mVersionTo,
                      string& mVersionToDescriptionURL, SPatchList& mPatchDataList)
{
    ReadBuffer reply(inData->data(), inData->length());
    if (inVersion != NULL) {
        reply.ReadString(mVersionTo);
        reply.ReadString(mVersionToDescriptionURL);
        int count = (unsigned short)reply.ReadShort();
        for (int i = 0; i < count; ++i) {
            unsigned short length = reply.ReadShort();
            SmartPtr<SPatch> patch = new SPatch;
            patch->SetFromVersion(inVersion->GetVersion());
            patch->SetToVersion(mVersionTo);
            patch->SetIsActive(true);
            patch->SetConfigName(inVersion->GetConfigName());
            ReadBuffer data(reply.ReadBytes(length), length);
            ReadPatch(*static_cast<SPatch*>(patch), data, false);
            mPatchDataList.push_back(patch);
        }
    } else {
        unsigned short numPatches = reply.ReadShort();
        for (int patchNum = 0; patchNum < numPatches; ++patchNum) {
            unsigned short patchDataSize = reply.ReadShort();
            ReadBuffer patchData(reply.ReadBytes(patchDataSize), patchDataSize);
            SmartPtr<SPatch> thePatchData = new SPatch;
            ReadPatch(*static_cast<SPatch*>(thePatchData), patchData, true);
            mPatchDataList.push_back(thePatchData);
        }
    }
}
// VersionBaseOp's filter
typedef std::set<std::string> FilterStringSet;
static void BuildVersionFilterSet(const std::string& theFilterString, FilterStringSet& theFilterSet)
{
    unsigned int lastPos = 0;
    unsigned int commaPos = theFilterString.find(',');
    theFilterSet.clear();
    if (theFilterString.length() == 0)
        return;
    while (commaPos != -1) {
        theFilterSet.insert(theFilterString.substr(lastPos, commaPos - lastPos));
        lastPos = commaPos + 1;
        commaPos = theFilterString.find(',', lastPos);
    }
    theFilterSet.insert(theFilterString.substr(lastPos, theFilterString.size() - lastPos));
}
static bool IsFiltered(const FilterStringSet& mFilterStringSet, const std::string& theVersionData)
{
    FilterStringSet aFilterSet;
    if (theVersionData.empty())
        return false;
    std::string versionDescription = StringToLowerCase(theVersionData);
    unsigned int aLineStart = 0;
    unsigned int aSpacePos = 0;
    std::string aKey = "";
    std::string aKeyValue = "";
    while (aLineStart != -1 && aKey != "pt") {
        aSpacePos = versionDescription.find(' ', aLineStart);
        aKey = versionDescription.substr(aLineStart, aSpacePos - aLineStart);
        aLineStart = versionDescription.find('\n', aSpacePos);
        if (aLineStart != -1)
            ++aLineStart;
        else {
            aLineStart = versionDescription.find('\r', aSpacePos);
            if (aLineStart != -1)
                ++aLineStart;
        }
    }
    if (aKey == "pt") {
        if (aLineStart != -1) {
            aKeyValue = versionDescription.substr(aSpacePos + 1, aLineStart - (aSpacePos + 1));
            unsigned int aRemove = aKeyValue.find('\n');
            if (aRemove != std::string::npos)
                aKeyValue = aKeyValue.erase(aRemove, 1);
            aRemove = aKeyValue.find('\r');
            if (aRemove != std::string::npos)
                aKeyValue = aKeyValue.erase(aRemove, 1);
        } else
            aKeyValue = versionDescription.substr(aSpacePos + 1, versionDescription.size() - (aSpacePos + 1));
    } else
        return false;
    BuildVersionFilterSet(aKeyValue, aFilterSet);
    FilterStringSet::const_iterator anItr = aFilterSet.begin();
    for (; anItr != aFilterSet.end(); ++anItr) {
        if (*anItr == "")
            continue;
        if (mFilterStringSet.find(*anItr) == mFilterStringSet.end())
            return true;
    }
    return false;
}
} // namespace OrigPatch

static string AStr() { return Pick(5) ? FromSet("ab,1.", Pick(6)) : FromSet("x", Pick(300)); }

static void RandomVersion(SVersion& v)
{
    v.mConfigName = AStr();
    v.mVersion = AStr();
    v.mDescriptionURL = AStr();
    v.mState = (VersionState)Pick(5);
    int n = Pick(4);
    for (int i = 0; i < n; i++) v.mVersionTypeList.push_back(AStr());
}
static void RandomPatch(SPatch& p)
{
    p.mConfigName = AStr(); p.mFromVersion = AStr(); p.mToVersion = AStr(); p.mDisplayName = AStr();
    p.mNetAddress = AStr(); p.mHostName = AStr(); p.mHostURL = AStr(); p.mHostText = AStr(); p.mHostImageURL = AStr();
    p.mCheckSum = Next() * 255u; p.mPatchSize = Next();
    p.mMustVisitHost = Pick(2) != 0; p.mIsActive = Pick(2) != 0;
}
static string Bytes(WriteBuffer& w) { return string(w.data() ? w.data() : "", w.length()); }

template <class F> static string Run(F f)
{
    try {
        return f();
    } catch (ReadBufferException&) {
        return "throw";
    }
}

static void CheckRecordOn(int kind, const string& m)
{
    if (kind == 0) {
        SVersion a, b;
        a.mConfigName = b.mConfigName = "old";
        a.mVersionTypeList.push_back("keep");
        b.mVersionTypeList.push_back("keep");
        ReadBuffer ra(m.data(), (unsigned long)m.size()), rb(m.data(), (unsigned long)m.size());
        string ta = Run([&]() { OrigPatch::ReadVersion(a, ra); return string("ok"); });
        string tb = Run([&]() { PatchReadVersion(b, rb); return string("ok"); });
        CHECK(ta == tb && a.Dump() == b.Dump() && ra.pos() == rb.pos());
        if (!(ta == tb && a.Dump() == b.Dump() && ra.pos() == rb.pos()) && sFailures < 8)
            printf("version %s: %s %s %d / %s %s %d\n", Hex(m).c_str(), ta.c_str(), a.Dump().c_str(), ra.pos(), tb.c_str(), b.Dump().c_str(), rb.pos());
    } else if (kind <= 2) {
        SPatch a, b;
        a.mHostName = b.mHostName = "old";
        ReadBuffer ra(m.data(), (unsigned long)m.size()), rb(m.data(), (unsigned long)m.size());
        string ta = Run([&]() { OrigPatch::ReadPatch(a, ra, kind == 2); return string("ok"); });
        string tb = Run([&]() { PatchReadPatch(b, rb, kind == 2); return string("ok"); });
        CHECK(ta == tb && a.Dump() == b.Dump() && ra.pos() == rb.pos());
        if (!(ta == tb && a.Dump() == b.Dump()) && sFailures < 8)
            printf("patch %s: %s %s / %s %s\n", Hex(m).c_str(), ta.c_str(), a.Dump().c_str(), tb.c_str(), b.Dump().c_str());
    } else {
        SVersion v;
        v.mVersion = "1.0";
        v.mConfigName = "cfg";
        ConstSmartPtr<ByteBuffer> data = new ByteBuffer(m.data(), (unsigned long)m.size(), false);
        string toa = "x", ua = "y", tob = "x", ub = "y";
        SPatchList la, lb;
        string ta = Run([&]() { OrigPatch::ReplyData(data.get(), kind == 3 ? &v : NULL, toa, ua, la); return string("ok"); });
        string tb = Run([&]() { PatchReplyData<SPatch>(data.get(), kind == 3 ? &v : NULL, &tob, &ub, lb); return string("ok"); });
        string da = Hex(toa) + Hex(ua), db = Hex(tob) + Hex(ub);
        for (SPatchList::iterator i = la.begin(); i != la.end(); ++i) da += " " + (*i)->Dump();
        for (SPatchList::iterator i = lb.begin(); i != lb.end(); ++i) db += " " + (*i)->Dump();
        CHECK(ta == tb && da == db);
        if (!(ta == tb && da == db) && sFailures < 8)
            printf("reply %d %s: %s %s / %s %s\n", kind, Hex(m).c_str(), ta.c_str(), da.c_str(), tb.c_str(), db.c_str());
    }
}

static string ARecord(int kind)
{
    WriteBuffer w(0, true);
    if (kind == 0) {
        SVersion v;
        RandomVersion(v);
        OrigPatch::WriteVersion(v, w, 3);
    } else if (kind <= 2) {
        SPatch p;
        RandomPatch(p);
        WriteBuffer x(0, true);
        OrigPatch::WritePatch(p, x, Pick(2) != 0);
        string s = Bytes(x);
        if (kind == 1) {    // without the config, versions and flag
            size_t at = 0;
            for (int k = 0; k < 3; k++) at += 2 + (unsigned char)s[at] + 256 * (unsigned char)s[at + 1];
            s = s.substr(at + 1);
        }
        return s;
    } else {
        if (kind == 3) {
            w.AppendString(AStr());
            w.AppendString(AStr());
        }
        int n = Pick(4);
        w.AppendShort(n);
        for (int i = 0; i < n; i++) {
            string r = ARecord(kind == 3 ? 1 : 2);
            if (Pick(6) == 0) r = r.substr(0, Pick((int)r.size() + 1));
            w.AppendShort((short)r.size());
            w.AppendBytes(r.data(), (unsigned long)r.size());
        }
    }
    return Bytes(w);
}

static void CheckRecords()
{
    for (int i = 0; i < 25000; i++) {
        int kind = i % 5;
        string m = ARecord(kind);
        CheckRecordOn(kind, m);
        for (size_t n = 0; n < m.size(); n++) CheckRecordOn(kind, m.substr(0, n));
        for (int j = 0; j < 3 && !m.empty(); j++) {
            string x = m;
            x[Pick((int)x.size())] = (char)Pick(256);
            CheckRecordOn(kind, x);
        }
    }
    // the writers and the version type split
    for (int i = 0; i < 50000; i++) {
        SVersion v;
        RandomVersion(v);
        DWORD flags = Pick(4);
        WriteBuffer a(0, true), b(0, true);
        OrigPatch::WriteVersion(v, a, flags);
        PatchWriteVersion(v, b, flags);
        CHECK(Bytes(a) == Bytes(b));
        SPatch p;
        RandomPatch(p);
        bool u = Pick(2) != 0;
        WriteBuffer c(0, true), d(0, true);
        OrigPatch::WritePatch(p, c, u);
        PatchWritePatch(p, d, u);
        CHECK(Bytes(c) == Bytes(d));
        string t = FromSet("a,", Pick(8));
        SVersion e, f;
        OrigPatch::AddVersionType(e, t);
        f.AddVersionType(t);
        CHECK(e.Dump() == f.Dump());
    }
}

static void CheckRequests()
{
    for (int i = 0; i < 50000; i++) {
        // CheckValidVersionOp::RunHook
        string product = AStr();
        SVersion v;
        RandomVersion(v);
        bool list = Pick(2) != 0;
        WriteBuffer a(0, true);
        a.AppendString(product);
        OrigPatch::WriteVersion(v, a, 0);
        a.AppendBool(list);
        std::vector<string> s;
        s.push_back(product);
        s.push_back(v.mConfigName);
        s.push_back(v.mVersion);
        for (VersionTypeList::iterator t = v.mVersionTypeList.begin(); t != v.mVersionTypeList.end(); ++t) s.push_back(*t);
        ConstSmartPtr<ByteBuffer> b = PatchRequest(0, s, list);
        CHECK(Bytes(a) == string(b->data(), b->length()));
        // GetPatchListOp::RunHook and ReportPatchStatusOp::RunHook
        int n = Pick(2) ? 4 : 5;
        std::vector<string> q;
        WriteBuffer c(0, true);
        for (int k = 0; k < n; k++) {
            q.push_back(AStr());
            c.AppendString(q.back());
        }
        unsigned char byte = (unsigned char)Pick(256);
        c.AppendByte(byte);
        ConstSmartPtr<ByteBuffer> d = PatchRequest(1, q, byte);
        CHECK(Bytes(c) == string(d->data(), d->length()));
    }
}

static long sFilterThrows;
static void CheckFilters()
{
    for (int i = 0; i < 300000; i++) {
        string f = FromSet("opt,res", Pick(10));
        OrigPatch::FilterStringSet a, b;
        OrigPatch::BuildVersionFilterSet(f, a);
        PatchFilterSet(f, b);
        CHECK(a == b);
        string d;
        int lines = Pick(4);
        for (int k = 0; k < lines; k++) {
            static const char* kKeys[] = { "pt", "PT", "xx", "p", "" };
            d += string(kKeys[Pick(5)]) + (Pick(8) ? " " : "") + FromSet("opt,resLOW", Pick(10)) + (Pick(2) ? "\n" : Pick(2) ? "\r\n" : "\r");
        }
        if (Pick(5) == 0) d = FromSet("pt \r\n,o", Pick(14));
        // the original's substr threw std::out_of_range (the game would stop) for some data: counted, and the
        // port's answer for it printed once
        bool ra;
        try {
            ra = OrigPatch::IsFiltered(a, d);
        } catch (std::out_of_range&) {
            if (sFilterThrows++ == 0)
                printf("filter [%s] [%s]: the original threw, the port says %d\n", f.c_str(), Hex(d).c_str(), PatchIsFiltered(b, d));
            continue;
        }
        CHECK(ra == PatchIsFiltered(b, d));
        if (ra != PatchIsFiltered(b, d) && sFailures < 8)
            printf("filter [%s] [%s]: %d / %d\n", f.c_str(), Hex(d).c_str(), ra, PatchIsFiltered(b, d));
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    setvbuf(stdout, NULL, _IONBF, 0);
    void (*kChecks[])() = { CheckFtp, CheckRecords, CheckRequests, CheckFilters };
    for (int i = 0; i < 4; i++) {
        try {
                kChecks[i]();
        } catch (std::exception& e) {
            printf("check %d threw %s\n", i, e.what());
            sFailures++;
        }
    }
    printf("won_misc_test: %lld checks, %d failures (%ld version data where the original threw)\n", sChecks, sFailures,
           sFilterThrows);
    return sFailures;
}
