// WON SDK thread-safe snapshot copies and routing-map traversal
// No original address identified
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
RoutingMemberInfoPtr SmartRoutingConnection::GetMemberRef(unsigned short theClientId, int theGroupId)
{
	AutoCrit aCrit(this->mDataCrit);
	if(theGroupId==-1)
		theGroupId = this->mDefaultGroupId;

	if(this->mHandlerMember.get()!=NULL && this->mHandlerMember->mClientId==theClientId && this->mHandlerGroup->mId==theGroupId)
		return this->mHandlerMember;

	RoutingGroupInfoPtr aGroup = this->GetGroup(theGroupId);
	if(aGroup.get()==NULL)
		return NULL;

	RoutingMemberMap::iterator anItr = aGroup->mMembers.find(theClientId);
	if(anItr==aGroup->mMembers.end())
		return NULL;
	else
		return anItr->second;
}

static bool CopyClient(SmartRoutingConnection::ClientInfo &theClientCopy, RoutingClientInfo *theClient)
{
	if(theClient==NULL)
		return false;

	theClientCopy.mFlags = theClient->mFlags;
	theClientCopy.mId = theClient->mClientId;
	theClientCopy.mName = theClient->mName;

	SmartRoutingClientInfo* aSmartClient = static_cast<SmartRoutingClientInfo*>(theClient);
	theClientCopy.mCDKeyId = aSmartClient->mAddress;
	return true;
}

static bool CopyGroup(SmartRoutingConnection::GroupInfo &theGroupCopy, RoutingGroupInfo *theGroup)
{
	if(theGroup==NULL)
		return false;

	theGroupCopy.mCaptainId = theGroup->mCaptainId;
	theGroupCopy.mFlags = theGroup->mFlags;
	theGroupCopy.mAsyncFlags = theGroup->mAsyncFlags;
	theGroupCopy.mId = theGroup->mId;
	theGroupCopy.mMaxPlayers = theGroup->mMaxClients;
	theGroupCopy.mName = theGroup->mName;
	theGroupCopy.mObserverCount = theGroup->mObserverCount;
	return true;
}

static bool CopyMember(SmartRoutingConnection::MemberInfo &theMemberCopy, RoutingMemberInfo *theMember)
{
	if(theMember==NULL)
		return false;
	
	RoutingClientInfo *aClient = theMember->mClientInfo;
	if(aClient!=NULL)
	{
		theMemberCopy.mName = aClient->mName;
		theMemberCopy.mClientFlags = aClient->mFlags;
	}

	theMemberCopy.mClientId = theMember->mClientId;
	theMemberCopy.mMemberFlags = theMember->mFlags;
	return true;
}

bool SmartRoutingConnection::GetClient(unsigned short theClientId, ClientInfo &theClient)
{
	AutoCrit aCrit(this->mDataCrit);
	RoutingClientInfoPtr aClient = this->GetClient(theClientId);
	return CopyClient(theClient,aClient);
}

bool SmartRoutingConnection::GetGroup(unsigned short theGroupId, GroupInfo &theGroup)
{
	AutoCrit aCrit(this->mDataCrit);
	RoutingGroupInfoPtr aGroup = this->GetGroup(theGroupId);
	return CopyGroup(theGroup,aGroup);
}

bool SmartRoutingConnection::GetMember(unsigned short theGroupId, unsigned short theClientId, MemberInfo &theMember)
{
	AutoCrit aCrit(this->mDataCrit);
	RoutingMemberInfoPtr aMember = this->GetMemberRef(theClientId,theGroupId);
	return CopyMember(theMember,aMember);
}

void SmartRoutingConnection::StartClientItr()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mClientItr = this->mClientMap.begin();
}

bool SmartRoutingConnection::GetNextClient(ClientInfo &theInfo)
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mClientItr==this->mClientMap.end())
		return false;

	CopyClient(theInfo,this->mClientItr->second);
	++this->mClientItr;
	return true;
}

void SmartRoutingConnection::StartGroupItr()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mGroupItr = this->mGroupMap.begin();
}

bool SmartRoutingConnection::GetNextGroup(GroupInfo &theInfo)
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mGroupItr==this->mGroupMap.end())
		return false;

	CopyGroup(theInfo,this->mGroupItr->second);
	++this->mGroupItr;
	return true;
}

bool SmartRoutingConnection::StartMemberItr(unsigned short theGroupId)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mMemberItrGroup = this->GetGroup(theGroupId);

	if(this->mMemberItrGroup.get()==NULL)
		return false;

	this->mMemberItr = this->mMemberItrGroup->mMembers.begin();
	return true;
}

bool SmartRoutingConnection::GetNextMember(MemberInfo &theMember)
{
	AutoCrit aCrit(this->mDataCrit);
	if(this->mMemberItrGroup.get()==NULL)
		return false;

	if(this->mMemberItr==this->mMemberItrGroup->mMembers.end())
		return false;

	CopyMember(theMember,this->mMemberItr->second);
	++this->mMemberItr;
	return true;
}

bool SmartRoutingConnection::GetLastTargetMember(MemberInfo &theMember)
{
	AutoCrit aCrit(this->mDataCrit);
	return CopyMember(theMember,this->mLastTargetMember);
}

bool SmartRoutingConnection::GetLastTargetClient(ClientInfo &theClient)
{
	AutoCrit aCrit(this->mDataCrit);
	return CopyClient(theClient, this->mLastTargetClient);
}
}
