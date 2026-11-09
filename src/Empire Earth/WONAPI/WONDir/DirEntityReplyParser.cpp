// WONAPI parser for the directory server's entity replies
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
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x006f04f0
// Returns whether the get flags ask for directories only (no services)
bool DirEntityReplyParser::OnlyDirs(DWORD inFlags)
{
    return (inFlags & 2) == 0;
}
// FUNCTION: WON 0x006f0500
// Returns whether the get flags ask for services only (no directories)
bool DirEntityReplyParser::OnlyServices(DWORD inFlags)
{
    return (inFlags & 5) == 0;
}
// FUNCTION: WON 0x006f0510
// Parses a multi-entity reply into entities with the fields its flags include; says whether more replies follow
WONStatus DirEntityReplyParser::ParseMultiEntityReply(const void* inData,
    unsigned long inLength, DirEntityList& outEntities, unsigned char* outSequence)
{
    int i, j, k;
    int count, dataCount, aclCount, userCount;
    unsigned char length;
    char type;
    bool isService, isDir;
    ReadBuffer message(inData, inLength);
    unsigned char header = message.ReadByte();
    unsigned short service = message.ReadShort();
    unsigned short messageType = message.ReadShort();
    if (header != 5 || service != 2 || messageType != 3)
        return (WONStatus)WS_ServerReq_InvalidReplyHeader;
    short status = message.ReadShort();
    if (status < 0) return status;
    unsigned char sequence = message.ReadByte();
    if (outSequence != NULL) *outSequence = sequence;
    bool moreReplies = (sequence & 0x80) == 0;
    DWORD flags = message.ReadLong();
    bool onlyDirs = DirEntityReplyParser::OnlyDirs(flags);
    bool onlyServices = DirEntityReplyParser::OnlyServices(flags);
    count = (unsigned short)message.ReadShort();
    for (i = 0; i < count; ++i) {
        DirEntityPtr entity = new DirEntity;
        if (flags & 0x10) entity->mType = message.ReadByte();
        isService = entity->mType == 'S' || onlyServices;
        isDir = entity->mType == 'D' || onlyDirs;
        if (isService) {
            if (flags & 0x01000000) message.ReadWString(entity->mPath, 2);
            if (flags & 0x02000000) message.ReadWString(entity->mName, 2);
            if (flags & 0x04000000) {
                length = message.ReadByte();
                entity->mNetAddr = new ByteBuffer(message.ReadBytes(length), length, false);
            }
        } else if (isDir) {
            if (flags & 0x00010000) message.ReadWString(entity->mPath, 2);
            if (flags & 0x00020000) message.ReadWString(entity->mName, 2);
            if (flags & 0x00040000) entity->mReqUnique = message.ReadBool();
        }
        if (flags & 0x20) message.ReadWString(entity->mDisplayName, 2);
        if (flags & 0x100) entity->mLifespan = message.ReadLong();
        if (flags & 0x40) entity->mCreated = message.ReadLong();
        if (flags & 0x80) entity->mTouched = message.ReadLong();
        if (flags & 0x2000) entity->mCRC = message.ReadLong();
        if (flags & 0x8000) {
            entity->mCreateIP = message.ReadLong();
            entity->mTouchIP = message.ReadLong();
        }
        if (flags & 0x4000) {
            entity->mCreateId = message.ReadLong();
            entity->mTouchId = message.ReadLong();
        }
        if (flags & 0x800) {
            dataCount = (unsigned short)message.ReadShort();
            for (j = 0; j < dataCount; ++j) {
                DirDataObject data;
                if (flags & 0x200) message.ReadString(data.mDataType, 1);
                if (flags & 0x400) data.mData = message.ReadBuf(2);
                entity->mDataObjects.push_back(data);
            }
        }
        if (flags & 0x1000) {
            aclCount = (unsigned short)message.ReadShort();
            for (j = 0; j < aclCount; ++j) {
                type = message.ReadByte();
                userCount = (unsigned short)message.ReadShort();
                for (k = 0; k < userCount; ++k) {
                    long user = message.ReadLong();
                    long comm = message.ReadLong();
                    short trust = message.ReadShort();
                    DirACL acl;
                    acl.mType=type; acl.mUser=user; acl.mCommunity=comm; acl.mTrust=trust;
                    entity->mACLs.push_back(acl);
                }
            }
        }
        outEntities.push_back(entity);
    }
    return moreReplies ? (WONStatus)100059 : (WONStatus)0;
}
}

