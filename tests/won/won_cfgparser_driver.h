// The drivers' ConfigParser: the game's class with the methods only drivers call (won_cfgparser_methods.inl) and
// the saved parser position. Include it before any game header.
#ifndef WON_CFGPARSER_DRIVER_H
#define WON_CFGPARSER_DRIVER_H
#define EE_WON_CFGPARSER_DRIVER
#include <string>
namespace WONAPI {
struct ConfigParserState {
    long mFilePos; // 0x00
    std::string mFileNameAndPath; // 0x04
    int mLineNum; // 0x14
    std::wstring mBackStr; // 0x18 (0x28 bytes in all)
};
}
#endif
