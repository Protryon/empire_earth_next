// Named skin controls: case-insensitive lookup and conditional attribute copy.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"

using namespace WONAPI;

// FUNCTION: WON 0x00715c90
// Copies the attributes of the named skin button type (case-insensitive) to a button; false if not found
bool WONSkin::CopyButtonType(const GUIString& type,WONButton* copyTo)
{
    ButtonTypeMap::iterator it=this->mButtonTypeMap.find(type);
    if(it==this->mButtonTypeMap.end())
        return false;
    copyTo->CopyAttributes(it->second.get());
    return true;
}

// FUNCTION: WON 0x00715d00
// Copies the attributes of the named simple button type (case-insensitive) to a button; false if not found
bool WONSkin::CopySimpleButtonType(const GUIString& type,WONSimpleButton* copyTo)
{
    SimpleButtonTypeMap::iterator it=this->mSimpleButtonTypeMap.find(type);
    if(it==this->mSimpleButtonTypeMap.end())
        return false;
    copyTo->CopyAttributes(it->second.get());
    return true;
}
