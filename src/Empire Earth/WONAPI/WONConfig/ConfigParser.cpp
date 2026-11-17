// C:\EEX\Empire Earth\WONAPI\WONConfig\ConfigParser.cpp
// ConfigParser (0x88 bytes): config file reader with comments, pushback, defines and group defines
// (file grouping inferred). The characters, comments, pushback, define scopes, group defines and the open's
// UTF-16 sniff are ee_won's (rust/crates/ee_won_net/src/config.rs); this keeps the parent walk, the file names and the
// error and warning texts.

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {

static void ConfigAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static void ConfigAppendUnits(void* inContext, const uint32_t* inUnits, size_t inCount)
{
    std::wstring* aText = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        *aText += (wchar_t)inUnits[i];
}
static void ConfigWideUnits(const std::wstring& inText, std::vector<uint32_t>& outUnits)
{
    outUnits.resize(inText.length());
    for (size_t i = 0; i < inText.length(); i++)
        outUnits[i] = (uint32_t)inText[i];
}

ConfigParser* ConfigParser::Root()
{
    ConfigParser* aRoot = this;
    while (aRoot->mParent != NULL)
        aRoot = aRoot->mParent;
    return aRoot;
}

EeWonConfigIo ConfigParser::Io()
{
    EeWonConfigIo anIo;
    anIo.reader = &this->mFileReader.mCore;
    anIo.line = &this->mLineNum;
    anIo.unicode = this->mIsUnicode;
    return anIo;
}

// Sets the parent parser and inherits its include-path and warnings-as-errors flags
void ConfigParser::SetParent(ConfigParser* inParent)
{
    this->mParent = inParent;
    if (inParent != NULL) {
        this->mIgnorePathOnIncludeFile = inParent->mIgnorePathOnIncludeFile;
        this->mWarningsAsErrors = inParent->mWarningsAsErrors;
    }
}
// Returns true when the file is exhausted and no character is pushed back
bool ConfigParser::EndOfFile()
{
    EeWonConfigIo anIo = this->Io();
    return ee_won_config_end_of_file(this->mLexer, &anIo);
}
// Reads the next character (pushback first, 16-bit in Unicode files), turning CR LF and lone CR into LF
int ConfigParser::GetChar()
{
    EeWonConfigIo anIo = this->Io();
    return ee_won_config_get_char(this->mLexer, &anIo);
}
// Returns the next character without consuming it, -1 at the end or with no file open
int ConfigParser::PeekChar()
{
    EeWonConfigIo anIo = this->Io();
    return ee_won_config_peek_char(this->mLexer, &anIo);
}
// Pushes a character back (EOF is ignored), uncounting a newline
void ConfigParser::UngetChar(int inChar)
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_unget_char(this->mLexer, &anIo, inChar);
}
// Pushes a string back so it is read again in order (each char sign-extended)
void ConfigParser::UngetStr(const std::string& inString)
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_unget_narrow(this->mLexer, &anIo, reinterpret_cast<const uint8_t*>(inString.data()),
                               inString.length());
}
// Skips whitespace and #, // and /* */ comments
void ConfigParser::SkipWhitespace()
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_skip_whitespace(this->mLexer, &anIo);
}
// Skips to the end of the line, consuming the newline
void ConfigParser::SkipLine()
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_skip_line(this->mLexer, &anIo);
}
// Skips to just past the closing */ of a block comment
void ConfigParser::SkipBlockComment()
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_skip_block_comment(this->mLexer, &anIo);
}
// Records a parse error with file and line, unless one is already set and append is off
void ConfigParser::SetError(const std::string& inError, bool inAppend)
{
    if (!this->mAbortReason.empty() && !inAppend) return;
    char location[1024];
    snprintf(location, sizeof(location), " (%s:%d)", this->mFileNameAndPath.c_str(), this->mLineNum);
    this->mAbortReason += inError + location;
}
// Appends a warning line with file and line (also the error when warnings are errors), unless aborted
void ConfigParser::AddWarning(const std::string& inWarning)
{
    if (!this->mAbortReason.empty()) return;
    if (this->mWarningsAsErrors) this->SetError(inWarning, false);
    char warning[1024];
    snprintf(warning, sizeof(warning), "Warning (%s: %d) - %s\n", this->mFileNameAndPath.c_str(), this->mLineNum,
             inWarning.c_str());
    this->mWarnings += warning;
}
// Appends another parser's warnings to this one's
void ConfigParser::CopyWarnings(ConfigParser& inSource)
{
    this->mWarnings += inSource.mWarnings;
}
// Returns the parse error, first setting "Parse aborted." when there is none
const std::string& ConfigParser::GetAbortReason()
{
    if (this->mAbortReason.empty()) this->SetError("Parse aborted.", false);
    return this->mAbortReason;
}
// Saves the read position, file name, line number and pushback for a later SetState
// ConfigParserState (0x28 bytes): position +0, path +4, line +0x14, pushback +0x18
void ConfigParser::GetState(ConfigParserState& outState)
{
    outState.mBackStr.erase();
    ee_won_config_get_back(this->mLexer, ConfigAppendUnits, &outState.mBackStr);
    outState.mFileNameAndPath = this->mFileNameAndPath;
    outState.mFilePos = this->mFileReader.pos();
    outState.mLineNum = this->mLineNum;
}
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
    std::vector<uint32_t> aUnits;
    ConfigWideUnits(inState.mBackStr, aUnits);
    ee_won_config_set_back(this->mLexer, aUnits.empty() ? NULL : &aUnits[0], aUnits.size());
    return true;
}
// Closes the config file
void ConfigParser::CloseFile()
{
    this->mFileReader.Close();
}
// Opens a config file (UTF-16 when it starts with FF FE), loads it into memory and notes its directory
bool ConfigParser::OpenFile(const std::string& inPath)
{
    this->CloseFile();
    this->mLineNum = 1;
    this->mIsUnicode = false;
    ee_won_config_set_back(this->mLexer, NULL, 0);
    this->mFileNameAndPath = inPath;
    std::string::size_type sep = this->mFileNameAndPath.rfind('/');
    if (sep == std::string::npos) sep = this->mFileNameAndPath.rfind('\\');
    if (sep == std::string::npos) this->mFilePath = "";
    else this->mFilePath = this->mFileNameAndPath.substr(0, sep + 1);
    EeWonConfigIo anIo = this->Io();
    int32_t aStatus = 0;
    bool anOpen = ee_won_config_open(&anIo, inPath.c_str(), &aStatus);
    this->mIsUnicode = anIo.unicode;
    if (aStatus != 0)
        throw FileReaderException(ee_won_file_status_text(aStatus));
    return anOpen;
}

