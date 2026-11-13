// AuthContext's data logic (AuthContext.cpp): the login secrets (their file, add, remove, commit, the login's
// secret bytes), the login's community data block and its hashes block, and the auto-refresh wait. Each is run the
// way the original AuthContext ran it (the statements copied from the C++ before the port, over the same std::list,
// std::map, WriteBuffer and MD5Digest) and through ee_won's authctx module (its C ABI, marshalled as the new
// AuthContext.cpp marshals it); the bytes, files and lists must be the same. Links WriteBuffer / ByteBuffer and
// MD5Digest. Prints one line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONMD5Digest.h"
#include "ee/wonapi/WONMD5.h"
#include "platform/PlatFile.h"
#include "rust/ee_won.h"

using namespace WONAPI;

static int sFailures;
static int sChecks;

static std::string Hex(const std::string& inBytes)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inBytes.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", (unsigned char)inBytes[i]);
        anOut += aBuffer;
    }
    return anOut;
}

static void Same(const char* inWhat, const std::string& inGot, const std::string& inWant)
{
    sChecks++;
    if (inGot == inWant)
        return;
    sFailures++;
    printf("FAIL %s\n  rust %s\n  orig %s\n", inWhat, Hex(inGot).c_str(), Hex(inWant).c_str());
}

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

static std::string Utf16(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    return aBytes;
}

static std::string FileBytes(const char* inPath)
{
    std::string aData;
    FILE* aFile = fopen(inPath, "rb");
    if (aFile == NULL)
        return "(none)";
    int c;
    while ((c = fgetc(aFile)) != EOF)
        aData += (char)c;
    fclose(aFile);
    return aData;
}

static void WriteFile(const char* inPath, const std::string& inData)
{
    FILE* aFile = fopen(inPath, "wb");
    fwrite(inData.data(), 1, inData.size(), aFile);
    fclose(aFile);
}

// ---- the original's data and statements (AuthContext.cpp before the port; the lock dropped)

struct OrigCDKey {
    bool mValid;
    ConstSmartPtr<ByteBuffer> mKey;
    OrigCDKey() : mValid(false) {}
};
struct OrigCommunityData {
    OrigCDKey mCDKey;
    RawBuffer mSimpleHash;
    RawBuffer mKeyedHashData;
};
struct OrigContext {
    struct SecretData {
        time_t mExpireTime;
        ConstSmartPtr<ByteBuffer> mSecret;
        SecretData() : mExpireTime(0) {}
        SecretData(time_t expires, const ByteBuffer* secret) : mExpireTime(expires), mSecret(secret) {}
    };
    typedef std::list<SecretData> SecretList;
    SecretList mSecretList;
    std::string mLoginSecretFile;
    std::map<std::wstring, OrigCommunityData> mCommunityMap;
    std::map<std::wstring, OrigCDKey> mCDKeyCommunityJoinMap;
    std::map<std::wstring, ConstSmartPtr<ByteBuffer> > mSetCommunityUserDataMap;
    std::map<std::wstring, std::wstring> mNicknameMap;

