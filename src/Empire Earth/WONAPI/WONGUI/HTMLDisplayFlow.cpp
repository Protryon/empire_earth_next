// HTML display generator: turns a document's tag list into line segments for a display (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <ctype.h>
#include <cctype>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyHTMLDisplayGen.h"
#include "ee/wonapi/WONLobbyHTMLAnchorSegment.h"
#include "ee/wonapi/WONLobbyHTMLSegments.h"
#include "ee/wonapi/WONLobbyHTMLImageSegments.h"
#include "ee/wonapi/WONLobbyHTMLElementSegments.h"
#include "ee/wonapi/WONLobbyAnimationComponent.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyMSBorderPainters.h"
#include "ee/wonapi/WONLobbyTextLinkSegment.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
inline int safe_isspace(unsigned long ch) { return ch<256?isspace(ch):0; }
inline void HTMLDisplayGen::HandlePreSpace(int ch,GUIString& text)
{
    if(ch=='\n')
    {
        this->AddDisplaySegment(this->GetTextLineSegment(text,this->mCurColor,this->mCurFont.get()),8);
        this->ClearAccumulatedSegment(8);
        text.erase();
    }
    else text.append(L' ');
}
// FUNCTION: WON 0x00737f50
// Creates an HTML display generator with no tag list, backgrounds enabled and the medium font size
// (??0HTMLDisplayGen@WONAPI@@QAE@XZ)
// FUNCTION: WON 0x00737ff0
// Static initializer: constructs the global default font face and registers its destructor
// (_$E77)
// FUNCTION: WON 0x00738080
// Registers the global default font face's destructor with atexit
// (_$E76)
// FUNCTION: WON 0x00738090
// Destroys the global default font face string at exit (default "Times New Roman")
// (_$E75)
static std::string gGlobalDefaultFontFace("Times New Roman");
// FUNCTION: WON 0x007380e0
// Sets the font face every later generated HTML document starts with
void HTMLDisplayGen::SetGlobalDefaultFontFace(const std::string& face)
{
    gGlobalDefaultFontFace=face;
}
// FUNCTION: WON 0x00738270
// Returns the next tag of the body and advances, or NULL at the end of the body
HTMLTag* HTMLDisplayGen::GetNextTag()
{
    if(this->mTagListItr==this->mTagListEnd) return NULL;
    return (this->mTagListItr++)->get();
}
// FUNCTION: WON 0x00738290
// Updates the empty-line state for a segment: breaks before/after start a fresh empty line, content marks it used
void HTMLDisplayGen::CalcLineEmpty(int flags,bool hasContent)
{
    if(flags&4)
    {
        this->mPrevLineEmpty=this->mCurLineEmpty;
        this->mCurLineEmpty=true;
        this->mLastWasSpace=true;
    }
    if(hasContent) this->mCurLineEmpty=false;
    if(!(flags&8))
        return;
    this->mPrevLineEmpty=this->mCurLineEmpty;
    this->mCurLineEmpty=true;
    this->mLastWasSpace=true;
}
// FUNCTION: WON 0x007382d0
// Adds a visible segment, or glues it to the previous one into a non-breaking line while a word continues
void HTMLDisplayGen::AddDisplaySegment(LineSegment* segment,int flags)
{
    if(this->mCanBreak)
    {
        this->ClearAccumulatedSegment();
        if(flags!=0) this->mDisplay->AddSegment(segment,(unsigned short)flags);
        else this->mPrevSegment=segment;
        this->CalcLineEmpty(flags,true);
    }
    else
    {
        if(this->mCurLine.mLine.get()==NULL)
        {
            this->mCurLine.StartBuild(new LineElement);
            this->mCurLine.mLine->mIsGlueLine=true;
        }
        if(this->mPrevSegment.get()!=NULL) this->mCurLine.AddSegment(this->mPrevSegment.get());
        this->mPrevSegment=segment;
        this->mCurLineEmpty=false;
    }
}
// FUNCTION: WON 0x00738430
// Flushes pending glued segments and adds an invisible layout segment (break, margin) to the display
void HTMLDisplayGen::AddNonDisplaySegment(LineSegment* segment,int flags)
{
    this->ClearAccumulatedSegment();
    this->mDisplay->AddSegment(segment,(unsigned short)flags);
    this->CalcLineEmpty(flags,false);
}
// FUNCTION: WON 0x00738460
// Flushes pending glued segments and adds a layout segment; it only ends the line when the line has content
void HTMLDisplayGen::AddEmptySegment(LineSegment* segment,int flags)
{
    this->ClearAccumulatedSegment();
    this->mDisplay->AddSegment(segment,(unsigned short)flags);
    if(!this->mCurLineEmpty) this->CalcLineEmpty(flags,false);
}
// FUNCTION: WON 0x007384a0
// Flushes the pending segment (or the glued line holding it) to the display and allows breaks again
void HTMLDisplayGen::ClearAccumulatedSegment(int flags)
{
    this->mCanBreak=true;
    if(this->mPrevSegment.get()==NULL)
        return;
    if(!this->mCurLine.IsEmpty())

    {
        this->mCurLine.AddSegment(this->mPrevSegment.get());
        this->mCurLine.FinishBuild();
        this->mDisplay->AddSegment(this->mCurLine.mLine.get(),(unsigned short)flags);
        this->mCurLine.mLine=NULL;
    }
    else this->mDisplay->AddSegment(this->mPrevSegment.get(),(unsigned short)flags);
    this->CalcLineEmpty(flags,true);
    this->mPrevSegment=NULL;
}

