// Resource config value instruction dispatch (file name inferred)
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
// FUNCTION: WON 0x00793350
// Routes a STRING/IMAGE/SOUND/FONT/COLOR/BACKGROUND resource line to its handler, ignores CONTROLID,
// and passes other keys to the config object
bool ResourceConfig::HandleValueInstruction(ConfigParser* parser,const std::string& key,StringParser& value)
{
    if(key=="STRING" || key=="CODESTRING") return this->HandleString(value);
    else if(key=="IMAGE" || key=="CODEIMAGE") return this->HandleImage(value);
    else if(key=="SOUND" || key=="CODESOUND") return this->HandleSound(value);
    else if(key=="FONT") return this->HandleFont(value);
    else if(key=="COLOR") return this->HandleColor(value);
    else if(key=="BACKGROUND") return this->HandleBackground(value);
    else if(key=="CONTROLID") return true;
    else return ConfigObject::HandleValueInstruction(parser,key,value);
}
}