    bool LoadSecretList()
    {
        this->mSecretList.clear();
        FILE* file=PlatFOpen(this->mLoginSecretFile.c_str(),"rb");
        if(file==NULL) return false;
        unsigned char count=static_cast<unsigned char>(fgetc(file));
        int i=0;
        while(!feof(file) && i<count) {
            int32_t expires=0;
            fread(&expires,sizeof(expires),1,file);
            char* raw=new char[8];
            memset(raw,0,8); // (the original left a short read's rest uninitialized)
            fread(raw,1,8,file);
            this->mSecretList.push_back(SecretData(expires,new ByteBuffer(raw,8,true)));
            ++i;
        }
        fclose(file);
        return true;
    }
    bool SaveSecretList()
    {
        FILE* file=PlatFOpen(this->mLoginSecretFile.c_str(),"wb");
        if(file==NULL) return false;
        unsigned char count=static_cast<unsigned char>(this->mSecretList.size());
        fwrite(&count,1,1,file);
        for(SecretList::iterator next=this->mSecretList.begin();next!=this->mSecretList.end();++next) {
            int32_t expires=static_cast<int32_t>(next->mExpireTime);
            fwrite(&expires,sizeof(expires),1,file);
            fwrite(next->mSecret.get()!=NULL?next->mSecret->data():NULL,1,
                   next->mSecret.get()!=NULL?next->mSecret->length():0,file);
        }
        fclose(file);
        return true;
    }
    void AppendLoginSecrets(WriteBuffer& output)
    {
        SecretList::iterator next=this->mSecretList.begin();
        unsigned long countPosition=output.length();
        unsigned char count=0;
        output.SkipBytes(1);
        while(next!=this->mSecretList.end() && count<256) {
            output.AppendBytes(next->mSecret.get()!=NULL?next->mSecret->data():NULL,
                               next->mSecret.get()!=NULL?next->mSecret->length():0);
            ++count;
            ++next;
        }
        output.SetByte(countPosition,count);
    }
    void AddSecret(const void* secret)
    {
        this->mSecretList.insert(this->mSecretList.begin(),SecretData(0,new ByteBuffer(secret,8,false)));
        this->SaveSecretList();
    }
    void RemoveSecret(const void* secret)
    {
        SecretList::iterator next=this->mSecretList.begin();
        while(next!=this->mSecretList.end()) {
            if(next->mExpireTime==0) {
                const ByteBuffer* buffer=next->mSecret.get();
                if(memcmp(buffer!=NULL?buffer->data():NULL,secret,8)==0) {
                    this->mSecretList.erase(next);
                    this->SaveSecretList();
                    return;
                }
            }
            ++next;
        }
    }
    void CommitSecrets(time_t authTime,time_t duration)
    {
        if(this->mSecretList.empty()) return;
        SecretList::iterator next=this->mSecretList.begin();
        while(next!=this->mSecretList.end()) {
            if(next->mExpireTime==0) {
                next->mExpireTime=static_cast<time_t>(static_cast<unsigned long>(authTime)+static_cast<unsigned long>(duration)+120UL);
                ++next;
            } else if(next->mExpireTime<=authTime) this->mSecretList.erase(next++);
            else ++next;
        }
        while(this->mSecretList.size()>=256) this->mSecretList.pop_back();
        this->SaveSecretList();
    }
    void AppendHashes(WriteBuffer& output,const RawBuffer& challenge)
    {
        int count=0;
        unsigned long countPosition=output.length();
        output.SkipBytes(1);
        std::map<std::wstring,OrigCommunityData>::iterator next=this->mCommunityMap.begin();
        while(next!=this->mCommunityMap.end()) {
            OrigCommunityData& data=next->second;
            if(!data.mSimpleHash.empty()) {
                MD5Digest keyedHash;
                keyedHash.update(challenge);
                keyedHash.update(data.mKeyedHashData);
                RawBuffer keyed=keyedHash.digest();
                output.AppendByte(1);
                output.AppendWString(next->first,2);
                output.AppendBytes(data.mSimpleHash.data(),data.mSimpleHash.length());
                output.AppendBytes(keyed.data(),keyed.length());
                ++count;
            }
            ++next;
        }
        output.SetByte(countPosition,static_cast<unsigned char>(count));
    }
    void AppendCommunityData(WriteBuffer& output)
    {
        output.AppendByte(0);
        output.AppendByte(static_cast<unsigned char>(this->mCommunityMap.size()));
        std::map<std::wstring,OrigCommunityData>::iterator community=this->mCommunityMap.begin();
        while(community!=this->mCommunityMap.end()) {
            output.AppendWString(community->first,2);
            ++community;
        }
        unsigned long countPosition=output.length();
        output.SkipBytes(2);
        int count=0;
        community=this->mCommunityMap.begin();
        while(community!=this->mCommunityMap.end()) {
            if(community->second.mCDKey.mValid) {
                ConstSmartPtr<ByteBuffer> key(community->second.mCDKey.mKey.get());
                if(key.get()!=NULL) {
                    output.AppendByte(1);
                    output.AppendShort(static_cast<short>(WideUtf16Length(community->first)*2+key->length()+2));
                    output.AppendWString(community->first,2);
                    output.AppendBytes(key->data(),key->length());
                    ++count;
                }
            }
            ++community;
        }
        std::map<std::wstring,OrigCDKey>::iterator joined=this->mCDKeyCommunityJoinMap.begin();
        while(joined!=this->mCDKeyCommunityJoinMap.end()) {
            ConstSmartPtr<ByteBuffer> key(joined->second.mKey.get());
            if(key.get()!=NULL) {
                output.AppendByte(7);
                output.AppendShort(static_cast<short>(WideUtf16Length(joined->first)*2+6+key->length()));
                output.AppendWString(joined->first,2);
                output.AppendLong(0);
                output.AppendBytes(key->data(),key->length());
                ++count;
            }
            ++joined;
        }
        std::map<std::wstring,ConstSmartPtr<ByteBuffer> >::iterator userData=this->mSetCommunityUserDataMap.begin();
        while(userData!=this->mSetCommunityUserDataMap.end()) {
            const ByteBuffer* data=userData->second.get();
            if(data!=NULL) {
                output.AppendByte(8);
                output.AppendShort(static_cast<short>(WideUtf16Length(userData->first)*2+6+data->length()));
                output.AppendWString(userData->first,2);
                output.AppendLong(0);
                output.AppendBuffer(data,0);
                ++count;
            }
            ++userData;
        }
        if(this->mSecretList.size()>0) {
            output.AppendByte(6);
            unsigned long position=output.length();
            output.SkipBytes(2);
            this->AppendLoginSecrets(output);
            output.SetShort(position,output.length()-position-2);
            ++count;
        }
        std::map<std::wstring,std::wstring>::iterator nickname=this->mNicknameMap.begin();
        while(nickname!=this->mNicknameMap.end()) {
            const std::wstring& key=nickname->first;
            const std::wstring& value=nickname->second;
            output.AppendByte(4);
            unsigned long position=output.length();
            output.SkipBytes(2);
            output.AppendWString(key,2);
            output.SetShort(position,output.length()-position-2);
            ++count;
            if(!value.empty()) {
                output.AppendByte(3);
                position=output.length();
                output.SkipBytes(2);
                output.AppendWString(key,2);
                output.AppendWString(value,2);
                output.SetShort(position,output.length()-position-2);
                ++count;
            }
            ++nickname;
        }
        output.SetShort(countPosition,static_cast<short>(count));
    }
    // StartAutoRefresh's timer decision (the wait, -1 for none)
    static long long AutoRefreshWait(bool doAutoRefresh, bool force, bool valid, time_t expireDelta)
    {
        if(doAutoRefresh) {
            bool refreshTimer=force;
            time_t wait=300;
            if(valid) {
                wait=expireDelta-120;
                if(wait<120) wait=120;
                refreshTimer=true;
            }
            if(refreshTimer) return (long long)wait;
        }
        return -1;
    }
};

