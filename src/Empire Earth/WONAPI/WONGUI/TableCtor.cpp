// Table construction (file name inferred)
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
namespace WONAPI {
// Creates a table showing the given contents, with no border padding (the constructor at 0x0070f9d0)
Table::Table(LineSegment* contents)
{
    this->mContents=contents;
    this->SetBorderPadding(0,0,0,0);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070f9d0
// (public: __thiscall WONAPI::Table::Table(class WONAPI::LineSegment *)    [??0Table@WONAPI@@QAE@PAVLineSegment@1@@Z])
// TEMPLATE: WON 0x0070fbb0
// (public: virtual __thiscall WONAPI::Table::~Table(void)    [??1Table@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070fad0
// (public: virtual void * __thiscall WONAPI::Table::`scalar deleting destructor'(unsigned int)    [??_GTable@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ced0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,struct std::_)
// TEMPLATE: WON 0x00720be0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::Table::AnchorCoordinate>,)
// TEMPLATE: WON 0x0070faf0
// (public: __thiscall std::map<class WONAPI::GUIString,struct WONAPI::Table::AnchorCoordinate,struct WONAPI::GUIStringLessNoCase,class std::allocator<str)
