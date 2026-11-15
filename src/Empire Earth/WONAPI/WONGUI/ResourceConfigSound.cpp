// ResourceConfig resource insertion: stores a named sound with the current parse generation (the SDK's other Add* forms had no caller)
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
    values[key]=typename Map::mapped_type(value,generation);
}
// FUNCTION: WON 0x007934a0
// Adds or replaces a named sound resource
void ResourceConfig::AddSound(const std::string& key,Sound* sound)
{
    ResourceConfig_AddToMap(this->mSoundMap,key,sound,this->mParseCount);
}
