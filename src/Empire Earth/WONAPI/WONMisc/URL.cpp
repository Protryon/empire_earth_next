// URL: parsing of http/ftp/file URLs and relative URL resolution
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include "platform/PlatString.h"
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONURL.h"
#include "ee/wonapi/WONHttpMsg.h"
namespace WONAPI {
// The results of ee_won's URL functions (http.rs) into a URL's members
namespace {
struct UrlOut {
    URLType* mType;
    bool* mIsAbsolute;
    std::string* mAddress;
    std::string* mPath;
};
void UrlSink(void* inContext, int32_t inKind, bool inAbsolute, const uint8_t* inAddress, size_t inAddressLen,
             const uint8_t* inPath, size_t inPathLen)
{
    UrlOut* o = static_cast<UrlOut*>(inContext);
    *o->mType = static_cast<URLType>(inKind);
    *o->mIsAbsolute = inAbsolute;
    o->mAddress->assign(reinterpret_cast<const char*>(inAddress), inAddressLen);
    o->mPath->assign(reinterpret_cast<const char*>(inPath), inPathLen);
}
EeWonUrl UrlIn(URLType inType, bool inAbsolute, const std::string& inAddress, const std::string& inPath)
{
    EeWonUrl u = {static_cast<int32_t>(inType), inAbsolute, reinterpret_cast<const uint8_t*>(inAddress.data()),
                  inAddress.size(), reinterpret_cast<const uint8_t*>(inPath.data()), inPath.size()};
    return u;
}
}
// FUNCTION: WON 0x006d7a20
// Parses an http, ftp or file URL into type, address and path; anything without a scheme is a local file path
URL::URL(const std::string& inURL)
{
    UrlOut o = {&this->mType, &this->mIsAbsolute, &this->mAddress, &this->mPath};
    ee_won_url_parse(reinterpret_cast<const uint8_t*>(inURL.data()), inURL.size(), UrlSink, &o);
}
// FUNCTION: WON 0x006d7d10
// Returns the path as a local file path (backslashes)
std::string URL::GetPathAsLocalFilePath() const
{
    WONFile file(this->mPath);
    return file.GetFilePath();
}
// FUNCTION: WON 0x006d7e90
// Resolves a relative URL against a base URL (fragments append to the base path, others replace its file part)
void URL::CalcRelativeTo(const URL& inBase)
{
    EeWonUrl self = UrlIn(this->mType, this->mIsAbsolute, this->mAddress, this->mPath);
    EeWonUrl base = UrlIn(inBase.mType, inBase.mIsAbsolute, inBase.mAddress, inBase.mPath);
    std::string anAddress, aPath;
    UrlOut o = {&this->mType, &this->mIsAbsolute, &anAddress, &aPath};
    ee_won_url_relative(&self, &base, UrlSink, &o);
    this->mAddress = anAddress;
    this->mPath = aPath;
}
// FUNCTION: WON 0x006d8150
// Builds the full URL text from scheme, address and path
void URL::GetEntire(std::string& outString)
{
    EeWonUrl self = UrlIn(this->mType, this->mIsAbsolute, this->mAddress, this->mPath);
    ee_won_url_entire(&self, HttpStringSink, &outString);
}
}
