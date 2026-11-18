#ifndef EE_WONAPI_WONLOBBYPERSISTENCE_H
#define EE_WONAPI_WONLOBBYPERSISTENCE_H
#include "rust/won_lobby/persistobj.h"
#include "rust/won_lobby/prefsobj.h"
#include "rust/won_net/ipobj.h"
#include "platform/PlatSim.h"
#include "rust/won_net/file.h"
#include <string>
#include <set>
#include <exception>
#include <map>
#include <list>
#include <stdint.h>
#include <stdio.h>
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONBuffer.h"
// WONLobby's saved settings (LobbyPersistentData, 0x1164 bytes; constructor 0x75bcb0), their accessors and the
// file reader / writer they use

namespace WONAPI {
// FileReader: 0x14 bytes (ConfigParser embeds one at 0x18; constructor 0x7b1d20). FileWriter: 4 bytes
// (constructor 0x6a1e10, destructor 0x6a1e20). Both are ee_won's (rust/crates/ee_won_net/src/file.rs): the state is
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
    void Check(int inStatus) { FileReaderCheck(inStatus); } // throws FileReaderException for a failed status (the image decoders' Rust reads)
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
// ---- the lobby's saved settings (R6 WONLobby round 15): LobbyPersistentData and the Lobby* functions over it are
// one Rust object (rust/crates/ee_won_lobby/src/persistobj.rs: the users, ids, ignored names, blocked keys, friends,
// game filters, reconnect ids, LAN names, direct connects, the password key, the files, every rule). The functions
// below forward to it; without lobby data (LobbyPersist() NULL) each answers what the original did without it. The
// class is the C++ handle WONLobbyPrv keeps (its constructor and the object's host: LobbyPersistentData.cpp).
class LobbyPersistentData : public RefCount {
public:
    LobbyPersistentData();
    ~LobbyPersistentData() { ee_won_persist_free(this->mRust); }
    EeWonPersist* mRust;
private:
    LobbyPersistentData(const LobbyPersistentData&);
    LobbyPersistentData& operator=(const LobbyPersistentData&);
};
EeWonPersist* LobbyPersist(); // the lobby's object (0x75c900's), NULL without lobby data
// a saved login user (0x18 bytes in the original): the Rust record, alive while the user is saved
typedef EeWonPersistUser LobbyUserInfo;
class LobbyFriend;

typedef EeWonPersistCall LobbyPC_;
inline EeWonPersistText LobbyPersistIn(const GUIString& inText)
{
    EeWonPersistText t = {inText.mData->mText, (size_t)inText.length(), sizeof(wchar_t)};
    return t;
}
inline EeWonPersistText LobbyPersistNone() { EeWonPersistText t = {NULL, 0, sizeof(wchar_t)}; return t; }
inline GUIString LobbyPersistString(const uint32_t* inUnits, size_t inCount)
{
    std::wstring aText;
    aText.reserve(inCount);
    for (size_t i = 0; i < inCount; i++) aText += (wchar_t)inUnits[i];
    return GUIString(aText);
}
inline intptr_t LobbyPC(LobbyPC_ c, int64_t a = 0, int64_t b = 0)
{ return ee_won_persist_call(LobbyPersist(), c, LobbyPersistNone(), LobbyPersistNone(), a, b, NULL); }
inline intptr_t LobbyPC1(LobbyPC_ c, const GUIString& t, int64_t a = 0)
{ return ee_won_persist_call(LobbyPersist(), c, LobbyPersistIn(t), LobbyPersistNone(), a, 0, NULL); }
inline intptr_t LobbyPC2(LobbyPC_ c, const GUIString& t1, const GUIString& t2, int64_t a = 0)
{ return ee_won_persist_call(LobbyPersist(), c, LobbyPersistIn(t1), LobbyPersistIn(t2), a, 0, NULL); }
inline GUIString LobbyPCText(LobbyPC_ c, int64_t a = 0, const GUIString* t = NULL)
{
    size_t n = 0;
    const uint32_t* u = (const uint32_t*)ee_won_persist_call(LobbyPersist(), c, t != NULL ? LobbyPersistIn(*t) : LobbyPersistNone(),
                                                             LobbyPersistNone(), a, 0, &n);
    return LobbyPersistString(u, n);
}
inline intptr_t LobbyPersistAddr(const IPAddr& inAddr) { return (intptr_t)inAddr.GetValue(); }

// the saved login users, by name ignoring case (none without lobby data)
inline size_t LobbyGetUserCount() { return (size_t)LobbyPC(LobbyPC_::UserCount); }
inline GUIString LobbyGetUserNameAt(size_t inPos) { return LobbyPCText(LobbyPC_::UserNameAt, (int64_t)inPos); }
inline long LobbyGetUserLastUseAt(size_t inPos) { return (long)LobbyPC(LobbyPC_::UserLastUseAt, (int64_t)inPos); }
inline LobbyUserInfo* LobbyFindUser(const GUIString& inName) { return (LobbyUserInfo*)LobbyPC1(LobbyPC_::FindUser, inName); }
// records a login as a saved user's most recent use (a new user gets a free id, its stale file deleted); with
// inExistingOnly NULL for a user never seen before
inline LobbyUserInfo* LobbyAddUser(const GUIString& inName, const GUIString& inPassword, bool inExistingOnly)
{ return (LobbyUserInfo*)LobbyPC2(LobbyPC_::AddUser, inName, inPassword, inExistingOnly); }
inline LobbyUserInfo* LobbyGetCurrentUser() { return (LobbyUserInfo*)LobbyPC(LobbyPC_::CurrentUser); } // NULL: the LAN user, or no record
// makes the user (NULL: the LAN user) current and loads its preferences
inline void LobbySetCurrentUser(LobbyUserInfo* inUser) { LobbyPC(LobbyPC_::SetCurrentUser, (int64_t)(intptr_t)inUser); }
// a record's fields: the id, the terms of use's two times (the original's mValue04 / mValue08), name, password
inline long LobbyUserTOU(const LobbyUserInfo* inUser, int inWhich) { return (long)ee_won_persist_user_number(inUser, 1 + inWhich); }
inline void LobbyUserSetTOU(LobbyUserInfo* inUser, long inSys, long inGame) { ee_won_persist_user_set_tou(inUser, inSys, inGame); }
inline GUIString LobbyUserText(const LobbyUserInfo* inUser, int inWhich)
{ size_t n = 0; const uint32_t* u = ee_won_persist_user_text(inUser, inWhich, &n); return LobbyPersistString(u, n); }
inline GUIString LobbyGetUserPassword(const GUIString& inName)
{ LobbyUserInfo* u = LobbyFindUser(inName); return u != NULL ? LobbyUserText(u, 1) : GUIString(); }

// the options (defaults without lobby data); a setter marks the settings changed only on a change, the sound and
// music ones set the global dirty flag to whether they differ from the saved state
#define EE_LOBBY_PERSIST_FLAG(Name, F) \
    inline bool LobbyGet##Name() { return LobbyPC(LobbyPC_::Flag, (int64_t)EeWonPrefsFlag::F) != 0; } \
    inline void LobbySet##Name(bool inOn) { LobbyPC(LobbyPC_::ChangeFlag, (int64_t)EeWonPrefsFlag::F, inOn); }
EE_LOBBY_PERSIST_FLAG(FilterDirtyWords, FilterDirtyWords)
EE_LOBBY_PERSIST_FLAG(DoAutoAFK, DoAutoAfk)
EE_LOBBY_PERSIST_FLAG(CrossPromotion, CrossPromotion)
EE_LOBBY_PERSIST_FLAG(LobbySoundEffects, LobbySoundEffects)
EE_LOBBY_PERSIST_FLAG(ChatSoundEffects, ChatSoundEffects)
EE_LOBBY_PERSIST_FLAG(StagingSoundEffects, StagingSoundEffects)
EE_LOBBY_PERSIST_FLAG(LobbyMusic, LobbyMusic)
EE_LOBBY_PERSIST_FLAG(ShowConfirmations, ShowConfirmations)
EE_LOBBY_PERSIST_FLAG(AutoPingGames, AutoPingGames)
EE_LOBBY_PERSIST_FLAG(ShowGamesInProgress, ShowGamesInProgress)
EE_LOBBY_PERSIST_FLAG(AnonymousToFriends, AnonymousToFriends)
#undef EE_LOBBY_PERSIST_FLAG
inline unsigned short LobbyGetConnectionSpeed() { return (unsigned short)LobbyPC(LobbyPC_::ConnectionSpeed); } // 50
inline void LobbySetConnectionSpeed(unsigned short inSpeed) { LobbyPC(LobbyPC_::ChangeConnectionSpeed, inSpeed); }
// TEST ONLY: the harness slot's port offset (EE_TEST_PORT_OFFSET under EE_TEST_UI, through gPlatSimPortOffset; 0
// otherwise): added to the LAN broadcast port and the lobby port, so that LAN games in two slots never meet
inline unsigned short LobbyTestPortOffset()
{ return gPlatSimPortOffset > 0 && gPlatSimPortOffset < 4096 ? (unsigned short)gPlatSimPortOffset : 0; }
// the LAN broadcast port (8585 without lobby data), the stored one moved by the test offset
inline unsigned short LobbyGetLanBroadcastPort()
{ return (unsigned short)(LobbyPC(LobbyPC_::LanBroadcastPort) + LobbyTestPortOffset()); }
inline void LobbySetLanBroadcastPort(unsigned short inPort)
{ LobbyPC(LobbyPC_::ChangeLanBroadcastPort, (unsigned short)(inPort - LobbyTestPortOffset())); }
inline long LobbyGetNetworkAdapter() { return (long)LobbyPC(LobbyPC_::NetworkAdapter); } // a numeric IPv4 address
inline void LobbySaveNetworkAdapter() { LobbyPC(LobbyPC_::SaveNetworkAdapter); } // the local host, the global flag set
// the files: written when forced or changed; the global one loaded
inline void LobbySaveGlobalPrefs(bool inForce) { LobbyPC(LobbyPC_::SaveGlobal, inForce); }
inline void LobbySaveUserPrefs(bool inForce) { LobbyPC(LobbyPC_::SaveUser, inForce); }
inline void LobbyLoadGlobalPrefs() { LobbyPC(LobbyPC_::LoadGlobal); }
inline void LobbyRestoreDefaults() { LobbyPC(LobbyPC_::RestoreDefaults); } // the user flag set, the global one on a change
inline GUIString LobbyGetTabSelect() { return LobbyPCText(LobbyPC_::Text, (int64_t)EeWonPrefsText::TabSelect); }
inline void LobbySetTabSelect(const GUIString& inTab) { LobbyPC1(LobbyPC_::ChangeTabSelect, inTab); }
inline GUIString LobbyGetLastServerName() { return LobbyPCText(LobbyPC_::Text, (int64_t)EeWonPrefsText::LastServerName); }
inline GUIString LobbyGetLastRoomName() { return LobbyPCText(LobbyPC_::Text, (int64_t)EeWonPrefsText::LastRoomName); }
inline void LobbySetLastRoom(const GUIString& inServer, const GUIString& inRoom) { LobbyPC2(LobbyPC_::SetLastRoom, inServer, inRoom); }

// the friends, by user id (not owned)
inline size_t LobbyGetFriendCount() { return (size_t)LobbyPC(LobbyPC_::FriendCount); }
inline LobbyFriend* LobbyGetFriendAt(size_t inPos) { return (LobbyFriend*)LobbyPC(LobbyPC_::FriendAt, (int64_t)inPos); }
inline LobbyFriend* LobbyFindFriend(unsigned long inId) { return (LobbyFriend*)LobbyPC(LobbyPC_::FindFriend, (int64_t)inId); }
// a new friend unless the id is there (the user flag set); the friend
inline LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inId)
{ return (LobbyFriend*)LobbyPC1(LobbyPC_::AddFriend, inName, (int64_t)inId); }
inline void LobbyRemoveFriend(unsigned long inId) { LobbyPC(LobbyPC_::RemoveFriend, (int64_t)inId); } // the user flag: whether one went
// the ignored names, ignoring case; the user flag set to whether the list changed
inline size_t LobbyGetIgnoredNameCount() { return (size_t)LobbyPC(LobbyPC_::IgnoredCount); }
inline GUIString LobbyGetIgnoredNameAt(size_t inPos) { return LobbyPCText(LobbyPC_::IgnoredAt, (int64_t)inPos); }
inline bool LobbyIsNameIgnored(const GUIString& inName) { return LobbyPC1(LobbyPC_::IsIgnored, inName) != 0; }
inline void LobbySetNameIgnored(const GUIString& inName, bool inIgnore) { LobbyPC1(LobbyPC_::SetIgnored, inName, inIgnore); }
// the blocked CD key ids (loaded and saved, never filled by the lobby)
inline bool LobbyIsCDKeyBlocked(unsigned long inId) { return LobbyPC(LobbyPC_::IsCDKeyBlocked, (int64_t)inId) != 0; }

