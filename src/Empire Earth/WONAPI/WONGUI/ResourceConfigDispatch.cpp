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
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00793350
// Routes a STRING/IMAGE/SOUND/FONT/COLOR/BACKGROUND resource line to its handler, ignores CONTROLID,
// and passes other keys to the config object
bool ResourceConfig::HandleValueInstruction(ConfigParser* parser,const std::string& key,StringParser& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_RESOURCECONFIG_HANDLEVALUEINSTRUCTION_KEY,key.data(),key.size());
    if(aKeyIdx==0 || aKeyIdx==1) return this->HandleString(value);
    else if(aKeyIdx==2 || aKeyIdx==3) return this->HandleImage(value);
    else if(aKeyIdx==4 || aKeyIdx==5) return this->HandleSound(value);
    else if(aKeyIdx==6) return this->HandleFont(value);
    else if(aKeyIdx==7) return this->HandleColor(value);
    else if(aKeyIdx==8) return this->HandleBackground(value);
    else if(aKeyIdx==9) return true;
    else return ConfigObject::HandleValueInstruction(parser,key,value);
}
}