// ---- the port: the marshalling of the new AuthContext.cpp

struct EntryText {
    std::string mName, mValue, mBytes, mExtra;
};
static EeWonAuthEntry Entry(EntryText& ioText, bool inPresent)
{
    EeWonAuthEntry anEntry;
    anEntry.name = reinterpret_cast<const uint8_t*>(ioText.mName.data());
    anEntry.name_len = ioText.mName.size();
    anEntry.value = reinterpret_cast<const uint8_t*>(ioText.mValue.data());
    anEntry.value_len = ioText.mValue.size();
    anEntry.bytes = reinterpret_cast<const uint8_t*>(ioText.mBytes.data());
    anEntry.bytes_len = ioText.mBytes.size();
    anEntry.extra = reinterpret_cast<const uint8_t*>(ioText.mExtra.data());
    anEntry.extra_len = ioText.mExtra.size();
    anEntry.present = inPresent;
    return anEntry;
}
static std::string Raw(const ByteBuffer* inBuffer)
{
    return inBuffer != NULL ? std::string(inBuffer->data(), inBuffer->length()) : std::string();
}
static std::string Raw(const RawBuffer& inBuffer)
{
    return std::string(reinterpret_cast<const char*>(inBuffer.data()), inBuffer.size());
}

static std::string PortCommunityData(OrigContext& inData, EeWonAuthSecrets* inSecrets)
{
    std::vector<EntryText> aTexts;
    aTexts.reserve(inData.mCommunityMap.size() + inData.mCDKeyCommunityJoinMap.size() +
                   inData.mSetCommunityUserDataMap.size() + inData.mNicknameMap.size());
    std::vector<EeWonAuthEntry> aCommunities, aJoins, aUserData, aNicknames;
    for (std::map<std::wstring, OrigCommunityData>::iterator i = inData.mCommunityMap.begin();
         i != inData.mCommunityMap.end(); ++i) {
        aTexts.push_back(EntryText());
        aTexts.back().mName = Utf16(i->first);
        aTexts.back().mBytes = Raw(i->second.mCDKey.mKey.get());
        aCommunities.push_back(Entry(aTexts.back(), i->second.mCDKey.mValid && i->second.mCDKey.mKey.get() != NULL));
    }
    for (std::map<std::wstring, OrigCDKey>::iterator i = inData.mCDKeyCommunityJoinMap.begin();
         i != inData.mCDKeyCommunityJoinMap.end(); ++i) {
        aTexts.push_back(EntryText());
        aTexts.back().mName = Utf16(i->first);
        aTexts.back().mBytes = Raw(i->second.mKey.get());
        aJoins.push_back(Entry(aTexts.back(), i->second.mKey.get() != NULL));
    }
    for (std::map<std::wstring, ConstSmartPtr<ByteBuffer> >::iterator i = inData.mSetCommunityUserDataMap.begin();
         i != inData.mSetCommunityUserDataMap.end(); ++i) {
        aTexts.push_back(EntryText());
        aTexts.back().mName = Utf16(i->first);
        aTexts.back().mBytes = Raw(i->second.get());
        aUserData.push_back(Entry(aTexts.back(), i->second.get() != NULL));
    }
    for (std::map<std::wstring, std::wstring>::iterator i = inData.mNicknameMap.begin();
         i != inData.mNicknameMap.end(); ++i) {
        aTexts.push_back(EntryText());
        aTexts.back().mName = Utf16(i->first);
        aTexts.back().mValue = Utf16(i->second);
        aNicknames.push_back(Entry(aTexts.back(), true));
    }
    std::string anOut;
    ee_won_auth_community_data(aCommunities.empty() ? NULL : &aCommunities[0], aCommunities.size(),
                               aJoins.empty() ? NULL : &aJoins[0], aJoins.size(),
                               aUserData.empty() ? NULL : &aUserData[0], aUserData.size(),
                               aNicknames.empty() ? NULL : &aNicknames[0], aNicknames.size(), inSecrets, Append,
                               &anOut);
    return anOut;
}

