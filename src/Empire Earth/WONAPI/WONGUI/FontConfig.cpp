// Font configuration: font selection, style flags and default/named font registration
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// Returns the configured font, first fetching it from the default window when the descriptor
// changed and no matching font is held
Font* FontConfig::GetFont()
{
    if(this->mNeedRecalcFont) {
        if(this->mFont.get()==NULL || this->mFont->mDescriptor!=this->mDescriptor)
            this->mFont=Window::GetDefaultWindow()->GetFont(this->mDescriptor).get();
    }
    return this->mFont.get();
}
// Sets the font directly, taking its descriptor, so no lookup is needed
void FontConfig::SetFont(Font* font)
{
    this->mFont=font;
    if(font!=NULL) this->mDescriptor=font->mDescriptor;
    this->mNeedRecalcFont=false;
}
// Returns the font's SIZE or HEIGHT as a named value; false without a font or for other names
bool FontConfig::GetNamedValue(const std::string& name,int& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_FONTCONFIG_GETNAMEDVALUE_NAME,name.data(),name.size());
    if(this->mFont.get()==NULL) return false;
    if(aKeyIdx==0) value=this->mDescriptor.mSize;
    else if(aKeyIdx==1) value=this->mFont->GetHeight();
    else return false;
    return true;
}
// Always succeeds (the linker folds this body into the identical one at 0x007099f0)
bool FontConfig::PostParse(ConfigParser*) {return true;}
}


namespace WONAPI {
// Registers the "Font" config type with the resource config table
void FontConfig_Init(ResourceConfigTable* table)
{
    table->AddAllocator("Font",FontConfig::CfgFactory,FontConfig::CompFactory);
}
}

namespace WONAPI {

// Font configs create no component (the linker folds this body into the identical one at 0x006c7b10)
Component* FontConfig::CompFactory()
{
    return NULL;
}

}

namespace WONAPI {
// Creates a new font config
ComponentConfig* FontConfig::CfgFactory()
{
    return new FontConfig;
}
// Creates a font config that needs its font looked up
FontConfig::FontConfig()
{
    this->mHaveInitedComponents=true;
    this->mNeedRecalcFont=true;
}
}
