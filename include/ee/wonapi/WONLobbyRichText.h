#ifndef EE_WONAPI_WONLOBBYRICHTEXT_H
#define EE_WONAPI_WONLOBBYRICHTEXT_H
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
// Original824B implementation34a1c0; call-only declaration, no new body.
void LobbyAppendFormattedText(TextArea* inArea,const GUIString& inFormat,
    const GUIString*const* inStrings,int inNumStrings,const int* inColors,int inNumColors,
    Font*const* inFonts,int inNumFonts,TextSegmentBase*const* inSegments,int inNumSegments,
    bool inLineBreak);
}
#endif
