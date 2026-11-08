// WONAPI op that lists several directories in one request (file grouping inferred)
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

// FUNCTION: WON 0x006f1520
// Sets 4-byte length fields and clears the failure count
void GetMultiDirOp::Init()
{
    this->mLengthFieldSize=4;
    this->mNumFailedRequests=0;
}
// FUNCTION: WON 0x6f1540
// Creates a multi-directory request for a server context
GetMultiDirOp::GetMultiDirOp(ServerContext* inContext)
    :ServerRequestOp(inContext),mCurRequest(NULL)
{
    this->Init();
}

// FUNCTION: WON 0x6f1790
// Forgets the entities each request received
void GetMultiDirOp::Reset()
{
    GetEntityRequestList::iterator i;
    i=this->mRequestList.begin();
    while(true) {
        if(i==this->mRequestList.end()) break;
        (*i)->Reset();
        ++i;
    }
}
// FUNCTION: WON 0x6f17c0
// Adds a directory query to the request
void GetMultiDirOp::AddRequest(GetEntityRequest* inRequest)
{
    this->mRequestList.push_back(inRequest);
}
// FUNCTION: WON 0x6f18f0
// Adds a query for a path with the op's flags and data types
void GetMultiDirOp::AddPath(const std::wstring& inPath)
{
    GetEntityRequestPtr request=new GetEntityRequest;
    request->SetPath(inPath);
    request->SetFlags(this->mFlags);
    request->SetDataTypes(this->mDataTypes);
    this->AddRequest(request.get());
}
// FUNCTION: WON 0x6f1a30
// Builds the multi-get message holding every query and starts reading replies at the first
WONStatus GetMultiDirOp::GetNextRequest()
{
    WriteBuffer message(this->mLengthFieldSize,true);
    message.AppendByte(5);
    message.AppendShort(2);
    message.AppendShort(113);
    message.AppendShort(0);
    message.AppendByte(static_cast<unsigned char>(this->mRequestList.size()));
    GetEntityRequestList::iterator i;
    i=this->mRequestList.begin();
    while(true) {
        if(!(i!=this->mRequestList.end())) break;
        (*i)->Pack(&message,GetEntityRequest::Pack_GetMultiDirOp);
        ++i;
    }
    this->mRequest=message.ToByteBuffer(true);
    this->mCurRequest=this->mRequestList.begin();
    return WS_ServerReq_Recv;
}
// FUNCTION: WON 0x6f1b90
// Hands each reply to the current query; when all are done reports success, partial or total failure
WONStatus GetMultiDirOp::CheckResponse()
{
    if(this->mCurRequest==this->mRequestList.end()) {
        GetEntityRequestPtr request=new GetEntityRequest;
        return request->ParseMultiEntityReply(this->mResponse->data(),this->mResponse->length());
    }
    (*this->mCurRequest)->ParseMultiEntityReply(this->mResponse->data(),this->mResponse->length());
    if((*this->mCurRequest)->GetStatus()!=100059) {
        if((*this->mCurRequest)->GetStatus()!=0)
            ++this->mNumFailedRequests;
        ++this->mCurRequest;
    }
    if(this->mCurRequest==this->mRequestList.end()) {
        if(this->mNumFailedRequests==0)
            return WS_Success;
        return this->mNumFailedRequests==static_cast<int>(this->mRequestList.size())?-1224:-1223;
    }
    return WS_ServerReq_Recv;
}
} // WONAPI

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1610
// (public: virtual void * __thiscall WONAPI::GetMultiDirOp::`scalar deleting destructor'(unsigned int)    [??_GGetMultiDirOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006f1630
// (public: virtual __thiscall WONAPI::GetMultiDirOp::~GetMultiDirOp(void)    [??1GetMultiDirOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f2060
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struct s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d9e90
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<)
// TEMPLATE: WON 0x006db280
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,stru)
// TEMPLATE: WON 0x006db380
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,stru)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f1db0
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006d9960
// (public: __thiscall std::set<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::less<class std::basic_)
