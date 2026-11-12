// The lobby's saved settings (LobbyPersistentData.cpp): the original's Save/Load statements (copied from the C++
// before the port, over the real FileReader / FileWriter and stand-in containers of the same kinds) against ee_won's
// persist module through its C ABI (the files it writes, and its reads' events applied as LobbyPersistentData's
// forwarders apply them). Every file is saved both ways and must be the same bytes; then the file, cut at every
// length and with a wrong version, is loaded both ways into stand-ins that must end the same. The rules (the user
// file name, the first free id, pruning users and LAN names, the most recent LAN name, the default name's number)
// against the original's statements too. A password's "encryption" is a fixed XOR on both sides (the Blowfish key
// is the machine's). Links FileReader / FileWriter / WONFile, the buffers and CRC16. Prints one line per failed
// check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONCRC16.h"
#include "rust/ee_won.h"
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI { std::wstring StringToWString(const std::string& inString); } // StringUtil.cpp

using namespace WONAPI;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

struct NoCase {
    bool operator()(const std::wstring& a, const std::wstring& b) const
    {
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            wint_t x = towupper(a[i]), y = towupper(b[i]);
            if (x != y) return x < y;
        }
        return a.size() < b.size();
    }
};

struct User {
    long mUserId, mValue04, mValue08, mLastUseTime;
    std::wstring mUserName, mPassword;
    User() : mUserId(0), mValue04(0), mValue08(0), mLastUseTime(0) {}
    bool operator==(const User& o) const
    {
        return mUserId == o.mUserId && mValue04 == o.mValue04 && mValue08 == o.mValue08 &&
               mLastUseTime == o.mLastUseTime && mUserName == o.mUserName && mPassword == o.mPassword;
    }
};
struct Reconnect {
    long mLastUseTime;
    int mReconnectId;
    Reconnect() : mLastUseTime(0), mReconnectId(-12345) {}
    bool operator==(const Reconnect& o) const { return mLastUseTime == o.mLastUseTime && mReconnectId == o.mReconnectId; }
};
struct OptionSet {
    std::list<std::wstring> mValues;
    std::set<std::wstring> mIndex;
    void Add(const std::wstring& v) { if (mIndex.insert(v).second) mValues.push_back(v); }
    bool operator==(const OptionSet& o) const { return mValues == o.mValues; }
};
typedef std::string Addr; // six bytes

// the stand-in of LobbyPersistentData's saved fields
struct Prefs {
    std::map<std::wstring, User, NoCase> mUsers;
    std::set<long> mUserIds;
    bool mLobbySoundEffects, mSavedLobbySoundEffects, mLobbyMusic, mSavedLobbyMusic;
    long mNetworkAdapter;
    bool mGlobalDirty, mUserDirty;
    long mCurrentUserId;
    std::map<std::wstring, long, NoCase> mLanNameInfoMap;
    std::wstring mLanName;
    std::map<Addr, std::wstring> mDirectConnectMap;
    unsigned short mLanBroadcastPort, mConnectionSpeed;
    std::map<Addr, Reconnect> mReconnectInfoMap;
    std::set<std::wstring, NoCase> mIgnoredNames;
    std::map<unsigned long, std::wstring> mFriends;
    std::set<unsigned long> mBlockedCDKeyIds;
    bool mFilterDirtyWords, mDoAutoAFK, mCrossPromotion, mAutoPingGames, mAnonymousToFriends, mShowGamesInProgress;
    std::wstring mTabSelect, mLastServerName, mLastRoomName;
    bool mChatSoundEffects, mStagingSoundEffects, mShowConfirmations;
    std::map<std::wstring, OptionSet, NoCase> mGameOptions, mDefaultGameOptions;
    bool mMuted; // LobbyGlobal_ButtonClick_Sound->SetMuted

    Prefs()
    {
        mLobbySoundEffects = mSavedLobbySoundEffects = mLobbyMusic = mSavedLobbyMusic = true;
        mNetworkAdapter = 0;
        mGlobalDirty = mUserDirty = true;
        mCurrentUserId = 0;
        mLanBroadcastPort = 8585;
        mConnectionSpeed = 25;
        mFilterDirtyWords = mDoAutoAFK = mCrossPromotion = mAutoPingGames = mShowGamesInProgress = true;
        mAnonymousToFriends = false;
        mChatSoundEffects = mStagingSoundEffects = mShowConfirmations = true;
        mMuted = false;
        mDefaultGameOptions[L"Default"].Add(L"x");
    }
    void RestoreDefaults()
    {
        mLastServerName = mLastRoomName = L"";
        mTabSelect = mCurrentUserId != 0 ? L"QuickPlay" : L"Lan";
        if (mCurrentUserId == 0) mLanName = L"Default";
        mFilterDirtyWords = mDoAutoAFK = mCrossPromotion = mLobbySoundEffects = mLobbyMusic = mShowConfirmations =
            mChatSoundEffects = mStagingSoundEffects = mAutoPingGames = mShowGamesInProgress = true;
        mAnonymousToFriends = false;
        mLanBroadcastPort = 8585;
        mConnectionSpeed = 25;
        mNetworkAdapter = 0;
    }
    void SelectRecentLanName()
    {
        mLanName = L"Default";
        long aLastTime = 0;
        for (std::map<std::wstring, long, NoCase>::iterator i = mLanNameInfoMap.begin(); i != mLanNameInfoMap.end(); ++i)
            if (aLastTime == 0 || i->second > aLastTime) {
                mLanName = i->first;
                aLastTime = i->second;
            }
    }
    bool operator==(const Prefs& o) const
    {
        return mUsers == o.mUsers && mUserIds == o.mUserIds && mLobbySoundEffects == o.mLobbySoundEffects &&
               mSavedLobbySoundEffects == o.mSavedLobbySoundEffects && mLobbyMusic == o.mLobbyMusic &&
               mSavedLobbyMusic == o.mSavedLobbyMusic && mNetworkAdapter == o.mNetworkAdapter &&
               mGlobalDirty == o.mGlobalDirty && mUserDirty == o.mUserDirty && mLanNameInfoMap == o.mLanNameInfoMap &&
               mLanName == o.mLanName && mDirectConnectMap == o.mDirectConnectMap &&
               mLanBroadcastPort == o.mLanBroadcastPort && mConnectionSpeed == o.mConnectionSpeed &&
               mReconnectInfoMap == o.mReconnectInfoMap && mIgnoredNames == o.mIgnoredNames && mFriends == o.mFriends &&
               mBlockedCDKeyIds == o.mBlockedCDKeyIds && mFilterDirtyWords == o.mFilterDirtyWords &&
               mDoAutoAFK == o.mDoAutoAFK && mCrossPromotion == o.mCrossPromotion &&
               mAutoPingGames == o.mAutoPingGames && mAnonymousToFriends == o.mAnonymousToFriends &&
               mShowGamesInProgress == o.mShowGamesInProgress && mTabSelect == o.mTabSelect &&
               mLastServerName == o.mLastServerName && mLastRoomName == o.mLastRoomName &&
               mChatSoundEffects == o.mChatSoundEffects && mStagingSoundEffects == o.mStagingSoundEffects &&
               mShowConfirmations == o.mShowConfirmations && mGameOptions == o.mGameOptions && mMuted == o.mMuted;
    }
};