// the LAN names: the current one, the recent ones (at most 50)
inline GUIString LobbyGetLanName() { return LobbyPCText(LobbyPC_::Text, (int64_t)EeWonPrefsText::LanName); }
inline size_t LobbyGetLanNameCount() { return (size_t)LobbyPC(LobbyPC_::LanNameCount); }
inline GUIString LobbyGetLanNameAt(size_t inPos) { return LobbyPCText(LobbyPC_::LanNameAt, (int64_t)inPos); }
inline void LobbySetLanName(const GUIString& inName) { LobbyPC1(LobbyPC_::SetLanName, inName); }
inline void LobbyRemoveLanName() { LobbyPC(LobbyPC_::RemoveLanName); } // the most recent other one becomes current
// the direct-connect addresses and their names, by address
inline size_t LobbyGetDirectConnectCount() { return (size_t)LobbyPC(LobbyPC_::DirectCount); }
inline IPAddr LobbyGetDirectConnectAddressAt(size_t inPos)
{
    const EeWonIpAddr* a = (const EeWonIpAddr*)LobbyPC(LobbyPC_::DirectAddrAt, (int64_t)inPos);
    return a != NULL ? IPAddr::FromValue(ee_won_ipaddr_clone(a)) : IPAddr();
}
inline GUIString LobbyGetDirectConnectNameAt(size_t inPos) { return LobbyPCText(LobbyPC_::DirectNameAt, (int64_t)inPos); }
inline void LobbyAddDirectConnect(const IPAddr& inAddr, const GUIString& inName)
{ LobbyPC1(LobbyPC_::AddDirect, inName, LobbyPersistAddr(inAddr)); }
inline void LobbyRemoveDirectConnect(const IPAddr& inAddr) { LobbyPC(LobbyPC_::RemoveDirect, LobbyPersistAddr(inAddr)); }
// a server's reconnect id (0 when none); setting one stamps the time and saves the user's file
inline int LobbyGetReconnectId(const IPAddr& inAddr) { return (int)LobbyPC(LobbyPC_::ReconnectId, LobbyPersistAddr(inAddr)); }
inline void LobbySetReconnectId(const IPAddr& inAddr, int inId) { LobbyPC(LobbyPC_::SetReconnectId, LobbyPersistAddr(inAddr), inId); }

// the game list's filters: a column's values (unique, in the order added), the defaults the config adds
inline size_t LobbyGetFilterCount(const GUIString& inColumn) { return (size_t)LobbyPC1(LobbyPC_::FilterCount, inColumn); }
inline GUIString LobbyGetFilterValueAt(const GUIString& inColumn, size_t inPos)
{ return LobbyPCText(LobbyPC_::FilterValueAt, (int64_t)inPos, &inColumn); }
inline void LobbyAddFilterValue(const GUIString& inColumn, const GUIString& inValue) { LobbyPC2(LobbyPC_::AddFilterValue, inColumn, inValue); }
// (an emptied column stays)
inline void LobbyRemoveFilterValue(const GUIString& inColumn, const GUIString& inValue) { LobbyPC2(LobbyPC_::RemoveFilterValue, inColumn, inValue); }
inline void LobbyResetFilters() { LobbyPC(LobbyPC_::ResetFilters); } // the defaults copied, the user flag set
// comma-separated values into a default filter; a column with values is skipped unless appending
inline void LobbyAddGameOption(const GUIString& inColumn, const GUIString& inValues, bool inAppend)
{ LobbyPC2(LobbyPC_::AddGameOption, inColumn, inValues, inAppend); }
}

#endif
