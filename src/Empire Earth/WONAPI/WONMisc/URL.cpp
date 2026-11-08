// URL: parsing of http/ftp/file URLs and relative URL resolution
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
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
namespace WONAPI {
// FUNCTION: WON 0x006d7a20
// Parses an http, ftp or file URL into type, address and path; anything without a scheme is a local file path
URL::URL(const std::string& inURL)
{
    this->mIsAbsolute=false;
    int protocolEnd=inURL.find("://");
    if (protocolEnd!=std::string::npos) {
        std::string protocol=inURL.substr(0,protocolEnd);
        if (stricmp(protocol.c_str(),"HTTP")==0) this->mType=URLType_HTTP;
        else if (stricmp(protocol.c_str(),"FTP")==0) this->mType=URLType_FTP;
        else if (stricmp(protocol.c_str(),"FILE")==0) this->mType=URLType_File;
        else {
            this->mType=URLType_None;
            return;
        }
        this->mIsAbsolute=true;
        protocolEnd+=3;
        int addressEnd=inURL.find_first_of('/',protocolEnd);
        this->mAddress=inURL.substr(protocolEnd,addressEnd-protocolEnd);
        if (addressEnd==std::string::npos) this->mPath="/";
        else this->mPath=inURL.substr(addressEnd);
    } else {
        this->mType=URLType_File;
        this->mPath=inURL;
    }
    if (this->mType==URLType_File) {
        for (int i=0;i<this->mPath.length();++i)
            if (this->mPath[i]=='\\') this->mPath[i]='/';
    }
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
    if (this->mIsAbsolute) return;
    this->mIsAbsolute=inBase.mIsAbsolute;
    this->mType=inBase.mType;
    this->mAddress=inBase.mAddress;
    if (this->mPath[0]=='#') {
        this->mPath=inBase.mPath+this->mPath;
        return;
    }
    if (!this->mPath.empty() && !inBase.mPath.empty() && this->mPath[0]!='/') {
        std::string relativePath=this->mPath;
        this->mPath=inBase.mPath;
        int slash=this->mPath.rfind('/');
        if (slash==std::string::npos) this->mPath+='/';
        else this->mPath=this->mPath.substr(0,slash+1);
        this->mPath+=relativePath;
    }
}
// FUNCTION: WON 0x006d8150
// Builds the full URL text from scheme, address and path
void URL::GetEntire(std::string& outString)
{
    outString.erase();
    switch (this->mType) {
    case URLType_HTTP: outString+="http://"; break;
    case URLType_FTP: outString+="ftp://"; break;
    }
    outString+=this->mAddress;
    outString+=this->mPath;
}
}
