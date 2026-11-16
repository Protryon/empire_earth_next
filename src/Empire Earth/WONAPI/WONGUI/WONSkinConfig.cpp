// Skin-file instructions that replace the global skin's template controls and set image cursors (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyWONButtonConfig.h"
#include "ee/wonapi/WONLobbyWONSkinConfig.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// Runs the base pre-parse and creates the global skin as the Windows-look skin when there is none
bool WONSkinConfig::PreParse(ConfigParser* parser)
{
    if(!ComponentConfig::PreParse(parser))
        return false;
    if(gSkin.get()==NULL)
        gSkin=new WONMSSkin;
    return true;
}
// Reads a sound and sets it as the click sound of the skin's template buttons and every named button type
void WONSkinConfig::HandleButtonClickSound()
{
    SmartPtr<Sound> sound=this->ReadSound();
    gSkin->mPushButton->SetClickSound(sound.get());
    gSkin->mCheckbox->SetClickSound(sound.get());
    gSkin->mRadioButton->SetClickSound(sound.get());
    gSkin->mHeaderButton->SetClickSound(sound.get());
    gSkin->mTabButton->SetClickSound(sound.get());
    gSkin->mCloseButton->SetClickSound(sound.get());
    gSkin->mComboButton->SetClickSound(sound.get());
    WONSkin::ButtonTypeMap::iterator it=gSkin->mButtonTypeMap.begin();
    for(;it!=gSkin->mButtonTypeMap.end();++it)
        it->second->SetClickSound(sound.get());
}


// Compiler-generated and header-inline functions this unit emits:
}
