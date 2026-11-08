// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// GameSpySupport: GameSpy query/reporting integration (WON SDK); no original address identified
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <cstdio>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONDirExtended.h"
#include "ee/wonapi/WONGameSpySupport.h"
#include "ee/wonapi/WONStatus.h"
using namespace std;
namespace WONAPI {
GameSpySupport::GameSpySupport(const char* aGameName, const char* aGameVersion, const char* aSecretKey, ServerContext *theDirServers)
	:	mIP(""), mSecretKey(aSecretKey), mInitialized(false), 
		mStatus(WS_None /* SDK WS_None */), mAutoPumpTime(0),	mGameSpyStatus(0),
		mDirServers(theDirServers), mDisabled(false)
{
	// Set Basic KeyValues
	this->mQueryMap[0]["gamename"] = aGameName;
	this->mQueryMap[0]["gamever"] = aGameVersion;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GameSpySupport::~GameSpySupport()
{ }


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus GameSpySupport::Startup(const IPAddr& anIPAddr)
{
	// Store the server address
	this->mLastGetServiceTime = 0;
	this->mDisabled = true; // Start off disabled until first GetServiceOp returns
	this->mIP = anIPAddr;

	// Initialize the gamespy sdk
	this->mGameSpyStatus = qr_init(&this->mQRInstance, this->mIP.GetHostString(true).c_str(), this->mIP.GetPort(), 
						this->mQueryMap[0]["gamename"].c_str(), this->mSecretKey.c_str(), 
						GameSpySupport::HandleQueryBasic, GameSpySupport::HandleQueryInfo, GameSpySupport::HandleQueryRules, GameSpySupport::HandleQueryPlayers, this);

	// Success
	if (0 == this->mGameSpyStatus)
	{
		this->mInitialized = true;
		this->mStatus = WS_Success;

		// Should we autopump?
		this->BeginAutoPump();
	}
	else
	{
		// Handle error
		switch(this->mGameSpyStatus)
		{
		case E_GOA_WSOCKERROR:	//1
			this->mStatus = WS_GameSpySupport_WinSockError /* SDK WinSockError */;
		case E_GOA_BINDERROR:	//2
			this->mStatus = WS_GameSpySupport_BindError /* SDK BindError */;
		case E_GOA_DNSERROR:	//3
			this->mStatus = WS_GameSpySupport_DNSError /* SDK DNSError */;
		case E_GOA_CONNERROR:	//4
			this->mStatus = WS_GameSpySupport_ConnError /* SDK ConnError */;
		default:	// Unknown error, maybe should create new status WS_Unhandled
			this->mStatus = WS_GameSpySupport_UnhandledError /* SDK UnhandledError */;
		}
	} 

	return this->mStatus;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::Shutdown()
{
	// Kill the timer op
	this->SetAutoPumpTime(0);				// stop future pump timers

	AutoCrit aCrit(this->mDataCrit);
	if(this->mGetServiceOp.get()!=NULL)
	{
		this->mGetServiceOp->Kill(true);
		this->mGetServiceOp = NULL;
	}
	aCrit.Leave();

	// "exiting" causes gamespy to deregister
	this->SetGameMode("exiting");

	// Do a final pump to send state change to gamespy
	this->Pump();
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::HandleQueryBasic(char *outbuf, int maxlen, void *pThat)
{
	GameSpySupport* pSupport = static_cast<GameSpySupport*>(pThat);

	// Load all keys from mBasicQueryMap into the buffer
	pSupport->DumpKeyValueMapToBuffer(pSupport->mQueryMap[0], outbuf, maxlen);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::HandleQueryInfo(char *outbuf, int maxlen, void *pThat)
{
	GameSpySupport* pSupport = static_cast<GameSpySupport*>(pThat);

	// Load all keys from mBasicQueryMap into the buffer
	pSupport->DumpKeyValueMapToBuffer(pSupport->mQueryMap[GameSpyQueryType_Info], outbuf, maxlen);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::HandleQueryRules(char *outbuf, int maxlen, void *pThat)
{
	GameSpySupport* pSupport = static_cast<GameSpySupport*>(pThat);

	// Load all keys from mBasicQueryMap into the buffer
	pSupport->DumpKeyValueMapToBuffer(pSupport->mQueryMap[GameSpyQueryType_Rules], outbuf, maxlen);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::HandleQueryPlayers(char *outbuf, int maxlen, void *pThat)
{
	GameSpySupport* pSupport = static_cast<GameSpySupport*>(pThat);

	// Load all keys from mBasicQueryMap into the buffer
	pSupport->DumpKeyValueMapToBuffer(pSupport->mQueryMap[GameSpyQueryType_Players], outbuf, maxlen);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sends all info
/*
void GameSpySupport::HandleQueryStatus(char *outbuf, int maxlen, void *pThat)
{
	GameSpySupport* pSupport = (GameSpySupport*)pThat;

	// Add extra pre-callback functionality here
	// ...

	// Call the users installed callback
	if (pSupport->mStatusCallback != NULL)
		(pSupport->mStatusCallback)(outbuf, maxlen, pSupport->mUserParam);
}
*/


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::BeginAutoPump()
{
	AutoCrit aCrit(this->mDataCrit);

	if (this->mPumpTimerOp.get() != NULL)
	{
		this->mPumpTimerOp->Kill(true);
		this->mPumpTimerOp = NULL;
	}

	if (this->mAutoPumpTime != 0)
	{
		this->mPumpTimerOp = new AsyncOp();
		this->mPumpTimerOp->mCompletion = new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(GameSpySupport::PumpTimerCompletion,this);
		this->mPumpTimerOp->RunAsTimer(this->mAutoPumpTime);
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::SetAutoPumpTime(unsigned long theMilliseconds)
{
	AutoCrit aCrit(this->mDataCrit);

	this->mAutoPumpTime = theMilliseconds;
	this->BeginAutoPump();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::PumpTimerCompletion(AsyncOpPtr theOp, SmartPtr<RefCount> theParam)
{
	if (theOp.get()	== NULL)
		return;

	if (theOp->mKilled)
		return;

	GameSpySupport *pThat = (GameSpySupport*)theParam.get();
	pThat->Pump();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::StaticGetServiceOpCompletion(AsyncOpPtr theOp, SmartPtr<RefCount> theGameSpy)
{
	GameSpySupport *aGameSpy = (GameSpySupport*)theGameSpy.get();
	GetServiceOp *anOp = (GetServiceOp*)theOp.get();
	aGameSpy->GetServiceOpCompletion(anOp);
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::GetServiceOpCompletion(AsyncOp *theOp)
{
	AutoCrit aCrit(this->mDataCrit);

	if(theOp->mKilled)
		return;

	if(theOp!=this->mGetServiceOp.get())
		return;

	this->mDisabled = theOp->GetStatus()==WS_DirServ_ServiceNotFound /* SDK WS_DirServ_ServiceNotFound */;
	this->mGetServiceOp = NULL;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::LaunchGetServiceOp()
{
	if(this->mGetServiceOp.get()!=NULL)
		this->mGetServiceOp->Kill(true);

	GetServiceOpPtr anOp = new GetServiceOp(this->mDirServers);
	anOp->SetPath(L"/GameSpy");
	std::string &aName = this->mQueryMap[0]["gamename"];
	anOp->SetName(StringToWString(aName));
	anOp->SetNetAddr(new ByteBuffer(aName.data(),aName.length(),false));
	anOp->mCompletion = new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(GameSpySupport::StaticGetServiceOpCompletion,this);
	this->mGetServiceOp = anOp;
	anOp->RunAsync(60000);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::Pump()
{
	AutoCrit aCrit(this->mDataCrit);

	// process queries
	if(!this->mDisabled)
	{
		qr_process_queries(this->mQRInstance);

		// Is a state changed heartbeat needed?
		if (this->mStateChanged)
		{
			qr_send_statechanged(this->mQRInstance);
			this->mStateChanged = false;
		}
	}

	// Is a pump timer needed?
	if (this->IsAutoPumping())
		this->mPumpTimerOp->RunAsTimer(this->mAutoPumpTime);

	// Check if GameSpy is disabled
	time_t aTime = time(NULL);
	if(aTime - this->mLastGetServiceTime >= 3600)
	{
		this->mLastGetServiceTime = aTime;
		this->LaunchGetServiceOp();
	}
}
		
		
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::SetGameMode(const char* aGameMode)
{
	this->SetKeyValue(GameSpyQueryType_Info, "gamemode", aGameMode);

	// Signal for a state-change heartbeat
	this->mStateChanged = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::LockKeyValues()
{
	this->mDataCrit.Enter();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::UnlockKeyValues()
{
	this->mDataCrit.Leave();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::SetKeyValue(GameSpyQueryType theType, const char* theKey, const char* theValue)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mQueryMap[theType][theKey] = theValue;
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const char* GameSpySupport::GetKeyValue(GameSpyQueryType theType, const char* theKey)
{
	KeyValueMap::iterator anItr = this->mQueryMap[theType].find(theKey);
	if(anItr==this->mQueryMap[theType].end())
		return NULL;
	else
		return anItr->second.c_str();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool GameSpySupport::EraseKeyValue(GameSpyQueryType theType, const std::string &theKey)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mQueryMap[theType].erase(theKey)>0;
	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::EraseAllKeyValues(GameSpyQueryType theType)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mQueryMap[theType].clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GameSpySupport::DumpKeyValueMapToBuffer(KeyValueMap& theMap, char* outbuf, unsigned int maxlen)
{
	AutoCrit aCrit(this->mDataCrit);

	unsigned int pos = 0;
	unsigned int pairlength = 0;

	KeyValueMap::iterator anItr = theMap.begin();
	for (; anItr != theMap.end(); ++anItr)
	{
		pairlength = anItr->first.length() + anItr->second.length() + 2; // add 2 for the seperating '\'
		
		if (pairlength < (maxlen-pos))
			sprintf(outbuf+pos, "\\%s\\%s", anItr->first.c_str(), anItr->second.c_str());

		pos += pairlength;
	}
}

}