// the password's stand-in encryption: the UTF-16 string's WriteBuffer form, each byte XOR 0x5a (an empty password
// "fails", as Blowfish's NULL result)
static std::string Encrypt(const std::wstring& inText)
{
    WriteBuffer aStorage(0, true);
    aStorage.AppendWString(inText, 2);
    std::string anOut(aStorage.data(), aStorage.length());
    for (size_t i = 0; i < anOut.size(); i++) anOut[i] ^= 0x5a;
    return anOut;
}
static std::wstring Decrypt(const char* inData, size_t inLength)
{
    if (inLength == 0) return L"";
    std::string aPlain(inData, inLength);
    for (size_t i = 0; i < aPlain.size(); i++) aPlain[i] ^= 0x5a;
    ReadBuffer aDecoded(aPlain.data(), aPlain.length());
    try {
        std::wstring aText;
        aDecoded.ReadWString(aText, 2);
        return aText;
    } catch (ReadBufferException&) {
    }
    return L"";
}

// ---- the original's statements (LobbyPersistentData.cpp before the port)

static long gGlobalVersion = 2, gUserVersion = 4;

static long OrigReadFileHeader(FileReader& inReader)
{
    try {
        char aHeader[7];
        aHeader[6] = 0;
        inReader.ReadBytes(aHeader, 6);
        if (strcmp(aHeader, "WONPER") == 0) return inReader.ReadLong();
    } catch (FileReaderException&) {
    }
    return 0;
}
static void OrigWriteFileHeader(FileWriter& outWriter, long inVersion)
{
    try {
        outWriter.WriteBytes("WONPER", 6);
        outWriter.WriteLong(inVersion);
    } catch (FileWriterException&) {
    }
}
static std::wstring OrigReadWideString(FileReader& inReader)
{
    unsigned short aLength = inReader.ReadShort();
    std::wstring aString;
    for (int i = 0; i < aLength; ++i) aString += (wchar_t)inReader.ReadShort();
    return aString;
}
static std::string OrigReadByteString(FileReader& inReader) // the bytes (UTF-8 since the D6 change)
{
    unsigned short aLength = inReader.ReadShort();
    std::string aBytes(aLength, '\0');
    for (int i = 0; i < aLength; ++i) aBytes[i] = (char)inReader.ReadByte();
    return aBytes;
}
static void OrigWriteByteString(FileWriter& outWriter, const std::string& inText)
{
    outWriter.WriteShort(inText.length());
    outWriter.WriteBytes(inText.c_str(), inText.length());
}
static void OrigWriteWideString(FileWriter& outWriter, const std::wstring& inText)
{
    outWriter.WriteShort(inText.length());
    for (size_t i = 0; i < inText.length(); ++i) outWriter.WriteShort(inText[i]);
}
static std::wstring OrigReadEncryptedString(FileReader& reader)
{
    unsigned short length = reader.ReadShort();
    std::string storage(length, '\0');
    if (length) reader.ReadBytes(&storage[0], length);
    return Decrypt(storage.data(), length);
}
static void OrigWriteEncryptedString(FileWriter& writer, const std::wstring& inText)
{
    std::string e = inText.empty() ? std::string() : Encrypt(inText);
    writer.WriteShort(e.length());
    writer.WriteBytes(e.data(), e.length());
}

static void OrigLoadGlobalPrefs(Prefs& p, const char* inFile)
{
    FileReader aReader;
    if (!aReader.Open(inFile)) return;
    p.mUsers.clear();
    p.mNetworkAdapter = 0;
    try {
        if (OrigReadFileHeader(aReader) != gGlobalVersion) return;
        unsigned short aCount = aReader.ReadShort();
        for (int i = 0; i < aCount; ++i) {
            std::wstring aName = OrigReadWideString(aReader);
            User& aUser = p.mUsers.insert(std::make_pair(aName, User())).first->second;
            aUser.mUserName = aName;
            aUser.mLastUseTime = aReader.ReadLong();
            aUser.mUserId = aReader.ReadShort();
            aUser.mValue04 = aReader.ReadLong();
            aUser.mValue08 = aReader.ReadLong();
            aUser.mPassword = OrigReadEncryptedString(aReader);
            p.mUserIds.insert(aUser.mUserId);
        }
        p.mLobbySoundEffects = p.mSavedLobbySoundEffects = aReader.ReadByte() != 0;
        p.mMuted = !p.mLobbySoundEffects;
        p.mLobbyMusic = p.mSavedLobbyMusic = aReader.ReadByte() != 0;
        p.mNetworkAdapter = aReader.ReadLong();
    } catch (FileReaderException&) {
    }
    p.mGlobalDirty = false;
}

