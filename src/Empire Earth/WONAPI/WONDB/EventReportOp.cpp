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
	WriteBuffer aBuf(this->mLengthFieldSize,true);
	aBuf.AppendByte(5);			// small message
	aBuf.AppendShort(4);		// Event Service
	aBuf.AppendShort(1);		// Report Event

	unsigned char aContentFlags = 0;
	if (this->mHasRelatedServer) aContentFlags |= 0x02;
	if (this->mHasRelatedClient) aContentFlags |= 0x04;
	if (this->mHasRelatedUser)   aContentFlags |= 0x08;
	aBuf.AppendByte(aContentFlags);

	aBuf.AppendByte(this->mDetailList.size());
	aBuf.AppendByte(this->mAttachmentList.size());
	
	// general info
	aBuf.AppendShort(this->mActivityType);
	
	// server info
	if (this->mHasRelatedServer)
	{
		aBuf.AppendShort(this->mServerType);
		aBuf.AppendWString(this->mServerLogicalName,2);
		aBuf.AppendString(this->mServerNetAddress);
	}

    // client info
	if (this->mHasRelatedClient)
	{
		aBuf.AppendWString(this->mClientName,2);
		aBuf.AppendString(this->mClientNetAddress);
	}

	// user info
	if (this->mHasRelatedUser)
	{
		aBuf.AppendShort(1); // Auth 1
		aBuf.AppendLong(this->mUserId);
		aBuf.AppendWString(this->mUserName,2);
	}

	// details
	DetailList::iterator aDetailItr = this->mDetailList.begin();
	while(aDetailItr!=this->mDetailList.end())
	{
		aBuf.AppendShort(aDetailItr->mDetailType);
		aBuf.AppendByte(aDetailItr->mDataType);
		switch(aDetailItr->mDataType)
		{
			case DetailStruct::DataType_Long: aBuf.AppendLong(aDetailItr->mNumber); break;
			case DetailStruct::DataType_WString: aBuf.AppendWString(aDetailItr->mString,2); break;
		}

		++aDetailItr;
	}

	// attachments
	AttachmentList::iterator anAttachmentItr = this->mAttachmentList.begin();
	while(anAttachmentItr != this->mAttachmentList.end())
	{
		aBuf.AppendWString(anAttachmentItr->mDescription,2);
		aBuf.AppendByte(anAttachmentItr->mContentType);
		aBuf.AppendBuffer(anAttachmentItr->mData,4);

		++anAttachmentItr;
	}

	this->mRequest = aBuf.ToByteBuffer(true);
	return (WONStatus)WS_ServerReq_Recv;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONStatus EventReportOp::CheckResponse()
{
	ReadBuffer aMsg(this->mResponse->data(),this->mResponse->length());
	unsigned char aHeaderType = aMsg.ReadByte();
	unsigned short aServiceType = aMsg.ReadShort();
	unsigned short aMessageType = aMsg.ReadShort();
	if(aHeaderType!=5 || aServiceType!=4 || aMessageType!=2)
		return this->InvalidReplyHeader();


	short aStatus = aMsg.ReadShort();
	return (WONStatus)aStatus;
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
