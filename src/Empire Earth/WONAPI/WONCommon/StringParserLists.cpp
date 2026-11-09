// StringParser string-list reads (the WON SDK's static ReadList<T>)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"

using namespace WONAPI;

// FUNCTION: WON 0x006a9fb0
// FUNCTION: WON 0x006aa0e0
// Reads comma-separated strings into the list until no comma follows; false when an element fails
template<class T>
static bool ReadList(StringParser* parser,std::list<T>& values)
{
    while(true)
    {
        values.push_back(T());
        if(!parser->ReadString(values.back(),false))
        {
            values.pop_back();
            return false;
        }
        if(!parser->CheckNextChar(','))
            break;
    }
    return true;
}

// FUNCTION: WON 0x006a57e0
// Reads a comma-separated list of strings
bool StringParser::ReadValue(std::list<std::string>& values)
{
    return ReadList<std::string>(this,values);
}

// FUNCTION: WON 0x006a5800
// Reads a comma-separated list of wide strings
bool StringParser::ReadValue(std::list<std::wstring>& values)
{
    return ReadList<std::wstring>(this,values);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aa2c0
// (protected: struct std::list<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::allocator<class std::ba)
// TEMPLATE: WON 0x006aa2f0
// (protected: void __thiscall std::list<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned shor)
// TEMPLATE: WON 0x006aa440
// (protected: void __thiscall std::list<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::allocator<clas)
// TEMPLATE: WON 0x006aa590
// (void __cdecl std::_Construct(class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > *,c)
