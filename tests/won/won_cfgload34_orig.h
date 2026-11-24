// ComponentConfig::ReadConfig_Unsafe as it was at ab1b33bcb (src/Empire Earth/WONAPI/WONGUI/ComponentConfigHost.cpp,
// copied with git show), included in won_cfgload34_test.cpp's namespace Orig over its world.
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
