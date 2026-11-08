// Multi-line element line and floating-segment lookup by position (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
struct LineYLess {
    bool operator()(const SmartPtr<LineElement>& a,const SmartPtr<LineElement>& b) const { return a->my<b->my; }
};
struct FloatingYLess {
    bool operator()(const FloatingSegment& a,const FloatingSegment& b) const { return a.my<b.my; }
};
// FUNCTION: WON 0x0070d040
// Returns the line containing height y (binary search; the last line above y, or the first line)
MultiLineElement::LineVector::iterator MultiLineElement::FindLineAt(int y)
{
    this->mSearchLine->my=y;
    LineVector::iterator line=std::lower_bound(this->mLineVector.begin(),this->mLineVector.end(),this->mSearchLine,LineYLess());
    if(line==this->mLineVector.end() || line->get()->my>y)
    {
        if(line!=this->mLineVector.begin()) --line;
    }
    return line;
}
// FUNCTION: WON 0x0070d0c0
// Returns the left or right floating segment under a point (edges inclusive), or NULL
FloatingSegment* MultiLineElement::FindFloatingSegmentAt(int x,int y,bool left)
{
    FloatingVector* items=left?&this->mFloatingItems[0]:&this->mFloatingItems[1];
    FloatingVector::iterator item=std::lower_bound(items->begin(),items->end(),FloatingSegment(NULL,x,y),FloatingYLess());
    if(item==items->end() || item->my>y)
    {
        if(item!=items->begin()) --item;
    }
    if(item==items->end()) return NULL;
    FloatingSegment& segment=*item;
    if(segment.mx>x || segment.mx+segment.GetWidth()<x || segment.my+segment.GetHeight()<y)
        return NULL;
    return &segment;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00723be0
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::LineElement>,int,class WONAPI::SmartPtr<class WONAPI::LineElement> *,class WONAPI::SmartPtr<cla)
// TEMPLATE: WON 0x00723c20
// (class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::FloatingSegment )
// TEMPLATE: WON 0x007253f0
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::LineElement>,int,class WONAPI::SmartPtr<class WONAPI::LineElement> *,class WONAPI::SmartPtr<cla)
// TEMPLATE: WON 0x00725480
// (class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,struct WONAPI::FloatingSegment )
// TEMPLATE: WON 0x00725df0
// (void __cdecl std::_Distance(class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,str)
// TEMPLATE: WON 0x00725f50
// (void __cdecl std::_Distance(class std::_Ptrit<struct WONAPI::FloatingSegment,int,struct WONAPI::FloatingSegment *,struct WONAPI::FloatingSegment &,str)
