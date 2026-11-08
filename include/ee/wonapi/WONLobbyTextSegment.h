#ifndef EE_WONAPI_WONLOBBYTEXTSEGMENT_H
#define EE_WONAPI_WONLOBBYTEXTSEGMENT_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
struct ElementSelectionInfo {
    int mStartSelSegment,mEndSelSegment;
    int mStartSelSegmentPos,mEndSelSegmentPos;
}; // 0x10 bytes; the selection readers use all four fields
// ElementFindInfo (0x34 bytes; 0x0070cd70 constructs and copies the whole record): owned GUIString
// at +0x10, case-sensitive flag at +0x14, find-previous flag at +0x15, and scalar outputs.
struct ElementFindInfo {
    int mStartSelSegment,mEndSelSegment;
    int mStartSelSegmentPos,mEndSelSegmentPos;
    GUIString mFindText; //10
    bool mCaseSensitive,mFindPrev; // offsets 0x14, 0x15 (read at 0x0070ce3c and 0x0070ce42)
    int mCurFoundLength; //18
    int mFoundStartSegment,mFoundEndSegment; //1c,20
    int mFoundStartChar,mFoundEndChar; //24,28
    int mFoundX,mFoundY; //2c,30
};
// TextLineSegment (0x28 bytes), initialized inline.
class TextLineSegment : public LineSegment {
public:
    TextLineSegment() :mWidth(0),mMinWidth(0) {} // inline default constructor
    TextLineSegment(const GUIString& text,int color,Font* font)
        :mText(text),mColor(color),mFont(font),mWidth(0),mMinWidth(0) {} // inline (as at 0x00739595..0x007395cd)
    virtual int GetWidth(); // cached getter (0x00709ed0)
    virtual int GetMinWidth(); //309e70
    virtual int GetHeight(); // at 0x00739720
    virtual int GetAscent(); // at 0x00739730
    virtual int GetFirstWrapWidth(); //309f00
    virtual void MouseEnter(int,int); // at 0x0070b310
    virtual int GetSelChar(int,int); // forwarding overload (0x00709f90)
    int GetSelChar(int,int,int,int); //309f40 substantive hit-position algorithm
    virtual bool GetSelCharPos(int&,int&,int,int); //30a260
    virtual void GetSelText(GUIString&,ElementSelectionInfo&); //309fc0
    virtual bool FindStr(ElementFindInfo&); // at 0x0070a3f0
    bool FindPartial(ElementFindInfo&,int,int); //30a320
    virtual void Wrap(MultiLineBuilder*,bool); // at 0x0070a590
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); // forwarding overload (0x0070a900)
    void Paint(Graphics&,int,int,int,int,int,int,int,ElementPaintInfo&); // paints the text and selection, at 0x0070a950
    void PaintSelection(Graphics&,int,int,int,int,int,int,int,ElementPaintInfo&); // paints the selection background only
    GUIString mText; // offset 0x14, retained
    int mColor; //18
    SmartPtr<Font> mFont; // offset 0x1c, retained
    int mWidth,mMinWidth; //20,24
};
}
#endif
