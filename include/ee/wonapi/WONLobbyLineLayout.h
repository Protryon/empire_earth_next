#ifndef EE_WONAPI_WONLOBBYLINELAYOUT_H
#define EE_WONAPI_WONLOBBYLINELAYOUT_H
#include "ee/wonapi/WONLobbyTextWrap.h"
namespace WONAPI {
// LineElement (0x38 bytes): constructor at 0x0070b7c0, destructor at 0x0070b8a0; the builder at
// 0x0070c087 allocates it. Base LineSegment (0x14). The default constructor is defined in
// TextLayoutConstructors.cpp.
class LineElement : public LineSegment {
public:
    LineElement(); // at 0x0070b7c0
    virtual int GetWidth(); // getter at 0x00711470
    virtual int GetHeight(); // getter at 0x0071a670
    virtual int GetAscent(); // getter at 0x0070b870
    virtual LineSegment* GetSegmentAt(int&,int&,bool); //30b920
    virtual void SetViewport(ScrollAreaViewport*,int,int); //30ba00
    virtual void SetSelIndex(); //30bab0
    virtual void GetSelText(GUIString&,ElementSelectionInfo&); //30bb10
    virtual bool GetSelCharPos(int&,int&,int,int); //30bb70
    virtual bool FindStr(ElementFindInfo&); //30bbd0
    virtual int GetEndSelIndex(); // getter at 0x0070baf0
    virtual void Paint(Graphics&,int,int,ElementPaintInfo&); //30bc40 target has no PaintSelection vslot/prepass
    typedef std::list<SmartPtr<LineSegment> > SegmentList;
    SegmentList mSegmentList; // offsets 0x14..0x1f, retained list
    bool mIsGlueLine; //20 natural padding21..23
    int mLeftMargin,mBaseline,mWidth,mHeight,my; //24,28,2c,30,34
};
inline bool LineBuilder::IsEmpty() { return this->mLine.get()?this->mLine->mSegmentList.empty():true; }
inline void LineBuilder::Clear() { this->mLine=NULL; }
}
#endif
