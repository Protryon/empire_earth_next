// Multi-line element constructor (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLineLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x0070c7e0
// Creates an empty multi-line element with zero widths and its own search line
MultiLineElement::MultiLineElement()
{
    this->mMinWidth=this->mMaxWidth=0;
    this->mCurLineWidth=0;
    this->mSearchLine=new LineElement;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071b440
// (public: __thiscall std::vector<struct WONAPI::FloatingSegment,class std::allocator<struct WONAPI::FloatingSegment> >::~vector<struct WONAPI::FloatingS)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070c930
// (public: virtual __thiscall WONAPI::MultiLineElement::~MultiLineElement(void))
// TEMPLATE: WON 0x0070c910
// (public: virtual void * __thiscall WONAPI::MultiLineElement::`scalar deleting destructor'(unsigned int))
