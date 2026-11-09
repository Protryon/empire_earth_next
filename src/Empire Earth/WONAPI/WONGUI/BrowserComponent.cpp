// HTML browser component: scroller, page history, anchors, links and document loading
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONURL.h"
#include "ee/wonapi/WONComponentFlags.h"
using namespace std;
namespace WONAPI {

// FUNCTION: WON 0x007FB1A0
// Attaches a scroller around the HTML view, sized to the browser, and adds it as a child
void BrowserComponent::SetScroller(Scroller *theScroller)
{
	this->mScroller = theScroller;
	if(theScroller->GetScrollArea()!=this->mHTMLView.get())
		theScroller->SetScrollArea(this->mHTMLView.get());

	this->Fn2F6140(theScroller,3,this,0,0,0,0,0); // CLI_SameSize = 3
	this->AddChild(theScroller);
}

// FUNCTION: WON 0x007FC540
// Scrolls to the remembered offset after back/forward, else to the top and then to the current anchor
void BrowserComponent::GotoAnchor()
{
	if(this->mVertOffset>=0) // this was a back or forward traversal and we were somewhere else on the page
	{
		this->mHTMLView->SetOffsets(0,this->mVertOffset);
		return;
	}

	this->mHTMLView->SetOffsets(0,0);
	if(this->mCurAnchor.length()==0)
		return;

	this->mHTMLView->GotoAnchor(this->mCurAnchor);
}

// FUNCTION: WON 0x007FC7C0
// Stores the view's vertical scroll offset in the current history entry
void BrowserComponent::RememberVertOffset()
{
	if(this->mCurPage>=0 && this->mCurPage<this->mPageVector.size()) // remember the current vert offset in this page
		this->mPageVector[this->mCurPage].mVertOffset = this->mHTMLView->mVertOffset;
}

// FUNCTION: WON 0x007FCF60
// Goes back one page in the history, restoring that page's scroll offset
void BrowserComponent::Back()
{
	if(this->mPageVector.empty() || !(this->mCurPage>0))
		return;
	this->RememberVertOffset();

	this->mCurPage--;
	PageLocation &aLoc = this->mPageVector[this->mCurPage];
	this->Go(aLoc.mLocation,false,true,aLoc.mVertOffset);
}

// FUNCTION: WON 0x007FCFC0
// Goes forward one page in the history, restoring that page's scroll offset
void BrowserComponent::Forward()
{
	if(this->mPageVector.empty() || !(this->mCurPage<this->mPageVector.size()-1))
		return;
	this->RememberVertOffset();

	this->mCurPage++;
	PageLocation &aLoc = this->mPageVector[this->mCurPage];
	this->Go(aLoc.mLocation,false,true,aLoc.mVertOffset);
}

// FUNCTION: WON 0x007FD030
// Resolves a link relative to the current page into a full path
void BrowserComponent::GetLinkPath(const std::string &theLink, std::string &thePath)
{
	URL aLink(theLink);
	string aStr = this->mCurPath;
	aLink.CalcRelativeTo(aStr);
	aLink.GetEntire(thePath);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007fb230
// Returns whether downloads are pending (the Stop button is enabled)
bool BrowserComponent::StopButtonEnabled()
{
    if(this->mHTTPSession.get()==NULL)
        return false;
    return this->mNumPendingOps>0;
}
// FUNCTION: WON 0x007fb250
// Returns whether there is a page to go back to
bool BrowserComponent::BackButtonEnabled()
{
    return !this->mPageVector.empty() && this->mCurPage>0;
}
// FUNCTION: WON 0x007fb280
// Returns whether there is a page to go forward to
bool BrowserComponent::ForwardButtonEnabled()
{
    return !this->mPageVector.empty() && this->mCurPage<this->mPageVector.size()-1;
}
// FUNCTION: WON 0x007fc480
// Clears the page history
void BrowserComponent::Reset()
{
    this->mPageVector.clear();
    this->mCurPage=0;
}
// FUNCTION: WON 0x007fc4e0
// Reloads the current page (forced, without adding a history entry)
void BrowserComponent::Refresh()
{
    if(!this->mPageVector.empty()) this->Go(this->mCurLocation,true,true);
}
// FUNCTION: WON 0x007fc510
// Cancels every pending download and signals a browser button change
void BrowserComponent::Stop()
{
    this->mHTTPSession->KillAll();
    this->mNumPendingOps=0;
    this->Fn2F5380(ComponentEvent_BrowserButtonChange,0);
}
}

#include "ee/wonapi/WONLobbyGUICompletion.h"
#include "ee/wonapi/WONLobbyHTMLDisplayGen.h"
#include "ee/wonapi/WONLobbyImageButton.h"

// FUNCTION: WON 0x007fced0
// Returns whether two paths are equal, comparing case-sensitively but treating slash and backslash alike
static bool PathsAreEqual(const WONAPI::GUIString& first,const WONAPI::GUIString& second)
{
    if(first.length()!=second.length()) return false;
    for(int i=0;i<first.length();++i)
    {
        if(first[i]==second[i])
            continue;
        int a=first[i],b=second[i];
        bool slashA=a=='\\' || a=='/';
        bool slashB=b=='\\' || b=='/';
        if(!slashA || !slashB) return false;
    }
    return true;
}

namespace WONAPI {
// FUNCTION: WON 0x007fc590
// Shows an already parsed document at the top of the view, unless its images still have to be downloaded
void BrowserComponent::Go(HTMLDocument* document)
{
    if(document==NULL) return;
    this->Stop();
    this->mHTMLDocument=document;
    SmartPtr<MultiLineElement> element=new MultiLineElement;
    if(!this->DownloadImages())
    {
        HTMLDisplayGen generator;
        generator.SetFontSize(this->mFontSize);
        generator.SetNoBackground(this->mHaveBackground);
        generator.Generate(this->mHTMLDocument.get(),element.get(),this->mHTMLView.get());
        this->mHTMLView->SetContents(element.get());
        this->mHTMLView->SetOffsets(0,0);
    }
}
// FUNCTION: WON 0x007fc800
// Navigates to a location: updates the history (at most 100 pages), jumps to an anchor on the same page,
// or loads a local file / starts an HTTP download (or hands the link to the system browser)
void BrowserComponent::Go(const GUIString& location,bool force,bool backTraversal,int vertOffset)
{
    this->Stop();
    int pound=location.find('#',0);
    GUIString path,anchor;
    if(pound>=0)
    {
        path=location.substr(0,(unsigned short)pound);
        anchor=location.substr((unsigned short)(pound+1));
    }
    else path=location;
    this->mCurAnchor=anchor;
    if(!backTraversal)
    {
        this->RememberVertOffset();
        if(!this->mPageVector.empty() && this->mCurPage<this->mPageVector.size()+1)
            this->mPageVector.erase(this->mPageVector.begin()+this->mCurPage+1,this->mPageVector.end());
        if(this->mPageVector.empty() || location!=this->mPageVector.back().mLocation)
        {
            this->mPageVector.push_back(PageLocation(location,0));
            if(this->mPageVector.size()>100)
                this->mPageVector.erase(this->mPageVector.begin(),this->mPageVector.begin()+this->mPageVector.size()-100);
        }
        this->mCurPage=this->mPageVector.size()-1;
    }
    this->mCurLocation=location;
    this->mVertOffset=vertOffset;
    this->Fn2F5380(ComponentEvent_BrowserLocationChange,0);
    this->Fn2F5380(ComponentEvent_BrowserButtonChange,0);
    if(PathsAreEqual(path,this->mCurPath) && !force)
    {
        this->GotoAnchor();
        return;
    }
    if(path.length()==0) return;
    if(this->mLaunchBrowserOnLink)
    {
        Browse(location);
        return;
    }
    URL pathURL(path);
    this->mCurPath=path;
    if(pathURL.GetType()==URLType_File)
    {
        this->mHTMLDocumentPath=pathURL.GetPathAsLocalFilePath();
        this->ParseDocument();
    }
    else if(pathURL.GetType()==URLType_HTTP)
    {
        SmartPtr<HTTPGetOp> op=new HTTPGetOp(pathURL.GetAddress(),pathURL.GetPath());
        op->SetHTTPCache(this->mHTTPCache.get());
        op->mCompletion=new OpGUICompletion(new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(BrowserComponent::DownloadDocumentCompletion,this));
        this->mHTTPSession->AddOp(op.get());
        ++this->mNumPendingOps;
        this->mHTMLView->SetContents(new MultiLineElement);
    }
}
}

#include "ee/wonapi/WONLobbyHTMLParser.h"

namespace WONAPI {
// FUNCTION: WON 0x007fc0d0
// Parses the downloaded or local HTML file, shows it unless images are pending, and goes to the anchor
void BrowserComponent::ParseDocument()
{
    if(this->mHTMLDocumentPath.empty())
    {
        this->Stop();
        return;
    }
    HTMLParser parser;
    parser.SetDocument(new HTMLDocument(this->mCurPath));
    parser.ParseFromFile(this->mHTMLDocumentPath.c_str());
    this->mHTMLDocument=parser.GetDocument();
    SmartPtr<MultiLineElement> element=new MultiLineElement;
    if(!this->DownloadImages())
    {
        HTMLDisplayGen generator;
        generator.SetFontSize(this->mFontSize);
        this->mHTMLDocument->DecodeImages(true);
        generator.SetNoBackground(this->mHaveBackground);
        generator.Generate(this->mHTMLDocument.get(),element.get(),this->mHTMLView.get());
        this->mHTMLView->SetContents(element.get());
    }
    this->GotoAnchor();
    this->mCurLocation=this->mCurPath;
    this->Fn2F5380(ComponentEvent_BrowserLocationChange,0);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007fd350
// Follows an activated hyperlink (in the system browser with Shift or when the link asks for it, unless Alt)
// and shows the hovered link's path in the status
void BrowserComponent::HandleComponentEvent(ComponentEvent* event)
{
    switch(event->mType)
    {
    case ComponentEvent_HyperLinkActivated:
        {
            HyperLinkEvent* linkEvent=static_cast<HyperLinkEvent*>(event);
            const std::string& link=linkEvent->mLinkParam->mLinkParam;
            std::string path;
            this->GetLinkPath(link,path);
            int keyMod=this->GetKeyMod();
            if((keyMod & KEYMOD_SHIFT) ||
               (linkEvent->mLinkParam->mLaunchBrowser && !(keyMod & KEYMOD_ALT)))
                Browse(path);
            else this->Go(path);
        }
        return;
    case ComponentEvent_HyperLinkOver:
        {
            HyperLinkEvent* linkEvent=static_cast<HyperLinkEvent*>(event);
            if(linkEvent->mLinkParam.get()!=NULL)
            {
                const std::string& link=linkEvent->mLinkParam->mLinkParam;
                std::string path;
                this->GetLinkPath(link,path);
                this->SetStatus(path);
            }
            else this->SetStatus("");
        }
        return;
    }
}
}
namespace WONAPI {
// FUNCTION: WON 0x007fd1f0
// Searches the page for text (next match, optionally case-sensitive); returns whether it was found
bool BrowserComponent::FindStr(const GUIString& text,bool next,bool caseSensitive)
{
    return this->mHTMLView->FindStr(text,next,caseSensitive);
}
}

#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x007fb0e0
// Creates a default scroll container with conditional scrollbars if none was set, then joins the parent
void BrowserComponent::AddedToParent()
{
    if(this->mScroller.get()==NULL)
    {
        SmartPtr<MSScrollContainer> scroller=new MSScrollContainer(this->mHTMLView.get());
        scroller->SetScrollbarConditions(ScrollbarCondition_Conditional,ScrollbarCondition_Conditional);
        this->SetScroller(scroller.get());
    }
    Container::AddedToParent();
}
}

namespace WONAPI {
// FUNCTION: WON 0x007fad10
// Creates a browser with an empty view, its own HTTP session, the global HTTP cache and medium font
BrowserComponent::BrowserComponent()
{
    this->mLaunchBrowserOnLink=false;
    this->mFontSize=HTMLFontSize_Medium;
    this->mHTTPSession=new HTTPSession;
    this->mHTTPCache=HTTPCache::GetGlobalCache();
    this->mCurPage=0;
    this->mNumPendingOps=0;
    this->mVertOffset=-1;
    this->mHTMLView=new Table;
    this->mHaveBackground=false;
}
// FUNCTION: WON 0x007faf30
// Destroys the browser (members only)
BrowserComponent::~BrowserComponent()
{
}
// FUNCTION: WON 0x007fd210
// Makes the page background transparent (no colour, no border) or keeps the document's
// The original has the older SDK's SetTransparent, not the later SetBackground/RemoveBackground
void BrowserComponent::SetTransparent(bool transparent)
{
    this->mHaveBackground=transparent;
    if(transparent) {
        Background background(-1,-1);
        this->mHTMLView->SetBackground(background);
    }
    this->mHTMLView->SetComponentFlags(ComponentFlag_GrabBG,this->mHaveBackground);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007faf10
// (public: virtual void * __thiscall WONAPI::BrowserComponent::`scalar deleting destructor'(unsigned int)    [??_GBrowserComponent@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fd650
// (public: unsigned int __thiscall std::vector<struct WONAPI::BrowserComponent::PageLocation,class std::allocator<struct WONAPI::BrowserComponent::PageLo)
// TEMPLATE: WON 0x007fd800
// (public: virtual void * __thiscall WONAPI::GUICompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp> >::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x007fd960
// (protected: bool __thiscall std::vector<struct WONAPI::BrowserComponent::PageLocation,class std::allocator<struct WONAPI::BrowserComponent::PageLocatio)
// TEMPLATE: WON 0x007fde30
// (private: virtual void __thiscall WONAPI::GUICompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp> >::Complete(class WONAPI::SmartPtr<class WONAPI::)
// TEMPLATE: WON 0x007fdf50
// (struct WONAPI::BrowserComponent::PageLocation * __cdecl std::copy(class std::_Ptrit<struct WONAPI::BrowserComponent::PageLocation,int,struct WONAPI::B)
// TEMPLATE: WON 0x007fe000
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::BrowserComponent::PageLocation,int,struct WONAPI::BrowserComponent::PageLocation *,struct WONA)
// TEMPLATE: WON 0x007fe070
// (class std::_Ptrit<struct WONAPI::BrowserComponent::PageLocation,int,struct WONAPI::BrowserComponent::PageLocation *,struct WONAPI::BrowserComponent::P)
// TEMPLATE: WON 0x007fe0f0
// (public: virtual void __thiscall WONAPI::GUICompletionEvent<class WONAPI::SmartPtr<class WONAPI::AsyncOp> >::Deliver(void))
// TEMPLATE: WON 0x007fe130
// (public: virtual void * __thiscall WONAPI::GUICompletionEvent<class WONAPI::SmartPtr<class WONAPI::AsyncOp> >::`scalar deleting destructor'(unsigned in)
// TEMPLATE: WON 0x007fe150
// (public: virtual __thiscall WONAPI::GUICompletionEvent<class WONAPI::SmartPtr<class WONAPI::AsyncOp> >::~GUICompletionEvent<class WONAPI::SmartPtr<clas)
// TEMPLATE: WON 0x007fda40
// TEMPLATE: WON 0x007fd900
// TEMPLATE: WON 0x007fd600
// TEMPLATE: WON 0x007fd670
// TEMPLATE: WON 0x007fdfc0
// TEMPLATE: WON 0x007fd840