static std::string PortHashes(OrigContext& inData, const RawBuffer& inChallenge)
{
    std::vector<EntryText> aTexts;
    aTexts.reserve(inData.mCommunityMap.size());
    std::vector<EeWonAuthEntry> anEntries;
    for (std::map<std::wstring, OrigCommunityData>::iterator i = inData.mCommunityMap.begin();
         i != inData.mCommunityMap.end(); ++i) {
        aTexts.push_back(EntryText());
        aTexts.back().mName = Utf16(i->first);
        aTexts.back().mBytes = Raw(i->second.mSimpleHash);
        aTexts.back().mExtra = Raw(i->second.mKeyedHashData);
        anEntries.push_back(Entry(aTexts.back(), true));
    }
    std::string anOut;
    ee_won_auth_hashes(anEntries.empty() ? NULL : &anEntries[0], anEntries.size(), inChallenge.data(),
                       inChallenge.size(), Append, &anOut);
    return anOut;
}

static std::string OrigList(OrigContext& inContext)
{
    std::string anOut;
    for (OrigContext::SecretList::iterator i = inContext.mSecretList.begin(); i != inContext.mSecretList.end(); ++i) {
        char aBuffer[32];
        snprintf(aBuffer, sizeof(aBuffer), "%lld:", (long long)i->mExpireTime);
        anOut += aBuffer + Hex(Raw(i->mSecret.get())) + " ";
    }
    return anOut;
}

static const char* const kOrigFile = "won_auth_orig.ks";
static const char* const kPortFile = "won_auth_port.ks";

// the Rust list as text, through its own save (the expiries the file keeps are 32-bit; the list's own are compared
// by the login bytes and the commit results)
static std::string PortFile(EeWonAuthSecrets* inSecrets)
{
    ee_won_auth_secrets_save(inSecrets, kPortFile);
    return FileBytes(kPortFile);
}

static std::string Login(OrigContext& inContext)
{
    WriteBuffer aBuffer(4, false);
    inContext.AppendLoginSecrets(aBuffer);
    return std::string(aBuffer.data(), aBuffer.length());
}
static std::string Login(EeWonAuthSecrets* inSecrets)
{
    std::string anOut;
    ee_won_auth_secrets_login(inSecrets, Append, &anOut);
    return anOut;
}

static void Key(unsigned char* outKey, int inSeed)
{
    for (int i = 0; i < 8; i++)
        outKey[i] = (unsigned char)(inSeed * 31 + i * 7);
}

static void RunSecrets()
{
    OrigContext anOrig;
    anOrig.mLoginSecretFile = kOrigFile;
    EeWonAuthSecrets* aPort = ee_won_auth_secrets_new();
    remove(kOrigFile);
    remove(kPortFile);
    Same("load missing", ee_won_auth_secrets_load(aPort, kPortFile) ? "1" : "0", anOrig.LoadSecretList() ? "1" : "0");
    unsigned char aKey[8];
    for (int i = 0; i < 5; i++) {
        Key(aKey, i);
        anOrig.AddSecret(aKey);
        ee_won_auth_secrets_add(aPort, aKey);
        ee_won_auth_secrets_save(aPort, kPortFile);
        Same("add file", FileBytes(kPortFile), FileBytes(kOrigFile));
    }
    Same("login 5", Login(aPort), Login(anOrig));
    Key(aKey, 2);
    anOrig.RemoveSecret(aKey);
    if (ee_won_auth_secrets_remove(aPort, aKey))
        ee_won_auth_secrets_save(aPort, kPortFile);
    Same("remove file", FileBytes(kPortFile), FileBytes(kOrigFile));
    Key(aKey, 2);
    anOrig.RemoveSecret(aKey);
    Same("remove again", ee_won_auth_secrets_remove(aPort, aKey) ? "1" : "0", "0");
    time_t aTimes[][2] = {{1000, 50}, {1100, 10}, {1300, 100}, {1171, 0}, {0x7fffffff, 0x7fffffff}, {-5, 3}};
    for (size_t t = 0; t < sizeof(aTimes) / sizeof(aTimes[0]); t++) {
        Key(aKey, 10 + (int)t);
        anOrig.AddSecret(aKey);
        ee_won_auth_secrets_add(aPort, aKey);
        anOrig.CommitSecrets(aTimes[t][0], aTimes[t][1]);
        if (ee_won_auth_secrets_commit(aPort, aTimes[t][0], aTimes[t][1]))
            ee_won_auth_secrets_save(aPort, kPortFile);
        Same("commit file", FileBytes(kPortFile), FileBytes(kOrigFile));
        Same("commit login", Login(aPort), Login(anOrig));
        Same("commit count", std::to_string(ee_won_auth_secrets_count(aPort)), std::to_string(anOrig.mSecretList.size()));
    }
    // the cap: 300 uncommitted secrets, committed
    for (int i = 0; i < 300; i++) {
        Key(aKey, 100 + i);
        anOrig.mSecretList.push_front(OrigContext::SecretData(0, new ByteBuffer(aKey, 8, false)));
        ee_won_auth_secrets_add(aPort, aKey);
    }
    Same("login 300", Login(aPort), Login(anOrig));
    anOrig.CommitSecrets(5000, 60);
    if (ee_won_auth_secrets_commit(aPort, 5000, 60))
        ee_won_auth_secrets_save(aPort, kPortFile);
    Same("cap file", FileBytes(kPortFile), FileBytes(kOrigFile));
    // loads of odd files: cut records, a count past the records, an empty file, a lone count
    std::string aFull = FileBytes(kOrigFile);
    const size_t aCuts[] = {0, 1, 2, 5, 13, 14, 19, 25, 37, aFull.size() - 1, aFull.size()};
    for (size_t c = 0; c < sizeof(aCuts) / sizeof(aCuts[0]); c++) {
        for (int aCount = -1; aCount < 3; aCount++) {
            std::string aData = aFull.substr(0, aCuts[c]);
            if (aCount >= 0 && !aData.empty())
                aData[0] = (char)(aCount == 2 ? 255 : aCount);
            WriteFile(kOrigFile, aData);
            WriteFile(kPortFile, aData);
            bool anOrigOk = anOrig.LoadSecretList();
            bool aPortOk = ee_won_auth_secrets_load(aPort, kPortFile);
            Same("load ok", aPortOk ? "1" : "0", anOrigOk ? "1" : "0");
            Same("load login", Login(aPort), Login(anOrig));
            anOrig.SaveSecretList();
            Same("load save", PortFile(aPort), FileBytes(kOrigFile));
        }
    }
    ee_won_auth_secrets_free(aPort);
    remove(kOrigFile);
    remove(kPortFile);
}

