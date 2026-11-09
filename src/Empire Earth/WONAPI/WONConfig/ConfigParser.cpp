// C:\EEX\Empire Earth\WONAPI\WONConfig\ConfigParser.cpp
// ConfigParser (0x88 bytes): config file reader with comments, pushback, defines and group defines
// (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <stdio.h>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {

int ConfigParser::mLineCount = 0;
// FUNCTION: WON 0x007b1f90
// Sets the parent parser and inherits its include-path and warnings-as-errors flags
void ConfigParser::SetParent(ConfigParser* inParent)
{
    this->mParent = inParent;
    if (inParent != NULL) {
        this->mIgnorePathOnIncludeFile = inParent->mIgnorePathOnIncludeFile;
        this->mWarningsAsErrors = inParent->mWarningsAsErrors;
    }
}
// FUNCTION: WON 0x007b32b0
// Returns true when the file is exhausted and no character is pushed back
bool ConfigParser::EndOfFile()
{
    if(!this->mFileReader.EndOfFile()) return false;
    return this->mBackStr.empty();
}
// FUNCTION: WON 0x007b32d0
// Reads the next character (pushback first, 16-bit in Unicode files), turning CR LF and lone CR into LF
// A carriage return reads the next character through GetChar recursively (each one counts a line)
int ConfigParser::GetChar()
{
    int ch;
    if (this->mBackStr.empty()) {
        if (this->mIsUnicode) {
            ch = this->mFileReader.TryReadByte();
            ch |= this->mFileReader.TryReadByte() << 8;
        } else {
            ch = this->mFileReader.TryReadByte();
        }
    } else {
        ch = this->mBackStr[this->mBackStr.length() - 1];
        this->mBackStr.resize(this->mBackStr.length() - 1);
    }
    if (ch == '\r') {
        int next = this->GetChar();
        if (next != '\n') {
            this->UngetChar(next);
            ++ConfigParser::mLineCount;
            ++this->mLineNum;
        }
        return '\n';
    }
    if (ch == '\n') {
        ++ConfigParser::mLineCount;
        ++this->mLineNum;
    }
    return ch;
}
// FUNCTION: WON 0x007b34b0
// Returns the next character without consuming it, -1 at the end or with no file open
// Returns -1 with no file open even when characters are pushed back
int ConfigParser::PeekChar()
{
    if ((this->mFileReader.mFile != NULL || this->mFileReader.mBuffer != NULL) && !this->EndOfFile()) {
        int ch = this->GetChar();
        this->UngetChar(ch);
        return ch;
    }
    return -1;
}
// FUNCTION: WON 0x007b34f0
// Pushes a character back (EOF is ignored), uncounting a newline
// The line counters drop before the append that may throw
void ConfigParser::UngetChar(int inChar)
{
    if (inChar == -1) return;
    if (inChar == '\n') {
        --this->mLineNum;
        --ConfigParser::mLineCount;
    }
    this->mBackStr += (wchar_t)inChar;
}
// FUNCTION: WON 0x007b35e0
// Pushes a string back so it is read again in order
// Characters are sign-extended before going into the wide pushback string
void ConfigParser::UngetStr(const std::string& inString)
{
    for (int i = (int)inString.length() - 1; i >= 0; --i)
        this->UngetChar(inString[i]);
}
// FUNCTION: WON 0x007b3c50
// Skips whitespace and #, // and /* */ comments
// A slash that starts no comment is pushed back with the character after it
void ConfigParser::SkipWhitespace()
{
    while (!this->EndOfFile()) {
        int ch = this->GetChar();
        if ((unsigned int)ch < 256 && isspace(ch))
            continue;
        if (ch == '#') {
            this->SkipLine();
            continue;
        }
        if (ch == '/') {
            int next = this->GetChar();
            if (next == '/') {
                this->SkipLine();
                continue;
            }
            if (next == '*') {
                this->SkipBlockComment();
                continue;
            }
            this->UngetChar(next);
        }
        this->UngetChar(ch);
        return;
    }
}
// FUNCTION: WON 0x007b3ce0
// Skips to the end of the line, consuming the newline
void ConfigParser::SkipLine()
{
    while (!this->EndOfFile())
        if (this->GetChar() == '\n') return;
}
// FUNCTION: WON 0x007b3d10
// Skips to just past the closing */ of a block comment
void ConfigParser::SkipBlockComment()
{
    bool star = false;
    while (!this->EndOfFile()) {
        int ch = this->GetChar();
        if (star && ch == '/') return;
        star = ch == '*';
    }
}
// FUNCTION: WON 0x007b1fb0
// Records a parse error with file and line, unless one is already set and append is off
void ConfigParser::SetError(const std::string& inError, bool inAppend)
{
    if (!this->mAbortReason.empty() && !inAppend) return;
    char location[1024];
    sprintf(location, " (%s:%d)", this->mFileNameAndPath.c_str(), this->mLineNum);
    this->mAbortReason += inError + location;
}
// FUNCTION: WON 0x007b20f0
// Appends a warning line with file and line (also the error when warnings are errors), unless aborted
void ConfigParser::AddWarning(const std::string& inWarning)
{
    if (!this->mAbortReason.empty()) return;
    if (this->mWarningsAsErrors) this->SetError(inWarning, false);
    char warning[1024];
    sprintf(warning, "Warning (%s: %d) - %s\n", this->mFileNameAndPath.c_str(), this->mLineNum, inWarning.c_str());
    this->mWarnings += warning;
}
// FUNCTION: WON 0x007b2310
// Appends another parser's warnings to this one's
void ConfigParser::CopyWarnings(ConfigParser& inSource)
{
    this->mWarnings += inSource.mWarnings;
}
// FUNCTION: WON 0x007b2420
// Returns the parse error, first setting "Parse aborted." when there is none
const std::string& ConfigParser::GetAbortReason()
{
    if (this->mAbortReason.empty()) this->SetError("Parse aborted.", false);
    return this->mAbortReason;
}
// FUNCTION: WON 0x007b3840
// Saves the read position, file name, line number and pushback for a later SetState
// ConfigParserState (0x28 bytes): position +0, path +4, line +0x14, pushback +0x18
void ConfigParser::GetState(ConfigParserState& outState)
{
    outState.mBackStr = this->mBackStr;
    outState.mFileNameAndPath = this->mFileNameAndPath;
    outState.mFilePos = this->mFileReader.pos();
    outState.mLineNum = this->mLineNum;
}
// FUNCTION: WON 0x007b3aa0
// Returns to a saved state, reopening its file when needed; false when the reopen fails
// A failed reopen is not rolled back
bool ConfigParser::SetState(const ConfigParserState& inState)
{
    if (!this->mFileReader.IsOpen() ||
        inState.mFileNameAndPath != this->mFileNameAndPath) {
        if (!this->OpenFile(inState.mFileNameAndPath)) return false;
    }
    this->mFileReader.SetPos(inState.mFilePos);
    this->mLineNum = inState.mLineNum;
    this->mBackStr = inState.mBackStr;
    return true;
}
// FUNCTION: WON 0x007b3830
// Closes the config file
void ConfigParser::CloseFile()
{
    this->mFileReader.Close();
}
// FUNCTION: WON 0x007b3620
// Opens a config file (UTF-16 when it starts with FF FE), loads it into memory and notes its directory

