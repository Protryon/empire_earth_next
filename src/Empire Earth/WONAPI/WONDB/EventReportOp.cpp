// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// No original address identified: WON SDK event-report request
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONEventReportOp.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONDBMsg.h"
using namespace WONAPI;

void EventReportOp::Init()
{
	this->mLengthFieldSize = 4;
	this->mHasRelatedServer = false;
	this->mHasRelatedClient = false;
	this->mHasRelatedUser = false;

	this->mActivityType = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
EventReportOp::EventReportOp(ServerContext *theEventContext) : ServerRequestOp(theEventContext)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
EventReportOp::EventReportOp(const IPAddr &theAddr) : ServerRequestOp(theAddr)
{
	this->Init();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus EventReportOp::GetNextRequest()
{
	// the request is Rust's (ee_won db.rs)
	DbRequest q(0,this->mLengthFieldSize);
	EeWonDbEvent e;
	memset(&e,0,sizeof(e));
	e.field_size = this->mLengthFieldSize;
	e.activity_type = this->mActivityType;
	e.has_server = this->mHasRelatedServer;
	e.server_type = this->mServerType;
	e.server_name = q.Text(this->mServerLogicalName);
	e.server_addr = DbRequest::Narrow(this->mServerNetAddress);
	e.has_client = this->mHasRelatedClient;
	e.client_name = q.Text(this->mClientName);
	e.client_addr = DbRequest::Narrow(this->mClientNetAddress);
	e.has_user = this->mHasRelatedUser;
	e.user_id = (uint32_t)this->mUserId;
	e.user_name = q.Text(this->mUserName);
	std::vector<EeWonDbDetail> aDetails;
	for(DetailList::iterator i = this->mDetailList.begin(); i != this->mDetailList.end(); ++i)
	{
		EeWonDbDetail d = {i->mDetailType, (int32_t)i->mDataType, (uint32_t)i->mNumber, q.Text(i->mString)};
		aDetails.push_back(d);
	}
	std::vector<EeWonDbAttachment> anAttachments;
	for(AttachmentList::iterator i = this->mAttachmentList.begin(); i != this->mAttachmentList.end(); ++i)
	{
		EeWonDbAttachment a = {q.Text(i->mDescription), i->mContentType, DbRequest::Buffer(i->mData.get())};
		anAttachments.push_back(a);
	}
	e.details = aDetails.empty() ? NULL : &aDetails[0];
	e.detail_count = aDetails.size();
	e.attachments = anAttachments.empty() ? NULL : &anAttachments[0];
	e.attachment_count = anAttachments.size();
	ByteBufferPtr aRequest;
	ee_won_db_event_request(&e, WonByteBufferSink, &aRequest);
	this->mRequest = aRequest;
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus EventReportOp::CheckResponse()
{
	// the reply's read is Rust's (ee_won db.rs)
	bool anInvalid=false;
	WONStatus aStatus=DbContestReply(this->mResponse.get(),NULL,&anInvalid);
	if(anInvalid)
		return this->InvalidReplyHeader();
	return aStatus;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EventReportOp::SetServerInfo(unsigned short theServerType, const std::wstring &theServerLogicalName, 
								   const std::string &theServerNetAddr)
{
	this->mHasRelatedServer = true;
	this->mServerType = theServerType;
	this->mServerLogicalName = theServerLogicalName;
	this->mServerNetAddress = theServerNetAddr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EventReportOp::SetClientInfo(const std::wstring &theClientName, const std::string &theClientNetAddr)
{
	this->mHasRelatedClient = true;
	this->mClientName = theClientName;
	this->mClientNetAddress = theClientNetAddr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EventReportOp::SetUserInfo(unsigned long theUserId, const std::wstring &theUserName)
{
	this->mHasRelatedUser = true;
	this->mUserId = theUserId;
	this->mUserName = theUserName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EventReportOp::AddDetail(unsigned short theDetailType, unsigned long theDetail)
{
	this->mDetailList.push_back(DetailStruct(theDetailType,theDetail));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EventReportOp::AddDetail(unsigned short theDetailType, const std::wstring &theDetail)
{
	this->mDetailList.push_back(DetailStruct(theDetailType,theDetail));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EventReportOp:: AddAttachment(const std::wstring &theDescription, unsigned char theType, const ByteBuffer *theData)
{
	this->mAttachmentList.push_back(AttachmentStruct(theDescription,theType,theData));	
}