static void OrigSaveGlobalPrefs(Prefs& p, const char* inFile)
{
    FileWriter aWriter;
    if (!aWriter.Open(inFile, false)) return;
    try {
        OrigWriteFileHeader(aWriter, gGlobalVersion);
        aWriter.WriteShort(p.mUsers.size());
        for (std::map<std::wstring, User, NoCase>::iterator i = p.mUsers.begin(); i != p.mUsers.end(); ++i) {
            User& aUser = i->second;
            OrigWriteWideString(aWriter, aUser.mUserName);
            aWriter.WriteLong(aUser.mLastUseTime);
            aWriter.WriteShort(aUser.mUserId);
            aWriter.WriteLong(aUser.mValue04);
            aWriter.WriteLong(aUser.mValue08);
            OrigWriteEncryptedString(aWriter, aUser.mPassword);
        }
        aWriter.WriteByte(p.mLobbySoundEffects ? 1 : 0);
        aWriter.WriteByte(p.mLobbyMusic ? 1 : 0);
        aWriter.WriteLong(p.mNetworkAdapter);
    } catch (FileWriterException&) {
    }
    p.mSavedLobbySoundEffects = p.mLobbySoundEffects;
    p.mSavedLobbyMusic = p.mLobbyMusic;
    p.mGlobalDirty = false;
}

static void OrigReadLanPrefs(Prefs& p, FileReader& reader)
{
    int count = reader.ReadShort();
    for (int nameIndex = 0; nameIndex < count; ++nameIndex) {
        std::wstring name = OrigReadWideString(reader);
        long lastUseTime = reader.ReadLong();
        p.mLanNameInfoMap[name] = lastUseTime;
    }
    p.SelectRecentLanName();
    count = reader.ReadShort();
    for (int addressIndex = 0; addressIndex < count; ++addressIndex) {
        char bytes[6];
        reader.ReadBytes(bytes, 6);
        std::wstring name = OrigReadWideString(reader);
        p.mDirectConnectMap[Addr(bytes, 6)] = name;
    }
    p.mLanBroadcastPort = reader.ReadShort();
}

static void OrigLoadUserPrefs(Prefs& p, const char* inFile)
{
    p.RestoreDefaults();
    p.mReconnectInfoMap.clear();
    p.mIgnoredNames.clear();
    p.mFriends.clear();
    p.mLanNameInfoMap.clear();
    p.mDirectConnectMap.clear();
    p.mGameOptions.clear();
    bool haveGameOptions = false;
    int i;
    FileReader reader;
    if (!reader.Open(inFile)) {
        p.mGameOptions = p.mDefaultGameOptions;
        return;
    }
    try {
        if (OrigReadFileHeader(reader) != gUserVersion) return;
        if (p.mCurrentUserId == 0) OrigReadLanPrefs(p, reader);
        unsigned short count = reader.ReadShort();
        for (i = 0; i < count; ++i) {
            char bytes[6];
            reader.ReadBytes(bytes, 6);
            Reconnect& info = p.mReconnectInfoMap.insert(std::make_pair(Addr(bytes, 6), Reconnect())).first->second;
            info.mLastUseTime = reader.ReadLong();
            info.mReconnectId = reader.ReadLong();
        }
        count = reader.ReadShort();
        for (i = 0; i < count; ++i) p.mIgnoredNames.insert(OrigReadWideString(reader));
        count = reader.ReadShort();
        for (i = 0; i < count; ++i) {
            std::wstring name = OrigReadWideString(reader);
            unsigned long userId = reader.ReadLong();
            p.mFriends.insert(std::make_pair(userId, name));
        }
        count = reader.ReadShort();
        for (i = 0; i < count; ++i) p.mBlockedCDKeyIds.insert(reader.ReadLong());
        p.mFilterDirtyWords = reader.ReadByte() == 1;
        p.mDoAutoAFK = reader.ReadByte() == 1;
        p.mCrossPromotion = reader.ReadByte() == 1;
        p.mAutoPingGames = reader.ReadByte() == 1;
        p.mAnonymousToFriends = reader.ReadByte() == 1;
        p.mShowGamesInProgress = reader.ReadByte() == 1;
        p.mTabSelect = OrigReadWideString(reader);
        p.mLastServerName = OrigReadWideString(reader);
        p.mLastRoomName = OrigReadWideString(reader);
        p.mLobbySoundEffects = reader.ReadByte() == 1;
        p.mLobbyMusic = reader.ReadByte() == 1;
        p.mChatSoundEffects = reader.ReadByte() == 1;
        p.mStagingSoundEffects = reader.ReadByte() == 1;
        p.mShowConfirmations = reader.ReadByte() == 1;
        p.mConnectionSpeed = reader.ReadShort();
        count = reader.ReadShort();
        haveGameOptions = true;
        for (i = 0; i < count; ++i) {
            std::string optionName = OrigReadByteString(reader);
            OptionSet& option = p.mGameOptions[StringToWString(optionName)];
            unsigned short numValues = reader.ReadShort();
            for (int j = 0; j < numValues; ++j) option.Add(OrigReadWideString(reader));
        }
    } catch (FileReaderException&) {
    }
    if (!haveGameOptions) p.mGameOptions = p.mDefaultGameOptions;
    p.mGlobalDirty = p.mLobbySoundEffects != p.mSavedLobbySoundEffects || p.mSavedLobbyMusic != p.mLobbyMusic;
    p.mUserDirty = false;
}

