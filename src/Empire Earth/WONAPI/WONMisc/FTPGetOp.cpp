// FTPGetOp: downloads a file over FTP (WON SDK)
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONFTP.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONFtpMsg.h"

using namespace WONAPI;
using namespace std;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::Init()
{
	this->mIsBinary = true;
	this->mDoResume = false;
	this->mUserName = "anonymous";
	this->mPassword = "anonymous";
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FTPGetOp::FTPGetOp(const std::string &theHost, const std::string &theRemotePath) 
	: mHost(theHost)
{
	this->Init();
	this->SetRemotePath(theRemotePath);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FTPGetOp::FTPGetOp(const std::string &theURL, bool doExtractUserAndPassword) 
{
	this->Init();
	FtpMsg<FTPGetOp>::FromURL(*this,theURL,doExtractUserAndPassword);
}

///////////////////////////////////////////////////////////////////////////////
// The ftp format string is:
//   ftp://username:password@ftpserver/url-path

// At this point, we have already stripped the 'ftp://', so now we look to see
// if there is a 'username' and a 'password'.
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::ExtractUserNameAndPasswordFromURL(std::string &theURL)
{
	FtpMsg<FTPGetOp>::ExtractUserNameAndPassword(*this,theURL);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::SetRemotePath(const std::string &thePath)
{
	FtpMsg<FTPGetOp>::SetRemotePath(*this,thePath);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::SetRecvChunkCompletion(CompletionBase<AsyncOpPtr> *theCompletion, DWORD theChunkSize)
{
	this->mRecvChunkCompletion = theCompletion;
	this->mRecvChunkSize = theChunkSize;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::CheckStatus(WONStatus theStatus)
{
	if(theStatus==0)
		return true;
	else
	{
		this->Finish(theStatus);
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::ExtractStatus(const char *theStatusLine)
{
	return FtpMsg<FTPGetOp>::ExtractStatus(*this,theStatusLine);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::DoConnect()
{
	// Connect
	IPAddr anAddr;
	anAddr.SetWithDefaultPort(this->mHost,21);
	if((this->GetMode()==OP_MODE_ASYNC))
	{
		this->mRequestSocket->QueueOp(static_cast<SocketOp*>(this->Track(new ConnectOp(anAddr),FTP_Track_RequestConnect)),-1);
		return false;
	}
	else
	{
		WONStatus aStatus = this->mRequestSocket->Connect(anAddr, this->TimeLeft());
		return this->CheckStatus(aStatus);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::RecvLineAsync()
{
	this->mRequestSocket->QueueOp(static_cast<SocketOp*>(this->Track(new RecvCRMsgOp,FTP_Track_Request)),-1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::SendRequest()
{
	std::string aReq = FtpMsg<FTPGetOp>::Command(*this);

	if((this->GetMode()==OP_MODE_ASYNC))
	{
		this->mRequestSocket->QueueOp(static_cast<SocketOp*>(this->Track(new SendBytesOp(new ByteBuffer(aReq.c_str(),aReq.length(),false)),0)),-1);
		this->RecvLineAsync();
		return false;
	}
	else	
	{
		WONStatus aStatus = this->mRequestSocket->SendBytes(new ByteBuffer(aReq.c_str(),aReq.length(),false),this->TimeLeft());
		return true;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::ParsePasv(const char *theStatusLine)
{
	return FtpMsg<FTPGetOp>::ParsePasv(*this,theStatusLine);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::HandleReply(RecvCRMsgOp *theReply)
{
	if(!this->CheckStatus(theReply->GetStatus()))
		return false;

	const char *aLine = theReply->GetMsg()->data();
	int aLen = theReply->GetMsg()->length();
	return FtpMsg<FTPGetOp>::HandleReply(*this,aLine,aLen);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::DoDataConnection()
{
	if((this->GetMode()==OP_MODE_ASYNC))
	{
		this->mDataSocket->QueueOp(static_cast<SocketOp*>(this->Track(new ConnectOp(this->mDataAddr),FTP_Track_DataConnect)),-1);
		return false;
	}
	else
	{
		WONStatus aStatus = this->mDataSocket->Connect(this->mDataAddr, this->TimeLeft());
		if(!this->CheckStatus(aStatus))
			return false;

		return true;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::RecvContent()
{
	SmartPtr<RecvBytesOp> aRecvBytes = new RecvBytesOp(0,this->mDataSocket);
	if(this->mRecvChunkCompletion.get()!=NULL)
		aRecvBytes->SetProgressCompletion(this->mRecvChunkCompletion.get(),this->mRecvChunkSize);

	if(!this->mLocalPath.empty())
	{
		aRecvBytes->SetFilePath(this->mLocalPath);
		aRecvBytes->SetAppendFile(this->mDoResume);

//		aRecvBytes->SetFileModifyTime(mLastModified);
	}	

	if((this->GetMode()==OP_MODE_ASYNC))
	{
		this->Track(aRecvBytes,FTP_Track_RecvContent);
		aRecvBytes->RunAsync(-1);
	}
	else
	{
		if(aRecvBytes->RunBlock(this->TimeLeft()))
		{
			if(this->mLocalPath.empty())
				this->mContent = aRecvBytes->GetBytes().get();
		}
		
		this->Finish(aRecvBytes->GetStatus());
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::CallbackHook(AsyncOp *theOp, int theParam)
{
	if(theOp->IsKilled()) 
	{
		this->Kill(true);
		return true;
	}

	if(!(theOp->GetStatus()==WS_Success))
	{
		this->Finish(theOp->GetStatus());
		return true;
	}

	switch(theParam) 
	{
		case FTP_Track_RequestConnect: 
			this->RecvLineAsync(); // receive initial success
			break;

		case FTP_Track_DataConnect:
			this->SendRequest(); // Send RETR command
			break;
		
		case FTP_Track_Request: 
		{
			RecvCRMsgOp *anOp = static_cast<RecvCRMsgOp*>(theOp);
			this->HandleReply(anOp);
			break;
		}
		
		case FTP_Track_RecvContent:
		{
			RecvBytesOp *anOp = static_cast<RecvBytesOp*>(theOp);
			if(this->mLocalPath.empty() && (anOp->GetStatus()==WS_Success))
				this->mContent = anOp->GetBytes().get();
			
			this->Finish(anOp->GetStatus());
			break;
		}

		default: return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::RunHook()
{
	this->mContent = NULL;
	this->mFTPStatus = 0;

	if(this->mDoResume) // determine local file size
	{
		struct stat fileInfo;
		if(stat(this->mLocalPath.c_str(), &fileInfo)==0)
			this->mLocalFileSize = fileInfo.st_size;
		else 
			this->mDoResume = false; // file doesn't exist --> don't resume
	}

	this->mRequestSocket = new BlockingSocket(0);
	this->mDataSocket = new BlockingSocket(0);

	this->mRequestState = RequestState_Start;

	if(!this->DoConnect())
		return;

	while(true)
	{
		SmartPtr<RecvCRMsgOp> aRecvOp = new RecvCRMsgOp(this->mRequestSocket);
		aRecvOp->RunBlock(this->TimeLeft());
		if(!this->HandleReply(aRecvOp))
			return;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::CleanupHook()
{
	AsyncOpWithTracker::CleanupHook();
	if(this->mRequestSocket.get()!=NULL)
	{
		this->mRequestSocket->Close();
		this->mRequestSocket = NULL;
	}

	if(this->mDataSocket.get()!=NULL)
	{
		this->mDataSocket->Close();
		this->mDataSocket = NULL;
	}
}
