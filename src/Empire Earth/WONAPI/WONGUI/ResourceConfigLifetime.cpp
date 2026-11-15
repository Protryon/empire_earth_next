
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
// Creates an empty resource config (no overwriting, parse count 0)
ResourceConfig::ResourceConfig()
{
    this->mAllowOverwrite=false;
    this->mParseCount=0;
}
// Destroys the resource config and all its resource maps
ResourceConfig::~ResourceConfig()
{
}
// Creates an empty table of resource configs
ResourceConfigTable::ResourceConfigTable()
{
}
// Destroys the table and the configs it holds
ResourceConfigTable::~ResourceConfigTable()
{
}
}
