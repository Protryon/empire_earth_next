// WONMisc's HTTP (HTTPGetOp's dates, status and header lines, redirect, chunk length, request text, URL split and
// up-to-date test; URL; HTTPCache's ids, file names and keep rule): the original's statements (copied from
// HTTPGetOp.cpp, URL.cpp and HTTPCache.cpp before the port, the op's members on a stand-in) against
// include/ee/wonapi/WONHttpMsg.h over ee_won's http module and the URL unit linked here, over generated input (the
// original's CRT calls: gmtime, sscanf, atoi, toupper). The exit code is the number of failed checks.
//
// `--print N` prints N lines of each kind in the golden's format (rust/crates/ee_won/src/golden_http.txt): the
// kind, the input in hex, the original's result (`http_golden` replays it).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONURL.h"
#include "ee/wonapi/WONHttpMsg.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 9191;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }
static long long Next64() { return ((long long)Next() << 24) ^ Next() ^ ((long long)Next() << 40); }

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

// ---- the original's statements

namespace Orig {
static const char* monthTable[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const unsigned long Jan_Value = ('J' * 0x00010000) + ('a' * 0x00000100) + 'n';
static const unsigned long JAN_Value = ('J' * 0x00010000) + ('A' * 0x00000100) + 'N';
static const unsigned long Feb_Value = ('F' * 0x00010000) + ('e' * 0x00000100) + 'b';
static const unsigned long FEB_Value = ('F' * 0x00010000) + ('E' * 0x00000100) + 'B';
static const unsigned long Mar_Value = ('M' * 0x00010000) + ('a' * 0x00000100) + 'r';
static const unsigned long MAR_Value = ('M' * 0x00010000) + ('A' * 0x00000100) + 'R';
static const unsigned long Apr_Value = ('A' * 0x00010000) + ('p' * 0x00000100) + 'r';
static const unsigned long APR_Value = ('A' * 0x00010000) + ('P' * 0x00000100) + 'R';
static const unsigned long May_Value = ('M' * 0x00010000) + ('a' * 0x00000100) + 'y';
static const unsigned long MAY_Value = ('M' * 0x00010000) + ('A' * 0x00000100) + 'Y';
static const unsigned long Jun_Value = ('J' * 0x00010000) + ('u' * 0x00000100) + 'n';
static const unsigned long JUN_Value = ('J' * 0x00010000) + ('U' * 0x00000100) + 'N';
static const unsigned long Jul_Value = ('J' * 0x00010000) + ('u' * 0x00000100) + 'l';
static const unsigned long JUL_Value = ('J' * 0x00010000) + ('U' * 0x00000100) + 'L';
static const unsigned long Aug_Value = ('A' * 0x00010000) + ('u' * 0x00000100) + 'g';
static const unsigned long AUG_Value = ('A' * 0x00010000) + ('U' * 0x00000100) + 'G';
static const unsigned long Sep_Value = ('S' * 0x00010000) + ('e' * 0x00000100) + 'p';
static const unsigned long SEP_Value = ('S' * 0x00010000) + ('E' * 0x00000100) + 'P';
static const unsigned long Oct_Value = ('O' * 0x00010000) + ('c' * 0x00000100) + 't';
static const unsigned long OCT_Value = ('O' * 0x00010000) + ('C' * 0x00000100) + 'T';
static const unsigned long Nov_Value = ('N' * 0x00010000) + ('o' * 0x00000100) + 'v';
static const unsigned long NOV_Value = ('N' * 0x00010000) + ('O' * 0x00000100) + 'V';
static const unsigned long Dec_Value = ('D' * 0x00010000) + ('e' * 0x00000100) + 'c';
static const unsigned long DEC_Value = ('D' * 0x00010000) + ('E' * 0x00000100) + 'C';

static string MakeInternetDateTime(time_t theTime)
{
    struct tm aTime = *gmtime(&theTime);
    string result;
    switch (aTime.tm_wday) {
    case 0: result = "Sun"; break;
    case 1: result = "Mon"; break;
    case 2: result = "Tue"; break;
    case 3: result = "Wed"; break;
    case 4: result = "Thu"; break;
    case 5: result = "Fri"; break;
    default: result = "Sat"; break;
    };
    result += ", ";
    result += char((aTime.tm_mday / 10) % 10) + 48;
    result += char(aTime.tm_mday % 10) + 48;
    result += " ";
    result += monthTable[aTime.tm_mon % 12];
    result += " ";
    unsigned short year = 1900 + aTime.tm_year;
    result += char((year / 1000) % 10) + 48;
    result += char((year / 100) % 10) + 48;
    result += char((year / 10) % 10) + 48;
    result += char(year % 10) + 48;
    result += " ";
    result += char((aTime.tm_hour / 10) % 10) + 48;
    result += char(aTime.tm_hour % 10) + 48;
    result += ":";
    result += char((aTime.tm_min / 10) % 10) + 48;
    result += char(aTime.tm_min % 10) + 48;
    result += ":";
    result += char((aTime.tm_sec / 10) % 10) + 48;
    result += char(aTime.tm_sec % 10) + 48;
    result += " GMT";
    return result;
}

static time_t ParseInternetDateTime(const char* str)
{
    const char* tmpStr = str;
    for (; *tmpStr == ' '; tmpStr++)
        ;
    for (; *tmpStr != ' '; tmpStr++) {
        if (!*tmpStr)
            return 0;
    }
    for (; *tmpStr == ' '; tmpStr++)
        ;
    bool gotDay = false;
    int day = 0;
    int month;
    if (*tmpStr >= '0' && *tmpStr <= '9') {
        for (; *tmpStr != ' ' && *tmpStr != '-'; tmpStr++) {
            if (!*tmpStr)
                return 0;
            day *= 10;
            day += *tmpStr - 48;
        }
        for (; *tmpStr == ' ' || *tmpStr == '-'; tmpStr++)
            ;
        gotDay = true;
    }
    unsigned long dateValue = *(tmpStr++) * 0x00010000;
    dateValue += *(tmpStr++) * 0x00000100;
    dateValue += *(tmpStr++);
    switch (dateValue) {
    case Jan_Value: case JAN_Value: month = 0; break;
    case Feb_Value: case FEB_Value: month = 1; break;
    case Mar_Value: case MAR_Value: month = 2; break;
    case Apr_Value: case APR_Value: month = 3; break;
    case May_Value: case MAY_Value: month = 4; break;
    case Jun_Value: case JUN_Value: month = 5; break;
    case Jul_Value: case JUL_Value: month = 6; break;
    case Aug_Value: case AUG_Value: month = 7; break;
    case Sep_Value: case SEP_Value: month = 8; break;
    case Oct_Value: case OCT_Value: month = 9; break;
    case NOV_Value: case Nov_Value: month = 10; break;
    case Dec_Value: case DEC_Value: month = 11; break;
    default: return 0;
    }
    for (; *tmpStr == ' ' || *tmpStr == '-'; tmpStr++)
        ;
    if (!gotDay) {
        for (; *tmpStr != ' ' && *tmpStr != '-'; tmpStr++) {
            if (!*tmpStr)
                return 0;
            day *= 10;
            day += *tmpStr - 48;
        }
        for (; *tmpStr == ' ' || *tmpStr == '-'; tmpStr++)
            ;
    }
    int year = 0;
    bool gotYear = false;
    if (*(tmpStr + 2) != ':') {
        for (; *tmpStr != ' '; tmpStr++) {
            if (!*tmpStr) {
                if (year != 0)
                    break;
                return 0;
            }
            year *= 10;
            year += *tmpStr - 48;
        }
        gotYear = true;
    }
    int hour = 0;
    int mins = 0;
    int sec = 0;
    for (; *tmpStr == ' '; tmpStr++)
        ;
    if (*tmpStr < '0' || *tmpStr > '9') return 0;
    hour = (*tmpStr - 48) * 10;
    tmpStr++;
    if (*tmpStr < '0' || *tmpStr > '9') return 0;
    hour += *tmpStr - 48;
    tmpStr++;
    if (*tmpStr != ':') return 0;
    tmpStr++;
    if (*tmpStr < '0' || *tmpStr > '9') return 0;
    mins = (*tmpStr - 48) * 10;
    tmpStr++;
    if (*tmpStr < '0' || *tmpStr > '9') return 0;
    mins += *tmpStr - 48;
    tmpStr++;
    if (*tmpStr != ':') return 0;
    tmpStr++;
    if (*tmpStr < '0' || *tmpStr > '9') return 0;
    sec = (*tmpStr - 48) * 10;
    tmpStr++;
    if (*tmpStr < '0' || *tmpStr > '9') return 0;
    sec += *tmpStr - 48;
    tmpStr++;
    for (; *tmpStr == ' '; tmpStr++)
        ;
    if (!gotYear) {
        for (; *tmpStr != ' '; tmpStr++) {
            if (!*tmpStr) {
                if (year != 0)
                    break;
                return 0;
            }
            year *= 10;
            year += *tmpStr - 48;
        }
    }
    if (year < 100)
        year += 1900;
    struct tm timeStruct;
    timeStruct.tm_sec = sec;
    timeStruct.tm_min = mins;
    timeStruct.tm_hour = hour;
    timeStruct.tm_mday = day;
    timeStruct.tm_mon = month;
    timeStruct.tm_year = year - 1900;
    timeStruct.tm_isdst = 0;
    timeStruct.tm_wday = 0;
    timeStruct.tm_yday = 0;
    long long y = timeStruct.tm_year + 1900 - (timeStruct.tm_mon < 2 ? 1 : 0);
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long m = timeStruct.tm_mon + 1;
    long long doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + timeStruct.tm_mday - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long long days = era * 146097 + doe - 719468;
    return static_cast<time_t>(days * 86400 + timeStruct.tm_hour * 3600 + timeStruct.tm_min * 60 + timeStruct.tm_sec);
}
} // namespace Orig

// the op's members ExtractStatus and ExtractHeaderLine touch, and its Finish
struct SOp {
    int mHTTPStatus;
    bool mCloseConnection, mContentIsChunked, mIsFirstChunk, mDoClose;
    DWORD mContentLength;
    string mContentType, mRedirectHost, mRedirectPath, mEntityTag;
    time_t mLastModified, mServerTime, mExpireTime;
    int mFinished;
    SOp() : mHTTPStatus(-5), mCloseConnection(false), mContentIsChunked(false), mIsFirstChunk(false), mDoClose(false),
            mContentLength(7), mContentType("t"), mRedirectHost("rh"), mRedirectPath("rp"), mEntityTag("e"),
            mLastModified(1), mServerTime(2), mExpireTime(3), mFinished(0) {}
    void Finish(int s) { this->mFinished = s; }
    string Dump() const
    {
        char b[200];
        snprintf(b, sizeof(b), "%d %d %d %d %u %lld %lld %lld %d ", this->mHTTPStatus, this->mContentIsChunked,
                 this->mIsFirstChunk, this->mDoClose, (unsigned)this->mContentLength, (long long)this->mLastModified,
                 (long long)this->mServerTime, (long long)this->mExpireTime, this->mFinished);
        return b + Hex(this->mContentType) + " " + Hex(this->mRedirectHost) + " " + Hex(this->mRedirectPath) + " " +
               Hex(this->mEntityTag);
    }
};

namespace Orig {
static bool ExtractStatus(SOp& o, const char* inStatusLine)
{
    char protocol[50];
    if (sscanf(inStatusLine, "%s%d", protocol, &o.mHTTPStatus) != 2) {
        o.Finish(100092);
        return false;
    }
    if (o.mHTTPStatus > 304 && o.mCloseConnection) {
        o.Finish(100093);
        return false;
    }
    return true;
}
static bool ExtractRedirect(SOp& o, const char* inLocation)
{
    const char* host = strstr(inLocation, "://");
    if (host == NULL) {
        o.Finish(100094);
        return false;
    }
    host += 3;
    const char* path = strchr(host, '/');
    if (path == NULL) {
        o.mRedirectHost = host;
        o.mRedirectPath = '/';
    } else {
        o.mRedirectHost.assign(host, path - host);
        o.mRedirectPath = path;
    }
    return true;
}
static bool ExtractHeaderLine(SOp& o, const char* theHeaderLine)
{
    const char* aColon = strchr(theHeaderLine, ':');
    if (aColon == NULL)
        return true;
    const int NUM_KEYS = 9;
    static const char* keys[NUM_KEYS] = { "Content-Length", "Content-Type", "Last-Modified", "Location",
                                          "Transfer-Encoding", "Connection", "Date", "Expires", "ETag" };
    int i;
    for (i = 0; i < NUM_KEYS; i++) {
        const char* aPtr = theHeaderLine;
        int aPos = 0;
        bool match = true;
        while (aPtr != aColon) {
            if (toupper(*aPtr) != toupper(keys[i][aPos])) {
                match = false;
                break;
            }
            aPtr++;
            aPos++;
        }
        if (match)
            break;
    }
    const char* aVal = aColon + 1;
    while (*aVal == ' ')
        aVal++;
    switch (i) {
    case 0: o.mContentLength = atoi(aVal); break;
    case 1: o.mContentType.assign(aVal); break;
    case 2: o.mLastModified = ParseInternetDateTime(aVal); break;
    case 3: return ExtractRedirect(o, aVal);
    case 4: {
        string aStr = aVal;
        StringToUpperCaseInPlace(aStr);
        if (aStr.find("CHUNKED") == 0) {
            o.mContentIsChunked = true;
            o.mIsFirstChunk = true;
        }
        break;
    }
    case 5: {
        string aStr = aVal;
        StringToUpperCaseInPlace(aStr);
        if (aStr.find("CLOSE") == 0)
            o.mDoClose = true;
        break;
    }
    case 6: o.mServerTime = ParseInternetDateTime(aVal); break;
    case 7: o.mExpireTime = ParseInternetDateTime(aVal); break;
    case 8: {
        o.mEntityTag = aVal;
        int anEndQuotePos = o.mEntityTag.find('"', 1);
        if (anEndQuotePos == string::npos)
            o.mEntityTag.erase();
        else
            o.mEntityTag.resize(anEndQuotePos + 1);
    } break;
    }
    return true;
}

// SendRequest's text, the stat's results and the cache entry's state handed in
struct Req {
    string mProxyHost, mHost, mRemotePath, mLocalPath, mEntityTag;
    bool mCloseConnection, mExists, mDoRangeResume, mHasEntry, mEntryValid;
    size_t mSize;
    time_t mTime;
};
static string SendRequestText(const Req& r)
{
    WriteBuffer aRequest(0, true);
    char aRangeOffset[20];
    aRequest.AppendString("GET ", 0);
    if (r.mProxyHost.length() != 0)
        aRequest.AppendString("http://" + r.mHost, 0);
    aRequest.AppendString(r.mRemotePath + " HTTP/1.1\r\n", 0);
    aRequest.AppendString("Host: " + r.mHost + "\r\n", 0);
    if (r.mCloseConnection)
        aRequest.AppendString("Connection: close\r\n", 0);
    else
        aRequest.AppendString("Connection: Keep-Alive\r\n", 0);
    if (!r.mLocalPath.empty()) {
        if (r.mExists) {
            size_t mLocalFileSize = r.mSize;
            time_t mLocalFileModifyTime = r.mTime;
            if (mLocalFileSize > 0) {
                if (r.mDoRangeResume) {
                    if (!r.mEntityTag.empty())
                        aRequest.AppendString("If-Range: " + r.mEntityTag + "\r\n", 0);
                    else
                        aRequest.AppendString("If-Range: " + MakeInternetDateTime(mLocalFileModifyTime) + "\r\n", 0);
                    aRequest.AppendString("Range: bytes=", 0);
                    snprintf(aRangeOffset, 10, "%d", static_cast<int>(mLocalFileSize));
                    aRequest.AppendString(aRangeOffset, 0);
                    aRequest.AppendString("-\r\n", 0);
                } else if (!r.mHasEntry || r.mEntryValid) {
                    if (!r.mEntityTag.empty())
                        aRequest.AppendString("If-None-Match: " + r.mEntityTag + "\r\n", 0);
                    aRequest.AppendString("If-Modified-Since: " + MakeInternetDateTime(mLocalFileModifyTime) + "\r\n", 0);
                }
            }
        }
    }
    aRequest.AppendString("\r\n", 0);
    ConstSmartPtr<ByteBuffer> b = aRequest.ToByteBuffer(true);
    return string(b->data(), b->length());
}

// HTTPGetOp(url)'s split
static void SplitURL(const std::string& inURL, string& outHost, string& outPath)
{
    std::string aURL(inURL);
    std::string::size_type aScheme = aURL.find("://");
    if (aScheme != std::string::npos)
        aURL = aURL.substr(aScheme + 3);
    std::string::size_type aSlash = aURL.find('/');
    outHost = aURL.substr(0, aSlash);
    if (aSlash != std::string::npos)
        outPath = aURL.substr(aSlash);
    else
        outPath = "/";
}

// RecvContent's up-to-date test
static bool UpToDate(bool append, size_t mLocalFileSize, DWORD mContentLength, time_t mLocalFileModifyTime,
                     time_t mLastModified, bool mDoClose)
{
    return !append && mLocalFileSize > 0 && mLocalFileSize == mContentLength &&
           abs(mLocalFileModifyTime - mLastModified) <= 1 && mDoClose;
}

// URL (the members public)
struct OrigURL {
    URLType mType;
    std::string mAddress;
    std::string mPath;
    bool mIsAbsolute;
    OrigURL() : mType(URLType_None), mIsAbsolute(false) {}
    OrigURL(const std::string& inURL)
    {
        this->mIsAbsolute = false;
        int protocolEnd = inURL.find("://");
        if (protocolEnd != std::string::npos) {
            std::string protocol = inURL.substr(0, protocolEnd);
            if (_stricmp(protocol.c_str(), "HTTP") == 0) this->mType = URLType_HTTP;
            else if (_stricmp(protocol.c_str(), "FTP") == 0) this->mType = URLType_FTP;
            else if (_stricmp(protocol.c_str(), "FILE") == 0) this->mType = URLType_File;
            else {
                this->mType = URLType_None;
                return;
            }
            this->mIsAbsolute = true;
            protocolEnd += 3;
            int addressEnd = inURL.find_first_of('/', protocolEnd);
            this->mAddress = inURL.substr(protocolEnd, addressEnd - protocolEnd);
            if (addressEnd == std::string::npos) this->mPath = "/";
            else this->mPath = inURL.substr(addressEnd);
        } else {
            this->mType = URLType_File;
            this->mPath = inURL;
        }
        if (this->mType == URLType_File) {
            for (int i = 0; i < this->mPath.length(); ++i)
                if (this->mPath[i] == '\\') this->mPath[i] = '/';
        }
    }
    void CalcRelativeTo(const OrigURL& inBase)
    {
        if (this->mIsAbsolute) return;
        this->mIsAbsolute = inBase.mIsAbsolute;
        this->mType = inBase.mType;
        this->mAddress = inBase.mAddress;
        if (this->mPath[0] == '#') {
            this->mPath = inBase.mPath + this->mPath;
            return;
        }
        if (!this->mPath.empty() && !inBase.mPath.empty() && this->mPath[0] != '/') {
            std::string relativePath = this->mPath;
            this->mPath = inBase.mPath;
            int slash = this->mPath.rfind('/');
            if (slash == std::string::npos) this->mPath += '/';
            else this->mPath = this->mPath.substr(0, slash + 1);
            this->mPath += relativePath;
        }
    }
    void GetEntire(std::string& outString)
    {
        outString.erase();
        switch (this->mType) {
        case URLType_HTTP: outString += "http://"; break;
        case URLType_FTP: outString += "ftp://"; break;
        default: break;
        }
        outString += this->mAddress;
        outString += this->mPath;
    }
};

// HTTPCache's statements over an id map
static int GetFreeId(const std::map<int, int>& ids, int& mNextFreeId)
{
    std::map<int, int>::const_iterator i = ids.find(mNextFreeId);
    while (i != ids.end()) {
        if (mNextFreeId != i->first) break;
        ++i;
        ++mNextFreeId;
    }
    return mNextFreeId;
}
static string CacheFileName(int anId, const std::string& inLocation)
{
    std::string type;
    int slash = inLocation.find_last_of('/');
    if (slash == std::string::npos) slash = inLocation.find_last_of('\\');
    if (slash != std::string::npos) {
        int dot = inLocation.find_last_of('.');
        if (dot > slash && dot + 10 >= inLocation.length())
            type = inLocation.substr(dot + 1);
        else type = "dat";
    } else type = "dat";
    char fileName[50];
    sprintf(fileName, "%06d.%s", anId, type.c_str());
    return fileName;
}
} // namespace Orig

static string Url(const URL& u)
{
    return std::to_string((int)u.GetType()) + " " + Hex(u.GetAddress()) + " " + Hex(u.GetPath());
}
static string Url(const Orig::OrigURL& u)
{
    return std::to_string((int)u.mType) + " " + Hex(u.mAddress) + " " + Hex(u.mPath);
}

// ---- generated input

static string ADate()
{
    long long t = Next64() % 32535215999LL;
    if (t < 0) t = -t;
    string d = Orig::MakeInternetDateTime((time_t)t);
    int form = Pick(4);
    if (form == 1) {    // RFC 850
        static const char* kDays[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        d = string(kDays[Pick(7)]) + ", " + d.substr(5, 2) + "-" + d.substr(8, 3) + "-" + d.substr(14, 2) + " " +
            d.substr(17, 8) + " GMT";
    } else if (form == 2) {     // asctime
        d = d.substr(0, 3) + " " + d.substr(8, 3) + " " + (d[5] == '0' ? string(" ") + d[6] : d.substr(5, 2)) + " " +
            d.substr(17, 8) + " " + d.substr(12, 4);
    } else if (form == 3) {
        for (size_t i = 0; i < d.size(); i++) if (d[i] >= 'a' && d[i] <= 'z' && Pick(3) == 0) d[i] -= 32;
    }
    int n = Pick(5);
    for (int i = 0; i < n && !d.empty(); i++) {
        int k = Pick(4);
        size_t at = Pick((int)d.size());
        if (k == 0) d.erase(at, 1);
        else if (k == 1) d[at] = " -:0123456789JanFxyz\x80\xff"[Pick(22)];
        else if (k == 2) d = d.substr(0, at);
        else d.insert(at, 1, " -:9"[Pick(4)]);
    }
    if (Pick(10) == 0) d = FromSet(" -:0123456789JanFebNOVxyz", Pick(30));
    return d;
}

static string ANumber()
{
    static const char* kNums[] = { "0", "200", "304", "305", "-1", "+7", "99999999999", "2147483647", "2147483648",
                                   "-2147483648", "4294967296", "123456789012345678901234567890", "-", "+", "", "007" };
    string s = kNums[Pick(16)];
    if (Pick(4) == 0) s = FromSet("0123456789", 1 + Pick(25));
    return s;
}

static string AStatusLine()
{
    string s = FromSet(" \t", Pick(3)) + (Pick(8) ? string("HTTP/1.") + (char)('0' + Pick(2)) : FromSet("HTP/1x\x80", Pick(12)));
    s += FromSet(" \t\v", Pick(4)) + ANumber();
    if (Pick(2)) s += " " + FromSet("OK Not Found", Pick(12));
    if (Pick(8) == 0) s = FromSet(" 0123456789-+aHT\t", Pick(20));
    return s;
}

static string AHeaderLine()
{
    static const char* kNames[] = { "Content-Length", "content-type", "LAST-MODIFIED", "Location", "Transfer-Encoding",
                                    "Connection", "Date", "Expires", "ETag", "Content", "", "X-Other", "Con",
                                    "Content-Lengthx", "e", "Dat", "Etag" };
    string name = kNames[Pick(17)];
    if (Pick(10) == 0) name = FromSet("abcdefgTAG-", Pick(10));
    string v;
    switch (Pick(8)) {
    case 0: v = ANumber(); break;
    case 1: v = ADate(); break;
    case 2: v = (Pick(2) ? "chunked" : "CLOSE") + FromSet(" x;", Pick(3)); break;
    case 3: v = (Pick(4) ? "http://" : Pick(2) ? "ftp:/" : "") + FromSet("ab.:", Pick(6)) + (Pick(2) ? "/" + FromSet("p/q.", Pick(8)) : ""); break;
    case 4: v = "\"" + FromSet("ab\"", Pick(6)) + FromSet("\"x", Pick(3)); break;
    case 5: v = FromSet("keep-alive close", Pick(12)); break;
    default: v = FromSet(" abc:\"/-0129", Pick(15)); break;
    }
    string s = name + (Pick(12) ? ":" : "") + FromSet(" ", Pick(4)) + v;
    if (Pick(6) == 0) s += "\r";
    return s;
}

static string AHexLine()
{
    static const char* kLines[] = { "0", "1a", "FFFFFFFF", "100000000", "0x10", "0X", "0x", "-1", "+f", " \t2f\r",
                                    "g", "", "-", "0xg", "123456789abcdef", "00000000000000001" };
    string s = kLines[Pick(16)];
    if (Pick(3) == 0) s = FromSet(" 0123456789abcdefABCDEFxX-+g\r", Pick(12));
    return s;
}

// ---- the checks

static void CheckDates()
{
    for (int i = 0; i < 100000; i++) {
        long long t = Next64() % 32535216000LL;
        if (t < 0) t = -t;
        if (i < 20) t = (long long)i * 86399 + (i & 1) * 32535215999LL / 2;
        CHECK(Orig::MakeInternetDateTime((time_t)t) == HttpFormatDate((time_t)t));
    }
    for (int i = 0; i < 300000; i++) {
        string d = ADate();
        char aBuf[700];
        memset(aBuf, 0, sizeof(aBuf));
        memcpy(aBuf, d.data(), d.size() < 600 ? d.size() : 600);
        time_t a = Orig::ParseInternetDateTime(aBuf);
        time_t b = HttpParseDate(aBuf);
        CHECK(a == b);
        if (a != b && sFailures < 8) printf("date [%s]: %lld / %lld\n", aBuf, (long long)a, (long long)b);
    }
}

static void CheckLines()
{
    for (int i = 0; i < 300000; i++) {
        string s = AStatusLine();
        SOp a, b;
        a.mCloseConnection = b.mCloseConnection = Pick(2) != 0;
        bool ra = Orig::ExtractStatus(a, s.c_str());
        bool rb = HttpExtractStatus(b, s.c_str());
        CHECK(ra == rb && a.Dump() == b.Dump());
        if (!(ra == rb && a.Dump() == b.Dump()) && sFailures < 8)
            printf("status [%s]: %d %s / %d %s\n", s.c_str(), ra, a.Dump().c_str(), rb, b.Dump().c_str());
    }
    for (int i = 0; i < 400000; i++) {
        string s = AHeaderLine();
        SOp a, b;
        bool ra = Orig::ExtractHeaderLine(a, s.c_str());
        bool rb = HttpExtractHeaderLine(b, s.c_str());
        CHECK(ra == rb && a.Dump() == b.Dump());
        if (!(ra == rb && a.Dump() == b.Dump()) && sFailures < 8)
            printf("header [%s]: %d %s / %d %s\n", s.c_str(), ra, a.Dump().c_str(), rb, b.Dump().c_str());
    }
    for (int i = 0; i < 200000; i++) {
        string s = AHexLine();
        DWORD a = 0x5a5a, b = 0x5a5a;
        bool ra = sscanf(s.c_str(), "%x", &a) == 1;
        uint32_t v = 0x5a5a;
        bool rb = ee_won_http_chunk_length(reinterpret_cast<const uint8_t*>(s.c_str()), &v);
        if (rb) b = v;
        CHECK(ra == rb && a == b);
        if (!(ra == rb && a == b) && sFailures < 8) printf("hex [%s]: %d %u / %d %u\n", s.c_str(), ra, (unsigned)a, rb, (unsigned)b);
    }
}

static void CheckRequests()
{
    for (int i = 0; i < 100000; i++) {
        Orig::Req r;
        r.mProxyHost = Pick(3) ? "" : "proxy:8080";
        r.mHost = FromSet("abc.", 1 + Pick(10));
        r.mRemotePath = "/" + FromSet("p/q.x", Pick(20));
        r.mLocalPath = Pick(4) ? "local" : "";
        r.mEntityTag = Pick(2) ? "" : "\"" + FromSet("ab", Pick(5)) + "\"";
        r.mCloseConnection = Pick(2) != 0;
        r.mExists = Pick(4) != 0;
        r.mDoRangeResume = Pick(2) != 0;
        r.mHasEntry = Pick(2) != 0;
        r.mEntryValid = Pick(2) != 0;
        static const size_t kSizes[] = { 0, 1, 999999999, 1000000000, 2147483647, 2147483648u, 4294967295u };
        r.mSize = Pick(3) ? (size_t)Pick(100000) : kSizes[Pick(7)];
        if (sizeof(size_t) > 4 && Pick(20) == 0) r.mSize = (size_t)Next64() & 0xfffffffffffULL;
        r.mTime = (time_t)((Next64() & 0x7fffffffffLL) % 32535215999LL);
        string a = Orig::SendRequestText(r);
        ConstSmartPtr<ByteBuffer> bb = HttpRequestBytes(!r.mProxyHost.empty(), r.mHost, r.mRemotePath,
                                                        r.mCloseConnection, !r.mLocalPath.empty() && r.mExists, r.mSize,
                                                        r.mTime, r.mDoRangeResume, r.mEntityTag,
                                                        !r.mHasEntry || r.mEntryValid);
        string b(bb->data(), bb->length());
        CHECK(a == b);
        if (a != b && sFailures < 8) printf("request:\n%s\n/\n%s\n", a.c_str(), b.c_str());
    }
    for (int i = 0; i < 100000; i++) {
        string u = (Pick(3) ? string("http://") : Pick(2) ? string("") : string("x:/")) + FromSet("ab./:", Pick(12));
        string ha, pa;
        Orig::SplitURL(u, ha, pa);
        size_t h0 = 0, h1 = 0;
        intptr_t p0 = 0;
        ee_won_http_split_url(reinterpret_cast<const uint8_t*>(u.data()), u.size(), &h0, &h1, &p0);
        string hb = u.substr(h0, h1 - h0), pb = p0 < 0 ? string("/") : u.substr(p0);
        CHECK(ha == hb && pa == pb);
    }
    for (int i = 0; i < 100000; i++) {
        bool append = Pick(4) == 0, close = Pick(4) != 0;
        size_t size = Pick(3) ? (size_t)Pick(5) : (size_t)Next64();
        DWORD length = Pick(2) ? (DWORD)size : (DWORD)Pick(5);
        time_t lt = Pick(2) ? (time_t)Pick(5) : (time_t)Next64(), lm = Pick(2) ? lt + Pick(5) - 2 : (time_t)Next64();
        CHECK(Orig::UpToDate(append, size, length, lt, lm, close) ==
              ee_won_http_up_to_date(append, size, length, lt, lm, close));
    }
}

static string AUrl()
{
    static const char* kSchemes[] = { "http://", "HTTP://", "ftp://", "File://", "gopher://", "", "://", "Http\0x://" };
    string s = Pick(8) == 7 ? string("Http\0x://", 9) : string(kSchemes[Pick(7)]);
    s += FromSet("ab.\\/#", Pick(14));
    return s;
}

static void CheckUrls()
{
    for (int i = 0; i < 200000; i++) {
        string s = AUrl(), base = AUrl();
        Orig::OrigURL a(s), ab(base);
        URL b(s), bb(base);
        CHECK(Url(a) == Url(b));
        a.CalcRelativeTo(ab);
        b.CalcRelativeTo(bb);
        CHECK(Url(a) == Url(b));
        string ea, eb;
        a.GetEntire(ea);
        b.GetEntire(eb);
        CHECK(ea == eb);
        if ((Url(a) != Url(b) || ea != eb) && sFailures < 8)
            printf("url [%s] [%s]: %s %s / %s %s\n", s.c_str(), base.c_str(), Url(a).c_str(), Hex(ea).c_str(), Url(b).c_str(), Hex(eb).c_str());
    }
}

static void CheckCache()
{
    for (int i = 0; i < 50000; i++) {
        std::map<int, int> m;
        int n = Pick(12);
        for (int j = 0; j < n; j++) m[Pick(15) - 2] = 1;
        int next = Pick(16) - 2;
        int a = next;
        int ra = Orig::GetFreeId(m, a);
        std::vector<int> ids;
        for (std::map<int, int>::iterator it = m.begin(); it != m.end(); ++it) ids.push_back(it->first);
        int rb = ee_won_http_cache_free_id(ids.empty() ? NULL : &ids[0], ids.size(), next);
        CHECK(ra == rb && a == rb);
    }
    for (int i = 0; i < 100000; i++) {
        string loc = FromSet("ab/\\.", Pick(20));
        if (Pick(10) == 0) loc += string(1, '\0') + "z";
        int id = Pick(4) ? Pick(1000000) : (int)Next64();
        string a = Orig::CacheFileName(id, loc);
        string b;
        ee_won_http_cache_file_name(id, reinterpret_cast<const uint8_t*>(loc.data()), loc.size(), HttpStringSink, &b);
        CHECK(a == b);
        if (a != b && sFailures < 8) printf("cache [%s]: %s / %s\n", Hex(loc).c_str(), a.c_str(), b.c_str());
    }
    for (int i = 0; i < 1000; i++) {
        time_t now = (time_t)Next64(), last = Pick(2) ? now - Pick(2000000) : (time_t)Next64();
        bool valid = Pick(4) != 0;
        CHECK((valid && now - last < 1209600) == ee_won_http_cache_keeps(valid, now, last));
    }
}

// the golden: dates parsed, status lines, header lines (their reading), hex lines, request inputs
static void Print(int inCount)
{
    for (int i = 0; i < inCount; i++) {
        long long t = (Next64() & 0x7fffffffffLL) % 32535215999LL;
        printf("format %lld %s\n", t, Orig::MakeInternetDateTime((time_t)t).c_str());
        string d = ADate();
        char aBuf[700];
        memset(aBuf, 0, sizeof(aBuf));
        memcpy(aBuf, d.data(), d.size() < 600 ? d.size() : 600);
        printf("parse %s %lld\n", Hex(d).c_str(), (long long)Orig::ParseInternetDateTime(aBuf));
        string s = AStatusLine();
        SOp o;
        o.mCloseConnection = true;
        bool r = Orig::ExtractStatus(o, s.c_str());
        printf("status %s %d %d\n", Hex(s).c_str(), o.mFinished == 100092 ? 0 : 1, o.mHTTPStatus);
        (void)r;
        s = AHeaderLine();
        SOp h;
        bool rh = Orig::ExtractHeaderLine(h, s.c_str());
        printf("header %s %d %s\n", Hex(s).c_str(), rh, h.Dump().c_str());
        s = AHexLine();
        DWORD x = 0x5a5a;
        int rx = sscanf(s.c_str(), "%x", &x);
        printf("hex %s %d %u\n", Hex(s).c_str(), rx == 1, (unsigned)x);
    }
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrint) {
        Print(aPrint);
        return 0;
    }
    CheckDates();
    CheckLines();
    CheckRequests();
    CheckUrls();
    CheckCache();
    printf("won_http_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
