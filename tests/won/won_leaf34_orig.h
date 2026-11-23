// BrowserComponent, ListAreaToolTip and StringLocalize::GetStr (and below the paints) as they were (WONGUI/BrowserComponent.cpp,
// ListAreaToolTip.cpp and StringLocalize.cpp at ab1b33bcb, the statements copied; the classes renamed Browser and Tip,
// their declarations as WONLobbyGUI.h had them), over won_leaf34_world.h.
namespace LfOrig {
using namespace Lf;
class Browser : public Container {
public:
    Browser();
    virtual ~Browser();
    virtual void AddedToParent();
    void SetTransparent(bool);
    void SetScroller(Scroller*);
    void SetFontSize(HTMLFontSize size) { this->mFontSize=size; }
    void Reset();
    void Refresh();
    void Go(const GUIString&,bool=false,bool=false,int=-1);
    void ShowFiles(const std::string& inFirst,const std::string& inSecond,bool inSkipFirstCharIfText);
    TextArea* GetHTMLView() { return this->mHTMLView.get(); }
    const GUIString& GetCurStatus() { return this->mCurStatus; }
    void SetPageText(const std::wstring&);
    SmartPtr<TextArea> mHTMLView;
    SmartPtr<Scroller> mScroller;
    HTMLFontSize mFontSize;
    bool mHaveBackground;
    GUIString mCurStatus;
    GUIString mCurLocation;
};
// Creates the view: an empty, word-wrapped, read-only text area
Browser::Browser()
{
    this->mFontSize=HTMLFontSize_Medium;
    this->mHaveBackground=false;
    this->mHTMLView=new TextArea;
    this->mHTMLView->SetTextFlags(TextFlag_ScrollOnBottom,false);
}
Browser::~Browser() {}
// Attaches a scroller around the text, sized to the view, and adds it as a child
void Browser::SetScroller(Scroller *theScroller)
{
    this->mScroller = theScroller;
    if(theScroller->GetScrollArea()!=this->mHTMLView.get())
        theScroller->SetScrollArea(this->mHTMLView.get());
    this->Fn2F6140(theScroller,3,this,0,0,0,0,0); // CLI_SameSize = 3
    this->AddChild(theScroller);
}
// Adds a default scroller (scroll bars when needed) if the layout gave none
void Browser::AddedToParent()
{
    if(this->mScroller.get()==NULL)
    {
        SmartPtr<ScrollbarScroller> scroller=static_cast<ScrollbarScroller*>(WONShellNew(EeWonShellKind::MsScrollContainer,(intptr_t)static_cast<ScrollArea*>(this->mHTMLView.get())));
        scroller->SetScrollbarConditions(ScrollbarCondition_Conditional,ScrollbarCondition_Conditional);
        this->SetScroller(scroller.get());
    }
    Container::AddedToParent();
}
// Lets the parent's background show through the text
void Browser::SetTransparent(bool transparent)
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
void Browser::SetPageText(const std::wstring& inText)
{
    this->mHTMLView->Clear();
    if(!inText.empty())
        this->mHTMLView->AddFormatedText(GUIString(inText),false);
    this->mHTMLView->SetOffsets(0,0);
    this->Invalidate();
}
// Empties the view
void Browser::Reset()
{
    this->mCurLocation=GUIString();
    this->SetPageText(std::wstring());
}
// Shows the page at a location: a file's text, or a web address as text
void Browser::Go(const GUIString& inLocation,bool,bool,int)
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
void Browser::ShowFiles(const std::string& inFirst,const std::string& inSecond,bool inSkipFirstCharIfText)
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
void Browser::Refresh()
{
    if(!this->mCurLocation.empty())
        this->Go(this->mCurLocation);
}

class Tip : public Component {
public:
    Tip(ListArea*);
    virtual void Paint(Graphics&);
    void DoTip(ListItem*);
    void EndTip();
    ListArea* mListArea;
    SmartPtr<ListItem> mItem;
};
// Creates the tooltip of a list box, showing no item
Tip::Tip(ListArea* area) {
    this->mListArea=area;
    this->mItem=NULL;
}
// Draws the tipped list item in full on a tooltip background with a tooltip-coloured border, using the
// list box's colour scheme unless the tooltip has its own
void Tip::Paint(Graphics& g) {
    Component::Paint(g);
    if(this->mItem.get()==NULL) return;
    ColorScheme* scheme=g.mColorScheme;
    if(this->GetOwnColorScheme()==NULL) g.mColorScheme=this->mListArea->GetColorScheme();
    int x=this->mListArea->GetLeftPad(),y=-this->mItem->mTop+2;
    g.SetSysColor(StandardColor_ToolTipBack);
    g.FillRect(0,0,this->Width(),this->Height());
    g.PushClipRect(0,2,this->Width(),this->Height()-4);
    g.Translate(x,y);
    this->mListArea->ScrollPaint(g);
    g.Translate(-x,-y);
    g.PopClipRect();
    g.SetSysColor(StandardColor_ToolTipText);
    g.DrawRect(0,0,this->Width(),this->Height());
    g.mColorScheme=scheme;
}
// Opens the tooltip over a list item, at least as wide as the list box; closes the old tip first unless
// the same item is already shown, and stops there for no item
void Tip::DoTip(ListItem* item) {
    if(this->mItem.get()==item && this->mItem.get()!=NULL && this->mParent!=NULL) return;
    this->EndTip();
    this->mItem=item;
    if(this->mItem.get()==NULL) return;
    int x=0,y=this->mListArea->GetTopPad()-this->mListArea->GetVertOffset();
    this->mListArea->LocalToRoot(x,y);
    y+=this->mItem->mTop-2;
    PopupParams params(this,0x6247,this->mListArea,x,y);
    int width=this->mItem->mWidth+this->mListArea->GetLeftPad()+this->mListArea->GetRightPad();
    if(width<this->mListArea->Width()) width=this->mListArea->Width();
    this->SetSize(width,this->mItem->mHeight+4);
    this->mListArea->GetRoot()->OpenPopup(params);
}
// Closes the tooltip popup if it is open
void Tip::EndTip() {
    if(this->mParent!=NULL) this->mListArea->GetRoot()->ClosePopup(this);
}

GUIString GetStr(const GUIString &theStr, const GUIString **subs, int numSubs)
{
	GUIString aNewStr;
	int aStartPos = 0;
	while(true)
	{
		int aBracketPos = theStr.find('<',aStartPos);
		if(aBracketPos<0)
			break;

		aNewStr.append(theStr,aStartPos,aBracketPos-aStartPos);
		int aNextChar = theStr[aBracketPos+1];
		if(aNextChar=='<') // literal '<' character
		{
			aNewStr.append('<');
			aStartPos = aBracketPos+2;
			continue;
		}
		else if(aBracketPos+1<theStr.length()) // substitution
		{
			int aNextNextChar = theStr[aBracketPos+2];
			int aSubId = aNextNextChar - '0' - 1;
			if(tolower(aNextChar)=='s' && aSubId>=0 && aSubId<numSubs && subs[aSubId]!=NULL)
				aNewStr.append(*(subs[aSubId]));
		}

		aStartPos = aBracketPos+1;
		aBracketPos = theStr.find('>',aStartPos); // find ending bracket
		if(aBracketPos<0)
			break;
			
		aStartPos = aBracketPos+1;
	}

	if(aStartPos < theStr.length())
		aNewStr.append(theStr,aStartPos);

	return aNewStr;
}
} // namespace LfOrig

