// The original ConfigParser (WONConfig/ConfigParser.cpp, ConfigToken.cpp and its declaration in
// ee/wonapi/WONLobbyConfig.h, as they were before WON round 21 moved the class whole into ee_won_net
// cfgparser.rs), renamed OrigConfigParser, over the lexer's C ABI it called (ee_won config.rs). For the
// drivers only: won_cfgparser_test runs it against the game's ConfigParser.
#ifndef EE_TESTS_WON_CFGPARSER_ORIG_H
#define EE_TESTS_WON_CFGPARSER_ORIG_H
#include "ee/wonapi/WONLobbyConfig.h"
#include "won_file37_orig.h" // FileReader
namespace WONAPI {
class OrigConfigParser {
private:
    OrigConfigParser(const OrigConfigParser&);
    OrigConfigParser& operator=(const OrigConfigParser&);
public:
    OrigConfigParser();
    // the process-wide line count (0x966b54) is ee_won's (rust/crates/ee_won_net/src/config.rs)
    static void ResetLineCount() { ee_won_config_reset_line_count(); }
    static int GetLineCount() { return ee_won_config_line_count(); }
    virtual ~OrigConfigParser(); // 0x7b1e10 (closes the file; the only virtual)
    bool ReadToken(std::string&, bool inToEndLine=false, bool inSkip=false);
    bool ReadToken(std::wstring&, bool inToEndLine=false, bool inSkip=false);
    void SetParent(OrigConfigParser*);
    void SetError(const std::string&, bool);
    void AddWarning(const std::string&);
    void CopyWarnings(OrigConfigParser&);
    const std::string& GetAbortReason();
    bool IsStopped() { return this->mStopped; }
    void Stop() { this->mStopped=true; }
    bool GetIgnorePathOnIncludeFile() { return this->mIgnorePathOnIncludeFile; }
    bool EndOfFile();
    int GetChar();
    int PeekChar();
    void UngetChar(int);
    void UngetStr(const std::string&);
    void UngetStr(const std::wstring&);
    void SkipWhitespace();
    bool OpenFile(const std::string&);
    void CloseFile();
    void Reset();
    void PushDefineStack();
    void PopDefineStack();
    bool AddDefine(const std::string&, const std::string&, bool);
    bool GetDefine(const std::string&, std::string&);
    bool GetDefine(const std::string&, std::wstring&);
    bool AddGroupDefine(const std::string&, ConfigParserState*);
    bool GetGroupDefine(const std::string&, ConfigParserState&);
    void GetState(ConfigParserState&);
    bool SetState(const ConfigParserState&);
    OrigConfigParser* Root(); // the topmost parent (the defines' owner)
    EeWonConfigIo Io(); // the members ee_won reads and writes in a call
    // The characters, tokens, pushback and define scopes are ee_won's (config.rs): the pushback (0x2c) and the
    // define stack (0x04..0x14: scopes of a define map and a group define map) live in mLexer.
    EeWonConfigLexer* mLexer;
    OrigConfigParser* mParent; // 0x14
    FileReader mFileReader; // 0x18..0x2c (0x14 bytes)
    std::string mAbortReason; // 0x3c
    std::string mWarnings; // 0x4c
    bool mWarningsAsErrors; // 0x5c
    bool mIsUnicode; // 0x5d (the original constructor left it uninitialized; false now)
    int mLineNum; // 0x60
    std::string mFilePath; // 0x64
    std::string mFileNameAndPath; // 0x74
    bool mIgnorePathOnIncludeFile; // 0x84
    bool mStopped; // 0x85 (0x88 bytes in all)
};

// C:\EEX\Empire Earth\WONAPI\WONConfig\OrigConfigParser.cpp
// OrigConfigParser (0x88 bytes): config file reader with comments, pushback, defines and group defines
// (file grouping inferred). The characters, comments, pushback, define scopes, group defines and the open's
// UTF-16 sniff are ee_won's (rust/crates/ee_won_net/src/config.rs); this keeps the parent walk, the file names and the
// error and warning texts.



static void OrigConfigAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static void OrigConfigAppendUnits(void* inContext, const uint32_t* inUnits, size_t inCount)
{
    std::wstring* aText = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        *aText += (wchar_t)inUnits[i];
}
static void OrigConfigWideUnits(const std::wstring& inText, std::vector<uint32_t>& outUnits)
{
    outUnits.resize(inText.length());
    for (size_t i = 0; i < inText.length(); i++)
        outUnits[i] = (uint32_t)inText[i];
}

OrigConfigParser* OrigConfigParser::Root()
{
    OrigConfigParser* aRoot = this;
    while (aRoot->mParent != NULL)
        aRoot = aRoot->mParent;
    return aRoot;
}

EeWonConfigIo OrigConfigParser::Io()
{
    EeWonConfigIo anIo;
    anIo.reader = &this->mFileReader.mCore;
    anIo.line = &this->mLineNum;
    anIo.unicode = this->mIsUnicode;
    return anIo;
}

// Sets the parent parser and inherits its include-path and warnings-as-errors flags
void OrigConfigParser::SetParent(OrigConfigParser* inParent)
{
    this->mParent = inParent;
    if (inParent != NULL) {
        this->mIgnorePathOnIncludeFile = inParent->mIgnorePathOnIncludeFile;
        this->mWarningsAsErrors = inParent->mWarningsAsErrors;
    }
}
// Returns true when the file is exhausted and no character is pushed back
bool OrigConfigParser::EndOfFile()
{
    EeWonConfigIo anIo = this->Io();
    return ee_won_config_end_of_file(this->mLexer, &anIo);
}
// Reads the next character (pushback first, 16-bit in Unicode files), turning CR LF and lone CR into LF
int OrigConfigParser::GetChar()
{
    EeWonConfigIo anIo = this->Io();
    return ee_won_config_get_char(this->mLexer, &anIo);
}
// Returns the next character without consuming it, -1 at the end or with no file open
int OrigConfigParser::PeekChar()
{
    EeWonConfigIo anIo = this->Io();
    return ee_won_config_peek_char(this->mLexer, &anIo);
}
// Pushes a character back (EOF is ignored), uncounting a newline
void OrigConfigParser::UngetChar(int inChar)
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_unget_char(this->mLexer, &anIo, inChar);
}
// Pushes a string back so it is read again in order (each char sign-extended)
void OrigConfigParser::UngetStr(const std::string& inString)
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_unget_narrow(this->mLexer, &anIo, reinterpret_cast<const uint8_t*>(inString.data()),
                               inString.length());
}
// Skips whitespace and #, // and /* */ comments
void OrigConfigParser::SkipWhitespace()
{
    EeWonConfigIo anIo = this->Io();
    ee_won_config_skip_whitespace(this->mLexer, &anIo);
}
// Records a parse error with file and line, unless one is already set and append is off
void OrigConfigParser::SetError(const std::string& inError, bool inAppend)
{
    if (!this->mAbortReason.empty() && !inAppend) return;
    char location[1024];
    snprintf(location, sizeof(location), " (%s:%d)", this->mFileNameAndPath.c_str(), this->mLineNum);
    this->mAbortReason += inError + location;
}
// Appends a warning line with file and line (also the error when warnings are errors), unless aborted
void OrigConfigParser::AddWarning(const std::string& inWarning)
{
    if (!this->mAbortReason.empty()) return;
    if (this->mWarningsAsErrors) this->SetError(inWarning, false);
    char warning[1024];
    snprintf(warning, sizeof(warning), "Warning (%s: %d) - %s\n", this->mFileNameAndPath.c_str(), this->mLineNum,
             inWarning.c_str());
    this->mWarnings += warning;
}
// Appends another parser's warnings to this one's
void OrigConfigParser::CopyWarnings(OrigConfigParser& inSource)
{
    this->mWarnings += inSource.mWarnings;
}
// Returns the parse error, first setting "Parse aborted." when there is none
const std::string& OrigConfigParser::GetAbortReason()
{
    if (this->mAbortReason.empty()) this->SetError("Parse aborted.", false);
    return this->mAbortReason;
}
// Saves the read position, file name, line number and pushback for a later SetState
// ConfigParserState (0x28 bytes): position +0, path +4, line +0x14, pushback +0x18
void OrigConfigParser::GetState(ConfigParserState& outState)
{
    outState.mBackStr.erase();
    ee_won_config_get_back(this->mLexer, OrigConfigAppendUnits, &outState.mBackStr);
    outState.mFileNameAndPath = this->mFileNameAndPath;
    outState.mFilePos = this->mFileReader.pos();
    outState.mLineNum = this->mLineNum;
}
// Returns to a saved state, reopening its file when needed; false when the reopen fails
// A failed reopen is not rolled back
bool OrigConfigParser::SetState(const ConfigParserState& inState)
{
    if (!this->mFileReader.IsOpen() ||
        inState.mFileNameAndPath != this->mFileNameAndPath) {
        if (!this->OpenFile(inState.mFileNameAndPath)) return false;
    }
    this->mFileReader.SetPos(inState.mFilePos);
    this->mLineNum = inState.mLineNum;
    std::vector<uint32_t> aUnits;
    OrigConfigWideUnits(inState.mBackStr, aUnits);
    ee_won_config_set_back(this->mLexer, aUnits.empty() ? NULL : &aUnits[0], aUnits.size());
    return true;
}
// Closes the config file
void OrigConfigParser::CloseFile()
{
    this->mFileReader.Close();
}
// Opens a config file (UTF-16 when it starts with FF FE), loads it into memory and notes its directory
bool OrigConfigParser::OpenFile(const std::string& inPath)
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
void OrigConfigParser::PushDefineStack() { ee_won_config_push_scope(this->Root()->mLexer); }
// Closes the innermost define scope of the root parser, if any
void OrigConfigParser::PopDefineStack() { ee_won_config_pop_scope(this->Root()->mLexer); }
// Adds a define to the innermost scope (REDEFINE replaces); warns about spaces or a redefinition
// The warning text keeps the original misspelling "Redefintion"
bool OrigConfigParser::AddDefine(const std::string& name,const std::string& value,bool redefine)
{
    OrigConfigParser* root=this->Root();
    int aResult=ee_won_config_add_define(root->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                         reinterpret_cast<const uint8_t*>(value.data()),value.length(),redefine);
    if(aResult==1) root->AddWarning("Not allowed to have spaces in define names.");
    else if(aResult==2) root->AddWarning("Redefintion of "+name);
    return true;
}
// Looks a define up from the innermost scope outwards; returns whether it exists
bool OrigConfigParser::GetDefine(const std::string& name,std::string& outValue)
{
    std::string aValue;
    if(!ee_won_config_get_define(this->Root()->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                 OrigConfigAppendBytes,&aValue))
        return false;
    outValue=aValue;
    return true;
}
// Looks a define up and returns its value as a wide string
bool OrigConfigParser::GetDefine(const std::string& name,std::wstring& outValue)
{
    std::string value;
    if(!this->GetDefine(name,value)) return false;
    outValue=StringToWString(value);
    return true;
}
static void OrigConfigTakeState(void* inContext, const EeWonConfigState* inState)
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
bool OrigConfigParser::AddGroupDefine(const std::string& name,ConfigParserState* state)
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
    OrigConfigParser* root=this->Root();
    int aResult=ee_won_config_add_group(root->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                        &aState);
    if(aResult==1) this->AddWarning("Not allowed to have spaces in define names.");
    else if(aResult==2) root->AddWarning("Redefintion of "+name);
    return true;
}
// Looks a group define up from the innermost scope outwards and copies its saved state
bool OrigConfigParser::GetGroupDefine(const std::string& name,ConfigParserState& outState)
{
    return ee_won_config_get_group(this->Root()->mLexer,reinterpret_cast<const uint8_t*>(name.data()),name.length(),
                                   OrigConfigTakeState,&outState);
}

