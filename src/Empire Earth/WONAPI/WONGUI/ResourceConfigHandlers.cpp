// Resource config handlers for named colors, fonts, sounds, strings, images and backgrounds (file name inferred).
// A key defined twice in one parse throws; one kept from an earlier parse is left as it is
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
namespace WONAPI {
// Resource map insert with the per-parse duplicate check (the DWORD instance stays out of line: TEMPLATE below).
inline void RCMCost(DWORD*) {  }
template <class T> inline void RCMCost(T*) { }
template <class Type,class Map>
bool ResourceConfig_AddToMap(const char* typeName,StringParser& value,Map& values,int generation,Type*& output)
{
    RCMCost(output);
    std::string key;
    value.ReadString(key,true);
    if(key.empty()) throw ConfigObjectException("Invalid resource key.");
    std::pair<Map::iterator,bool> inserted=values.insert(Map::value_type(key,Map::referent_type(Type(),generation)));
    int& oldGeneration=inserted.first->second.second;
    if(inserted.second) {
        output=&inserted.first->second.first;
        return true;
    } else if(oldGeneration==generation) {
        char text[512];
        sprintf(text,"%s resource already exists: %s",typeName,key.c_str());
        throw ConfigObjectException(text);
    }
    return false;
}
// FUNCTION: WON 0x007923d0
// Defines a named color resource from the config line
bool ResourceConfig::HandleColor(StringParser& value)
{
    DWORD* output;
    if(ResourceConfig_AddToMap("Color",value,this->mColorMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadColor(value);
    }
    return true;
}

// FUNCTION: WON 0x00792410
// Defines a named font resource from the config line
bool ResourceConfig::HandleFont(StringParser& value)
{
    SmartPtr<Font>* output;
    if(ResourceConfig_AddToMap("Font",value,this->mFontMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadFont(value);
    }
    return true;
}

// FUNCTION: WON 0x00792820
// Defines a named sound resource from the config line
bool ResourceConfig::HandleSound(StringParser& value)
{
    SmartPtr<Sound>* output;
    if(ResourceConfig_AddToMap("Sound",value,this->mSoundMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadSound(value);
    }
    return true;
}

// FUNCTION: WON 0x00792ac0
// Defines a named text string resource from the config line
bool ResourceConfig::HandleString(StringParser& value)
{
    GUIString* output;
    if(!ResourceConfig_AddToMap("String",value,this->mStringMap,this->mParseCount,output))
        return true;
    std::wstring text;
    value.ReadString(text,false);
    *output=text;
    return true;
}

// FUNCTION: WON 0x00792d90
// Defines a named image resource from the config line
bool ResourceConfig::HandleImage(StringParser& value)
{
    SmartPtr<Image>* output;
    if(ResourceConfig_AddToMap("Image",value,this->mImageMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadImage(value);
    }
    return true;
}

// FUNCTION: WON 0x00793020
// Defines a named background resource from the config line
bool ResourceConfig::HandleBackground(StringParser& value)
{
    Background* output;
    if(ResourceConfig_AddToMap("Background",value,this->mBackgroundMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadBackground(value);
    }
    return true;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007932d0
// (public: __thiscall WONAPI::Background::Background(class WONAPI::Background const &)    [??0Background@WONAPI@@QAE@ABV01@@Z])
// TEMPLATE: WON 0x007a3200
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
// TEMPLATE: WON 0x007a4c50
// (public: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std:)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a36a0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)
// TEMPLATE: WON 0x007a4600
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a21e0
// (bool __cdecl WONAPI::ResourceConfig_AddToMap(char const *,class WONAPI::StringParser &,class WONAPI::ResourceConfig::ResMap<unsigned long> &,int,unsig)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a2f40
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
// TEMPLATE: WON 0x007a2fd0
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
// TEMPLATE: WON 0x007a3060
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
// TEMPLATE: WON 0x007a30f0
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
// TEMPLATE: WON 0x007a4300
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a33a0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)
// TEMPLATE: WON 0x007a3a00
// TEMPLATE: WON 0x007a4d10
// TEMPLATE: WON 0x007a34a0
// TEMPLATE: WON 0x007a3d00
// TEMPLATE: WON 0x007a4d90
// TEMPLATE: WON 0x007a35a0
// TEMPLATE: WON 0x007a4000
// TEMPLATE: WON 0x007a4e10
// TEMPLATE: WON 0x007a3850
// TEMPLATE: WON 0x007a4900
// TEMPLATE: WON 0x007a50c0
// TEMPLATE: WON 0x007a4e90
// TEMPLATE: WON 0x007a5000
// TEMPLATE: WON 0x007a37a0
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
// TEMPLATE: WON 0x007a3950
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct std::pair<class WON)
