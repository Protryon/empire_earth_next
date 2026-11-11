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
#include "rust/ee_rust.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;
// The lines' metrics, alignment and wrap test, the bottom rule, the delayed wrap, the limits' trims and the
// renumbering after them, the segment and selection under a point and the scrolls are ee_won's guitext module
// (rust/crates/ee_won/src/guitext.rs), the line at a y and its visibility guilist's; this unit keeps the lines,
// segments, fonts, links, cursors, timers, events and the paint. The Rust reads the lines through these:
static int32_t TextLineTopOf(const void* inLines, uintptr_t i)
{
    return (*static_cast<const TextArea::LineVector*>(inLines))[i]->my;
}
struct TextRemoveSearch { const TextArea::LineVector* mLines; TextLine* mFindLine; TextSegmentBase* mSegment; };
static uint32_t TextRemoveFlagsOf(const void* inSearch, uintptr_t i)
{
    const TextRemoveSearch* aSearch = static_cast<const TextRemoveSearch*>(inSearch);
    TextLine* aLine = (*aSearch->mLines)[i].get();
    uint32_t aFlags = 0;
    if (aLine == aSearch->mFindLine) aFlags |= 1;
    if (!aLine->mSegmentList.empty()) {
        aFlags |= 2;
        if (aLine->mSegmentList.back()->GetActual() == aSearch->mSegment) aFlags |= 4;
    }
    return aFlags;
}

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
        if(ee_won_gui_text_delay_wrap(this->mTextFlags,this->mSegmentList.back()->mAbsEndChar)) {
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
            this->mExtraHeight=ee_won_gui_list_full_line_extra(this->mTotalHeight,this->mScrollArea.height,line->my,line->mHeight);
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
    EeWonTextLineY aLast={0,0},aPrev={0,0};
    size_t aCount=this->mLineVector.size();
    if(aCount>0) { aLast.y=this->mLineVector.back()->my;aLast.height=this->mLineVector.back()->mHeight; }
    if(aCount>1) { aPrev.y=this->mLineVector[aCount-2]->my;aPrev.height=this->mLineVector[aCount-2]->mHeight; }
    return ee_won_gui_text_scroll_on_bottom(this->mTextFlags,this->mUserHasScrolled,aCount,aLast,aPrev,
                                            this->mVertOffset,this->mScrollArea.height);
}

