#ifndef EE_WON_COMMON_MISC_UTIL_H
#define EE_WON_COMMON_MISC_UTIL_H
#include <string>
namespace WONAPI {
typedef void (*BrowseCallback)(const std::string& sURL, void* pData);
void SetPreBrowseCallback(BrowseCallback pCallback, void* pData);
bool Browse(const std::string& sURL);
}
#endif