// Body of the constructor whose marker (0x00737f50) sits at the top of the file
HTMLDisplayGen::HTMLDisplayGen()
    :mTagListItr(),mTagListEnd()
{
    this->mNoBackground=false;
    this->mFontSize=HTMLFontSize_Medium;
}
// FUNCTION: WON 0x00738560
// Ends the current line with a line break unless it is already empty
void HTMLDisplayGen::StartFreshLine()
{
    if(!this->mCurLineEmpty)
        this->AddNonDisplaySegment(new LineBreakSegment(0),LineSegmentFlag_PostLineBreak);
}
// FUNCTION: WON 0x007385c0
// Starts a new paragraph: ends the line and leaves a 20-pixel gap unless the previous line was already empty
void HTMLDisplayGen::StartFreshParagraph()
{
    if(!this->mCurLineEmpty)
        this->AddNonDisplaySegment(new LineBreakSegment(20),LineSegmentFlag_PreLineBreak|LineSegmentFlag_PostLineBreak);
    else if(!this->mPrevLineEmpty)
        this->AddNonDisplaySegment(new LineBreakSegment(20),LineSegmentFlag_PostLineBreak);
}
// FUNCTION: WON 0x00738650
// Adds a line break (<BR>), 20 pixels high when the current line is empty, with optional clear flags
void HTMLDisplayGen::AddLineBreak(int extraFlags)
{
    if(!this->mCurLineEmpty)
        this->AddNonDisplaySegment(new LineBreakSegment(0),extraFlags|LineSegmentFlag_PostLineBreak);
    else
        this->AddNonDisplaySegment(new LineBreakSegment(20),extraFlags|LineSegmentFlag_PostLineBreak);
}
// FUNCTION: WON 0x007386f0
// Returns the bold/italic/underline style from the open tag counts
int HTMLDisplayGen::GetFontStyle()
{
    int style=0;
    if(this->mBoldCount) style|=FontStyle_Bold;
    if(this->mItalicCount) style|=FontStyle_Italic;
    if(this->mUnderlineCount) style|=FontStyle_Underline;
    return style;
}
// FUNCTION: WON 0x00738720
// Returns whether the innermost font on the font stack is monospace (false when the stack is empty)
bool HTMLDisplayGen::GetIsFontMonospace()
{
    return this->mFontStack.empty()?false:this->mFontStack.back().mIsMonoSpace;
}
// FUNCTION: WON 0x00738750
// Recomputes the current font and text color from the top of the font stack and the document's font size
void HTMLDisplayGen::CalcFont()
{
    static int FONT_SIZE_TABLE[5][8]={
        {4,6,8,9,10,12,16,24},
        {5,7,9,10,12,16,20,30},
        {6,8,10,12,14,18,24,36},
        {8,10,12,14,16,20,28,42},
        {10,12,14,16,18,24,32,48}
    };
    if(this->mFontStack.empty()) return;
    FontInfo info=this->mFontStack.back();
    int size=info.mSize-1;
    if(size<0) size=0;
    else if(size>6) size=6;
    if(!info.mIsMonoSpace) size++;
    int pointSize=FONT_SIZE_TABLE[this->mFontSize][size];
    FontDescriptor descriptor(info.mFace,this->GetFontStyle(),pointSize);
    this->mCurFont=this->mWindow->GetFont(descriptor);
    if(info.mColor==-1) this->mCurColor=this->mDefaultTextColor; else this->mCurColor=info.mColor;
}
// FUNCTION: WON 0x00738920
// Destroys a font stack entry, releasing its face name
// (??1FontInfo@HTMLDisplayGen@WONAPI@@QAE@XZ)
// FUNCTION: WON 0x00738960
// Pushes a font derived from the current one (or the defaults), overriding face, size 1-7 and color when given
void HTMLDisplayGen::PushFont(const char* face,int size,int color,bool mono)
{
    FontInfo info;
    if(!this->mFontStack.empty()) info=this->mFontStack.back();
    else
    {
        info.mColor=-1;
        info.mSize=3;
        info.mFace=this->mDefaultFontFace;
        info.mIsMonoSpace=false;
    }
    if(face!=NULL) info.mFace=face;
    if(size>=1 && size<=7) info.mSize=size;
    if(color!=-1) info.mColor=color;
    info.mIsMonoSpace=mono;
    this->mFontStack.push_back(info);
    this->CalcFont();
}
// FUNCTION: WON 0x00738b30
// Pops the innermost font (never the base font) and recomputes the current font
void HTMLDisplayGen::PopFont()
{
    if(this->mFontStack.size()>1)
    {
        this->mFontStack.pop_back();
        this->CalcFont();
    }
}
// FUNCTION: WON 0x00738ba0
// Returns the innermost alignment, or the generator's default alignment when none is open
HTMLAlign HTMLDisplayGen::GetCurAlign()
{
    return this->mAlignStack.empty()?this->mDefaultAlign:this->mAlignStack.back();
}

