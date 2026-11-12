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
#include "rust/ee_rust.h"

namespace WONAPI {
// The files' layouts (written from lists, read as events in the original's order of assignments), the user file
// name, the first free id, the pruning and the recent LAN name are ee_won's persist module
// (rust/crates/ee_won/src/persist.rs); the maps and sets are Rust containers (lobbymap.rs, round 17); this class
// keeps them, the password key and the defaults.
static void PersistAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
// a list of wide texts as UTF-16 units for the Rust writer
struct PersistTexts {
    std::vector<uint16_t> mUnits;
    std::vector<size_t> mOffsets;
    PersistTexts() : mOffsets(1, 0) {}
    void Add(const GUIString& inText)
    {
        std::string aBytes;
        WideAppendUtf16LE(aBytes, (std::wstring)inText);
        for (size_t i = 0; i + 1 < aBytes.size(); i += 2)
            this->mUnits.push_back((uint16_t)((unsigned char)aBytes[i] | ((unsigned char)aBytes[i + 1] << 8)));
        this->mOffsets.push_back(this->mUnits.size());
    }
    EeWonTextList List() const
    {
        EeWonTextList aList = {this->mUnits.empty() ? NULL : &this->mUnits[0], &this->mOffsets[0],
                               this->mOffsets.size() - 1};
        return aList;
    }
};
// a list of byte strings (the passwords' Blowfish bytes, the filters' UTF-8 names)
struct PersistBytes {
    std::string mBytes;
    std::vector<size_t> mOffsets;
    PersistBytes() : mOffsets(1, 0) {}
    void Add(const void* inData, size_t inLength)
    {
        this->mBytes.append(static_cast<const char*>(inData), inLength);
        this->mOffsets.push_back(this->mBytes.size());
    }
    EeWonTextList List() const
    {
        EeWonTextList aList = {this->mBytes.data(), &this->mOffsets[0], this->mOffsets.size() - 1};
        return aList;
    }
};
static GUIString PersistText(const EeWonPrefsEvent* inEvent)
{
    std::wstring aText;
    for (size_t i = 0; i < inEvent->count; i++)
        WideAppendUtf16Unit(aText, inEvent->units[i]);
    return GUIString(aText);
}
// the whole file (false when it does not open)
static bool PersistReadFile(const std::string& inName, std::string& outBytes)
{
    FileReader aReader;
    if (!aReader.Open(inName.c_str()))
        return false;
    char aBuffer[4096];
    try {
        int aCount;
        while ((aCount = aReader.ReadMaxBytes(aBuffer, sizeof(aBuffer))) > 0)
            outBytes.append(aBuffer, aCount);
    } catch (FileReaderException&) {
    }
    return true;
}
template <class T> static const T* PersistArray(const std::vector<T>& inItems)
{
    return inItems.empty() ? NULL : &inItems[0];
}

// FUNCTION: WON 0x0075e6e0
// Returns the preferences file name of a lobby user, _wonuser<id>.dat
std::string LobbyPersistentData::GetUserFileName(long inUserId)
{
    std::string aName;
    ee_won_prefs_user_file_name((int32_t)inUserId, PersistAppendBytes, &aName);
    return aName;
}
// FUNCTION: WON 0x0075e770
// Returns the first free user id after the run of consecutive ids starting at the smallest (1 when none exist)
long LobbyPersistentData::FindUnusedUserId()
{
    std::vector<int32_t> anIds;
    for (size_t i = 0; i < this->mUserIds.size(); i++)
        anIds.push_back((int32_t)this->mUserIds.KeyAt(i));
    return ee_won_prefs_unused_user_id(PersistArray(anIds), anIds.size());
}
// (ReadFileHeader 0x0075f6a0 and WriteFileHeader 0x0075f750: persist.rs header / read_header)

}

