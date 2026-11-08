// Text-to-HTML character stream, prefix matching, URL-aware text insertion and document generation.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <wctype.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyHTMLTag.h"
#include "ee/wonapi/WONLobbyHTMLFromText.h"
#include "ee/wonapi/WONLobbyHTMLParser.h"
// FUNCTION: WON 0x0073f6e0
// Static initializer: constructs the URL protocol prefix table and registers its destructor
// (_$E74)
// FUNCTION: WON 0x0073f6f0
// Constructs the five URL protocol prefixes (HTTP://, HTTPS://, FTP://, MAILTO:, WWW.)
// (_$E71)
// FUNCTION: WON 0x0073f740
// Registers the protocol prefix table's destructor with atexit
// (_$E73)
// FUNCTION: WON 0x0073f750
// Destroys the protocol prefix table at exit
// (_$E72)
extern const WONAPI::GUIString gProtocols[5];
extern const WONAPI::GUIString gProtocolPrepends[5];
const WONAPI::GUIString gProtocols[] = { "HTTP://", "HTTPS://", "FTP://", "MAILTO:", "WWW." };
// FUNCTION: WON 0x0073f770
// Static initializer: constructs the link prefix table and registers its destructor
// (_$E79)
// FUNCTION: WON 0x0073f780
// Constructs the link prefixes added before each protocol's links (HTTP:// for WWW., else none)
// (_$E76)
// FUNCTION: WON 0x0073f7d0
// Registers the link prefix table's destructor with atexit
// (_$E78)
// FUNCTION: WON 0x0073f7e0
// Destroys the link prefix table at exit
// (_$E77)
const WONAPI::GUIString gProtocolPrepends[] = { "", "", "", "", "HTTP://" };
static int gNumProtocols = 5;
namespace WONAPI {
// FUNCTION: WON 0x0073f510
// Creates a text-to-HTML converter with no open file
HTMLFromText::HTMLFromText()
{
    this->mFile=NULL;
}
// FUNCTION: WON 0x0073f580
// Drops the pending text and closes the input file
void HTMLFromText::CloseFile()
{
	this->mBackStr = GetEmptyGUIStringData();
	if(this->mFile!=NULL)
	{
		fclose(this->mFile);
		this->mFile = NULL;
	}
}
// FUNCTION: WON 0x0073f5f0
// Returns whether the input is used up: no pending text and no file data left
bool HTMLFromText::EndOfFile()
{	
	if(!this->mFile || feof(this->mFile))
		return this->mBackStr.length()==0;
	else
		return false;
}
// FUNCTION: WON 0x0073F610
// Returns the next input character from the pending text (stored reversed) or the file; -1 at the end
int HTMLFromText::GetChar()
{
	int aChar = 0;
	if(this->mBackStr.length()==0)
	{
		if(this->mFile)
			aChar = fgetc(this->mFile);
		else 
			aChar = -1;
	}
	else
	{
		aChar = this->mBackStr[this->mBackStr.length()-1];
		this->mBackStr.resize(this->mBackStr.length()-1);
	}

	return aChar;
}

// FUNCTION: WON 0x0073F800
// Returns whether one string contains another at the given positions, ignoring case
bool HTMLFromText::StartsWith(const GUIString &s1, int p1, const GUIString &s2, int p2)
{
	int c1, c2;
	do
	{
		c1 = towupper(s1[p1++]);
		c2 = towupper(s2[p2++]);
		if(c2=='\0')
			return true;
		else if(c1!=c2)
			return false;

	} while(c1!='\0');

	return false;
}

// FUNCTION: WON 0x0073F8B0
// Turns a URL starting a word at the position into a link tag; adds the text before it and keeps the rest
bool HTMLFromText::CheckURL(GUIString &aStr, int &aPos)
{
	for(int i=0; i<gNumProtocols; i++)
	{
		if(this->StartsWith(aStr,aPos,gProtocols[i],0) &&
		   (aPos==0 || aStr[aPos-1]== ' '))
		{
			int anEndPos = aPos;
			int anItr = aPos;
			while(anItr<aStr.length())
			{
				int aChar = aStr[anItr];
				if(aChar>='0' && aChar<='9' || aChar>='a' && aChar<='z' || aChar>='A' && aChar<='Z')
					anEndPos = anItr+1;
				else if(aChar==' ')
					break;
				
				anItr++;
			}
			
			if(aPos>0)
				this->mDocument->AddTag(new HTMLTextTag(aStr.substr(0,aPos)));

			HTMLAnchorTag *anAnchor = new HTMLAnchorTag;
			GUIString aLink = aStr.substr(aPos,anEndPos-aPos);
			GUIString anHRef = gProtocolPrepends[i];
			anHRef.append(aLink);

			anAnchor->mHRef = anHRef;
			this->mDocument->AddTag(anAnchor);
			this->mDocument->AddTag(new HTMLTextTag(aLink));

			anAnchor = new HTMLAnchorTag;
			anAnchor->SetFlags(HTMLTagFlag_IsEndTag,true);
			this->mDocument->AddTag(anAnchor);

			aStr = aStr.substr(anEndPos);
			aPos = 0;
			return true;
		}
	}

	return false;
}
// FUNCTION: WON 0x0073fc30
// Deleting destructor of an anchor tag (compiler-generated)
// (??_GHTMLAnchorTag@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0073fc50
// Destroys an anchor tag, releasing its name and link
// (??1HTMLAnchorTag@WONAPI@@UAE@XZ)
// FUNCTION: WON 0x0073fce0
// Deleting destructor of a text tag (compiler-generated)
// (??_GHTMLTextTag@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0073fd00
// Destroys a text tag, releasing its text
// (??1HTMLTextTag@WONAPI@@UAE@XZ)

// FUNCTION: WON 0x0073FD60
// Adds a line of plain text to the document, turning URLs in it into links
void HTMLFromText::AddStr(GUIString &theStr)
{
	int aPos = 0;

	while(aPos<theStr.length())
	{
		if(this->CheckURL(theStr,aPos))
			continue;
		
//		if(CheckEmoticon(aStr,aPos))
//			continue;

//		if(CheckFontInstruction(aStr,aPos,aCurFont))
//			continue;

		// check change color instruction
//		if(CheckChangeColor(aStr,aPos))
//			continue;

		aPos++;
	}

	if(theStr.length()!=0)
		this->mDocument->AddTag(new HTMLTextTag(theStr));
}


// FUNCTION: WON 0x0073fe40
// Converts the plain-text input into a document: one line break tag per newline, optionally inside <BODY>
HTMLDocument* HTMLFromText::Generate(bool wantBody)
{
    this->mDocument=new HTMLDocument(gEmptyGUIString);
    if(wantBody) this->mDocument->AddTag(new HTMLBodyTag);
    GUIString current((unsigned short)32);
    while(!this->EndOfFile()) {
        int ch=this->GetChar();
        if(ch=='\n') {
            if(current.length()!=0) {
                this->AddStr(current);
                current.erase();
                current.reserve(32);
            }
            this->mDocument->AddTag(new HTMLLineBreakTag);
        } else if(ch!=-1) current.append(ch);

        else break;
    }
    if(current.length()!=0) this->AddStr(current);
    if(wantBody) {
        HTMLBodyTag* tag=new HTMLBodyTag;
        tag->SetFlags(HTMLTagFlag_IsEndTag,true);
        this->mDocument->AddTag(tag);
    }
    this->CloseFile();
    return this->mDocument.get();
}

// FUNCTION: WON 0x007400a0
// Converts a file to a document, parsed as HTML when flagged (or optionally when it starts with '<'); NULL if unopened
HTMLDocument* HTMLFromText::GenerateFromFile(const GUIString& path,int flags)
{
    this->CloseFile();
    if(path.empty()) return NULL;
    std::string narrowPath=path;
    this->mFile=fopen(narrowPath.c_str(),"r");
    if(this->mFile==NULL) return NULL;
    bool isHTML=(flags&HTMLDocGenFlag_IsHTML)!=0;
    if(flags&HTMLDocGenFlag_IsOptionalHTML) {
        int first=fgetc(this->mFile);
        if(first=='<') {
            this->CloseFile();
            isHTML=true;
        } else if(!(flags&HTMLDocGenFlag_SkipFirstCharIfNotHTML)) ungetc(first,this->mFile);
    }
    if(isHTML) {
        HTMLParser parser;
        parser.ParseFromFile(path);
        this->mDocument=parser.GetDocument();
        // The file opened above stays open when HTML was forced by flag (closed by the next CloseFile)
        return this->mDocument.get();
    }
    return this->Generate(false);
}

// FUNCTION: WON 0x00740340
// Destroys an HTML parser
// (??1HTMLParser@WONAPI@@QAE@XZ)

// FUNCTION: WON 0x00740440
// Converts text to a document, parsed as HTML when flagged (or optionally when it starts with '<')
HTMLDocument* HTMLFromText::GenerateFromText(const GUIString& text,int flags)
{
    this->CloseFile();
    bool isHTML=(flags&HTMLDocGenFlag_IsHTML)!=0;
    if((flags&HTMLDocGenFlag_IsOptionalHTML) && text.length()!=0 && text[0]=='<')
        isHTML=true;
    if(isHTML) {
        HTMLParser parser;
        parser.ParseFromText(text);
        this->mDocument=parser.GetDocument();
        return this->mDocument.get();
    }
    this->mBackStr.resize(0);
    this->mBackStr.reserve(text.length());
    for(int i=text.length()-1;i>=0;--i) this->mBackStr.append(text[i]);
    if(flags&HTMLDocGenFlag_IsOptionalHTML) {
        if((flags&HTMLDocGenFlag_SkipFirstCharIfNotHTML) && this->mBackStr.length()>0)
            this->mBackStr.resize(this->mBackStr.length()-1);
    }
    return this->Generate(false);
}

// FUNCTION: WON 0x00740710
// Converts a file or a text to a document depending on the file flag
HTMLDocument* HTMLFromText::Generate(const GUIString& text,int flags)
{
    if(flags&HTMLDocGenFlag_IsFile) return this->GenerateFromFile(text,flags);
    return this->GenerateFromText(text,flags);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0074b130
// (public: __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Mapk<class)
// TEMPLATE: WON 0x0074b580
// (public: __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>::~pair<class WONAPI::GUIString const ,class WONAPI::GUIString>(vo)
// TEMPLATE: WON 0x0074bdd0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Mapk<class WONA)
// TEMPLATE: WON 0x0074dec0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Mapk<class WONA)
// TEMPLATE: WON 0x0074e3e0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>,struct std::_Ma)
// TEMPLATE: WON 0x00754fe0
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::GUIString>::`scalar deleting destructor'(unsigned int))