// FUNCTION: WON 0x00738bd0
// Pushes an alignment (none keeps the current one) and adds an alignment segment when it changes or is forced
void HTMLDisplayGen::PushAlign(HTMLAlign align,bool force)
{
    HTMLAlign oldAlign=this->GetCurAlign();
    if(align==HTMLAlign_None) align=oldAlign;
    this->mAlignStack.push_back(align);
    if(oldAlign!=align || force)
        this->AddEmptySegment(new AlignLineSegment((HorzAlign)align),LineSegmentFlag_PreLineBreak);
}
// FUNCTION: WON 0x00738c70
// Destroys a margin line segment
// (??1MarginLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x00738c80
// Pops the innermost alignment and adds an alignment segment when the alignment changes
void HTMLDisplayGen::PopAlign()
{
    if(this->mAlignStack.empty()) return;
    HTMLAlign oldAlign=this->GetCurAlign();
    this->mAlignStack.pop_back();
    HTMLAlign newAlign=this->GetCurAlign();
    if(newAlign!=oldAlign)
        this->AddEmptySegment(new AlignLineSegment((HorzAlign)newAlign),LineSegmentFlag_PreLineBreak);
}
// FUNCTION: WON 0x00738d30
// Applies <BODY>: background, default colors, base font and margins; the end tag adds the bottom margin and stops
void HTMLDisplayGen::HandleBody(HTMLBodyTag* tag)
{
    if(tag->IsEnd())
    {
        if(this->mBottomMargin>0)
            this->AddNonDisplaySegment(new LineBreakSegment(this->mBottomMargin),LineSegmentFlag_PreLineBreak|LineSegmentFlag_PostLineBreak);
        this->mDone=true;
    }
    else
    {
    if(this->mDisplayContainer.get()!=NULL)
    {
        if(this->mNoBackground)
        {
            Background aNone(-1);
            this->mDisplayContainer->SetBackground(aNone);
        }
        else if(tag->mBGImage.get()!=NULL)
        {
            {
                Background anImage(tag->mBGImage.get());
                this->mDisplayContainer->SetBackground(anImage);
            }
            if(tag->mTextColor<0) this->mDefaultTextColor=0;
            if(tag->mLinkColor<0) this->mDefaultLinkColor=0x0000ff;
            if(tag->mALinkColor<0) this->mDefaultALinkColor=0xff0000;
        }
        else if(tag->mBGColor<0)
            this->mDisplayContainer->SetBackground(Background());
        else
        {
            Background aColor(tag->mBGColor);
            this->mDisplayContainer->SetBackground(aColor);
        }
    }
    if(tag->mTextColor>=0) this->mDefaultTextColor=tag->mTextColor;
    if(tag->mLinkColor>=0) this->mDefaultLinkColor=tag->mLinkColor;
    if(tag->mALinkColor>=0) this->mDefaultALinkColor=tag->mALinkColor;
    this->mFontStack.clear();
    this->PushFont(this->mDefaultFontFace.c_str(),3,-1);
    this->mLeftMargin=tag->mLeftMargin;
    this->mRightMargin=tag->mRightMargin;
    this->mTopMargin=tag->mTopMargin;
    this->mBottomMargin=tag->mBottomMargin;
    if(this->mLeftMargin>0 || this->mRightMargin>0)
        this->AddNonDisplaySegment(new MarginLineSegment(this->mLeftMargin,this->mRightMargin));
    if(this->mTopMargin>0)
        this->AddNonDisplaySegment(new LineBreakSegment(this->mTopMargin),LineSegmentFlag_PostLineBreak);
    }
}
// FUNCTION: WON 0x00739100
// Assigns one background to another
// (??4Background@WONAPI@@QAEAAV01@ABV01@@Z)
// FUNCTION: WON 0x007391c0
// Peeks at the first tag and inserts an implied <BODY> when it is not one; returns false for an empty document
bool HTMLDisplayGen::FindBody()
{
    HTMLTag* tag=this->GetNextTag();
    if(tag==NULL) return false;
    if(tag->GetType()!=HTMLTagType_Body)
    {
        SmartPtr<HTMLBodyTag> body=new HTMLBodyTag;
        this->HandleTag(body.get());
    }
    --this->mTagListItr;
    return true;
}
// FUNCTION: WON 0x00739290
// Finishes a word that continues from the previous text tag, gluing it on; returns the characters used
int HTMLDisplayGen::HandleBeginGlueWord(const GUIString& oldText)
{
    int i=0;
    GUIString text;
    if(this->mLastWasSpace && !this->mInPre)
    {
        for(;i<oldText.length();i++)
            if(!safe_isspace(oldText[i])) break;
        if(i==oldText.length()) return i;
    }
    bool lastWasSpace=false;
    for(;i<oldText.length();i++)
    {
        int ch=oldText[i];
        if(!safe_isspace(ch)) text.append((wchar_t)ch);
        else
        {
            if(i==0)
            {
                this->ClearAccumulatedSegment();
                break;
            }
            if(this->mInPre) this->HandlePreSpace(ch,text);
            else text.append(L' ');
            lastWasSpace=true;
            i++;
            break;
        }
    }
    if(text.length()!=0)
    {
        this->AddDisplaySegment(this->GetTextLineSegment(text,this->mCurColor,this->mCurFont.get()));
        if(lastWasSpace) this->ClearAccumulatedSegment();
    }
    this->mLastWasSpace=lastWasSpace;
    return i;
}
// FUNCTION: WON 0x00739540
// Creates a text segment, or a link text segment while inside an anchor
TextLineSegment* HTMLDisplayGen::GetTextLineSegment(const GUIString& text,int color,Font* font)
{
    if(this->mInAnchor)
        return new TextLinkLineSegment(this->mCurLinkParam.get(),text,color,this->mDefaultALinkColor,font);
    return new TextLineSegment(text,color,font);
}
// FUNCTION: WON 0x00739740
// Deleting destructor of a text line segment (compiler-generated)
// (??_GTextLineSegment@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x00739760
// Destroys a text line segment, releasing its text and font
// (??1TextLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x007397f0
// Deleting destructor of a text link segment (compiler-generated)
// (??_GTextLinkLineSegment@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x00739810
// Destroys a text link segment, releasing its link parameter
// (??1TextLinkLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x007398c0
// Adds a text tag's words as text segments, collapsing white space (kept in <PRE>) and gluing words split across tags
void HTMLDisplayGen::HandleTextTag(HTMLTextTag* tag)
{
    const GUIString& oldText=tag->GetText();
    if(oldText.empty()) return;
    GUIString text((unsigned short)oldText.length());
    int i=0,lastWord=0;
    if(!this->mCanBreak) i=this->HandleBeginGlueWord(oldText);
    bool lastWasSpace=this->mLastWasSpace;
    const int length=oldText.length();
    for(;i<length;i++)
    {
        int ch=oldText[i];
        if(lastWasSpace)
        {
            if(!safe_isspace(ch))
            {
                lastWord=text.length();
                text.append((wchar_t)ch);
                lastWasSpace=false;
            }
            else if(this->mInPre) this->HandlePreSpace(ch,text);
        }
        else if(safe_isspace(ch))
        {
            if(this->mInPre) this->HandlePreSpace(ch,text);
            else text.append(L' ');
            lastWasSpace=true;
        }
        else text.append((wchar_t)ch);
    }
    this->mLastWasSpace=lastWasSpace;
    if(text.length()==0)
    {
        this->mCanBreak=lastWasSpace;
        return;
    }
    if(!lastWasSpace)
    {
        HTMLTextTag* next=this->GetNextTextTag();
        if(next!=NULL && !safe_isspace(next->GetText()[0]))
        {
            GUIString word=text.substr((unsigned short)lastWord);
            text.resize(lastWord);
            if(text.length()!=0)
            {
                this->AddDisplaySegment(this->GetTextLineSegment(text,this->mCurColor,this->mCurFont.get()));
                this->ClearAccumulatedSegment();
            }
            if(word.length()!=0) this->AddDisplaySegment(this->GetTextLineSegment(word,this->mCurColor,this->mCurFont.get()));
            this->mCanBreak=false;
            return;
        }
    }
    if(text.length()!=0)
    {
        this->ClearAccumulatedSegment();
        this->AddDisplaySegment(this->GetTextLineSegment(text,this->mCurColor,this->mCurFont.get()));
    }
    this->mCanBreak=lastWasSpace;
}
// FUNCTION: WON 0x00739d20
// Returns the next text tag ahead without consuming it, or NULL when a line-breaking tag or the list end comes first
HTMLTextTag* HTMLDisplayGen::GetNextTextTag()
{
    HTMLTagList::const_iterator itr=this->mTagListItr;
    while(itr!=this->mTagList->end())
    {
        HTMLTag* tag=itr->get();
        if(tag->GetType()==HTMLTagType_Text)
            return static_cast<HTMLTextTag*>(tag);
        else if(tag->InducesLineBreak())
            return NULL;
        ++itr;
    }
    return NULL;
}
// FUNCTION: WON 0x00739d50
// Applies <BR>, passing its clear-left/clear-right options
void HTMLDisplayGen::HandleLineBreak(HTMLLineBreakTag* tag)
{
    int flags=0;
    if(tag->mClearLeft) flags|=LineSegmentFlag_BreakClearLeft;
    if(tag->mClearRight) flags|=LineSegmentFlag_BreakClearRight;
    this->AddLineBreak(flags);
}