namespace WONAPI {
long gLobbyGlobalPrefsVersion = 2;
long gLobbyUserPrefsVersion = 4;


// FUNCTION: WON 0x0075baa0
// Adds a value to the option set unless it is already present, keeping the values in insertion order
void LobbyOptionSet::Add(const GUIString& inValue)
{
    this->mValues.Insert(inValue);
}
// FUNCTION: WON 0x0075bc20
// Removes a value from the option set; returns whether it was present
bool LobbyOptionSet::Erase(const GUIString& inValue)
{
    return this->mValues.Erase(inValue);
}

LobbyPersistentData* GetLobbyPersistentData()
{
    return gLobbyData.get() != NULL ? gLobbyData->mPersistentData.get() : NULL;
}


// the global file's events applied as the original assigned them (the user, its fields, its id into the id set
// after the password; the sound bytes mute the click sound)
struct PersistGlobalLoad {
    LobbyPersistentData* mData;
    LobbyUserInfo* mUser;
    static void Event(void* inContext, const EeWonPrefsEvent* e)
    {
        PersistGlobalLoad* aLoad = static_cast<PersistGlobalLoad*>(inContext);
        LobbyPersistentData* d = aLoad->mData;
        switch (e->kind) {
        case EeWonPrefsEventKind::User: {
            GUIString aName = PersistText(e);
            aLoad->mUser = &d->mUsers[aName];
            aLoad->mUser->mUserName = aName;
            break;
        }
        case EeWonPrefsEventKind::UserTime: aLoad->mUser->mLastUseTime = (long)e->value; break;
        case EeWonPrefsEventKind::UserId: aLoad->mUser->mUserId = (long)e->value; break;
        case EeWonPrefsEventKind::UserValue04: aLoad->mUser->mValue04 = (long)e->value; break;
        case EeWonPrefsEventKind::UserValue08: aLoad->mUser->mValue08 = (long)e->value; break;
        case EeWonPrefsEventKind::UserPassword:
            aLoad->mUser->mPassword = d->DecryptString(e->bytes, e->length);
            d->mUserIds.Insert(aLoad->mUser->mUserId);
            break;
        case EeWonPrefsEventKind::SoundEffects:
            d->SetFlag(LobbyPersistentData::kLobbySoundEffects, e->value != 0);
            d->SetFlag(LobbyPersistentData::kSavedLobbySoundEffects, e->value != 0);
            // the click sound is used without a null check, as the original did
            ::LobbyGlobal_ButtonClick_Sound->SetMuted(!d->GetFlag(LobbyPersistentData::kLobbySoundEffects));
            break;
        case EeWonPrefsEventKind::Music:
            d->SetFlag(LobbyPersistentData::kLobbyMusic, e->value != 0);
            d->SetFlag(LobbyPersistentData::kSavedLobbyMusic, e->value != 0);
            break;
        // the network adapter is stored as its numeric IPv4 address
        case EeWonPrefsEventKind::NetworkAdapter: d->SetNetworkAdapter((long)e->value); break;
        default: break;
        }
    }
};
// FUNCTION: WON 0x0075f7b0
// Loads the saved login users (names, ids, encrypted passwords) and the lobby sound, music and network adapter
// settings from _wonlobbypersistent.dat, muting the button click sound to match; another version changes nothing
// more (the dirty flag stays), a cut file keeps what was read before the cut
void LobbyPersistentData::LoadGlobalPrefs()
{
    std::string aBytes;
    if (!PersistReadFile("_wonlobbypersistent.dat", aBytes))
        return;
    this->mUsers.clear();
    this->SetNetworkAdapter(0);
    PersistGlobalLoad aLoad = {this, NULL};
    if (ee_won_prefs_read_global(reinterpret_cast<const uint8_t*>(aBytes.data()), aBytes.length(),
                                 PersistGlobalLoad::Event, &aLoad) == EeWonPrefsRead::OtherVersion)
        return;
    this->SetFlag(LobbyPersistentData::kGlobalDirty, false);
}

// FUNCTION: WON 0x0075faf0
// Writes the login users and the lobby sound, music and network adapter settings to _wonlobbypersistent.dat
void LobbyPersistentData::SaveGlobalPrefs()
{
    FileWriter aWriter;
    if (!aWriter.Open("_wonlobbypersistent.dat", false))
        return;
    PersistTexts aNames;
    PersistBytes aPasswords;
    std::vector<int32_t> aTimes, aValues04, aValues08;
    std::vector<uint16_t> anIds;
    for (size_t anItr = 0; anItr < this->mUsers.size(); ++anItr)
    {
        LobbyUserInfo& aUser = this->mUsers.At(anItr);
        aNames.Add(aUser.mUserName);
        aTimes.push_back((int32_t)aUser.mLastUseTime);
        anIds.push_back((uint16_t)aUser.mUserId);
        aValues04.push_back((int32_t)aUser.mValue04);
        aValues08.push_back((int32_t)aUser.mValue08);
        ByteBufferPtr anEncrypted = this->EncryptString(aUser.mPassword);
        aPasswords.Add(anEncrypted.get() != NULL ? anEncrypted->data() : "",
                       anEncrypted.get() != NULL ? anEncrypted->length() : 0);
    }
    EeWonGlobalPrefsOut aPrefs;
    aPrefs.names = aNames.List();
    aPrefs.last_use = PersistArray(aTimes);
    aPrefs.ids = PersistArray(anIds);
    aPrefs.value04 = PersistArray(aValues04);
    aPrefs.value08 = PersistArray(aValues08);
    aPrefs.passwords = aPasswords.List();
    aPrefs.sound_effects = this->GetFlag(LobbyPersistentData::kLobbySoundEffects);
    aPrefs.music = this->GetFlag(LobbyPersistentData::kLobbyMusic);
    aPrefs.network_adapter = (int32_t)this->GetNetworkAdapter();
    std::string aFile;
    ee_won_prefs_write_global(&aPrefs, PersistAppendBytes, &aFile);
    try
    {
        aWriter.WriteBytes(aFile.data(), aFile.length());
    }
    catch (FileWriterException&)
    {
    }
    // The saved sound and music states are updated even when a write failed
    ee_won_lobbyprefs_after_global_save(this->mPrefs);
}


// FUNCTION: WON 0x0075c720
// Resets the user's lobby options to their defaults: Quick Play tab for an internet user, LAN tab and the
// default LAN name for the LAN user, all sounds and confirmations on, broadcast port 8585
void LobbyPersistentData::RestoreDefaults()
{
    std::string aQuickPlay = (std::string)::LobbyGlobal_QuickPlay_String, aLan = (std::string)::LobbyGlobal_Lan_String;
    if (ee_won_lobbyprefs_restore_defaults(this->mPrefs, reinterpret_cast<const uint8_t*>(aQuickPlay.data()),
                                           aQuickPlay.length(), reinterpret_cast<const uint8_t*>(aLan.data()),
                                           aLan.length()))
        this->SetLanName(this->GetDefaultLanName());
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
    size_t anItr = aData->Users().Insert(inUserName);
    LobbyUserInfo& aUser = aData->Users().At(anItr);
    if (inExistingOnly && aUser.mLastUseTime == 0)
    {
        aData->Users().EraseAt(anItr);
        return NULL;
    }
    if (aUser.mUserId == 0)
    {
        aUser.mUserId = aData->FindUnusedUserId();
        aData->UserIds().Insert(aUser.mUserId);
        WONFile aFile(aData->GetUserFileName(aUser.mUserId));
        aFile.Delete(true);
    }
    aUser.mLastUseTime = time(NULL);
    aUser.mUserName = inUserName;
    aUser.mPassword = inPassword;
    aData->SetFlag(LobbyPersistentData::kGlobalDirty, true);
    return &aUser;
}

// FUNCTION: WON 0x0075eb30
// Keeps at most 50 saved users: drops users without an id, then deletes the least recently used ones and their files
// (persist.rs prune_users chooses)
void LobbyPersistentData::PruneUsers()
{
    if (this->mUsers.size() <= 50)
        return;
    std::vector<int32_t> anIds;
    std::vector<int64_t> aTimes;
    for (size_t anItr = 0; anItr < this->mUsers.size(); ++anItr)
    {
        anIds.push_back((int32_t)this->mUsers.At(anItr).mUserId);
        aTimes.push_back(this->mUsers.At(anItr).mLastUseTime);
    }
    std::vector<uint8_t> aDrop(anIds.size(), 0);
    ee_won_prefs_prune_users(&anIds[0], &aTimes[0], anIds.size(), &aDrop[0]);
    // from the end, so the earlier positions stay
    for (size_t i = anIds.size(); i-- > 0;)
    {
        if (aDrop[i] == 0)
            continue;
        if (aDrop[i] == 2)
        {
            LobbyUserInfo& aUser = this->mUsers.At(i);
            WONFile aFile(this->GetUserFileName(aUser.mUserId));
            aFile.Delete(true);
            this->mUserIds.Erase(aUser.mUserId);
        }
        this->mUsers.EraseAt(i);
    }
}

// FUNCTION: WON 0x0075d460
// Adds a player to the current user's friend list unless the user id is already there; returns the friend entry
LobbyFriend* LobbyAddFriend(const GUIString& inName, unsigned long inUserId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return NULL;
    size_t anAt = aData->Friends().Insert(inUserId);
    if (aData->Friends().At(anAt) == NULL)
    {
        aData->Friends().SetAt(anAt, new LobbyFriend(inName, inUserId));
        aData->SetFlag(LobbyPersistentData::kUserDirty, true);
    }
    return aData->Friends().At(anAt);
}
// FUNCTION: WON 0x0075d2c0
// Removes a user id from the friend list; the dirty flag is set to whether one was removed, so a miss clears it
void LobbyRemoveFriend(unsigned long inUserId)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData != NULL)
        aData->SetFlag(LobbyPersistentData::kUserDirty, aData->Friends().Erase(inUserId));
}

}

