// Resource group creation in the resource config table
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
// Returns the named resource group, creating it when absent; reports through alreadyExists whether it existed
ResourceConfig* ResourceConfigTable::AddConfig(const std::string& name,bool* alreadyExists)
{
    ResourceMap::iterator it=this->mResourceMap.insert(ResourceMap::value_type(name,NULL)).first;
    if(it->second.get()==NULL) {
        it->second=new ResourceConfig;
        it->second->mName=name;
        if(alreadyExists!=NULL) *alreadyExists=false;
    } else {
        if(alreadyExists!=NULL) *alreadyExists=true;
    }
    return it->second.get();
}
}
