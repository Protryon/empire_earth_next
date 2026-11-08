#ifndef EE_WONAPI_WONLOBBYTEXTWRAP_H
#define EE_WONAPI_WONLOBBYTEXTWRAP_H
#include "ee/wonapi/WONLobbyTextSegment.h"
namespace WONAPI {
class LineElement;
class LineBuilder {
public:
    int mMaxAscent,mMaxDescent,mTopAlignHeight,mWidth; //00..0f
    SmartPtr<LineElement> mLine; // offset 0x10; class size 0x14
    int GetWidth() { return this->mWidth; }
    void AddSegment(LineSegment*); // builds the line metrics, at 0x0070b5d0
    void StartBuild(LineElement*); // at 0x0070b560
    void FinishBuild(); // at 0x0070b790
    int GetHeight() { return this->mMaxAscent+this->mMaxDescent>=this->mTopAlignHeight?this->mMaxAscent+this->mMaxDescent:this->mTopAlignHeight; }
    inline void Clear(); // defined in WONLobbyLineLayout.h after LineElement (MultiLineBuilder 0x70c000 inlines it)
    inline bool IsEmpty(); // defined in WONLobbyLineLayout.h after LineElement (HTMLDisplayGen 0x7384a0 inlines it)
};
// MultiLineBuilder (0x80 bytes): constructor 0x0070bd60, destructor 0x0070d3e0. There is no later-SDK
// mCurSelIndex; the lists sit at +0x50/+0x5c/+0x68/+0x74.
class MultiLineBuilder {
public:
    typedef std::vector<SmartPtr<LineElement> > LineVector;
    typedef std::list<SegmentInfo> SegmentList;
    typedef std::vector<FloatingSegment> FloatingVector;
    typedef std::list<LineSegment*> LineSegmentList;
    typedef std::list<FloatingSegment> FloatingList;
    LineVector* mLineVector; //00 borrowed
    SegmentList* mSegmentList; //04 borrowed
    int mWidth,mHeight,mSpecifiedWidth; //08,0c,10
    int mLeftMargin,mRightMargin; //14,18
    int mMaxLeftHeight,mMaxRightHeight; //1c,20
    int mLeftIndent,mRightIndent; //24,28
    HorzAlign mAlignment; //2c
    LineBuilder mCurLine; // offsets 0x30..0x43, retained current line at +0x40
    FloatingVector* mLeftItems; //44 borrowed
    FloatingVector* mRightItems; //48 borrowed
    bool mHaveDeferredItems; //4c naturalpadding4d..4f
    LineSegmentList mDeferredLeftItems,mDeferredRightItems; //50..5b,5c..67 borrowed payload pointers
    FloatingList mCurLeftItems,mCurRightItems; // offsets 0x68..0x73 and 0x74..0x7f, retained floating records
    MultiLineBuilder(); // at 0x0070bd60 (compiler-generated)
    int GetCurMaxLineWidth() { return this->mRightMargin-this->mLeftMargin; }
    LineBuilder* CurLine() { return &this->mCurLine; }
    void SetLineAlignment(HorzAlign align) { this->mAlignment=align; } // inline (the callback at 0x007099c0 writes +0x2c)
    void SetIndentSize(int,int); // at 0x0070be50
    void AlignCurLine(); //30be70
    bool FlushDeferredItems(); //30bf20
    LineElement* AddLine(bool=false,bool=false); //30c000
    void AddSegmentToCurLine(LineSegment*); //30c150
    void CalcMargins(); //30c1b0
    void AddFloatingSegment(LineSegment*,bool); //30c310
    void AddSegments(SegmentList*,LineVector*,FloatingVector*,FloatingVector*,int); // at 0x0070c560 (five arguments)
};
// TextPieceLineSegment (0x24 bytes; allocated at 0x0070a67d and 0x0070a737, the inline constructor
// writes +0x14..+0x20). Base LineSegment (0x14): RefCount (8) + ints at +0x08/+0x0c/+0x10, 32 slots.
// mSegment is borrowed (not retained); the implicit destructor has no owned fields.
class TextPieceLineSegment : public LineSegment {
public:
    TextLineSegment* mSegment; //14 borrowed
    int mStartChar,mEndChar,mWidth; //18,1c,20
    TextPieceLineSegment(TextLineSegment* segment,int start,int end,int width) :
        mSegment(segment),mStartChar(start),mEndChar(end),mWidth(width) { this->mStartSelIndex=segment->mStartSelIndex; }
    virtual void MouseEnter(int x,int y) { this->mSegment->MouseEnter(x,y); }
    virtual void MouseExit() { this->mSegment->MouseExit(); }
    virtual void MouseDown(int x,int y,MouseButton b) { this->mSegment->MouseDown(x,y,b); }
    virtual void MouseUp(int x,int y,MouseButton b) { this->mSegment->MouseUp(x,y,b); }
    virtual void MouseMove(int x,int y) { this->mSegment->MouseMove(x,y); }
    virtual void MouseDrag(int x,int y) { this->mSegment->MouseDrag(x,y); }
    virtual bool IsInvalid() { return this->mSegment->IsInvalid(); }
    virtual int GetSelChar(int,int); // forwarding (0x0070b330)
    virtual bool GetSelCharPos(int&,int&,int,int); //30a2c0
    virtual bool CanStartSelection() { return this->mSegment->CanStartSelection(); }
    virtual int GetWidth() { return this->mWidth; }
    virtual int GetHeight() { return this->mSegment->GetHeight(); }
    virtual int GetAscent() { return this->mSegment->GetAscent(); }
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); // forwarding (0x0070b350)
};
}
#endif
