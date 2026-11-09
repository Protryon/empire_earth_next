// URL (0x28 bytes, no vtable): two strings and an explicit URL type
#ifndef EE_WONAPI_WONURL_H
#define EE_WONAPI_WONURL_H
#include <string>
namespace WONAPI {
enum URLType { URLType_None,URLType_File,URLType_HTTP,URLType_FTP };
class URL {
protected:
    URLType mType; //00
    std::string mAddress; //04
    std::string mPath; //14
    bool mIsAbsolute; //24; naturalpadding25..27
public:
    URL() : mType(URLType_None),mIsAbsolute(false) {} //SDK inline
    URL(const std::string&); //2d7a20
    void CalcRelativeTo(const URL&); //2d7e90
    URLType GetType() const { return this->mType; } //SDK inline accessors
    const std::string& GetAddress() const { return this->mAddress; }
    const std::string& GetPath() const { return this->mPath; }
    std::string GetPathAsLocalFilePath() const; //2d7d10,sret
    void GetEntire(std::string&); //2d8150
};
}
#endif
