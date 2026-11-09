#ifndef EE_WONAPI_WONLOBBYHTMLFROMTEXT_H
#define EE_WONAPI_WONLOBBYHTMLFROMTEXT_H
#include <cstdio>
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
enum HTMLDocGenFlags {
    HTMLDocGenFlag_IsHTML=0x0001,HTMLDocGenFlag_IsFile=0x0002,
    HTMLDocGenFlag_IsOptionalHTML=0x0004,HTMLDocGenFlag_SkipFirstCharIfNotHTML=0x0008
};
// HTMLFromText (0x0c bytes, stack value, no virtual slots): constructor at 0x0073f510; the implicit
// destructor (0x0073f170) releases the GUIString at +8, then the document at +0. The FILE* at +4
// is managed by Generate/CloseFile.
class HTMLFromText {
protected:
    SmartPtr<HTMLDocument> mDocument; //00
    FILE* mFile; //04
    GUIString mBackStr; //08
    HTMLDocument* Generate(bool); // at 0x0073fe40
    HTMLDocument* GenerateFromFile(const GUIString&,int); // at 0x007400a0
    HTMLDocument* GenerateFromText(const GUIString&,int); // at 0x00740440
    void CloseFile(); // cleanup (0x0073f580)
    bool EndOfFile(); // EOF query (0x0073f5f0)
    int GetChar(); //33f610
    bool StartsWith(const GUIString&,int,const GUIString&,int); //33f800
    bool CheckURL(GUIString&,int&); // at 0x0073f8b0
    void AddStr(GUIString&); //33fd60
public:
    HTMLFromText(); // at 0x0073f510
    HTMLDocument* Generate(const GUIString&,int=0); // at 0x00740710; returns a raw pointer
    // Natural implicit destructor matches original; no invented explicit empty stub.
};
}
#endif