// FUNCTION: WON 0x00739d80
// Adds a horizontal rule (<HR>) on its own line with the tag's size, width, shading and alignment
void HTMLDisplayGen::HandleHorizontalRule(HTMLHorizontalRuleTag* tag)
{
    SmartPtr<HorizontalRuleSegment> segment=new HorizontalRuleSegment;
    segment->mHeight=tag->mSize;
    segment->mWidthSpec=tag->mWidthSpec;
    segment->mNoShade=tag->mNoShade;
    this->PushAlign(tag->mAlign);
    this->StartFreshLine();
    this->AddDisplaySegment(segment.get(),LineSegmentFlag_PostLineBreak);
    this->PopAlign();
}
// FUNCTION: WON 0x00739e80
// Applies <H1>-<H6>: a new paragraph in bold at a size by heading level; the end tag restores font and alignment
void HTMLDisplayGen::HandleHeading(HTMLHeadingTag* tag)
{
    if(tag->IsStart())
    {
        this->StartFreshParagraph();
        this->PushAlign(tag->mAlign);
        ++this->mBoldCount;
        this->PushFont(this->mDefaultFontFace.c_str(),HTMLTagType_H6-tag->GetType()+1,-1);
    }
    else
    {
        --this->mBoldCount;
        this->PopFont();
        this->PopAlign();
        this->StartFreshParagraph();
    }
}
// FUNCTION: WON 0x00739f00
// Applies <A>: adds a named anchor and, for a link, underlines in the link color until the end tag
void HTMLDisplayGen::HandleAnchor(HTMLAnchorTag* tag)
{
    if(tag->IsStart())
    {
        if(!tag->mName.empty())
            this->mDisplay->AddSegment(new AnchorLineSegment(tag->mName));
        if(tag->mHRef.length()!=0)
        {
            this->mInAnchor=true;
            ++this->mUnderlineCount;
            this->PushFont(NULL,-1,this->mDefaultLinkColor,this->GetIsFontMonospace());

            this->mCurLinkParam=new TextLinkParam(tag->mHRef,tag->mLaunchBrowser);
        }
    }
    else if(this->mInAnchor)
    {
        this->mInAnchor=false;
        --this->mUnderlineCount;
        this->PopFont();
    }
}
// FUNCTION: WON 0x0073a0c0
// Returns the anchor segment's vertical alignment (always 2)
// (?GetVertAlign@AnchorLineSegment@WONAPI@@UAEDXZ)
// FUNCTION: WON 0x0073a0d0
// Deleting destructor of an anchor segment (compiler-generated)
// (??_GAnchorLineSegment@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x0073a0f0
// Destroys an anchor segment, releasing its name
// (??1AnchorLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073a150
// Deleting destructor of a link parameter (compiler-generated)
// (??_GTextLinkParam@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x0073a170
// Destroys a link parameter, releasing its link target
// (??1TextLinkParam@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073a220
// Applies <PRE>: a new paragraph in monospace Courier New with white space kept, until the end tag
void HTMLDisplayGen::HandlePre(HTMLTag* tag)
{
    if(tag->IsStart())
    {
        this->StartFreshParagraph();
        this->mInPre=true;
        ++this->mNoBreak;
        this->PushFont("Courier New",-1,-1,true);
    }
    else
    {
        this->PopFont();
        this->mInPre=false;
        --this->mNoBreak;
        this->StartFreshParagraph();
    }
}
// FUNCTION: WON 0x0073a280
// Applies <CENTER>: starts a fresh line and pushes or pops center alignment
void HTMLDisplayGen::HandleCenter(HTMLTag* tag)
{
    this->StartFreshLine();
    if(tag->IsStart()) this->PushAlign(HTMLAlign_Center);
    else this->PopAlign();
}
// FUNCTION: WON 0x0073a2c0
// Applies <B>, <I> or <U>: counts the style up or down and recomputes the font
void HTMLDisplayGen::HandleModifyFontStyle(HTMLTag* tag,int style)
{
    int increment=tag->IsStart()?1:-1;
    if(style&FontStyle_Bold) this->mBoldCount+=increment;
    if(style&FontStyle_Italic) this->mItalicCount+=increment;
    if(style&FontStyle_Underline) this->mUnderlineCount+=increment;
    this->CalcFont();
}
// FUNCTION: WON 0x0073a300
// Pushes the given font on a start tag and pops it on the end tag
void HTMLDisplayGen::HandleModifyFont(HTMLTag* tag,const char* face,int size,int color,bool mono)
{
    if(tag->IsStart()) this->PushFont(face,size,color,mono);
    else this->PopFont();
}
// FUNCTION: WON 0x0073a340
// Applies <FONT>/<BASEFONT>: pushes the tag's face, size and color; the end tag pops it
void HTMLDisplayGen::HandleFont(HTMLFontTag* tag)
{
    if(tag->IsStart())
    {
        if(tag->mFace.empty()) this->PushFont(NULL,tag->mSize,tag->mColor);
        else
        {
            std::string face=tag->mFace;
            this->PushFont(face.c_str(),tag->mSize,tag->mColor);
        }
    }
    else this->PopFont();
}
// FUNCTION: WON 0x0073a4a0
// Closes the open paragraph: restores its alignment and starts a fresh paragraph
void HTMLDisplayGen::EndParagraph()
{
    this->PopAlign();
    this->StartFreshParagraph();
    this->mInParagraph=false;
}
// FUNCTION: WON 0x0073a4c0
// Applies <P> and <DIV>: starts a paragraph or line with the tag's alignment; the end tag restores it
void HTMLDisplayGen::HandleBlock(HTMLBlockTag* tag)
{
    if(tag->IsStart())
    {
        if(tag->GetType()==HTMLTagType_Paragraph)
        {
            this->StartFreshParagraph();
            this->mInParagraph=true;
        }
        else this->StartFreshLine();
        this->PushAlign(tag->mAlign);
    }
    else
    {
        if(tag->GetType()!=HTMLTagType_Paragraph)
        {
            this->StartFreshLine();
            this->PopAlign();
        }
        else if(this->mInParagraph) this->EndParagraph();
    }
}