bool ConfigParser::OpenFile(const std::string& inPath)
{
    this->CloseFile();
    this->mLineNum = 1;
    this->mIsUnicode = false;
    this->mBackStr.erase();
    this->mFileNameAndPath = inPath;
    std::string::size_type sep = this->mFileNameAndPath.rfind('/');
    if (sep == std::string::npos) sep = this->mFileNameAndPath.rfind('\\');
    if (sep == std::string::npos) this->mFilePath = "";
    else this->mFilePath = this->mFileNameAndPath.substr(0, sep + 1);
    this->mFileReader.OpenText(inPath.c_str());
    if (this->mFileReader.IsOpen()) {
        int first = this->mFileReader.TryReadByte();
        int second = this->mFileReader.TryReadByte();
        this->mFileReader.TryReadByte();
        int fourth = this->mFileReader.TryReadByte();
        if (first == 0xff && second == 0xfe && fourth == 0) {
            this->mFileReader.Open(inPath.c_str());
            this->mFileReader.SkipBytes(2);
            this->mIsUnicode = true;
        } else {
            this->mFileReader.Rewind();
        }
    }
    this->mFileReader.ReadIntoMemory();
    return this->mFileReader.IsOpen();
}
// Unlike the archived WON SDK, the define map holds std::string values, not DefineStruct