static ConstSmartPtr<ByteBuffer> Buffer(const char* inText) { return new ByteBuffer(inText, strlen(inText), false); }

static void RunCommunityData()
{
    OrigContext aData;
    EeWonAuthSecrets* aSecrets = ee_won_auth_secrets_new();
    for (int aRound = 0; aRound < 6; aRound++) {
        WriteBuffer anOut(4, false);
        anOut.AppendLong(0x11223344); // something before the block: the counts' positions are not 0
        aData.AppendCommunityData(anOut);
        std::string aWant(anOut.data() + 4, anOut.length() - 4);
        char aWhat[32];
        snprintf(aWhat, sizeof(aWhat), "community data %d", aRound);
        Same(aWhat, PortCommunityData(aData, aSecrets), aWant);
        RawBuffer aChallenge(reinterpret_cast<const unsigned char*>("challenge seed"), 14);
        WriteBuffer aHashes(4, false);
        aHashes.AppendByte(9);
        aData.AppendHashes(aHashes, aChallenge);
        snprintf(aWhat, sizeof(aWhat), "hashes %d", aRound);
        Same(aWhat, PortHashes(aData, aChallenge), std::string(aHashes.data() + 1, aHashes.length() - 1));
        // grow the maps for the next round
        switch (aRound) {
        case 0: {
            aData.mCommunityMap[L"EmpireEarth"].mCDKey.mValid = true;
            aData.mCommunityMap[L"EmpireEarth"].mCDKey.mKey = Buffer("KEY-0001");
            aData.mCommunityMap[L"Other"];
            aData.mCommunityMap[L"Invalid"].mCDKey.mKey = Buffer("no");
            break;
        }
        case 1:
            aData.mCDKeyCommunityJoinMap[L"Join\x00e9"].mKey = Buffer("JK");
            aData.mCDKeyCommunityJoinMap[L"NoKey"];
            aData.mSetCommunityUserDataMap[L"Data"] = Buffer("userdata");
            aData.mSetCommunityUserDataMap[L"Null"] = NULL;
            break;
        case 2: {
            unsigned char aKey[8];
            Key(aKey, 1);
            ee_won_auth_secrets_add(aSecrets, aKey);
            aData.mSecretList.push_front(OrigContext::SecretData(0, new ByteBuffer(aKey, 8, false)));
            Key(aKey, 2);
            ee_won_auth_secrets_add(aSecrets, aKey);
            aData.mSecretList.push_front(OrigContext::SecretData(0, new ByteBuffer(aKey, 8, false)));
            break;
        }
        case 3:
            aData.mNicknameMap[L"nick"] = L"";
            aData.mNicknameMap[L"alias"] = L"Name \x263a";
            break;
        case 4: {
            RawBuffer aHash(16, (unsigned char)0xab);
            aData.mCommunityMap[L"EmpireEarth"].mSimpleHash = aHash;
            aData.mCommunityMap[L"EmpireEarth"].mKeyedHashData = RawBuffer(48, (unsigned char)0x5c);
            aData.mCommunityMap[L"Other"].mSimpleHash = RawBuffer(16, (unsigned char)1);
            for (int i = 0; i < 300; i++) {
                wchar_t aName[16];
                swprintf(aName, 16, L"c%03d", i);
                aData.mCommunityMap[aName];
            }
            break;
        }
        }
    }
    ee_won_auth_secrets_free(aSecrets);
}

