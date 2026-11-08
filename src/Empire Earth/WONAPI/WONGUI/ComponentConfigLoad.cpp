// Configuration file loading and recursive component initialisation
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
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
#include "ee/wonapi/WONPerfTimer.h"
#include "ee/wonapi/WONMSMessageBoxAPI.h"

// Parse statistics shared with the configuration instruction code (defined there)
extern bool gShowParseTime;
extern int gLinesParsed;

namespace WONAPI {
// FUNCTION: WON 0x007828c0
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
    activeParser.mWarningsAsErrors=true;
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
        sprintf(text,"Lines parsed: %d\nConfig Parse Time: %.2f (%.2f) (= %%%d)",
                     gLinesParsed,PerfTimer::GetElapsedTime(),PerfTimer::GetCountedTime(),
                     (int)(PerfTimer::GetPercentTime()*100));
        MSMessageBox::Msg(Window::GetDefaultWindow(),GUIString("Config Parse Time"),
                          GUIString(text),gEmptyGUIString,gEmptyGUIString,gEmptyGUIString);
    }
    gShowParseTime=oldShowParseTime;
    WONAPI::WindowManager::GetDefaultWindowManager()->SetCacheImages(false);
    ComponentConfig::mResourceConfigTable=oldTable;
    return true;
}

// FUNCTION: WON 0x00782cc0
// Initialises the component from this configuration once, then every child configuration in name order
void ComponentConfig::InitComponents()
{
    if(this->mHaveInitedComponents) return;
    this->mHaveInitedComponents=true;
    this->mComponent->InitComponent(ComponentConfigInit(this));
    std::map<std::string,SmartPtr<ComponentConfig>,StringLessNoCase>::iterator it=this->mConfigMap.begin();
    while(it!=this->mConfigMap.end()) {
        it->second->InitComponents();
        ++it;
    }
}
}

#include <stdlib.h>
namespace WONAPI {
// Reads the configuration file; on failure shows the error in a message box, closes every window and
// exits the program (not identified in the original exe)
void ComponentConfig::ReadConfig(Component* theComponent,const char* theFileName,ResourceConfigTable* theTable,ConfigParser* theParser)
{
    if(this->ReadConfig_Unsafe(theComponent,theFileName,theTable,theParser))
        return;
    // Inline InitSystemColorScheme: makes sure the system colour scheme exists
    WindowManager::GetDefaultWindowManager()->GetSystemColorScheme();
    MSMessageBox::Msg(Window::GetDefaultWindow(),GUIString("Config Error"),this->mConfigError,
                      GUIString("Ok"),gEmptyGUIString,gEmptyGUIString);
    WindowManager::GetDefaultWindowManager()->CloseAllWindows();
    exit(0);
}
}

namespace WONAPI {
// Creates the init record (type 2) that hands the configuration to InitComponent (emitted at 0x0078aa40)
ComponentConfigInit::ComponentConfigInit(ComponentConfig* theConfig)
{
    this->mType = 2;
    this->mConfig = theConfig;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078aa40
// (public: __thiscall WONAPI::ComponentConfigInit::ComponentConfigInit(class WONAPI::ComponentConfig *)    [??0ComponentConfigInit@WONAPI@@QAE@PAVCompone)