// FUNCTION: WON 0x007b2510
// Opens a new (empty) define scope on the root parser
void ConfigParser::PushDefineStack()
{
    ConfigParser* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    root->mDefineStack.push_back(NULL);
}
// FUNCTION: WON 0x007b25b0
// Closes the innermost define scope of the root parser, if any
void ConfigParser::PopDefineStack()
{
    ConfigParser* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    if(!root->mDefineStack.empty()) root->mDefineStack.pop_back();
}
// FUNCTION: WON 0x007b2610
// Adds a define to the innermost scope (REDEFINE replaces); warns about spaces or a redefinition
// The warning text keeps the original misspelling "Redefintion"
bool ConfigParser::AddDefine(const std::string& name,const std::string& value,bool redefine)
{
    ConfigParser* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    if(name.find(' ')!=std::string::npos) {
        root->AddWarning("Not allowed to have spaces in define names.");
        return true;
    }
    if(root->mDefineStack.empty()) root->mDefineStack.push_back(new DefineMapContainer);
    if(root->mDefineStack.back().get()==NULL) root->mDefineStack.back()=new DefineMapContainer;
    DefineMapContainer::DefineMap& defines=root->mDefineStack.back()->mDefineMap;
    if(redefine) defines[name]=value;
    else if(!defines.insert(DefineMapContainer::DefineMap::value_type(name,value)).second)
        root->AddWarning("Redefintion of "+name);
    return true;
}
// FUNCTION: WON 0x007b2b00
// Looks a define up from the innermost scope outwards; returns whether it exists
bool ConfigParser::GetDefine(const std::string& name,std::string& outValue)
{
    ConfigParser* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    DefineStack::reverse_iterator i=root->mDefineStack.rbegin();
    while(i!=root->mDefineStack.rend()) {
        if(i->get()!=NULL) {
            DefineMapContainer::DefineMap& defines=(*i)->mDefineMap;
            DefineMapContainer::DefineMap::iterator found=defines.find(name);
            if(found!=defines.end()) {outValue=found->second;return true;}
        }
        ++i;
    }
    return false;
}
// FUNCTION: WON 0x007b2be0
// Looks a define up and returns its value as a wide string
bool ConfigParser::GetDefine(const std::string& name,std::wstring& outValue)
{
    std::string value;
    if(!this->GetDefine(name,value)) return false;
    outValue=StringToWString(value);
    return true;
}
// FUNCTION: WON 0x007b2d20
// Records a named group at the given (or current) parser state in the root's innermost scope
// The local state is constructed even when the caller supplies one or a parent takes the call
bool ConfigParser::AddGroupDefine(const std::string& name,ConfigParserState* state)
{
    if(name.find(' ')!=std::string::npos) {
        this->AddWarning("Not allowed to have spaces in define names.");
        return true;
    }
    ConfigParserState localState;
    if(state==NULL) {this->GetState(localState);state=&localState;}
    if(this->mParent!=NULL) return this->mParent->AddGroupDefine(name,state);
    if(this->mDefineStack.empty()) this->mDefineStack.push_back(new DefineMapContainer);
    if(this->mDefineStack.back().get()==NULL) this->mDefineStack.back()=new DefineMapContainer;
    DefineMapContainer::GroupDefineMap& aMap=this->mDefineStack.back()->mGroupDefineMap;
    if(!aMap.insert(DefineMapContainer::GroupDefineMap::value_type(name,*state)).second) {
        this->AddWarning("Redefintion of "+name);
        return true;
    }
    return true;
}
// FUNCTION: WON 0x007b31a0
// Looks a group define up from the innermost scope outwards and copies its saved state
bool ConfigParser::GetGroupDefine(const std::string& name,ConfigParserState& outState)
{
    ConfigParser* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    DefineStack::reverse_iterator i=root->mDefineStack.rbegin();
    while(i!=root->mDefineStack.rend()) {
        if(i->get()!=NULL) {
            DefineMapContainer::GroupDefineMap& groups=(*i)->mGroupDefineMap;
            DefineMapContainer::GroupDefineMap::iterator found=groups.find(name);
            if(found!=groups.end()) {outState=found->second;return true;}
        }
        ++i;
    }
    return false;
}