namespace WONAPI {

// (ReadByteString 0x0075ee80, ReadWideString 0x0075f000, WriteByteString 0x0075f3d0 and WriteWideString 0x0075f4e0:
// persist.rs's strings)




// FUNCTION: WON 0x0075f190
// Decrypts a saved password with the machine-bound key (ReadEncryptedString's half after the read); returns an
// empty string if it cannot be decoded
GUIString LobbyPersistentData::DecryptString(const uint8_t* inData, size_t inLength)
{
    WriteBuffer storage(0, true);
    storage.Reserve(inLength); // grows the capacity only; the length stays 0
    if (inLength != 0)
        memcpy(storage.data(), inData, inLength);
    ByteBufferPtr decrypted = this->mEncryptKey.Decrypt(storage.data(), (int)inLength);
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
// Encrypts a password with the machine-bound key (WriteEncryptedString's half before the write; NULL if encryption
// fails: a length of 0 is saved)
ByteBufferPtr LobbyPersistentData::EncryptString(const GUIString& inText)
{
    WriteBuffer storage(0, true);
    std::wstring text = (std::wstring)inText;
    storage.AppendWString(text, 2);
    return this->mEncryptKey.Encrypt(storage.data(), storage.length());
}
// the user file's events applied as the original assigned them
struct PersistUserLoad {
    LobbyPersistentData* mData;
    size_t mReconnect; // the reconnect entry the next events fill (its position)
    LobbyOptionSet* mOption;
    bool mHaveGameOptions;
    static void Event(void* inContext, const EeWonPrefsEvent* e)
    {
        PersistUserLoad* aLoad = static_cast<PersistUserLoad*>(inContext);
        LobbyPersistentData* d = aLoad->mData;
        // the settings in the file's order (persist.rs Settings)
        typedef LobbyPersistentData P;
        static const int aFlags[14] = {P::kFilterDirtyWords, P::kDoAutoAFK, P::kCrossPromotion, P::kAutoPingGames,
                                       P::kAnonymousToFriends, P::kShowGamesInProgress, -1, -1, -1,
                                       P::kLobbySoundEffects, P::kLobbyMusic, P::kChatSoundEffects,
                                       P::kStagingSoundEffects, P::kShowConfirmations};
        static const EeWonPrefsText aTexts[3] = {EeWonPrefsText::TabSelect, EeWonPrefsText::LastServerName,
                                                 EeWonPrefsText::LastRoomName};
        switch (e->kind) {
        case EeWonPrefsEventKind::LanName:
            d->mLanNameInfoMap.SetNumAt(d->mLanNameInfoMap.Insert(PersistText(e)), (long)e->value);
            break;
        case EeWonPrefsEventKind::LanNamesDone: d->SelectRecentLanName(); break;
        case EeWonPrefsEventKind::DirectConnect: {
            IPAddr anAddress;
            anAddress.SetSixByte(e->address);
            d->mDirectConnectMap.SetTextAt(d->mDirectConnectMap.Insert(anAddress), PersistText(e));
            break;
        }
        case EeWonPrefsEventKind::BroadcastPort: d->SetLanBroadcastPort((unsigned short)e->value); break;
        case EeWonPrefsEventKind::Reconnect: {
            IPAddr anAddress;
            anAddress.SetSixByte(e->address);
            aLoad->mReconnect = d->mReconnectInfoMap.Insert(anAddress);
            break;
        }
        case EeWonPrefsEventKind::ReconnectTime: d->mReconnectInfoMap.SetNumAt(aLoad->mReconnect, (long)e->value, 0); break;
        case EeWonPrefsEventKind::ReconnectId: d->mReconnectInfoMap.SetNumAt(aLoad->mReconnect, (int)e->value, 1); break;
        case EeWonPrefsEventKind::Ignored: d->mIgnoredNames.Insert(PersistText(e)); break;
        case EeWonPrefsEventKind::Friend: {
            unsigned long aUserId = (unsigned long)e->value;
            SmartPtr<LobbyFriend> aFriend = new LobbyFriend(PersistText(e), aUserId);
            bool aNew = false;
            size_t anAt = d->mFriends.Insert(aUserId, &aNew);
            if (aNew) d->mFriends.SetAt(anAt, aFriend.get());
            break;
        }
        case EeWonPrefsEventKind::Blocked: d->mBlockedCDKeyIds.Insert((unsigned long)e->value); break;
        case EeWonPrefsEventKind::Flag:
            if (e->index < 14 && aFlags[e->index] >= 0) d->SetFlag((P::Flag)aFlags[e->index], e->value != 0);
            break;
        case EeWonPrefsEventKind::SettingText:
            if (e->index >= 6 && e->index < 9) d->SetText(aTexts[e->index - 6], PersistText(e));
            break;
        case EeWonPrefsEventKind::ConnectionSpeed: d->SetConnectionSpeed((unsigned short)e->value); break;
        case EeWonPrefsEventKind::OptionsBegin: aLoad->mHaveGameOptions = true; break;
        case EeWonPrefsEventKind::OptionName:
            // the filter's name is UTF-8 (D6)
            aLoad->mOption = &d->mGameOptions[GUIString(std::string(reinterpret_cast<const char*>(e->bytes), e->length))];
            break;
        case EeWonPrefsEventKind::OptionValue: aLoad->mOption->Add(PersistText(e)); break;
        default: break;
        }
    }
};
// A file with another version returns early: the game filters then stay empty and the dirty flags unchanged
// FUNCTION: WON 0x0075fc70
// Loads the current user's _wonuser file: reconnect ids, ignored names, friends, blocked CD keys (loaded and saved
// but never filled by the lobby), options, last server/room, sounds and game filters; LAN data for the LAN user
// (ReadLanPrefs 0x00760b10 is persist.rs read_user's first part)
void LobbyPersistentData::LoadUserPrefs()
{
    this->RestoreDefaults();
    this->mReconnectInfoMap.clear();
    this->mIgnoredNames.clear();
    this->mFriends.clear();
    this->mLanNameInfoMap.clear();
    this->mDirectConnectMap.clear();
    this->mGameOptions.clear();
    std::string aBytes;
    if (!PersistReadFile(this->GetUserFileName(this->GetCurrentUserId()), aBytes))
    {
        this->mGameOptions = this->mDefaultGameOptions;
        return;
    }
    PersistUserLoad aLoad = {this, 0, NULL, false};
    if (ee_won_prefs_read_user(reinterpret_cast<const uint8_t*>(aBytes.data()), aBytes.length(),
                               this->GetCurrentUserId() == 0, PersistUserLoad::Event, &aLoad) == EeWonPrefsRead::OtherVersion)
        return;
    if (!aLoad.mHaveGameOptions)
        this->mGameOptions = this->mDefaultGameOptions;
    ee_won_lobbyprefs_after_user_load(this->mPrefs);
}



// (WriteLanPrefs 0x00760da0 is persist.rs write_user's first part)
// FUNCTION: WON 0x00760770
// Writes the current user's _wonuser file (the counterpart of LoadUserPrefs) and clears the user dirty flag; a game
// filter with no values is written without its count (persist.rs keeps the original's bug)
void LobbyPersistentData::SaveUserPrefs()
{
    FileWriter writer;
    std::string fileName = this->GetUserFileName(this->GetCurrentUserId());
    if (!writer.Open(fileName.c_str(), false))
        return;
    EeWonUserPrefsOut aPrefs;
    memset(&aPrefs, 0, sizeof(aPrefs));
    aPrefs.lan_user = this->GetCurrentUserId() == 0;
    PersistTexts aLanNames, aDirectNames, anIgnored, aFriendNames, aTexts, aValues;
    std::vector<int32_t> aLanTimes, aTimes, anIds;
    std::vector<uint32_t> aFriendIds, aBlocked;
    std::string aDirect, aReconnect;
    PersistBytes anOptionNames;
    std::vector<size_t> anOptionFirst(1, 0);
    for (size_t i = 0; i < this->mLanNameInfoMap.size(); ++i)
    {
        aLanNames.Add(this->mLanNameInfoMap.KeyAt(i));
        aLanTimes.push_back((int32_t)this->mLanNameInfoMap.NumAt(i));
    }
    for (size_t j = 0; j < this->mDirectConnectMap.size(); ++j)
    {
        aDirect.append(this->mDirectConnectMap.KeyAt(j).GetSixByte(), 6);
        aDirectNames.Add(this->mDirectConnectMap.TextAt(j));
    }
    for (size_t a = 0; a < this->mReconnectInfoMap.size(); ++a)
    {
        aReconnect.append(this->mReconnectInfoMap.KeyAt(a).GetSixByte(), 6);
        aTimes.push_back((int32_t)this->mReconnectInfoMap.NumAt(a, 0));
        anIds.push_back((int32_t)this->mReconnectInfoMap.NumAt(a, 1));
    }
    for (size_t n = 0; n < this->mIgnoredNames.size(); ++n)
        anIgnored.Add(this->mIgnoredNames.KeyAt(n));
    for (size_t f = 0; f < this->mFriends.size(); ++f)
    {
        aFriendNames.Add(this->mFriends.At(f)->mName);
        aFriendIds.push_back((uint32_t)this->mFriends.KeyAt(f));
    }
    for (size_t u = 0; u < this->mBlockedCDKeyIds.size(); ++u)
        aBlocked.push_back((uint32_t)this->mBlockedCDKeyIds.KeyAt(u));
    aTexts.Add(this->GetTabSelect());
    aTexts.Add(this->GetLastServerName());
    aTexts.Add(this->GetLastRoomName());
    for (size_t o = 0; o < this->mGameOptions.size(); ++o)
    {
        // the filter's name as UTF-8 (GetString)
        std::string aName = (std::string)this->mGameOptions.KeyAt(o);
        anOptionNames.Add(aName.data(), aName.length());
        const LobbyOptionSet& anOptions = this->mGameOptions.At(o);
        for (size_t v = 0; v < anOptions.size(); ++v)
            aValues.Add(anOptions.ValueAt(v));
        anOptionFirst.push_back(aValues.mOffsets.size() - 1);
    }
    aPrefs.lan_names = aLanNames.List();
    aPrefs.lan_times = PersistArray(aLanTimes);
    aPrefs.direct_addresses = reinterpret_cast<const uint8_t*>(aDirect.data());
    aPrefs.direct_names = aDirectNames.List();
    aPrefs.broadcast_port = this->GetLanBroadcastPort();
    aPrefs.reconnect_count = aTimes.size();
    aPrefs.reconnect_addresses = reinterpret_cast<const uint8_t*>(aReconnect.data());
    aPrefs.reconnect_times = PersistArray(aTimes);
    aPrefs.reconnect_ids = PersistArray(anIds);
    aPrefs.ignored = anIgnored.List();
    aPrefs.friend_names = aFriendNames.List();
    aPrefs.friend_ids = PersistArray(aFriendIds);
    aPrefs.blocked_count = aBlocked.size();
    aPrefs.blocked = PersistArray(aBlocked);
    bool aFirst[6] = {this->GetFlag(LobbyPersistentData::kFilterDirtyWords), this->GetFlag(LobbyPersistentData::kDoAutoAFK), this->GetFlag(LobbyPersistentData::kCrossPromotion), this->GetFlag(LobbyPersistentData::kAutoPingGames),
                      this->GetFlag(LobbyPersistentData::kAnonymousToFriends), this->GetFlag(LobbyPersistentData::kShowGamesInProgress)};
    bool aSecond[5] = {this->GetFlag(LobbyPersistentData::kLobbySoundEffects), this->GetFlag(LobbyPersistentData::kLobbyMusic), this->GetFlag(LobbyPersistentData::kChatSoundEffects), this->GetFlag(LobbyPersistentData::kStagingSoundEffects),
                       this->GetFlag(LobbyPersistentData::kShowConfirmations)};
    memcpy(aPrefs.first, aFirst, sizeof(aFirst));
    memcpy(aPrefs.second, aSecond, sizeof(aSecond));
    aPrefs.texts = aTexts.List();
    aPrefs.connection_speed = this->GetConnectionSpeed();
    aPrefs.option_names = anOptionNames.List();
    aPrefs.option_first = &anOptionFirst[0];
    aPrefs.option_values = aValues.List();
    std::string aFile;
    ee_won_prefs_write_user(&aPrefs, PersistAppendBytes, &aFile);
    try
    {
        writer.WriteBytes(aFile.data(), aFile.length());
    }
    catch (FileWriterException&) {}
    this->SetFlag(LobbyPersistentData::kUserDirty, false);
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
    size_t anInfo = aData->ReconnectInfoMap().Insert(inAddress);
    aData->ReconnectInfoMap().SetNumAt(anInfo, (long)time(NULL), 0);
    aData->ReconnectInfoMap().SetNumAt(anInfo, inReconnectId, 1);
    aData->SaveUserPrefs();
}
// FUNCTION: WON 0x0075db00
// Returns the saved reconnect id for a server address, or 0 if none
int LobbyGetReconnectId(const IPAddr& inAddress)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return 0;
    long anItr = aData->ReconnectInfoMap().Find(inAddress);
    return anItr < 0 ? 0 : (int)aData->ReconnectInfoMap().NumAt((size_t)anItr, 1);
}
// FUNCTION: WON 0x0075e040
// Adds comma-separated values to a default game filter column; skips a column that already has values unless
// inAppend is set
void LobbyAddGameOption(const GUIString& inName, const GUIString& inValues, bool inAppend)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL)
        return;
    LobbyOptionSet& anOptions = aData->DefaultGameOptions()[inName];
    if (!anOptions.empty() && !inAppend)
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
        aData->IgnoredNames().Insert(inName, &aDirty);
    else
        aDirty = aData->IgnoredNames().Erase(inName);
    aData->SetFlag(LobbyPersistentData::kUserDirty, aDirty);
}
// FUNCTION: WON 0x0075d6c0
// Makes a non-empty name the current LAN name, stamping its use time and keeping the 50 most recent names
void LobbySetLanName(const GUIString& inName)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL || inName.mData->mLength == 0 ||
        inName.mData->Compare(aData->GetLanName().mData.get()) == 0) return;
    long aTime = time(NULL);
    aData->LanNameInfoMap().SetNumAt(aData->LanNameInfoMap().Insert(inName), aTime);
    aData->SetLanName(inName);
    aData->PruneLanNames();
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// FUNCTION: WON 0x0075d770
// Forgets the current LAN name and falls back to the most recently used remaining one
void LobbyRemoveLanName()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    if (!aData->LanNameInfoMap().Erase(aData->GetLanName()))
        return;
    aData->SelectRecentLanName();
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// FUNCTION: WON 0x0075d7f0
// Saves a named direct-connect address
void LobbyAddDirectConnect(const IPAddr& inAddress, const GUIString& inName)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    aData->DirectConnectMap().SetTextAt(aData->DirectConnectMap().Insert(inAddress), inName);
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// FUNCTION: WON 0x0075d870
// Forgets a saved direct-connect address
void LobbyRemoveDirectConnect(const IPAddr& inAddress)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    if (aData->DirectConnectMap().Erase(inAddress))
        aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// FUNCTION: WON 0x0075db70
