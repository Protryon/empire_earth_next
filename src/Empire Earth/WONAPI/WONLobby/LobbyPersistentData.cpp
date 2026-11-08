// C:\EEX\Empire Earth\WONAPI\WONLobby\LobbyPersistentData.cpp (file name inferred: no __FILE__)
// LobbyPersistentData (RTTI name; 1164-byte object, constructor 0x75bcb0, destructor 0x75c2c0, vtable 0x851c28):
// the lobby's saved settings. _wonlobbypersistent.dat holds the login users and global sound/adapter options;
// _wonuser<N>.dat holds one user's reconnect ids, ignore list, friends, blocked CD keys, options, LAN names,
// direct-connect addresses and game filters

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <exception>
#include <stdio.h>
#include <stdlib.h>
#include "ee/wonapi/WONCRC16.h"
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include <time.h>
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyParser.h"

namespace WONAPI {
// FUNCTION: WON 0x0075e6e0
// Returns the preferences file name of a lobby user, _wonuser<id>.dat
std::string LobbyPersistentData::GetUserFileName(long inUserId)
{
    // The original's frame holds a 52-byte area; the 50-character buffer size is inferred
    char aFileName[50];
    sprintf(aFileName, "_wonuser%d.dat", inUserId);
    return aFileName;
}
// FUNCTION: WON 0x0075e770
// Returns the first free user id after the run of consecutive ids starting at the smallest (1 when none exist)
long LobbyPersistentData::FindUnusedUserId()
{
    if (this->mUserIds.empty())
        return 1;
    std::set<long>::iterator anItr = this->mUserIds.begin();
    long anId = *anItr;
    while (anItr != this->mUserIds.end() && anId == *anItr)
    {
        ++anId;
        ++anItr;
    }
    return anId;
}
// FUNCTION: WON 0x0075f6a0
// Reads the "WONPER" file tag and returns the file version that follows (0 for a bad tag or a read error)
long LobbyPersistentData::ReadFileHeader(FileReader& inReader)
{
    try
    {
        char aHeader[7];
        aHeader[6] = 0;
        inReader.ReadBytes(aHeader, 6);
        if (strcmp(aHeader, "WONPER") == 0)
            return inReader.ReadLong();
    }
    catch (FileReaderException&)
    {
    }
    return 0;
}
// FUNCTION: WON 0x0075f750
// Writes the "WONPER" file tag and the given version, ignoring write errors
void LobbyPersistentData::WriteFileHeader(FileWriter& outWriter, long inVersion)
{
    try
    {
        outWriter.WriteBytes("WONPER", 6);
        outWriter.WriteLong(inVersion);
    }
    catch (FileWriterException&)
    {
    }
}
}

