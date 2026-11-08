// Browser launch helpers: find the default browser in the registry and start it
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <string.h>
#include "ee/wonapi/WONCommonMiscUtil.h"
namespace WONAPI {

BrowseCallback gPreBrowseCallback = NULL;
void* gPreBrowseData = NULL;
void SetPreBrowseCallback(BrowseCallback pCallback, void* pData)
{
    gPreBrowseCallback = pCallback;
    gPreBrowseData = pData;
}
// FUNCTION: WON 0x006a30e0
// Opens the URL in the default browser after the pre-browse callback; returns whether the process started
bool Browse(const std::string& url)
{
    std::string browser;
    std::string command=" ";
    if(gPreBrowseCallback!=NULL) gPreBrowseCallback(url,gPreBrowseData);
    GetBrowserCommandLineFromRegistry(browser);
    if(browser.empty()) return false;
    command+=url;
    STARTUPINFOA startup;
    memset(&startup,0,sizeof(startup));
    startup.cb=sizeof(startup);
    PROCESS_INFORMATION process;
    BOOL result=CreateProcessA(browser.c_str(),const_cast<char*>(command.c_str()),
        NULL,NULL,FALSE,0,NULL,NULL,&startup,&process);
    if(result==TRUE) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    return result==TRUE;
}
// FUNCTION: WON 0x006a32b0
// Reads the .html open command from the registry and extracts the browser's executable path
bool GetBrowserCommandLineFromRegistry(std::string& browser)
{
    char value[260];
    char shortPath[260];
    HKEY key;
    DWORD length=260;
    browser="";
    if(RegOpenKeyExA(HKEY_CLASSES_ROOT,".html",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)
        return false;
    LONG status=RegQueryValueExA(key,"",NULL,NULL,reinterpret_cast<BYTE*>(value),&length);
    RegCloseKey(key);
    if(status!=ERROR_SUCCESS) return false;
    std::string command=value;
    command+="\\shell\\open\\command";
    if(RegOpenKeyExA(HKEY_CLASSES_ROOT,command.c_str(),0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)
        return false;
    length=260;
    status=RegQueryValueExA(key,"",NULL,NULL,reinterpret_cast<BYTE*>(value),&length);
    RegCloseKey(key);
    if(status!=ERROR_SUCCESS) return false;
    command=value;
    int quote=command.find('"');
    if(quote==0) {
        command.erase(0,1);
        quote=command.find('"',1);
        if(quote!=-1) command.erase(quote,command.length()-quote);
        DWORD shortLength=GetShortPathNameA(command.c_str(),shortPath,260);
        if(shortLength==0 || shortLength>=260) return false;
        browser=shortPath;
    } else {
        if(quote!=-1) command.erase(quote,command.length()-quote);
        int argument=command.find("%1");
        if(argument!=-1) command.erase(argument,command.length()-argument);
        browser=command;
        std::string::size_type last=browser.length();
        while(last>0) {
            if(browser[last-1]!=' ' && browser[last-1]!='\t' && browser[last-1]!='\n')
                break;
            browser.erase(last-1);
            last=browser.length();
        }
    }
    return browser.length()>0;
}
}

// No original address identified: WON SDK SierraUp launcher
namespace WONAPI {
DWORD LaunchSierraUp(const char* thePath)
{
    char sPath[MAX_PATH];
    GetModuleFileNameA(NULL, sPath, sizeof(sPath));

    PROCESS_INFORMATION aProcessInfo;
    STARTUPINFOA aStartInfo;
    memset(&aStartInfo, 0, sizeof(aStartInfo));
    aStartInfo.cb = sizeof(aStartInfo);

    char* pBS = strrchr(sPath, '\\');
    if(pBS)
        pBS[1] = 0;

    std::string aCommandLine;
    if(thePath == NULL)
        aCommandLine = sPath;
    else
        aCommandLine = thePath;

    aCommandLine += std::string("SierraUp.exe");
    if(!::CreateProcessA(NULL, (char*)aCommandLine.c_str(), NULL, NULL,
        FALSE, 0, NULL, NULL, &aStartInfo, &aProcessInfo))
        return GetLastError();

    return 0;
}
}