static void OrigSaveUserPrefs(Prefs& p, const char* inFile)
{
    FileWriter writer;
    if (!writer.Open(inFile, false)) return;
    try {
        OrigWriteFileHeader(writer, gUserVersion);
        if (p.mCurrentUserId == 0) {
            writer.WriteShort((unsigned short)p.mLanNameInfoMap.size());
            for (std::map<std::wstring, long, NoCase>::const_iterator i = p.mLanNameInfoMap.begin();
                 i != p.mLanNameInfoMap.end(); ++i) {
                OrigWriteWideString(writer, i->first);
                writer.WriteLong(i->second);
            }
            writer.WriteShort((unsigned short)p.mDirectConnectMap.size());
            for (std::map<Addr, std::wstring>::const_iterator j = p.mDirectConnectMap.begin();
                 j != p.mDirectConnectMap.end(); ++j) {
                writer.WriteBytes(j->first.data(), 6);
                OrigWriteWideString(writer, j->second);
            }
            writer.WriteShort(p.mLanBroadcastPort);
        }
        writer.WriteShort((unsigned short)p.mReconnectInfoMap.size());
        for (std::map<Addr, Reconnect>::const_iterator a = p.mReconnectInfoMap.begin(); a != p.mReconnectInfoMap.end();
             ++a) {
            writer.WriteBytes(a->first.data(), 6);
            writer.WriteLong(a->second.mLastUseTime);
            writer.WriteLong(a->second.mReconnectId);
        }
        writer.WriteShort((unsigned short)p.mIgnoredNames.size());
        for (std::set<std::wstring, NoCase>::const_iterator n = p.mIgnoredNames.begin(); n != p.mIgnoredNames.end(); ++n)
            OrigWriteWideString(writer, *n);
        writer.WriteShort((unsigned short)p.mFriends.size());
        for (std::map<unsigned long, std::wstring>::const_iterator f = p.mFriends.begin(); f != p.mFriends.end(); ++f) {
            OrigWriteWideString(writer, f->second);
            writer.WriteLong(f->first);
        }
        writer.WriteShort((unsigned short)p.mBlockedCDKeyIds.size());
        for (std::set<unsigned long>::const_iterator u = p.mBlockedCDKeyIds.begin(); u != p.mBlockedCDKeyIds.end(); ++u)
            writer.WriteLong(*u);
        writer.WriteByte(p.mFilterDirtyWords ? 1 : 0);
        writer.WriteByte(p.mDoAutoAFK ? 1 : 0);
        writer.WriteByte(p.mCrossPromotion ? 1 : 0);
        writer.WriteByte(p.mAutoPingGames ? 1 : 0);
        writer.WriteByte(p.mAnonymousToFriends ? 1 : 0);
        writer.WriteByte(p.mShowGamesInProgress ? 1 : 0);
        OrigWriteWideString(writer, p.mTabSelect);
        OrigWriteWideString(writer, p.mLastServerName);
        OrigWriteWideString(writer, p.mLastRoomName);
        writer.WriteByte(p.mLobbySoundEffects ? 1 : 0);
        writer.WriteByte(p.mLobbyMusic ? 1 : 0);
        writer.WriteByte(p.mChatSoundEffects ? 1 : 0);
        writer.WriteByte(p.mStagingSoundEffects ? 1 : 0);
        writer.WriteByte(p.mShowConfirmations ? 1 : 0);
        writer.WriteShort(p.mConnectionSpeed);
        writer.WriteShort((unsigned short)p.mGameOptions.size());
        for (std::map<std::wstring, OptionSet, NoCase>::const_iterator o = p.mGameOptions.begin();
             o != p.mGameOptions.end(); ++o) {
            OrigWriteByteString(writer, WStringToString(o->first));
            unsigned long aCount = o->second.mIndex.size();
            if (aCount != 0) {
                writer.WriteShort((unsigned short)aCount);
                for (std::list<std::wstring>::const_iterator v = o->second.mValues.begin(); v != o->second.mValues.end();
                     ++v)
                    OrigWriteWideString(writer, *v);
            }
        }
    } catch (FileWriterException&) {
    }
    p.mUserDirty = false;
}

// ---- the port: the forwarders' lists and event handlers (as LobbyPersistentData.cpp has them)

static void AppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
struct Texts {
    std::vector<uint16_t> mUnits;
    std::vector<size_t> mOffsets;
    Texts() : mOffsets(1, 0) {}
    void Add(const std::wstring& inText)
    {
        std::string aBytes;
        WideAppendUtf16LE(aBytes, inText);
        for (size_t i = 0; i + 1 < aBytes.size(); i += 2)
            mUnits.push_back((uint16_t)((unsigned char)aBytes[i] | ((unsigned char)aBytes[i + 1] << 8)));
        mOffsets.push_back(mUnits.size());
    }
    EeWonTextList List() const { EeWonTextList l = {mUnits.empty() ? NULL : &mUnits[0], &mOffsets[0], mOffsets.size() - 1}; return l; }
};
struct Bytes {
    std::string mBytes;
    std::vector<size_t> mOffsets;
    Bytes() : mOffsets(1, 0) {}
    void Add(const std::string& inBytes) { mBytes += inBytes; mOffsets.push_back(mBytes.size()); }
    EeWonTextList List() const { EeWonTextList l = {mBytes.data(), &mOffsets[0], mOffsets.size() - 1}; return l; }
};
static std::wstring Text(const EeWonPrefsEvent* e)
{
    std::wstring aText;
    for (size_t i = 0; i < e->count; i++) WideAppendUtf16Unit(aText, e->units[i]);
    return aText;
}

