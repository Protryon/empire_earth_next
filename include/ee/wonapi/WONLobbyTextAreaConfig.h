#ifndef EE_WONAPI_WONLOBBYTEXTAREACONFIG_H
#define EE_WONAPI_WONLOBBYTEXTAREACONFIG_H
#include "ee/wonapi/WONLobbyResourceConfig.h"
namespace WONAPI {
// TextBox records. TextSegmentBase (0x2c) and TextAlignInstruction (0x30) are complete for the
// TextArea instructions; the base constructor and destructor are out of line.
class TextArea;
class TextLine;
class TextSegmentBase : public RefCount {
public:
    TextSegmentBase(); // at 0x00710a30 (0x2c bytes)
    virtual int GetLength(); //04
    virtual void CalcDimensions(); //08
    virtual void PerformInstructions(TextArea*); //0c
    virtual void GetSelText(TextArea*,GUIString&); //10
    virtual void Paint(Graphics&,TextArea*); //14
    virtual int CharAt(int); //18
    virtual void Wrap(TextArea*,TextLine*); //1c
    virtual bool IsLink(); //20
    virtual void Pressed(); //24
    virtual bool Released(); //28
    virtual const GUIString& GetLinkParam(); //2c
    virtual TextSegmentBase* GetActual(); //30
    TextLine* mLine; //08 borrowed
    int mAbsStartChar,mAbsEndChar; //0c/10
    int mWidth,mHeight,mAscent,mx,my; //14/18/1c/20/24
    bool mLineBreak; //28, natural29..2b
};
// TextAlignInstruction (0x30 bytes, allocated at 0x00712756): base (0x2c) plus the alignment at +0x2c.
// The implicit constructor calls TextSegmentBase (0x00710a30); the implicit destructor shares the
// base's (0x00738c70).
class TextAlignInstruction : public TextSegmentBase {
public:
    virtual void PerformInstructions(TextArea*); // slot 0x0c, at 0x00711680
    HorzAlign mAlign; // offset 0x2c; class size 0x30
};
// TextWidthInstruction (0x30 bytes) (not identified in the original exe).
class TextWidthInstruction : public TextSegmentBase {
public:
    TextWidthInstruction(int width=0) :mLineShouldBeAtLeastThisWidth(width) {}
    virtual void PerformInstructions(TextArea*);
    int mLineShouldBeAtLeastThisWidth;
};
// TextAnchorInstruction (0x30 bytes) (not identified in the original exe).
class TextAnchorInstruction : public TextSegmentBase 
{
public:
	GUIString mAnchorName;
	TextAnchorInstruction(const GUIString &theAnchorName) : mAnchorName(theAnchorName) { }
	virtual void PerformInstructions(TextArea *theTextArea);	
};
// TextSegment (0x38 bytes): allocated at 0x00713c9e, constructor 0x00710c40, destructor 0x00710ce0.
class TextSegment : public TextSegmentBase {
public:
    TextSegment(); // at 0x00710c40
    virtual int GetLength() { return this->mText.length(); } // inline (body at 0x00710cb0, slot 0x04)
    virtual void CalcDimensions(); //310d70
    virtual void GetSelText(TextArea*,GUIString&); //310db0
    void Paint(Graphics&,TextArea*,int,int,int,int,int); //311200
    virtual void Paint(Graphics&,TextArea*); //311330
    int CharAt(int,int,int); //311360
    virtual int CharAt(int); //3113c0
    virtual void Wrap(TextArea*,TextLine*); //310fb0
    GUIString mText; //2c realowner
    int mColor; //30
    SmartPtr<Font> mFont; //34 realowner
};
// TextLink (0x48 bytes): TextSegment (0x38) + owned GUIString at +0x38 + bool at +0x3c +
// DWORDs at +0x40/+0x44. Constructor 0x007114f0, vtable 0x00850354; implicit destructor 0x00711590.
class TextLink : public TextSegment {
public:
    TextLink();
    virtual bool IsLink();
    virtual void Pressed();
    virtual bool Released();
    virtual const GUIString& GetLinkParam();
    GUIString mLinkParam;
    bool mPressed;
    unsigned long mDownColor,mNormalColor;
};
// TextSegmentPiece (0x38 bytes): allocated at 0x007110b9 and 0x00711191, constructor 0x007113f0. Its own
// fields are borrowed or scalar; the implicit destructor is the shared base cleanup 0x00738c70.
class TextSegmentPiece : public TextSegmentBase {
public:
    TextSegmentPiece(TextSegment*,int,int,int); // at 0x007113f0
    virtual void Paint(Graphics&,TextArea*); //3114a0
    virtual int CharAt(int); //3114d0
    virtual bool IsLink(); // at 0x00711430
    virtual void Pressed(); //311440
    virtual bool Released(); //311450
    virtual const GUIString& GetLinkParam(); //311460
    virtual TextSegmentBase* GetActual(); //311470
    TextSegment* mSeg; // offset 0x2c, borrowed (not retained)
    int GetStartChar() { return this->mStartChar; } // inline
    int GetEndChar() { return this->mEndChar; } // inline
    int mStartChar,mEndChar; //30/34
};
class TextLine : public RefCount {
public:
    TextLine(); // at 0x00711690
    virtual ~TextLine(); // at 0x00711730 (vtable 0x0085038c, one slot)
    int mx,my,mWidth,mHeight,mBaseline,mDescent; //08..1c
    typedef std::list<SmartPtr<TextSegmentBase> > SegmentList;
    SegmentList mSegmentList; //20..2b originalexternal lifetime
};
// TextLinkActivatedEvent (0x24 bytes, allocated at 0x00713753), RTTI 0x008504ac, two slots.
// Retains the segment through a SmartPtr at +0x20.
class TextLinkActivatedEvent : public ComponentEvent {
public:
    TextLinkActivatedEvent(Component* component,TextSegmentBase* link)
      :ComponentEvent(component,ComponentEvent_TextLinkActivated),mLink(link) {}
    SmartPtr<TextSegmentBase> mLink; //20
};
enum TextFlags {
    TextFlag_WordWrap=0x01,TextFlag_AllowSelection=0x02,TextFlag_ScrollOnBottom=0x04,
    TextFlag_ShowPartialLines=0x08,TextFlag_ScrollFullLine=0x10,TextFlag_DelayWordWrap=0x20
};
// TextArea (0x198 bytes, 55 virtual slots): constructor at 0x007117c0, destructor at 0x00711be0.
class TextArea : public ScrollArea {
public:
    TextArea(); // at 0x007117c0
    virtual ~TextArea(); // at 0x00711be0
    virtual void Virtual08(); //312030 SizeChanged
    virtual void MouseEnter(int,int); //3139f0 MouseEnter
    virtual void MouseExit(); //3138f0 MouseExit
    virtual void MouseDown(int,int,int); //3135e0 MouseDown
    virtual void MouseUp(int,int,int);   // at 0x00713680
    virtual void MouseMove(int,int); //313930 MouseMove
    virtual void MouseDrag(int,int); //313a40 MouseDrag
    virtual bool TimerEvent(int); //313480 TimerEvent
    virtual bool KeyChar(int);   //313c20 KeyChar
    virtual bool KeyDown(int);   //313a90 KeyDown
    virtual void NotifyRootVisibility(bool); //313080 NotifyRootVisibility
    virtual void AdjustPosition(bool); //3120f0 AdjustPosition
    virtual void ScrollPaint(Graphics&); //3130d0 ScrollPaint
    virtual void CheckSel(int,int,bool);  //3133d0 CheckSel, oneactual appendedslot
    void Reset(bool=false); //311df0
    void RecalcWordWrap(bool); //311fc0
    void AddSegmentPrv(TextSegmentBase*); //3127d0
    TextLine* AddLine(int=0,int=0); // at 0x007124e0
    void AddSeg(TextSegmentBase*); //312260
    void AlignLine(TextLine*,HorzAlign); //3124a0
    void AddSegment(TextSegmentBase*); //312980
    void EnforceInvariants(); //312b60
    void RemoveSegmentsUpTo(TextSegmentBase*); //312cd0
    bool ShouldScrollOnBottom(); //3128c0
    bool IsLineVisible(TextLine*); //313030
    void FindTopLine(); //313250
    TextSegmentBase* FindSegment(int,int,bool); //313290
    void CopySelection(); //313b60
    void EnforceMaxChars(); // at 0x00712f70
    void EnforceMaxLines(); // at 0x00712fd0
    void AddImage(Image*,const GUIString&,bool=false); // adds a retained inline-image segment
    void AddSegment(const GUIString&,bool=false); // at 0x00713c60
    void AddSegment(const GUIString&,int,bool=false); // at 0x00713c80
    void AddSegment(const GUIString&,int,Font*,bool=false); // at 0x00713da0
    void LineBreak(int=0); // at 0x00712690
    void AddLink(const GUIString&,const GUIString&,bool=false);
    void AddAnchor(const GUIString&,int);
    bool GotoAnchor(const GUIString&);
    void Clear() { this->Reset(); }
    void SetFont(Font* font) { this->mFont=font; }
    void SetLinkFont(Font* font) { this->mLinkFont=font; }
    void SetWrapIndentSize(int size) { this->mWrapIndentSize=size; }
    int GetNumLines() { return this->mLineVector.size(); }
    void AddFormatedText(const GUIString&,bool=false); // at 0x00713ec0 (SDK spelling)
    void SetTextFlags(int,bool); // at 0x00713ff0
    void SetMaxChars(int); // at 0x00714090
    void SetMaxLines(int); // at 0x007140c0
    void SetLineAlignment(HorzAlign); // at 0x00712730
    typedef std::list<SmartPtr<TextSegmentBase> > SegmentList;
    typedef std::deque<SmartPtr<TextLine> > LineVector;
    LineVector::iterator FindLineAt(int); //3131b0
    SegmentList mSegmentList; //110..11b originalowned list12
    LineVector mLineVector; //11c..12f originalowned deque20
    int mTopLineVectorPos,mMaxChars,mMaxLines,mNumLines; //130/134/138/13c
    bool mNeedRecalcWordWrap; //140, natural141..143
    int mDelayWordWrapTimeAccum; //144
    TextSegmentBase *mCurLink,*mLastFindSeg; //148/14c borrowed
    GUIString mLinkParam; //150 originalowner
    bool mSelecting,mBackwardsSel,mHasBeenVisible; //154/155/156, natural157
    int mSelStartChar,mSelEndChar; //158/15c
    SmartPtr<TextLine> mSearchLine; //160 originalowned searchitem
    int mTextFlags; // offset 0x164; see SetTextFlags (0x00713ff0)
    bool TextFlagSet(int theFlag) { return this->mTextFlags&theFlag?true:false; }
    SmartPtr<Font> mFont,mLinkFont; // offsets 0x168, 0x16c, retained
    int mTextColor,mLinkColor; //170/174
    SelectionColor mSelColor; // offsets 0x178..0x183 (12 bytes)
    int mWrapIndentSize; //184
    HorzAlign mLineAlign; //188
    typedef std::map<GUIString,long> AnchorMap;
    AnchorMap mAnchorMap; // offsets 0x18c..0x197 (map, 12 bytes)
};
class TextAreaConfig : public ScrollAreaConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0079b4d0
    static Component* CompFactory() { return new TextArea; } // inline (body at 0x0079b470)
    virtual bool Virtual24(const std::string&); //395990
    virtual void SetComponent(Component*); // override, at 0x0078f860
    TextArea* mTextArea; //9c, borrowed prefixa0 only
};
}
#endif