// The original leaves the Unicode flag (+0x5d) uninitialized
// FUNCTION: WON 0x007b1d20
// Creates a parser with no file, no parent and all flags off
ConfigParser::ConfigParser()
{
    this->mLineNum=0;
    this->mParent=NULL;
    this->mStopped=false;
    this->mWarningsAsErrors=false;
    this->mIgnorePathOnIncludeFile=false;
}
// FUNCTION: WON 0x007b1e10
// Destroys the parser, closing its file
ConfigParser::~ConfigParser() {this->CloseFile();}

}

namespace WONAPI {
// No original address identified: WON SDK reset and wide pushback
void ConfigParser::Reset(void)
{
//	mDefineMap.clear();
	this->mDefineStack.clear();
	this->mAbortReason.erase();
	this->mWarnings.erase();
	this->mBackStr.erase();

	this->mLineNum = 1;
	this->mStopped = false;
//	if(mFile)
//		rewind(mFile);
	this->mFileReader.Rewind();
}

void ConfigParser::UngetStr(const std::wstring &theStr)
{
	for(int i=theStr.length()-1; i>=0; i--)
		this->UngetChar(theStr[i]);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b1df0
// (public: virtual void * __thiscall WONAPI::ConfigParser::`scalar deleting destructor'(unsigned int)    [??_GConfigParser@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007b30d0
// (std::pair<const string,ConfigParserState>::~pair)
// TEMPLATE: WON 0x007b3d90
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struc)
// TEMPLATE: WON 0x007b3de0
// (public: __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct WONAPI::ConfigParserState,s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b29d0
// (public: virtual void * __thiscall WONAPI::ConfigParser::DefineMapContainer::`scalar deleting destructor'(unsigned int)    [??_GDefineMapContainer@Conf)
// TEMPLATE: WON 0x007b29f0
// (public: virtual __thiscall WONAPI::ConfigParser::DefineMapContainer::~DefineMapContainer(void)    [??1DefineMapContainer@ConfigParser@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007b3e30
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007b5210
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x007b5260
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x007b5860
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic)
// TEMPLATE: WON 0x006d9c90
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class std::basic_string<ch)
// TEMPLATE: WON 0x007b4b00
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x007b4e00
// (public: class std::_Ptrit<class WONAPI::SmartPtr<struct WONAPI::ConfigParser::DefineMapContainer>,int,class WONAPI::SmartPtr<struct WONAPI::ConfigPars)
// TEMPLATE: WON 0x007b4ee0
// (public: class std::_Ptrit<class WONAPI::SmartPtr<struct WONAPI::ConfigParser::DefineMapContainer>,int,class WONAPI::SmartPtr<struct WONAPI::ConfigPars)
// TEMPLATE: WON 0x007b5200
// (public: __thiscall std::reverse_iterator<class std::_Ptrit<class WONAPI::SmartPtr<struct WONAPI::ConfigParser::DefineMapContainer>,int,class WONAPI::S)
// TEMPLATE: WON 0x007b4a50
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x007b4e60
// (public: class std::_Ptrit<class WONAPI::SmartPtr<struct WONAPI::ConfigParser::DefineMapContainer>,int,class WONAPI::SmartPtr<struct WONAPI::ConfigPars)
// TEMPLATE: WON 0x007b52d0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x007b5820
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x007b58b0
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<struct WONAPI::ConfigParser::DefineMapContainer>,class std::allocator<class WONAPI::SmartPt)
// TEMPLATE: WON 0x007b6a30
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,struct WO)
// TEMPLATE: WON 0x006aa7b0
// (public: __thiscall std::length_error::length_error(class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const &))
// TEMPLATE: WON 0x006da260
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class std::basic_string<ch)
// TEMPLATE: WON 0x006dcb40
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class std)
// TEMPLATE: WON 0x00754be0
// (class std::_Ptrit<class WONAPI::SmartPtr<struct WONAPI::ConfigParser::DefineMapContainer>,int,class WONAPI::SmartPtr<struct WONAPI::ConfigParser::Defi)
// TEMPLATE: WON 0x007b48c0
// (public: __thiscall WONAPI::ConfigParserState::ConfigParserState(struct WONAPI::ConfigParserState const &))
// TEMPLATE: WON 0x007693f0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<cla)