static void PortSaveGlobalPrefs(Prefs& p, const char* inFile)
{
    FileWriter aWriter;
    if (!aWriter.Open(inFile, false)) return;
    Texts aNames;
    Bytes aPasswords;
    std::vector<int32_t> aTimes, a04, a08;
    std::vector<uint16_t> anIds;
    for (std::map<std::wstring, User, NoCase>::iterator i = p.mUsers.begin(); i != p.mUsers.end(); ++i) {
        aNames.Add(i->second.mUserName);
        aTimes.push_back(i->second.mLastUseTime);
        anIds.push_back((uint16_t)i->second.mUserId);
        a04.push_back(i->second.mValue04);
        a08.push_back(i->second.mValue08);
        aPasswords.Add(i->second.mPassword.empty() ? std::string() : Encrypt(i->second.mPassword));
    }
    EeWonGlobalPrefsOut o;
    o.names = aNames.List();
    o.last_use = aTimes.empty() ? NULL : &aTimes[0];
    o.ids = anIds.empty() ? NULL : &anIds[0];
    o.value04 = a04.empty() ? NULL : &a04[0];
    o.value08 = a08.empty() ? NULL : &a08[0];
    o.passwords = aPasswords.List();
    o.sound_effects = p.mLobbySoundEffects;
    o.music = p.mLobbyMusic;
    o.network_adapter = p.mNetworkAdapter;
    std::string aFile;
    ee_won_prefs_write_global(&o, AppendBytes, &aFile);
    try {
        aWriter.WriteBytes(aFile.data(), aFile.length());
    } catch (FileWriterException&) {
    }
    p.mSavedLobbySoundEffects = p.mLobbySoundEffects;
    p.mSavedLobbyMusic = p.mLobbyMusic;
    p.mGlobalDirty = false;
}

static bool ReadWhole(const char* inFile, std::string& outBytes)
{
    FileReader aReader;
    if (!aReader.Open(inFile)) return false;
    char aBuffer[4096];
    try {
        int n;
        while ((n = aReader.ReadMaxBytes(aBuffer, sizeof(aBuffer))) > 0) outBytes.append(aBuffer, n);
    } catch (FileReaderException&) {
    }
    return true;
}

struct GlobalLoad {
    Prefs* mPrefs;
    User* mUser;
    static void Event(void* inContext, const EeWonPrefsEvent* e)
    {
        GlobalLoad* l = static_cast<GlobalLoad*>(inContext);
        Prefs& p = *l->mPrefs;
        switch (e->kind) {
        case EeWonPrefsEventKind::User: {
            std::wstring aName = Text(e);
            l->mUser = &p.mUsers.insert(std::make_pair(aName, User())).first->second;
            l->mUser->mUserName = aName;
            break;
        }
        case EeWonPrefsEventKind::UserTime: l->mUser->mLastUseTime = (long)e->value; break;
        case EeWonPrefsEventKind::UserId: l->mUser->mUserId = (long)e->value; break;
        case EeWonPrefsEventKind::UserValue04: l->mUser->mValue04 = (long)e->value; break;
        case EeWonPrefsEventKind::UserValue08: l->mUser->mValue08 = (long)e->value; break;
        case EeWonPrefsEventKind::UserPassword:
            l->mUser->mPassword = Decrypt(reinterpret_cast<const char*>(e->bytes), e->length);
            p.mUserIds.insert(l->mUser->mUserId);
            break;
        case EeWonPrefsEventKind::SoundEffects:
            p.mLobbySoundEffects = p.mSavedLobbySoundEffects = e->value != 0;
            p.mMuted = !p.mLobbySoundEffects;
            break;
        case EeWonPrefsEventKind::Music: p.mLobbyMusic = p.mSavedLobbyMusic = e->value != 0; break;
        case EeWonPrefsEventKind::NetworkAdapter: p.mNetworkAdapter = (long)e->value; break;
        default: break;
        }
    }
};

static void PortLoadGlobalPrefs(Prefs& p, const char* inFile)
{
    std::string aBytes;
    if (!ReadWhole(inFile, aBytes)) return;
    p.mUsers.clear();
    p.mNetworkAdapter = 0;
    GlobalLoad aLoad = {&p, NULL};
    if (ee_won_prefs_read_global(reinterpret_cast<const uint8_t*>(aBytes.data()), aBytes.length(), GlobalLoad::Event,
                                 &aLoad) == EeWonPrefsRead::OtherVersion)
        return;
    p.mGlobalDirty = false;
}