// FUNCTION: WON 0x0073a540
// Adds an <IMG> as an animation, linked image (anchor or image map) or plain image, floated or aligned per the tag
void HTMLDisplayGen::HandleImage(HTMLImageTag* tag)
{
    if(tag->IsEnd()) return;
    if(tag->mAnimation.get()!=NULL)
    {
        SmartPtr<AnimationComponent> animation=new AnimationComponent(tag->mAnimation.get());
        animation->Start();
        this->AddDisplaySegment(new ComponentLineSegment(animation.get()));
        this->mCanBreak=false;
        return;
    }
    else if(tag->mImage.get()==NULL) return;
    Image* image=tag->mImage.get();
    SmartPtr<ImageLineSegment> segment;
    ImageMap* map=tag->mImageMap.get();
    int border=this->mInAnchor?2:0;
    int borderColor=this->mCurColor;
    if(this->mInAnchor)
        segment=new ImageLinkLineSegment(this->mCurLinkParam.get(),this->mCurColor,this->mDefaultALinkColor,image);
    else if(map!=NULL)
    {
        border=2;
        borderColor=this->mDefaultLinkColor;
        segment=new ImageLinkLineSegment(NULL,this->mCurColor,this->mDefaultALinkColor,image);
        static_cast<ImageLinkLineSegment*>(segment.get())->mImageMap=map;
    }
    else segment=new ImageLineSegment(image);
    if(tag->mBorder>=0) border=tag->mBorder;
    int flags=0;
    int align=0;
    switch(tag->mAlign)
    {
    case HTMLAlign_Left: flags|=LineSegmentFlag_FloatLeft; break;
    case HTMLAlign_Right: flags|=LineSegmentFlag_FloatRight; break;
    case HTMLAlign_Middle: align=1; break;
    case HTMLAlign_Top: align=2; break;
    }
    segment->mHSpace=tag->mHSpace;
    segment->mVSpace=tag->mVSpace;
    segment->mAlign=(char)align;
    segment->mBorder=border;
    segment->mBorderColor=borderColor;
    this->ClearAccumulatedSegment();
    this->AddDisplaySegment(segment.get(),flags);
    if(flags==0)
    {
        this->mCanBreak=false;
    }
}
// FUNCTION: WON 0x0073a9b0
// Deleting destructor of a component segment (compiler-generated)
// (??_GComponentLineSegment@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x0073a9d0
// Destroys a component segment, releasing its component
// (??1ComponentLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073aaf0
// Destroys an image segment, releasing its image
// (??1ImageLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073ab50
// Deleting destructor of a linked image segment (compiler-generated)
// (??_GImageLinkLineSegment@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x0073ab70
// Destroys a linked image segment, releasing its link parameter and image map
// (??1ImageLinkLineSegment@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073ac40
// Applies <UL>/<OL>: indents 40 pixels per nesting level, with a paragraph break around the outermost list
void HTMLDisplayGen::HandleList(HTMLListTag* tag)
{
    if(tag->IsEnd())
    {
        if(this->mListDepth<=0) return;
        --this->mListDepth;
        this->AddEmptySegment(new MarginLineSegment(this->mListDepth*40+this->mLeftMargin,this->mRightMargin),LineSegmentFlag_PreLineBreak);
        if(this->mListDepth==0) this->StartFreshParagraph();
    }
    else
    {
        ++this->mListDepth;
        if(this->mListDepth==1) this->StartFreshParagraph();
        this->AddEmptySegment(new MarginLineSegment(this->mListDepth*40+this->mLeftMargin,this->mRightMargin),LineSegmentFlag_PreLineBreak);
    }
}