namespace WONAPI {
long gLobbyGlobalPrefsVersion = 2;
long gLobbyUserPrefsVersion = 4;


// FUNCTION: WON 0x0075baa0
// Adds a value to the option set unless it is already present, keeping the values in insertion order
void LobbyOptionSet::Add(const GUIString& inValue)
{
    std::pair<StringMap::iterator, bool> aResult = this->mIndex.insert(StringMap::value_type(inValue, this->mValues.end()));
    if (!aResult.second)
        return;
    this->mValues.push_back(inValue);
    StringList::iterator anItr = this->mValues.end();
    --anItr;
    aResult.first->second = anItr;
}
// FUNCTION: WON 0x0075bc20
// Removes a value from the option set; returns whether it was present
bool LobbyOptionSet::Erase(const GUIString& inValue)
{
    StringMap::iterator anItr = this->mIndex.find(inValue);
    if (anItr != this->mIndex.end())
    {
        this->mValues.erase(anItr->second);
        this->mIndex.erase(anItr);
        return true;
    }
    return false;
}

LobbyPersistentData* GetLobbyPersistentData()
{
    return gLobbyData.get() != NULL ? gLobbyData->mPersistentData.get() : NULL;
}


// FUNCTION: WON 0x0075f7b0
// Loads the saved login users (names, ids, encrypted passwords) and the lobby sound, music and network adapter
// settings from _wonlobbypersistent.dat, muting the button click sound to match
void LobbyPersistentData::LoadGlobalPrefs()
{
    FileReader aReader;
    if (!aReader.Open("_wonlobbypersistent.dat"))
        return;
    this->mUsers.clear();
    this->mNetworkAdapter = 0;
    try
    {
        if (this->ReadFileHeader(aReader) != gLobbyGlobalPrefsVersion)
            return;
        unsigned short aCount = aReader.ReadShort();
        for (int i = 0; i < aCount; ++i)
        {
            GUIString aName = this->ReadWideString(aReader);
            std::pair<LobbyUserInfoMap::iterator,bool> aPair = this->mUsers.insert(LobbyUserInfoMap::value_type(aName, LobbyUserInfo()));
            LobbyUserInfo& aUser = aPair.first->second;
            aUser.mUserName = aName;
            aUser.mLastUseTime = aReader.ReadLong();
            aUser.mUserId = aReader.ReadShort();
            aUser.mValue04 = aReader.ReadLong();
            aUser.mValue08 = aReader.ReadLong();
            aUser.mPassword = this->ReadEncryptedString(aReader);
            this->mUserIds.insert(aUser.mUserId);
        }
        this->mLobbySoundEffects = this->mSavedLobbySoundEffects = aReader.ReadByte() != 0;
        ::LobbyGlobal_ButtonClick_Sound->SetMuted(!this->mLobbySoundEffects);
        this->mLobbyMusic = this->mSavedLobbyMusic = aReader.ReadByte() != 0;
        // The network adapter is stored as its numeric IPv4 address; the click sound is used without a null check
        this->mNetworkAdapter = aReader.ReadLong();
    }
    catch (FileReaderException&)
    {
    }
    this->mGlobalDirty = false;
}

// FUNCTION: WON 0x0075faf0
// Writes the login users and the lobby sound, music and network adapter settings to _wonlobbypersistent.dat
void LobbyPersistentData::SaveGlobalPrefs()
{
    FileWriter aWriter;
    if (!aWriter.Open("_wonlobbypersistent.dat", false))
        return;
    try
    {
        this->WriteFileHeader(aWriter, gLobbyGlobalPrefsVersion);
        aWriter.WriteShort(this->mUsers.size());
        for (LobbyUserInfoMap::iterator anItr = this->mUsers.begin(); anItr != this->mUsers.end(); ++anItr)
        {
            LobbyUserInfo& aUser = anItr->second;
            this->WriteWideString(aWriter, aUser.mUserName);
            aWriter.WriteLong(aUser.mLastUseTime);
            aWriter.WriteShort(aUser.mUserId);
            aWriter.WriteLong(aUser.mValue04);
            aWriter.WriteLong(aUser.mValue08);
            this->WriteEncryptedString(aWriter, aUser.mPassword);
        }
        aWriter.WriteByte(this->mLobbySoundEffects ? 1 : 0);
        aWriter.WriteByte(this->mLobbyMusic ? 1 : 0);
        aWriter.WriteLong(this->mNetworkAdapter);
    }
    catch (FileWriterException&)
    {
    }
    // The saved sound and music states are updated even when a write failed
    this->mSavedLobbySoundEffects = this->mLobbySoundEffects;
    this->mSavedLobbyMusic = this->mLobbyMusic;
    this->mGlobalDirty = false;
}


// FUNCTION: WON 0x0075c720
// Resets the user's lobby options to their defaults: Quick Play tab for an internet user, LAN tab and the
// default LAN name for the LAN user, all sounds and confirmations on, broadcast port 8585
void LobbyPersistentData::RestoreDefaults()
{
    this->mLastServerName = GetEmptyGUIStringData();
    this->mLastRoomName = GetEmptyGUIStringData();
    if (this->mCurrentUserId != 0)
        this->mTabSelect = ::LobbyGlobal_QuickPlay_String;
    else
    {
        this->mTabSelect = ::LobbyGlobal_Lan_String;
        this->mLanName = this->GetDefaultLanName();
    }
    this->mFilterDirtyWords = true;
    this->mDoAutoAFK = true;
    this->mCrossPromotion = true;
    this->mLobbySoundEffects = true;
    this->mLobbyMusic = true;
    this->mShowConfirmations = true;
    this->mChatSoundEffects = true;
    this->mStagingSoundEffects = true;
    this->mAutoPingGames = true;
    this->mShowGamesInProgress = true;
    this->mAnonymousToFriends = false;
    this->mLanBroadcastPort = 8585;
    this->mConnectionSpeed = 25;
    this->mNetworkAdapter = 0;
}



// FUNCTION: WON 0x0075c910
// Records a login (name and password) as the most recent use of a saved user, giving a new user a free id and
// deleting any stale preferences file; with inExistingOnly returns NULL for a user never seen before
LobbyUserInfo* LobbyAddUser(const GUIString& inUserName, const GUIString& inPassword, bool inExistingOnly)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return NULL;
    if (!inExistingOnly)
        aData->PruneUsers();
    LobbyUserInfoMap::iterator anItr = aData->mUsers.insert(
        LobbyUserInfoMap::value_type(inUserName, LobbyUserInfo())).first;
    LobbyUserInfo& aUser = anItr->second;
    if (inExistingOnly && aUser.mLastUseTime == 0)
    {
        aData->mUsers.erase(anItr);
        return NULL;
    }
    if (aUser.mUserId == 0)
    {
        aUser.mUserId = aData->FindUnusedUserId();
        aData->mUserIds.insert(aUser.mUserId);
        WONFile aFile(aData->GetUserFileName(aUser.mUserId));
        aFile.Delete(true);
    }
    aUser.mLastUseTime = time(NULL);
    aUser.mUserName = inUserName;
    aUser.mPassword = inPassword;
    aData->mGlobalDirty = true;
    return &aUser;
}

// The age index is a multimap keyed by last use time (equal times kept), each entry pointing back into mUsers
// FUNCTION: WON 0x0075eb30
// Keeps at most 50 saved users: drops users without an id, then deletes the least recently used ones and their files
void LobbyPersistentData::PruneUsers()
{
    if (this->mUsers.size() <= 50)
        return;
    typedef std::multimap<long, LobbyUserInfoMap::iterator> UserTimeMap;
    UserTimeMap aTimes;
    LobbyUserInfoMap::iterator anItr = this->mUsers.begin();
    while (anItr != this->mUsers.end())
    {
        if (anItr->second.mUserId == 0)
            this->mUsers.erase(anItr++);
        else
        {
            aTimes.insert(UserTimeMap::value_type(anItr->second.mLastUseTime, anItr));
            ++anItr;
        }
    }
    UserTimeMap::iterator aTime = aTimes.begin();
    while (aTime != aTimes.end() && this->mUsers.size() > 50)
    {
        LobbyUserInfo& aUser = aTime->second->second;
        WONFile aFile(this->GetUserFileName(aUser.mUserId));
        aFile.Delete(true);
        this->mUserIds.erase(aUser.mUserId);
        this->mUsers.erase(aTime->second);
        aTime++;
    }
}