static void RunAutoRefresh()
{
    const time_t aDeltas[] = {-1000, 0, 119, 120, 239, 240, 241, 100000};
    for (int e = 0; e < 2; e++)
        for (int f = 0; f < 2; f++)
            for (int v = 0; v < 2; v++)
                for (size_t d = 0; d < sizeof(aDeltas) / sizeof(aDeltas[0]); d++)
                    Same("auto refresh",
                         std::to_string(ee_won_auth_auto_refresh_wait(e != 0, f != 0, v != 0, aDeltas[d])),
                         std::to_string(OrigContext::AutoRefreshWait(e != 0, f != 0, v != 0, aDeltas[d])));
}

// ---- the login data as one object: sequences of AuthContext's setters, file hashes and secrets over the original's
// statements (AuthContext.cpp before round 13, without its lock: OrigLogin below, the maps and MD5 loop as they
// were) against ee_won's login object (authlogin.rs) through its C ABI. After each step the members, the
// community data block, the hashes block and the secrets' block must be the same. `--print N` prints N sequences
// with what the original did (rust/crates/ee_won/src/golden_authlogin.txt; `authlogin::tests::golden` replays it).

static unsigned int sSeed = 5151;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

struct OrigLogin : public OrigContext {
    std::wstring mName;
    std::wstring mPassword;
    void SetUserName(const std::wstring& name) { this->mName=name; }
    void SetPassword(const std::wstring& password) { this->mPassword=password; }
    void AddCommunity(const std::wstring& community)
    {
        this->mCommunityMap.insert(std::map<std::wstring,OrigCommunityData>::value_type(community,OrigCommunityData()));
    }
    void SetCommunity(const std::wstring& community)
    {
        std::map<std::wstring,OrigCommunityData>::iterator found=this->mCommunityMap.find(community);
        if(found!=this->mCommunityMap.end()) {
            OrigCommunityData data=found->second;
            this->mCommunityMap.clear();
            this->mCommunityMap[community]=data;
        }
        else {
            this->mCommunityMap.clear();
            this->AddCommunity(community);
        }
    }
    void RemoveCommunity(const std::wstring& community) { this->mCommunityMap.erase(community); }
    void RemoveAllCommunity() { this->mCommunityMap.clear(); }
    void SetCDKey(const std::wstring& community,const OrigCDKey& key) { this->mCommunityMap[community].mCDKey=key; }
    void AddCDKeyCommunityJoin(const std::wstring& theCommunity,const OrigCDKey& theKey) { this->mCDKeyCommunityJoinMap[theCommunity]=theKey; }
    void ClearCDKeyCommunityJoins() { this->mCDKeyCommunityJoinMap.clear(); }
    void AddUserData(const std::wstring& theCommunity,const ByteBuffer* theData) { this->mSetCommunityUserDataMap[theCommunity]=theData; }
    void ClearUserData() { this->mSetCommunityUserDataMap.clear(); }
    void ClearOneTimeData() { this->ClearUserData(); this->ClearCDKeyCommunityJoins(); }
    void AddNickname(const std::wstring& theKey) { this->mNicknameMap[theKey]=L""; }
    void SetNickname(const std::wstring& theKey,const std::wstring& theName) { this->mNicknameMap[theKey]=theName; }
    void ClearNicknames() { this->mNicknameMap.clear(); }
    bool SetHashFileList(const std::wstring& community,const std::list<std::string>& files)
    {
        OrigCommunityData& data=this->mCommunityMap[community];
        data.mSimpleHash.erase();
        data.mKeyedHashData.erase();
        MD5 simpleHash;
        MD5 hashSection;
        int sectionPosition=0;
        std::list<std::string>::const_iterator next=files.begin();
        while(next!=files.end()) {
            FILE* file=PlatFOpen(next->c_str(),"rb");
            if(file==NULL) return false;
            ++next;
            char buffer[16384];
            while(!feof(file)) {
                int count=fread(buffer,1,16384-sectionPosition,file);
                if(count<=0)
                    continue;
                sectionPosition+=count;
                simpleHash.Update(buffer,count);
                hashSection.Update(buffer,count);
                unsigned char digest[16];
                if(sectionPosition!=16384 && (!feof(file) || next!=files.end()))
                    continue;
                sectionPosition=0;
                hashSection.Digest(digest);
                data.mKeyedHashData.append(digest,16);
                hashSection.Reset();
            }
            fclose(file);
        }
        unsigned char digest[16];
        simpleHash.Digest(digest);
        data.mSimpleHash.assign(digest,16);
        return true;
    }
};

