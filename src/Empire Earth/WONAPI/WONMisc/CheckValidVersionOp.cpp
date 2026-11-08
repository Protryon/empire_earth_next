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
    ReadBuffer reply(this->mReplyData->data(),this->mReplyData->length());
    // Constructed and destroyed without being read (as in the original)
    std::string unusedVersion;
    std::string unusedURL;
    reply.ReadString(this->mVersionTo);
    reply.ReadString(this->mVersionToDescriptionURL);
    int count=(unsigned short)reply.ReadShort();
    for (int i=0;i<count;++i) {
        unsigned short length=reply.ReadShort();
        SmartPtr<PatchData> patch=new PatchData;
        patch->SetFromVersion(this->mVersionData->GetVersion());
        patch->SetToVersion(this->mVersionTo);
        patch->SetIsActive(true);
        patch->SetConfigName(this->mVersionData->GetConfigName());
        ReadBuffer data(reply.ReadBytes(length),length);
        patch->ReadFromBuffer(data,false);
        this->mPatchDataList.push_back(patch);
    }
    return result;
}
// FUNCTION: WON 0x006d9250
// Sends the DB proxy request with the product name, version data and patch-list flag
void CheckValidVersionOp::RunHook()
{
    this->mMessageType=1004;
    this->mSubMessageType=this->mMsgType;
    WriteBuffer request(0,true);
    const std::string& aProductName=this->mProductName;
    request.AppendString(aProductName);
    this->mVersionData->WriteToBuffer(request,0);
    request.AppendBool(this->mGetPatchList);
    this->mRequestData=request.ToByteBuffer(true);
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
