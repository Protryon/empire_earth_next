// Browser launch helper: opens a URL in the user's browser
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <string>
#include "ee/wonapi/WONCommonMiscUtil.h"
#include "platform/PlatWindow.h"
namespace WONAPI {

BrowseCallback gPreBrowseCallback = NULL;
void* gPreBrowseData = NULL;
void SetPreBrowseCallback(BrowseCallback pCallback, void* pData)
{
    gPreBrowseCallback = pCallback;
    gPreBrowseData = pData;
}
// The original looked up the .html open command under HKEY_CLASSES_ROOT (GetBrowserCommandLineFromRegistry,
// 0x006a32b0) and started it with CreateProcessA; the platform opens the URL with the system's handler. The WON
// SDK's LaunchSierraUp (SierraUp.exe, no caller) is gone.
// FUNCTION: WON 0x006a30e0
// Opens the URL in the default browser after the pre-browse callback; returns whether it was opened
bool Browse(const std::string& url)
{
    if(gPreBrowseCallback!=NULL) gPreBrowseCallback(url,gPreBrowseData);
    return PlatOpenURL(url.c_str())!=0;
}
}
