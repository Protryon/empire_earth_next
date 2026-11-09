
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyParser.h"
namespace WONAPI {

// Same body as StringParser.cpp 0x6a5a70: appends the integer in decimal
void StringParser::ValueToString(const int& inValue, std::string& outText)
{
    char aBuffer[50];
    sprintf(aBuffer, "%d", inValue);
    outText += aBuffer;
}

// Same body as StringParser.cpp 0x6a5c40: widens the 16-bit value and appends it via the int form
void StringParser::ValueToString(const unsigned short& inValue, std::string& outText)
{
    int aValue = inValue;
    StringParser::ValueToString(aValue, outText);
}

// Same body as StringParser.cpp 0x6a5c60: appends the string
void StringParser::ValueToString(const std::string& inValue, std::string& outText)
{
    outText += inValue;
}

// Same body as StringParser.cpp 0x6a5d70: appends the wide string converted to narrow
void StringParser::ValueToString(const std::wstring& inValue, std::string& outText)
{
    outText += WStringToString(inValue);
}

}
