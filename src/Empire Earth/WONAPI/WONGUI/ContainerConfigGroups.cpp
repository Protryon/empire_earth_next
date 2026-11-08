// Container config: PERCENTLAYOUT and COLORSCHEME groups (file name inferred)
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
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLayoutConfigs.h"
#include "ee/wonapi/WONPercentLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x007885d0
// Reads a PERCENTLAYOUT group into a percent layout (relative to the component if no reference is given)
// or a COLORSCHEME group into the container's colour scheme; other instructions go to the base config
bool ContainerConfig::HandleInstruction(ConfigParser* parser,const std::string& type)
{
    if(type=="PERCENTLAYOUT") {
        SmartPtr<PercentLayoutConfig> config=new PercentLayoutConfig;
        if(this->ParseChild(*parser,config.get(),false,true)) {
            int i;
            for(i=0;i<4;++i) if(config->refs[i]!=NULL) break;
            if(i==4) config->refs[0]=this->mComponent.get();
            PercentLayoutParam param(config->refs[0],config->refs[1],config->refs[2],config->refs[3],
                config->params[0],config->params[1],config->params[2],config->params[3],
                config->params[4],config->params[5],config->params[6],config->params[7]);
            this->mContainer->AddChildLayout(new PercentLayout(config->target,param),this->mCurLayoutId);
        }
        return true;
    }
    if(type=="COLORSCHEME") {
        SmartPtr<ColorSchemeConfig> config=new ColorSchemeConfig;
        if(this->ParseChild(*parser,config.get(),false,true))
            this->mContainer->SetColorScheme(config->mColorScheme.get());
        return true;
    }
    return ComponentConfig::HandleInstruction(parser,type);
}
}
