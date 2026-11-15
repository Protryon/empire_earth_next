#ifndef EE_WONAPI_WONLOBBYPERSISTENCE_H
#define EE_WONAPI_WONLOBBYPERSISTENCE_H
#include "rust/ee_won.h"
#include <string>
#include <set>
#include <exception>
#include <map>
#include <list>
#include <stdint.h>
#include <stdio.h>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONBlowfish.h"
// WONLobby's saved settings (LobbyPersistentData, 0x1164 bytes; constructor 0x75bcb0), their accessors and the
// file reader / writer they use

namespace WONAPI {
// FileReader: 0x14 bytes (ConfigParser embeds one at 0x18; constructor 0x7b1d20). FileWriter: 4 bytes
// (constructor 0x6a1e10, destructor 0x6a1e20). Both are ee_won's (rust/crates/ee_won/src/file.rs): the state is
// its #[repr(C)] core, these methods forward and throw the exceptions its statuses name (FileReader.cpp,
// FileReaderWideStrings.cpp and FileWriter.cpp are deleted: net round 27).
class FileReaderException;
class FileWriterException;
inline void FileReaderCheck(int inStatus);
inline void FileWriterCheck(int inStatus);
class FileReader {
public:
    bool IsOpen() { return this->mCore.file!=NULL || this->mCore.buffer!=NULL; }
    FileReader() { ee_won_file_reader_init(&this->mCore); }
    FileReader(const char* inPath) { ee_won_file_reader_init(&this->mCore); this->Open(inPath); }
    FileReader(const void* inData, unsigned long inLength) { ee_won_file_reader_init(&this->mCore); this->Open(inData, inLength); }
    bool OpenText(const char* inPath) { return ee_won_file_reader_open_text(&this->mCore, inPath); } // fopen "r"
    void Open(const void* inData, unsigned long inLength) { ee_won_file_reader_open_memory(&this->mCore, inData, inLength); }
    unsigned long pos() { return ee_won_file_reader_pos(&this->mCore); }
    unsigned long length() // measures the file once by seeking to its end
    { int32_t s = 0; unsigned long n = ee_won_file_reader_length(&this->mCore, &s); FileReaderCheck(s); return n; }
    unsigned long Available() { return this->length()-this->pos(); }
    void SetPos(unsigned long inPos) { FileReaderCheck(ee_won_file_reader_set_pos(&this->mCore, inPos)); }
    int ReadMaxBytes(void* outData, int inMaxBytes)
    { int32_t s = 0; int n = ee_won_file_reader_read_max(&this->mCore, outData, inMaxBytes, &s); FileReaderCheck(s); return n; }
    int TryReadByte() { return ee_won_file_reader_try_read_byte(&this->mCore); } // -1 at the end
    void SkipBytes(unsigned long inLength) { FileReaderCheck(ee_won_file_reader_skip(&this->mCore, inLength)); }
    void ReadIntoMemory() { FileReaderCheck(ee_won_file_reader_read_into_memory(&this->mCore)); }
    int ReadLength(int inFieldSize) // 1, 2 or 4 bytes; 0 for any other size
    { int v = 0; FileReaderCheck(ee_won_file_reader_read_length(&this->mCore, inFieldSize, &v)); return v; }
    void ReadRawString(std::string& outString, int inNumChars)
    {
        outString.erase();
        outString.reserve(inNumChars);
        for (int i = 0; i < inNumChars; ++i) outString += (char)this->ReadByte();
    }
    void ReadString(std::string& outString, unsigned char inFieldSize) { this->ReadRawString(outString, this->ReadLength(inFieldSize)); }
    void ReadRawWString(std::wstring& outString, int inNumChars) // UTF-16 units, one by one
    {
        outString.erase();
        outString.reserve(inNumChars);
        for (int i = 0; i < inNumChars; i++) WideAppendUtf16Unit(outString, this->ReadShort());
    }
    void ReadWString(std::wstring& outString, unsigned char inFieldSize = 2) { this->ReadRawWString(outString, this->ReadLength(inFieldSize)); }
    void Rewind() { ee_won_file_reader_rewind(&this->mCore); }
    bool EndOfFile() { return ee_won_file_reader_end_of_file(&this->mCore); } // also when nothing is open
    ~FileReader() { ee_won_file_reader_close(&this->mCore); }
    bool Open(const char* inPath) { return ee_won_file_reader_open(&this->mCore, inPath); }
    void Close() { ee_won_file_reader_close(&this->mCore); }
    void ReadBytes(void* outData, unsigned long inLength) { FileReaderCheck(ee_won_file_reader_read_bytes(&this->mCore, outData, inLength)); }
    unsigned char ReadByte() { unsigned char v; this->ReadBytes(&v, 1); return v; }
    unsigned short ReadShort() { unsigned short v; this->ReadBytes(&v, 2); return v; }
    int32_t ReadLong() { int32_t v; this->ReadBytes(&v, 4); return v; } // 32-bit on every OS
    // the file (0x00), the memory source (0x04; freed when owned), length (0x08), position (0x0c), owned (0x10)
    // and memory mode (0x11: set by the memory Open at 0x6a0dd6 and by length(), cleared by the file Open)
    EeWonFileReader mCore;
private:
    FileReader(const FileReader&);
    FileReader& operator=(const FileReader&);
};
// closes a FILE* when it goes out of scope
struct AutoFile {
    FILE* mFile;
    AutoFile(FILE* file) : mFile(file) {}
    ~AutoFile() { fclose(this->mFile); }
};
class FileWriter {
public:
    FileWriter() { ee_won_file_writer_init(&this->mCore); }
    unsigned long pos() { return ee_won_file_writer_pos(&this->mCore); } // 0 with no file
    void SetPos(unsigned long inPos) { FileWriterCheck(ee_won_file_writer_set_pos(&this->mCore, inPos)); }
    void WriteLength(unsigned long inLength, unsigned char inFieldSize) // 1, 2 or 4 bytes; nothing for another size
    { FileWriterCheck(ee_won_file_writer_write_length(&this->mCore, inLength, inFieldSize)); }
    void WriteRawString(const std::string& inString) { this->WriteBytes(inString.c_str(), inString.length()); }
    void WriteString(const std::string& inString, unsigned char inFieldSize)
    { this->WriteLength(inString.length(), inFieldSize); this->WriteRawString(inString); }
    void WriteRawWString(const std::wstring& inString)
    { std::string b; WideAppendUtf16LE(b, inString); this->WriteBytes(b.data(), b.length()); }
    void WriteWString(const std::wstring& inString, unsigned char inFieldSize = 2)
    { this->WriteLength(WideUtf16Length(inString), inFieldSize); this->WriteRawWString(inString); }
    ~FileWriter() { this->Close(); }
    bool Open(const char* inPath, bool inOpenIfReadOnly) // retries after clearing the read-only flag when asked
    {
        if (ee_won_file_writer_open(&this->mCore, inPath)) return true;
        if (inPath[0] == 0 || !inOpenIfReadOnly) return false;
        WONFile(inPath).SetReadOnly(false);
        return ee_won_file_writer_open(&this->mCore, inPath);
    }
    void Close() { ee_won_file_writer_close(&this->mCore); }
    void WriteBytes(const void* inData, unsigned long inLength) { FileWriterCheck(ee_won_file_writer_write_bytes(&this->mCore, inData, inLength)); }
    void WriteByte(unsigned char inValue) { this->WriteBytes(&inValue, 1); }
    void WriteShort(unsigned short inValue) { this->WriteBytes(&inValue, 2); }
    void WriteLong(int32_t inValue) { this->WriteBytes(&inValue, 4); } // 32-bit on every OS
    EeWonFileWriter mCore; // the file (0x00)
private:
    FileWriter(const FileWriter&);
    FileWriter& operator=(const FileWriter&);
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
inline void FileReaderCheck(int inStatus)
{ if (inStatus != 0) throw FileReaderException(ee_won_file_status_text(inStatus)); }
inline void FileWriterCheck(int inStatus)
{ if (inStatus != 0) throw FileWriterException(ee_won_file_status_text(inStatus)); }
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
// the saved settings' containers are Rust objects (round 17, WONLobbyMap.h; lobbymap.rs): the Lobby* functions that
// handed out references to the C++ maps (0x75ce60, 0x75d270, 0x75cfc0, 0x75d660, 0x75d8e0, 0x75dc80) are counts and
// positions now, in the maps' order, empty without lobby data
typedef LobbyOwnedMap<GUIString, LobbyUserInfo> LobbyUserInfoMap; // GUIStringLessNoCase
size_t LobbyGetUserCount(); // the saved login users (0x75ce60's map), by name
GUIString LobbyGetUserNameAt(size_t inPos); // the map's key
long LobbyGetUserLastUseAt(size_t inPos);
GUIString LobbyGetUserPassword(const GUIString& inUserName);
LobbyUserInfo* LobbyFindUser(const GUIString& inUserName);
LobbyUserInfo* LobbyAddUser(const GUIString& inUserName, const GUIString& inPassword, bool inExistingOnly);

// the saved options' getters and setters: LobbyPersistentData offsets, defaults without lobby data
// (provisional names)
GUIString LobbyGetTabSelect(); // 0x58, or gEmptyGUIString
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
GUIString LobbyGetLastServerName(); // 0x75dc40 (0x74)
GUIString LobbyGetLastRoomName(); // 0x75dc60 (0x78)

// 0x75d270 returns the friends map (its 0x30), or the static empty map 0x966300 without lobby data
typedef LobbyRefMap<unsigned long, LobbyFriend> LobbyFriendMap;
size_t LobbyGetFriendCount(); // the friends (0x75d270's map), by user id
LobbyFriend* LobbyGetFriendAt(size_t inPos); // not owned
LobbyFriend* LobbyFindFriend(unsigned long inUserId); // 0x75d3f0 (not owned)
void LobbyRemoveFriend(unsigned long inUserId);
void LobbySetLastRoom(const GUIString& inServerName, const GUIString& inGroupName);
void LobbySetLanName(const GUIString& inName);
void LobbyAddDirectConnect(const IPAddr& inAddress, const GUIString& inName);
void LobbyRemoveDirectConnect(const IPAddr& inAddress);

// 0x75cfc0 returns the ignored names (its 0x18), or the static empty set 0x966320 without lobby data
typedef LobbyKeyedMap<GUIString> LobbyIgnoredNameSet; // GUIStringLessNoCase
size_t LobbyGetIgnoredNameCount(); // the ignored names (0x75cfc0's set)
GUIString LobbyGetIgnoredNameAt(size_t inPos);
// 0x75d460: adds a friend (a new LobbyFriend, 0x50 bytes)
LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inUserId);

typedef LobbyKeyedMap<GUIString> LobbyLanNameInfoMap; // GUIStringLessNoCase; the number: the last use time
typedef LobbyKeyedMap<IPAddr> LobbyDirectConnectMap; // the text: the name
typedef LobbyKeyedMap<IPAddr> LobbyReconnectInfoMap; // the numbers: the last use time, the reconnect id
GUIString LobbyGetLanName(); // 0x75d640 (0x68, or the empty string)
size_t LobbyGetLanNameCount(); // the recent LAN names (0x75d660's map)
GUIString LobbyGetLanNameAt(size_t inPos);
void LobbyRemoveLanName(); // 0x75d770 (case-insensitive compare 0x728030)
void LobbyResetFilters();
size_t LobbyGetDirectConnectCount(); // the saved direct connects (0x75d8e0's map), by address
IPAddr LobbyGetDirectConnectAddressAt(size_t inPos);
GUIString LobbyGetDirectConnectNameAt(size_t inPos);
void LobbySetCurrentUser(LobbyUserInfo* inUser); // 0x75cf90 (then reads the user)
// a set of strings in insertion order (0x18 bytes; constructor 0x75dd20; Add 0x75baa0, Erase 0x75bc20): the original's
// list with an index of list iterators is one Rust container in insertion order, unique by GUIString::Compare. A
// copy is a copy (the original's memberwise copy kept the source's list iterators in its index: undefined behaviour
// at the copy's first Erase)
class LobbyOptionSet {
public:
    LobbyOptionSet() : mValues(EeWonLobbyKeyKind::InOrder, EeWonLobbyMapHooks()) {}
    void Add(const GUIString& inValue);
    bool Erase(const GUIString& inValue);
    void clear(); // as the SDK's ServerListFilterSet::clear
    size_t size() const { return this->mValues.size(); }
    bool empty() const { return this->mValues.empty(); }
    GUIString ValueAt(size_t inPos) const { return this->mValues.KeyAt(inPos); } // in insertion order
private:
    LobbyKeyedMap<GUIString> mValues;
};

// the lobby's saved settings: users, friends, ignored names, options and LAN data (0x1164 bytes)

struct LobbyReconnectInfo {
    long mLastUseTime;
    int mReconnectId;
    LobbyReconnectInfo() : mLastUseTime(0) {} // the original leaves mReconnectId uninitialized
};
class LobbyPersistentData : public RefCount {
public:
    LobbyPersistentData(); // 0x75bcb0 (the original left mCurrentUserId unset before the reset; 0 here)
    ~LobbyPersistentData(); // the original's was implicit (0x75c2c0); frees the scalar state's object
    std::string GetUserFileName(long inUserId);
    long FindUnusedUserId();
    void PruneUsers(); // 0x75eb30 (drops the oldest users)
    // the files' layouts are ee_won's (persist.rs); the password's encryption stays here
    GUIString DecryptString(const uint8_t* inData, size_t inLength); // 0x75f190's decryption
    ByteBufferPtr EncryptString(const GUIString& inText); // 0x75f530's encryption (NULL when it fails)
    GUIString GetDefaultLanName(); // 0x75c520 (uses a CRC)
    void RestoreDefaults();
    void LoadGlobalPrefs();
    void SaveGlobalPrefs();
    void LoadUserPrefs();
    void SaveUserPrefs();
    void PruneLanNames(); // 0x75e7e0 (keeps the 50 most recent LAN names)
    void SelectRecentLanName();

