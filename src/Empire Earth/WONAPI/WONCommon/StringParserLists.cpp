// StringParser string-list reads (the WON SDK's static ReadList<T>)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyParser.h"

using namespace WONAPI;

// Reads comma-separated strings into the list until no comma follows; false when an element fails
template<class T>
static bool ReadList(StringParser* parser,std::list<T>& values)
{
    while(true)
    {
        values.push_back(T());
        if(!parser->ReadString(values.back(),false))
        {
            values.pop_back();
            return false;
        }
        if(!parser->CheckNextChar(','))
            break;
    }
    return true;
}

// Reads a comma-separated list of strings
bool StringParser::ReadValue(std::list<std::string>& values)
{
    return ReadList<std::string>(this,values);
}

// Reads a comma-separated list of wide strings
bool StringParser::ReadValue(std::list<std::wstring>& values)
{
    return ReadList<std::wstring>(this,values);
}