static void PortSaveUserPrefs(Prefs& p, const char* inFile)
{
    FileWriter writer;
    if (!writer.Open(inFile, false)) return;
    EeWonUserPrefsOut o;
    memset(&o, 0, sizeof(o));
    o.lan_user = p.mCurrentUserId == 0;
    Texts aLanNames, aDirectNames, anIgnored, aFriendNames, aTexts, aValues;
    std::vector<int32_t> aLanTimes, aTimes, anIds;
    std::string aDirect, aReconnect;
    std::vector<uint32_t> aFriendIds, aBlocked;
    Bytes anOptionNames;
    std::vector<size_t> anOptionFirst(1, 0);
    for (std::map<std::wstring, long, NoCase>::const_iterator i = p.mLanNameInfoMap.begin(); i != p.mLanNameInfoMap.end(); ++i) {
        aLanNames.Add(i->first);
        aLanTimes.push_back(i->second);
    }
    for (std::map<Addr, std::wstring>::const_iterator j = p.mDirectConnectMap.begin(); j != p.mDirectConnectMap.end(); ++j) {
        aDirect += j->first;
        aDirectNames.Add(j->second);
    }
    for (std::map<Addr, Reconnect>::const_iterator a = p.mReconnectInfoMap.begin(); a != p.mReconnectInfoMap.end(); ++a) {
        aReconnect += a->first;
        aTimes.push_back(a->second.mLastUseTime);
        anIds.push_back(a->second.mReconnectId);
    }
    for (std::set<std::wstring, NoCase>::const_iterator n = p.mIgnoredNames.begin(); n != p.mIgnoredNames.end(); ++n)
        anIgnored.Add(*n);
    for (std::map<unsigned long, std::wstring>::const_iterator f = p.mFriends.begin(); f != p.mFriends.end(); ++f) {
        aFriendNames.Add(f->second);
        aFriendIds.push_back(f->first);
    }
    for (std::set<unsigned long>::const_iterator u = p.mBlockedCDKeyIds.begin(); u != p.mBlockedCDKeyIds.end(); ++u)
        aBlocked.push_back(*u);
    aTexts.Add(p.mTabSelect);
    aTexts.Add(p.mLastServerName);
    aTexts.Add(p.mLastRoomName);
    for (std::map<std::wstring, OptionSet, NoCase>::const_iterator op = p.mGameOptions.begin(); op != p.mGameOptions.end(); ++op) {
        anOptionNames.Add(WStringToString(op->first));
        for (std::list<std::wstring>::const_iterator v = op->second.mValues.begin(); v != op->second.mValues.end(); ++v)
            aValues.Add(*v);
        anOptionFirst.push_back(aValues.mOffsets.size() - 1);
    }
    o.lan_names = aLanNames.List();
    o.lan_times = aLanTimes.empty() ? NULL : &aLanTimes[0];
    o.direct_addresses = reinterpret_cast<const uint8_t*>(aDirect.data());
    o.direct_names = aDirectNames.List();
    o.broadcast_port = p.mLanBroadcastPort;
    o.reconnect_count = aTimes.size();
    o.reconnect_addresses = reinterpret_cast<const uint8_t*>(aReconnect.data());
    o.reconnect_times = aTimes.empty() ? NULL : &aTimes[0];
    o.reconnect_ids = anIds.empty() ? NULL : &anIds[0];
    o.ignored = anIgnored.List();
    o.friend_names = aFriendNames.List();
    o.friend_ids = aFriendIds.empty() ? NULL : &aFriendIds[0];
    o.blocked_count = aBlocked.size();
    o.blocked = aBlocked.empty() ? NULL : &aBlocked[0];
    bool aFirst[6] = {p.mFilterDirtyWords, p.mDoAutoAFK, p.mCrossPromotion, p.mAutoPingGames, p.mAnonymousToFriends,
                      p.mShowGamesInProgress};
    bool aSecond[5] = {p.mLobbySoundEffects, p.mLobbyMusic, p.mChatSoundEffects, p.mStagingSoundEffects,
                       p.mShowConfirmations};
    memcpy(o.first, aFirst, sizeof(aFirst));
    memcpy(o.second, aSecond, sizeof(aSecond));
    o.texts = aTexts.List();
    o.connection_speed = p.mConnectionSpeed;
    o.option_names = anOptionNames.List();
    o.option_first = &anOptionFirst[0];
    o.option_values = aValues.List();
    std::string aFile;
    ee_won_prefs_write_user(&o, AppendBytes, &aFile);
    try {
        writer.WriteBytes(aFile.data(), aFile.length());
    } catch (FileWriterException&) {
    }
    p.mUserDirty = false;
}

struct UserLoad {
    Prefs* mPrefs;
    Reconnect* mReconnect;
    OptionSet* mOption;
    bool mHaveGameOptions;
    static void Event(void* inContext, const EeWonPrefsEvent* e)
    {
        UserLoad* l = static_cast<UserLoad*>(inContext);
        Prefs& p = *l->mPrefs;
        bool* const kFlags[14] = {&p.mFilterDirtyWords, &p.mDoAutoAFK, &p.mCrossPromotion, &p.mAutoPingGames,
                                  &p.mAnonymousToFriends, &p.mShowGamesInProgress, NULL, NULL, NULL,
                                  &p.mLobbySoundEffects, &p.mLobbyMusic, &p.mChatSoundEffects,
                                  &p.mStagingSoundEffects, &p.mShowConfirmations};
        std::wstring* const kTexts[3] = {&p.mTabSelect, &p.mLastServerName, &p.mLastRoomName};
        switch (e->kind) {
        case EeWonPrefsEventKind::LanName: p.mLanNameInfoMap[Text(e)] = (long)e->value; break;
        case EeWonPrefsEventKind::LanNamesDone: p.SelectRecentLanName(); break;
        case EeWonPrefsEventKind::DirectConnect:
            p.mDirectConnectMap[Addr(reinterpret_cast<const char*>(e->address), 6)] = Text(e);
            break;
        case EeWonPrefsEventKind::BroadcastPort: p.mLanBroadcastPort = (unsigned short)e->value; break;
        case EeWonPrefsEventKind::Reconnect:
            l->mReconnect = &p.mReconnectInfoMap
                                 .insert(std::make_pair(Addr(reinterpret_cast<const char*>(e->address), 6), Reconnect()))
                                 .first->second;
            break;
        case EeWonPrefsEventKind::ReconnectTime: l->mReconnect->mLastUseTime = (long)e->value; break;
        case EeWonPrefsEventKind::ReconnectId: l->mReconnect->mReconnectId = (int)e->value; break;
        case EeWonPrefsEventKind::Ignored: p.mIgnoredNames.insert(Text(e)); break;
        case EeWonPrefsEventKind::Friend: p.mFriends.insert(std::make_pair((unsigned long)e->value, Text(e))); break;
        case EeWonPrefsEventKind::Blocked: p.mBlockedCDKeyIds.insert((unsigned long)e->value); break;
        case EeWonPrefsEventKind::Flag:
            if (e->index < 14 && kFlags[e->index] != NULL) *kFlags[e->index] = e->value != 0;
            break;
        case EeWonPrefsEventKind::SettingText:
            if (e->index >= 6 && e->index < 9) *kTexts[e->index - 6] = Text(e);
            break;
        case EeWonPrefsEventKind::ConnectionSpeed: p.mConnectionSpeed = (unsigned short)e->value; break;
        case EeWonPrefsEventKind::OptionsBegin: l->mHaveGameOptions = true; break;
        case EeWonPrefsEventKind::OptionName:
            l->mOption = &p.mGameOptions[StringToWString(std::string(reinterpret_cast<const char*>(e->bytes), e->length))];
            break;
        case EeWonPrefsEventKind::OptionValue: l->mOption->Add(Text(e)); break;
        default: break;
        }
    }
};