// FUNCTION: WON 0x0073ad40
// Applies <LI>: starts a line with a bullet, or with the item's number text in an ordered list
void HTMLDisplayGen::HandleListItem(HTMLListItemTag* tag)
{
    if(tag->IsEnd()) return;
    this->StartFreshLine();
    if(this->mListDepth<1)
        this->AddDisplaySegment(new BulletElement(this->mCurColor));
    else if(!tag->mBulletText.empty())
        this->AddDisplaySegment(new OrderedListElement(this->mCurColor,this->mCurFont.get(),tag->mBulletText));
    else
        this->AddDisplaySegment(new BulletElement(this->mCurColor,tag->mListType-HTMLListType_Disc+1));
    this->mCanBreak=false;
    this->mLastWasSpace=true;
    this->mCurLineEmpty=this->mPrevLineEmpty=true;
}
// FUNCTION: WON 0x0073af00
// Returns the ascent of the ordered-list number's font
// (?GetAscent@OrderedListElement@WONAPI@@UAEHXZ)
// FUNCTION: WON 0x0073af10
// Deleting destructor of an ordered-list number element (compiler-generated)
// (??_GOrderedListElement@WONAPI@@UAEPAXI@Z)
// FUNCTION: WON 0x0073af30
// Destroys an ordered-list number element, releasing its text and font
// (??1OrderedListElement@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073afc0
// Starts a table element from <TABLE>; the end tag adds the finished table, floated or aligned per the tag
void HTMLDisplayGen::HandleTable(HTMLTableTag* tag)
{
    if(tag->IsEnd())
    {
        if(this->mInTable)
        {
            this->mInTable=false;
            int flags=LineSegmentFlag_PreLineBreak;
            if(this->mCurTableAlign==HTMLAlign_Left) flags|=LineSegmentFlag_FloatLeft;
            else if(this->mCurTableAlign==HTMLAlign_Right) flags|=LineSegmentFlag_FloatRight;
            else if(this->mCurTableAlign==HTMLAlign_Center) this->PushAlign(HTMLAlign_Center);
            this->AddDisplaySegment(this->mCurTable.get(),flags);
            if(this->mCurTableAlign==HTMLAlign_Center) this->PopAlign();
            else if(this->mCurTableAlign==HTMLAlign_None) this->StartFreshLine();
        }
        else if(this->mParsingTableData) this->HandleEndTableData();
    }
    else
    {
        this->ClearAccumulatedSegment();
        this->mInTable=true;
        this->mCurTable=new TableElement;
        this->mCurTable->mCellSpacing=tag->mCellSpacing;
        this->mCurTable->mCellPadding=tag->mCellPadding;
        this->mCurTable->mBorder=tag->mBorder;
        this->mCurTable->mBGColor=tag->mBGColor;
        this->mCurTable->mWidthSpec=tag->mWidthSpec;
        this->mCurTableAlign=tag->mAlign;
        this->mCurTable->StartTraversal();
    }
}
// FUNCTION: WON 0x0073b160
// Applies <TR>: starts the next table row with its background color and alignment
void HTMLDisplayGen::HandleTableRow(HTMLTableRowTag* tag)
{
    if(tag->IsEnd())
    {
        if(this->mParsingTableData && !this->mInTable) this->HandleEndTableData();
        return;
    }
    if(this->mInTable)
    {
        this->mCurTable->NextRow();
        this->mCurTable->SetRowBGColor(tag->mBGColor);
        int horizontal=tag->mHAlign-HTMLAlign_Left;
        int vertical=tag->mVAlign-HTMLAlign_Top;
        this->mCurTable->SetRowAlign(horizontal,vertical);
    }
    else if(this->mParsingTableData) this->HandleEndTableData();
}
// FUNCTION: WON 0x0073b1d0
// Applies <TD>/<TH>: sets up the next cell and fills it by running a child generator over the cell's tags
void HTMLDisplayGen::HandleTableData(HTMLTableDataTag* tag)
{
    if(tag->IsEnd())
    {
        if(this->mParsingTableData && !this->mInTable) this->HandleEndTableData();
        return;
    }
    if(this->mInTable)
    {
        TableElement::CellInfo* cell=this->mCurTable->NextColumn();
        MultiLineElement* element=cell->mContents.get();
        cell->mBGColor=tag->mBGColor;
        cell->mHAlign=(char)(tag->mHAlign-HTMLAlign_Left);
        cell->mVAlign=(char)(tag->mVAlign-HTMLAlign_Top);
        if(cell->mVAlign<0 || cell->mVAlign>2) cell->mVAlign=-1;
        if(cell->mHAlign<0 || cell->mHAlign>2) cell->mHAlign=-1;
        cell->mNoWrap=tag->mNoWrap;
        if(tag->mWidthSpec>=0) cell->mWidth=tag->mWidthSpec;
        else if(tag->mColSpan<=1) this->mCurTable->SetColumnWidth(tag->mWidthSpec);
        if(tag->mHeight>0) cell->mHeight=tag->mHeight;
        HTMLAlign align=(HTMLAlign)((this->mCurTable->GetCurRowAlign()-HorzAlign_Left)+HTMLAlign_Left);
        if(cell->mHAlign==1) align=HTMLAlign_Center;
        else if(cell->mHAlign==2) align=HTMLAlign_Right;
        // SetCellSpan may reallocate the cell vector; no cell pointer access follows.
        if(tag->mRowSpan>1 || tag->mColSpan>1)
            this->mCurTable->SetCellSpan(tag->mRowSpan,tag->mColSpan);
        HTMLDisplayGen generator;
        generator.GenerateTableData(*this,element,tag->GetType()==HTMLTagType_TableHeader,align);
    }
    else if(this->mParsingTableData) this->HandleEndTableData();
}
// FUNCTION: WON 0x0073b4a0
// Destroys the generator, releasing its stacks, fonts, current table, combo box and link parameter
// (??1HTMLDisplayGen@WONAPI@@QAE@XZ)
// FUNCTION: WON 0x0073b660
// Ends the current cell: steps back so the parent generator sees the tag again, and stops this generator
void HTMLDisplayGen::HandleEndTableData()
{
    --this->mTagListItr;
    this->mDone=true;
}
// FUNCTION: WON 0x0073b670
// Adds an embedded GUI component as an unbreakable segment
void HTMLDisplayGen::HandleGUIComponent(HTMLGUIComponentTag* tag)
{
    this->AddDisplaySegment(new ComponentLineSegment(tag->mComponent.get()));
    this->mCanBreak=false;
    this->mLastWasSpace=false;
}