// FUNCTION: WON 0x00713030
// Returns whether a line is inside the view (partly, when partial lines are shown)
bool TextArea::IsLineVisible(TextLine* line) {
    return ee_won_gui_list_visible(line->my,line->mHeight,this->mVertOffset,this->mScrollArea.height,
                                   this->TextFlagSet(TextFlag_ShowPartialLines));
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

// a line's place for the draw plan
static EeWonRowPlace TextLinePlaceOf(const void* ctx,size_t index) {
    const TextArea::LineVector& lines=*(const TextArea::LineVector*)ctx;
    EeWonRowPlace r;
    r.x=lines[index]->mx;
    r.top=lines[index]->my;
    r.height=lines[index]->mHeight;
    return r;
}

// FUNCTION: WON 0x007130d0
// Paints the segments of every visible line, starting at the top line
void TextArea::ScrollPaint(Graphics& g) {
    // each visible line from the top line, translated to its place, its segments painted (guidraw.rs text_lines)
    WONDrawOps ops([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_text_lines((size_t)this->mTopLineVectorPos,
        this->mLineVector.size(),this->mVertOffset,this->mScrollArea.height,this->TextFlagSet(TextFlag_ShowPartialLines),
        TextLinePlaceOf,&this->mLineVector,o,n); });
    WONDrawRun(g,ops,WONDrawSlots(),[&](const EeWonDrawOp& o) {
        TextLine* line=this->mLineVector[o.b].get();
        for(TextLine::SegmentList::iterator segItr=line->mSegmentList.begin();segItr!=line->mSegmentList.end();++segItr)
            segItr->get()->Paint(g,this);
    });
}

// FUNCTION: WON 0x007131b0
// Returns the line containing height y (the last line starting at or above y)
TextArea::LineVector::iterator TextArea::FindLineAt(int y) {
    this->mSearchLine->my=y;
    return this->mLineVector.begin()+ee_won_gui_list_line_at(this->mLineVector.size(),y,TextLineTopOf,&this->mLineVector);
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
    if(lenient) ee_won_gui_text_find_clamp(&x,&y,this->mTotalWidth,this->mTotalHeight);
    if(this->mLastFindSeg!=NULL) {
        TextLine* line=this->mLastFindSeg->mLine;
        if(ee_won_gui_text_find_cached(x,y,line->mx,line->my,line->mHeight,this->mLastFindSeg->mx,this->mLastFindSeg->mWidth))
            return this->mLastFindSeg;
    }
    LineVector::iterator lineItr=this->FindLineAt(y);
    TextLine* line=(*lineItr).get();
    std::vector<int> aXs,aWidths;
    std::vector<TextSegmentBase*> aSegs;
    for(TextLine::SegmentList::iterator segItr=line->mSegmentList.begin();segItr!=line->mSegmentList.end();++segItr) {
        aXs.push_back((*segItr)->mx);
        aWidths.push_back((*segItr)->mWidth);
        aSegs.push_back(segItr->get());
    }
    int64_t aPick=ee_won_gui_text_find_pick(x-line->mx,y,line->my,line->mHeight,aXs.empty()?NULL:&aXs[0],
                                            aWidths.empty()?NULL:&aWidths[0],aXs.size(),lenient);
    if(aPick==-2) return NULL;
    TextSegmentBase* seg=aPick<0?NULL:aSegs[(size_t)aPick];
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
    ee_won_gui_text_check_sel(&this->mSelStartChar,&this->mSelEndChar,&this->mBackwardsSel,selection,newStart);
    this->Invalidate();
}

// FUNCTION: WON 0x00713480
// Runs a pending delayed word wrap; while selecting with the mouse outside the area, scrolls toward
// it and extends the selection; returns whether the timer is still needed
bool TextArea::TimerEvent(int) {
    if(this->mNeedRecalcWordWrap) {
        this->mDelayWordWrapTimeAccum++;
        int chars=0;
        if(!this->mSegmentList.empty()) chars=this->mSegmentList.back()->mAbsEndChar;
        if(ee_won_gui_text_wrap_due(this->mDelayWordWrapTimeAccum,chars)) this->RecalcWordWrap(true);
    }
    if(!this->mSelecting) return this->mNeedRecalcWordWrap;
    int x,y;this->GetMousePos(x,y);
    if(!ee_won_gui_text_drag_scroll(x,y,this->Width(),this->Height(),this->TextFlagSet(TextFlag_ScrollFullLine),
                                    this->mDownScrollAmount,this->mUpScrollAmount,&this->mVertOffset,&this->mHorzOffset))
        return true;
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
    if(!ee_won_gui_text_key_scroll(key,&this->mVertOffset,&this->mHorzOffset,this->mDownScrollAmount,
                                   this->mUpScrollAmount,this->mScrollArea.height))
        return false;
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
    std::vector<EeWonTextPart> aParts(text.length()+1);
    size_t aCount=ee_won_gui_text_split(text.mData->mText,text.length(),sizeof(wchar_t),lineBreak,&aParts[0],aParts.size());
    for(size_t i=0;i<aCount && i<aParts.size();i++)
        this->AddSegment(text.substr((unsigned short)aParts[i].start,(unsigned short)aParts[i].len),aParts[i].line_break);
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
    EeWonTextLineM aMetrics={line->mHeight,line->mBaseline,line->mDescent};
    bool adjust=ee_won_gui_text_add_seg(&aMetrics,segment->mHeight,segment->mAscent,&this->mTotalHeight);
    line->mHeight=aMetrics.height;
    line->mBaseline=aMetrics.baseline;
    line->mDescent=aMetrics.descent;
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
    line->mx=ee_won_gui_text_align_x(this->mScrollArea.width,line->mWidth,(int)align);
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
    std::vector<int> aWidths,aHeights,aYs,aStarts,aEnds;
    std::vector<char> aBreaks;
    for(LineVector::iterator lineItr=this->mLineVector.begin();lineItr!=this->mLineVector.end();++lineItr) {
        aWidths.push_back((*lineItr)->mWidth);
        aHeights.push_back((*lineItr)->mHeight);
        aYs.push_back((*lineItr)->my);
    }
    for(SegmentList::iterator itr=this->mSegmentList.begin();itr!=this->mSegmentList.end();++itr) {
        aStarts.push_back((*itr)->mAbsStartChar);
        aEnds.push_back((*itr)->mAbsEndChar);
        aBreaks.push_back((*itr)->mLineBreak);
    }
    int aTotals[3];
    ee_won_gui_text_renumber(&aWidths[0],&aHeights[0],&aYs[0],aYs.size(),&aStarts[0],&aEnds[0],(const bool*)&aBreaks[0],
                             aStarts.size(),&this->mSelStartChar,&this->mSelEndChar,&this->mVertOffset,aTotals);
    size_t i=0;
    for(LineVector::iterator lineItr=this->mLineVector.begin();lineItr!=this->mLineVector.end();++lineItr,++i)
        (*lineItr)->my=aYs[i];
    i=0;
    for(SegmentList::iterator itr=this->mSegmentList.begin();itr!=this->mSegmentList.end();++itr,++i) {
        (*itr)->mAbsStartChar=aStarts[i];
        (*itr)->mAbsEndChar=aEnds[i];
    }
    this->mTotalWidth=aTotals[0];
    this->mTotalHeight=aTotals[1];
    this->mNumLines=aTotals[2];
}
// FUNCTION: WON 0x00712cd0
// Removes the lines up to the one ending past the given segment and every segment before that line,
// then renormalizes positions
void TextArea::RemoveSegmentsUpTo(TextSegmentBase* segment) {
    TextLine* findLine=segment->mLine;
    this->mLastFindSeg=NULL;
    TextRemoveSearch aSearch={&this->mLineVector,findLine,segment};
    LineVector::iterator lineItr=this->mLineVector.begin()+
        ee_won_gui_text_remove_stop(this->mLineVector.size(),TextRemoveFlagsOf,&aSearch);
    if(lineItr!=this->mLineVector.end()) segment->mLine=lineItr->get();
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
    if(this->mSegmentList.empty()) return;
    int toRemove=ee_won_gui_text_chars_excess(this->mMaxChars,this->mSegmentList.back()->mAbsEndChar);
    if(toRemove<=0) return;
    std::vector<int> aEnds;
    std::vector<TextSegmentBase*> aSegs;
    for(SegmentList::iterator itr=this->mSegmentList.begin();itr!=this->mSegmentList.end();++itr) {
        aEnds.push_back((*itr)->mAbsEndChar);
        aSegs.push_back(itr->get());
    }
    this->RemoveSegmentsUpTo(aSegs[ee_won_gui_text_chars_cut(&aEnds[0],aEnds.size(),toRemove)]);
}
// FUNCTION: WON 0x00712fd0
// Removes the oldest lines while the text has more line breaks than the line limit
void TextArea::EnforceMaxLines() {
    if(this->mSegmentList.empty()) return;
    int toRemove=ee_won_gui_text_lines_excess(this->mMaxLines,this->mNumLines);
    if(toRemove<=0) return;
    std::vector<char> aBreaks;
    std::vector<TextSegmentBase*> aSegs;
    for(SegmentList::iterator itr=this->mSegmentList.begin();itr!=this->mSegmentList.end();++itr) {
        aBreaks.push_back((*itr)->mLineBreak);
        aSegs.push_back(itr->get());
    }
    this->RemoveSegmentsUpTo(aSegs[ee_won_gui_text_lines_cut((const bool*)&aBreaks[0],aBreaks.size(),toRemove)]);
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
    if(ee_won_gui_text_seg_fits(this->Width(),line->mWidth,segment->mWidth,this->mScrollArea.width,
                                this->TextFlagSet(TextFlag_WordWrap),this->mHasBeenVisible)) {
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
