// Browser downloads: document and image completions and image download scheduling
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyHTMLDisplayGen.h"
#include "ee/wonapi/WONURL.h"
#include "ee/wonapi/WONLobbyGUICompletion.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// FUNCTION: WON 0x007fb2c0
// Sets the browser's status text and signals a status change
void BrowserComponent::SetStatus(const GUIString& status)
{
    this->mCurStatus=status;
    this->Fn2F5380(ComponentEvent_BrowserStatusChange,0);
}

// FUNCTION: WON 0x007fb320
// Finishes a page download: reports it in the status and parses the file, or on failure updates the buttons
// (and sets the same status again)
void BrowserComponent::DownloadDocumentCompletion(AsyncOpPtr op,SmartPtr<RefCount> that)
{
    BrowserComponent* browser=(BrowserComponent*)that.get();
    HTTPGetOp* request=(HTTPGetOp*)op.get();
    if(request->mKilled)
        return;
    char status[512];
    sprintf(status,"Got %s%s - %s %d",request->GetHost().c_str(),
        request->GetRemotePath().c_str(),WONStatusToString(request->GetStatus()),request->GetHTTPStatus());
    browser->SetStatus(status);
    --browser->mNumPendingOps;
    if(request->GetStatus()!=WS_Success) {
        browser->Fn2F5380(ComponentEvent_BrowserButtonChange,0);
        browser->SetStatus(status);
        return;
    }
    browser->mHTMLDocumentPath=request->GetLocalPath();
    browser->mCurPath="http://"+request->GetEndingHost()+request->GetEndingPath();
    browser->ParseDocument();
}

// FUNCTION: WON 0x007fb6a0
// Decodes the downloaded images, rebuilds the page view, clears the status and cancels remaining downloads
void BrowserComponent::FinishImages()
{
    this->mHTMLDocument->DecodeImages(false);
    SmartPtr<MultiLineElement> element=new MultiLineElement;
    HTMLDisplayGen generator;
    generator.SetFontSize(this->mFontSize);
    generator.SetNoBackground(this->mHaveBackground);
    generator.Generate(this->mHTMLDocument.get(),element.get(),this->mHTMLView.get());
    this->mHTMLView->SetContents(element.get());
    this->SetStatus("");
    this->mHTTPSession->KillAll();
    this->mNumPendingOps=0;
    this->Fn2F5380(ComponentEvent_BrowserButtonChange,0);
}

// FUNCTION: WON 0x007fb930
// Finishes one image download: decodes it (replacing a cached copy on a 2xx reply), reports it,
// and finishes the page when it was the last pending download
void BrowserComponent::DownloadImageCompletion(AsyncOpPtr op,ImageParamPtr param)
{
    HTTPGetOp* request=(HTTPGetOp*)op.get();
    char status[1024];
    if(request->mKilled) {
        sprintf(status,"Killed %s%s.",request->GetHost().c_str(),request->GetRemotePath().c_str());
        param->mBrowser->SetStatus(status);
        return;
    }
    int httpStatus=request->GetHTTPStatus();
    HTMLDocument::LoadedImageStruct* image=param->mImageStruct;
    if(request->Succeeded()) {
        if(image->mImage.get()!=NULL && httpStatus>=200 && httpStatus<300)
            image->mImage=NULL;
        if(image->mImage.get()==NULL)
            param->mBrowser->mHTMLDocument->DoDecodeImage(param->mImageStruct,
                param->mBrowser->GetRoot(),request->GetLocalPath());
    }
    sprintf(status,"Got %s%s - %s %d",request->GetHost().c_str(),request->GetRemotePath().c_str(),
        WONStatusToString(request->GetStatus()),httpStatus);
    param->mBrowser->SetStatus(status);
    int pending=--param->mBrowser->mNumPendingOps;
    if(pending<=0)
        param->mBrowser->FinishImages();
}

// FUNCTION: WON 0x007fbc10
// Loads the page's images: local files at once, HTTP ones from the cache and by download; returns true when
// nothing is pending (the page is then finished)
bool BrowserComponent::DownloadImages()
{
    Window* window=this->GetRoot();
    HTMLDocument::LoadedImageMap& images=this->mHTMLDocument->GetLoadedImageMap();
    HTMLDocument::LoadedImageMap::iterator i=images.begin();
    while(i!=images.end()) {
        HTMLDocument::LoadedImageStruct& image=i->second;
        URL path(i->first);
        URL base(this->mCurPath);
        path.CalcRelativeTo(base);
        if(path.GetType()==URLType_File)
            this->mHTMLDocument->DoDecodeImage(&image,window,path.GetPathAsLocalFilePath());
        else if(path.GetType()==URLType_HTTP) {
            SmartPtr<HTTPGetOp> request=new HTTPGetOp(path.GetAddress(),path.GetPath());
            SmartPtr<HTTPCacheEntry> entry=this->mHTTPCache->GetEntry(path.GetAddress()+path.GetPath());
            if(entry->IsValid())
                this->mHTMLDocument->DoDecodeImage(&image,window,entry->GetLocalPath());
            request->SetHTTPCache(this->mHTTPCache.get());
            request->mCompletion=new OpGUICompletion(new ParamCompletion<AsyncOpPtr,ImageParamPtr>(
                BrowserComponent::DownloadImageCompletion,new ImageParam(this,&image)));
            this->mHTTPSession->AddOp(request.get());
            ++this->mNumPendingOps;
        }
        ++i;
    }
    if(this->mNumPendingOps<=0) {
        this->FinishImages();
        return true;
    }
    return false;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fd6f0
// (public: __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<struct WONAPI::BrowserComponent::Image)
// TEMPLATE: WON 0x007fd9c0
// (protected: static struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImageSt)
// TEMPLATE: WON 0x007fd750
// TEMPLATE: WON 0x007fd820
// TEMPLATE: WON 0x007fd8a0
// TEMPLATE: WON 0x007fd9a0
// TEMPLATE: WON 0x007fd9e0
