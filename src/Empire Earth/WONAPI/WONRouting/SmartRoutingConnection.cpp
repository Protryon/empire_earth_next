// WONAPI SmartRoutingConnection: client, group and member tracking, ignore list and chat commands
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <wctype.h>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x006bfb00
// Creates the smart routing connection with the ignore file under the WON directory and the default chat commands
SmartRoutingConnection::SmartRoutingConnection()
    : mClientItr(NULL), mGroupItr(NULL), mMemberItr(NULL)
{
    this->mDefaultGroupId = 0;
    this->mClientId = 0;
    this->mLastInputError = InputError_None;
    this->mIgnoreFilePath = WONAPICore::GetDefaultFileDirectory() + "_wonignore.bin";
    this->mDoColonEmote = true;
    this->mReadClientCDKeyId = false;
    this->AddDefaultUserCommandStrings();
    this->AddDefaultModeratorCommandStrings();
    this->SetAllowPartialNameMatch(true);
    this->SetServerWideCommands(false);
}
// FUNCTION: WON 0x006bfe60
// Returns a new smart client record (group count and ignore flag)
RoutingClientInfoPtr SmartRoutingConnection::GetNewClientInfo()
{
    return new SmartRoutingClientInfo;
}
// FUNCTION: WON 0x006c0330
// Returns the client with an id (the client being handled first), or NULL
RoutingClientInfoPtr SmartRoutingConnection::GetClient(unsigned short id)
{
    AutoCrit crit(this->mDataCrit);
    if(this->mHandlerClient.get()!=NULL && this->mHandlerClient->mClientId==id)
        return this->mHandlerClient;
    RoutingClientMap::iterator i=this->mClientMap.find(id);
    if(i==this->mClientMap.end()) return NULL;
    return i->second;
}
// FUNCTION: WON 0x006c0450
// Returns the group with an id (the group being handled first), or NULL
RoutingGroupInfoPtr SmartRoutingConnection::GetGroup(unsigned short id)
{
    AutoCrit crit(this->mDataCrit);
    if(this->mHandlerGroup.get()!=NULL && this->mHandlerGroup->mId==id)
        return this->mHandlerGroup;
    RoutingGroupMap::iterator i=this->mGroupMap.find(id);
    if(i==this->mGroupMap.end()) return NULL;
    return i->second;
}
// FUNCTION: WON 0x6c0570
// Removes a client from the client map, stepping the client iterator past it
void SmartRoutingConnection::EraseClient(const RoutingClientMap::iterator &theItr)
{
	if(theItr==this->mClientItr)
		++this->mClientItr;

	this->mClientMap.erase(theItr);
}
// FUNCTION: WON 0x6c0600
// Removes the client with an id from the client map, if present
void SmartRoutingConnection::EraseClient(unsigned short theId)
{
	RoutingClientMap::iterator anItr = this->mClientMap.find(theId);
	if(anItr!=this->mClientMap.end())
		this->EraseClient(anItr);
}
// FUNCTION: WON 0x6c0670
// Removes a group from the group map, resetting the member and group iterators that point into it
void SmartRoutingConnection::EraseGroup(const RoutingGroupMap::iterator &theItr)
{
	if(theItr->second.get()==this->mMemberItrGroup.get())
		this->mMemberItrGroup = NULL;

	if(theItr==this->mGroupItr)
		++this->mGroupItr;

	this->mGroupMap.erase(theItr);
}
// FUNCTION: WON 0x6c0740
// Removes a member from a group's member map, stepping the member iterator past it
void SmartRoutingConnection::EraseMember(RoutingMemberMap &theMap, const RoutingMemberMap::iterator &theItr)
{
	if(theItr==this->mMemberItr)
		++this->mMemberItr;

	theMap.erase(theItr);
}
// FUNCTION: WON 0x6c07d0
// Sets a client's name and marks it ignored when the name is in the ignore list
void SmartRoutingConnection::SetClientName(RoutingClientInfo *theClient, const std::wstring &theName)
{
	SmartRoutingClientInfo *aClient = static_cast<SmartRoutingClientInfo*>(theClient);
	aClient->mName = theName;
	aClient->mIgnored = this->mIgnoreSet.find(theName)!=this->mIgnoreSet.end();
}
// FUNCTION: WON 0x6c0860
// Returns the client record for an id, creating and adding an empty one when missing
SmartRoutingClientInfo* SmartRoutingConnection::EnsureInClientMap(unsigned short theClientId)
{
	RoutingClientMap::iterator anItr = this->mClientMap.insert(RoutingClientMap::value_type(theClientId, NULL)).first;
	if(anItr->second.get()==NULL)
	{
		anItr->second = this->GetNewClientInfo();
		anItr->second->mClientId = theClientId;
	}

	return (SmartRoutingClientInfo*)anItr->second.get();
}
// FUNCTION: WON 0x006c0a10
// Records the client a notification is about (no group or member)
void SmartRoutingConnection::InitHandlerVariables(RoutingClientInfo* client)
{
    this->mHandlerClient=client;
    this->mHandlerGroup=NULL;
    this->mHandlerMember=NULL;
}
// FUNCTION: WON 0x006c0ac0
// Records the group and member a notification is about, and the member's client
void SmartRoutingConnection::InitHandlerVariables(RoutingGroupInfo* group,RoutingMemberInfo* member)
{
    this->mHandlerGroup=group;
    this->mHandlerMember=member;
    if(member!=NULL) this->mHandlerClient=member->mClientInfo;
    else this->mHandlerClient=NULL;
}
// FUNCTION: WON 0x6c0be0
// Updates the client, group and member maps from a finished routing op or notification, by op type
void SmartRoutingConnection::RoutingOpCompleteHook(RoutingOp *theOp)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mHandlerClient = NULL;
	this->mHandlerGroup = NULL;
	this->mHandlerMember = NULL;

	switch(theOp->GetType())
	{
		case RoutingOp_ClientFlagsChanged:		this->HandleClientFlagsChanged	(static_cast<RoutingClientFlagsChangedOp*>(theOp));	break;
		case RoutingOp_ClientJoinedGroup:		this->HandleClientJoinedGroup		(static_cast<RoutingClientJoinedGroupOp*>(theOp));	break;
		case RoutingOp_ClientJoinedServer:		this->HandleClientJoinedServer	(static_cast<RoutingClientJoinedServerOp*>(theOp));	break;
		case RoutingOp_ClientLeftGroup:			this->HandleClientLeftGroup		(static_cast<RoutingClientLeftGroupOp*>(theOp));	break;
		case RoutingOp_ClientLeftServer:		this->HandleClientLeftServer		(static_cast<RoutingClientLeftServerOp*>(theOp));	break;

		case RoutingOp_GetClientList:			this->HandleGetClientList			(static_cast<RoutingGetClientListOp*>(theOp)); break;
		case RoutingOp_GetGroupList:			this->HandleGetGroupList			(static_cast<RoutingGetGroupListOp*>(theOp)); break;
		case RoutingOp_GetMembersOfGroup:		this->HandleGetMembersOfGroup		(static_cast<RoutingGetMembersOfGroupOp*>(theOp)); break;

		case RoutingOp_GroupCaptainChanged:		this->HandleGroupCaptainChanged	(static_cast<RoutingGroupCaptainChangedOp*>(theOp)); break;
		case RoutingOp_GroupCreated:			this->HandleGroupCreated			(static_cast<RoutingGroupCreatedOp*>(theOp)); break;
		case RoutingOp_GroupDeleted:			this->HandleGroupDeleted			(static_cast<RoutingGroupDeletedOp*>(theOp)); break;
		case RoutingOp_GroupFlagsChanged:		this->HandleGroupFlagsChanged		(static_cast<RoutingGroupFlagsChangedOp*>(theOp)); break;
		case RoutingOp_GroupMaxPlayersChanged:	this->HandleGroupMaxPlayersChanged(static_cast<RoutingGroupMaxPlayersChangedOp*>(theOp)); break;
		case RoutingOp_GroupNameChanged:		this->HandleGroupNameChanged		(static_cast<RoutingGroupNameChangedOp*>(theOp)); break;
		case RoutingOp_GroupObserverCount:		this->HandleGroupObserverCount	(static_cast<RoutingGroupObserverCountOp*>(theOp)); break;

		case RoutingOp_JoinGroup:				this->HandleJoinGroup				(static_cast<RoutingJoinGroupOp*>(theOp)); break;
		case RoutingOp_LeaveGroup:				this->HandleLeaveGroup			(static_cast<RoutingLeaveGroupOp*>(theOp)); break;
		case RoutingOp_MemberFlagsChanged:		this->HandleMemberFlagsChanged	(static_cast<RoutingMemberFlagsChangedOp*>(theOp)); break;
		case RoutingOp_RegisterClient:			this->HandleRegisterClient		(static_cast<RoutingRegisterClientOp*>(theOp)); break;
		case RoutingOp_SetClientFlags:			this->HandleSetClientFlags		(static_cast<RoutingSetClientFlagsOp*>(theOp)); break;

		case RoutingOp_ReadDataObject:			this->HandleReadDataObject		(static_cast<RoutingReadDataObjectOp*>(theOp)); break;
		case RoutingOp_DataObjectCreated:		this->HandleDataObjectCreated		(static_cast<RoutingDataObjectCreatedOp*>(theOp)); break;
		case RoutingOp_DataObjectModified:		this->HandleDataObjectModified	(static_cast<RoutingDataObjectModifiedOp*>(theOp)); break;
		case RoutingOp_DataObjectDeleted:		this->HandleDataObjectDeleted		(static_cast<RoutingDataObjectDeletedOp*>(theOp)); break;



		// For ignoring
		case RoutingOp_GroupInvitation:			this->HandleGroupInvitation		(static_cast<RoutingGroupInvitationOp*>(theOp)); break;
		case RoutingOp_GroupJoinAttempt:		this->HandleGroupJoinAttempt		(static_cast<RoutingGroupJoinAttemptOp*>(theOp)); break;
		case RoutingOp_PeerChat:				this->HandlePeerChat				(static_cast<RoutingPeerChatOp*>(theOp)); break;

	}	
}
// FUNCTION: WON 0x6c0ec0
// After registering, loads the ignore list and takes over the client id, flags and client list (or just itself)
void SmartRoutingConnection::HandleRegisterClient(RoutingRegisterClientOp *theOp)
{
	if(!(theOp->mStatus==WS_Success))
		return;

	this->LoadIgnoreSet();

	this->mClientId = theOp->GetClientId();
	this->mAsyncFlags = theOp->GetAsyncMessageFlags();

	this->mGroupMap.clear();
	this->mClientMap = theOp->GetClientMap();
	if(this->mClientMap.empty()) // didn't get client list (insert myself)
	{
		this->mMyClient = this->GetNewClientInfo();
		this->mMyClient->mName = theOp->GetClientName();
		this->mMyClient->mClientId = theOp->GetClientId();
		this->mMyClient->mFlags = theOp->GetClientFlags();
		this->mClientMap[this->mMyClient->mClientId] = this->mMyClient;
	}
	else
		this->mMyClient = this->GetClient(this->mClientId);

	this->SyncClientMapWithIgnoreList();
}
// FUNCTION: WON 0x6c1100
// Applies a successful async-flags change to the connection's async message flags
void SmartRoutingConnection::HandleSetClientFlags(RoutingSetClientFlagsOp *theOp)
{
	if(!(theOp->mStatus==WS_Success))
		return;

	unsigned long aMask = theOp->mAsyncMessageFlagMask;
	unsigned long aNewFlags = theOp->mAsyncMessageFlags;

	aNewFlags &= aMask;
	this->mAsyncFlags &= ~aMask;
	this->mAsyncFlags |= aNewFlags;
}
// FUNCTION: WON 0x6c1130
// Merges a fresh client list into the client map (drops gone clients, adds new, updates names and flags)
void SmartRoutingConnection::HandleGetClientList(RoutingGetClientListOp *theOp)
{
	const RoutingClientMap &aMap = theOp->mClientMap;
	RoutingClientMap::const_iterator aNewItr = aMap.begin();
	RoutingClientMap::iterator anOldItr = this->mClientMap.begin();

	while(anOldItr!=this->mClientMap.end() && aNewItr!=aMap.end())
	{
		if(anOldItr->first < aNewItr->first)
			this->EraseClient(anOldItr++);
		else 
		{
			if(aNewItr->first < anOldItr->first)
				anOldItr = this->mClientMap.insert(anOldItr, *aNewItr);
			else
			{
				anOldItr->second->mName = aNewItr->second->mName;
				anOldItr->second->mFlags = aNewItr->second->mFlags;
			}
			
			anOldItr++;
			aNewItr++;
		}
	}

	while(anOldItr!=this->mClientMap.end())
		this->EraseClient(anOldItr++);

	while(aNewItr!=aMap.end())	
	{
		anOldItr = this->mClientMap.insert(anOldItr, *aNewItr);
		aNewItr++;
	}


	this->mMyClient = this->GetClient(this->mClientId);
	this->SyncClientMapWithIgnoreList();
}
// FUNCTION: WON 0x6c1400
// Takes each client's CD-key id from a read of the _KeyId data objects
void SmartRoutingConnection::HandleReadDataObject(RoutingReadDataObjectOp *theOp)
{
	// return if not parsing client CDKey ID's
	if (theOp->mDataType != "_KeyId")
		return;

	const RoutingReadDataObjectOp* aOp = theOp;
	RoutingDataObjectList::const_iterator anItr = aOp->mDataObjects.begin();
	for (; anItr != aOp->mDataObjects.end(); ++anItr)
		this->SetClientCDKeyIdFromDataObject((*anItr)->mLinkId, (*anItr)->mData);
}
// FUNCTION: WON 0x6c14e0
// Takes a client's CD-key id from a new _KeyId data object
void SmartRoutingConnection::HandleDataObjectCreated(RoutingDataObjectCreatedOp *theOp)
{
	if (theOp->mDataType != "_KeyId")
		return;

	this->SetClientCDKeyIdFromDataObject(theOp->GetLinkId(), theOp->GetData());
}
// FUNCTION: WON 0x6c1560
// Takes a client's CD-key id from a changed _KeyId data object
void SmartRoutingConnection::HandleDataObjectModified(RoutingDataObjectModifiedOp *theOp)
{
	if (theOp->mDataType != "_KeyId")
		return;

	this->SetClientCDKeyIdFromDataObject(theOp->GetLinkId(), theOp->GetData());
}
// FUNCTION: WON 0x6c15d0
// Stores the CD-key id read from a _KeyId data object on the client (0 if the data is short)
void SmartRoutingConnection::SetClientCDKeyIdFromDataObject(unsigned short theClientID, ByteBufferPtr theBuf)
{
	AutoCrit aCrit(this->mDataCrit);

	// Set the client key hash
	RoutingClientInfoPtr theClientInfo = this->GetClient(theClientID);

	if (theClientInfo.get() == NULL)
		return;

	SmartPtr<SmartRoutingClientInfo> theSmartClientInfo = (SmartRoutingClientInfo*)theClientInfo.get();

	unsigned long anId = 0;
	ReadBuffer aReadBuf(theBuf->data(), theBuf->length());
	try
	{
		aReadBuf.ReadLong();	// the community
		anId = aReadBuf.ReadLong();
	}
	catch(ReadBufferException&)
	{
	}

	theSmartClientInfo->mAddress = anId;
}
// FUNCTION: WON 0x6c1770
// Adds the joining client as a member of the group, creating or updating its client record
void SmartRoutingConnection::HandleClientJoinedGroup(RoutingClientJoinedGroupOp *theOp)
{
	int aGroupId = theOp->GetGroupId();
	RoutingGroupInfoPtr aGroup = this->GetGroup(aGroupId);
	if(aGroup.get()==NULL)
		return;

	RoutingMemberInfoPtr aMember = this->GetNewMemberInfo();
	aMember->mClientId = theOp->GetClientId();
	aMember->mFlags = theOp->GetMemberFlags();

	SmartRoutingClientInfo *aClient = this->EnsureInClientMap(theOp->GetClientId());

	aMember->mClientInfo = aClient;
	if(!theOp->GetClientName().empty())
		this->SetClientName(aClient, theOp->GetClientName());

	if(theOp->HasClientFlags())
		aClient->mFlags = theOp->GetClientFlags();

	std::pair<RoutingMemberMap::iterator,bool> aRet;
	aRet = aGroup->mMembers.insert(RoutingMemberMap::value_type(aMember->mClientId,aMember));
	if(aRet.second)
		aClient->mNumGroups++;

	this->InitHandlerVariables(aGroup.get(),aRet.first->second.get());
}
// FUNCTION: WON 0x6c19f0
// Removes the leaving member; drops the client too when it is in no group and only group clients are tracked
void SmartRoutingConnection::HandleClientLeftGroup(RoutingClientLeftGroupOp *theOp)
{
	int aGroupId = theOp->GetGroupId();
	RoutingGroupInfoPtr aGroup = this->GetGroup(aGroupId);
	if(aGroup.get()==NULL)
		return;

	int aClientId = theOp->GetClientId();

	RoutingMemberMap::iterator anItr = aGroup->mMembers.find(aClientId);
	if(anItr!=aGroup->mMembers.end())
	{
		SmartRoutingClientInfo *aClient = (SmartRoutingClientInfo*)anItr->second->mClientInfo.get();
		aClient->mNumGroups--;
		if((this->mAsyncFlags & 2) && aClient->mClientId!=this->mClientId && aClient->mNumGroups==0)
			this->EraseClient(aClient->mClientId);

		this->InitHandlerVariables(aGroup.get(),anItr->second.get());
		this->EraseMember(aGroup->mMembers, anItr);
	}
}
// FUNCTION: WON 0x6c1b30
// Updates a group's observer count and merges the fetched member list into it
void SmartRoutingConnection::HandleGetMembersOfGroup(RoutingGetMembersOfGroupOp *theOp)
{
	int aGroupId = theOp->GetGroupId();
	RoutingGroupInfoPtr aGroup = this->GetGroup(aGroupId);
	if(aGroup.get()==NULL)
		return;

	aGroup->mObserverCount = theOp->GetObserverCount();
	this->SyncMemberMapWithClientMap(aGroup->mMembers, theOp->mMembers);
}
// FUNCTION: WON 0x6c1bd0
// Merges a new member list into a group's members, keeping client records and group counts in step
void SmartRoutingConnection::SyncMemberMapWithClientMap(RoutingMemberMap &theOldMap, const RoutingMemberMap &theNewMap)
{
	AutoCrit aCrit(this->mDataCrit);
	RoutingMemberMap::iterator anOldItr = theOldMap.begin();
	RoutingMemberMap::const_iterator aNewItr = theNewMap.begin();

	while(anOldItr!=theOldMap.end() || aNewItr!=theNewMap.end())
	{
		if(aNewItr==theNewMap.end() || (anOldItr!=theOldMap.end() && anOldItr->first<aNewItr->first))
		{
			SmartRoutingClientInfo *anOldClient = (SmartRoutingClientInfo*)anOldItr->second->mClientInfo.get();
			anOldClient->mNumGroups--;
			if((this->mAsyncFlags & 2) && anOldClient->mClientId!=this->mClientId && anOldClient->mNumGroups==0)
				this->EraseClient(anOldClient->mClientId);
			
			this->EraseMember(theOldMap, anOldItr++);
		}
		else if(anOldItr==theOldMap.end() || aNewItr->first<anOldItr->first)
		{
			RoutingMemberInfo *aNewMember = aNewItr->second.get();

			SmartRoutingClientInfo *anOldClient = this->EnsureInClientMap(aNewItr->first);
			SmartRoutingClientInfo *aNewClient = (SmartRoutingClientInfo*)aNewMember->mClientInfo.get();
			if(aNewClient!=NULL)
			{
				if(!aNewClient->mName.empty())
					this->SetClientName(anOldClient, aNewClient->mName);

				if(aNewClient->mFlags!=0)
					anOldClient->mFlags = aNewClient->mFlags;
			}

			aNewMember->mClientInfo = anOldClient;
			anOldClient->mNumGroups++;
			
			theOldMap.insert(*aNewItr);
			++aNewItr;
		}
		else
		{
			anOldItr->second->mFlags = aNewItr->second->mFlags;
			RoutingClientInfo *aNewClient = aNewItr->second->mClientInfo.get();
			if(aNewClient!=NULL)
			{
				RoutingClientInfo *anOldClient = anOldItr->second->mClientInfo.get();
				if(!aNewClient->mName.empty())
					this->SetClientName(anOldClient, aNewClient->mName);

				if(aNewClient->mFlags!=0)
					anOldClient->mFlags = aNewClient->mFlags;
			}

			anOldItr++;
			++aNewItr;
		}
	}
}
// FUNCTION: WON 0x6c1f20
// Links every member of a new group to its shared client record, counting the group on each client
void SmartRoutingConnection::SyncMemberMapWithClientMap(RoutingMemberMap &theMap)
{
	AutoCrit aCrit(this->mDataCrit);
	RoutingMemberMap::iterator anItr = theMap.begin();

	while(anItr!=theMap.end())
	{
		RoutingMemberInfo *aNewMember = anItr->second.get();

		SmartRoutingClientInfo *anOldClient = this->EnsureInClientMap(anItr->first);
		SmartRoutingClientInfo *aNewClient = (SmartRoutingClientInfo*)aNewMember->mClientInfo.get();
		if(aNewClient!=NULL)
		{
			if(!aNewClient->mName.empty())
				this->SetClientName(anOldClient, aNewClient->mName);

			if(aNewClient->mFlags!=0)
				anOldClient->mFlags = aNewClient->mFlags;
		}

		aNewMember->mClientInfo = anOldClient;
		anOldClient->mNumGroups++;		
		
		++anItr;
	}
}
// FUNCTION: WON 0x6c2070
// Merges a fresh group list into the group map (drops gone groups, adds new, updates fields and members)
void SmartRoutingConnection::HandleGetGroupList(RoutingGetGroupListOp *theOp)
{
	const RoutingGroupMap &aNewMap = theOp->GetGroupMap();
	RoutingGroupMap::const_iterator aNewItr = aNewMap.begin();
	RoutingGroupMap::iterator anOldItr = this->mGroupMap.begin();
	RoutingMemberMap anEmptyMemberMap;
	bool hasMembers = theOp->IncludeMembers();

	while(anOldItr!=this->mGroupMap.end() || aNewItr!=aNewMap.end())
	{
		if(aNewItr==aNewMap.end() || (anOldItr!=this->mGroupMap.end() && anOldItr->first<aNewItr->first))
		{
			this->SyncMemberMapWithClientMap(anOldItr->second->mMembers, anEmptyMemberMap);
			this->EraseGroup(anOldItr++);
		}
		else if(anOldItr==this->mGroupMap.end() || aNewItr->first<anOldItr->first)
		{
			if(hasMembers)
				this->SyncMemberMapWithClientMap(aNewItr->second->mMembers);
			
			this->mGroupMap.insert(*aNewItr);
			++aNewItr;
		}
		else
		{
			RoutingGroupInfo *anOldGroup = anOldItr->second.get();
			RoutingGroupInfo *aNewGroup = aNewItr->second.get();

			anOldGroup->mName = aNewGroup->mName;
			anOldGroup->mId = aNewGroup->mId;
			anOldGroup->mCaptainId = aNewGroup->mCaptainId;
			anOldGroup->mMaxClients = aNewGroup->mMaxClients;
			anOldGroup->mFlags = aNewGroup->mFlags;
			anOldGroup->mAsyncFlags = aNewGroup->mAsyncFlags;

			if(hasMembers)
			{
				anOldGroup->mObserverCount = aNewGroup->mObserverCount;
				this->SyncMemberMapWithClientMap(anOldGroup->mMembers, aNewGroup->mMembers);
			}

			anOldItr++;
			++aNewItr;
		}
	}
}
// FUNCTION: WON 0x6c2430
// After joining, makes the group the default and takes its member list when one came with the reply
void SmartRoutingConnection::HandleJoinGroup(RoutingJoinGroupOp *theOp)
{
	if(!(theOp->mStatus==WS_Success))
		return;

	int aGroupId = theOp->GetGroupId();
	if((theOp->mStatus==WS_Success))
		this->mDefaultGroupId = aGroupId;
	
	RoutingGroupInfoPtr aGroup = this->GetGroup(aGroupId);
	if(aGroup.get()==NULL)
		return;

	if(theOp->GetJoinFlags()&1)
		this->SyncMemberMapWithClientMap(aGroup->mMembers, theOp->mMemberMap);

	this->InitHandlerVariables(aGroup.get());
}
// FUNCTION: WON 0x006c24f0
// Stores a client's new flags
void SmartRoutingConnection::HandleClientFlagsChanged(RoutingClientFlagsChangedOp* op)
{
    int anId=op->GetClientId();
    RoutingClientInfoPtr client=this->GetClient(anId);
    if(client.get()==NULL) return;
    client->mFlags=op->GetClientFlags();
    this->InitHandlerVariables(client.get());
}
// FUNCTION: WON 0x6c2590
// Adds the new client to the client map, applying the ignore list to its name
void SmartRoutingConnection::HandleClientJoinedServer(RoutingClientJoinedServerOp *theOp)
{
	RoutingClientInfoPtr aClient = theOp->mClientInfo;
	this->SetClientName(aClient.get(), theOp->GetClientName());
	this->mClientMap[aClient->mClientId] = aClient;

	this->InitHandlerVariables(aClient.get());
}
// FUNCTION: WON 0x6c26c0
// Removes the departed client from the client map
void SmartRoutingConnection::HandleClientLeftServer(RoutingClientLeftServerOp *theOp)
{
	int aClientId = theOp->mClientId;

	RoutingClientMap::iterator anItr = this->mClientMap.find(aClientId);
	if(anItr==this->mClientMap.end())
		return;

	this->InitHandlerVariables(anItr->second.get());
	this->EraseClient(anItr);
}
// FUNCTION: WON 0x6c2740
// Re-marks every known client as ignored or not from the ignore list
void SmartRoutingConnection::SyncClientMapWithIgnoreList()
{
	AutoCrit aCrit(this->mDataCrit);

	RoutingClientMap::iterator aClientItr = this->mClientMap.begin();
	while(aClientItr!=this->mClientMap.end())
	{
		((SmartRoutingClientInfo*)aClientItr->second.get())->mIgnored = (this->mIgnoreSet.find(aClientItr->second->mName)!=this->mIgnoreSet.end());
		++aClientItr;
	}
}
// FUNCTION: WON 0x6c2860
// Stores a group's new captain
void SmartRoutingConnection::HandleGroupCaptainChanged(RoutingGroupCaptainChangedOp* op)
{
    int anId=op->GetGroupId();
    RoutingGroupInfoPtr group=this->GetGroup(anId);
    if(group.get()==NULL) return;
    group->mCaptainId=op->GetCaptainId();
    this->InitHandlerVariables(group.get());
}
// FUNCTION: WON 0x6c2900
// Stores a group's new player limit
void SmartRoutingConnection::HandleGroupMaxPlayersChanged(RoutingGroupMaxPlayersChangedOp* op)
{
    int anId=op->GetGroupId();
    RoutingGroupInfoPtr group=this->GetGroup(anId);
    if(group.get()==NULL) return;
    group->mMaxClients=op->GetMaxPlayers();
    this->InitHandlerVariables(group.get());
}
// FUNCTION: WON 0x6c29a0
// Adds the new group (no captain yet) to the group map
void SmartRoutingConnection::HandleGroupCreated(RoutingGroupCreatedOp *theOp)
{
	RoutingGroupInfoPtr aGroup = this->GetNewGroupInfo();
	aGroup->mCaptainId = 0xffff;
	aGroup->mFlags = theOp->GetFlags();
	aGroup->mAsyncFlags = theOp->GetAsyncFlags();
	aGroup->mId = theOp->GetGroupId();
	aGroup->mMaxClients = theOp->GetMaxClients();
	aGroup->mName = theOp->GetName();

	this->mGroupMap[aGroup->mId] = aGroup;
	this->InitHandlerVariables(aGroup.get());
}
// FUNCTION: WON 0x6c2b30
// Removes the deleted group from the group map
void SmartRoutingConnection::HandleGroupDeleted(RoutingGroupDeletedOp *theOp)
{
	int aGroupId = theOp->mGroupId;
	RoutingGroupMap::iterator anItr = this->mGroupMap.find(aGroupId);
	if(anItr==this->mGroupMap.end())
		return;
		
	this->InitHandlerVariables(anItr->second.get());
	this->EraseGroup(anItr);
}
// FUNCTION: WON 0x6c2bb0
// Stores a group's new flags (its async flags are not updated)
void SmartRoutingConnection::HandleGroupFlagsChanged(RoutingGroupFlagsChangedOp* op)
{
    int anId=op->GetGroupId();
    RoutingGroupInfoPtr group=this->GetGroup(anId);
    if(group.get()==NULL) return;
    group->mFlags=op->GetGroupFlags();
    this->InitHandlerVariables(group.get());
}
// FUNCTION: WON 0x6c2c50
// Stores a group's new name
void SmartRoutingConnection::HandleGroupNameChanged(RoutingGroupNameChangedOp* op)
{
    int anId=op->GetGroupId();
    RoutingGroupInfoPtr group=this->GetGroup(anId);
    if(group.get()==NULL) return;
    group->mName=op->mName;
    this->InitHandlerVariables(group.get());
}
// FUNCTION: WON 0x6c2d00
// Stores a group's new observer count
void SmartRoutingConnection::HandleGroupObserverCount(RoutingGroupObserverCountOp* op)
{
    int anId=op->GetGroupId();
    RoutingGroupInfoPtr group=this->GetGroup(anId);
    if(group.get()==NULL) return;
    group->mObserverCount=op->GetObserverCount();
    this->InitHandlerVariables(group.get());
}
// FUNCTION: WON 0x006c2da0
// Stores a group member's new member flags
void SmartRoutingConnection::HandleMemberFlagsChanged(RoutingMemberFlagsChangedOp* op)
{
    int anId=op->GetGroupId();
    RoutingGroupInfoPtr group=this->GetGroup(anId);
    if(group.get()==NULL) return;
    int aClientId=op->GetClientId();
    RoutingMemberMap::iterator i=group->mMembers.find(aClientId);
    if(i==group->mMembers.end()) return;
    i->second->mFlags=op->GetMemberFlags();
    this->InitHandlerVariables(group.get(),i->second.get());
}

