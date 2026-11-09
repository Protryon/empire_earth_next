// PeerAuthOp: connects a socket to a peer and runs the client peer authentication, blocking or asynchronous
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
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006e6a10
// Creates a peer auth op for an address, auth context, auth type and socket
PeerAuthOp::PeerAuthOp(const IPAddr &theAddr, AuthContext *theAuthContext, AuthType theType, 
					   BlockingSocket *theSocket, bool useAuth2) : mConnectAddr(theAddr)
{
	this->mAuthContext = theAuthContext;
	this->mAuthType = theType;
	this->mSocket = theSocket;
	this->mPeerAuthClient.SetUseAuth2(useAuth2);
}

// FUNCTION: WON 0x006e6d10
// Destroys the op (members only)
PeerAuthOp::~PeerAuthOp()
{
}

// FUNCTION: WON 0x006e6ed0
// Advances the asynchronous handshake on socket and certificate-refresh completions; false for unknown ops
bool PeerAuthOp::CallbackHook(AsyncOp *theOp, int theParam)
{
	switch(theParam)
	{
		case PeerAuth_Track_Socket:
		{
			if(theOp->GetStatus()!=WS_Success)
			{
				this->Finish(theOp->GetStatus());
				break;
			}

			if(this->mAuthType==AUTH_TYPE_NONE) // just connecting
			{
				this->Finish(WS_Success);
				break;
			}

			RecvMsgOp *aRecvOp = dynamic_cast<RecvMsgOp*>(theOp);
			if(aRecvOp==NULL)
				break;

			ByteBufferPtr aSendMsg;
			ByteBufferPtr aRecvMsg = aRecvOp->GetMsg();
			WONStatus aStatus = this->mPeerAuthClient.HandleRecvMsg(aRecvMsg->data(),aRecvMsg->length(),aSendMsg);
			if(aStatus==(-4) || aStatus==(-19)) // expired certificate
			{
				this->mPeerAuthClient.Reset();
				switch(this->mGetCertStatus)
				{
					case WS_None: this->AsyncRefreshCert(); break;
					case WS_Pending: break;
					case WS_Success: this->AsyncRetry(); break;
					default: this->Finish(WS_PeerAuth_GetCertFailure);
				}	

				break;
			}
			else if(this->CheckStatus(aStatus))
				break;

			if(aSendMsg.get()==NULL) 
				this->Success(); 				// done
			else
				this->AsyncSend(aSendMsg.get()); 	// Need to send more stuff
		}
		break;


		case PeerAuth_Track_GetCert:
		{
			LightGetCertOp *aGetCert = static_cast<LightGetCertOp*>(theOp);
			this->mGetCertStatus = aGetCert->GetStatus();
			this->mPeerData = aGetCert->GetPeerData();
			if(this->mPeerAuthClient.GetState()==PeerAuthClient::STATE_NOT_STARTED)
			{
				if(this->mGetCertStatus!=WS_Success)
				{
					this->Finish(WS_PeerAuth_GetCertFailure);
					break;
				}
				else
					this->AsyncRetry();
			}
		}
		break;

		default:
			return false;
	}

	return true;
}



// FUNCTION: WON 0x006e71a0
// Reconnects and resends the auth request after a certificate refresh
void PeerAuthOp::AsyncRetry()
{
	this->mGetCertStatus = (-4); 
	this->AsyncConnect();
	this->AsyncSend(this->mPeerAuthClient.Start(this->mPeerData.get(),this->mAuthType,this->mLengthFieldSize).get());
}

// FUNCTION: WON 0x006e7240
// Queues a send of the message followed by a receive on the socket
void PeerAuthOp::AsyncSend(const ByteBuffer *theMsg)
{
	this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new SendMsgOp(theMsg),PeerAuth_Track_Socket)), this->SendTime());
	this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new RecvMsgOp,PeerAuth_Track_Socket)), this->RecvTime());
}

// FUNCTION: WON 0x006e7370
// Queues a connect to the peer address on the socket
void PeerAuthOp::AsyncConnect()
{
	this->mSocket->QueueOp(static_cast<SocketOp*>(this->Track(new ConnectOp(this->mConnectAddr),PeerAuth_Track_Socket)), this->ConnectTime());
}

// FUNCTION: WON 0x006e74a0
// Asks the auth context for a fresh certificate and tracks the request
void PeerAuthOp::AsyncRefreshCert()
{
	this->mGetCertStatus = WS_Pending;
	SmartPtr<LightGetCertOp> aGetCert = new LightGetCertOp;
	this->Track(aGetCert.get(),PeerAuth_Track_GetCert);
	this->mAuthContext->RefreshAsync(aGetCert.get());
}

// FUNCTION: WON 0x006e75f0
// Starts the asynchronous flow: refreshes a soon-expiring certificate, then connects and sends the request
void PeerAuthOp::AsyncRun()
{
	this->mGetCertStatus = WS_None;
	if(this->mAuthType!=AUTH_TYPE_NONE)
	{
		if(this->mPeerData->CertificateExpired(120))
			this->AsyncRefreshCert();

		if(this->mPeerData->CertificateExpired(30)) // don't even try until get the certificate
			return;
	}

	this->AsyncConnect();
	if(this->mAuthType!=AUTH_TYPE_NONE)
		this->AsyncSend(this->mPeerAuthClient.Start(this->mPeerData.get(),this->mAuthType,this->mLengthFieldSize).get());		
}

