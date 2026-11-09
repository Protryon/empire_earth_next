// The lobby's page view (layout type BrowserComponent): the message of the day, the terms of use and the help and
// news tabs. WON's HTML rendering is not ported (docs/RUST_PLAN.md D8): a page is shown as plain text in a TextArea,
// HTML pages with their tags dropped and entities decoded by ee_won (rust/crates/ee_won/src/plaintext.rs). There are
// no links, so no history; a web address is shown as text (the WON servers are gone).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONURL.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "ee/wonapi/WONLobbyGUICompletion.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "platform/PlatFile.h"
namespace WONAPI {
// Creates the view: an empty, word-wrapped, read-only text area
BrowserComponent::BrowserComponent()
{
    this->mFontSize=HTMLFontSize_Medium;
    this->mHaveBackground=false;
    this->mHTMLView=new TextArea;
    this->mHTMLView->SetTextFlags(TextFlag_ScrollOnBottom,false);
}
BrowserComponent::~BrowserComponent() {}
// Attaches a scroller around the text, sized to the view, and adds it as a child
void BrowserComponent::SetScroller(Scroller *theScroller)
{
    this->mScroller = theScroller;
    if(theScroller->GetScrollArea()!=this->mHTMLView.get())
        theScroller->SetScrollArea(this->mHTMLView.get());
    this->Fn2F6140(theScroller,3,this,0,0,0,0,0); // CLI_SameSize = 3
    this->AddChild(theScroller);
}
// Adds a default scroller (scroll bars when needed) if the layout gave none
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
// Lets the parent's background show through the text
void BrowserComponent::SetTransparent(bool transparent)
{
    this->mHaveBackground=transparent;
    if(transparent) {
        Background background(-1,-1);
        this->mHTMLView->SetBackground(background);
    }
    this->mHTMLView->SetComponentFlags(ComponentFlag_GrabBG,this->mHaveBackground);
}
// the page text ee_won made, as a wide string
static void AppendText(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring part;
    WonWideSink(&part,inUnits,inCount);
    *static_cast<std::wstring*>(inContext)+=part;
}
// Reads a page file's text (HTML converted); false when the file cannot be read
static bool ReadPage(const std::string& inPath, bool inSkipFirstCharIfText, std::wstring& outText)
{
    FILE* file=PlatFOpen(inPath.c_str(),"rb");
    if(file==NULL)
        return false;
    std::string bytes;
    char buffer[4096];
    size_t n;
    while((n=fread(buffer,1,sizeof(buffer),file))>0)
        bytes.append(buffer,n);
    fclose(file);
    ee_won_page_text(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.length(),inSkipFirstCharIfText,AppendText,
                     &outText);
    return true;
}
// Shows text from the top
void BrowserComponent::SetPageText(const std::wstring& inText)
{
    this->mHTMLView->Clear();
    if(!inText.empty())
        this->mHTMLView->AddFormatedText(GUIString(inText),false);
    this->mHTMLView->SetOffsets(0,0);
    this->Invalidate();
}
// Empties the view
void BrowserComponent::Reset()
{
    this->mCurLocation=GUIString();
    this->SetPageText(std::wstring());
}
// Shows the page at a location: a file's text, or a web address as text
void BrowserComponent::Go(const GUIString& inLocation,bool,bool,int)
{
    this->mCurLocation=inLocation;
    std::string location=inLocation;
    std::wstring text;
    URL url(location);
    if(url.GetType()==URLType_HTTP || url.GetType()==URLType_FTP)
        text=std::wstring(GUIString(location+"\n\n(a web page: the lobby shows no web pages)"));
    else if(!ReadPage(url.GetType()==URLType_File ? url.GetPathAsLocalFilePath() : location,false,text))
        text=std::wstring(GUIString("(no page: "+location+")"));
    this->SetPageText(text);
}
// Shows the readable ones of up to two page files, one after the other with a blank line between them (the MOTD
// files start with an "is new" flag character, dropped from text pages)
void BrowserComponent::ShowFiles(const std::string& inFirst,const std::string& inSecond,bool inSkipFirstCharIfText)
{
    std::wstring first,second;
    bool haveFirst=!inFirst.empty() && ReadPage(inFirst,inSkipFirstCharIfText,first);
    bool haveSecond=!inSecond.empty() && ReadPage(inSecond,inSkipFirstCharIfText,second);
    std::wstring text=first;
    if(haveFirst && haveSecond && !first.empty() && !second.empty())
        text+=L"\n\n";
    text+=second;
    this->SetPageText(text);
}
// Shows the current location again
void BrowserComponent::Refresh()
{
    if(!this->mCurLocation.empty())
        this->Go(this->mCurLocation);
}
}
