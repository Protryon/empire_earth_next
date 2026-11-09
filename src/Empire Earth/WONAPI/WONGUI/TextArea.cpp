// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <algorithm>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
using namespace WONAPI;

// FUNCTION: WON 0x00711df0
// Clears the laid-out lines and anchors; unless only the display is reset, also drops every segment,
// the selection and the scroll position
void TextArea::Reset(bool justDisplay) {
    this->mLineVector.clear();
    this->mAnchorMap.clear();
    if(!justDisplay) {
        this->mSegmentList.clear();
        this->mSelecting=false;
        this->mSelStartChar=this->mSelEndChar=0;
        this->mBackwardsSel=false;
        this->mVertOffset=this->mHorzOffset=0;
        this->mTotalWidth=this->mTotalHeight=0;
        this->AdjustPosition(false);
    }
    this->mNumLines=0;
    this->mTopLineVectorPos=0;
    this->mTotalHeight=this->mTotalWidth=0;
    this->mCurLink=NULL;
    this->mLastFindSeg=NULL;
    this->mNeedRecalcWordWrap=false;
    this->mDelayWordWrapTimeAccum=0;
}

// FUNCTION: WON 0x00711fc0
// Lays every segment out again (after a width change) and keeps the view at the bottom when it
// scrolls on bottom
void TextArea::RecalcWordWrap(bool wasDelayed) {
    this->Reset(true);
    SegmentList::iterator itr=this->mSegmentList.begin();
    while(itr!=this->mSegmentList.end()) {
        this->AddSegmentPrv(itr->get());
        ++itr;
    }
    if(this->ShouldScrollOnBottom()) this->mVertOffset=this->mTotalHeight;
    if(wasDelayed) this->AdjustPosition(false);
}

// FUNCTION: WON 0x00712030
// Relays out the scroll area; re-wraps the text when the width changed (deferred to the timer for
// over 5000 characters with delayed wrap) and keeps a bottom-scrolled view at the bottom
void TextArea::Virtual08() {
    int oldWidth=this->mScrollArea.width;
    bool bottom=this->ShouldScrollOnBottom();
    ScrollArea::Virtual08();
    if(this->TextFlagSet(TextFlag_WordWrap) && !this->mSegmentList.empty() && this->mScrollArea.width!=oldWidth && this->mHasBeenVisible) {
        if(this->TextFlagSet(TextFlag_DelayWordWrap) && !this->mSegmentList.empty() && this->mSegmentList.back()->mAbsEndChar>5000) {
            this->mDelayWordWrapTimeAccum=0;
            this->mNeedRecalcWordWrap=true;
            this->RequestTimer(true);
        } else this->RecalcWordWrap(false);
    }
    if(bottom) this->mVertOffset=this->mTotalHeight+this->mExtraHeight-this->mScrollArea.height;
    this->AdjustPosition(false);
}

// FUNCTION: WON 0x007120f0
// Clamps the scroll position and finds the top line; with full-line scrolling, snaps the view to
// the top line's start and sets the up/down scroll steps to the neighbouring line heights
void TextArea::AdjustPosition(bool becauseOfScrollbar) {
    this->mExtraHeight=0;
    if(this->TextFlagSet(TextFlag_ScrollFullLine) && this->mTotalHeight>this->mScrollArea.height) {
        LineVector::iterator itr=this->FindLineAt(this->mTotalHeight-this->mScrollArea.height);
        if(itr!=this->mLineVector.end()) {
            TextLine* line=(*itr).get();
            this->mExtraHeight=line->my+line->mHeight+this->mScrollArea.height-this->mTotalHeight;
            if(this->mExtraHeight<0) this->mExtraHeight=0;
        }
    }
    ScrollArea::AdjustPosition(becauseOfScrollbar);
    this->FindTopLine();
    if(!this->TextFlagSet(TextFlag_ScrollFullLine))
        return;
    if(!(this->mTopLineVectorPos<this->mLineVector.size()))
        return;
    TextLine* line=this->mLineVector[this->mTopLineVectorPos].get();
    this->mVertOffset=line->my;
    this->mDownScrollAmount=line->mHeight;
    if(this->mTopLineVectorPos>0) {
        line=this->mLineVector[this->mTopLineVectorPos-1].get();
        this->mUpScrollAmount=line->mHeight;
    }
    ScrollArea::AdjustPosition(true);
}

