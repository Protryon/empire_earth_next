#ifndef EE_WONAPI_WONLOBBYPERSISTENCE_H
#define EE_WONAPI_WONLOBBYPERSISTENCE_H
#include <string>
#include <set>
#include <exception>
#include <map>
#include <list>
#include <stdio.h>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONBlowfish.h"
// WONLobby's saved settings (LobbyPersistentData, 0x1164 bytes; constructor 0x75bcb0), their accessors and the
// file reader / writer they use

namespace WONAPI {
// FileReader: 0x14 bytes (ConfigParser embeds one at 0x18; constructor 0x7b1d20). FileWriter: 4 bytes
// (constructor 0x6a1e10, destructor 0x6a1e20).
class FileReader {
public:
    bool IsOpen() { return this->mFile!=NULL || this->mBuffer!=NULL; }
    FileReader(); // 0x6a0d00
    FileReader(const char* inPath); // 0x6a0d10
    FileReader(const void* theMemDataP, unsigned long theMemDataLen);
    bool OpenText(const char* inPath); // 0x6a0d80 (fopen "r"; provisional name)
    void Open(const void* inData, unsigned long inLength); // 0x6a0dc0
    unsigned long pos(); // 0x6a0e20
    unsigned long length(); // 0x6a0e40
    unsigned long Available() { return this->length()-this->pos(); }
    void SetPos(unsigned long inPos); // 0x6a0e90
    int ReadMaxBytes(void* outData, int inMaxBytes); // 0x6a12d0
    int TryReadByte(); // 0x6a1780
    void SkipBytes(unsigned long inLength); // 0x6a17c0
    void ReadIntoMemory(); // 0x6a1ac0
    int ReadLength(int inFieldSize); // 0x6a1b80
    void ReadRawString(std::string& outString, int inNumChars); // 0x6a1bc0
    void ReadString(std::string& outString, unsigned char inFieldSize); // 0x6a1d90
    void ReadRawWString(std::wstring& theWStr,int theNumChars);
    void ReadWString(std::wstring& theWStr,unsigned char theLengthFieldSize=2);
    void Rewind(); // 0x6a1dc0
    bool EndOfFile(); // 0x6a1de0
private:
    void Init(); // 0x6a0ce0 (zeroes the six fields)
public:
    ~FileReader(); // 0x6a0d30
    bool Open(const char* inPath); // 0x6a0d40
    void Close(); // 0x6a0de0
    void ReadBytes(void* outData, unsigned long inLength); // 0x6a1460
    unsigned char ReadByte(); // 0x6a1b20
    unsigned short ReadShort(); // 0x6a1b40
    long ReadLong(); // 0x6a1b60
    FILE* mFile; // 0x00
    const char* mBuffer; // 0x04 (memory source; delete[] when mOwnBuffer)
    unsigned long mLength; // 0x08
    unsigned long mPosition; // 0x0c
    bool mOwnBuffer; // 0x10
    bool mMemoryMode; // 0x11 (set by the memory Open at 0x6a0dd6, cleared by the file Open)
};
// closes a FILE* when it goes out of scope
struct AutoFile {
    FILE* mFile;
    AutoFile(FILE* file) : mFile(file) {}
    ~AutoFile() { fclose(this->mFile); }
};
class FileWriter {
public:
    FileWriter(); // 0x6a1e10
    unsigned long pos(); // 0x6a2080
    void SetPos(unsigned long inPos); // 0x6a20a0
    void WriteLength(unsigned long inLength, unsigned char inFieldSize); // 0x6a2620
    void WriteRawString(const std::string& inString); // 0x6a2660
    void WriteString(const std::string& inString, unsigned char inFieldSize); // 0x6a2680
    void WriteRawWString(const std::wstring& theWStr);
    void WriteWString(const std::wstring& theWStr,unsigned char theLengthFieldSize=2);
    ~FileWriter(); // 0x6a1e20
    bool Open(const char* inPath, bool inOpenIfReadOnly); // 0x6a1e30 (retries after clearing read-only)
    void Close(); // 0x6a2060
    void WriteBytes(const void* inData, unsigned long inLength); // 0x6a2400
    void WriteByte(unsigned char inValue); // 0x6a25f0
    void WriteShort(unsigned short inValue); // 0x6a2600
    void WriteLong(long inValue); // 0x6a2610
    FILE* mFile; // 0x00
};

// both derive from std::exception (RTTI); only the destructor is overridden
class FileReaderException : public std::exception {
public:
    FileReaderException() : std::exception() {}
    FileReaderException(const std::string& inDescription) { this->mDescription = inDescription; }
    // the destructor is implicit (0x6a10e0)
    std::string mDescription;       // 0x08 (thrown by 0x6a1460)
};
class FileWriterException : public std::exception {
public:
    FileWriterException(const std::string& inDescription) { this->mDescription = inDescription; }
    // the destructor is implicit (0x6a2210)
    std::string mDescription;       // 0x08 (thrown by 0x6a2400)
};
// GUIStringLessNoCase comes from WONLobbyGUI.h through the model header.
// a saved user (0x18 bytes) (provisional member names)
struct LobbyUserInfo {
    LobbyUserInfo() : mUserId(0), mValue04(0), mValue08(0), mLastUseTime(0) {} // four zero stores (0x75f867..0x75f870)
    long mUserId; // 0x00 (read as a short, zero-extended)
    long mValue04; // 0x04 (read as a long)
    long mValue08; // 0x08 (read as a long)
    long mLastUseTime; // 0x0c (read as a long; LoginCtrl 0x75c910 stores time(NULL))
    GUIString mUserName; // 0x10
    GUIString mPassword; // 0x14
};
typedef std::map<GUIString, LobbyUserInfo, GUIStringLessNoCase> LobbyUserInfoMap;
const LobbyUserInfoMap& LobbyFn35CE60(); // 0x75ce60 (an empty function-local static map without lobby data)
GUIString LobbyGetUserPassword(const GUIString& inUserName);
LobbyUserInfo* LobbyFindUser(const GUIString& inUserName);
LobbyUserInfo* LobbyAddUser(const GUIString& inUserName, const GUIString& inPassword, bool inExistingOnly);

// the saved options' getters and setters: LobbyPersistentData offsets, defaults without lobby data
// (provisional names)
const GUIString& LobbyGetTabSelect(); // 0x58, or gEmptyGUIString
bool LobbyGetFilterDirtyWords(); // 0x54; false
bool LobbyGetDoAutoAFK(); // 0x55
bool LobbyGetCrossPromotion(); // 0x56
bool LobbyGetLobbySoundEffects(); // 0x5c
bool LobbyGetChatSoundEffects(); // 0x5e
bool LobbyGetStagingSoundEffects(); // 0x5f
bool LobbyGetLobbyMusic(); // 0x5d
bool LobbyGetShowConfirmations(); // 0x60
bool LobbyGetAutoPingGames(); // 0x61
bool LobbyGetShowGamesInProgress(); // 0x62
bool LobbyGetAnonymousToFriends(); // 0x63
unsigned short LobbyGetConnectionSpeed(); // 0x64 (0x7bee7b masks it to 16 bits); 50
unsigned short LobbyGetLanBroadcastPort(); // 0x66; 8585
long LobbyGetNetworkAdapter(); // 0x70 (a numeric IPv4 address, copied from 0x965f40)
void LobbySetLanBroadcastPort(unsigned short); // 0x66
void LobbySetFilterDirtyWords(bool); // 0x54 (sets the user dirty flag 0x7d only on a change)
void LobbySetDoAutoAFK(bool); // 0x55
void LobbySetCrossPromotion(bool); // 0x56
void LobbySetTabSelect(const GUIString&); // 0x58
void LobbySetLobbySoundEffects(bool); // 0x5c (also recomputes the global dirty flag 0x7c from 0x6c)
void LobbySetChatSoundEffects(bool); // 0x5e
void LobbySetStagingSoundEffects(bool); // 0x5f
void LobbySetLobbyMusic(bool); // 0x5d (also recomputes 0x7c from 0x6d)
void LobbySetShowConfirmations(bool); // 0x60
void LobbySetAutoPingGames(bool); // 0x61
void LobbySetShowGamesInProgress(bool); // 0x62
void LobbySetAnonymousToFriends(bool); // 0x63
void LobbySetConnectionSpeed(unsigned short); // 0x64
void LobbySaveNetworkAdapter(); // copies 0x965f40 to 0x70 and sets the global dirty flag 0x7c
void LobbySaveGlobalPrefs(bool); // writes the global preferences (0x75faf0) when forced or 0x7c is set
void LobbySaveUserPrefs(bool); // writes the user's preferences (0x760770) when forced or 0x7d is set
void LobbyRestoreDefaults(); // RestoreDefaults 0x75c720, then sets 0x7d and recomputes 0x7c
const GUIString& LobbyGetLastServerName(); // 0x75dc40 (0x74)
const GUIString& LobbyGetLastRoomName(); // 0x75dc60 (0x78)

// 0x75d270 returns the friends map (its 0x30), or the static empty map 0x966300 without lobby data
typedef std::map<unsigned long, SmartPtr<LobbyFriend> > LobbyFriendMap;
const LobbyFriendMap& LobbyFn35D270();
LobbyFriend* LobbyFindFriend(unsigned long inUserId); // 0x75d3f0 (not owned)
void LobbyRemoveFriend(unsigned long inUserId);
void LobbySetLastRoom(const GUIString& inServerName, const GUIString& inGroupName);
void LobbySetLanName(const GUIString& inName);
void LobbyAddDirectConnect(const IPAddr& inAddress, const GUIString& inName);
void LobbyRemoveDirectConnect(const IPAddr& inAddress);

// 0x75cfc0 returns the ignored names (its 0x18), or the static empty set 0x966320 without lobby data
typedef std::set<GUIString, GUIStringLessNoCase> LobbyIgnoredNameSet;
const LobbyIgnoredNameSet& LobbyFn35CFC0();
// 0x75d460: adds a friend (a new LobbyFriend, 0x50 bytes)
LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inUserId);

