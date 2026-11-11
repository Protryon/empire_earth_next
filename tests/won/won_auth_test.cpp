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
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONMD5Digest.h"
#include "platform/PlatFile.h"
#include "rust/ee_rust.h"

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

int main()
{
    RunSecrets();
    RunCommunityData();
    RunAutoRefresh();
    printf("won_auth_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
