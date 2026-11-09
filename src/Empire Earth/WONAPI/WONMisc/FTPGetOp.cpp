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

	string aStr = theURL;
	int aPos = aStr.find("://");
	if(aPos!=string::npos)
		aStr = aStr.substr(aPos+3);

	if(doExtractUserAndPassword)
		this->ExtractUserNameAndPasswordFromURL(aStr);

	aPos = aStr.find('/');
	this->mHost = aStr.substr(0,aPos);
	if(aPos!=string::npos)
		this->SetRemotePath(aStr.substr(aPos+1));
}

///////////////////////////////////////////////////////////////////////////////
// The ftp format string is:
//   ftp://username:password@ftpserver/url-path

// At this point, we have already stripped the 'ftp://', so now we look to see
// if there is a 'username' and a 'password'.
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::ExtractUserNameAndPasswordFromURL(std::string &theURL)
{
	// Do we have UserName?
	int aColenPos = theURL.find(':'); // Follows the user name.
	if(aColenPos!=string::npos)
	{
		this->SetUserName(theURL.substr(0,aColenPos));
		theURL = theURL.substr(aColenPos+1);
	}

	// Do we have Password?
	int aSlashPos = theURL.find('/');         // Follows the password.
	int aAtPos = theURL.rfind('@', aSlashPos); // Preceeds the slash.
	if(aAtPos!=string::npos)
	{
		this->SetPassword(theURL.substr(0,aAtPos));
		theURL = theURL.substr(aAtPos+1);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FTPGetOp::SetRemotePath(const std::string &thePath)
{
	this->mRemotePath = thePath;
	int aSlashPos = thePath.find_last_of('/');
	if(aSlashPos==string::npos)
	{
		this->mActualPath = "";
		this->mActualFile = thePath;
	}
	else
	{
		this->mActualPath = thePath.substr(0,aSlashPos);
		this->mActualFile = thePath.substr(aSlashPos+1);
	}
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
	if(sscanf(theStatusLine,"%d",&this->mFTPStatus)!=1)
	{
		this->Finish(WS_FTP_InvalidResponse);
		return false;
	}

	if(this->mFTPStatus>=200 && this->mFTPStatus<300)
		return true;
	
	if(this->mFTPStatus==331) // just need password
		return true;

	if(this->mFTPStatus==125 || this->mFTPStatus==150) // transfer starting
		return true;

	if(this->mFTPStatus==350)	// requested file action pending further action (restart)
		return true;

	if(this->mFTPStatus==504) // not implemented (should only be for resume)
		return true;


	this->Finish(WS_FTP_StatusError);
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::DoConnect()
{
	// Connect
	IPAddr anAddr;
	anAddr.SetWithDefaultPort(this->mHost,21);
	if((this->mRunMode==OP_MODE_ASYNC))
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
	std::string aReq;
	switch(this->mRequestState)
	{
		case RequestState_User: aReq = "USER " + this->mUserName; break;
		case RequestState_Password: aReq = "PASS " + this->mPassword; break;
		case RequestState_ChangeDir: aReq = "CWD " + this->mActualPath; break;
		case RequestState_ChangeType: aReq = string("TYPE ") + (this->mIsBinary?'I':'A'); break;
		case RequestState_Passive: aReq = "PASV"; break;
		
		case RequestState_Restart: 
		{
			char aBuf[50];
			sprintf(aBuf,"REST %d",this->mLocalFileSize);
			aReq = aBuf;
		}	
		break;

		case RequestState_Retrieve: aReq = "RETR " + this->mActualFile; break;
	}
	aReq+="\r\n";

	if((this->mRunMode==OP_MODE_ASYNC))
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
	const char *aBufP = strchr(theStatusLine,'(');
	if(aBufP==NULL)
	{
		this->Finish(WS_FTP_InvalidPasvResponse);
		return false;
	}

	int aNum[6], aResult;
	aResult = sscanf(aBufP,"(%d,%d,%d,%d,%d,%d)",aNum,aNum+1,aNum+2,aNum+3,aNum+4,aNum+5);
	if(aResult!=6)
	{
		this->Finish(WS_FTP_InvalidPasvResponse);
		return false;
	}

	long aHost = (aNum[0]<<24) | (aNum[1]<<16) | (aNum[2]<<8) | aNum[3];
	unsigned short aPort = (aNum[4]<<8) | aNum[5];
	this->mDataAddr.Fn2B2C20(aHost,aPort);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::HandleReply(RecvCRMsgOp *theReply)
{
	if(!this->CheckStatus(theReply->GetStatus()))
		return false;

	const char *aLine = theReply->GetMsg()->data();
	int aLen = theReply->GetMsg()->length();
	if(aLen>=4 && aLine[3]=='-') // still more reply lines
	{
		if((this->mRunMode==OP_MODE_ASYNC))
			this->RecvLineAsync();
		
		return true;
	}

	if(!this->ExtractStatus(aLine))
		return false;

	switch(this->mRequestState)
	{
		case RequestState_Start:
			this->mRequestState = RequestState_User;
			return this->SendRequest();
		
		case RequestState_User:
			if(this->mFTPStatus==331)
			{
				this->mRequestState = RequestState_Password;			
				return this->SendRequest();
			}
			// fall through, don't need password

		case RequestState_Password:
			if(!this->mActualPath.empty())
			{
				this->mRequestState = RequestState_ChangeDir;
				return this->SendRequest();
			}
			// fall through, don't need to change dir

		case RequestState_ChangeDir:
			this->mRequestState = RequestState_ChangeType;
			return this->SendRequest();

		case RequestState_ChangeType:
			this->mRequestState = RequestState_Passive;
			return this->SendRequest();

		case RequestState_Passive:
			if(!this->ParsePasv(aLine))
				return false;

			if(this->mDoResume)
				this->mRequestState = RequestState_Restart;
			else
				this->mRequestState = RequestState_Retrieve;

			if(!this->DoDataConnection())
				return false;

			return this->SendRequest();
			
		case RequestState_Restart:
			if(this->mFTPStatus==504) // resume not implemented
				this->mDoResume = false;

			this->mRequestState = RequestState_Retrieve;
			return this->SendRequest();

		case RequestState_Retrieve:
			this->RecvContent();
			return false;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FTPGetOp::DoDataConnection()
{
	if((this->mRunMode==OP_MODE_ASYNC))
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

	if((this->mRunMode==OP_MODE_ASYNC))
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
	if(theOp->mKilled) 
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