// Creates a parser with no file, no parent and all flags off
OrigConfigParser::OrigConfigParser()
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
OrigConfigParser::~OrigConfigParser() {this->CloseFile();ee_won_config_free(this->mLexer);}



// No original address identified: WON SDK reset and wide pushback
void OrigConfigParser::Reset(void)
{
	ee_won_config_reset(this->mLexer);
	this->mAbortReason.erase();
	this->mWarnings.erase();

	this->mLineNum = 1;
	this->mStopped = false;
	this->mFileReader.Rewind();
}

void OrigConfigParser::UngetStr(const std::wstring &theStr)
{
	std::vector<uint32_t> aUnits;
	OrigConfigWideUnits(theStr, aUnits);
	EeWonConfigIo anIo = this->Io();
	ee_won_config_unget_wide(this->mLexer, &anIo, aUnits.empty() ? NULL : &aUnits[0], aUnits.size());
}
// OrigConfigParser token reader (wide and narrow), with quotes, line joins and $define$ expansion. The reading is
// ee_won's (rust/crates/ee_won_net/src/config.rs read_token); the errors it names are set here.

struct OrigConfigTokenOut {
    std::string* mNarrow;
    std::wstring* mWide;
    std::string mName;
    static void Narrow(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<OrigConfigTokenOut*>(inContext)->mNarrow->assign(reinterpret_cast<const char*>(inBytes), inLength);
    }
    static void Wide(void* inContext, const uint32_t* inUnits, size_t inCount)
    {
        std::wstring* aText = static_cast<OrigConfigTokenOut*>(inContext)->mWide;
        aText->erase();
        for (size_t i = 0; i < inCount; i++)
            *aText += (wchar_t)inUnits[i];
    }
    static void Name(void* inContext, const uint8_t* inBytes, size_t inLength)
    {
        static_cast<OrigConfigTokenOut*>(inContext)->mName.assign(reinterpret_cast<const char*>(inBytes), inLength);
    }
};

