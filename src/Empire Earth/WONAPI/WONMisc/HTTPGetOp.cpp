// HTTPGetOp (0x15c bytes): an HTTP/1.1 GET with proxy, redirects, chunked replies, range resume and caching
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <algorithm>
#include <map>
#include <stdio.h>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include "platform/PlatFile.h"
using std::string;
#include "ee/wonapi/WONLobbyGUI.h"
using WONAPI::CriticalSection;
static const char* monthTable[] = {	"Jan", "Feb", "Mar",
									"Apr", "May", "Jun",
									"Jul", "Aug", "Sep",
									"Oct", "Nov", "Dec" };


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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x006d5b60
// Formats a time as an RFC 1123 HTTP date ("Sun, 06 Nov 1994 08:49:37 GMT")
static string MakeInternetDateTime(time_t theTime)
{
	struct tm aTime = *gmtime(&theTime);
	
	string result;

	switch (aTime.tm_wday)
	{
	case 0:
		result = "Sun";
		break;
	case 1:
		result = "Mon";
		break;
	case 2:
		result = "Tue";
		break;
	case 3:
		result = "Wed";
		break;
	case 4:
		result = "Thu";
		break;
	case 5:
		result = "Fri";
		break;
//	case 6:
	default:
		result = "Sat";
		break;
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
	result += " GMT";	// claims GMT for now

	return result;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Potential internet date/time formats:

//      Sun, 06 Nov 1994 08:49:37 GMT    ; RFC 822, updated by RFC 1123
//      Sunday, 06-Nov-94 08:49:37 GMT   ; RFC 850, obsoleted by RFC 1036
//      Sun Nov  6 08:49:37 1994         ; ANSI C's asctime() format

// FUNCTION: WON 0x006d4c60
// Parses an HTTP date in RFC 1123, RFC 850 or asctime form; returns 0 on failure (time zone ignored)
static time_t ParseInternetDateTime(const char* str)
{
	const char* tmpStr = str;

	// skip leading spaces
	for (;*tmpStr == ' '; tmpStr++)
		;

	// skip day of week
	for (;*tmpStr != ' ' ; tmpStr++)
	{
		if (!*tmpStr)
			return 0;
	}

	// skip spaces
	for (;*tmpStr == ' '; tmpStr++)
		;

	bool gotDay = false;
	int day = 0;
	int month;

	// if the next char is numeric, then we're recving a day, else we're recving a month
	if (*tmpStr >= '0' && *tmpStr <= '9')
	{
		// read day of month
		for (;*tmpStr != ' ' && *tmpStr != '-'; tmpStr++)
		{
			if (!*tmpStr)
				return 0;

			day *= 10;
			day += *tmpStr - 48;
		}

		// skip spaces or dashes
		for (;*tmpStr == ' ' || *tmpStr == '-'; tmpStr++)
			;
		gotDay = true;
	}

	// read month

	//  Cheap trick here.  Since the month must be 3 chars, and 3 bytes fits in a long, we can make a switch statement
	unsigned long dateValue  = *(tmpStr++) * 0x00010000;
	dateValue += *(tmpStr++) * 0x00000100;
	dateValue += *(tmpStr++);
	switch (dateValue)
	{
	case Jan_Value:
	case JAN_Value:
		month = 0;
		break;
	case Feb_Value:
	case FEB_Value:
		month = 1;
		break;
	case Mar_Value:
	case MAR_Value:
		month = 2;
		break;
	case Apr_Value:
	case APR_Value:
		month = 3;
		break;
	case May_Value:
	case MAY_Value:
		month = 4;
		break;
	case Jun_Value:
	case JUN_Value:
		month = 5;
		break;
	case Jul_Value:
	case JUL_Value:
		month = 6;
		break;
	case Aug_Value:
	case AUG_Value:
		month = 7;
		break;
	case Sep_Value:
	case SEP_Value:
		month = 8;
		break;
	case Oct_Value:
	case OCT_Value:
		month = 9;
		break;
	case NOV_Value:
	case Nov_Value:
		month = 10;
		break;
	case Dec_Value:
	case DEC_Value:
		month = 11;
		break;
	default:
		return 0;
	}

	// skip spaces
	for (;*tmpStr == ' ' || *tmpStr == '-'; tmpStr++)
		;

	if (!gotDay)
	{
		// read day of month

		for (;*tmpStr != ' ' && *tmpStr != '-'; tmpStr++)
		{
			if (!*tmpStr)
				return 0;

			day *= 10;
			day += *tmpStr - 48;
		}

		// skip spaces or dashes
		for (;*tmpStr == ' ' || *tmpStr == '-'; tmpStr++)
			;
	}

	int year = 0;

	bool gotYear = false;
	if (*(tmpStr+2) != ':')	// Check to see if year preceeds time
	{
		// read year

		for (;*tmpStr != ' '; tmpStr++)
		{
			if (!*tmpStr)
			{
				if (year != 0)	// year might still be good
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

	// skip spaces
	for (;*tmpStr == ' '; tmpStr++)
		;

	// parse 2 digit hour
	
	if (*tmpStr < '0' || *tmpStr > '9')
		return 0;
	
	hour = (*tmpStr - 48) * 10;
	
	tmpStr++;

	if (*tmpStr < '0' || *tmpStr > '9')
		return 0;
	
	hour += *tmpStr - 48;

	// skip ':'
	tmpStr++;

	if (*tmpStr != ':')
		return 0;

	// parse 2 digit min
	tmpStr++;

	if (*tmpStr < '0' || *tmpStr > '9')
		return 0;

	mins = (*tmpStr - 48) * 10;

	tmpStr++;

	if (*tmpStr < '0' || *tmpStr > '9')
		return 0;

	mins += *tmpStr - 48;

	// skip ':'
	tmpStr++;

	if (*tmpStr != ':')
		return 0;

	// parse 2 digit seconds
	tmpStr++;

	if (*tmpStr < '0' || *tmpStr > '9')
		return 0;

	sec = (*tmpStr - 48) * 10;

	tmpStr++;

	if (*tmpStr < '0' || *tmpStr > '9')
		return 0;

	sec += *tmpStr - 48;

	tmpStr++;

	// skip spaces
	for (;*tmpStr == ' '; tmpStr++)
		;

	if (!gotYear)
	{
		// read year

		for (;*tmpStr != ' '; tmpStr++)
		{
			if (!*tmpStr)
			{
				if (year != 0)	// year might still be good
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
	timeStruct.tm_isdst = 0;  // GMT, no DST

	// Unused
	timeStruct.tm_wday = 0;
	timeStruct.tm_yday = 0;

	// The value is in GMT: the original took mktime (standard local time) minus _timezone, i.e. timegm, which has
	// no portable name: seconds since 1970 from the civil date (proleptic Gregorian)
	long long y = timeStruct.tm_year + 1900 - (timeStruct.tm_mon < 2 ? 1 : 0);
	long long era = (y >= 0 ? y : y - 399) / 400;
	long long yoe = y - era * 400;
	long long m = timeStruct.tm_mon + 1;
	long long doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + timeStruct.tm_mday - 1;
	long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	long long days = era * 146097 + doe - 719468;
	return static_cast<time_t>(days * 86400 + timeStruct.tm_hour * 3600 + timeStruct.tm_min * 60 + timeStruct.tm_sec);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace WONAPI {
std::string HTTPGetOp::mStaticProxyHost;
}
static string& GetProxyHostFileName()
{
	static string mProxyHostFileName = "_wonwebproxy.txt";
	return mProxyHostFileName;
}
static CriticalSection& GetStaticCrit()
{
	static CriticalSection aCrit;
	return aCrit;
}
namespace WONAPI {
// FUNCTION: WON 0x006d3830
// Reads the web proxy host from the proxy file (once unless forced)
void HTTPGetOp::ReadProxyHostFile(bool force)
{
	static bool checked = false;
	AutoCrit aCrit(GetStaticCrit());
	if(checked && !force)
		return;

	checked = true;

	FILE *aFile = PlatFOpen(GetProxyHostFileName().c_str(),"r");
	if(aFile==NULL)
		return;

	char aProxyBuf[512];
	aProxyBuf[0] = '\0';
	fgets(aProxyBuf,500,aFile);
	fclose(aFile);

	char *aPtr = strchr(aProxyBuf,'\n');
	if(aPtr!=NULL)
		*aPtr = '\0';

	HTTPGetOp::mStaticProxyHost = aProxyBuf;
}
inline void HTTPGetOp::Init()
{
    this->mBinaryFile=true;
    this->mCloseConnection=true;
    this->mDoRangeResume=true;
    this->mAutoRedirect=true;
    this->mOnlyRecv=false;
    this->mOnlySend=false;
}
// FUNCTION: WON 0x006d3b10
// Creates a GET for a host and path, prefixing the proxy file name with the WON directory and reading it
HTTPGetOp::HTTPGetOp(const std::string& inHost,const std::string& inRemotePath)
    : mHost(inHost),mRemotePath(inRemotePath),mContentAccumulator(0,true),
      mNumRedirects(0)
{
    this->Init();
    GetProxyHostFileName()=WONAPICore::GetDefaultFileDirectory()+GetProxyHostFileName();
    HTTPGetOp::ReadProxyHostFile(false);
}
// FUNCTION: WON 0x006d3f90
// Creates a GET for a URL, splitting it into host and path ("/" when none)
HTTPGetOp::HTTPGetOp(const std::string& inURL)
    : mContentAccumulator(0,true),mNumRedirects(0)
{
    this->Init();
    GetProxyHostFileName()=WONAPICore::GetDefaultFileDirectory()+GetProxyHostFileName();
    HTTPGetOp::ReadProxyHostFile(false);
    std::string aURL(inURL);
    std::string::size_type aScheme=aURL.find("://");
    if (aScheme!=std::string::npos)
        aURL=aURL.substr(aScheme+3);
    std::string::size_type aSlash=aURL.find('/');
    this->mHost=aURL.substr(0,aSlash);
    if (aSlash!=std::string::npos)
        this->mRemotePath=aURL.substr(aSlash);
    else
        this->mRemotePath="/";
}
// FUNCTION: WON 0x006d4370
// Finishes with success, or with the HTTP error status above 304
void HTTPGetOp::DoFinish()
{
    if (this->mHTTPStatus>304) this->Finish(100093);
    else this->Finish(0);
}
// FUNCTION: WON 0x006d4390
// Sets the proxy as host:port
void HTTPGetOp::SetProxy(const std::string& hostAndPort)
{
    this->mProxyHost=hostAndPort;
}
// FUNCTION: WON 0x006d44e0
// Sets the progress completion called every given number of received bytes
void HTTPGetOp::SetRecvChunkCompletion(CompletionBase<AsyncOpPtr>* completion,DWORD chunkSize)
{
    this->mRecvChunkCompletion=completion;
    this->mRecvChunkSize=chunkSize;
}
// FUNCTION: WON 0x006d4550
// Returns true for success, otherwise finishes the op with the status
bool HTTPGetOp::CheckStatus(WONStatus inStatus)
{
    if (inStatus==0) return true;
    this->Finish(inStatus);
    return false;
}
// FUNCTION: WON 0x006d4570
// Parses the HTTP status line; finishes with an error when malformed, or for an error status on a closing connection
bool HTTPGetOp::ExtractStatus(const char* inStatusLine)
{
    char protocol[50]; //the original reserves 52 bytes here; the size 50 is inferred
    if (sscanf(inStatusLine,"%s%d",protocol,&this->mHTTPStatus)!=2) {
        this->Finish(100092);
        return false;
    }
    if (this->mHTTPStatus>304 && this->mCloseConnection) {
        this->Finish(100093);
        return false;
    }
    return true;
}
// FUNCTION: WON 0x006d45f0
// Splits a Location URL into the redirect host and path; finishes with an error when it has no scheme
bool HTTPGetOp::ExtractRedirect(const char* inLocation)
{
    const char* host=strstr(inLocation,"://");
    if (host==NULL) {
        this->Finish(100094);
        return false;
    }
    host+=3;
    const char* path=strchr(host,'/');
    if (path==NULL) {
        this->mRedirectHost=host;
        this->mRedirectPath='/';
    } else {
        this->mRedirectHost.assign(host,path-host);
        this->mRedirectPath=path;
    }
    return true;
}
// FUNCTION: WON 0x006d47e0
// Records a known reply header (length, type, dates, location, chunked, close, ETag); unknown ones are ignored
bool HTTPGetOp::ExtractHeaderLine(const char *theHeaderLine)
{
	const char *aColon = strchr(theHeaderLine,':');
	if(aColon==NULL)
		return true;

	const int NUM_KEYS = 9;
	static const char* keys[NUM_KEYS] = 
	{
		"Content-Length",
		"Content-Type",
		"Last-Modified",
		"Location",
		"Transfer-Encoding",
		"Connection",
		"Date",
		"Expires",
		"ETag"
	};

	int i;
	for(i=0; i<NUM_KEYS; i++)
	{
		const char *aPtr = theHeaderLine;
		int aPos = 0;
		bool match = true;
		while(aPtr!=aColon)
		{
			if(toupper(*aPtr)!=toupper(keys[i][aPos]))
			{
				match = false;
				break;
			}

			aPtr++;
			aPos++;
		}

		if(match)
			break;
	}

	const char *aVal = aColon+1;
	while(*aVal==' ')
		aVal++;

	switch(i)
	{
		case 0: this->mContentLength = atoi(aVal); break;
		case 1: this->mContentType.assign(aVal); break;
		case 2: this->mLastModified = ParseInternetDateTime(aVal); break;
		case 3: return this->ExtractRedirect(aVal); 
		case 4: 
		{
			string aStr = aVal;
			StringToUpperCaseInPlace(aStr);
			if(aStr.find("CHUNKED")==0)
			{
				this->mContentIsChunked = true;
				this->mIsFirstChunk = true;
			}
			break;
		}
		case 5: 
		{
			string aStr = aVal;
			StringToUpperCaseInPlace(aStr);
			if(aStr.find("CLOSE")==0)
				this->mDoClose = true;

			break;
		}
		case 6: this->mServerTime = ParseInternetDateTime(aVal); break;
		case 7: this->mExpireTime = ParseInternetDateTime(aVal); break;

		case 8:
		{
			this->mEntityTag = aVal;
			int anEndQuotePos = this->mEntityTag.find('"',1);
			if(anEndQuotePos==string::npos)
				this->mEntityTag.erase();
			else
				this->mEntityTag.resize(anEndQuotePos+1);
		}
		break;
	}

	return true;
}
// FUNCTION: WON 0x006d5090
// Connects the socket to the proxy or host on port 80 unless connected; returns true when the request can be sent
bool HTTPGetOp::DoConnect()
{
    if (this->mSocket->IsConnected())
        return true;
    IPAddr anAddress;
    if (!this->mProxyHost.empty())
        anAddress.SetWithDefaultPort(this->mProxyHost,80);
    else
        anAddress.SetWithDefaultPort(this->mHost,80);
    if (this->mRunMode==1) {
        this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(
            new ConnectOp(anAddress),HTTP_Track_Connect)),-1);
        return false;
    }
    else
        return this->CheckStatus(this->mSocket->Connect(anAddress,this->TimeLeft()));
}
// FUNCTION: WON 0x006d5270
// Builds and sends the GET with Host, Connection and resume or cache-validation headers
bool HTTPGetOp::SendRequest()
{
    WriteBuffer aRequest(0,true);
    char aRangeOffset[20];
    aRequest.AppendString("GET ",0);
    if (this->mProxyHost.length()!=0)
        aRequest.AppendString("http://"+this->mHost,0);
    aRequest.AppendString(this->mRemotePath+" HTTP/1.1\r\n",0);
    aRequest.AppendString("Host: "+this->mHost+"\r\n",0);
    if (this->mCloseConnection)
        aRequest.AppendString("Connection: close\r\n",0);
    else
        aRequest.AppendString("Connection: Keep-Alive\r\n",0);
    if (!this->mLocalPath.empty()) {
        WONFile aStat(this->mLocalPath);
        if (aStat.Exists()) {
            this->mLocalFileSize=aStat.GetSize();
            this->mLocalFileModifyTime=aStat.GetModifyTime();
            if (this->mLocalFileSize>0) {
                if (this->mDoRangeResume) {
                    if (!this->mEntityTag.empty())
                        aRequest.AppendString("If-Range: "+this->mEntityTag+"\r\n",0);
                    else
                        aRequest.AppendString("If-Range: "+MakeInternetDateTime(this->mLocalFileModifyTime)+"\r\n",0);
                    aRequest.AppendString("Range: bytes=",0);
                    snprintf(aRangeOffset,10,"%d",static_cast<int>(this->mLocalFileSize));
                    aRequest.AppendString(aRangeOffset,0);
                    aRequest.AppendString("-\r\n",0);
                }
                else if (this->mHTTPCacheEntry.get()==NULL || this->mHTTPCacheEntry->IsValid()) {
                    if (!this->mEntityTag.empty())
                        aRequest.AppendString("If-None-Match: "+this->mEntityTag+"\r\n",0);
                    aRequest.AppendString("If-Modified-Since: "+MakeInternetDateTime(this->mLocalFileModifyTime)+"\r\n",0);
                }
            }
        }
    }
    aRequest.AppendString("\r\n",0);
    if (this->IsAsync()) {

        this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(
            new SendBytesOp(aRequest.ToByteBuffer(true).get()),HTTP_Track_SendRequest)),-1);
        if (!this->GetOnlySend())
            this->AsyncRecvCRMsg(HTTP_Track_RecvStatus);
        return false;
    }
    else {
        this->mSocket->SendBytes(aRequest.ToByteBuffer(true).get(),this->TimeLeft());
    }
    return true;
}
// FUNCTION: WON 0x006d6170
// Queues the receive of the next reply line, tracked as the given step
void HTTPGetOp::AsyncRecvCRMsg(TrackType inType)
{
    this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new RecvCRMsgOp,inType)),-1);
}
// FUNCTION: WON 0x006d6240
// Parses a hex chunk length; on the last chunk stores the content and reads the footer; true to read the chunk
bool HTTPGetOp::ExtractChunkLength(const char* inLine)
{
    if (sscanf(inLine,"%x",&this->mChunkLength)==1) {
        this->mContentLength+=this->mChunkLength;
        if (this->mChunkLength==0) {
            if (this->mHTTPCacheEntry.get()!=NULL)
                this->mHTTPCacheEntry->SetValid(true);
            if (this->mLocalPath.empty())
                this->mContent=this->mContentAccumulator.ToByteBuffer(true);
            if (this->mRunMode==1) {
                this->AsyncRecvCRMsg(HTTP_Track_RecvFooter);
                return false;
            }
            while (true) {
                ByteBufferPtr aFooter;
                if (!this->CheckStatus(this->mSocket->RecvCRMsg(aFooter,this->TimeLeft())))
                    return false;
                if (aFooter.get()!=NULL && aFooter->length()==1)
                    break;
            }
            this->DoFinish();
            return false;
        }
        return true;
    }
    this->Finish(100092);
    return false;
}
// FUNCTION: WON 0x006d6430
// Reads the CRLF after a chunk and the next chunk length line
bool HTTPGetOp::RecvChunkLength()
{
    if (this->mIsFirstChunk)
        this->mContentLength=0;
    if (this->mRunMode==1) {
        if (!this->mIsFirstChunk)
            this->AsyncRecvCRMsg(HTTP_Track_RecvChunkCRLF);
        this->AsyncRecvCRMsg(HTTP_Track_RecvChunkLength);
        return false;
    }
    ByteBufferPtr aLine;
    if (!this->mIsFirstChunk) {
        if (!this->CheckStatus(this->mSocket->RecvCRMsg(aLine,this->TimeLeft())))
            return false;
    }
    if (!this->CheckStatus(this->mSocket->RecvCRMsg(aLine,this->TimeLeft())))
        return false;
    return this->ExtractChunkLength(aLine->data());
}
// FUNCTION: WON 0x006d65c0
// Stores received content (or a chunk), then reads the next chunk or finishes; true to keep receiving
bool HTTPGetOp::DoneRecvContent(RecvBytesOp* inOp)
{
    this->mIsFirstChunk=false;
    if (this->mLocalPath.empty() && inOp->GetStatus()==0) {
        if (this->mContentIsChunked)
            this->mContentAccumulator.AppendBuffer(inOp->GetBytes().get(),0);
        else
            this->mContent=inOp->GetBytes();
    }
    if (inOp->GetStatus()!=0) {
        this->Finish(inOp->GetStatus());
        return false;
    }
    if (this->mContentIsChunked)
        return this->RecvChunkLength();
    if (this->mHTTPCacheEntry.get()!=NULL)
        this->mHTTPCacheEntry->SetValid(true);
    this->DoFinish();
    return false;
}
// FUNCTION: WON 0x006d6730
// Receives the body to memory or the local file (appending for resumes), skipping error bodies; 304 is not new
void HTTPGetOp::RecvContent()
{
    if (this->mHTTPStatus==304) {
        this->mContentIsNew=false;
        this->DoFinish();
        return;
    }
    bool skip;
    if (this->mHTTPStatus>=200 && this->mHTTPStatus<300) skip=false;
    else {
        skip=true;
        if (this->mDoClose) {
            this->Finish(100093);
            return;
        }
    }
    for (;;) {
        unsigned long length=this->mContentLength;
        if (this->mContentIsChunked) length=this->mChunkLength;
        SmartPtr<RecvBytesOp> recv=new RecvBytesOp(length,this->mSocket.get());
        if (skip) recv->SetSkipBytes(true);
        else {
            if (this->mRecvChunkCompletion.get()!=NULL)
                recv->SetProgressCompletion(this->mRecvChunkCompletion.get(),this->mRecvChunkSize);
            if (this->mHTTPCacheEntry.get()!=NULL) this->mHTTPCacheEntry->SetValid(false);
            if (!this->mLocalPath.empty()) {
                bool append=this->mHTTPStatus==206 || (this->mContentIsChunked && !this->mIsFirstChunk);
                if (!append && this->mLocalFileSize>0 && this->mLocalFileSize==this->mContentLength &&
                    abs(this->mLocalFileModifyTime-this->mLastModified)<=1 && this->mDoClose) {
                    this->DoFinish();
                    this->mContentIsNew=false;
                    return;
                }
                recv->SetBinaryFile(this->mBinaryFile);
                recv->SetFilePath(this->mLocalPath);
                recv->SetAppendFile(append);
                recv->SetFileModifyTime(this->mLastModified);
            }
        }
        if (this->mRunMode==1) {
            this->Track(recv.get(),HTTP_Track_RecvContent);
            recv->RunAsync(-1);
            return;
        }
        if (recv->RunBlock(this->TimeLeft())) {
            if (!this->DoneRecvContent(recv.get())) return;
        } else {
            this->Finish(recv->GetStatus());
            return;
        }
    }
}
// FUNCTION: WON 0x006d6a50
// Advances the async request state machine as each connect, send or receive step finishes
bool HTTPGetOp::CallbackHook(AsyncOp* inOp,int inParam)
{
    if (inOp->mKilled) {
        this->Kill(true);
        return true;
    }
    if (inOp->mStatus!=0) {
        this->Finish(inOp->mStatus);
        return true;
    }
    switch (inParam) {
    case HTTP_Track_Connect:
        this->SendRequest();
        break;
    case HTTP_Track_SendRequest:
        if (this->mOnlySend) this->Finish(100097);
        break;
    case HTTP_Track_RecvStatus: {
        RecvCRMsgOp* op=static_cast<RecvCRMsgOp*>(inOp);
        ByteBufferPtr line=op->GetMsg();
        if (!this->ExtractStatus(line->data())) break;
        this->AsyncRecvCRMsg(HTTP_Track_RecvHeader);
        break;
    }
    case HTTP_Track_RecvHeader: {
        RecvCRMsgOp* op=static_cast<RecvCRMsgOp*>(inOp);
        ByteBufferPtr line=op->GetMsg();
        if (line->length()==1) {
            this->DoneRecvHeader();
            break;
        }
        if (!this->ExtractHeaderLine(line->data())) break;
        this->AsyncRecvCRMsg(HTTP_Track_RecvHeader);
        break;
    }
    case HTTP_Track_RecvFooter: {
        RecvCRMsgOp* op=static_cast<RecvCRMsgOp*>(inOp);
        ByteBufferPtr line=op->GetMsg();
        if (line->length()==1) this->DoFinish();
        else this->AsyncRecvCRMsg(HTTP_Track_RecvFooter);
        break;
    }
    case HTTP_Track_RecvContent:
        this->DoneRecvContent(static_cast<RecvBytesOp*>(inOp));
        break;
    case HTTP_Track_Redirect:
        this->FinishRedirect(static_cast<HTTPGetOp*>(inOp));
        break;
    case HTTP_Track_RecvChunkLength: {
        RecvCRMsgOp* op=static_cast<RecvCRMsgOp*>(inOp);
        ByteBufferPtr line=op->GetMsg();
        if (this->ExtractChunkLength(line->data())) this->RecvContent();
        break;
    }
    default:
        return false;
    }
    return true;
}
// FUNCTION: WON 0x006d6d90
// Takes over the results of the redirected request and finishes with its status
void HTTPGetOp::FinishRedirect(HTTPGetOp* inOp)
{
    this->mLastModified=inOp->mLastModified;
    this->mContentType=inOp->mContentType;
    this->mContentLength=inOp->mContentLength;
    this->mContent=inOp->mContent;
    this->mLocalPath=inOp->mLocalPath;
    if (!inOp->mRedirectHost.empty()) {
        this->mRedirectHost=inOp->mRedirectHost;
        this->mRedirectPath=inOp->mRedirectPath;
    }
    this->mHTTPStatus=inOp->mHTTPStatus;
    this->mContentIsNew=inOp->mContentIsNew;
    this->mNumRedirects=inOp->mNumRedirects;
    this->Finish(inOp->GetStatus());
}
// FUNCTION: WON 0x006d6f80
// Drops the connection and repeats the request at the redirect target (at most 5 times)
void HTTPGetOp::FollowRedirect()
{
    this->mTracker.KillAll();
    if (this->mSocket.get()!=NULL) {
        this->mSocket->Kill();
        this->mSocket=NULL;
    }
    if (this->mNumRedirects>5) {
        this->Finish(100095);
        return;
    }
    if (!this->mAutoRedirect) {
        this->Finish(100096);
        return;
    }
    SmartPtr<HTTPGetOp> anOp=new HTTPGetOp(this->mRedirectHost,this->mRedirectPath);
    anOp->SetBinaryFile(this->mBinaryFile);
    anOp->SetLocalPath(this->mLocalPath);
    anOp->SetHTTPCache(this->mHTTPCache);
    anOp->mNumRedirects=this->mNumRedirects+1;
    if (this->mRecvChunkCompletion.get()!=NULL)
        anOp->SetRecvChunkCompletion(this->mRecvChunkCompletion.get(),this->mRecvChunkSize);
    if (this->mRunMode==1) {
        this->Track(anOp.get(),HTTP_Track_Redirect);
        anOp->RunAsync(-1);
        return;
    }
    anOp->RunBlock(this->TimeLeft());
    this->FinishRedirect(anOp.get());
}
// FUNCTION: WON 0x006d7140
// After the headers: follows a 301/302, updates the cache entry's expiry and ETag, then reads the body
void HTTPGetOp::DoneRecvHeader()
{
    if (this->mHTTPStatus==301 || this->mHTTPStatus==302) {
        this->FollowRedirect();
        return;
    }
    HTTPCacheEntry* anEntry=this->mHTTPCacheEntry.get();
    if (anEntry!=NULL) {
        if (this->mServerTime>0 && this->mExpireTime>0 && this->mExpireTime>this->mServerTime)
            anEntry->SetExpirationTime(time(NULL)+this->mExpireTime-this->mServerTime);
        if (!this->mEntityTag.empty())
            this->mHTTPCacheEntry->SetEntityTag(this->mEntityTag);
    }
    if (this->mContentIsChunked && !this->RecvChunkLength())
        return;
    this->RecvContent();
}
// FUNCTION: WON 0x006d7330
// Starts the request: resets the results, answers from a fresh cache entry, else connects, sends and reads the reply
void HTTPGetOp::RunHook()
{
    if (!this->mOnlyRecv) {
        if (!HTTPGetOp::mStaticProxyHost.empty())
            this->SetProxy(HTTPGetOp::mStaticProxyHost);
        this->mLastModified=0;
        this->mServerTime=0;
        this->mExpireTime=0;
        this->mContentType.erase();
        this->mContentLength=0;
        this->mContent=NULL;
        this->mDoClose=this->mCloseConnection;
        this->mHTTPStatus=0;
        this->mLocalFileModifyTime=0;
        this->mLocalFileSize=0;
        this->mNumRedirects=0;
        this->mContentIsNew=true;
        this->mChunkLength=0;
        this->mContentIsChunked=false;
        this->mIsFirstChunk=false;
        this->mHTTPCacheEntry=NULL;
        this->mContentAccumulator.Reset();
        if (this->mSocket.get()==NULL)
            this->mSocket=new BlockingSocket(0);
        this->mRedirectHost="";
        this->mRedirectPath="";
        if (this->mHTTPCache.get()!=NULL) {
            this->mHTTPCacheEntry=this->mHTTPCache->GetEntry(this->mHost+this->mRemotePath);
            if (this->mHTTPCacheEntry.get()!=NULL) {
                this->mLocalPath=this->mHTTPCacheEntry->GetLocalPath();
                this->mEntityTag=this->mHTTPCacheEntry->GetEntityTag();
                if (this->mHTTPCacheEntry->IsValid() &&
                    this->mHTTPCacheEntry->GetExpirationTime()>time(NULL)) {
                    this->mContentIsNew=false;
                    this->Finish(0);
                    return;
                }
            }
        }
        if (!this->DoConnect())
            return;
        if (!this->SendRequest())
            return;
    }
    if (this->mOnlySend) {
        this->Finish(100097);
        return;
    }
    if (this->mOnlyRecv && this->mRunMode==1) {
        this->AsyncRecvCRMsg(HTTP_Track_RecvStatus);
        return;
    }
    ByteBufferPtr aLine;
    if (!this->CheckStatus(this->mSocket->RecvCRMsg(aLine,this->TimeLeft())))
        return;
    if (!this->ExtractStatus(aLine->data()))
        return;
    while (true) {
        if (!this->CheckStatus(this->mSocket->RecvCRMsg(aLine,this->TimeLeft())))
            return;
        if (aLine->length()==1)
            break;
        if (!this->ExtractHeaderLine(aLine->data()))
            return;
    }
    this->DoneRecvHeader();
}
// FUNCTION: WON 0x006d7770
// Closes the socket unless it stays open for keep-alive and updates the cache entry
void HTTPGetOp::CleanupHook()
{
    AsyncOpWithTracker::CleanupHook();
    if (this->mSocket.get()!=NULL) {
        if ((this->mDoClose && !this->mOnlySend) || this->mSocket->mStatus!=100004) {
            this->mSocket->Close();
            this->mSocket=NULL;
        }
    }
    if (!this->mOnlySend && this->mHTTPCache.get()!=NULL && this->mHTTPCacheEntry.get()!=NULL)
        this->mHTTPCache->UpdateEntry(this->mHTTPCacheEntry.get());
}
// FUNCTION: WON 0x006d77f0
// Sets the socket to reuse for the request
void HTTPGetOp::SetSocket(BlockingSocket* socket) { this->mSocket=socket; }
// FUNCTION: WON 0x006d7850
// Returns the socket when it is still connected, else NULL
BlockingSocket* HTTPGetOp::GetSocket()
{
    if(this->mSocket.get()==NULL) return NULL;
    if(!this->mSocket->IsConnected()) return NULL;
    return this->mSocket.get();
}
// FUNCTION: WON 0x006d7870
// Returns the content's last-modified time, falling back to the local file's time
 time_t HTTPGetOp::GetLastModifiedTime()
{
    if (this->mLastModified!=0)
        return this->mLastModified;
    if (this->mLocalPath.empty()) return 0;
    WONFile file(this->mLocalPath);
    return this->mLastModified=file.GetModifyTime();
    return this->mLastModified;
}
}

// No original address identified: WON SDK proxy settings
namespace WONAPI {
bool HTTPGetOp::WriteProxyHostFile(const std::string &theHostAndPort)
{
	AutoCrit aCrit(GetStaticCrit());
	HTTPGetOp::mStaticProxyHost = theHostAndPort;

	if(theHostAndPort.empty())
		return PlatDeleteFile(GetProxyHostFileName().c_str())!=0;

	FILE *aFile = PlatFOpen(GetProxyHostFileName().c_str(),"w");
	if(aFile==NULL)
		return false;

	fprintf(aFile,"%s\n",theHostAndPort.c_str());
	fclose(aFile);
	return true;
}

const std::string& HTTPGetOp::GetStaticProxyHost()
{
	HTTPGetOp::ReadProxyHostFile(false);
	return HTTPGetOp::mStaticProxyHost;
}

void HTTPGetOp::SetProxyHostFileName(std::string &theFileName) // set file name and path for proxy info file
{
	AutoCrit aCrit(GetStaticCrit());
	GetProxyHostFileName() = theFileName;
}

void HTTPGetOp::SetProxy(const std::string &theHost, unsigned short thePort)
{
	char aBuf[50];
	sprintf(aBuf,":%d",thePort);
	this->mProxyHost = theHost + aBuf;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d3a70
// (_$E77)
// TEMPLATE: WON 0x006d3b00
// (_$E79)

// TEMPLATE: WON 0x006d3d30
// (HTTPGetOp scalar deleting destructor)
// TEMPLATE: WON 0x006d3d50
// (HTTPGetOp implicit destructor)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d39c0
// (class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > & __cdecl GetProxyHostFileName(void)    [?GetProxyHostFileName)
// TEMPLATE: WON 0x006d3ac0
// (class WONAPI::CriticalSection & __cdecl GetStaticCrit(void)    [?GetStaticCrit@@YAAAVCriticalSection@WONAPI@@XZ])

// TEMPLATE: WON 0x006cfe50
// (AsyncOpTracker no-argument constructor: the inline refused here, kept out of line)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d9c70
// (public: void * __thiscall WONAPI::AsyncOpTrackerEntry::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006d3790
// (_$E75)
// TEMPLATE: WON 0x006d37a0
// (_$E71)
// TEMPLATE: WON 0x006d37c0
// (_$E74)
// TEMPLATE: WON 0x006d37d0
// (_$E72)