// FUNCTION: WON 0x007128c0
// Returns whether new text should scroll the view to the bottom: scroll-on-bottom is set and the
// user has not scrolled away from the last (non-empty) line
bool TextArea::ShouldScrollOnBottom() {
    if(!this->TextFlagSet(TextFlag_ScrollOnBottom)) return false;
    if(!this->mUserHasScrolled) return true;
    if(!this->mLineVector.empty()) {
        TextLine* line=this->mLineVector.back().get();
        if(line->mHeight==0 && this->mLineVector.size()>1) line=this->mLineVector[this->mLineVector.size()-2].get();
        if(this->IsLineVisible(line)) return true;
    }
    return false;
}

// FUNCTION: WON 0x00713030
// Returns whether a line is inside the view (partly, when partial lines are shown)
bool TextArea::IsLineVisible(TextLine* line) {
    if(line->my+line->mHeight<this->mVertOffset) return false;
    int y=line->my;
    if(!this->TextFlagSet(TextFlag_ShowPartialLines)) y+=line->mHeight;
    return y<=this->mVertOffset+this->mScrollArea.height;
}

// FUNCTION: WON 0x00713080
// On first becoming visible, word-wraps the text that was added while hidden
void TextArea::NotifyRootVisibility(bool visible) {
    ScrollArea::NotifyRootVisibility(visible);
    if(visible && !this->mHasBeenVisible) {
        this->mHasBeenVisible=true;
        if(this->TextFlagSet(TextFlag_WordWrap) && !this->mSegmentList.empty()) this->RecalcWordWrap(true);
    }
}

// FUNCTION: WON 0x007130d0
// Paints the segments of every visible line, starting at the top line
void TextArea::ScrollPaint(Graphics& g) {
    LineVector::iterator lineItr=this->mLineVector.begin()+this->mTopLineVectorPos;
    while(lineItr!=this->mLineVector.end()) {
        TextLine* line=(*lineItr).get();
        if(!this->IsLineVisible(line)) break;
        TextLine::SegmentList::iterator segItr=line->mSegmentList.begin();
        g.Translate(line->mx,line->my);
        while(segItr!=line->mSegmentList.end()) {
            segItr->get()->Paint(g,this);
            ++segItr;
        }
        g.Translate(-line->mx,-line->my);
        ++lineItr;
    }
}

struct TextLineYLess {
    bool operator()(const SmartPtr<TextLine>& a,const SmartPtr<TextLine>& b) const { return a->my<b->my; }
};
// FUNCTION: WON 0x007131b0
// Returns the line containing height y (the last line starting at or above y)
TextArea::LineVector::iterator TextArea::FindLineAt(int y) {
    this->mSearchLine->my=y;
    LineVector::iterator itr=std::lower_bound(this->mLineVector.begin(),this->mLineVector.end(),this->mSearchLine,TextLineYLess());
    if(itr==this->mLineVector.end() || (*itr)->my>y) {
        if(itr!=this->mLineVector.begin()) --itr;
    }
    return itr;
}

// FUNCTION: WON 0x00713250
// Records the index of the line at the current vertical scroll offset
void TextArea::FindTopLine() {
    LineVector::iterator itr=this->FindLineAt(this->mVertOffset);
    this->mTopLineVectorPos=itr-this->mLineVector.begin();
}

