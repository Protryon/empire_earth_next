// WONLobby command parser: original allocation 0x54, RTTI ChatCommandParser : RefCount.
// Names are provisional by behavior; field representations follow constructors, callers and EH cleanup.
#ifndef EE_WONAPI_WONLOBBYPARSER_H
#define EE_WONAPI_WONLOBBYPARSER_H
#include "rust/ee_won.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONStringCompare.h"
namespace WONAPI {



std::string WStringToString(const std::wstring&); // StringUtil.cpp
// Real non-polymorphic scanner used by the original command/configuration parsers.
// Character accessors 0x2a48c0/0x2a4910 and EOF 0x2a4940 prove this representation. Every scan is ee_won's parser
// (ffi_parse.rs) over the text, its unit size and the cursor; the class is header-only since net round 27.
class StringParser {
public:
    bool StartsWith(const char* theStr) { return ee_won_parser_starts_with(this->Text(), this->Unit(), &this->mPos, theStr); }
    // a matched prefix is not rewound when input follows it
    bool Is(const char* theStr) { return ee_won_parser_is(this->Text(), this->Unit(), &this->mPos, theStr); }
    void SkipNonWhitespace() { ee_won_parser_skip_non_whitespace(this->Text(), this->Unit(), &this->mPos); }
    // comma-separated lists; on a failed element the ones read so far stay
    bool ReadValue(std::list<int>& theList) { return ReadList(this, theList); }
    bool ReadValue(std::list<std::string>& theList) { return ReadStrings(theList); }
    bool ReadValue(std::list<std::wstring>& theList) { return ReadStrings(theList); }
    // "(1, 2), (3)": on a failed element or a missing ')' the groups read so far stay
    bool ReadValue(std::list<std::list<int> >& theList)
    {
        while (this->CheckNextChar('(')) {
            theList.push_back(std::list<int>());
            if (!ReadList(this, theList.back()) || !this->CheckNextChar(')')) return false;
            if (!this->CheckNextChar(',')) break;
        }
        return true;
    }
    template<class T> static void ValueToString(const std::list<T>& theList, std::string& theStr) // ", " between
    {
        for (typename std::list<T>::const_iterator i = theList.begin(); i != theList.end();) {
            StringParser::ValueToString(*i, theStr);
            if (++i != theList.end()) theStr += ", ";
        }
    }
    static void ValueToString(const std::list<std::list<int> >& theList, std::string& theStr) // "(a, b), (c)"
    {
        for (std::list<std::list<int> >::const_iterator i = theList.begin(); i != theList.end();) {
            theStr += "(";
            ValueToString(*i, theStr);
            theStr += ")";
            if (++i != theList.end()) theStr += ", ";
        }
    }
    StringParser() : mPos(0), mWide(false) { mNarrow=""; }
    void Set(const char* theStr) { mNarrow=theStr; this->mPos=0; this->mWide=false; }
    void Set(const wchar_t* theStr) { mWideText=theStr; this->mPos=0; this->mWide=true; }
    const char* GetStr() { return mNarrow; }
    const wchar_t* GetWStr() { return mWideText; }
    int GetPos() { return this->mPos; }
    void SetIsWString(bool isWString) { this->mWide=isWString; }
    bool GetIsWString() { return this->mWide; }
    StringParser(const char* inText) { this->Set(inText); }
    StringParser(const wchar_t* inText) { this->Set(inText); }
    // wide characters above 255 read as 0xff
    unsigned char GetChar(int inOffset) { return ee_won_parser_get_char(this->Text(), this->Unit(), this->mPos, inOffset); } //2a48c0
    wchar_t GetWChar(int inOffset) { return (wchar_t)ee_won_parser_get_wchar(this->Text(), this->Unit(), this->mPos, inOffset); } //2a4910
    void IncrementPos(int inAmount) { this->mPos += inAmount; } //2a4900
    bool EndOfString() { return ee_won_parser_get_wchar(this->Text(), this->Unit(), this->mPos, 0) == 0; } //2a4940
    void SkipWhitespace() { ee_won_parser_skip_whitespace(this->Text(), this->Unit(), &this->mPos); } //2a4970
    // whitespace stays consumed on a mismatch
    bool CheckNextChar(int inChar) { return ee_won_parser_check_next_char(this->Text(), this->Unit(), &this->mPos, inChar); } //2a49b0
    // a bare or quoted (escapes, L"...", "a"+"b") token, trailing whitespace trimmed; always true
    bool ReadString(std::wstring& outString, bool inStopAtWhitespace) //2a4ff0
    {
        outString.erase();
        ee_won_parser_read_wstring(this->Text(), this->Unit(), &this->mPos, inStopAtWhitespace, AppendUnits, &outString);
        return true;
    }
    // decimal or 0x hex with an optional '-'; true when the cursor moved (a lone '-' or '0x' too)
    bool ReadValue(int& outValue) //2a53d0
    { int32_t v = 0; if (!ee_won_parser_read_int(this->Text(), this->Unit(), &this->mPos, &v)) return false; outValue = v; return true; }
    // the SDK's 64-bit read accumulates in an int, as ReadValue(int&)
    bool ReadValue(__int64 &theInt64) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theInt64 = anInt; return aRet; }
    // the sized reads store only on success (the original stored an uninitialised int after a failed read)
    bool ReadValue(unsigned __int64 &theInt64)  { __int64 anInt64; bool aRet = this->ReadValue(anInt64); if (aRet) theInt64 = anInt64; return aRet; }
    bool ReadValue(long &theLong) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theLong = anInt; return aRet; }
    bool ReadValue(unsigned long &theLong) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theLong = anInt; return aRet; }
    bool ReadValue(short &theShort) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theShort = (short)anInt; return aRet; }
    bool ReadValue(unsigned short &theShort) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theShort = (unsigned short)anInt; return aRet; }
    bool ReadValue(char &theByte) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theByte = (char)anInt; return aRet; }
    bool ReadValue(unsigned char &theByte) { int anInt; bool aRet = this->ReadValue(anInt); if (aRet) theByte = (unsigned char)anInt; return aRet; }
    // true/yes/on/y/t/1 or false/no/off/n/f/0; another word is consumed and leaves the value
    bool ReadValue(bool& outValue) { return ee_won_parser_read_bool(this->Text(), this->Unit(), &this->mPos, &outValue); } //2a5550
    bool ReadString(std::string& outString, bool inStopAtWhitespace)
    {
        outString.erase();
        ee_won_parser_read_string(this->Text(), this->Unit(), &this->mPos, inStopAtWhitespace, AppendBytes, &outString);
        return true;
    }
    static void ValueToString(const bool& v, std::string& s) { ee_won_parser_format_bool(v, AppendBytes, &s); } //2a5820 True/False
    static void ValueToString(const int& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); } //2a5a70
    static void ValueToString(const unsigned __int64& v, std::string& s) { ee_won_parser_format_u64(v, AppendBytes, &s); }
    static void ValueToString(const __int64& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const unsigned long& v, std::string& s) { ee_won_parser_format_u64(v, AppendBytes, &s); }
    static void ValueToString(const long& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const short& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const char& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const unsigned char& v, std::string& s) { ee_won_parser_format_u64(v, AppendBytes, &s); }
    static void ValueToString(const unsigned short& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); } //2a5c40
    static void ValueToString(const std::string& v, std::string& s) { s += v; } //2a5c60
    static void ValueToString(const std::wstring& v, std::string& s) { s += WStringToString(v); } //2a5d70
    union { const char* mNarrow; const wchar_t* mWideText; };
    int mPos;
    bool mWide;
