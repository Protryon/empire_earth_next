// GetHTTPDocumentOp download requests (host, path and per-language document names)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <time.h>
#include <list>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "platform/PlatFile.h"
// FUNCTION: WON 0x006d0e70
// Returns the document host name (www.sierra.com)
static std::string& GetHost()
{
    static std::string host="www.sierra.com";
    return host;
}
// FUNCTION: WON 0x006d0f70
// Returns the document base path (/motd/)
static std::string& GetPath()
{
    static std::string path="/motd/";
    return path;
}
namespace WONAPI {
// FUNCTION: WON 0x006cffb0
// Once per process opens the type's _won*path.txt override file but ignores it (host and path stay default)
void GetHTTPDocumentOp::CheckHostAndPath(HTTPDocType inType)
{
    static bool checked=false;
    static CriticalSection crit;
    AutoCrit guard(crit);
    if (checked) return;
    checked=true;
    // The fopen results are discarded.
    if (inType==HTTPDocType_MOTD)
        PlatFOpen((WONAPICore::GetDefaultFileDirectory()+"_wonmotdpath.txt").c_str(),"r");
    else if (inType==HTTPDocType_TOU)
        PlatFOpen((WONAPICore::GetDefaultFileDirectory()+"_wontoupath.txt").c_str(),"r");
    else if (inType==HTTPDocType_ADV)
        PlatFOpen((WONAPICore::GetDefaultFileDirectory()+"_wonadvpath.txt").c_str(),"r");
}
// FUNCTION: WON 0x006d0810
// Records a finished download and finishes the op when no download is left
bool GetHTTPDocumentOp::CallbackHook(AsyncOp* inOp,int inParam)
{
    this->OpFinished(static_cast<HTTPGetOp*>(inOp),inParam);
    if (this->mTracker.mMaps.mEntries.empty()) this->DoFinish();
    return true;
}
// FUNCTION: WON 0x006d0840
// Starts the download of the system or game copy, in the configured language or the default file
void GetHTTPDocumentOp::TryHTTPOp(HTTPDocOwner inOwner,bool inForceDefault)
{
    std::string document;
    std::string config=StringToLowerCase(this->mPath);
    if (config=="en") config.erase();
    if (config.empty() || inForceDefault) {
        this->mRetried[inOwner]=true;
        if (this->mDocumentType==HTTPDocType_MOTD) document="motd.txt";
        else if (this->mDocumentType==HTTPDocType_TOU) document="tou.txt";
        else if (this->mDocumentType==HTTPDocType_ADV) document="adv.txt";
    } else {
        if (this->mDocumentType==HTTPDocType_MOTD) document="motd_"+config+".txt";
        else if (this->mDocumentType==HTTPDocType_TOU) document="tou_"+config+".txt";
        else if (this->mDocumentType==HTTPDocType_ADV) document="adv_"+config+".txt";
    }
    std::string path;
    if (inOwner==HTTPDocOwner_Sys) path=GetPath()+"sys/";
    else if (inOwner==HTTPDocOwner_Game) path=GetPath()+StringToLowerCase(this->mProduct)+"/";
    SmartPtr<HTTPGetOp> op=new HTTPGetOp(GetHost(),path+document);
    op->SetDoRangeResume(false);
    op->SetLocalPath(this->mDocuments[inOwner]);
    if (this->mRunMode==OP_MODE_ASYNC) {
        this->Track(op.get(),inOwner);
        if (this->mSession.get()!=NULL) this->mSession->AddOp(op.get());
        else op->RunAsync(-1);
        return;
    }
    op->Run(2,this->TimeLeft());
    this->OpFinished(op.get(),inOwner);
}
}

// TEMPLATE: WON 0x006d0f20
// (GetHost static-local atexit destructor)
// TEMPLATE: WON 0x006d1020
// (GetPath static-local atexit destructor)
