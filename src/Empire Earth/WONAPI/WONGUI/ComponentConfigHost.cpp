// The component configs (R6 WONLobby round 25): ComponentConfig's readers, look-ups, config map and instructions,
// ResourceConfig and ResourceConfigTable are ee_won_gui's compcfg.rs objects; their forwards and host are
// ee/wonapi/WONCompCfg.inl. This unit compiles them and keeps the load of a config file (ReadConfig_Unsafe), the
// statics and the missing-component report.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <exception>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONGUIShell.h"
bool gShowParseTime = false; // (SHOWPARSETIME sets it during a parse: WONCfgTable.inl)
int gLinesParsed = 0; // (ConfigTableHost.cpp counts the lines)
namespace WONAPI {
#include "ee/wonapi/WONCompCfg.inl"

SmartPtr<ResourceConfigTable> ComponentConfig::mResourceConfigTable;

// Parses the configuration file, glues its resources and initialises every configured component; on failure
// returns false with the error text, leaving the resource table, parse-time flag and image caching as set
bool ComponentConfig::ReadConfig_Unsafe(Component* component,const char* fileName,
                                      ResourceConfigTable* table,ConfigParser* parser)
{
    this->SetComponent(component);
    WONAPI::WindowManager::GetDefaultWindowManager()->SetCacheImages(true);
    SmartPtr<ResourceConfigTable> oldTable=ComponentConfig::mResourceConfigTable;
    if(table!=NULL) ComponentConfig::mResourceConfigTable=table;
    else ComponentConfig::mResourceConfigTable=new ResourceConfigTable;
    bool oldShowParseTime=gShowParseTime;
    gShowParseTime=false;
    std::string path=ComponentConfig::GetResourceFile(fileName);
    ConfigParser localParser;
    ConfigParser& activeParser=parser?*parser:localParser;
    activeParser.SetWarningsAsErrors(true);
    if(!this->ParseTopLevel(activeParser,path.c_str())) {
        activeParser.CloseFile();
        this->mConfigError=activeParser.GetAbortReason();
        return false;
    }
    try {
        ComponentConfig::mResourceConfigTable->SafeGlueResources();
        this->InitComponents();
    } catch(ConfigObjectException& error) {
        this->mConfigError=GUIString(error.mText);
        return false;
    }
    if(gShowParseTime) {
        char text[512];
        snprintf(text,sizeof text,"Lines parsed: %d\nConfig Parse Time: %.2f (%.2f) (= %%%d)",
                 gLinesParsed,PerfTimer::GetElapsedTime(),PerfTimer::GetCountedTime(),
                 (int)(PerfTimer::GetPercentTime()*100));
        MSMessageBoxMsg(Window::GetDefaultWindow(),GUIString("Config Parse Time"),
                        GUIString(text),gEmptyGUIString,gEmptyGUIString,gEmptyGUIString);
    }
    gShowParseTime=oldShowParseTime;
    WONAPI::WindowManager::GetDefaultWindowManager()->SetCacheImages(false);
    ComponentConfig::mResourceConfigTable=oldTable;
    return true;
}

// The init record (type 2) that hands the configuration to InitComponent
ComponentConfigInit::ComponentConfigInit(ComponentConfig* theConfig)
{
    this->mType = 2;
    this->mConfig = theConfig;
}

// A container configuration with no layout reference and layout id 0
ContainerConfig::ContainerConfig()
{
    this->mLayoutRef=NULL;
    this->mCurLayoutId=0;
}

// Throws a configuration error naming the missing component and the source file and line that wanted it
void ComponentConfigMissing(const std::string& name,const char* file,int line)
{
    char message[512];
    snprintf(message,sizeof message,"InitComponent error: %s (%s:%d)",name.c_str(),file,line);
    throw ConfigObjectException(message);
}

// The component configured under the name, or NULL when there is none
Component* ComponentConfigGet(ComponentConfig* config,const char* name)
{
    return config->Fn386860(std::string(name));
}
}
