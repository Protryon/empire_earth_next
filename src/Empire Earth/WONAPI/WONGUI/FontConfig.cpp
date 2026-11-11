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
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x0078a300
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
// FUNCTION: WON 0x0078a3e0
// Sets the font directly, taking its descriptor, so no lookup is needed
void FontConfig::SetFont(Font* font)
{
    this->mFont=font;
    if(font!=NULL) this->mDescriptor=font->mDescriptor;
    this->mNeedRecalcFont=false;
}
// FUNCTION: WON 0x0078a580
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
// FUNCTION: WON 0x0078a5f0
// Handles the font config instructions SRC, SIZE, STYLE (Bold/Italic/Plain/Underline flags added or
// removed; throws on an unknown one), SETDEFAULTFONT and SETNAMEDFONT; false for others
bool FontConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_FONTCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->SetFont(this->ReadFont().get());
    else if(aKeyIdx==1) {
        this->mNeedRecalcFont=true;
        this->mDescriptor.mSize=this->ReadInt();
    } else if(aKeyIdx==2) {
        while(true) {
            this->mNeedRecalcFont=true;
            bool add=true;
            GUIString flag=this->ReadFlag(&add);
            if(flag.length()==0) break;
            int style=0;
            const int aNameIdx_flag=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_FONTCONFIG_VIRTUAL24_FLAG,flag.mData->mText,flag.mData->mLength,sizeof(wchar_t));
            if(aNameIdx_flag==0) style=FontStyle_Bold;
            else if(aNameIdx_flag==1) style=FontStyle_Italic;
            else if(aNameIdx_flag==2) style=FontStyle_Plain;
            else if(aNameIdx_flag==3) style=FontStyle_Underline;
            else throw ConfigObjectException("Unknown font style: "+(std::string)flag);
            if(add) this->mDescriptor.mStyle|=style;
            else this->mDescriptor.mStyle&=~style;
        }
    } else if(aKeyIdx==3) WONAPI::WindowManager::GetDefaultWindowManager()->SetDefaultFont(this->ReadFont().get());
    else if(aKeyIdx==4) {
        GUIString name=this->ReadString(false);
        this->EnsureComma();
        SmartPtr<Font> font=this->ReadFont();
        WONAPI::WindowManager::GetDefaultWindowManager()->SetNamedFont(name,font.get());
    } else return false;
    return true;
}
// Always succeeds (the linker folds this body into the identical one at 0x007099f0)
bool FontConfig::PostParse(ConfigParser*) {return true;}
}


namespace WONAPI {
// FUNCTION: WON 0x0078a080
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
// FUNCTION: WON 0x0078a160
// Creates a new font config
ComponentConfig* FontConfig::CfgFactory()
{
    return new FontConfig;
}
// FUNCTION: WON 0x0078a1c0
// Creates a font config that needs its font looked up
FontConfig::FontConfig()
{
    this->mHaveInitedComponents=true;
    this->mNeedRecalcFont=true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078a080
// (void __cdecl WONAPI::FontConfig_Init(class WONAPI::ResourceConfigTable *)    [?FontConfig_Init@WONAPI@@YAXPAVResourceConfigTable@1@@Z])
// TEMPLATE: WON 0x0078a250
// (public: virtual __thiscall WONAPI::FontConfig::~FontConfig(void)    [??1FontConfig@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078a230
// (public: virtual void * __thiscall WONAPI::FontConfig::`scalar deleting destructor'(unsigned int)    [??_GFontConfig@WONAPI@@UAEPAXI@Z])
