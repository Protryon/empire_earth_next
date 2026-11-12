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
#include "ee/wonapi/WONHttpMsg.h"
using WONAPI::CriticalSection;
// FUNCTION: WON 0x006d5b60
// Formats a time as an RFC 1123 HTTP date ("Sun, 06 Nov 1994 08:49:37 GMT"): ee_won http.rs
static string MakeInternetDateTime(time_t theTime)
{
    return WONAPI::HttpFormatDate(theTime);
}

// FUNCTION: WON 0x006d4c60
// Parses an HTTP date in RFC 1123, RFC 850 or asctime form; returns 0 on failure (time zone ignored): ee_won
// http.rs
static time_t ParseInternetDateTime(const char* str)
{
    return WONAPI::HttpParseDate(str);
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
    size_t aHost=0,anEnd=0;
    intptr_t aPath=-1;
    ee_won_http_split_url(reinterpret_cast<const uint8_t*>(inURL.data()),inURL.size(),&aHost,&anEnd,&aPath);
    this->mHost=inURL.substr(aHost,anEnd-aHost);
    if (aPath>=0)
        this->mRemotePath=inURL.substr(aPath);
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
    return HttpExtractStatus(*this,inStatusLine);
}
// FUNCTION: WON 0x006d45f0
// Splits a Location URL into the redirect host and path; finishes with an error when it has no scheme
bool HTTPGetOp::ExtractRedirect(const char* inLocation)
{
    return HttpExtractRedirect(*this,inLocation);
}
// FUNCTION: WON 0x006d47e0
// Records a known reply header (length, type, dates, location, chunked, close, ETag); unknown ones are ignored
bool HTTPGetOp::ExtractHeaderLine(const char *theHeaderLine)
{
    return HttpExtractHeaderLine(*this,theHeaderLine);
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
    if (this->GetMode()==1) {
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
    bool aFile=false;
    if (!this->mLocalPath.empty()) {
        WONFile aStat(this->mLocalPath);
        if (aStat.Exists()) {
            this->mLocalFileSize=aStat.GetSize();
            this->mLocalFileModifyTime=aStat.GetModifyTime();
            aFile=true;
        }
    }
    ByteBufferPtr aRequest=HttpRequestBytes(this->mProxyHost.length()!=0,this->mHost,this->mRemotePath,
                                            this->mCloseConnection,aFile,this->mLocalFileSize,
                                            this->mLocalFileModifyTime,this->mDoRangeResume,this->mEntityTag,
                                            this->mHTTPCacheEntry.get()==NULL || this->mHTTPCacheEntry->IsValid());
    if (this->IsAsync()) {

        this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(
            new SendBytesOp(aRequest.get()),HTTP_Track_SendRequest)),-1);
        if (!this->GetOnlySend())
            this->AsyncRecvCRMsg(HTTP_Track_RecvStatus);
        return false;
    }
    else {
        this->mSocket->SendBytes(aRequest.get(),this->TimeLeft());
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
    uint32_t aChunkLength=0;
    if (ee_won_http_chunk_length(reinterpret_cast<const uint8_t*>(inLine),&aChunkLength)) {
        this->mChunkLength=aChunkLength;
        this->mContentLength+=this->mChunkLength;
        if (this->mChunkLength==0) {
            if (this->mHTTPCacheEntry.get()!=NULL)
                this->mHTTPCacheEntry->SetValid(true);
            if (this->mLocalPath.empty())
                this->mContent=this->mContentAccumulator.ToByteBuffer(true);
            if (this->GetMode()==1) {
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
    if (this->GetMode()==1) {
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
                if (ee_won_http_up_to_date(append,this->mLocalFileSize,this->mContentLength,
                                           this->mLocalFileModifyTime,this->mLastModified,this->mDoClose)) {
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
        if (this->GetMode()==1) {
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
    if (inOp->IsKilled()) {
        this->Kill(true);
        return true;
    }
    if (inOp->GetStatus()!=0) {
        this->Finish(inOp->GetStatus());
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
    if (this->GetMode()==1) {
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
    if (this->mOnlyRecv && this->GetMode()==1) {
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
        if ((this->mDoClose && !this->mOnlySend) || this->mSocket->mCore.status!=100004) {
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
