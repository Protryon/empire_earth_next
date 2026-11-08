#ifndef EE_WON_COMMON_MISC_UTIL_H
#define EE_WON_COMMON_MISC_UTIL_H
#include <windows.h>
#include <string>
namespace WONAPI {
typedef void (*BrowseCallback)(const std::string& sURL, void* pData);
void SetPreBrowseCallback(BrowseCallback pCallback, void* pData);
bool GetBrowserCommandLineFromRegistry(std::string& sBrowser);
bool Browse(const std::string& sURL);
DWORD LaunchSierraUp(const char* thePath = NULL);
}
#endif