// FUNCTION: WON 0x6c2ec0
// Drops the op's completion when its sender is ignored, else records the sender as the handled client
void SmartRoutingConnection::CheckIgnore(RoutingOp *theOp, unsigned short theSenderId)
{
	RoutingClientInfoPtr aClient = this->GetClient(theSenderId);
	SmartRoutingClientInfo *aSmartClient = (SmartRoutingClientInfo*)aClient.get();
	if(aSmartClient==NULL)
		return;

	if(aSmartClient->mIgnored)
		theOp->mCompletion = NULL; // swallow the completion
	else
		this->InitHandlerVariables(aSmartClient);	
}
// FUNCTION: WON 0x006c2f80
// Suppresses group invitations from ignored clients
void SmartRoutingConnection::HandleGroupInvitation(RoutingGroupInvitationOp *theOp)
{
	this->CheckIgnore(theOp, theOp->mClientId);
}

// (same code as HandlePeerChat; the linker folded the two at 0x006c2fa0)
void SmartRoutingConnection::HandleGroupJoinAttempt(RoutingGroupJoinAttemptOp *theOp)
{
	this->CheckIgnore(theOp, theOp->mClientId);
}

// Same body as 0x6fa500 in the original (an empty method folded by the linker)
void SmartRoutingConnection::HandleLeaveGroup(RoutingLeaveGroupOp *)
{
	// erase group from map or erase member list?
}