    // the members behind accessors (round 16; the Lobby* functions read them through these). The scalar state is a
    // Rust object (rust/crates/ee_won/src/prefsobj.rs EeWonLobbyPrefs: the current user, the flags, the connection
    // speed, the LAN broadcast port, the network adapter and the four texts, with the setters' rules); the Flag
    // values are EeWonPrefsFlag's
    enum Flag { kFilterDirtyWords, kDoAutoAFK, kCrossPromotion, kLobbySoundEffects, kLobbyMusic, kChatSoundEffects,
                kStagingSoundEffects, kShowConfirmations, kAutoPingGames, kShowGamesInProgress, kAnonymousToFriends,
                kSavedLobbySoundEffects, kSavedLobbyMusic, kGlobalDirty, kUserDirty };
    bool GetFlag(Flag inFlag) const;
    void SetFlag(Flag inFlag, bool inOn);
    void ChangeFlag(Flag inFlag, bool inOn); // the Lobby setters' rule (prefsobj.rs change_flag)
    long GetCurrentUserId() const;
    void SetCurrentUserId(long inId);
    unsigned short GetConnectionSpeed() const;
    void SetConnectionSpeed(unsigned short inSpeed);
    void ChangeConnectionSpeed(unsigned short inSpeed); // a new value marks the user's settings changed
    unsigned short GetLanBroadcastPort() const;
    void SetLanBroadcastPort(unsigned short inPort);
    void ChangeLanBroadcastPort(unsigned short inPort);
    long GetNetworkAdapter() const;
    void SetNetworkAdapter(long inAdapter);
    void SaveNetworkAdapter(long inLocalHost); // LobbySaveNetworkAdapter's rule
    GUIString GetTabSelect() const; // the texts are copies (the original returned references to the members)
    void SetTabSelect(const GUIString& inText);
    void ChangeTabSelect(const GUIString& inText); // a new tab marks the user's settings changed
    GUIString GetLanName() const;
    void SetLanName(const GUIString& inText);
    GUIString GetLastServerName() const;
    void SetLastServerName(const GUIString& inText);
    GUIString GetLastRoomName() const;
    void SetLastRoomName(const GUIString& inText);
    void SetLastRoom(const GUIString& inServer, const GUIString& inRoom); // LobbySetLastRoom's rule
    void AfterRestoreDefaults(long inOldAdapter); // LobbyRestoreDefaults' rule after RestoreDefaults
    typedef LobbyOwnedMap<GUIString, LobbyOptionSet> OptionMap; // GUIStringLessNoCase
    LobbyKeyedMap<long>& UserIds() { return this->mUserIds; }
    LobbyIgnoredNameSet& IgnoredNames() { return this->mIgnoredNames; }
    LobbyKeyedMap<unsigned long>& BlockedCDKeyIds() { return this->mBlockedCDKeyIds; }
    LobbyFriendMap& Friends() { return this->mFriends; }
    OptionMap& DefaultGameOptions() { return this->mDefaultGameOptions; }
    OptionMap& GameOptions() { return this->mGameOptions; }
    LobbyUserInfoMap& Users() { return this->mUsers; }
    LobbyReconnectInfoMap& ReconnectInfoMap() { return this->mReconnectInfoMap; }
    LobbyLanNameInfoMap& LanNameInfoMap() { return this->mLanNameInfoMap; }
    LobbyDirectConnectMap& DirectConnectMap() { return this->mDirectConnectMap; }
private:
    friend struct PersistGlobalLoad; // the files' events applied (LobbyPersistentData.cpp)
    friend struct PersistUserLoad;
    LobbyPersistentData(const LobbyPersistentData&);
    LobbyPersistentData& operator=(const LobbyPersistentData&);
    GUIString GetText(EeWonPrefsText inText) const;
    void SetText(EeWonPrefsText inText, const GUIString& inValue);
    EeWonLobbyPrefs* mPrefs; // the scalar state (the original's 0x14 and 0x54..0x7d)
    LobbyKeyedMap<long> mUserIds;              // 0x08 (std::set<long>: the insert at 0x75fa52 gives the type)
    LobbyIgnoredNameSet mIgnoredNames;         // 0x18
    LobbyKeyedMap<unsigned long> mBlockedCDKeyIds; // 0x24 (loaded and saved, but nothing in the lobby fills it)
    LobbyFriendMap mFriends;                   // 0x30
    OptionMap mDefaultGameOptions;             // 0x3c
    OptionMap mGameOptions;                    // 0x48
    LobbyUserInfoMap mUsers;                   // 0x80
    LobbyReconnectInfoMap mReconnectInfoMap;   // 0x8c (LobbyReconnectInfo's two fields as the entry's numbers)
    Blowfish mEncryptKey;                      // 0x98..0x114c (0x10b4 bytes)
    LobbyLanNameInfoMap mLanNameInfoMap;
    LobbyDirectConnectMap mDirectConnectMap;   // 0x1158 (0x1164 bytes in all)

};
LobbyPersistentData* GetLobbyPersistentData(); // 0x75c900 (the lobby's instance; not owned)
int LobbyGetReconnectId(const IPAddr& inAddress); // 0x75db00 (0 when missing)
void LobbySetReconnectId(const IPAddr& inAddress, int inReconnectId); // 0x75d940 (then saves)
// the filter sets are LobbyOptionSet records (the SDK calls them ServerListFilterSet)
LobbyOptionSet LobbyGetFilterValues(const GUIString& inColumn); // 0x75dc80 (a copy; empty for a column without values)
void LobbyAddFilterValue(const GUIString& inColumn, const GUIString& inValue);
void LobbyRemoveFilterValue(const GUIString& inColumn, const GUIString& inValue);
}

#endif
