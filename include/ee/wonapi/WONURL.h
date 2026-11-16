// URL (0x28 bytes, no vtable): two strings and an explicit URL type. Parsing, relative resolution and the full text
// are ee_won's (http.rs); header-only since net round 29 (URL.cpp deleted)
#ifndef EE_WONAPI_WONURL_H
#define EE_WONAPI_WONURL_H
#include <string>
#include "ee/wonapi/WONLobbyResource.h" // WONFile, rust/ee_won.h
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
    // an http, ftp or file URL into type, address and path; anything without a scheme is a local file path (2d7a20)
    URL(const std::string& inURL) { ee_won_url_parse(B(inURL), inURL.size(), Sink, this); }
    // a relative URL against a base (fragments append to the base path, others replace its file part; 2d7e90)
    void CalcRelativeTo(const URL& inBase)
    {
        EeWonUrl self = this->In(), base = inBase.In();
        URL aResult;
        ee_won_url_relative(&self, &base, Sink, &aResult);
        this->mType = aResult.mType;
        this->mIsAbsolute = aResult.mIsAbsolute;
        this->mAddress = aResult.mAddress;
        this->mPath = aResult.mPath;
    }
    URLType GetType() const { return this->mType; } //SDK inline accessors
    const std::string& GetAddress() const { return this->mAddress; }
    const std::string& GetPath() const { return this->mPath; }
    std::string GetPathAsLocalFilePath() const { return WONFile(this->mPath).GetFilePath(); } //2d7d10: backslashes
    void GetEntire(std::string& outString) //2d8150: the full text from scheme, address and path
    {
        EeWonUrl self = this->In();
        ee_won_url_entire(&self, Text, &outString);
    }
private:
    static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
    static void Text(void* inText, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inText)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
    EeWonUrl In() const
    {
        EeWonUrl u = {static_cast<int32_t>(this->mType), this->mIsAbsolute, B(this->mAddress), this->mAddress.size(),
                      B(this->mPath), this->mPath.size()};
        return u;
    }
    static void Sink(void* inURL, int32_t inKind, bool inAbsolute, const uint8_t* inAddress, size_t inAddressLen,
                     const uint8_t* inPath, size_t inPathLen)
    {
        URL* u = static_cast<URL*>(inURL);
        u->mType = static_cast<URLType>(inKind);
        u->mIsAbsolute = inAbsolute;
        u->mAddress.assign(reinterpret_cast<const char*>(inAddress), inAddressLen);
        u->mPath.assign(reinterpret_cast<const char*>(inPath), inPathLen);
    }
};
}
#endif