// Opens a new (empty) define scope on the root parser
void ConfigParser::PushDefineStack() { ee_won_config_push_scope(this->Root()->mLexer); }
// Closes the innermost define scope of the root parser, if any
void ConfigParser::PopDefineStack() { ee_won_config_pop_scope(this->Root()->mLexer); }
// Adds a define to the innermost scope (REDEFINE replaces); warns about spaces or a redefinition
// The warning text keeps the original misspelling "Redefintion"
bool ConfigParser::AddDefine(const std::string& name,const std::string& value,bool redefine)
{
    ConfigParser* root=this->Root();
    int aResult=ee_won_config_add_define(root->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                         reinterpret_cast<const uint8_t*>(value.data()),value.length(),redefine);
    if(aResult==1) root->AddWarning("Not allowed to have spaces in define names.");
    else if(aResult==2) root->AddWarning("Redefintion of "+name);
    return true;
}
// Looks a define up from the innermost scope outwards; returns whether it exists
bool ConfigParser::GetDefine(const std::string& name,std::string& outValue)
{
    std::string aValue;
    if(!ee_won_config_get_define(this->Root()->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                 ConfigAppendBytes,&aValue))
        return false;
    outValue=aValue;
    return true;
}
// Looks a define up and returns its value as a wide string
bool ConfigParser::GetDefine(const std::string& name,std::wstring& outValue)
{
    std::string value;
    if(!this->GetDefine(name,value)) return false;
    outValue=StringToWString(value);
    return true;
}
static void ConfigTakeState(void* inContext, const EeWonConfigState* inState)
{
    ConfigParserState* aState = static_cast<ConfigParserState*>(inContext);
    aState->mFilePos = inState->pos;
    aState->mFileNameAndPath.assign(reinterpret_cast<const char*>(inState->path), inState->path_len);
    aState->mLineNum = inState->line;
    aState->mBackStr.erase();
    for (size_t i = 0; i < inState->back_len; i++)
        aState->mBackStr += (wchar_t)inState->back[i];
}
// Records a named group at the given (or current) parser state in the root's innermost scope
// (a name with spaces warns on this parser, a redefinition on the root)
bool ConfigParser::AddGroupDefine(const std::string& name,ConfigParserState* state)
{
    ConfigParserState localState;
    if(state==NULL) {this->GetState(localState);state=&localState;}
    std::vector<uint16_t> aBack(state->mBackStr.length());
    for(size_t i=0;i<aBack.size();i++) aBack[i]=(uint16_t)state->mBackStr[i];
    EeWonConfigState aState;
    aState.pos=state->mFilePos;
    aState.path=reinterpret_cast<const uint8_t*>(state->mFileNameAndPath.data());
    aState.path_len=state->mFileNameAndPath.length();
    aState.line=state->mLineNum;
    aState.back=aBack.empty()?NULL:&aBack[0];
    aState.back_len=aBack.size();
    ConfigParser* root=this->Root();
    int aResult=ee_won_config_add_group(root->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                        &aState);
    if(aResult==1) this->AddWarning("Not allowed to have spaces in define names.");
    else if(aResult==2) root->AddWarning("Redefintion of "+name);
    return true;
}
// Looks a group define up from the innermost scope outwards and copies its saved state
bool ConfigParser::GetGroupDefine(const std::string& name,ConfigParserState& outState)
{
    return ee_won_config_get_group(this->Root()->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                   ConfigTakeState,&outState);
}

// Creates a parser with no file, no parent and all flags off
ConfigParser::ConfigParser()
{
    this->mLexer=ee_won_config_new();
    this->mLineNum=0;
    this->mParent=NULL;
    this->mStopped=false;
    this->mWarningsAsErrors=false;
    this->mIgnorePathOnIncludeFile=false;
    this->mIsUnicode=false;
}
// Destroys the parser, closing its file
ConfigParser::~ConfigParser() {this->CloseFile();ee_won_config_free(this->mLexer);}

}

namespace WONAPI {
// No original address identified: WON SDK reset and wide pushback
void ConfigParser::Reset(void)
{
	ee_won_config_reset(this->mLexer);
	this->mAbortReason.erase();
	this->mWarnings.erase();

	this->mLineNum = 1;
	this->mStopped = false;
	this->mFileReader.Rewind();
}

void ConfigParser::UngetStr(const std::wstring &theStr)
{
	std::vector<uint32_t> aUnits;
	ConfigWideUnits(theStr, aUnits);
	EeWonConfigIo anIo = this->Io();
	ee_won_config_unget_wide(this->mLexer, &anIo, aUnits.empty() ? NULL : &aUnits[0], aUnits.size());
}
}