typedef std::map<GUIString, long, GUIStringLessNoCase> LobbyLanNameInfoMap;
typedef std::map<IPAddr, GUIString> LobbyDirectConnectMap;
const GUIString& LobbyGetLanName(); // 0x75d640 (0x68, or the empty string)
const LobbyLanNameInfoMap& LobbyFn35D660(); // 0x75d660 (0x114c, or the static 0x966388)
void LobbyRemoveLanName(); // 0x75d770 (case-insensitive compare 0x728030)
void LobbyResetFilters(); // 0x75df00
const LobbyDirectConnectMap& LobbyFn35D8E0(); // 0x75d8e0 (0x1158, or the static 0x966330)
void LobbySetCurrentUser(LobbyUserInfo* inUser); // 0x75cf90 (then reads the user)
// a set of strings with a lookup index (0x18 bytes; constructor 0x75dd20; Add 0x75baa0, Erase 0x75bc20)
class LobbyOptionSet {
public:
    typedef std::list<GUIString> StringList;
    typedef std::map<GUIString, StringList::iterator> StringMap;
    void Add(const GUIString& inValue); // 0x75baa0
    bool Erase(const GUIString& inValue); // 0x75bc20
    void clear(); // as the SDK's ServerListFilterSet::clear
    StringMap mIndex; // 0x00
    StringList mValues; // 0x0c
};