static std::string RawHex(const RawBuffer& inBuffer)
{
    return Hex(std::string(reinterpret_cast<const char*>(inBuffer.data()), inBuffer.size()));
}
static std::string KeyHex(const ConstSmartPtr<ByteBuffer>& inKey)
{
    return inKey.get() == NULL ? std::string("n") : Hex(std::string(inKey->data(), inKey->length()));
}
// the members as authlogin.rs's describe prints them (empty bytes: "-")
static std::string Dash(const std::string& inHex) { return inHex.empty() ? std::string("-") : inHex; }
static std::string Describe(OrigLogin& o)
{
    std::string s = "name " + Dash(Hex(Utf16(o.mName))) + " pass " + Dash(Hex(Utf16(o.mPassword))) + " c";
    for (std::map<std::wstring, OrigCommunityData>::iterator i = o.mCommunityMap.begin(); i != o.mCommunityMap.end(); ++i)
        s += " " + Dash(Hex(Utf16(i->first))) + ":" + Dash(KeyHex(i->second.mCDKey.mKey)) + "/" +
             (i->second.mCDKey.mValid ? "1" : "0") + "/" + Dash(RawHex(i->second.mSimpleHash)) + "/" +
             Dash(RawHex(i->second.mKeyedHashData));
    s += " j";
    for (std::map<std::wstring, OrigCDKey>::iterator i = o.mCDKeyCommunityJoinMap.begin(); i != o.mCDKeyCommunityJoinMap.end(); ++i)
        s += " " + Dash(Hex(Utf16(i->first))) + ":" + Dash(KeyHex(i->second.mKey));
    s += " d";
    for (std::map<std::wstring, ConstSmartPtr<ByteBuffer> >::iterator i = o.mSetCommunityUserDataMap.begin();
         i != o.mSetCommunityUserDataMap.end(); ++i)
        s += " " + Dash(Hex(Utf16(i->first))) + ":" + Dash(KeyHex(i->second));
    s += " n";
    for (std::map<std::wstring, std::wstring>::iterator i = o.mNicknameMap.begin(); i != o.mNicknameMap.end(); ++i)
        s += " " + Dash(Hex(Utf16(i->first))) + ":" + Dash(Hex(Utf16(i->second)));
    return s;
}
static std::string Blocks(OrigLogin& o)
{
    WriteBuffer c(4, false);
    o.AppendCommunityData(c);
    WriteBuffer h(4, false);
    o.AppendHashes(h, RawBuffer(reinterpret_cast<const unsigned char*>("ch"), 2));
    WriteBuffer l(4, false);
    o.AppendLoginSecrets(l);
    return " | " + Dash(Hex(std::string(c.data(), c.length()))) + " " + Dash(Hex(std::string(h.data(), h.length()))) +
           " " + Dash(Hex(std::string(l.data(), l.length())));
}
static void Sink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
static std::string Blocks(EeWonAuthLogin* r)
{
    std::string c, h, l;
    ee_won_auth_login_block(r, 0, NULL, 0, Sink, &c);
    ee_won_auth_login_block(r, 1, reinterpret_cast<const uint8_t*>("ch"), 2, Sink, &h);
    ee_won_auth_login_block(r, 2, NULL, 0, Sink, &l);
    return " | " + Dash(Hex(c)) + " " + Dash(Hex(h)) + " " + Dash(Hex(l));
}

static const wchar_t* const kNames[] = { L"EmpireEarth", L"Admin", L"", L"\x00e9t\x00e9", L"Z", L"\x263a" };
static const char* const kBytes[] = { "KEY-0001", "", "xy", "user data 123" };
// a hashed file's content: byte i of file `seed` (the replay makes the same bytes)
static std::string FileContent(int inSeed, int inSize)
{
    std::string s(inSize, '\0');
    for (int i = 0; i < inSize; i++) s[i] = (char)((inSeed * 7 + i * 13 + (i >> 9)) & 0xff);
    return s;
}
static const int kSizes[] = { 0, 1, 100, 16383, 16384, 16385, 40000, -1 };