// No original address identified: WON SDK single-entity reply parser
#include "ee/wonapi/WONDirExtended.h"
namespace WONAPI {
WONStatus DirEntityReplyParser::ParseSingleEntityReply(const void *theMsg, unsigned long theMsgLen, DirEntityPtr &theEntity)
{
	ReadBuffer aMsg(theMsg,theMsgLen);
	unsigned char aHeaderType = aMsg.ReadByte();		// 5 = Small Header
	unsigned short aServiceType = aMsg.ReadShort();		// 2 = DirServer
	unsigned short aMessageType = aMsg.ReadShort();		// 2 = SingleEntity Reply

	if(aHeaderType!=5 || aServiceType!=2 || aMessageType!=2)
		return (WONStatus)WS_ServerReq_InvalidReplyHeader;

	short aStatus = aMsg.ReadShort();
	if(aStatus<0)
		return (WONStatus)aStatus;

	DWORD flags = aMsg.ReadLong();

	DirEntityPtr anEntity = new DirEntity;

	if(flags & DIR_GF_ADDTYPE) 
		anEntity->mType = aMsg.ReadByte();

	if(flags & DIR_GF_SERVADDPATH)
		aMsg.ReadWString(anEntity->mPath,2);

	if(flags & DIR_GF_SERVADDNAME)
		aMsg.ReadWString(anEntity->mName,2);

	if(flags & DIR_GF_SERVADDNETADDR)
		anEntity->mNetAddr = aMsg.ReadBuf(1);

	if(flags & DIR_GF_ADDDISPLAYNAME)
		aMsg.ReadWString(anEntity->mDisplayName,2);

	if(flags & DIR_GF_ADDLIFESPAN)
		anEntity->mLifespan = aMsg.ReadLong();

	if(flags & DIR_GF_ADDCREATED)
		anEntity->mCreated = aMsg.ReadLong();

	if(flags & DIR_GF_ADDTOUCHED)
		anEntity->mTouched = aMsg.ReadLong();

	if(flags & DIR_GF_ADDCRC)
		anEntity->mCRC = aMsg.ReadLong();

	if(flags & DIR_GF_ADDDATAOBJECTS) // read data objects
	{
		unsigned short numDataObjects = aMsg.ReadShort();
		for(int j=0; j<numDataObjects; j++)
		{
			DirDataObject anObject;
			if(flags & DIR_GF_ADDDOTYPE)
				aMsg.ReadString(anObject.mDataType,1);

			if(flags & DIR_GF_ADDDODATA)
				anObject.mData = aMsg.ReadBuf(2);

			anEntity->mDataObjects.push_back(anObject);
		}
	}

	if(flags & DIR_GF_ADDACLS) // read ACLS
	{
		unsigned short aNumACLs = aMsg.ReadShort();
		for(int i=0; i<aNumACLs; i++)
		{
			char aType = aMsg.ReadByte();
			unsigned short aNumPermissions = aMsg.ReadShort();
			for(int j=0; j<aNumPermissions; j++)
			{
				unsigned long aUserId = aMsg.ReadLong();
				unsigned long aCommunity = aMsg.ReadLong();
				unsigned short aTrust = aMsg.ReadShort();

				DirACL anACL; anACL.mType=aType; anACL.mUser=aUserId; anACL.mCommunity=aCommunity; anACL.mTrust=aTrust; anEntity->mACLs.push_back(anACL);
			}
		}
	}
	
	theEntity = anEntity;
	return (WONStatus)WS_Success;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1f10
// (protected: void __thiscall std::list<struct WONAPI::DirDataObject,class std::allocator<struct WONAPI::DirDataObject> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x006f2680
// (void __cdecl std::_Construct(struct WONAPI::DirDataObject *,struct WONAPI::DirDataObject const &))
// TEMPLATE: WON 0x006f2700
// (void __cdecl std::_Construct(struct WONAPI::DirACL *,struct WONAPI::DirACL const &))