// FUNCTION: WON 0x0075d460
// Adds a player to the current user's friend list unless the user id is already there; returns the friend entry
LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inUserId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return NULL;
    std::pair<LobbyFriendMap::iterator,bool> aPair = aData->mFriends.insert(
        LobbyFriendMap::value_type(inUserId, SmartPtr<LobbyFriend>()));
    if (aPair.first->second.get() == NULL)
    {
        aPair.first->second = new LobbyFriend(inName, inUserId);
        aData->mUserDirty = true;
    }
    return aPair.first->second.get();
}
// FUNCTION: WON 0x0075d2c0
// Removes a user id from the friend list; the dirty flag is set to whether one was removed, so a miss clears it
void LobbyRemoveFriend(unsigned long inUserId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData != NULL)
        aData->mUserDirty = aData->mFriends.erase(inUserId) > 0;
}

}

namespace WONAPI {

// FUNCTION: WON 0x0075ee80
// Reads a length-prefixed 8-bit string from a preferences file
GUIString LobbyPersistentData::ReadByteString(FileReader& inReader)
{
    unsigned short aLength = inReader.ReadShort();
    GUIString aString(new GUIStringData(aLength));
    for (int i = 0; i < aLength; ++i)
        aString.append((wchar_t)inReader.ReadByte());
    return aString;
}
// FUNCTION: WON 0x0075f000
// Reads a length-prefixed 16-bit (unencrypted) string from a preferences file
GUIString LobbyPersistentData::ReadWideString(FileReader& inReader)
{
    unsigned short aLength = inReader.ReadShort();
    GUIString aString(new GUIStringData(aLength));
    for (int i = 0; i < aLength; ++i)
        aString.append((wchar_t)inReader.ReadShort());
    return aString;
}
// FUNCTION: WON 0x0075f3d0
// Writes a string as a length-prefixed 8-bit string
void LobbyPersistentData::WriteByteString(FileWriter& outWriter, const GUIString& inText)
{
    std::string aString = (std::string)inText;
    outWriter.WriteShort(aString.length());
    outWriter.WriteBytes(aString.c_str(), aString.length());
}
// FUNCTION: WON 0x0075f4e0
// Writes a string as a length-prefixed 16-bit (unencrypted) string
void LobbyPersistentData::WriteWideString(FileWriter& outWriter, const GUIString& inText)
{
    outWriter.WriteShort(inText.length());
    for (int i = 0; i < inText.length(); ++i)
        outWriter.WriteShort(inText[i]);
}

// FUNCTION: WON 0x0075f190
// Reads and decrypts a saved password with the machine-bound key; returns an empty string if it cannot be decoded
GUIString LobbyPersistentData::ReadEncryptedString(FileReader& reader)
{
    unsigned short length = reader.ReadShort();
    WriteBuffer storage(0, true);
    storage.Reserve(length); // grows the capacity only; the length stays 0
    reader.ReadBytes(storage.mData, length);
    ByteBufferPtr decrypted = this->mEncryptKey.Decrypt(storage.mData, length);
    if (decrypted.get() == NULL)
        return GUIString("");
    ReadBuffer decoded(decrypted->data(), decrypted->length());
    try
    {
        std::wstring text;
        decoded.ReadWString(text, 2);
        return GUIString(text);
    }
    catch (ReadBufferException&)
    {
    }
    return GUIString("");
}

// FUNCTION: WON 0x0075f530
// Encrypts a password with the machine-bound key and writes it length-prefixed (length 0 if encryption fails)
void LobbyPersistentData::WriteEncryptedString(FileWriter& writer, const GUIString& inText)
{
    WriteBuffer storage(0, true);
    std::wstring text = (std::wstring)inText;
    storage.AppendWString(text, 2);
    ByteBufferPtr encrypted = this->mEncryptKey.Encrypt(storage.mData, storage.mLength);
    writer.WriteShort(encrypted.get() != NULL ? encrypted->length() : 0);
    writer.WriteBytes(encrypted.get() != NULL ? encrypted->data() : NULL,
                      encrypted.get() != NULL ? encrypted->length() : 0);
}
// A file with another version returns early: the game filters then stay empty and the dirty flags unchanged
// FUNCTION: WON 0x0075fc70
// Loads the current user's _wonuser file: reconnect ids, ignored names, friends, blocked CD keys (loaded and saved
// but never filled by the lobby), options, last server/room, sounds and game filters; LAN data for the LAN user
void LobbyPersistentData::LoadUserPrefs()
{
    this->RestoreDefaults();
    this->mReconnectInfoMap.clear();
    this->mIgnoredNames.clear();
    this->mFriends.clear();
    this->mLanNameInfoMap.clear();
    this->mDirectConnectMap.clear();
    this->mGameOptions.clear();
    bool haveGameOptions = false;
    int i;
    FileReader reader;
    std::string fileName = this->GetUserFileName(this->mCurrentUserId);
    if (!reader.Open(fileName.c_str()))
    {
        this->mGameOptions = this->mDefaultGameOptions;
        return;
    }
    try
    {
        if (this->ReadFileHeader(reader) != gLobbyUserPrefsVersion)
            return;
        if (this->mCurrentUserId == 0)
            this->ReadLanPrefs(reader);
        unsigned short count = reader.ReadShort();
        for (i = 0; i < count; ++i)
        {
            unsigned char bytes[6];
            reader.ReadBytes(bytes, 6);
            IPAddr address;
            address.SetSixByte(bytes);
            LobbyReconnectInfo& info = this->mReconnectInfoMap.insert(
                std::map<IPAddr, LobbyReconnectInfo>::value_type(address, LobbyReconnectInfo())).first->second;
            info.mLastUseTime = reader.ReadLong();
            info.mReconnectId = reader.ReadLong();
        }
        count = reader.ReadShort();
        for (i = 0; i < count; ++i)
            this->mIgnoredNames.insert(this->ReadWideString(reader));
        count = reader.ReadShort();
        for (i = 0; i < count; ++i)
        {
            std::wstring name = (std::wstring)this->ReadWideString(reader);
            unsigned long userId = reader.ReadLong();
            SmartPtr<LobbyFriend> aFriend = new LobbyFriend(GUIString(name), userId);
            this->mFriends.insert(LobbyFriendMap::value_type(userId, aFriend));
        }
        count = reader.ReadShort();
        for (i = 0; i < count; ++i)
            this->mBlockedCDKeyIds.insert(reader.ReadLong());
        this->mFilterDirtyWords = reader.ReadByte() == 1;
        this->mDoAutoAFK = reader.ReadByte() == 1;
        this->mCrossPromotion = reader.ReadByte() == 1;
        this->mAutoPingGames = reader.ReadByte() == 1;
        this->mAnonymousToFriends = reader.ReadByte() == 1;
        this->mShowGamesInProgress = reader.ReadByte() == 1;
        this->mTabSelect = this->ReadWideString(reader);
        this->mLastServerName = this->ReadWideString(reader);
        this->mLastRoomName = this->ReadWideString(reader);
        this->mLobbySoundEffects = reader.ReadByte() == 1;
        this->mLobbyMusic = reader.ReadByte() == 1;
        this->mChatSoundEffects = reader.ReadByte() == 1;
        this->mStagingSoundEffects = reader.ReadByte() == 1;
        this->mShowConfirmations = reader.ReadByte() == 1;
        this->mConnectionSpeed = reader.ReadShort();
        count = reader.ReadShort();
        haveGameOptions = true;
        for (i = 0; i < count; ++i)
        {
            GUIString optionName = this->ReadByteString(reader);
            LobbyOptionSet& option = this->mGameOptions[optionName];
            unsigned short numValues = reader.ReadShort();
            for (int j = 0; j < numValues; ++j)
                option.Add(this->ReadWideString(reader));
        }
    }
    catch (FileReaderException&) {}
    if (!haveGameOptions)
        this->mGameOptions = this->mDefaultGameOptions;
    this->mGlobalDirty = this->mLobbySoundEffects != this->mSavedLobbySoundEffects || this->mSavedLobbyMusic != this->mLobbyMusic;
    this->mUserDirty = false;
}

// FUNCTION: WON 0x00760b10
// Reads the LAN user's recent LAN names (selecting the most recent), direct-connect addresses and broadcast port
void LobbyPersistentData::ReadLanPrefs(FileReader& reader)
{
    int count = reader.ReadShort();
    for (int nameIndex = 0; nameIndex < count; ++nameIndex)
    {
        GUIString name = this->ReadWideString(reader);
        long lastUseTime = reader.ReadLong();
        this->mLanNameInfoMap[name] = lastUseTime;
    }
    this->SelectRecentLanName();
    count = reader.ReadShort();
    for (int addressIndex = 0; addressIndex < count; ++addressIndex)
    {
        IPAddr address;
        unsigned char bytes[6];
        reader.ReadBytes(bytes, 6);
        address.SetSixByte(bytes);
        GUIString name = this->ReadWideString(reader);
        this->mDirectConnectMap[address] = name;
    }
    this->mLanBroadcastPort = reader.ReadShort();
}

// FUNCTION: WON 0x00760da0
// Writes the LAN user's recent LAN names, direct-connect addresses and broadcast port
void LobbyPersistentData::WriteLanPrefs(FileWriter& writer)
{
    writer.WriteShort((unsigned short)this->mLanNameInfoMap.size());
    for (std::map<GUIString, long, GUIStringLessNoCase>::const_iterator i = this->mLanNameInfoMap.begin(); i != this->mLanNameInfoMap.end(); ++i)
    {
        this->WriteWideString(writer, i->first);
        writer.WriteLong(i->second);
    }
    writer.WriteShort((unsigned short)this->mDirectConnectMap.size());
    for (std::map<IPAddr, GUIString>::const_iterator j = this->mDirectConnectMap.begin(); j != this->mDirectConnectMap.end(); ++j)
    {
        writer.WriteBytes(j->first.GetSixByte(), 6);
        this->WriteWideString(writer, j->second);
    }
    writer.WriteShort(this->mLanBroadcastPort);
}
// FUNCTION: WON 0x00760770
// Writes the current user's _wonuser file (the counterpart of LoadUserPrefs) and clears the user dirty flag
void LobbyPersistentData::SaveUserPrefs()
{
    FileWriter writer;
    std::string fileName = this->GetUserFileName(this->mCurrentUserId);
    if (!writer.Open(fileName.c_str(), false))
        return;
    try
    {
        this->WriteFileHeader(writer, gLobbyUserPrefsVersion);
        if (this->mCurrentUserId == 0)
            this->WriteLanPrefs(writer);
        writer.WriteShort((unsigned short)this->mReconnectInfoMap.size());
        for (std::map<IPAddr, LobbyReconnectInfo>::const_iterator a = this->mReconnectInfoMap.begin(); a != this->mReconnectInfoMap.end(); ++a)
        {
            const LobbyReconnectInfo& anInfo = a->second;
            writer.WriteBytes(a->first.GetSixByte(), 6);
            writer.WriteLong(anInfo.mLastUseTime);
            writer.WriteLong(anInfo.mReconnectId);
        }
        writer.WriteShort((unsigned short)this->mIgnoredNames.size());
        for (LobbyIgnoredNameSet::const_iterator n = this->mIgnoredNames.begin(); n != this->mIgnoredNames.end(); ++n)
            this->WriteWideString(writer, *n);
        writer.WriteShort((unsigned short)this->mFriends.size());
        for (LobbyFriendMap::const_iterator f = this->mFriends.begin(); f != this->mFriends.end(); ++f)
        {
            this->WriteWideString(writer, f->second->mName);
            writer.WriteLong(f->first);
        }
        writer.WriteShort((unsigned short)this->mBlockedCDKeyIds.size());
        for (std::set<unsigned long>::const_iterator u = this->mBlockedCDKeyIds.begin(); u != this->mBlockedCDKeyIds.end(); ++u)
            writer.WriteLong(*u);
        writer.WriteByte(this->mFilterDirtyWords ? 1 : 0);
        writer.WriteByte(this->mDoAutoAFK ? 1 : 0);
        writer.WriteByte(this->mCrossPromotion ? 1 : 0);
        writer.WriteByte(this->mAutoPingGames ? 1 : 0);
        writer.WriteByte(this->mAnonymousToFriends ? 1 : 0);
        writer.WriteByte(this->mShowGamesInProgress ? 1 : 0);
        this->WriteWideString(writer, this->mTabSelect);
        this->WriteWideString(writer, this->mLastServerName);
        this->WriteWideString(writer, this->mLastRoomName);
        writer.WriteByte(this->mLobbySoundEffects ? 1 : 0);
        writer.WriteByte(this->mLobbyMusic ? 1 : 0);
        writer.WriteByte(this->mChatSoundEffects ? 1 : 0);
        writer.WriteByte(this->mStagingSoundEffects ? 1 : 0);
        writer.WriteByte(this->mShowConfirmations ? 1 : 0);
        writer.WriteShort(this->mConnectionSpeed);
        writer.WriteShort((unsigned short)this->mGameOptions.size());
        for (std::map<GUIString, LobbyOptionSet, GUIStringLessNoCase>::const_iterator o = this->mGameOptions.begin(); o != this->mGameOptions.end(); ++o)
        {
            this->WriteByteString(writer, o->first);
            // A filter with no values writes no count at all, which the loader does not expect: the filters after it
            // are misread on the next load
            const LobbyOptionSet& aSet = o->second;
            unsigned long aCount = aSet.mIndex.size();
            if (aCount != 0)
            {
                writer.WriteShort((unsigned short)aCount);
                for (LobbyOptionSet::StringList::const_iterator v = aSet.mValues.begin(); v != aSet.mValues.end(); ++v)
                    this->WriteWideString(writer, *v);
            }
        }
    }
    catch (FileWriterException&) {}
    this->mUserDirty = false;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0075d940
// Remembers the reconnect id for a server address with the current time and saves the user's preferences
void LobbySetReconnectId(const IPAddr& inAddress, int inReconnectId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    std::pair<std::map<IPAddr, LobbyReconnectInfo>::iterator,bool> aPair = aData->mReconnectInfoMap.insert(
        std::map<IPAddr, LobbyReconnectInfo>::value_type(inAddress, LobbyReconnectInfo()));
    LobbyReconnectInfo& anInfo = aPair.first->second;
    anInfo.mLastUseTime = time(NULL);
    anInfo.mReconnectId = inReconnectId;
    aData->SaveUserPrefs();
}
// FUNCTION: WON 0x0075db00
// Returns the saved reconnect id for a server address, or 0 if none
int LobbyGetReconnectId(const IPAddr& inAddress)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return 0;
    std::map<IPAddr, LobbyReconnectInfo>::iterator anItr = aData->mReconnectInfoMap.find(inAddress);
    return anItr == aData->mReconnectInfoMap.end() ? 0 : anItr->second.mReconnectId;
}
// FUNCTION: WON 0x0075e040
// Adds comma-separated values to a default game filter column; skips a column that already has values unless
// inAppend is set
void LobbyAddGameOption(const GUIString& inName, const GUIString& inValues, bool inAppend)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    LobbyOptionSet& anOptions = aData->mDefaultGameOptions[inName];
    if (!anOptions.mIndex.empty() && !inAppend)
        return;
    std::wstring aValues = inValues;
    StringParser aParser(aValues.c_str());
    while (true) {
        std::wstring aValue;
        aParser.ReadString(aValue, false);
        if (aValue.empty())
            break;
        anOptions.Add(GUIString(new GUIStringData(aValue)));
        if (!aParser.CheckNextChar(','))
            break;
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x0075d100
// Adds a player name to or removes it from the ignore list; the dirty flag is set to whether the list changed
void LobbySetNameIgnored(const GUIString& inName, bool inIgnore)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    bool aDirty;
    if (inIgnore)
        aDirty = aData->mIgnoredNames.insert(inName).second;
    else
        aDirty = aData->mIgnoredNames.erase(inName) > 0;
    aData->mUserDirty = aDirty;
}
// FUNCTION: WON 0x0075d6c0
// Makes a non-empty name the current LAN name, stamping its use time and keeping the 50 most recent names
void LobbySetLanName(const GUIString& inName)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL || inName.mData->mLength == 0 ||
        inName.mData->Compare(aData->mLanName.mData.get()) == 0) return;
    long aTime = time(NULL);
    aData->mLanNameInfoMap[inName] = aTime;
    aData->mLanName = inName;
    aData->PruneLanNames();
    aData->mUserDirty = true;
}
// FUNCTION: WON 0x0075d770
// Forgets the current LAN name and falls back to the most recently used remaining one
void LobbyRemoveLanName()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    LobbyLanNameInfoMap::iterator anItr = aData->mLanNameInfoMap.find(aData->mLanName);
    if (anItr == aData->mLanNameInfoMap.end())
        return;
    aData->mLanNameInfoMap.erase(anItr);
    aData->SelectRecentLanName();
    aData->mUserDirty = true;
}
// FUNCTION: WON 0x0075d7f0
// Saves a named direct-connect address
void LobbyAddDirectConnect(const IPAddr& inAddress, const GUIString& inName)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    aData->mDirectConnectMap[inAddress] = inName;
    aData->mUserDirty = true;
}
// FUNCTION: WON 0x0075d870
// Forgets a saved direct-connect address
void LobbyRemoveDirectConnect(const IPAddr& inAddress)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    LobbyDirectConnectMap::iterator anItr = aData->mDirectConnectMap.find(inAddress);
    if (anItr != aData->mDirectConnectMap.end()) {
        aData->mDirectConnectMap.erase(anItr);
        aData->mUserDirty = true;
    }
}
// FUNCTION: WON 0x0075db70
// Remembers the last server and room the user joined
void LobbySetLastRoom(const GUIString& inServer, const GUIString& inGroup)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    if (inServer.mData->Compare(aData->mLastServerName.mData.get()) == 0 &&
        inGroup.mData->Compare(aData->mLastRoomName.mData.get()) == 0) return;
    aData->mLastServerName = inServer;
    aData->mLastRoomName = inGroup;
    aData->mUserDirty = true;
}
// FUNCTION: WON 0x0075df00
// Resets the user's game filters to the configured defaults
void LobbyResetFilters()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    // The original copies the option sets memberwise, stored list iterators included
    aData->mGameOptions = aData->mDefaultGameOptions;
    aData->mUserDirty = true;
}
// FUNCTION: WON 0x0075e7e0
// Keeps at most 50 recent LAN names, dropping the least recently used
void LobbyPersistentData::PruneLanNames()
{
    GetLobbyPersistentData();
    if (this->mLanNameInfoMap.size() <= 50) return;
    typedef std::multimap<long, LobbyLanNameInfoMap::iterator> TimeMap;
    TimeMap aTimes;
    for (LobbyLanNameInfoMap::iterator anItr = this->mLanNameInfoMap.begin();
         anItr != this->mLanNameInfoMap.end(); ++anItr)
        aTimes.insert(TimeMap::value_type(anItr->second, anItr));
    TimeMap::iterator aTime = aTimes.begin();
    while (aTime != aTimes.end() && this->mLanNameInfoMap.size() > 50)
    {
        this->mLanNameInfoMap.erase(aTime->second);
        aTime++;
    }
}
// FUNCTION: WON 0x0075ea40
// Makes the most recently used saved LAN name current, or the default name when none is saved
void LobbyPersistentData::SelectRecentLanName()
{
    GetLobbyPersistentData();
    this->mLanName = this->GetDefaultLanName();
    long aLastTime = 0;
    LobbyLanNameInfoMap::iterator anItr;
    for (anItr = this->mLanNameInfoMap.begin();
         anItr != this->mLanNameInfoMap.end(); ++anItr) {
        if (aLastTime == 0 || anItr->second > aLastTime) {
            this->mLanName = anItr->first;
            aLastTime = anItr->second;
        }
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x0075c520
// Returns the default LAN player name, with a number derived from the local IP address appended unless the
// lobby configuration turns that off
GUIString LobbyPersistentData::GetDefaultLanName()
{
    GUIString aName = ::LobbyOptions_DefaultName_String;
    LobbyConfig* aConfig = GetLobbyConfig();
    if (aConfig != NULL && !aConfig->mAppendNumberToDefaultLanName)
        return aName;
    long anAddress = IPAddr::GetLocalAddr().GetHost();
    CRC16 aCRC;
    aCRC.Put(&anAddress,4);
    char aNumber[256];
    unsigned short aValue = aCRC.Get() % 10000;
    _itoa(aValue, aNumber, 10);
    aName.append(GUIString(aNumber));
    return aName;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0075bcb0
// Creates the lobby's saved settings with default options and a password key bound to this machine
LobbyPersistentData::LobbyPersistentData()
{
    WriteBuffer aKey(0,true);
    aKey.Reserve(8);
    aKey.AppendLong(0x22fa655f);
    aKey.AppendLong(CDKey::GetMachineId(NULL));
    this->mEncryptKey.SetKey(aKey.data(),aKey.length());
    // The original does not set mCurrentUserId before this reset
    this->RestoreDefaults();
    this->mLanBroadcastPort = 8585;
    this->mGlobalDirty = false;
    this->mUserDirty = false;
    this->mSavedLobbySoundEffects = true;
    this->mSavedLobbyMusic = true;
}
}

// Empties an option set (value index and ordered value list); no copy of its own was found in the original,
// it has the same shape as the SDK's ServerListFilterSet::clear
namespace WONAPI {
void LobbyOptionSet::clear()
{
    this->mIndex.clear();
    this->mValues.clear();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0075c140
// (public: __thiscall std::map<unsigned long,class WONAPI::SmartPtr<class WONAPI::LobbyFriend>,struct std::less<unsigned long>,class std::allocator<struc)
// TEMPLATE: WON 0x0075c900
// (class WONAPI::LobbyPersistentData * __cdecl WONAPI::GetLobbyPersistentData(void)    [?GetLobbyPersistentData@WONAPI@@YAPAVLobbyPersistentData@1@XZ])
// TEMPLATE: WON 0x0075cc90
// (public: __thiscall WONAPI::LobbyUserInfo::~LobbyUserInfo(void)    [??1LobbyUserInfo@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0075cd20
// (public: __thiscall std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>::~pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUs)
// TEMPLATE: WON 0x0075dd20
// (public: __thiscall WONAPI::LobbyOptionSet::LobbyOptionSet(void)    [??0LobbyOptionSet@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x0075ddb0
// (public: __thiscall WONAPI::LobbyOptionSet::~LobbyOptionSet(void)    [??1LobbyOptionSet@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0075bfb0
// (public: virtual void * __thiscall WONAPI::LobbyPersistentData::`scalar deleting destructor'(unsigned int)    [??_GLobbyPersistentData@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x0075c2c0
// (public: virtual __thiscall WONAPI::LobbyPersistentData::~LobbyPersistentData(void)    [??1LobbyPersistentData@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0075c200
// (public: __thiscall std::map<class WONAPI::GUIString,class WONAPI::LobbyOptionSet,struct WONAPI::GUIStringLessNoCase,class std::allocator<struct std::p)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0075bfd0
// (public: __thiscall std::set<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoCase,class std::allocator<class WONAPI::GUIString> >::~set<class WON)
// TEMPLATE: WON 0x0075e990
// (public: __thiscall std::multimap<long,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk)
// TEMPLATE: WON 0x0075edd0
// (public: __thiscall std::multimap<long,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUse)
// TEMPLATE: WON 0x00760610
// (public: class std::map<class WONAPI::GUIString,class WONAPI::LobbyOptionSet,struct WONAPI::GUIStringLessNoCase,class std::allocator<struct std::pair<c)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0075bf00
// (public: __thiscall std::set<long,struct std::less<long>,class std::allocator<long> >::~set<long,struct std::less<long>,class std::allocator<long> >(vo)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0075c090
// (public: __thiscall std::set<unsigned long,struct std::less<unsigned long>,class std::allocator<unsigned long> >::~set<unsigned long,struct std::less<u)
// TEMPLATE: WON 0x0071d2c0
// (public: void __thiscall std::list<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::_Insert(class std::list<class WONAPI::GUISt)
// TEMPLATE: WON 0x00721f20
// (protected: void __thiscall std::list<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::_Incsize(unsigned int))
// TEMPLATE: WON 0x00766b60
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class std::list<class WONAPI::GUIString,class std::al)
// TEMPLATE: WON 0x00767370
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<class)
// TEMPLATE: WON 0x007674f0
// (public: __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>,struct std::_Mapk<)
// TEMPLATE: WON 0x007675b0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>,struct std::_Mapk<class)
// TEMPLATE: WON 0x00767db0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GUIString,long,)
// TEMPLATE: WON 0x007684d0
// (public: __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk<class WONAP)
// TEMPLATE: WON 0x00768590
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk<class WONAPI::IP)
// TEMPLATE: WON 0x00768be0
// (public: __thiscall WONAPI::LobbyUserInfo::LobbyUserInfo(struct WONAPI::LobbyUserInfo const &))
// TEMPLATE: WON 0x00768c50
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>::~pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>(void))
// TEMPLATE: WON 0x00769a00
// (public: class std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoCase,)
// TEMPLATE: WON 0x00769ab0
// (public: class std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoCase,)
// TEMPLATE: WON 0x00769b00
// (protected: class std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoCa)
// TEMPLATE: WON 0x0076a2b0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<class)
// TEMPLATE: WON 0x0076a4a0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>,struct std::_Mapk<class)
// TEMPLATE: WON 0x0076a550
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>,struct std::_Mapk<cl)
// TEMPLATE: WON 0x0076a950
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_Mapk<cla)
// TEMPLATE: WON 0x0076ac50
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_M)
// TEMPLATE: WON 0x0076af40
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk<class WONAPI::IP)
// TEMPLATE: WON 0x0076aff0
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk<class )
// TEMPLATE: WON 0x0076b040
// (?insert@?$_Tree@JU?$pair@$$CBJViterator@?$_Tree@VGUIString@WONAPI@@U?$pair@$$CBVGUIString@WONAPI@@J@std@@U_Kfn@?$_Mapk@VGUIString@WONAPI@@JUGUIStringL)
// TEMPLATE: WON 0x0076b0c0
// ( ?? ?? :: ?? ::insert)
// TEMPLATE: WON 0x0076b140
// (public: class std::_Tree<long,struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,st)
// TEMPLATE: WON 0x0076c3c0
// (public: class std::_Tree<long,long,struct std::_Setk<long,struct std::less<long>,class std::allocator<long> >::_Kfn,struct std::less<long>,class std::)
// TEMPLATE: WON 0x0076c400
// (public: class std::_Tree<long,long,struct std::_Setk<long,struct std::less<long>,class std::allocator<long> >::_Kfn,struct std::less<long>,class std::)
// TEMPLATE: WON 0x0076c530
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class std::list<class WONAPI::GUIString,class std)
// TEMPLATE: WON 0x0076d030
// (protected: static struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::LobbyFriend> >,struct std)
// TEMPLATE: WON 0x0076d5a0
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<c)
// TEMPLATE: WON 0x0076d630
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std)
// TEMPLATE: WON 0x0076d670
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<cl)
// TEMPLATE: WON 0x0076d970
// (protected: static struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::)
// TEMPLATE: WON 0x0076d990
// (protected: static struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::)
// TEMPLATE: WON 0x0076d9b0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>,struct std)
// TEMPLATE: WON 0x0076df60
// (protected: struct std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_Mapk<cl)
// TEMPLATE: WON 0x0076dfb0
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_M)
// TEMPLATE: WON 0x0076e350
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GUIString,l)
// TEMPLATE: WON 0x0076e3a0
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk<cla)
// TEMPLATE: WON 0x0076e6e0
// (public: class std::_Tree<long,struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,lo)
// TEMPLATE: WON 0x0076eb90
// (protected: void __thiscall std::_Tree<long,struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIStr)
// TEMPLATE: WON 0x0076ebd0
// (protected: class std::_Tree<long,struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const )
// TEMPLATE: WON 0x0076eed0
// (public: class std::_Tree<long,struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,st)
// TEMPLATE: WON 0x0076f380
// (protected: void __thiscall std::_Tree<long,struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIStr)
// TEMPLATE: WON 0x0076f790
// (protected: struct std::_Tree<class WONAPI::GUIString,class WONAPI::GUIString,struct std::_Setk<class WONAPI::GUIString,struct WONAPI::GUIStringLessNoC)
// TEMPLATE: WON 0x0076f7e0
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<c)
// TEMPLATE: WON 0x0076fa00
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo> *,struct std::pair<class WONAPI::GUIString )
// TEMPLATE: WON 0x0076fe80
// (public: void __thiscall std::list<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::insert(class std::list<class WONAPI::GUIStr)
// TEMPLATE: WON 0x0076ffb0
// (void __cdecl std::_Construct(struct std::pair<long const ,class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,lon)
// TEMPLATE: WON 0x0076ffd0
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x0076fff0
// (public: void * __thiscall std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x00770070
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x007700b0
// (public: void __thiscall std::list<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::_Insert(class std::list<class WONAPI::GUISt)
// TEMPLATE: WON 0x00768af0
// (public: __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>::~pair<class WONAPI::GUIString const ,class WONAPI::LobbyOpt)
// TEMPLATE: WON 0x0076a360
// (public: __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOpti)
// TEMPLATE: WON 0x0076a450
// (public: __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::LobbyUserInfo>,struct std::_Mapk<)
// TEMPLATE: WON 0x0076a850
// (public: __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_Mapk<c)
// TEMPLATE: WON 0x0076d050
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct std::_Mapk<class)
// TEMPLATE: WON 0x0076da40
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_Mapk<class )
// TEMPLATE: WON 0x0076fa80
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo> *,struct std::pair<class WONAPI::IPAddr c)
// TEMPLATE: WON 0x0076fd90
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet> *,struct std::pair<class WONAPI::GUIString )
// TEMPLATE: WON 0x0076ff00
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString> *,struct std::pair<class WONAPI::IPAddr const ,clas)
// TEMPLATE: WON 0x00767ad0
// (public: __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_Mapk<c)
// TEMPLATE: WON 0x00767b90
// (public: long & __thiscall std::map<class WONAPI::GUIString,long,struct WONAPI::GUIStringLessNoCase,class std::allocator<struct std::pair<class WONAPI:)
// TEMPLATE: WON 0x00767cf0
// (public: __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GUIString,)
// TEMPLATE: WON 0x007682f0
// (public: class WONAPI::GUIString & __thiscall std::map<class WONAPI::IPAddr,class WONAPI::GUIString,struct std::less<class WONAPI::IPAddr>,class std::a)
// TEMPLATE: WON 0x00769700
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GUIString,lo)
// TEMPLATE: WON 0x00769e00
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::LobbyFriend> >,struct std::_Mapk<uns)
// TEMPLATE: WON 0x0076a1b0
// (public: struct std::pair<class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::LobbyOptionSet>,struct)
// TEMPLATE: WON 0x0076a8a0
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std::_Mapk<class )
// TEMPLATE: WON 0x0076aca0
// (public: struct std::pair<class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI)
// TEMPLATE: WON 0x0076ada0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GUIString,long,)
// TEMPLATE: WON 0x0076ae50
// (public: struct std::pair<class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk)
// TEMPLATE: WON 0x0076b1f0
// (public: __thiscall std::map<class WONAPI::GUIString,class std::list<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::iterator,)
// TEMPLATE: WON 0x0076df20
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::LobbyReconnectInfo>,struct std:)
// TEMPLATE: WON 0x0076e010
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GU)
// TEMPLATE: WON 0x0076e3e0
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::GUIString>,struct std::_Mapk<class WONAPI:)
// TEMPLATE: WON 0x0076f6c0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class std::list<class WONAPI::GUIString,)
// TEMPLATE: WON 0x0076f800
// (protected: struct std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class std::list<class WONAPI::GUIString,class std)
// TEMPLATE: WON 0x006a9950
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::LobbyFriend> >,struct std::_Mapk)
// TEMPLATE: WON 0x006a9980
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,class WONAPI::SmartPtr<class WONAPI::LobbyFriend> >,struct std::_Mapk)
// TEMPLATE: WON 0x0076c440
// (public: __thiscall std::list<class WONAPI::GUIString,class std::allocator<class WONAPI::GUIString> >::list<class WONAPI::GUIString,class std::allocato)
// TEMPLATE: WON 0x0076e050
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,long>,struct std::_Mapk<class WONAPI::GUIString,lo)
