// Constructors of text segments and table elements, and the text line destructor
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
#include "ee/wonapi/WONLobbyLineLayout.h"
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {
// TextSegmentBase() and TextLine() live in TextAreaInstructions.cpp / TextAreaLines.cpp (the original object
// defines them before TextArea's `new` sites).
// Creates an empty text segment drawn in black (emitted at 0x00710c40)
TextSegment::TextSegment()
{
    this->mColor=0;
}

// Creates an empty table with 2-pixel cell spacing, 1-pixel padding, no border and no background colour
// (emitted at 0x0070d7f0)
TableElement::TableElement()
{
    this->mCurCell=NULL;
    this->mCellSpacing=2;
    this->mCellPadding=1;
    this->mBorder=0;
    this->mBGColor=-1;
    this->mMinWidth=this->mMaxWidth=0;
    this->mWidthSpec=0;
}
}

namespace WONAPI {
// Destroys the text line: an explicit destructor (the original stores TextLine's vtable, then destroys the
// segment list at +0x20, then the RefCount base; emitted at 0x00711730)
TextLine::~TextLine() {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070d7f0
// (public: __thiscall WONAPI::TableElement::TableElement(void)    [??0TableElement@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00710c40
// (public: __thiscall WONAPI::TextSegment::TextSegment(void)    [??0TextSegment@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00710ce0
// (public: virtual __thiscall WONAPI::TextSegment::~TextSegment(void)    [??1TextSegment@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00711730
// (public: virtual __thiscall WONAPI::TextLine::~TextLine(void)    [??1TextLine@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0071b6f0
// (public: __thiscall std::vector<struct WONAPI::TableElement::ColumnInfo,class std::allocator<struct WONAPI::TableElement::ColumnInfo> >::~vector<struct)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070d8c0
// (public: virtual void * __thiscall WONAPI::TableElement::`scalar deleting destructor'(unsigned int)    [??_GTableElement@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00710cc0
// (public: virtual void * __thiscall WONAPI::TextSegment::`scalar deleting destructor'(unsigned int)    [??_GTextSegment@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070d8a0
// (public: virtual int __thiscall WONAPI::TableElement::GetWidth(void)    [?GetWidth@TableElement@WONAPI@@UAEHXZ])
// TEMPLATE: WON 0x0070d8b0
// (public: virtual int __thiscall WONAPI::TableElement::GetHeight(void)    [?GetHeight@TableElement@WONAPI@@UAEHXZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070d8e0
// (public: virtual __thiscall WONAPI::TableElement::~TableElement(void))
// TEMPLATE: WON 0x00710cb0
// (public: virtual int __thiscall WONAPI::TextSegment::GetLength(void))
// TEMPLATE: WON 0x0071b6a0
// (protected: void __thiscall std::vector<struct WONAPI::TableElement::CellInfo,class std::allocator<struct WONAPI::TableElement::CellInfo> >::_Tidy(void)
// TEMPLATE: WON 0x0071b4c0
// (public: __thiscall std::vector<struct WONAPI::TableElement::RowInfo,class std::allocator<struct WONAPI::TableElement::RowInfo> >::~vector<struct WONAP)
