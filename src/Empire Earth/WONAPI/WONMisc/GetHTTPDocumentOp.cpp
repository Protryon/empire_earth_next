// GetHTTPDocumentOp: downloads the WON system and game MOTD, terms of use or ad documents (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdio.h>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x006d0170
// Creates a document download for the product and document type
GetHTTPDocumentOp::GetHTTPDocumentOp(const std::string& inProduct, HTTPDocType inType)
{
    this->mProduct = inProduct;
    this->SetDocType(inType);
}

// FUNCTION: WON 0x006d0340
// Sets the document type, its hosts and paths, and the local cache file names for the system and game copies
void GetHTTPDocumentOp::SetDocType(HTTPDocType inType)
{
    this->mDocumentType = inType;
    GetHTTPDocumentOp::CheckHostAndPath(inType);
    if (this->mDocumentType == HTTPDocType_MOTD) {
        this->mDocuments[HTTPDocOwner_Sys] = WONAPICore::GetDefaultFileDirectory() + "_wonsysmotd.txt";
        this->mDocuments[HTTPDocOwner_Game] = WONAPICore::GetDefaultFileDirectory() + "_won" + this->mProduct + "motd.txt";
    } else if (this->mDocumentType == HTTPDocType_TOU) {
        this->mDocuments[HTTPDocOwner_Sys] = WONAPICore::GetDefaultFileDirectory() + "_wonsystou.txt";
        this->mDocuments[HTTPDocOwner_Game] = WONAPICore::GetDefaultFileDirectory() + "_won" + this->mProduct + "tou.txt";
    } else if (this->mDocumentType == HTTPDocType_ADV) {
        this->mDocuments[HTTPDocOwner_Sys] = WONAPICore::GetDefaultFileDirectory() + "_wonsysadv.txt";
        this->mDocuments[HTTPDocOwner_Game] = WONAPICore::GetDefaultFileDirectory() + "_won" + this->mProduct + "adv.txt";
    }
}

// FUNCTION: WON 0x006d06c0
// Finishes with system-not-found, game-not-found or success from the two download results
void GetHTTPDocumentOp::DoFinish()
{
    if (this->mDocumentStatus[HTTPDocOwner_Sys] != 0 && this->mDocumentType != HTTPDocType_ADV)
        this->Finish(WS_GetMOTD_SysNotFound);
    else if (this->mDocumentStatus[HTTPDocOwner_Game] != 0)
        this->Finish(WS_GetMOTD_GameNotFound);
    else
        this->Finish(WS_Success);
}

// FUNCTION: WON 0x006d0700
// Records one download's result (retrying once on a bad redirect or 404); for a MOTD reads the is-new flag
void GetHTTPDocumentOp::OpFinished(HTTPGetOp* inOp, int inOwner)
{
    if (!this->mRetried[inOwner] && (inOp->mStatus == WS_HTTP_InvalidRedirect || inOp->mHTTPStatus == 404)) {
        this->TryHTTPOp(static_cast<HTTPDocOwner>(inOwner),true);
        return;
    }
    this->mDocumentStatus[inOwner] = inOp->mStatus;
    this->mHTTPStatus[inOwner] = inOp->mHTTPStatus;
    this->mLastModified[inOwner] = inOp->GetLastModifiedTime();
    if (inOp->mStatus != WS_Success) return;
    this->mIsNew[inOwner] = inOp->mContentIsNew;
    if (this->mDocumentType != HTTPDocType_MOTD) return;
    FILE* file = fopen(this->mDocuments[inOwner].c_str(),"r");
    if (file != NULL) {
        int first = fgetc(file);
        if (first == EOF) {
            this->mIsNew[inOwner] = false;
            fclose(file);
            return;
        }
        if (toupper(first & 0xff) != '1') this->mIsNew[inOwner] = true;
    }
    // The original also calls fclose when fopen failed (NULL)
    fclose(file);
}

// FUNCTION: WON 0x006d1070
// Clears the results and starts the downloads (game copy only for ads); blocking runs finish at once
void GetHTTPDocumentOp::RunHook()
{
    for (int i=0; i<HTTPDocOwner_Count; ++i) {
        this->mIsNew[i] = false;
        this->mDocumentStatus[i] = 100004;
        this->mHTTPStatus[i] = 0;
        this->mLastModified[i] = 0;
        this->mRetried[i] = false;
    }
    if (this->mDocumentType == HTTPDocType_ADV) this->TryHTTPOp(HTTPDocOwner_Game,false);
    else for (int owner=0; owner<HTTPDocOwner_Count; ++owner)
        this->TryHTTPOp(static_cast<HTTPDocOwner>(owner),false);
    if (this->mRunMode != OP_MODE_ASYNC) this->DoFinish();
}

// FUNCTION: WON 0x006d10e0
// Returns the cached document as a NUL-terminated buffer (a MOTD drops its leading flag character)
ByteBufferPtr GetHTTPDocumentOp::GetDocument(HTTPDocOwner inOwner) const
{
    FILE* file = NULL;
    if (!this->mDocuments[inOwner].empty()) file = fopen(this->mDocuments[inOwner].c_str(), "r");
    if (file == NULL) return new ByteBuffer("");
    WriteBuffer buffer(0,true);
    if (this->mDocumentType == HTTPDocType_MOTD) {
        int first = fgetc(file);
        if (first == '<') ungetc(first,file);
    }
    char bytes[1024];
    while (!feof(file)) {
        int count = fread(bytes,1,1024,file);
        if (count>0) buffer.AppendBytes(bytes,count);
    }
    buffer.AppendByte(0);
    fclose(file);
    return buffer.ToByteBuffer(true);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cfe30
// (public: virtual void * __thiscall WONAPI::AsyncOpWithTracker::`scalar deleting destructor'(unsigned int)    [??_GAsyncOpWithTracker@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006d0320
// (public: virtual void * __thiscall WONAPI::GetHTTPDocumentOp::`scalar deleting destructor'(unsigned int)    [??_GGetHTTPDocumentOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d9d20
// (public: __thiscall WONAPI::AsyncOpTrackerMaps::AsyncOpTrackerMaps(bool,bool))
// TEMPLATE: WON 0x00769650
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,class std)