// FUNCTION: WON 0x00713290
// Returns the segment under a point (cached from the last lookup when still under it); lenient
// clamps the point into the text, and a point right of a line's end hits its last segment
TextSegmentBase* TextArea::FindSegment(int x,int y,bool lenient) {
    if(this->mLineVector.empty()) return NULL;
    if(lenient) {
        if(x>=this->mTotalWidth && this->mTotalWidth!=0) x=this->mTotalWidth-1;
        if(x<0) x=0;
        if(y>=this->mTotalHeight) y=this->mTotalHeight-1;
        if(y<0) y=0;
    }
    if(this->mLastFindSeg!=NULL) {
        TextLine* line=this->mLastFindSeg->mLine;
        int xpos=x-line->mx;
        if(line->my<=y && line->my+line->mHeight>=y) {
            if(xpos>=this->mLastFindSeg->mx && xpos<=this->mLastFindSeg->mx+this->mLastFindSeg->mWidth) return this->mLastFindSeg;
        }
    }
    LineVector::iterator lineItr=this->FindLineAt(y);
    TextLine* line=(*lineItr).get();
    if(!lenient && line->my+line->mHeight<y) return NULL;
    x-=line->mx;
    TextLine::SegmentList::iterator segItr=line->mSegmentList.begin();
    TextSegmentBase* seg=NULL;
    while(segItr!=line->mSegmentList.end()) {
        seg=segItr->get();
        if(x<=seg->mx+seg->mWidth) break;
        ++segItr;
    }
    if(seg!=NULL && !lenient && x<seg->mx) seg=NULL;
    this->mLastFindSeg=seg;
    return seg;
}

// FUNCTION: WON 0x007133d0
// Starts the selection at the character under a point, or extends it there, swapping its ends when
// the drag crosses the anchor
void TextArea::CheckSel(int x,int y,bool newStart) {
    TextSegmentBase* seg=this->FindSegment(x,y,true);
    if(seg==NULL) return;
    int selection=seg->CharAt(x-seg->mLine->mx);
    if(newStart) {
        this->mSelStartChar=this->mSelEndChar=selection;
        this->mBackwardsSel=false;
        this->Invalidate();
        return;
    }
    bool backwards=this->mBackwardsSel;
    if(backwards) this->mSelStartChar=selection;
    else this->mSelEndChar=selection;
    if(this->mSelStartChar>this->mSelEndChar) {
        this->mBackwardsSel=!backwards;
        int old=this->mSelStartChar;this->mSelStartChar=this->mSelEndChar;this->mSelEndChar=old;
    }
    this->Invalidate();
}

// FUNCTION: WON 0x00713480
// Runs a pending delayed word wrap; while selecting with the mouse outside the area, scrolls toward
// it and extends the selection; returns whether the timer is still needed
bool TextArea::TimerEvent(int) {
    if(this->mNeedRecalcWordWrap) {
        this->mDelayWordWrapTimeAccum++;
        if(this->mDelayWordWrapTimeAccum>1) {
            int chars=0;
            if(!this->mSegmentList.empty()) chars=this->mSegmentList.back()->mAbsEndChar;
            if(this->mDelayWordWrapTimeAccum>=10 || this->mDelayWordWrapTimeAccum>=chars/16000) this->RecalcWordWrap(true);
        }
    }
    if(!this->mSelecting) return this->mNeedRecalcWordWrap;
    int x,y;this->GetMousePos(x,y);
    if(x>=0 && x<=this->Width() && y>=0 && y<=this->Height()) return true;
    if(y>this->Height()) {
        int delta=y-this->Height();
        if(this->TextFlagSet(TextFlag_ScrollFullLine) && delta<this->mDownScrollAmount) delta=this->mDownScrollAmount;
        this->mVertOffset+=delta;
    } else if(y<0) {
        int delta=-y;
        if(this->TextFlagSet(TextFlag_ScrollFullLine) && delta<this->mUpScrollAmount) delta=this->mUpScrollAmount;
        this->mVertOffset-=delta;
    }
    if(x>this->Width()) this->mHorzOffset+=x-this->Width();
    else if(x<0) this->mHorzOffset+=x;
    this->AdjustPosition(false);
    this->ScrollTranslate(x,y);
    this->CheckSel(x,y,false);
    return true;
}

// FUNCTION: WON 0x007135e0
// On a left press, takes focus and presses the link under the mouse, or starts (shift: extends) a
// text selection
void TextArea::MouseDown(int x,int y,int button) {
    ScrollArea::MouseDown(x,y,button);
    if(button!=0) return;
    this->ScrollTranslate(x,y);
    this->RequestFocus();
    if(this->mCurLink!=NULL) {
        this->mCurLink->Pressed();
        this->Invalidate();
        this->mSelecting=false;
        return;
    }
    if(!this->TextFlagSet(TextFlag_AllowSelection))
        return;
    this->mSelecting=true;
    this->RequestTimer(true);
    this->CheckSel(x,y,(this->GetKeyMod()&1)==0);
}

