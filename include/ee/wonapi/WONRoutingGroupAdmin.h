// No original address identified: WON SDK routing operations
#ifndef EE_WONAPI_WONROUTINGGROUPADMIN_H
#define EE_WONAPI_WONROUTINGGROUPADMIN_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
namespace WONAPI {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RoutingRelinquishCaptaincyOp : public RoutingOp
{
private:
	unsigned short mGroupId;
	unsigned short mNewCaptainId;

	virtual void SendRequest();
	virtual WONStatus HandleReply(unsigned char theMsgType, ReadBuffer &theMsg);

public:
	RoutingRelinquishCaptaincyOp(RoutingConnection *theConnection, unsigned short theGroupId = 0, unsigned short theNewCaptainId = 0xffff /* SDK RoutingId_Invalid */) : RoutingOp(theConnection), mGroupId(theGroupId), mNewCaptainId(theNewCaptainId) {}

	void SetGroupId(unsigned short theId) { this->mGroupId = theId; }	
	void SetNewCaptainId(unsigned short theId) { this->mNewCaptainId = theId; }

	unsigned short GetGroupId() const { return this->mGroupId; }
	unsigned short GetNewCaptainId() const { return this->mNewCaptainId; }

	virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_RelinquishCaptaincy; /* SDK RoutingOp_RelinquishCaptaincy */ }
};


typedef SmartPtr<RoutingRelinquishCaptaincyOp> RoutingRelinquishCaptaincyOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RoutingSetGroupMaxPlayersOp : public RoutingOp
{
private:
	unsigned short mGroupId;
	unsigned short mMaxPlayers;

	virtual void SendRequest();
	virtual WONStatus HandleReply(unsigned char theMsgType, ReadBuffer &theMsg);

public:
	RoutingSetGroupMaxPlayersOp(RoutingConnection *theConnection, unsigned short theGroupId = 0, unsigned short theMaxPlayers = 0) : RoutingOp(theConnection), mGroupId(theGroupId), mMaxPlayers(theMaxPlayers) {}

	void SetGroupId(unsigned short theId) { this->mGroupId = theId; }	
	void SetMaxPlayers(unsigned short theMaxPlayers) { this->mMaxPlayers = theMaxPlayers; }

	unsigned short GetGroupId() const { return this->mGroupId; }
	unsigned short GetMaxPlayers() const { return this->mMaxPlayers; }

	virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_SetGroupMaxPlayers; /* SDK RoutingOp_SetGroupMaxPlayers */ }
};


typedef SmartPtr<RoutingSetGroupMaxPlayersOp> RoutingSetGroupMaxPlayersOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RoutingSetGroupNameOp : public RoutingOp
{
private:
	unsigned short mGroupId;
	std::wstring mGroupName;

	virtual void SendRequest();
	virtual WONStatus HandleReply(unsigned char theMsgType, ReadBuffer &theMsg);

public:
	RoutingSetGroupNameOp(RoutingConnection *theConnection) : RoutingOp(theConnection), mGroupId(0) {}
	RoutingSetGroupNameOp(RoutingConnection *theConnection, unsigned short theGroupId, const std::wstring &theName) : RoutingOp(theConnection), mGroupId(theGroupId), mGroupName(theName) {}

	void SetGroupId(unsigned short theId) { this->mGroupId = theId; }	
	void SetGroupName(const std::wstring &theGroupName) { this->mGroupName = theGroupName; }

	unsigned short GetGroupId() const { return this->mGroupId; }
	const std::wstring& GetGroupName() const { return this->mGroupName; }

	virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_SetGroupName; /* SDK RoutingOp_SetGroupName */ }
};


typedef SmartPtr<RoutingSetGroupNameOp> RoutingSetGroupNameOpPtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RoutingSetGroupPasswordOp : public RoutingOp
{
private:
	unsigned short mGroupId;
	std::wstring mGroupPassword;

	virtual void SendRequest();
	virtual WONStatus HandleReply(unsigned char theMsgType, ReadBuffer &theMsg);

public:
	RoutingSetGroupPasswordOp(RoutingConnection *theConnection) : RoutingOp(theConnection), mGroupId(0) {}
	RoutingSetGroupPasswordOp(RoutingConnection *theConnection, unsigned short theGroupId, const std::wstring &thePassword) : RoutingOp(theConnection), mGroupId(theGroupId), mGroupPassword(thePassword) {}

	void SetGroupId(unsigned short theId) { this->mGroupId = theId; }	
	void SetGroupPassword(const std::wstring &theGroupPassword) { this->mGroupPassword = theGroupPassword; }

	unsigned short GetGroupId() const { return this->mGroupId; }
	const std::wstring& GetGroupPassword() const { return this->mGroupPassword; }

	virtual RoutingOpType GetType() const { return (RoutingOpType)RoutingOp_SetGroupPassword; /* SDK RoutingOp_SetGroupPassword */ }
};


typedef SmartPtr<RoutingSetGroupPasswordOp> RoutingSetGroupPasswordOpPtr;
}
#endif