// one sequence: `inSteps` steps on both halves; printed (the original's) or compared
static void LoginSequence(int inSteps, bool inPrint)
{
    OrigLogin o;
    o.mLoginSecretFile = "won_auth_login_orig.ks";
    remove("won_auth_login_orig.ks");
    remove("won_auth_login_port.ks");
    const char* aPortFile = "won_auth_login_port.ks";
    EeWonAuthLogin* r = ee_won_auth_login_new(reinterpret_cast<const uint8_t*>(aPortFile), strlen(aPortFile));
    if (inPrint) printf("S\n");
    for (int k = 0; k < inSteps; k++) {
        int op = Pick(20);
        std::wstring key = kNames[Pick(6)];
        std::string value = Pick(5) ? kBytes[Pick(4)] : std::string();
        std::wstring text = kNames[Pick(6)];
        int flags = Pick(4);
        std::string line = "L " + std::to_string(op) + " " + Dash(Hex(Utf16(key))) + " ";
        std::string args;
        std::string utf = Utf16(text);
        switch (op) {
        case 1: o.SetUserName(text); args = Dash(Hex(utf)); ee_won_auth_login_set(r, op, NULL, 0, reinterpret_cast<const uint8_t*>(utf.data()), utf.size(), 0); break;
        case 2: o.SetPassword(text); args = Dash(Hex(utf)); ee_won_auth_login_set(r, op, NULL, 0, reinterpret_cast<const uint8_t*>(utf.data()), utf.size(), 0); break;
        case 15: o.SetNickname(key, text); args = Dash(Hex(utf)); break;
        case 8: case 9: case 11: {
            args = Dash(Hex(value)) + " " + std::to_string(flags);
            ConstSmartPtr<ByteBuffer> b = (flags & 1) ? new ByteBuffer(value.data(), (unsigned long)value.size(), false) : NULL;
            OrigCDKey c;
            c.mKey = b;
            c.mValid = (flags & 2) != 0;
            if (op == 8) o.SetCDKey(key, c);
            else if (op == 9) o.AddCDKeyCommunityJoin(key, c);
            else o.AddUserData(key, b.get());
            break;
        }
        case 4: o.AddCommunity(key); break;
        case 5: o.SetCommunity(key); break;
        case 6: o.RemoveCommunity(key); break;
        case 7: o.RemoveAllCommunity(); break;
        case 10: o.ClearCDKeyCommunityJoins(); break;
        case 12: o.ClearUserData(); break;
        case 13: o.ClearOneTimeData(); break;
        case 14: o.AddNickname(key); break;
        case 16: o.ClearNicknames(); break;
        case 0: case 17: { // SetHashFileList: up to 3 files of the sizes (-1: missing)
            int n = 1 + Pick(3);
            std::list<std::string> files;
            std::string paths;
            args = std::to_string(n);
            for (int f = 0; f < n; f++) {
                int size = kSizes[Pick(8)];
                int seed = Pick(50);
                char path[64];
                snprintf(path, sizeof(path), "won_auth_h%d_%d.bin", k, f);
                remove(path);
                if (size >= 0) {
                    std::string c = FileContent(seed, size);
                    FILE* fp = fopen(path, "wb");
                    if (!c.empty()) fwrite(c.data(), 1, c.size(), fp);
                    fclose(fp);
                }
                files.push_back(path);
                paths += path;
                paths += '\0';
                args += " " + std::to_string(size) + ":" + std::to_string(seed);
            }
            bool a = o.SetHashFileList(key, files);
            std::string ku = Utf16(key);
            bool b = ee_won_auth_login_hash_files(r, reinterpret_cast<const uint8_t*>(ku.data()), ku.size(),
                                                  reinterpret_cast<const uint8_t*>(paths.data()), paths.size());
            args += std::string(" =") + (a ? "1" : "0");
            if (!inPrint) Same("hash files", b ? "1" : "0", a ? "1" : "0");
            for (std::list<std::string>::iterator i = files.begin(); i != files.end(); ++i) remove(i->c_str());
            break;
        }
        case 18: case 19: { // a secret added and removed again at times, or the secrets committed
            unsigned char aKey[8];
            Key(aKey, Pick(4));
            args = Hex(std::string(reinterpret_cast<const char*>(aKey), 8)) + " " + std::to_string(flags);
            if (op == 18) {
                if (flags & 1) {
                    o.RemoveSecret(aKey);
                    ee_won_auth_login_secrets(r, 3, aKey, 0, 0);
                } else {
                    o.AddSecret(aKey);
                    ee_won_auth_login_secrets(r, 2, aKey, 0, 0);
                }
            } else {
                time_t t = 1000 + flags * 400;
                o.CommitSecrets(t, 300);
                ee_won_auth_login_secrets(r, 4, NULL, (int64_t)t, 300);
            }
            break;
        }
        default: break;
        }
        // the setters' Rust half (the hash files and secrets ran above)
        if (op >= 4 && op <= 16) {
            std::string ku = Utf16(key);
            std::string v = op == 15 ? utf : value;
            ee_won_auth_login_set(r, op, reinterpret_cast<const uint8_t*>(ku.data()), ku.size(),
                                  reinterpret_cast<const uint8_t*>(v.data()), v.size(), flags);
        }
        std::string want = Describe(o) + Blocks(o);
        if (inPrint) {
            printf("%s%s => %s\n", line.c_str(), Dash(args).c_str(), want.c_str());
            continue;
        }
        std::string d;
        ee_won_auth_login_describe(r, Sink, &d);
        Same("login step", d + Blocks(r), want);
        if (sFailures > 0 && sFailures < 4 && d + Blocks(r) != want) printf("  at %s%s\n", line.c_str(), args.c_str());
        Same("secret file", FileBytes(aPortFile), FileBytes("won_auth_login_orig.ks"));
    }
    ee_won_auth_login_free(r);
    remove("won_auth_login_orig.ks");
    remove("won_auth_login_port.ks");
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i + 1 < argc; i += 2)
        if (strcmp(argv[i], "--print") == 0) aPrint = atoi(argv[i + 1]);
    if (aPrint > 0) {
        for (int q = 0; q < aPrint; q++) LoginSequence(4 + Pick(30), true);
        return 0;
    }
    RunSecrets();
    RunCommunityData();
    RunAutoRefresh();
    for (int q = 0; q < 3000; q++) LoginSequence(4 + Pick(30), false);
    printf("won_auth_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
