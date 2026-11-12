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
#include "ee/wonapi/WONDirRequest.h"
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
    :ServerRequestOp(inContext)
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
    // the message's bytes are Rust's (ee_won dirops.rs)
    DirRequestFields aFields(1, true);
    for(GetEntityRequestList::iterator i=this->mRequestList.begin();i!=this->mRequestList.end();++i)
        aFields.AddQuery((*i)->GetFlags(),(*i)->GetPath(),(*i)->GetDataTypes());
    this->mRequest=aFields.Build(this->mLengthFieldSize);
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
    // the accounting is Rust's (ee_won dirops.rs)
    int32_t aFailed=this->mNumFailedRequests;
    if(ee_won_dir_multi_step((*this->mCurRequest)->GetStatus(),&aFailed))
        ++this->mCurRequest;
    this->mNumFailedRequests=aFailed;
    if(this->mCurRequest==this->mRequestList.end())
        return (WONStatus)ee_won_dir_multi_end(this->mNumFailedRequests,this->mRequestList.size());
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