// FUNCTION: WON 0x006e76d0
// Runs the handshake: async flow, or blocking connect/request/challenges, refreshing once on an expired cert
void PeerAuthOp::RunHook()
{	
	this->mLengthFieldSize = this->mSocket->GetLengthFieldSize();
	this->mPeerAuthClient.SetClientKeySize(this->mClientKeySize, this->mUseClientKey);

	if(this->mAuthType!=AUTH_TYPE_NONE)
		this->mPeerData = this->mAuthContext->GetPeerData();

	// Asynchronous operation
	if(this->mRunMode==OP_MODE_ASYNC)
	{
		this->AsyncRun();
		return;
	}

	// Blocking operation

	// Refresh certificate if necessary
	if(this->mAuthType!=AUTH_TYPE_NONE && this->mPeerData->CertificateExpired(30))
	{
		this->mGetCertStatus = this->mAuthContext->RefreshBlock(this->TimeLeft());
		if(this->mGetCertStatus!=WS_Success)
		{
			this->Finish(WS_PeerAuth_GetCertFailure);
			return;
		}

		this->mPeerData = this->mAuthContext->GetPeerData();
	}

	for(int i=0; i<2; i++)
	{
		WONStatus aStatus;

		// Connect
		aStatus = this->mSocket->Connect(this->mConnectAddr, this->ConnectTime());
		if(this->CheckStatus(aStatus))
			return;

		// If no auth then done
		if(this->mAuthType==AUTH_TYPE_NONE)
		{
			this->Finish(WS_Success);
			return;
		}

		// Send peer auth request
		ByteBufferPtr aSendMsg = this->mPeerAuthClient.Start(this->mPeerData.get(),this->mAuthType,this->mLengthFieldSize);
		aStatus = this->mSocket->SendMsg(aSendMsg.get(), this->SendTime());
		if(this->CheckStatus(aStatus))
			return;

		// Receive challenge1
		ByteBufferPtr aRecvMsg;
		aStatus = this->mSocket->RecvMsg(aRecvMsg, this->RecvTime());
		if(this->CheckStatus(aStatus))
			return;

		// Check challenge 1
		aStatus = this->mPeerAuthClient.HandleRecvMsg(aRecvMsg->data(),aRecvMsg->length(),aSendMsg);
		if(i==0 && (aStatus==(-4) || aStatus==(-19))) // expired certificate
		{
			this->mPeerAuthClient.Reset();
			this->mGetCertStatus = this->mAuthContext->RefreshBlock(this->TimeLeft());
			if(this->mGetCertStatus!=WS_Success)
			{
				this->Finish(WS_PeerAuth_GetCertFailure);
				return;
			}

			this->mPeerData = this->mAuthContext->GetPeerData();
			continue; // try one more time
		}
		else if(this->CheckStatus(aStatus))
			return;


		// Send challenge 2
		aStatus = this->mSocket->SendMsg(aSendMsg.get(), this->SendTime());
		if(this->CheckStatus(aStatus))
			return;

		// Receive peer auth complete
		aStatus = this->mSocket->RecvMsg(aRecvMsg, this->RecvTime());
		if(this->CheckStatus(aStatus))
			return;

		// Check peer auth complete
		aStatus = this->mPeerAuthClient.HandleRecvMsg(aRecvMsg->data(),aRecvMsg->length(),aSendMsg);
		if(this->CheckStatus(aStatus))
			return;

		// Success... add encryption protocol if necessary
		this->Success();
		return;
	}
}

// FUNCTION: WON 0x006e7c40
// Adds the session encryption to the socket when the auth type encrypts, then finishes with success
void PeerAuthOp::Success()
{
	if(this->mAuthType==AUTH_TYPE_SESSION || this->mAuthType==AUTH_TYPE_PERSISTENT) // add crypt protocol
		this->mSocket->AddMsgTransform(new CryptTransform(this->GetSession()));

	this->Finish(WS_Success);
}



}

namespace WONAPI {
// FUNCTION: WON 0x006e8fa0
// Finishes the op with the status and returns true unless it is success
bool PeerAuthOp::CheckStatus(WONStatus status)
{
    if(status==0)
        return false;
    this->Finish(status);
    return true;
}
// FUNCTION: WON 0x006e7cb0
// Cleans up the tracked sub-ops
void PeerAuthOp::CleanupHook()
{
    AsyncOpWithTracker::CleanupHook();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e7580
// (public: virtual __thiscall WONAPI::LightGetCertOp::~LightGetCertOp(void)    [??1LightGetCertOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e7560
// (public: virtual void * __thiscall WONAPI::LightGetCertOp::`scalar deleting destructor'(unsigned int)    [??_GLightGetCertOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e6b90
// (protected: virtual void * __thiscall WONAPI::PeerAuthOp::`scalar deleting destructor'(unsigned int)    [??_GPeerAuthOp@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e6bb0
// (public: __thiscall WONAPI::PeerAuthClient::~PeerAuthClient(void)    [??1PeerAuthClient@WONAPI@@QAE@XZ])