// Remembers the last server and room the user joined
void LobbySetLastRoom(const GUIString& inServer, const GUIString& inGroup)
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    aData->SetLastRoom(inServer, inGroup);
}
// FUNCTION: WON 0x0075df00
// Resets the user's game filters to the configured defaults
void LobbyResetFilters()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if (aData == NULL) return;
    // The original copied the option sets memberwise, stored list iterators included (lobbymap.rs copies them)
    aData->GameOptions() = aData->DefaultGameOptions();
    aData->SetFlag(LobbyPersistentData::kUserDirty, true);
}
// FUNCTION: WON 0x0075e7e0
// Keeps at most 50 recent LAN names, dropping the least recently used (persist.rs prune_lan_names chooses)
void LobbyPersistentData::PruneLanNames()
{
    GetLobbyPersistentData();
    if (this->mLanNameInfoMap.size() <= 50) return;
    std::vector<int64_t> aTimes;
    for (size_t anItr = 0; anItr < this->mLanNameInfoMap.size(); ++anItr)
        aTimes.push_back(this->mLanNameInfoMap.NumAt(anItr));
    std::vector<uint8_t> aDrop(aTimes.size(), 0);
    ee_won_prefs_prune_lan_names(&aTimes[0], aTimes.size(), reinterpret_cast<bool*>(&aDrop[0]));
    for (size_t i = aTimes.size(); i-- > 0;)
        if (aDrop[i])
            this->mLanNameInfoMap.EraseAt(i);
}
// FUNCTION: WON 0x0075ea40
// Makes the most recently used saved LAN name current, or the default name when none is saved
// (persist.rs recent_lan_name chooses)
void LobbyPersistentData::SelectRecentLanName()
{
    GetLobbyPersistentData();
    this->SetLanName(this->GetDefaultLanName());
    std::vector<int64_t> aTimes;
    for (size_t anItr = 0; anItr < this->mLanNameInfoMap.size(); ++anItr)
        aTimes.push_back(this->mLanNameInfoMap.NumAt(anItr));
    int64_t aChosen = ee_won_prefs_recent_lan_name(PersistArray(aTimes), aTimes.size());
    if (aChosen >= 0)
        this->SetLanName(this->mLanNameInfoMap.KeyAt((size_t)aChosen));
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
    char aNumber[256];
    unsigned short aValue = ee_won_prefs_default_lan_number((uint32_t)anAddress);
    snprintf(aNumber, sizeof(aNumber), "%u", (unsigned)aValue);
    aName.append(GUIString(aNumber));
    return aName;
}
}

