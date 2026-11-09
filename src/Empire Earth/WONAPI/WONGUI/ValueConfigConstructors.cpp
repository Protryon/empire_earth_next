// Constructors of the percent layout and color scheme configurations (file name inferred)
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
#include "ee/wonapi/WONLayoutConfigs.h"
// FUNCTION: WON 0x00787fb0
// Creates a percent layout configuration with no component references and every parameter unset (-1)
PercentLayoutConfig::PercentLayoutConfig()
{
    int i;
    for(i=0;i<4;++i) this->refs[i]=NULL;
    for(i=0;i<8;++i) this->params[i]=-1;
}
// FUNCTION: WON 0x00788260
// Creates a color scheme configuration owning a new color scheme
ColorSchemeConfig::ColorSchemeConfig()
{
    this->mColorScheme=new WONAPI::ColorScheme;
}
// FontConfig::FontConfig (0x0078a1c0) is not written yet: it needs the FontDescriptor default
// constructor that zeroes mStyle/mSize, which the shared declaration lacks


// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00788340
// (public: virtual __thiscall ColorSchemeConfig::~ColorSchemeConfig(void)    [??1ColorSchemeConfig@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00788320
// (public: virtual void * __thiscall ColorSchemeConfig::`scalar deleting destructor'(unsigned int))