// FUNCTION: WON 0x0073b6e0
// Reads an <OPTION>'s text (white space collapsed) into the open combo box and widens the box to fit
void HTMLDisplayGen::HandleOption(HTMLTag* tag)
{
    if(this->mCurCombo.get()==NULL || tag->IsEnd())
        return;
    GUIString option;
    bool lastWasSpace=true;
    while(true) {
        HTMLTag* next=this->GetNextTag();
        if(next==NULL)
            break;
        if(next->GetType()==HTMLTagType_Option) {
            if(next->IsStart())
                --this->mTagListItr;
            break;
        }
        else if(next->GetType()==HTMLTagType_Text) {
            const GUIString& text=static_cast<HTMLTextTag*>(next)->GetText();
            for(int i=0;i<text.length();++i) {
                unsigned int c=text[i];
                if(c<256 && isspace(c)) {
                    if(!lastWasSpace) {
                        option.append(' ');
                        lastWasSpace=true;
                    }
                }
                else {
                    option.append(c);
                    lastWasSpace=false;
                }
            }
        }
        else {
            --this->mTagListItr;
            break;
        }
    }
    this->mCurCombo->AddString(option,-1);
    int width=this->mCurCombo->mListArea->GetTotalWidth();
    this->mCurCombo->SetSize(width+20,this->mCurCombo->Height());
}
// FUNCTION: WON 0x0073b8b0
// Creates the control for <INPUT>/<SELECT> (button, radio, image button, text box, combo box) as a segment
void HTMLDisplayGen::HandleInput(HTMLInputTag* tag)
{
    if(tag->IsEnd()) {
        if(this->mCurCombo.get()!=NULL) {
            this->AddDisplaySegment(new ComponentLineSegment(this->mCurCombo.get()));
            this->mCanBreak=false;
            this->mLastWasSpace=false;
            this->mCurCombo=NULL;
        }
        return;
    }
    SmartPtr<LineSegment> segment;
    SmartPtr<Font> font=this->mWindow->GetFont(FontDescriptor("Tahoma",FontStyle_Plain,10));
    switch(tag->mInputType) {
    case HTMLInputType_Button:
    case HTMLInputType_Submit:
    case HTMLInputType_Reset:
        {
            SmartPtr<MSButton> button=new MSButton(tag->mValue);
            button->SetFont(font.get());
            button->SetDesiredSize(0,0);
            segment=new ComponentLineSegment(button.get());
            break;
        }
    case HTMLInputType_Radio:
        {
            SmartPtr<MSRadioButton> radio=new MSRadioButton;
            radio->SetTransparent(true);
            radio->SetSize(15,12);
            radio->SetButtonFlags(8,false);
            segment=new ComponentLineSegment(radio.get());
            break;
        }
    case HTMLInputType_Image:
        {
            Image* image=tag->mImage.get();
            if(image==NULL)
                break;
            SmartPtr<ImageButton> button=new ImageButton;
            button->SetSize(image->mWidth,image->mHeight);
            button->SetImage(ImageButtonType_Normal,image);
            button->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_Hand));
            segment=new ComponentLineSegment(button.get());
            break;
        }
    case HTMLInputType_Checkbox:
    case HTMLInputType_File:
    case HTMLInputType_Hidden:
        break;
    case HTMLInputType_Select:
        this->mCurCombo=new MSComboBox;
        this->mCurCombo->SetSize(50,22);
        break;
    case HTMLInputType_Text:
    case HTMLInputType_Password:
        {
            SmartPtr<MSInputBox> input=new MSInputBox;
            input->mFont=font;
            if(tag->mInputType==HTMLInputType_Password)
                input->SetPasswordChar('*');
            else if(tag->mReadOnly)
                input->SetComponentFlags(ComponentFlag_WantFocus,false);
            input->SetSize(tag->mSize*6+24,22);
            input->mMaxChars=tag->mMaxLength;
            input->SetText(tag->mValue,true);
            segment=new ComponentLineSegment(input.get());
            break;
        }
    }
    if(segment.get()!=NULL) {
        this->AddDisplaySegment(segment.get());
        this->mCanBreak=false;
        this->mLastWasSpace=false;
    }
}
// FUNCTION: WON 0x0073bea0
// Deleting destructor of an input box (compiler-generated)
// (??_GMSInputBox@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0073c020
// Dispatches one tag to its handler by tag type; unknown tags are ignored
void HTMLDisplayGen::HandleTag(HTMLTag* tag)
{
    switch(tag->GetType())
    {
    case HTMLTagType_Body: this->HandleBody(static_cast<HTMLBodyTag*>(tag)); break;
    case HTMLTagType_Anchor: this->HandleAnchor(static_cast<HTMLAnchorTag*>(tag)); break;
    case HTMLTagType_LineBreak: this->HandleLineBreak(static_cast<HTMLLineBreakTag*>(tag)); break;
    case HTMLTagType_HorizontalRule: this->HandleHorizontalRule(static_cast<HTMLHorizontalRuleTag*>(tag)); break;
    case HTMLTagType_Text: this->HandleTextTag(static_cast<HTMLTextTag*>(tag)); break;
    case HTMLTagType_Bold: this->HandleModifyFontStyle(tag,FontStyle_Bold); break;
    case HTMLTagType_Italic: this->HandleModifyFontStyle(tag,FontStyle_Italic); break;
    case HTMLTagType_Underline: this->HandleModifyFontStyle(tag,FontStyle_Underline); break;
    case HTMLTagType_Center: this->HandleCenter(tag); break;
    case HTMLTagType_Small: this->HandleModifyFont(tag,NULL,2,-1,this->GetIsFontMonospace()); break;
    case HTMLTagType_Big: this->HandleModifyFont(tag,NULL,4,-1,this->GetIsFontMonospace()); break;
    case HTMLTagType_Code: this->HandleModifyFont(tag,"Courier New",-1,-1,true); break;
    case HTMLTagType_Pre: this->HandlePre(tag); break;
    case HTMLTagType_Font:
    case HTMLTagType_BaseFont: this->HandleFont(static_cast<HTMLFontTag*>(tag)); break;
    case HTMLTagType_Div:
    case HTMLTagType_Paragraph: this->HandleBlock(static_cast<HTMLBlockTag*>(tag)); break;
    case HTMLTagType_Table: this->HandleTable(static_cast<HTMLTableTag*>(tag)); break;
    case HTMLTagType_TableRow: this->HandleTableRow(static_cast<HTMLTableRowTag*>(tag)); break;
    case HTMLTagType_TableData:
    case HTMLTagType_TableHeader: this->HandleTableData(static_cast<HTMLTableDataTag*>(tag)); break;
    case HTMLTagType_UnorderedList:
    case HTMLTagType_OrderedList: this->HandleList(static_cast<HTMLListTag*>(tag)); break;
    case HTMLTagType_ListItem: this->HandleListItem(static_cast<HTMLListItemTag*>(tag)); break;
    case HTMLTagType_Image: this->HandleImage(static_cast<HTMLImageTag*>(tag)); break;
    case HTMLTagType_Input:
    case HTMLTagType_Select: this->HandleInput(static_cast<HTMLInputTag*>(tag)); break;
    case HTMLTagType_Option: this->HandleOption(tag); break;
    case HTMLTagType_H1:
    case HTMLTagType_H2:
    case HTMLTagType_H3:
    case HTMLTagType_H4:
    case HTMLTagType_H5:
    case HTMLTagType_H6: this->HandleHeading(static_cast<HTMLHeadingTag*>(tag)); break;
    case HTMLTagType_GUIComponent: this->HandleGUIComponent(static_cast<HTMLGUIComponentTag*>(tag)); break;
    default: break;
    }
}
// FUNCTION: WON 0x0073c250
// Generates one table cell's contents, inheriting colors, font and tag position from the parent generator
void HTMLDisplayGen::GenerateTableData(HTMLDisplayGen& parent,MultiLineElement* display,bool header,HTMLAlign align)
{
    this->mDisplay=display;
    this->mWindow=parent.mWindow;
    this->mDefaultLinkColor=parent.mDefaultLinkColor;
    this->mDefaultALinkColor=parent.mDefaultALinkColor;
    this->mDefaultTextColor=parent.mDefaultTextColor;
    this->mDefaultFontFace=parent.mDefaultFontFace;
    this->mDefaultAlign=align;
    this->mBoldCount=this->mItalicCount=this->mUnderlineCount=0;
    this->mTagList=parent.mTagList;
    this->mTagListItr=parent.mTagListItr;
    this->mTagListEnd=parent.mTagListEnd;
    this->mFontSize=parent.mFontSize;
    this->mLeftMargin=this->mRightMargin=this->mTopMargin=this->mBottomMargin=0;
    if(header) this->mBoldCount++;
    this->mParsingTableData=true;
    this->PushFont(this->mDefaultFontFace.c_str(),3,-1);
    if(this->mDefaultAlign!=HTMLAlign_Left) this->PushAlign(this->mDefaultAlign,true);
    this->GenerateBase();
    parent.mTagListItr=this->mTagListItr;
}