namespace WONAPI {
// the scalar state's accessors and rules (prefsobj.rs EeWonLobbyPrefs)
static EeWonPrefsFlag PrefsFlag(LobbyPersistentData::Flag inFlag) { return static_cast<EeWonPrefsFlag>(inFlag); }
static const uint8_t* PrefsBytes(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
bool LobbyPersistentData::GetFlag(Flag inFlag) const { return ee_won_lobbyprefs_flag(this->mPrefs, PrefsFlag(inFlag)); }
void LobbyPersistentData::SetFlag(Flag inFlag, bool inOn) { ee_won_lobbyprefs_set_flag(this->mPrefs, PrefsFlag(inFlag), inOn); }
void LobbyPersistentData::ChangeFlag(Flag inFlag, bool inOn)
{ ee_won_lobbyprefs_change_flag(this->mPrefs, PrefsFlag(inFlag), inOn); }
long LobbyPersistentData::GetCurrentUserId() const { return (long)ee_won_lobbyprefs_current_user(this->mPrefs); }
void LobbyPersistentData::SetCurrentUserId(long inId) { ee_won_lobbyprefs_set_current_user(this->mPrefs, inId); }
unsigned short LobbyPersistentData::GetConnectionSpeed() const
{ return ee_won_lobbyprefs_number(this->mPrefs, EeWonPrefsNumber::ConnectionSpeed); }
void LobbyPersistentData::SetConnectionSpeed(unsigned short inSpeed)
{ ee_won_lobbyprefs_set_number(this->mPrefs, EeWonPrefsNumber::ConnectionSpeed, inSpeed); }
void LobbyPersistentData::ChangeConnectionSpeed(unsigned short inSpeed)
{ ee_won_lobbyprefs_change_number(this->mPrefs, EeWonPrefsNumber::ConnectionSpeed, inSpeed); }
unsigned short LobbyPersistentData::GetLanBroadcastPort() const
{ return ee_won_lobbyprefs_number(this->mPrefs, EeWonPrefsNumber::LanBroadcastPort); }
void LobbyPersistentData::SetLanBroadcastPort(unsigned short inPort)
{ ee_won_lobbyprefs_set_number(this->mPrefs, EeWonPrefsNumber::LanBroadcastPort, inPort); }
void LobbyPersistentData::ChangeLanBroadcastPort(unsigned short inPort)
{ ee_won_lobbyprefs_change_number(this->mPrefs, EeWonPrefsNumber::LanBroadcastPort, inPort); }
long LobbyPersistentData::GetNetworkAdapter() const { return (long)ee_won_lobbyprefs_network_adapter(this->mPrefs); }
void LobbyPersistentData::SetNetworkAdapter(long inAdapter) { ee_won_lobbyprefs_set_network_adapter(this->mPrefs, inAdapter); }
void LobbyPersistentData::SaveNetworkAdapter(long inLocalHost)
{ ee_won_lobbyprefs_save_network_adapter(this->mPrefs, inLocalHost); }
GUIString LobbyPersistentData::GetText(EeWonPrefsText inText) const
{
    size_t aLength = 0;
    const uint8_t* aBytes = ee_won_lobbyprefs_text(this->mPrefs, inText, &aLength);
    return GUIString(std::string(reinterpret_cast<const char*>(aBytes), aLength));
}
void LobbyPersistentData::SetText(EeWonPrefsText inText, const GUIString& inValue)
{
    std::string aText = (std::string)inValue;
    ee_won_lobbyprefs_set_text(this->mPrefs, inText, PrefsBytes(aText), aText.length());
}
GUIString LobbyPersistentData::GetTabSelect() const { return this->GetText(EeWonPrefsText::TabSelect); }
void LobbyPersistentData::SetTabSelect(const GUIString& inText) { this->SetText(EeWonPrefsText::TabSelect, inText); }
void LobbyPersistentData::ChangeTabSelect(const GUIString& inText)
{
    std::string aText = (std::string)inText;
    ee_won_lobbyprefs_change_tab_select(this->mPrefs, PrefsBytes(aText), aText.length());
}
GUIString LobbyPersistentData::GetLanName() const { return this->GetText(EeWonPrefsText::LanName); }
void LobbyPersistentData::SetLanName(const GUIString& inText) { this->SetText(EeWonPrefsText::LanName, inText); }
GUIString LobbyPersistentData::GetLastServerName() const { return this->GetText(EeWonPrefsText::LastServerName); }
void LobbyPersistentData::SetLastServerName(const GUIString& inText)
{ this->SetText(EeWonPrefsText::LastServerName, inText); }
GUIString LobbyPersistentData::GetLastRoomName() const { return this->GetText(EeWonPrefsText::LastRoomName); }
void LobbyPersistentData::SetLastRoomName(const GUIString& inText) { this->SetText(EeWonPrefsText::LastRoomName, inText); }
void LobbyPersistentData::SetLastRoom(const GUIString& inServer, const GUIString& inRoom)
{
    std::string aServer = (std::string)inServer, aRoom = (std::string)inRoom;
    ee_won_lobbyprefs_set_last_room(this->mPrefs, PrefsBytes(aServer), aServer.length(), PrefsBytes(aRoom),
                                    aRoom.length());
}
void LobbyPersistentData::AfterRestoreDefaults(long inOldAdapter)
{ ee_won_lobbyprefs_after_restore(this->mPrefs, inOldAdapter); }
LobbyPersistentData::~LobbyPersistentData() { ee_won_lobbyprefs_free(this->mPrefs); }

// FUNCTION: WON 0x0075bcb0
// Creates the lobby's saved settings with default options and a password key bound to this machine
LobbyPersistentData::LobbyPersistentData()
    : mPrefs(ee_won_lobbyprefs_new()), mUserIds(EeWonLobbyKeyKind::Signed, EeWonLobbyMapHooks()),
      mIgnoredNames(EeWonLobbyKeyKind::NoCase, EeWonLobbyMapHooks()),
      mBlockedCDKeyIds(EeWonLobbyKeyKind::Unsigned, EeWonLobbyMapHooks()), mFriends(EeWonLobbyKeyKind::Unsigned),
      mDefaultGameOptions(EeWonLobbyKeyKind::NoCase), mGameOptions(EeWonLobbyKeyKind::NoCase),
      mUsers(EeWonLobbyKeyKind::NoCase), mReconnectInfoMap(EeWonLobbyKeyKind::Addr, EeWonLobbyMapHooks()),
      mLanNameInfoMap(EeWonLobbyKeyKind::NoCase, EeWonLobbyMapHooks()),
      mDirectConnectMap(EeWonLobbyKeyKind::Addr, EeWonLobbyMapHooks())
{
    WriteBuffer aKey(0,true);
    aKey.Reserve(8);
    aKey.AppendLong(0x22fa655f);
    aKey.AppendLong(CDKey::GetMachineId(NULL));
    this->mEncryptKey.SetKey(aKey.data(),aKey.length());
    // The original did not set mCurrentUserId before this reset (the object starts at 0: the LAN user)
    this->RestoreDefaults();
    this->SetLanBroadcastPort(8585);
    this->SetFlag(LobbyPersistentData::kGlobalDirty, false);
    this->SetFlag(LobbyPersistentData::kUserDirty, false);
    this->SetFlag(LobbyPersistentData::kSavedLobbySoundEffects, true);
    this->SetFlag(LobbyPersistentData::kSavedLobbyMusic, true);
}
}

// Empties an option set (value index and ordered value list); no copy of its own was found in the original,
// it has the same shape as the SDK's ServerListFilterSet::clear
namespace WONAPI {
void LobbyOptionSet::clear()
{
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
