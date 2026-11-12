// CheckValidVersionOp: asks the DB proxy server whether the product version is current and for its patches
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONPatchMsg.h"
namespace WONAPI {
// FUNCTION: WON 0x006d8c80
// Creates a version check for the named product against the given servers
CheckValidVersionOp::CheckValidVersionOp(const std::string& inProduct,ServerContext* inContext)
    : DBProxyOp(inContext),mProductName(inProduct),mMsgType(1)
{
    this->Init();
}
// FUNCTION: WON 0x006d8d90
// Destroys the op (members only)
CheckValidVersionOp::~CheckValidVersionOp()
{
}
// FUNCTION: WON 0x006d8f60
// Reads the latest version, its description URL and the patch list from an out-of-date/valid reply; returns the status
WONStatus CheckValidVersionOp::CheckResponse()
{
    WONStatus result=DBProxyOp::CheckResponse();
    if (result<WS_DBProxyServ_ValidNotLatest || (result>WS_DBProxyServ_OutOfDate && result!=WS_Success))
        return result;
    if (this->mSubMessageReplyType!=this->mMsgType+1) return this->InvalidReplyHeader();
    PatchReplyData<PatchData>(this->mReplyData.get(),this->mVersionData.get(),&this->mVersionTo,
                              &this->mVersionToDescriptionURL,this->mPatchDataList);
    return result;
}
// FUNCTION: WON 0x006d9250
// Sends the DB proxy request with the product name, version data and patch-list flag
void CheckValidVersionOp::RunHook()
{
    this->mMessageType=1004;
    this->mSubMessageType=this->mMsgType;
    std::vector<std::string> aStrings;
    aStrings.push_back(this->mProductName);
    aStrings.push_back(this->mVersionData->GetConfigName());
    aStrings.push_back(this->mVersionData->GetVersion());
    const VersionTypeList& aTypes=this->mVersionData->GetVersionTypeList();
    for (VersionTypeList::const_iterator i=aTypes.begin();i!=aTypes.end();++i) aStrings.push_back(*i);
    this->mRequestData=PatchRequest(0,aStrings,this->mGetPatchList);
    DBProxyOp::RunHook();
}
// FUNCTION: WON 0x006d9380
// Resets the op with fresh version data, no target version and the patch list requested
void CheckValidVersionOp::Init()
{
    this->Reset();
    this->mLengthFieldSize=4;
    this->mVersionData=new VersionData;
    this->mVersionData->SetState(VersionState_BadState);
    this->mVersionTo="";
    this->mVersionToDescriptionURL="";
    this->mGetPatchList=true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d8d70
// (protected: virtual void * __thiscall WONAPI::CheckValidVersionOp::`scalar deleting destructor'(unsigned int)    [??_GCheckValidVersionOp@WONAPI@@MAEPA)
// TEMPLATE: WON 0x006dc810
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::PatchData>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::PatchDa)