// ImageButton, TextListItem and MenuItem as they were (ImageButton.cpp, TextListItem.cpp, MenuBehavior.cpp at
// ab1b33bcb, their measures and paints; Graphics and ListArea are the world's PaintGraphics and PaintList)
namespace LfOrig {
using namespace Lf;
#include "won_leaf34_paint_classes.h"
void ImageButton::GetDesiredSize(int& outWidth,int& outHeight) {
    int width=0,height=0;
    for(int i=0;i<5;++i) {
        Image* image=this->mImages[i].get();
        if(image) {
            if(image->GetWidth()>width) width=image->GetWidth();
            if(image->GetHeight()>height) height=image->GetHeight();
        }
    }
    outWidth=width;
    outHeight=height;
}

void ImageButton::Paint(PaintGraphics& g) {
    Component::Paint(g);
    Image* image=NULL;
    if(this->IsDisabled()) image=this->mImages[ImageButtonType_Disabled].get();
    else if(this->IsChecked() && this->mImages[ImageButtonType_Checked].get())
        image=this->mImages[ImageButtonType_Checked].get();
    else if(this->IsPressed()) image=this->mImages[ImageButtonType_Down].get();
    else if(this->IsMouseOver()) image=this->mImages[ImageButtonType_Over].get();
    if(!image) image=this->mImages[ImageButtonType_Normal].get();
    if(!image) return;
    if(this->mStretch && (image->GetWidth()!=this->Width() || image->GetHeight()!=this->Height()))
        image->SetSize(this->Width(),this->Height());
    g.DrawImage(image,0,0);
}

void TextListItem::CalcDimensions(PaintList* inList)
{
    if(this->mFont.get()==NULL) this->mFont=inList->mFont;
    if(this->mColor==-1) this->mColor=inList->GetTextColor();
    Image* i=this->mImage.get(); // the sizes are guidraw.rs text_item_size
    ee_won_gui_text_item_size(this->mFont->GetStringWidth(this->mText),this->mFont->GetHeight(),i!=NULL,i?i->GetWidth():0,
                              i?i->GetHeight():0,false,&this->mWidth,&this->mHeight);
}

void TextListItem::Paint(PaintGraphics& inGraphics,PaintList* inList)
{
    // the selection behind a selected item, the image, the text centred on the row (guidraw.rs text_row)
    Image* anImage=this->mImage.get();
    EeWonTextRowPaint r;
    r.selected=this->mSelected;
    r.full_row_select=inList->ListFlagSet(0x10);
    r.no_image_select=inList->ListFlagSet(0x20);
    r.has_image=anImage!=NULL;
    r.sel=WONDrawSelOf(inList->GetSelColor());
    r.color=(uint32_t)this->mColor;
    r.paint_width=inList->GetPaintWidth();
    r.width=this->mWidth;
    r.height=this->mHeight;
    r.image_width=anImage ? anImage->GetWidth() : 0;
    r.image_height=anImage ? anImage->GetHeight() : 0;
    r.font_height=this->mFont->GetHeight();
    WONDrawSlots aSlots;
    aSlots.mImages[0]=anImage;
    aSlots.mFont=this->mFont.get();
    aSlots.mString=&this->mText;
    WONDrawOps anOps([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_text_row(&r,o,n); });
    WONDrawRun(inGraphics,anOps,aSlots);
}

void MenuItem::CalcDimensions(PaintList* area)
{
    TextListItem::CalcDimensions(area);
    ee_won_gui_menu_item_size(&this->mWidth,&this->mHeight);
}

void MenuItem::Paint(PaintGraphics& g,PaintList* area)
{
    // guidraw.rs menu_item
    Image* anImage=this->mImage.get();
    EeWonMenuItemPaint r;
    r.selected=this->mSelected;
    r.full_row_select=area->ListFlagSet(0x10);
    r.has_image=anImage!=NULL;
    r.disabled=this->mDisabled;
    r.has_submenu=this->mSubMenu.get()!=NULL;
    r.sel=WONDrawSelOf(area->mSelColor);
    r.color=(uint32_t)this->mColor;
    r.paint_width=area->GetPaintWidth();
    r.width=this->mWidth;
    r.height=this->mHeight;
    r.image_height=anImage ? anImage->GetHeight() : 0;
    r.font_height=this->mFont->GetHeight();
    r.view_width=area->GetViewWidth();
    WONDrawSlots aSlots;
    aSlots.mImages[0]=anImage;
    aSlots.mFont=this->mFont.get();
    aSlots.mString=&this->mText;
    WONDrawOps anOps([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_menu_item(&r,o,n); });
    WONDrawRun(g,anOps,aSlots);
}

// Background::Paint as it was (Background.cpp at ab1b33bcb)
void Background::Paint(PaintGraphics &g, int xpos, int ypos, int width, int height, int xoff, int yoff)
{
	// guidraw.rs background: the stretched image, or the fill, border, tiles and component
	EeWonBackground b;
	b.has_image = this->mImage.get()!=NULL;
	b.stretch_image = this->mStretchImage;
	b.use_offsets = this->mUseOffsets;
	b.has_component = this->mComponent.get()!=NULL;
	b.image_width = b.has_image ? this->mImage->GetWidth() : 0;
	b.image_height = b.has_image ? this->mImage->GetHeight() : 0;
	b.color = this->mColor;
	b.border_color = this->mBorderColor;
	b.x = xpos;
	b.y = ypos;
	b.width = width;
	b.height = height;
	b.x_offset = xoff;
	b.y_offset = yoff;
	WONDrawSlots aSlots;
	aSlots.mImages[0] = this->mImage.get();
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_background(&b,o,n); });
	WONDrawRun(g,anOps,aSlots,[&](const EeWonDrawOp& o) {
		this->mComponent->SetSize(o.b,o.c);
		this->mComponent->Invalidate();
		this->mComponent->Paint(g);
	});
}
} // namespace LfOrig