static void PortLoadUserPrefs(Prefs& p, const char* inFile)
{
    p.RestoreDefaults();
    p.mReconnectInfoMap.clear();
    p.mIgnoredNames.clear();
    p.mFriends.clear();
    p.mLanNameInfoMap.clear();
    p.mDirectConnectMap.clear();
    p.mGameOptions.clear();
    std::string aBytes;
    if (!ReadWhole(inFile, aBytes)) {
        p.mGameOptions = p.mDefaultGameOptions;
        return;
    }
    UserLoad aLoad = {&p, NULL, NULL, false};
    if (ee_won_prefs_read_user(reinterpret_cast<const uint8_t*>(aBytes.data()), aBytes.length(), p.mCurrentUserId == 0,
                               UserLoad::Event, &aLoad) == EeWonPrefsRead::OtherVersion)
        return;
    if (!aLoad.mHaveGameOptions) p.mGameOptions = p.mDefaultGameOptions;
    p.mGlobalDirty = p.mLobbySoundEffects != p.mSavedLobbySoundEffects || p.mSavedLobbyMusic != p.mLobbyMusic;
    p.mUserDirty = false;
}

// ---- the rules

static long OrigFindUnusedUserId(const std::set<long>& inIds)
{
    if (inIds.empty()) return 1;
    std::set<long>::const_iterator anItr = inIds.begin();
    long anId = *anItr;
    while (anItr != inIds.end() && anId == *anItr) {
        ++anId;
        ++anItr;
    }
    return anId;
}

static std::string FileBytes(const char* inFile)
{
    std::string aBytes;
    ReadWhole(inFile, aBytes);
    return aBytes;
}
static void WriteFile(const char* inFile, const std::string& inBytes)
{
    FILE* f = fopen(inFile, "wb");
    if (f == NULL) return;
    fwrite(inBytes.data(), 1, inBytes.size(), f);
    fclose(f);
}

static Prefs Sample(int inVariant)
{
    Prefs p;
    p.mCurrentUserId = inVariant & 1 ? 3 : 0;
    p.mUsers[L"max"].mUserName = L"max";
    p.mUsers[L"max"].mUserId = 2;
    p.mUsers[L"max"].mLastUseTime = 1700000000;
    p.mUsers[L"max"].mValue04 = -1;
    p.mUsers[L"max"].mPassword = L"secret \xe9";
    p.mUsers[L"Zed"].mUserName = L"Zed";
    p.mUsers[L"Zed"].mUserId = 0xffff;
    p.mUsers[L"Zed"].mValue08 = 77;
    p.mLobbySoundEffects = (inVariant & 2) == 0;
    p.mLobbyMusic = (inVariant & 4) != 0;
    p.mNetworkAdapter = 0x0a000001;
    p.mLanNameInfoMap[L"Bob"] = 100;
    p.mLanNameInfoMap[L"\xc9ve"] = 300;
    p.mLanNameInfoMap[L"al"] = 200;
    p.mDirectConnectMap[Addr("\x01\x02\x03\x04\x05\x06", 6)] = L"server";
    p.mLanBroadcastPort = 33333;
    p.mReconnectInfoMap[Addr("\x09\x09\x01\x02\x03\x04", 6)].mLastUseTime = 5;
    p.mReconnectInfoMap[Addr("\x09\x09\x01\x02\x03\x04", 6)].mReconnectId = -6;
    p.mReconnectInfoMap[Addr("\x00\x09\x01\x02\x03\x04", 6)].mReconnectId = 1;
    p.mIgnoredNames.insert(L"troll");
    p.mIgnoredNames.insert(L"Imp");
    p.mFriends[0xdeadbeef] = L"pal";
    p.mFriends[4] = L"";
    p.mBlockedCDKeyIds.insert(7);
    p.mFilterDirtyWords = (inVariant & 8) != 0;
    p.mAnonymousToFriends = true;
    p.mTabSelect = L"LAN";
    p.mLastRoomName = L"room \x263a";
    p.mStagingSoundEffects = false;
    p.mConnectionSpeed = 56;
    p.mGameOptions[L"Epoch"].Add(L"Stone");
    p.mGameOptions[L"Epoch"].Add(L"Atomic");
    p.mGameOptions[L"Map\xe9"].Add(L"Large");
    if (inVariant & 16) p.mGameOptions[L"Empty"]; // no values: no count (the original's bug)
    return p;
}

