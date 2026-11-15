
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
// FUNCTION: WON 0x00790740
// Creates an empty resource config (no overwriting, parse count 0)
ResourceConfig::ResourceConfig()
{
    this->mAllowOverwrite=false;
    this->mParseCount=0;
}
// FUNCTION: WON 0x00790d10
// Destroys the resource config and all its resource maps
ResourceConfig::~ResourceConfig()
{
}
// FUNCTION: WON 0x007938f0
// Creates an empty table of resource configs
ResourceConfigTable::ResourceConfigTable()
{
}
// FUNCTION: WON 0x00793aa0
// Destroys the table and the configs it holds
ResourceConfigTable::~ResourceConfigTable()
{
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007939c0
// (protected: virtual void * __thiscall WONAPI::ResourceConfigTable::`scalar deleting destructor'(unsigned int)    [??_GResourceConfigTable@WONAPI@@MAEPA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079d990
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0079ef40
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x007a1d00
// (public: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std:)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00790870
// (protected: virtual void * __thiscall WONAPI::ResourceConfig::`scalar deleting destructor'(unsigned int)    [??_GResourceConfig@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00790b90
// (public: __thiscall WONAPI::ResourceConfig::ResMap<class WONAPI::Background>::~ResMap<class WONAPI::Background>(void)    [??1?$ResMap@VBackground@WONAP)
// TEMPLATE: WON 0x0079bd70
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079ef00
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007939e0
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class WONAPI::SmartPtr<class WONAP)
// TEMPLATE: WON 0x0079c5d0
// (public: class std::_Tree<void (__cdecl*)(class WONAPI::ResourceConfigTable *),void (__cdecl*)(class WONAPI::ResourceConfigTable *),struct std::_Setk<v)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079bab0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079d950
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0079def0
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0079e450
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x0079f9c0
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00790890
// (public: __thiscall WONAPI::ResourceConfig::ResMap<class WONAPI::GUIString>::~ResMap<class WONAPI::GUIString>(void)    [??1?$ResMap@VGUIString@WONAPI@@)
// TEMPLATE: WON 0x00790950
// (public: __thiscall WONAPI::ResourceConfig::ResMap<class WONAPI::SmartPtr<class WONAPI::Image> >::~ResMap<class WONAPI::SmartPtr<class WONAPI::Image> >)
// TEMPLATE: WON 0x00790a10
// (public: __thiscall WONAPI::ResourceConfig::ResMap<class WONAPI::SmartPtr<class WONAPI::Font> >::~ResMap<class WONAPI::SmartPtr<class WONAPI::Font> >(v)
// TEMPLATE: WON 0x0079bb60
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079bc10
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079bf20
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079bcc0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079d9d0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079df30
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079f4a0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x007a32c0
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct WONAPI::Conf)
// TEMPLATE: WON 0x007a3300
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<cl)
// TEMPLATE: WON 0x007a3320
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<cl)
// TEMPLATE: WON 0x007a3340
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<cl)
// TEMPLATE: WON 0x007a3360
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<cl)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079d440
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079e9b0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0079be20
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x0079e490
// TEMPLATE: WON 0x0079e970
// TEMPLATE: WON 0x00790ad0
// TEMPLATE: WON 0x0079ef80
// TEMPLATE: WON 0x0079f460
// TEMPLATE: WON 0x00790c50
// TEMPLATE: WON 0x0079bed0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0074ef20
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x00754fc0
// (public: void * __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<in)
