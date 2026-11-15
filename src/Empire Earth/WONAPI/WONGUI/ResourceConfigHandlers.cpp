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
    std::pair<typename Map::iterator,bool> inserted=values.insert(typename Map::value_type(key,typename Map::mapped_type(Type(),generation)));
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
// Defines a named color resource from the config line
bool ResourceConfig::HandleColor(StringParser& value)
{
    DWORD* output;
    if(ResourceConfig_AddToMap("Color",value,this->mColorMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadColor(value);
    }
    return true;
}

// Defines a named font resource from the config line
bool ResourceConfig::HandleFont(StringParser& value)
{
    SmartPtr<Font>* output;
    if(ResourceConfig_AddToMap("Font",value,this->mFontMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadFont(value);
    }
    return true;
}

// Defines a named sound resource from the config line
bool ResourceConfig::HandleSound(StringParser& value)
{
    SmartPtr<Sound>* output;
    if(ResourceConfig_AddToMap("Sound",value,this->mSoundMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadSound(value);
    }
    return true;
}

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

// Defines a named image resource from the config line
bool ResourceConfig::HandleImage(StringParser& value)
{
    SmartPtr<Image>* output;
    if(ResourceConfig_AddToMap("Image",value,this->mImageMap,this->mParseCount,output)) {
        *output=ResourceConfig::SafeReadImage(value);
    }
    return true;
}

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
