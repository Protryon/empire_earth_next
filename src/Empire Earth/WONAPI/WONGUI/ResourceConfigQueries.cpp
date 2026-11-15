
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
// FUNCTION: WON 0x00793340
// Counts one more parse pass of the resource group (the generation stamped on resources added afterwards); always true
bool ResourceConfig::PreParse(ConfigParser*)
{
    ++this->mParseCount;
    return true;
}
// FUNCTION: WON 0x007934c0
// Looks up a named string resource; copies it to the output and returns true when found
bool ResourceConfig::GetString(const std::string& name,GUIString& output)
{
    ResourceConfig::StringMap& aMap=this->mStringMap;
    ResourceConfig::StringMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
// FUNCTION: WON 0x00793570
// Looks up a named image resource; stores the image pointer and returns true when found
bool ResourceConfig::GetImage(const std::string& name,Image*& output)
{
    ResourceConfig::ImageMap& aMap=this->mImageMap;
    ResourceConfig::ImageMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first.get();
        return true;
    }
    return false;
}
// FUNCTION: WON 0x007935d0
// Looks up a named font resource; stores the font pointer and returns true when found
bool ResourceConfig::GetFont(const std::string& name,Font*& output)
{
    ResourceConfig::FontMap& aMap=this->mFontMap;
    ResourceConfig::FontMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first.get();
        return true;
    }
    return false;
}
// FUNCTION: WON 0x00793630
// Looks up a named sound resource; stores the sound pointer and returns true when found
bool ResourceConfig::GetSound(const std::string& name,Sound*& output)
{
    ResourceConfig::SoundMap& aMap=this->mSoundMap;
    ResourceConfig::SoundMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first.get();
        return true;
    }
    return false;
}
// FUNCTION: WON 0x007936a0
// Looks up a named color resource; stores the color and returns true when found
bool ResourceConfig::GetColor(const std::string& name,DWORD& output)
{
    ResourceConfig::ColorMap& aMap=this->mColorMap;
    ResourceConfig::ColorMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
// FUNCTION: WON 0x00793710
// Looks up a named background resource; copies it to the output and returns true when found
bool ResourceConfig::GetBackground(const std::string& name,Background& output)
{
    ResourceConfig::BackgroundMap& aMap=this->mBackgroundMap;
    ResourceConfig::BackgroundMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
// FUNCTION: WON 0x00793820
// Looks up a named integer resource; stores the value and returns true when found
bool ResourceConfig::GetInt(const std::string& name,int& output)
{
    ResourceConfig::IntMap& aMap=this->mIntMap;
    ResourceConfig::IntMap::iterator i=aMap.find(name);
    if(i!=aMap.end())
    {
        output=i->second.first;
        return true;
    }
    return false;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a4c00
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
