// Named component lookup and the missing-component error used by SafeGetComponent
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x0078ab30
// Throws a configuration error naming the missing component and the source file and line that wanted it
void ComponentConfigMissing(const std::string& name,const char* file,int line)
{
    char message[512];
    sprintf(message,"InitComponent error: %s (%s:%d)",name.c_str(),file,line);
    throw ConfigObjectException(message);
}

// FUNCTION: WON 0x0078aa60
// Returns the component configured under the name, or NULL when there is none
Component* ComponentConfigGet(ComponentConfig* config,const char* name)
{
    return config->Fn386860(std::string(name));
}

}
