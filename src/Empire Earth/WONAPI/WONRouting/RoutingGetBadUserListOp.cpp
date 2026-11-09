// WONAPI routing op that fetches a banned or muted user list
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// FUNCTION: WON 0x6c5320
// Sends the routing server a request for the banned/muted user list of a group
void RoutingGetBadUserListOp::SendRequest()
{
    this->InitSendMsg(22);
    this->mSendMsg.AppendByte((unsigned char)this->mListType);
    this->mSendMsg.AppendShort(this->mGroupId);
    this->mSendMsg.AppendByte(this->mFlags);
    this->SendMsg();
    this->AddOp();
}

// FUNCTION: WON 0x6c5370
// Parses the bad-user list reply into the op's list, reading the optional fields its flags select; returns the status
WONStatus RoutingGetBadUserListOp::HandleReply(unsigned char inMsgType,ReadBuffer& inMsg)
{
    if(inMsgType!=21) return (WONStatus)WS_RoutingOp_DontWantReply;
    this->mBadUserList.clear();
    WONStatus status=inMsg.ReadShort();
    if(status==0) {
        this->mListType=(RoutingBadUserListType)(signed char)inMsg.ReadByte();
        this->mGroupId=inMsg.ReadShort();
        this->mFlags=inMsg.ReadByte();
        int count=(unsigned short)inMsg.ReadShort();
        for(;count>0;--count) {
            unsigned short length=inMsg.ReadShort();
            long start=inMsg.pos();
            RoutingBadUser user; // default: the three longs zeroed, three empty wstrings
            user.mWONId=inMsg.ReadLong();
            inMsg.ReadWString(user.mName,2);
            if(this->GetFlags()&1) user.mExpirationDiff=inMsg.ReadLong();
            if(this->GetFlags()&2) user.mModeratorWONId=inMsg.ReadLong();
            if(this->GetFlags()&4) inMsg.ReadWString(user.mModeratorName,2);
            if(this->GetFlags()&8) inMsg.ReadWString(user.mModeratorComment,2);
            this->mBadUserList.push_back(user);
            inMsg.ReadBytes(length-inMsg.pos()+start);
        }
    }
    return status;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006c5600
// (public: __thiscall WONAPI::RoutingBadUser::~RoutingBadUser(void)    [??1RoutingBadUser@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ca450
// (protected: void __thiscall std::list<struct WONAPI::RoutingBadUser,class std::allocator<struct WONAPI::RoutingBadUser> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x006cbb90
// (void __cdecl std::_Construct(struct WONAPI::RoutingBadUser *,struct WONAPI::RoutingBadUser const &))
// TEMPLATE: WON 0x006c9660
// (public: class std::list<struct WONAPI::RoutingBadUser,class std::allocator<struct WONAPI::RoutingBadUser> >::iterator __thiscall std::list<struct WONA)
