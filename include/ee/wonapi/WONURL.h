// URL (0x28 bytes in the original, no vtable: two strings and an explicit URL type) is ee_won's (http.rs EeWonUrlObj,
// R6 WON round 33: the parse, the relative resolution, the full text and the members); this class owns one and every
// method forwards. URL.cpp was deleted in net round 29
#ifndef EE_WONAPI_WONURL_H
#define EE_WONAPI_WONURL_H
#include "ee/wonapi/WONDriver.h" // EE_WON_DRV
#include <string>
#include "ee/wonapi/WONLobbyResource.h" // WONFile, rust/ee_won.h
#include "rust/won_net/http.h"
namespace WONAPI {
enum URLType { URLType_None,URLType_File,URLType_HTTP,URLType_FTP };
class URL {
public:
    URL() : mUrl(ee_won_url_obj_new(NULL, 0)) {} //SDK inline: URLType_None
    // an http, ftp or file URL into type, address and path; anything without a scheme is a local file path (2d7a20)
    URL(const std::string& inURL) : mUrl(ee_won_url_obj_new(B(inURL), inURL.size())) {}
    URL(const URL& inURL) : mUrl(ee_won_url_obj_clone(inURL.mUrl)) {}
    URL& operator=(const URL& inURL) { ee_won_url_obj_assign(this->mUrl, inURL.mUrl); return *this; }
    ~URL() { ee_won_url_obj_free(this->mUrl); }
    #include EE_WON_DRV(URL_URL) // the drivers' members
    URLType GetType() const { return static_cast<URLType>(ee_won_url_obj_kind(this->mUrl)); } //SDK inline accessors
    std::string GetAddress() const { return this->Text(0); } // copies (the SDK returned its members)
    std::string GetPath() const { return this->Text(1); }
private:
    static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
    static void Sink(void* inText, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inText)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
    std::string Text(int inWhich) const { std::string s; ee_won_url_obj_text(this->mUrl, inWhich, Sink, &s); return s; }
    EeWonUrlObj* mUrl; // owned
};
}
#endif
