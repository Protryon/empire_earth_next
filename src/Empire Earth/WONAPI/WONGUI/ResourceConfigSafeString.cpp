// ResourceConfig lookups that throw when a named resource is missing
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
using namespace WONAPI;
// Returns the named entry of a resource map, or throws "<type> resource not found: <key> (ResourceGroup: <group>)"
// (this address holds the string instance; the image and sound instances follow at 0x007a27f0 and 0x007a2960)
template<class Map>
static typename Map::mapped_type::first_type& ResourceConfig_SafeGetRes(
    const char* typeName,const std::string& key,Map& values,ResourceConfig* config)
{
    typename Map::iterator found=values.find(key);
    if(found!=values.end()) return found->second.first;
    char text[512];
    // mName is the config object's name (ConfigObject::GetName); the original reads its text at +0x1c.
    sprintf(text,"%s resource not found: %s (ResourceGroup: %s)",
        typeName,key.c_str(),config->mName.c_str());
    throw ConfigObjectException(text);
}
// Returns the named string resource; throws when it is missing
const GUIString& ResourceConfig::SafeGetString(const std::string& key)
{
    return ResourceConfig_SafeGetRes("String",key,this->mStringMap,this);
}

// Returns the named image resource; throws when it is missing
Image* ResourceConfig::SafeGetImage(const std::string& key)
{
    return ResourceConfig_SafeGetRes("Image",key,this->mImageMap,this).get();
}
// Returns the named sound resource; throws when it is missing
Sound* ResourceConfig::SafeGetSound(const std::string& key)
{
    return ResourceConfig_SafeGetRes("Sound",key,this->mSoundMap,this).get();
}