// FUNCTION: WON 0x007138f0
// Releases a hovered link and stops selecting when the mouse leaves
void TextArea::MouseExit() {
    ScrollArea::MouseExit();
    if(this->mCurLink!=NULL) {
        this->mCurLink->Released();
        this->mCurLink=NULL;
        this->Invalidate();
    }
    this->mSelecting=false;
}

// FUNCTION: WON 0x00713930
// Tracks the link under the mouse and shows the hand cursor over it, else the text cursor (when
// selection is allowed)
void TextArea::MouseMove(int x,int y) {
    ScrollArea::MouseMove(x,y);
    this->ScrollTranslate(x,y);
    TextSegmentBase* seg=this->FindSegment(x,y,false);
    if(seg==this->mCurLink) return;
    if(seg!=NULL && seg->IsLink()) {
        this->mCurLink=seg;
        this->mParent->SetCursor(Cursor::GetStandardCursor(StandardCursor_Hand));
    } else {
        this->mCurLink=NULL;
        this->mParent->SetCursor(this->TextFlagSet(TextFlag_AllowSelection)?Cursor::GetStandardCursor(StandardCursor_IBeam):(Cursor*)NULL);
    }
}

// FUNCTION: WON 0x007139f0
// Shows the text cursor (when selection is allowed) and checks for a link under the mouse
void TextArea::MouseEnter(int x,int y) {
    ScrollArea::MouseEnter(x,y);
    this->mParent->SetCursor(this->TextFlagSet(TextFlag_AllowSelection)?Cursor::GetStandardCursor(StandardCursor_IBeam):(Cursor*)NULL);
    this->MouseMove(x,y);
}

// FUNCTION: WON 0x00713a40
// Extends the selection to the mouse while selecting
void TextArea::MouseDrag(int x,int y) {
    ScrollArea::MouseDrag(x,y);
    this->ScrollTranslate(x,y);
    if(this->mSelecting) this->CheckSel(x,y,false);
}

// FUNCTION: WON 0x00713a90
// Scrolls with the arrow and page keys and marks the view as user-scrolled; returns whether the key
// was used
bool TextArea::KeyDown(int key) {
    if(ScrollArea::KeyDown(key)) return true;
    switch(key) {
    case VK_RIGHT:this->mHorzOffset+=10;break;
    case VK_LEFT:this->mHorzOffset-=10;break;
    case VK_DOWN:this->mVertOffset+=this->mDownScrollAmount;break;
    case VK_UP:this->mVertOffset-=this->mUpScrollAmount;break;
    case VK_NEXT:this->mVertOffset+=this->mScrollArea.height;break;
    case VK_PRIOR:this->mVertOffset-=this->mScrollArea.height;break;
    default:return false;
    }
    this->mUserHasScrolled=true;
    this->AdjustPosition(false);
    return true;
}

// FUNCTION: WON 0x00713b60
// Copies the selected text of every segment to the clipboard
void TextArea::CopySelection() {
    GUIString text;
    SegmentList::iterator itr=this->mSegmentList.begin();
    while(itr!=this->mSegmentList.end()) {
        itr->get()->GetSelText(this,text);
        ++itr;
    }
    Clipboard::Set(text);
}

// FUNCTION: WON 0x00713c20
// Copies the selection on Ctrl+C; returns whether the character was used
bool TextArea::KeyChar(int key) {
    if(ScrollArea::KeyChar(key)) return true;
    if(key==3) { this->CopySelection();return true; }
    return false;
}