// the lobby's saved settings: users, friends, ignored names, options and LAN data (0x1164 bytes)

struct LobbyReconnectInfo {
    long mLastUseTime;
    int mReconnectId;
    LobbyReconnectInfo() : mLastUseTime(0) {} // the original leaves mReconnectId uninitialized
};
class LobbyPersistentData : public RefCount {
public:
    LobbyPersistentData(); // 0x75bcb0 (leaves mCurrentUserId unset before the reset)
    // the destructor is implicit (0x75c2c0; it does not store its own vtable)
    std::string GetUserFileName(long inUserId); // 0x75e6e0 (provisional name)
    long FindUnusedUserId();                  // 0x75e770 (provisional name)
    long ReadFileHeader(FileReader& inReader);  // 0x75f6a0 (provisional name)
    void WriteFileHeader(FileWriter& outWriter, long inVersion);  // 0x75f750
    void PruneUsers(); // 0x75eb30 (drops the oldest users)
    GUIString ReadByteString(FileReader& inReader); // 0x75ee80 (byte characters)
    void WriteByteString(FileWriter& outWriter, const GUIString& inText); // 0x75f3d0
    GUIString ReadWideString(FileReader& inReader); // 0x75f000 (16-bit characters)
    GUIString ReadEncryptedString(FileReader& inReader); // 0x75f190
    void WriteWideString(FileWriter& outWriter, const GUIString& inText); // 0x75f4e0
    void WriteEncryptedString(FileWriter& outWriter, const GUIString& inText); // 0x75f530
    GUIString GetDefaultLanName(); // 0x75c520 (uses a CRC)
    void RestoreDefaults(); // 0x75c720
    void LoadGlobalPrefs(); // 0x75f7b0
    void SaveGlobalPrefs(); // 0x75faf0
    void LoadUserPrefs(); // 0x75fc70
    void SaveUserPrefs(); // 0x760770
    void ReadLanPrefs(FileReader& inReader); // 0x760b10
    void WriteLanPrefs(FileWriter& outWriter); // 0x760da0
    void PruneLanNames(); // 0x75e7e0 (keeps the 50 most recent LAN names)
    void SelectRecentLanName(); // 0x75ea40
    std::set<long> mUserIds;                  // 0x08 (the insert at 0x75fa52 gives the type)
long mCurrentUserId; // 0x14
std::set<GUIString, GUIStringLessNoCase> mIgnoredNames; // 0x18
std::set<unsigned long> mBlockedCDKeyIds; // 0x24 (loaded and saved, but nothing in the lobby fills it)
std::map<unsigned long, SmartPtr<LobbyFriend> > mFriends; // 0x30
std::map<GUIString, LobbyOptionSet, GUIStringLessNoCase> mDefaultGameOptions; // 0x3c
std::map<GUIString, LobbyOptionSet, GUIStringLessNoCase> mGameOptions; // 0x48
bool mFilterDirtyWords, mDoAutoAFK, mCrossPromotion; // 0x54, 0x55, 0x56
GUIString mTabSelect; // 0x58
bool mLobbySoundEffects, mLobbyMusic, mChatSoundEffects, mStagingSoundEffects, mShowConfirmations, mAutoPingGames, mShowGamesInProgress, mAnonymousToFriends;
unsigned short mConnectionSpeed, mLanBroadcastPort; // 0x64, 0x66
GUIString mLanName; // 0x68
bool mSavedLobbySoundEffects, mSavedLobbyMusic; // 0x6c, 0x6d
long mNetworkAdapter; // 0x70 (a numeric IPv4 address) (provisional name)
GUIString mLastServerName, mLastRoomName; // 0x74, 0x78
bool mGlobalDirty, mUserDirty; // 0x7c, 0x7d

LobbyUserInfoMap mUsers; // 0x80
std::map<IPAddr, LobbyReconnectInfo> mReconnectInfoMap; // 0x8c
Blowfish mEncryptKey; // 0x98..0x114c (0x10b4 bytes)
std::map<GUIString, long, GUIStringLessNoCase> mLanNameInfoMap; // 0x114c
std::map<IPAddr, GUIString> mDirectConnectMap; // 0x1158 (0x1164 bytes in all)

};
LobbyPersistentData* GetLobbyPersistentData(); // 0x75c900 (the lobby's instance; not owned)
int LobbyGetReconnectId(const IPAddr& inAddress); // 0x75db00 (0 when missing)
void LobbySetReconnectId(const IPAddr& inAddress, int inReconnectId); // 0x75d940 (then saves)
// the filter sets are LobbyOptionSet records (the SDK calls them ServerListFilterSet)
const LobbyOptionSet& LobbyFn35DC80(const GUIString& inColumn);
void LobbyAddFilterValue(const GUIString& inColumn, const GUIString& inValue);
void LobbyRemoveFilterValue(const GUIString& inColumn, const GUIString& inValue);
}

#endif