// Same body as 0x6fa500 in the original (an empty method folded by the linker)
void SmartRoutingConnection::HandleDataObjectDeleted(RoutingDataObjectDeletedOp	*theOp)
{
	// client will be removed from the client map, no special processing needed
}


// FUNCTION: WON 0x006c2fa0
// Suppresses chat from ignored clients
void SmartRoutingConnection::HandlePeerChat(RoutingPeerChatOp *theOp)
{
	this->CheckIgnore(theOp, theOp->mClientId);
}
// FUNCTION: WON 0x006c2fc0
// Sets the path of the ignore list file
void SmartRoutingConnection::SetProductName(const std::string &thePath)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mIgnoreFilePath = thePath;
}

// FUNCTION: WON 0x6c3110
// Adds the names from the ignore list file (NUL-separated wide strings) to the ignore set
void SmartRoutingConnection::LoadIgnoreSet()
{
    AutoCrit crit(this->mDataCrit);
    if(this->mIgnoreFilePath.empty()) return;
    FILE* file=fopen(this->mIgnoreFilePath.c_str(),"rb");
    if(file==NULL) return;
    std::wstring name;
    while(!feof(file)) {
        wchar_t ch=0;
        if(fread(&ch,sizeof(wchar_t),1,file)!=1) break;
        if(ch!=0) name+=ch;
        else {
            this->mIgnoreSet.insert(name);
            name.erase();
        }
    }
    fclose(file);
}
// FUNCTION: WON 0x6c3460
// Writes the ignore set to the ignore list file as NUL-terminated wide strings
void SmartRoutingConnection::SaveIgnoreSet()
{
    AutoCrit crit(this->mDataCrit);
    if(this->mIgnoreFilePath.empty()) return;
    FILE* file=fopen(this->mIgnoreFilePath.c_str(),"wb");
    if(file==NULL) return;
    IgnoreSet::iterator i=this->mIgnoreSet.begin();
    while(i!=this->mIgnoreSet.end()) {
        fwrite(i->c_str(),sizeof(wchar_t),i->length(),file);
        wchar_t end=0;
        fwrite(&end,sizeof(wchar_t),1,file);
        ++i;
    }
    fclose(file);
}
// FUNCTION: WON 0x6c3730
// Compares a typed name with a client name ignoring case; true on an exact match, partial set on a prefix match
static bool RoutingNamesMatch(const std::wstring& query,const std::wstring& name,bool& partial)
{
    partial=false;
    std::wstring::const_iterator q=query.begin(),n=name.begin();
    while(q!=query.end() && n!=name.end()) {
        if(towupper(*q)!=towupper(*n)) return false;
        ++q;
        ++n;
    }
    if(query.length()<=name.length()) partial=true;
    return query.length()==name.length();
}
// FUNCTION: WON 0x6c35a0
// Finds the client a typed name refers to: exact, unique prefix, ambiguous or not found
SmartRoutingConnection::MatchResult SmartRoutingConnection::FindClient(
    const std::wstring& inName,RoutingClientInfoPtr& outClient)
{
    AutoCrit crit(this->mDataCrit);
    MatchResult result=MatchResult_NotFound;
    for(RoutingClientMap::iterator i=this->mClientMap.begin();i!=this->mClientMap.end();++i) {
        const std::wstring& aName=i->second->mName;
        bool partial=false;
        if(RoutingNamesMatch(inName,aName,partial)) {
            if(result==MatchResult_Exact) return MatchResult_Ambiguous;
            result=MatchResult_Exact;
            outClient=i->second;
        } else if(partial) {
            if(result==MatchResult_NotFound) {
                result=MatchResult_Partial;
                outClient=i->second;
            } else if(result==MatchResult_Partial) result=MatchResult_Ambiguous;
        }
    }
    return result;
}
// FUNCTION: WON 0x006c37d0
// Ignores or unignores a client by name and saves the ignore list
void SmartRoutingConnection::IgnoreClient(RoutingClientInfo *theClient, bool on)
{
	AutoCrit aCrit(this->mDataCrit);

	static_cast<SmartRoutingClientInfo*>(theClient)->mIgnored = on;
	if(on)
		this->mIgnoreSet.insert(theClient->mName);
	else
		this->mIgnoreSet.erase(theClient->mName);

	this->SaveIgnoreSet();
}
// FUNCTION: WON 0x006c3970
// Maps a chat command word (case-insensitive) to a command
void SmartRoutingConnection::AddCommandString(const std::wstring& inText, InputCommand inCommand)
{
    AutoCrit crit(this->mDataCrit);
    this->mCommandStringMap[WStringToUpperCase(inText)] = inCommand;
}
// FUNCTION: WON 0x6c3a50
// Registers the user chat commands (whisper, emote, ignore, away)
void SmartRoutingConnection::AddDefaultUserCommandStrings()
{
    AutoCrit crit(this->mDataCrit);
    this->AddCommandString(L"W", InputCommand_Whisper);
    this->AddCommandString(L"WHISPER", InputCommand_Whisper);
    this->AddCommandString(L"ME", InputCommand_Emote);
    this->AddCommandString(L"EMOTE", InputCommand_Emote);
    this->AddCommandString(L"I", InputCommand_Ignore);
    this->AddCommandString(L"IGNORE", InputCommand_Ignore);
    this->AddCommandString(L"AWAY", InputCommand_Away);
    this->AddCommandString(L"AFK", InputCommand_Away);
}
// FUNCTION: WON 0x6c3d90
// Registers the moderator chat commands (moderator mode, mute, ban, unmute, server-wide forms)
void SmartRoutingConnection::AddDefaultModeratorCommandStrings()
{
    AutoCrit crit(this->mDataCrit);
    this->AddCommandString(L"MOD", InputCommand_BecomeModerator);
    this->AddCommandString(L"SMUTE", InputCommand_ServerMute);
    this->AddCommandString(L"SBAN", InputCommand_ServerBan);
    this->AddCommandString(L"MUTE", InputCommand_Mute);
    this->AddCommandString(L"BAN", InputCommand_Ban);
    this->AddCommandString(L"UNMUTE", InputCommand_Unmute);
    this->AddCommandString(L"SUNMUTE", InputCommand_ServerUnmute);
}
// FUNCTION: WON 0x006c40a0
// Sets whether chat commands accept a unique name prefix or only exact names
void SmartRoutingConnection::SetAllowPartialNameMatch(bool inAllow)
{
    AutoCrit crit(this->mDataCrit);
    if(inAllow)
        this->mMatchTolerance = MatchResult_Partial;
    else
        this->mMatchTolerance = MatchResult_Exact;
}
// FUNCTION: WON 0x006c40f0
// Sets whether mute/ban commands apply server-wide
void SmartRoutingConnection::SetServerWideCommands(bool inOn)
{
    AutoCrit crit(this->mDataCrit);
    this->mServerWideCommands = inOn;
}
// FUNCTION: WON 0x006c4120
// Forgets this client's id and record, then closes the routing connection
void SmartRoutingConnection::CloseHook()
{
    this->mClientId=0;
    this->mMyClient=NULL;
    RoutingConnection::CloseHook();
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006bfce0
// (public: __thiscall std::map<unsigned short,class WONAPI::SmartPtr<struct WONAPI::RoutingClientInfo>,struct std::less<unsigned short>,class std::alloca)
// TEMPLATE: WON 0x006bfda0
// (public: __thiscall std::map<unsigned short,class WONAPI::SmartPtr<struct WONAPI::RoutingGroupInfo>,struct std::less<unsigned short>,class std::allocat)
// TEMPLATE: WON 0x006bff00
// (public: virtual void * __thiscall WONAPI::SmartRoutingClientInfo::`scalar deleting destructor'(unsigned int)    [??_GSmartRoutingClientInfo@WONAPI@@UA)
// TEMPLATE: WON 0x006bff20
// (public: virtual __thiscall WONAPI::SmartRoutingClientInfo::~SmartRoutingClientInfo(void)    [??1SmartRoutingClientInfo@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006bff90
// (public: virtual void * __thiscall WONAPI::SmartRoutingConnection::`scalar deleting destructor'(unsigned int)    [??_GSmartRoutingConnection@WONAPI@@UA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006bffb0
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,enum)
// TEMPLATE: WON 0x006c0070
// (public: __thiscall std::set<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,stru)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c0130
// (public: virtual __thiscall WONAPI::SmartRoutingConnection::~SmartRoutingConnection(void)    [??1SmartRoutingConnection@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c1550
// (public: unsigned short __thiscall WONAPI::RoutingDataObjectCreatedOp::GetLinkId(void)const     [?GetLinkId@RoutingDataObjectCreatedOp@WONAPI@@QBEGXZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c2370
// (public: __thiscall std::map<unsigned short,class WONAPI::SmartPtr<struct WONAPI::RoutingMemberInfo>,struct std::less<unsigned short>,class std::alloca)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c3420
// (_wmemset)
// TEMPLATE: WON 0x006c9040
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingClientInfo> >,struct std::)
// TEMPLATE: WON 0x006c9200
// (public: struct std::pair<class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingGroupInfo)
// TEMPLATE: WON 0x006c92f0
// (public: class WONAPI::SmartPtr<struct WONAPI::RoutingDataObject> const & __thiscall std::list<class WONAPI::SmartPtr<struct WONAPI::RoutingDataObject>)
// TEMPLATE: WON 0x006c9300
// (protected: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingMemberInfo> >,struct st)
// TEMPLATE: WON 0x006c96e0
// (protected: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingGroupInfo> >,struct std)
// TEMPLATE: WON 0x006c99e0
// (public: struct std::pair<class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned)
// TEMPLATE: WON 0x006c9ae0
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006c9b90
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,class s)
// TEMPLATE: WON 0x006c9c40
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,class s)
// TEMPLATE: WON 0x006c9c90
// (protected: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006ca290
// (protected: struct std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingMemberInfo> >,struct s)
// TEMPLATE: WON 0x006ca750
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006cac30
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006cacb0
// (protected: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,stru)
// TEMPLATE: WON 0x006cafb0
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,class s)
// TEMPLATE: WON 0x006cb490
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,class s)
// TEMPLATE: WON 0x006cb4e0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006cb550
// (protected: struct std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,cla)
// TEMPLATE: WON 0x006cb5a0
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
// TEMPLATE: WON 0x006cbc60
// (void __cdecl std::_Construct(struct std::pair<unsigned short const ,class WONAPI::SmartPtr<struct WONAPI::RoutingMemberInfo> > *,struct std::pair<unsi)
// TEMPLATE: WON 0x006cbca0
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsi)
// TEMPLATE: WON 0x006cbe00
// (void __cdecl std::_Distance(class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsig)
}