// FUNCTION: WON 0x00713ec0
// Appends text split at CR, LF and CRLF into one line-broken segment per line; the last line breaks
// only when lineBreak is set
void TextArea::AddFormatedText(const GUIString& text,bool lineBreak) {
    int start=0,next=0;
    while(start<text.length()) {
        int end=start;
        while(end<text.length()) {
            int ch=text[end];
            if(ch=='\r') {
                if(end+1<text.length() && text[end+1]=='\n') next=end+2;
                else next=end+1;
                break;
            } else if(ch=='\n') { next=end+1;break; }
            end++;
        }
        bool foundReturn=true;
        if(end>=text.length()) { next=text.length();foundReturn=lineBreak; }
        this->AddSegment(text.substr((unsigned short)start,(unsigned short)(end-start)),foundReturn);
        start=next;
    }
}

// FUNCTION: WON 0x00713ff0
// Sets or clears text flags; turning word wrap on relays out the area, and disallowing selection
// clears any selection
void TextArea::SetTextFlags(int flags,bool on) {
    bool wordWrap=this->TextFlagSet(TextFlag_WordWrap);
    if(on) this->mTextFlags|=flags;else this->mTextFlags&=~flags;
    if(!wordWrap && this->TextFlagSet(TextFlag_WordWrap)) this->Virtual08();
    if(this->TextFlagSet(TextFlag_AllowSelection) || this->mSelStartChar==this->mSelEndChar)
        return;
    this->mSelecting=false;
    this->mSelStartChar=this->mSelEndChar=0;
    this->Invalidate();
}