// FUNCTION: WON 0x0073c430
// Builds the display for an HTML document: resets fonts, colors and alignment, then generates the body
void HTMLDisplayGen::Generate(HTMLDocument* document,MultiLineElement* display,Table* container)
{
    this->mTagList=&document->GetTagList();
    this->mTagListItr=document->GetBodyStart();
    this->mTagListEnd=document->GetBodyEnd();
    this->mWindow=WONAPI::WindowManager::GetDefaultWindowManager()->mDefaultWindow;
    this->mDisplay=display;
    this->mDisplayContainer=container;
    this->mAlignStack.clear();
    this->mFontStack.clear();
    this->mDefaultTextColor=(int)(0x80000000u|StandardColor_Text);
    this->mDefaultFontFace=gGlobalDefaultFontFace;
    this->mBoldCount=this->mItalicCount=this->mUnderlineCount=0;
    this->mDefaultLinkColor=(int)(0x80000000u|StandardColor_Link);
    this->mDefaultALinkColor=(int)(0x80000000u|StandardColor_LinkDown);
    this->mParsingTableData=false;
    this->mDefaultAlign=HTMLAlign_Left;
    if(!this->FindBody()) return;
    this->GenerateBase();
    LineSegment::mCounter=0;
    this->mDisplay->SetSelIndex();
}

// FUNCTION: WON 0x0073c5b0
// Resets the line state and handles tags until the body or cell ends, then flushes the pending segment
void HTMLDisplayGen::GenerateBase()
{
    this->mListDepth=0;
    this->mCanBreak=true;
    this->mLastWasSpace=true;
    this->mCurLineEmpty=true;
    this->mPrevLineEmpty=true;
    this->mInPre=false;
    this->mNoBreak=0;
    this->mInAnchor=false;
    this->mInParagraph=false;
    this->mDone=false;
    this->mInTable=false;
    this->mNextTextTag=NULL;
    while(!this->mDone)
    {
        HTMLTag* tag=this->GetNextTag();
        if(tag==NULL) break;
        this->HandleTag(tag);
    }
    this->ClearAccumulatedSegment();
}
// FUNCTION: WON 0x0074afc0
// Creates an empty image map smart pointer
// (??0?$SmartPtr@VImageMap@WONAPI@@@WONAPI@@QAE@XZ)
// FUNCTION: WON 0x0074afd0
// Appends a segment pointer to a line segment list
// (?push_back@?$list@PAVLineSegment@WONAPI@@V?$allocator@PAVLineSegment@WONAPI@@@std@@@std@@QAEXABQAVLineSegment@WONAPI@@@Z)
// TEMPLATE: WON 0x00702370
// (public: __thiscall WONAPI::GUIString::operator class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >(void)const )
// TEMPLATE: WON 0x00711480
// (public: virtual void * __thiscall WONAPI::MarginLineSegment::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x00738000
// (_$E71)
// TEMPLATE: WON 0x007385b0
// (public: virtual int __thiscall WONAPI::LineBreakSegment::GetHeight(void))
// TEMPLATE: WON 0x00739710
// (public: __thiscall WONAPI::LineSegment::LineSegment(void))
// TEMPLATE: WON 0x0073aef0
// (public: virtual int __thiscall WONAPI::OrderedListElement::GetHeight(void))
// TEMPLATE: WON 0x0073bec0
// (public: virtual __thiscall WONAPI::MSInputBox::~MSInputBox(void))
// TEMPLATE: WON 0x0074c8a0
// (public: class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisplayGen::FontInf)
// TEMPLATE: WON 0x0074c920
// (public: class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisplayGen::FontInf)
// TEMPLATE: WON 0x0074c9a0
// (public: class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisplayGen::FontInf)
// TEMPLATE: WON 0x0074ca20
// (protected: void __thiscall std::vector<struct WONAPI::HTMLDisplayGen::FontInfo,class std::allocator<struct WONAPI::HTMLDisplayGen::FontInfo> >::_Tidy()
// TEMPLATE: WON 0x0074ca70
// (public: class std::_Ptrit<enum WONAPI::HTMLAlign,int,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign &,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign)
// TEMPLATE: WON 0x0074cad0
// (public: class std::_Ptrit<enum WONAPI::HTMLAlign,int,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign &,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign)
// TEMPLATE: WON 0x0074cb40
// (public: class std::_Ptrit<enum WONAPI::HTMLAlign,int,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign &,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign)
// TEMPLATE: WON 0x007500f0
// (public: void __thiscall std::vector<struct WONAPI::HTMLDisplayGen::FontInfo,class std::allocator<struct WONAPI::HTMLDisplayGen::FontInfo> >::insert(cl)
// TEMPLATE: WON 0x00750580
// (protected: void __thiscall std::vector<struct WONAPI::HTMLDisplayGen::FontInfo,class std::allocator<struct WONAPI::HTMLDisplayGen::FontInfo> >::_Destr)
// TEMPLATE: WON 0x007505b0
// (public: void __thiscall std::vector<enum WONAPI::HTMLAlign,class std::allocator<enum WONAPI::HTMLAlign> >::insert(class std::_Ptrit<enum WONAPI::HTMLA)
// TEMPLATE: WON 0x007520e0
// (protected: void __thiscall std::vector<enum WONAPI::HTMLAlign,class std::allocator<enum WONAPI::HTMLAlign> >::_Xlen(void)const )
// TEMPLATE: WON 0x007543c0
// (class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisplayGen::FontInfo &,stru)
// TEMPLATE: WON 0x007544e0
// (struct WONAPI::HTMLDisplayGen::FontInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGe)
// TEMPLATE: WON 0x00754660
// (class std::_Ptrit<enum WONAPI::HTMLAlign,int,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign &,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign &> __cd)
// TEMPLATE: WON 0x007546a0
// (enum WONAPI::HTMLAlign * __cdecl std::copy(class std::_Ptrit<enum WONAPI::HTMLAlign,int,enum WONAPI::HTMLAlign *,enum WONAPI::HTMLAlign &,enum WONAPI:)
// TEMPLATE: WON 0x00754880
// (protected: struct WONAPI::HTMLDisplayGen::FontInfo * __thiscall std::vector<struct WONAPI::HTMLDisplayGen::FontInfo,class std::allocator<struct WONAPI)
// TEMPLATE: WON 0x007548c0
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisp)
// TEMPLATE: WON 0x00754a30
// (class std::_Ptrit<struct WONAPI::HTMLDisplayGen::FontInfo,int,struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisplayGen::FontInfo &,stru)
// TEMPLATE: WON 0x00754e40
// (void __cdecl std::_Construct(struct WONAPI::HTMLDisplayGen::FontInfo *,struct WONAPI::HTMLDisplayGen::FontInfo const &))
// TEMPLATE: WON 0x0073aa30
// TEMPLATE: WON 0x0073aad0
}
