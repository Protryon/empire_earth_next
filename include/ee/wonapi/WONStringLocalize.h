// StringLocalize core API (no state); the wrappers are in the original only
#ifndef EE_WONAPI_WONSTRINGLOCALIZE_H
#define EE_WONAPI_WONSTRINGLOCALIZE_H
#include "ee/wonapi/WONLobbyResource.h"
namespace WONAPI {
class StringLocalize {
public:
    static GUIString GetStr(const GUIString&,const GUIString**,int); //349f10, cdecl, returns an owning sret
};
}
#endif