// FUNCTION: WON 0x00714090
// Sets the character limit and drops the oldest text beyond it
void TextArea::SetMaxChars(int chars) {
    this->mMaxChars=chars;
    this->EnforceMaxChars();
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x007140c0
// Sets the line limit and drops the oldest lines beyond it
void TextArea::SetMaxLines(int lines) {
    this->mMaxLines=lines;
    this->EnforceMaxLines();
    this->AdjustPosition(false);
}

// FUNCTION: WON 0x00712260
// Places a segment at the end of the last line, growing the line's height and baseline (and
// realigning its segments) as needed, then aligns the line
void TextArea::AddSeg(TextSegmentBase* segment) {
    TextLine* line=this->mLineVector.back().get();
    bool adjust=false;
    int baseline=segment->mAscent;
    if(baseline>line->mBaseline) { line->mBaseline=baseline;adjust=true; }
    int descent=segment->mHeight-segment->mAscent;
    if(descent>line->mDescent) line->mDescent=descent;
    if(segment->mHeight>line->mHeight) {
        this->mTotalHeight-=line->mHeight;
        line->mHeight=segment->mHeight;
        this->mTotalHeight+=line->mHeight;
        adjust=true;
    }
    if(line->mDescent+line->mBaseline>line->mHeight) {
        this->mTotalHeight-=line->mHeight;
        line->mHeight=line->mBaseline+line->mDescent;
        this->mTotalHeight+=line->mHeight;
        adjust=true;
    }
    if(adjust) {
        TextLine::SegmentList::iterator itr=line->mSegmentList.begin();
        while(itr!=line->mSegmentList.end()) {
            TextSegmentBase* seg=itr->get();
            seg->my=line->mBaseline-seg->mAscent;
            ++itr;
        }
    }
    segment->mx=line->mWidth;
    segment->my=line->mBaseline-segment->mAscent;
    segment->mLine=line;
    line->mSegmentList.push_back(SmartPtr<TextSegmentBase>(segment));
    line->mWidth+=segment->mWidth;
    this->AlignLine(line,this->mLineAlign);
}
// FUNCTION: WON 0x007124a0
// Sets a line's x for left, centre or right alignment in the view (never negative)
void TextArea::AlignLine(TextLine* line,HorzAlign align) {
    int x=0;
    switch(align) {
    case HorzAlign_Center:x=(this->mScrollArea.width-line->mWidth)/2;break;
    case HorzAlign_Right:x=this->mScrollArea.width-line->mWidth;break;
    }
    if(x<=0) x=0;
    line->mx=x;
}
// FUNCTION: WON 0x00712980
// Appends a segment after the last one, lays it out, enforces the character and line limits and
// keeps a bottom-scrolled view at the bottom
void TextArea::AddSegment(TextSegmentBase* segment) {
    if(!this->mSegmentList.empty()) segment->mAbsStartChar=this->mSegmentList.back()->mAbsEndChar;
    segment->mAbsEndChar=segment->mAbsStartChar+segment->GetLength();
    bool bottom=this->ShouldScrollOnBottom();
    this->mSegmentList.push_back(SmartPtr<TextSegmentBase>(segment));
    segment->CalcDimensions();
    this->AddSegmentPrv(segment);
    this->EnforceMaxChars();
    this->EnforceMaxLines();
    if(bottom) this->mVertOffset=this->mTotalHeight+this->mExtraHeight-this->mScrollArea.height;
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x00712b60
// After leading text was removed, shifts lines, character positions, selection and scroll offset
// back to start at zero and recounts total size and line breaks
void TextArea::EnforceInvariants() {
    this->mTotalWidth=0;
    this->mTotalHeight=0;
    this->mNumLines=0;
    if(this->mLineVector.empty() || this->mSegmentList.empty()) return;
    int deltaPos=this->mLineVector.front()->my;
    LineVector::iterator lineItr=this->mLineVector.begin();
    while(lineItr!=this->mLineVector.end()) {
        TextLine* line=(*lineItr).get();
        if(line->mWidth>this->mTotalWidth) this->mTotalWidth=line->mWidth;
        line->my-=deltaPos;
        this->mTotalHeight+=line->mHeight;
        ++lineItr;
    }
    int deltaChar=this->mSegmentList.front()->mAbsStartChar;
    SegmentList::iterator itr=this->mSegmentList.begin();
    while(itr!=this->mSegmentList.end()) {
        TextSegmentBase* seg=itr->get();
        seg->mAbsStartChar-=deltaChar;
        seg->mAbsEndChar-=deltaChar;
        if(seg->mLineBreak) this->mNumLines++;
        ++itr;
    }
    this->mSelStartChar-=deltaChar;
    this->mSelEndChar-=deltaChar;
    if(this->mSelStartChar<0) this->mSelStartChar=0;
    if(this->mSelEndChar<0) this->mSelEndChar=0;
    this->mVertOffset-=deltaPos;
}
// FUNCTION: WON 0x00712cd0
// Removes the lines up to the one ending past the given segment and every segment before that line,
// then renormalizes positions
void TextArea::RemoveSegmentsUpTo(TextSegmentBase* segment) {
    TextLine* findLine=segment->mLine;
    this->mLastFindSeg=NULL;
    bool found=false;
    LineVector::iterator lineItr=this->mLineVector.begin();
    while(lineItr!=this->mLineVector.end()) {
        TextLine* line=(*lineItr).get();
        if(line==findLine) found=true;
        if(found && !line->mSegmentList.empty()) {
            TextSegmentBase* seg=line->mSegmentList.back().get();
            if(seg->GetActual()!=segment) {
                segment->mLine=line;
                break;
            }
        }
        ++lineItr;
    }
    this->mLineVector.erase(this->mLineVector.begin(),lineItr);
    TextSegmentBase* stop=NULL;
    if(!this->mLineVector.empty()) {
        TextLine* line=this->mLineVector.front().get();
        if(!line->mSegmentList.empty()) stop=line->mSegmentList.front()->GetActual();
    }
    SegmentList::iterator itr=this->mSegmentList.begin();
    while(itr!=this->mSegmentList.end()) {
        TextSegmentBase* seg=itr->get();
        if(seg==stop) break;
        if(seg==this->mCurLink) this->mCurLink=NULL;
        ++itr;
    }
    this->mSegmentList.erase(this->mSegmentList.begin(),itr);
    this->EnforceInvariants();
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x00712f70
// Removes the oldest segments while the text is longer than the character limit
void TextArea::EnforceMaxChars() {
    if(this->mMaxChars<=0 || this->mSegmentList.empty()) return;
    TextSegmentBase* seg=this->mSegmentList.back().get();
    int toRemove=seg->mAbsEndChar-this->mMaxChars;
    if(toRemove<=0) return;
    SegmentList::iterator itr=this->mSegmentList.begin();
    while(itr!=this->mSegmentList.end()) {
        seg=itr->get();
        if(seg->mAbsEndChar+1>=toRemove) break;
        ++itr;
    }
    this->RemoveSegmentsUpTo(seg);
}
// FUNCTION: WON 0x00712fd0
// Removes the oldest lines while the text has more line breaks than the line limit
void TextArea::EnforceMaxLines() {
    if(this->mMaxLines<=0 || this->mSegmentList.empty()) return;
    TextSegmentBase* seg=this->mSegmentList.back().get();
    int toRemove=this->mNumLines-this->mMaxLines;
    if(toRemove<=0) return;
    int count=0;
    SegmentList::iterator itr=this->mSegmentList.begin();
    while(itr!=this->mSegmentList.end()) {
        seg=itr->get();
        if(seg->mLineBreak) count++;
        if(count>=toRemove) break;
        ++itr;
    }
    this->RemoveSegmentsUpTo(seg);
}

// FUNCTION: WON 0x00713680
// On a left release, activates a pressed link still under the mouse (sends a link-activated event),
// updates the hovered link and cursor, and ends selecting
void TextArea::MouseUp(int x,int y,int button) {
    ScrollArea::MouseUp(x,y,button);
    if(button!=0) return;
    this->ScrollTranslate(x,y);
    TextSegmentBase* seg=this->FindSegment(x,y,false);
    bool pressed=false;
    if(this->mCurLink!=NULL) pressed=this->mCurLink->Released();
    if(seg==this->mCurLink && pressed) {
        this->mLinkParam=this->mCurLink->GetLinkParam();
        this->DeliverEvent(new TextLinkActivatedEvent(this,this->mCurLink->GetActual()),false);
    }
    if(this->mCurLink!=NULL) {
        if(seg==NULL || !seg->IsLink()) {
            this->mCurLink=NULL;
            this->mParent->SetCursor(this->TextFlagSet(TextFlag_AllowSelection)?Cursor::GetStandardCursor(StandardCursor_IBeam):(Cursor*)NULL);
        } else this->mCurLink=seg;
        this->Invalidate();
    }
    this->mSelecting=false;
}

// FUNCTION: WON 0x007127d0
// Lays out one segment: runs its instructions, adds it to the last line or word-wraps it, and starts
// a new line after a line break
void TextArea::AddSegmentPrv(TextSegmentBase* segment) {
    if(this->mLineVector.empty()) this->AddLine();
    TextLine* line=this->mLineVector.back().get();
    segment->mLine=line;
    segment->PerformInstructions(this);
    if(this->Width()<=0 || line->mWidth+segment->mWidth<=this->mScrollArea.width || !this->TextFlagSet(TextFlag_WordWrap) || !this->mHasBeenVisible) {
        this->AddSeg(segment);
        if(line->mWidth>this->mTotalWidth && !this->TextFlagSet(TextFlag_WordWrap)) this->mTotalWidth=line->mWidth;
    } else segment->Wrap(this,line);
    if(segment->mLineBreak) {
        this->mNumLines++;
        this->AddLine();
    }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00713840
// (public: virtual void * __thiscall WONAPI::TextLinkActivatedEvent::`scalar deleting destructor'(unsigned int)    [??_GTextLinkActivatedEvent@WONAPI@@UA)
// TEMPLATE: WON 0x0071b800
// (public: class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::itera)
// TEMPLATE: WON 0x0071b810
// (public: class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::itera)
// TEMPLATE: WON 0x0071b830
// (public: class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::itera)
// TEMPLATE: WON 0x0071b850
// (public: bool __thiscall std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> )
// TEMPLATE: WON 0x0071d4e0
// (public: class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::itera)
// TEMPLATE: WON 0x00723de0
// (class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::iterator __cd)
// TEMPLATE: WON 0x00723eb0
// (class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::iterator __cd)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00713860
// (public: virtual __thiscall WONAPI::TextLinkActivatedEvent::~TextLinkActivatedEvent(void)    [??1TextLinkActivatedEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071d350
// (public: void __thiscall std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> )
// TEMPLATE: WON 0x0071efd0
// (public: class std::list<class WONAPI::SmartPtr<class WONAPI::TextSegmentBase>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextSegmentBas)
// TEMPLATE: WON 0x007255a0
// (class std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLine> > >::iterator __cd)
