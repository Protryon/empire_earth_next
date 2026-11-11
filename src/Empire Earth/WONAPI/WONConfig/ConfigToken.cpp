// ConfigParser token reader (wide and narrow), with quotes, line joins and $define$ expansion. The reading is
// ee_won's (rust/crates/ee_won/src/config.rs read_token); the errors it names are set here.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {
struct ConfigTokenOut {
    std::string* mNarrow;
    std::wstring* mWide;
    std::string mName;
    static void Narrow(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<ConfigTokenOut*>(inContext)->mNarrow->assign(reinterpret_cast<const char*>(inBytes), inLength);
    }
    static void Wide(void* inContext, const uint32_t* inUnits, size_t inCount)
    {
        std::wstring* aText = static_cast<ConfigTokenOut*>(inContext)->mWide;
        aText->erase();
        for (size_t i = 0; i < inCount; i++)
            *aText += (wchar_t)inUnits[i];
    }
    static void Name(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<ConfigTokenOut*>(inContext)->mName.assign(reinterpret_cast<const char*>(inBytes), inLength);
    }
};

// Nothing changes at the end of the data; otherwise the token is what was read (the whole token, or the part
// before an error)
static bool ConfigReadToken(ConfigParser* inParser, std::string* outNarrow, std::wstring* outWide, bool inToEndLine,
                            bool inSkip)
{
    ConfigTokenOut anOut;
    anOut.mNarrow = outNarrow;
    anOut.mWide = outWide;
    ConfigParser* aRoot = inParser->Root();
    EeWonConfigIo anIo = inParser->Io();
    int aResult = ee_won_config_read_token(inParser->mLexer, &anIo, aRoot == inParser ? NULL : aRoot->mLexer,
                                           outWide != NULL ? (uint32_t)sizeof(wchar_t) : 0, inToEndLine, inSkip,
                                           ConfigTokenOut::Narrow, ConfigTokenOut::Wide, ConfigTokenOut::Name,
                                           &anOut);
    switch (aResult) {
    case 1: return true;
    case 2: inParser->SetError("Expecting end quote", false); return false;
    case 3: inParser->SetError("Expecting closing '$'", false); return false;
    case 4: inParser->SetError("Define not found: " + anOut.mName, false); return false;
    default: return false;
    }
}

// FUNCTION: WON 0x007b5cf0
// (ReadToken<wchar_t>) The wide token reader, ee_won's now
// FUNCTION: WON 0x007b3d50
// Reads the next token as a wide string (or the rest of the line), expanding $name$ defines; false at the end or
// on an error
bool ConfigParser::ReadToken(std::wstring& token,bool toEndLine,bool skip)
{ return ConfigReadToken(this,NULL,&token,toEndLine,skip); }
// FUNCTION: WON 0x007b6370
// (ReadToken<char>) The narrow token reader, ee_won's now
// FUNCTION: WON 0x007b3d70
// Reads the next token as a narrow string (or the rest of the line), expanding $name$ defines; false at the end or
// on an error
bool ConfigParser::ReadToken(std::string& token,bool toEndLine,bool skip)
{ return ConfigReadToken(this,&token,NULL,toEndLine,skip); }
}
