// WONLobby command parser: original allocation 0x54, RTTI ChatCommandParser : RefCount.
// Names are provisional by behavior; field representations follow constructors, callers and EH cleanup.
#ifndef EE_WONAPI_WONLOBBYPARSER_H
#define EE_WONAPI_WONLOBBYPARSER_H
#include "ee/wonapi/WONDriver.h" // EE_WON_DRV
#include "rust/won_lobby/chatobj.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_net/chat.h"
#include "rust/won_net/ffi_parse.h"
namespace WONAPI {



// Real non-polymorphic scanner used by the original command/configuration parsers.
// Character accessors 0x2a48c0/0x2a4910 and EOF 0x2a4940 prove this representation. Every scan is ee_won's parser
// (ffi_parse.rs) over the text, its unit size and the cursor; the class is header-only since net round 27. The list
// reads and writes have no game caller (the hash file list is lobbyprv.rs's): R6 WON lobby
// and network round 38 moved them to tests/won/won_parser_lists.inl.
class StringParser {
public:
#include EE_WON_DRV(Parser_StringParser) // the drivers' members
    // a matched prefix is not rewound when input follows it
    bool Is(const char* theStr) { return ee_won_parser_is(this->Text(), this->Unit(), &this->mPos, theStr); }
    StringParser() : mPos(0), mWide(false) { mNarrow=""; }
    void Set(const char* theStr) { mNarrow=theStr; this->mPos=0; this->mWide=false; }
    void Set(const wchar_t* theStr) { mWideText=theStr; this->mPos=0; this->mWide=true; }
    const char* GetStr() { return mNarrow; }
    StringParser(const char* inText) { this->Set(inText); }
    StringParser(const wchar_t* inText) { this->Set(inText); }
    bool EndOfString() { return ee_won_parser_get_wchar(this->Text(), this->Unit(), this->mPos, 0) == 0; } //2a4940
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
#ifdef EE_WON_STRINGPARSER_DRIVER
#include "won_parser_lists.inl" // the list reads and writes only the drivers call (tests/won)
#endif
};

// ChatCommandParser (original allocation 0x54, vtable 0x851a9c; ChatCommandParser.cpp, 0x755750..0x757d67): the whole
// class is a Rust object since R6 WONLobby round 17 (rust/crates/ee_won_lobby/src/chatobj.rs EeWonChatParser: the
// command tables, the settings, what a parse leaves, the room's client list and the target client, InitCommands over
// the resource strings, the name look-ups and the help); these are the game's forwards (its reads are lobbytext.rs's,
// the drivers' forwards tests/won/drv/won_drv_Parser_ChatCommandParser.inl). Kept from the original: a
// one-character rest of line reads as empty, a name prefix shared by two clients is ambiguous, a duration needs a unit
// letter (D/H/M/S), and INFINITE keeps the previous duration.
class ChatCommandParser : public RefCount {
public:
    ChatCommandParser() : mParser(ee_won_chatparser_new()) {}
    virtual ~ChatCommandParser() { ee_won_chatparser_free(this->mParser); } // 0x3559f0; only virtual slot
    void InitCommands() { ee_won_chatparser_init_commands(this->mParser); }
    // the list held and the room flags (the help's)
    void SetClientList(LobbyClientList* inClients, int inRoomFlags)
    { ee_won_chatparser_set_client_list(this->mParser, inClients, inRoomFlags); }
#include EE_WON_DRV(Parser_ChatCommandParser) // the drivers' members (the commands, the parse and its results)
public:
    EeWonChatParser* mParser; // read in place by ee_won_lobby lobbytext.rs (ParserCommandText; R6 WON GUI round 40)
};
}
#endif