// Nothing changes at the end of the data; otherwise the token is what was read (the whole token, or the part
// before an error)
static bool OrigConfigReadToken(OrigConfigParser* inParser, std::string* outNarrow, std::wstring* outWide, bool inToEndLine,
                            bool inSkip)
{
    OrigConfigTokenOut anOut;
    anOut.mNarrow = outNarrow;
    anOut.mWide = outWide;
    OrigConfigParser* aRoot = inParser->Root();
    EeWonConfigIo anIo = inParser->Io();
    int aResult = ee_won_config_read_token(inParser->mLexer, &anIo, aRoot == inParser ? NULL : aRoot->mLexer,
                                           outWide != NULL ? (uint32_t)sizeof(wchar_t) : 0, inToEndLine, inSkip,
                                           OrigConfigTokenOut::Narrow, OrigConfigTokenOut::Wide, OrigConfigTokenOut::Name,
                                           &anOut);
    switch (aResult) {
    case 1: return true;
    case 2: inParser->SetError("Expecting end quote", false); return false;
    case 3: inParser->SetError("Expecting closing '$'", false); return false;
    case 4: inParser->SetError("Define not found: " + anOut.mName, false); return false;
    default: return false;
    }
}

// Reads the next token as a wide string (or the rest of the line), expanding $name$ defines; false at the end or
// on an error
bool OrigConfigParser::ReadToken(std::wstring& token,bool toEndLine,bool skip)
{ return OrigConfigReadToken(this,NULL,&token,toEndLine,skip); }
// Reads the next token as a narrow string (or the rest of the line), expanding $name$ defines; false at the end or
// on an error
bool OrigConfigParser::ReadToken(std::string& token,bool toEndLine,bool skip)
{ return OrigConfigReadToken(this,&token,NULL,toEndLine,skip); }
}
#endif
