// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
using namespace WONAPI;

// Defined before AddLine in the original object (no delete-on-throw guard around `new TextLine`).
// FUNCTION: WON 0x00711690
// Creates an empty text line at the origin with zero size
TextLine::TextLine()
{
    this->mx=this->my=0;
    this->mHeight=0;
    this->mWidth=0;
    this->mBaseline=0;
    this->mDescent=0;
}
// FUNCTION: WON 0x007124e0
// Appends a line of the given height and baseline below the last line, grows the total height and
// returns the line
TextLine* TextArea::AddLine(int height,int baseline)
{
    SmartPtr<TextLine> line=new TextLine;
    line->mHeight=height;
    line->mBaseline=baseline;
    if(!this->mLineVector.empty())
    {
        TextLine* previous=this->mLineVector.back().get();
        line->my=previous->my+previous->mHeight;
    }
    this->mLineVector.push_back(line);
    this->mTotalHeight+=height;
    return line.get();
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00711710
// (public: virtual void * __thiscall WONAPI::TextLine::`scalar deleting destructor'(unsigned int)    [??_GTextLine@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071d3b0
// (protected: void __thiscall std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLin)
// TEMPLATE: WON 0x00722350
// (protected: void __thiscall std::deque<class WONAPI::SmartPtr<class WONAPI::TextLine>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TextLin)
