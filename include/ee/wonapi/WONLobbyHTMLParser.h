// HTMLParser (0x2c bytes, non-virtual); the original constructs it at 0x00742ae0. The destructor
// is implicit (member cleanup).
#ifndef EE_WONAPI_WONLOBBYHTMLPARSER_H
#define EE_WONAPI_WONLOBBYHTMLPARSER_H
#include <cstdio>
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyHTMLTag.h"
namespace WONAPI {
class HTMLParser {
protected:
    FILE* mFile; //00, older target usesCRT FILE* rather than laterSDK FileReader20
    GUIString mBackStr; //04
    int mLineNum; //08
    HTMLTagAttributes mTagAttributes; //0c..17
    SmartPtr<HTMLDocument> mDocument; // offset 0x18 (the document's destructor is virtual)
    HTMLTagList mTagList; //1c..27
    typedef HTMLTag* (*TagFactoryFunc)(HTMLTagType);
    struct TagLookupInfo {
        HTMLTagType mType;
        TagFactoryFunc mFactoryFunc;
        TagLookupInfo() : mType(HTMLTagType_None),mFactoryFunc(NULL) {}
        TagLookupInfo(TagFactoryFunc f,HTMLTagType t) : mType(t),mFactoryFunc(f) {}
    };
    typedef std::map<GUIString,TagLookupInfo,GUIStringLessNoCase> TagNameMap;
    static TagNameMap& GetTagNameMap(); // tag table initializer at 0x00740740
    TagNameMap& mTagNameMap; //28
    void CloseFile(); //342bb0
    bool EndOfFile() { if(this->mFile==NULL || feof(this->mFile)) return (this->mBackStr.length()==0); else return false; } //343ff0 = its out-of-line COMDAT
    int GetChar(); //3434d0
    int PeekChar(); // at 0x00744010
    void UngetChar(int theChar) { if(theChar==EOF) return; if(theChar=='\n') this->mLineNum--; this->mBackStr.append(theChar); } //3430f0 = its out-of-line COMDAT
    void UngetString(const GUIString&);
    void UngetString(const GUIString&,int);
    bool SkipWhitespace(); //344200
    int ReadEntity(); //342c10
    bool ReadWord(GUIString&,const char* =NULL,bool=true,bool=false,bool=true); //343280
    void SkipTag(const GUIString&); //3435e0
    bool GetActualTag(); //3438a0
    bool GetNextTag(); //3443f0
    bool Parse(); // at 0x007445f0 (allocates the 0x80-byte HTMLDocument)
public:
    HTMLParser(); // at 0x00742ae0
    void SetDocument(HTMLDocument* document) { this->mDocument=document; } // inline
    HTMLDocument* GetDocument() { return this->mDocument.get(); } // inline getter (borrowed)
    bool ParseFromFile(const GUIString&); //3446e0
    bool ParseFromText(const GUIString&); //344850
};
}
#endif
