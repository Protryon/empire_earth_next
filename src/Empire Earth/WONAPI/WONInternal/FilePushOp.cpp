// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK file push operation
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONFilePush.h"
#include "ee/wonapi/WONStatus.h"



using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FilePushOp::FilePushOp(const IPAddr &theAddr) :
	mConnectAddr(theAddr), mIsCompress(false), mBlockTransferSize(65535)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
FilePushOp::~FilePushOp()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::CallbackHook(AsyncOp *theOp, int theId)
{
	if(theOp->GetStatus()!=WS_Success)
	{
		this->Finish(theOp->GetStatus());
		return true;
	}

	switch(theId)
	{
		case TRACK_AUTH: 
			if(this->GetEncryptKey(static_cast<PeerAuthOp*>(theOp)))
				this->SendStreamFileRequest(); 
			return true;
	
		case TRACK_STATUS_REPLY: 
		{
			RecvMsgOp *anOp = static_cast<RecvMsgOp*>(theOp);
			this->HandleFactStatusReply(anOp->GetMsg());
			return true;
		}

		case TRACK_ACK:
		{
			RecvBytesOp *anOp = static_cast<RecvBytesOp*>(theOp);
			this->HandleAck(anOp->GetBytes()->data()[0]);
			return true;
		}

	}
	
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::CheckError(WONStatus theStatus)
{
	if(theStatus!=(WONStatus)0)
	{
		this->Finish(theStatus);
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::GetEncryptKey(PeerAuthOp *theOp)
{
	AuthSession *aSession = theOp->GetSession();
	if(aSession==NULL)
	{
		this->Finish((WONStatus)WS_AuthRequired);
		return false;
	}

	this->mEncryptKey = aSession->GetKey();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FilePushOp::DoProgressCompletion()
{
	if(this->mProgressCompletion.get()!=NULL)
		this->mProgressCompletion->Complete(this);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::HandleFactStatusReply(const ByteBuffer *theReply)
{
	ReadBuffer aReply(theReply->data(), theReply->length());
	WONStatus aStatus = (WONStatus)100004;
	try
	{
		unsigned char aHeaderType = aReply.ReadByte();
		unsigned short aServiceType = aReply.ReadShort();
		unsigned short aMessageType = aReply.ReadShort();
		aStatus = (WONStatus)aReply.ReadShort();
		if(this->CheckError(aStatus))
			return false;
	}
	catch(ReadBufferException&)
	{
		this->Finish((WONStatus)WS_MessageUnpackFailure);
		return false;
	}

	if(this->mTotalBytesSent > 0) // this is the second status reply so we're done
	{
		this->DoProgressCompletion();
		this->Finish((WONStatus)WS_Success);
		return false;
	}
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::RecvFactStatusReply()
{
	if((this->mRunMode==OP_MODE_ASYNC))
	{
		this->RecvMsgAsync(TRACK_STATUS_REPLY);
		return false;
	}

	ByteBufferPtr aReply;
	WONStatus aStatus = this->mSocket->RecvMsg(aReply, this->RecvTime());
	if(this->CheckError(aStatus))
		return false;

	return this->HandleFactStatusReply(aReply);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::HandleAck(char theAck)
{
	if(theAck==1)
	{
		this->DoProgressCompletion();
		if((this->mRunMode==OP_MODE_ASYNC))
			this->SendFileChunk();

		return true;
	}

	return this->RecvFactStatusReply();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::RecvAck()
{
	if((this->mRunMode==OP_MODE_ASYNC))
	{
		SmartPtr<RecvBytesOp> anOp = new RecvBytesOp(1,this->mSocket);
		this->Track(anOp, TRACK_ACK);
		this->mSocket->QueueOp(anOp,-1);
		return false;
	}

	ByteBufferPtr aByte;
	WONStatus aStatus = this->mSocket->RecvBytes(aByte, 1,0xffffffff);
	if(this->CheckError(aStatus))
		return false;

	return this->HandleAck(aByte->data()[0]);

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::SendFileChunk()
{		
	WriteBuffer aBuf(4,false);
	aBuf.Reserve(this->mBlockTransferSize + 20);

	int aLen = fread(aBuf.data(),1,this->mBlockTransferSize,this->mFile.GetFile());
	WONStatus aStatus = (WONStatus)100004;

	// Encrypt bytes and pack them in aBuf
	if(aLen > 0)
	{
		ByteBufferPtr anEncrypt = this->mEncryptKey.Encrypt(aBuf.data(),aLen);
		aBuf.AppendBytes(anEncrypt->data(), anEncrypt->length());
	}

	// Send bytes
	this->mLastBytesSent = aLen;
	this->mTotalBytesSent += aLen;
	if((this->mRunMode==OP_MODE_ASYNC))
		this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new SendBytesOp(aBuf.ToByteBuffer(true)),0)),-1);
	else
	{
		aStatus = this->mSocket->SendBytes(aBuf.ToByteBuffer(true), this->SendTime());
		if(this->CheckError(aStatus))
			return false;
	}

	if(aLen>0)
		return this->RecvAck();
	else 
		return this->RecvFactStatusReply();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool FilePushOp::SendStreamFileRequest()
{
	WriteBuffer aMsg(this->mLengthFieldSize,true);
	aMsg.AppendByte(5);					// Small Message
	aMsg.AppendShort(5);				// Small Factory Service
	aMsg.AppendShort(21);				// FactStreamFile Msg
	aMsg.AppendString(this->mRemoteFileName);
	aMsg.AppendBool(this->mIsCompress);
	aMsg.AppendLong(this->mModifyTime.dwHighDateTime);
	aMsg.AppendLong(this->mModifyTime.dwLowDateTime);
	aMsg.AppendString(this->mComment);

	if((this->mRunMode==OP_MODE_ASYNC))
	{
		this->SendMsgAsync(aMsg.ToByteBuffer(true),TRACK_STREAM_REQUEST);
		this->RecvFactStatusReply();
		this->RecvAck();
		return false;
	}

	WONStatus aStatus = this->mSocket->SendMsg(aMsg.ToByteBuffer(true), this->SendTime());
	if(this->CheckError(aStatus))
		return false;

	if(!this->RecvFactStatusReply())
		return false;

	return this->RecvAck();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FilePushOp::RunHook(void)
{
	this->mTotalBytesSent = 0;
	this->mLastBytesSent = 0;
	FILE *aFile = fopen(this->mFileName.c_str(),"rb");
	this->mFile.Attach(aFile);
	if(aFile==NULL)
	{
		this->Finish((WONStatus)WS_FailedToOpenFile);
		return;
	}

	this->mSocket = new BlockingSocket(AsyncSocket::TCP);
	this->mSocket->SetLengthFieldSize(this->mLengthFieldSize);

	SmartPtr<PeerAuthOp> aPeerAuthOp = new PeerAuthOp(this->mConnectAddr, this->mAuthContext, this->mAuthType, this->mSocket,true);
	aPeerAuthOp->CopyMaxTimes(this);
	if((this->mRunMode==OP_MODE_ASYNC))
	{
		this->Track(aPeerAuthOp,TRACK_AUTH);
		aPeerAuthOp->RunAsync(-1);
		return;
	}
	
	if(!aPeerAuthOp->RunBlock(this->TimeLeft()))
	{
		this->Finish(aPeerAuthOp->GetStatus());
		return;
	}

	if(!this->GetEncryptKey(aPeerAuthOp))
		return;

	if(!this->SendStreamFileRequest())
		return;

	while(true)
	{
		if(!this->SendFileChunk())
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FilePushOp::CleanupHook(void)
{
	ServerOp::CleanupHook();
	this->mFile.Detach();
}
