// ResourceConfig resource insertion: stores named resources with the current parse generation
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
using namespace WONAPI;
// FUNCTION: WON 0x007a2530
// Stores a value under a key in a resource map, tagged with the parse generation (replaces an existing entry)
template<class Type,class Map>
static void ResourceConfig_AddToMap(Map& values,const std::string& key,Type& value,int generation)
{
    values[key]=Map::referent_type(value,generation);
}
// FUNCTION: WON 0x007934a0
// Adds or replaces a named sound resource
void ResourceConfig::AddSound(const std::string& key,Sound* sound)
{
    ResourceConfig_AddToMap(this->mSoundMap,key,sound,this->mParseCount);
}

// String, image, font, color, background and int insertion (not identified in the original exe)
void ResourceConfig::AddString(const std::string &theKey, const GUIString &theStr)
{
	ResourceConfig_AddToMap(this->mStringMap,theKey,theStr,this->mParseCount);
}

void ResourceConfig::AddImage(const std::string &theKey, Image *theImage)
{
	ResourceConfig_AddToMap(this->mImageMap,theKey,theImage,this->mParseCount);
}

void ResourceConfig::AddFont(const std::string &theKey, Font *theFont)
{
	ResourceConfig_AddToMap(this->mFontMap,theKey,theFont,this->mParseCount);
}

void ResourceConfig::AddColor(const std::string &theKey, DWORD theColor)
{
	ResourceConfig_AddToMap(this->mColorMap,theKey,theColor,this->mParseCount);
}

void ResourceConfig::AddBackground(const std::string &theKey, const Background &theBackground)
{
	ResourceConfig_AddToMap(this->mBackgroundMap,theKey,theBackground,this->mParseCount);
}

void ResourceConfig::AddInt(const std::string &theKey, int theInt)
{
	ResourceConfig_AddToMap(this->mIntMap,theKey,theInt,this->mParseCount);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a39c0
// (public: __thiscall std::pair<class WONAPI::GUIString,int>::pair<class WONAPI::GUIString,int>(struct std::pair<class WONAPI::GUIString,int> const &)   )