private:
    const void* Text() const { return this->mWide ? static_cast<const void*>(this->mWideText) : static_cast<const void*>(this->mNarrow); }
    uint32_t Unit() const { return this->mWide ? (uint32_t)sizeof(wchar_t) : 1u; }
    static void AppendBytes(void* inOut, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inOut)->append(reinterpret_cast<const char*>(inBytes), inLength); }
    static void AppendUnits(void* inOut, const uint32_t* inUnits, size_t inCount)
    { for (size_t i = 0; i < inCount; i++) *static_cast<std::wstring*>(inOut) += (wchar_t)inUnits[i]; }
    template<class T> static bool ReadList(StringParser* inParser, std::list<T>& theList)
    {
        while (true) {
            theList.push_back(T());
            if (!inParser->ReadValue(theList.back())) { theList.pop_back(); return false; }
            if (!inParser->CheckNextChar(',')) return true;
        }
    }
    template<class S> bool ReadStrings(std::list<S>& theList) // a string read never fails
    {
        do { theList.push_back(S()); this->ReadString(theList.back(), false); } while (this->CheckNextChar(','));
        return true;
    }
};

// The parser's state is a Rust object (round 17, rust/crates/ee_won_lobby/src/chatobj.rs EeWonChatParser: the command
// tables, the settings, what a parse leaves); the class keeps the room's client list and the target client.
class ChatCommandParser : public RefCount {
public:
    ChatCommandParser();
    virtual ~ChatCommandParser();         // 0x3559f0; only virtual slot
    void InitCommands();
    void SetClientList(LobbyClientList* inClients, int inMatchThreshold);
    void AddCommand(const GUIString& inCommand, int inId);
    GUIString GetCommandString(int inId);
    const char* GetCommandName(int inId);
    void AddCommands(const std::wstring& inCommands, int inId);
    LobbyClient* FindClient(const GUIString& inName, int* outMatch);
    bool HandleInput(const GUIString& inInput); // with ParseCommand (0x756f40), chatobj.rs's
    void AddHelpEntry(int inId, const GUIString& inCommand, GUIString& outHelp);
    GUIString GetHelp(unsigned short inClientId);

    int GetCommand() const;
    GUIString GetTargetName() const; // the texts are copies (the original returned references to the members)
    int GetPrefix() const;
    LobbyClient* GetTarget() const { return this->mTarget.get(); }
    GUIString GetText() const;
    unsigned long GetDuration() const;
    unsigned long GetRoomFlags() const;
    LobbyClientList* GetClients() const { return this->mClients.get(); }
    int GetError() const;
    bool GetInvitation() const;
    void SetCommand(int inCommand);
    void SetInput(const GUIString& inInput); // the reply target's name
    void SetPrefix(int inPrefix);
    void SetMatchThreshold(int inThreshold);
    void SetAllowColonEmote(bool inAllow);
    void SetText(const GUIString& inText);
    void SetInvitation(bool inInvitation);
private:
    ChatCommandParser(const ChatCommandParser&);
    ChatCommandParser& operator=(const ChatCommandParser&);
    GUIString Text(EeWonChatParserText inText) const;
    SmartPtr<LobbyClientList> mClients;    // +0x08
    SmartPtr<LobbyClient> mTarget;         // +0x38
    EeWonChatParser* mParser;              // the original's +0x0c..+0x50 but these two
};
}
#endif
