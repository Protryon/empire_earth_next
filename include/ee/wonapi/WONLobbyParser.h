// WONLobby command parser: original allocation 0x54, RTTI ChatCommandParser : RefCount.
// Names are provisional by behavior; field representations follow constructors, callers and EH cleanup.
#ifndef EE_WONAPI_WONLOBBYPARSER_H
#define EE_WONAPI_WONLOBBYPARSER_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONStringCompare.h"
namespace WONAPI {



// Real non-polymorphic scanner used by the original command/configuration parsers.
// Character accessors 0x2a48c0/0x2a4910 and EOF 0x2a4940 prove this representation.
class StringParser {
public:
    bool StartsWith(const char* theStr);
    bool Is(const char* theStr);
    void SkipNonWhitespace();
    bool ReadValue(std::string& theStr,bool stopAtWhitespace=false);
    bool ReadValue(std::wstring& theStr,bool stopAtWhitespace=false);
    bool ReadValue(std::list<bool>& theList);
    bool ReadValue(std::list<int>& theList);
    bool ReadValue(std::list<std::string>& theList);
    bool ReadValue(std::list<std::wstring>& theList);
    bool ReadValue(std::list<std::list<bool> >& theList);
    bool ReadValue(std::list<std::list<int> >& theList);
    bool ReadValue(std::list<std::list<std::string> >& theList);
    bool ReadValue(std::list<std::list<std::wstring> >& theList);
    static void ValueToString(const std::list<bool>& theVal,std::string& theStr);
    static void ValueToString(const std::list<int>& theVal,std::string& theStr);
    static void ValueToString(const std::list<std::wstring>& theVal,std::string& theStr);
    static void ValueToString(const std::list<std::list<bool> >& theVal,std::string& theStr);
    static void ValueToString(const std::list<std::list<int> >& theVal,std::string& theStr);
    static void ValueToString(const std::list<std::list<std::string> >& theVal,std::string& theStr);
    static void ValueToString(const std::list<std::list<std::wstring> >& theVal,std::string& theStr);
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
    unsigned char GetChar(int inOffset); //2a48c0, wide >=256 clamps0xff
    wchar_t GetWChar(int inOffset); //2a4910
    void IncrementPos(int inAmount); //2a4900
    bool EndOfString(); //2a4940
    void SkipWhitespace(); //2a4970
    bool CheckNextChar(int inChar); //2a49b0
    bool ReadString(std::wstring& outString, bool inStopAtWhitespace); //2a4ff0
    bool ReadValue(int& outValue); //2a53d0
    bool ReadValue(__int64 &theInt64);
    bool ReadValue(unsigned __int64 &theInt64)  { __int64 anInt64; bool aRet = this->ReadValue(anInt64); theInt64 = anInt64; return aRet; }
    bool ReadValue(long &theLong) { int anInt; bool aRet = this->ReadValue(anInt); theLong = anInt; return aRet; }
    bool ReadValue(unsigned long &theLong) { int anInt; bool aRet = this->ReadValue(anInt); theLong = anInt; return aRet; }
    bool ReadValue(short &theShort) { int anInt; bool aRet = this->ReadValue(anInt); theShort = (short)anInt; return aRet; }
    bool ReadValue(unsigned short &theShort) { int anInt; bool aRet = this->ReadValue(anInt); theShort = (unsigned short)anInt; return aRet; }
    bool ReadValue(char &theByte) { int anInt; bool aRet = this->ReadValue(anInt); theByte = (char)anInt; return aRet; }
    bool ReadValue(unsigned char &theByte) { int anInt; bool aRet = this->ReadValue(anInt); theByte = (unsigned char)anInt; return aRet; }
    bool ReadValue(bool& outValue); //2a5550
    bool ReadString(std::string& outString, bool inFlag); // 0x2a49f0
    // Genuine nominal static formatting overloads; call-only original-owned implementations.
    static void ValueToString(const bool&,std::string&); //2a5820
    static void ValueToString(const int&,std::string&); //2a5a70 call-only scalar formatter
    static void ValueToString(const unsigned __int64&,std::string&);
    static void ValueToString(const __int64&,std::string&);
    static void ValueToString(const unsigned long&,std::string&);
    static void ValueToString(const long&,std::string&);
    static void ValueToString(const short&,std::string&);
    static void ValueToString(const char&,std::string&);
    static void ValueToString(const unsigned char&,std::string&);
    static void ValueToString(const unsigned short&,std::string&); //2a5c40
    static void ValueToString(const std::string&,std::string&); //2a5c60
    static void ValueToString(const std::wstring&,std::string&); //2a5d70
    static void ValueToString(const std::list<std::string>&,std::string&); //2a5e70
    union { const char* mNarrow; const wchar_t* mWideText; };
    int mPos;
    bool mWide;
};

class ChatCommandParser : public RefCount {
public:
    typedef std::map<std::wstring, int, WStringLessNoCase> CommandMap;
    ChatCommandParser();                  // 0x355750
    virtual ~ChatCommandParser();         // 0x3559f0; only virtual slot
    void InitCommands();
    void SetClientList(LobbyClientList* inClients, int inMatchThreshold);
    void AddCommand(const GUIString& inCommand, int inId);
    GUIString GetCommandString(int inId);
    const char* GetCommandName(int inId);
    void AddCommands(const std::wstring& inCommands, int inId);
    LobbyClient* FindClient(const GUIString& inName, int* outMatch);
    bool ParseCommand(const GUIString& inInput);
    bool HandleInput(const GUIString& inInput);
    void AddHelpEntry(int inId, const GUIString& inCommand, GUIString& outHelp);
    GUIString GetHelp(unsigned short inClientId);

    int GetCommand() const { return this->mCommand; }
    const GUIString& GetTargetName() const { return this->mTargetName; }
    int GetPrefix() const { return this->mPrefix; }
    LobbyClient* GetTarget() const { return this->mTarget.get(); }
    const GUIString& GetText() const { return this->mText; }
    unsigned long GetDuration() const { return this->mDuration; }
    unsigned long GetRoomFlags() const { return this->mRoomFlags; }
    LobbyClientList* GetClients() const { return this->mClients.get(); }
    SmartPtr<LobbyClientList> mClients;    // +0x08
    GUIString mInput;                     // +0x0c
    CommandMap mCommands;                 // +0x10
    std::map<int, std::wstring> mCommandStrings; // +0x1c
    int mMatchThreshold;                  // +0x28
    bool mB2C;
    bool mAllowColonEmote;                // +0x2d
    int mError;                           // +0x30
    int mCommand;                         // +0x34
    SmartPtr<LobbyClient> mTarget;         // +0x38
    GUIString mTargetName;                // +0x3c
    GUIString mText;                      // +0x40
    bool mInvitation;                     // +0x44
    unsigned long mDuration;              // +0x48
    int mPrefix;                          // +0x4c
    unsigned long mRoomFlags;             // +0x50 (GetHelp 0x7579c0 shifts it with shr)
};
}
#endif