int main()
{
    const char* kOrig = "won_persist_orig.dat";
    const char* kPort = "won_persist_port.dat";
    const char* kCut = "won_persist_cut.dat";

    for (int aVariant = 0; aVariant < 32; aVariant++) {
        // the global file
        {
            Prefs a = Sample(aVariant), b = Sample(aVariant);
            OrigSaveGlobalPrefs(a, kOrig);
            PortSaveGlobalPrefs(b, kPort);
            std::string aWant = FileBytes(kOrig);
            CHECK(FileBytes(kPort) == aWant && a == b);
            std::string aWrong = aWant;
            aWrong[6] = 3;
            for (size_t n = 0; n <= aWant.length() + 1; n++) {
                for (int aCase = 0; aCase < 2; aCase++) {
                    std::string aBytes = (aCase ? aWrong : aWant).substr(0, n);
                    if (n > aWant.length()) aBytes += '\x01';
                    WriteFile(kCut, aBytes);
                    Prefs x = Sample(aVariant ^ 6), y = Sample(aVariant ^ 6);
                    OrigLoadGlobalPrefs(x, kCut);
                    PortLoadGlobalPrefs(y, kCut);
                    CHECK(x == y);
                    if (!(x == y) && sFailures < 40) printf("  global variant %d cut %d case %d\n", aVariant, (int)n, aCase);
                    if (n == aWant.length() && aCase == 0) {
                        // the whole file loads what was saved
                        Prefs z = Sample(aVariant);
                        CHECK(x.mUsers == z.mUsers && x.mNetworkAdapter == z.mNetworkAdapter);
                    }
                }
            }
        }
        // the user file
        {
            Prefs a = Sample(aVariant), b = Sample(aVariant);
            OrigSaveUserPrefs(a, kOrig);
            PortSaveUserPrefs(b, kPort);
            std::string aWant = FileBytes(kOrig);
            CHECK(FileBytes(kPort) == aWant && a == b);
            std::string aWrong = aWant;
            aWrong[6] = 5;
            std::string aOdd = aWant;
            if (aOdd.length() > 30) aOdd[aOdd.length() - 30] = 2; // a setting or count byte of 2
            for (size_t n = 0; n <= aWant.length() + 1; n++) {
                for (int aCase = 0; aCase < 3; aCase++) {
                    std::string aBytes = (aCase == 1 ? aWrong : aCase == 2 ? aOdd : aWant).substr(0, n);
                    if (n > aWant.length()) aBytes += '\x01';
                    WriteFile(kCut, aBytes);
                    Prefs x = Sample(aVariant ^ 8), y = Sample(aVariant ^ 8);
                    OrigLoadUserPrefs(x, kCut);
                    PortLoadUserPrefs(y, kCut);
                    CHECK(x == y);
                    if (!(x == y) && sFailures < 40) printf("  user variant %d cut %d case %d\n", aVariant, (int)n, aCase);
                }
            }
        }
    }
    // a missing file
    {
        remove(kCut);
        Prefs x = Sample(1), y = Sample(1);
        OrigLoadUserPrefs(x, kCut);
        PortLoadUserPrefs(y, kCut);
        OrigLoadGlobalPrefs(x, kCut);
        PortLoadGlobalPrefs(y, kCut);
        CHECK(x == y);
    }
    remove(kOrig);
    remove(kPort);
    remove(kCut);

    // the rules
    for (long anId = -2; anId <= 12; anId++) {
        char aWant[50];
        sprintf(aWant, "_wonuser%d.dat", (int)anId);
        std::string aGot;
        ee_won_prefs_user_file_name((int32_t)anId, AppendBytes, &aGot);
        CHECK(aGot == aWant);
    }
    const long kIdSets[][5] = {{0, 0, 0, 0, 0}, {1, 2, 3, 0, 0}, {1, 2, 4, 0, 0}, {5, 6, 9, 0, 0}, {-1, 0, 1, 2, 7}};
    const int kIdCounts[] = {0, 3, 3, 3, 5};
    for (int s = 0; s < 5; s++) {
        std::set<long> anIds(kIdSets[s], kIdSets[s] + kIdCounts[s]);
        std::vector<int32_t> aSorted(anIds.begin(), anIds.end());
        CHECK(ee_won_prefs_unused_user_id(aSorted.empty() ? NULL : &aSorted[0], aSorted.size()) ==
              OrigFindUnusedUserId(anIds));
    }
    // pruning: the original's multimap walk over 40..60 users with some ids 0 and repeated times
    for (int aCount = 40; aCount <= 60; aCount++)
        for (int aSeed = 0; aSeed < 4; aSeed++) {
            std::vector<int32_t> anIds;
            std::vector<int64_t> aTimes;
            for (int i = 0; i < aCount; i++) {
                anIds.push_back((aSeed == 1 && i % 7 == 3) ? 0 : i + 1);
                aTimes.push_back(aSeed == 2 ? 5 : (int64_t)((i * 37 + aSeed * 11) % 23));
            }
            // the original over a vector standing for the map (its order)
            std::vector<bool> aGone(aCount, false), aFile(aCount, false);
            size_t aSize = aCount;
            if (aSize > 50) {
                std::multimap<long, int> aByTime;
                for (int i = 0; i < aCount; i++) {
                    if (anIds[i] == 0) {
                        aGone[i] = true;
                        aSize--;
                    } else
                        aByTime.insert(std::make_pair((long)aTimes[i], i));
                }
                for (std::multimap<long, int>::iterator t = aByTime.begin(); t != aByTime.end() && aSize > 50; ++t) {
                    aGone[t->second] = aFile[t->second] = true;
                    aSize--;
                }
            }
            std::vector<uint8_t> aDrop(aCount, 9);
            ee_won_prefs_prune_users(&anIds[0], &aTimes[0], aCount, &aDrop[0]);
            bool aSame = true;
            for (int i = 0; i < aCount; i++) aSame &= aDrop[i] == (aGone[i] ? (aFile[i] ? 2 : 1) : 0);
            CHECK(aSame);
            // LAN names: the same walk without the id rule
            std::vector<bool> aLanGone(aCount, false);
            if (aCount > 50) {
                std::multimap<long, int> aByTime;
                for (int i = 0; i < aCount; i++) aByTime.insert(std::make_pair((long)aTimes[i], i));
                size_t aLeft = aCount;
                for (std::multimap<long, int>::iterator t = aByTime.begin(); t != aByTime.end() && aLeft > 50; ++t) {
                    aLanGone[t->second] = true;
                    aLeft--;
                }
            }
            bool aDropLan[64];
            ee_won_prefs_prune_lan_names(&aTimes[0], aCount, aDropLan);
            aSame = true;
            for (int i = 0; i < aCount; i++) aSame &= aDropLan[i] == aLanGone[i];
            CHECK(aSame);
            // the most recent LAN name
            long aLast = 0;
            int aChosen = -1;
            for (int i = 0; i < aCount; i++)
                if (aLast == 0 || aTimes[i] > aLast) {
                    aChosen = i;
                    aLast = (long)aTimes[i];
                }
            CHECK(ee_won_prefs_recent_lan_name(&aTimes[0], aCount) == aChosen);
        }
    CHECK(ee_won_prefs_recent_lan_name(NULL, 0) == -1);
    // the default LAN name's number (GetDefaultLanName's CRC of the host's four bytes)
    const uint32_t kHosts[] = {0, 1, 0x0a000001, 0xc0a80105, 0xffffffff};
    for (size_t h = 0; h < sizeof(kHosts) / sizeof(kHosts[0]); h++) {
        long anAddress = (long)kHosts[h];
        CRC16 aCRC;
        aCRC.Put(&anAddress, 4);
        CHECK(ee_won_prefs_default_lan_number(kHosts[h]) == (unsigned short)(aCRC.Get() % 10000));
    }

    printf("won_persist_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
