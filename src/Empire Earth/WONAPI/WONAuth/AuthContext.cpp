// AuthContext: login credentials, communities, CD keys, login secrets, peer-session cache and certificate refresh
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONMD5Digest.h"
#include "ee/wonapi/WONMD5.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONBuffer.h"
#include "platform/PlatFile.h"
#include "platform/PlatPath.h"
// File-local lazy critical section: the original's getter is 0x006ddda0 (50 bytes), its atexit cleanup 0x2ddde0
static WONAPI::CriticalSection& GetVerifierCrit()
{
    static WONAPI::CriticalSection verifierCritical;
    return verifierCritical;
}


static char VerifierKeyBuf[] = 
	"\x30\x82\x03\x2c\x02\x82\x01\x01\x00\x86\x18\x0e\x3d\xd4\x9e\xfa\x42\xb9\x4c\xb3\x3e"
	"\xf7\x2b\x76\xce\xe6\xb0\x27\x8c\x64\x07\x93\x3b\x24\x64\x6c\x47\xb6\x9e\x51\x7b\xbf"
	"\xc9\xd5\xa1\xfb\xcf\x04\xfa\x7b\x8e\xc3\x6e\xd1\x88\xf6\x2e\xbd\x2b\x29\xda\xb5\x76"
	"\x66\x12\xb9\x7a\x64\x73\x19\x53\xa9\x78\xd8\x1e\xe5\x11\x0a\x23\x1c\x08\x21\x33\xad"
	"\x65\x76\x48\xff\xc5\x0b\xa2\xaa\x10\x22\xcb\x63\xbb\x68\xfc\xf0\x15\xf3\xe9\x90\x5f"
	"\x9d\x17\x8e\x77\x67\xec\xde\xdf\x8b\xcc\xbb\xa4\xd6\x06\xfa\x0e\xd8\xc0\x67\xd4\x36"
	"\xc9\x3a\xeb\x3f\xa4\x93\x57\xad\x62\x29\x24\x08\x67\x91\x6a\x0b\xd4\xdc\xad\xa4\x86"
	"\x85\x9f\xed\xf8\xb8\xda\xe1\xac\xf3\xf4\x44\xaf\x85\x8d\x21\x95\x6a\xf4\x6e\xac\x36"
	"\x83\x8f\x70\x0a\xc6\x17\xc1\x90\xc4\x7f\xbb\xe8\x18\xed\x4d\x0b\x8c\xdc\x76\x7f\xa9"
	"\xb2\x45\x0f\xc6\xa8\xca\x0a\x9f\xec\x9f\xe5\xe0\xbd\x71\x63\x06\xef\x32\xba\xe0\x40"
	"\x47\xf1\x91\xa8\xbf\x5e\xad\xb8\x2d\x28\x3a\x94\x63\xfa\x85\x1d\x24\xe3\xd2\x78\x7f"
	"\x0e\x85\xb1\xbb\x3f\x2d\x27\x16\x34\xcf\x94\x7d\x71\xdd\xf3\xec\x41\xc1\x27\xe5\x6f"
	"\xe6\x88\x78\xc8\x65\x15\x45\x3c\x0c\x5f\x30\x60\xc7\x02\x1d\x02\xe1\x4f\x25\x13\x97"
	"\xee\xdb\x46\x94\xb2\x54\xfc\x54\x26\x20\xa2\x6d\x57\x72\xdd\x47\xd2\xf0\x92\x00\x38"
	"\x51\xad\x02\x82\x01\x00\x72\x44\x9d\x49\x40\xff\x84\x75\xd8\x73\x57\x1f\xa7\xff\x52"
	"\x08\x82\x63\xfa\xd1\xa7\x30\xb8\x63\xc5\x10\x45\x8e\x12\xca\x9c\x36\x4f\xc8\x3e\xe8"
	"\x9a\x95\xfc\x48\xae\xcf\x80\xea\x75\xf6\x11\x3b\x2a\xf0\xd9\x52\xfb\x66\xee\x4b\xdb"
	"\x6c\xc4\x06\x28\x12\x28\xd2\x2c\x53\x3f\x08\x86\x89\x69\x6a\x89\x57\x7e\xb0\xd7\x81"
	"\xac\x79\xae\xfb\x56\xc0\x24\x0d\x84\x5a\xe1\x22\xc8\x3f\xba\x3c\x77\xbc\x22\x56\xdf"
	"\xf1\x78\x55\xcc\xd5\x97\x88\x00\x71\x1a\xe8\x8e\xa7\xb6\x87\x62\x55\x4d\x53\x87\xa2"
	"\x49\x3e\xa2\x79\x99\x51\x94\xd7\xe2\xf5\xce\x37\x1e\xc7\x79\xbd\xea\x0a\x31\x56\x1d"
	"\x56\x1a\x8c\xb7\xf3\xa5\xdc\x2a\xcd\xdc\xb1\x5c\x3e\x47\x20\xc7\xc2\x30\x32\xdf\x64"
	"\x91\xd5\xc6\xab\x85\x21\x67\x59\xc6\x09\x7c\x61\xd5\x90\x9b\xff\x5c\x6b\x48\x5e\x41"
	"\x85\xdb\x12\xed\x63\x04\xd3\x35\xca\x86\xce\x64\xa6\x54\x5b\x0f\x77\x90\x7e\xad\xd9"
	"\x56\x5d\x2f\x8a\x52\xd9\xcc\x79\x41\x69\x73\xca\x15\x0a\xbc\x68\x93\xbe\x2f\x03\x7c"
	"\x60\x7b\xf2\xa3\x03\x98\x34\x36\x9f\x88\x8e\xfb\x2b\x8a\x40\xdd\x35\xa9\x21\x2d\x92"
	"\xba\x1b\xe8\x58\xe1\x1c\xc2\x47\x34\x9c\x02\x82\x01\x00\x7f\xb4\x68\x1d\xde\x12\xd3"
	"\x00\x3a\x26\xb6\x60\x7e\xb9\x62\x9f\x31\xcb\x6b\x3a\x4f\x0e\x18\x64\x4f\x65\xad\x79"
	"\x1d\xd7\xe8\xbf\x83\x68\x78\xc4\x9e\x2b\x90\xcd\xd8\xad\x36\x1a\x1d\x4f\x56\x4b\x8a"
	"\x6f\xc3\x77\x93\x5e\xf4\x7d\x61\xfd\x48\xa5\xa5\xbe\x27\xa3\x5d\xa9\x0a\xb0\x77\xc9"
	"\x6d\x8d\xae\x94\x5a\x90\x07\xb4\x96\x1b\xda\x93\x56\xc5\x50\x19\xdd\xa4\xb3\x40\x66"
	"\x0b\xb0\x55\xdd\xd7\x9a\x50\x7b\x9a\xe7\xbc\x63\x65\x9b\x45\x2d\x82\xf3\xe7\x49\x19"
	"\x71\x10\x9d\x46\x2e\x82\x8e\x15\x35\xf8\x53\x99\x3f\x4f\xec\x83\x48\xef\x0f\x27\x28"
	"\x75\x1f\x73\x97\x1e\x14\x16\x24\x16\xe6\x3f\x67\xdc\x50\x4a\xdc\x83\x89\x47\xf9\x41"
	"\xa5\xec\xe1\x86\x08\x46\xf4\xa9\x97\x14\x73\xaf\xe6\x01\x30\xa1\xad\x2f\xb0\x2a\x35"
	"\xdd\xad\x1d\x70\x25\x95\xbf\x07\xd2\x8c\x14\x46\xc4\xd2\x79\x21\xdc\xa9\x21\x6e\xff"
	"\x99\x69\x47\x8b\x67\x95\x98\x4d\x09\x75\xee\x97\x0b\xd9\x3d\x75\xd1\xa8\x82\x1f\xc8"
	"\x15\x2e\x39\x29\x6f\x95\x4b\xf4\x94\x74\xeb\xd8\xe3\x4a\xfb\x41\xa8\x4c\x5c\x3d\xb6"
	"\x42\xc0\x15\x35\x9e\x1a\xe2\x54\x2c\x0c\x0b\x34\x5f\xe2\x35\x8d\x30\x73";
namespace WONAPI {
// Class statics: the signing public key (0x566018, 0x1a8 bytes) and its file-checked flag (0x5661e1)
ElGamal AuthContext::mVerifierKey(VerifierKeyBuf,sizeof(VerifierKeyBuf),false);
bool AuthContext::mCheckedVerifierFile=false;

// FUNCTION: WON 0x006ddb70
// Sets up the peer data, login-secret file path and server context, and loads the key file once per process
void AuthContext::Init()
{
    this->mSessionCleanItr=this->mSessionMap.begin();
    this->mPeerData=new AuthPeerData;
    this->mLoginSecretFile=WONAPICore::GetDefaultFileDirectory()+"_wonlogin.ks";
    this->mServerContext=new ServerContext;
    this->mDoAutoRefresh=false;
    this->mRefreshStatus=0x186a4;
    AutoCrit lock(GetVerifierCrit());
    if(AuthContext::mCheckedVerifierFile) return;
    AuthContext::LoadVerifierKey(WONAPICore::GetDefaultFileDirectory()+"_wonkver.pub");
}
// FUNCTION: WON 0x006dddf0
// Creates an empty auth context
AuthContext::AuthContext()
{
    this->Init();
}
// FUNCTION: WON 0x006de370
// Destroys the auth context (members only)
AuthContext::~AuthContext()
{
}

// FUNCTION: WON 0x006deeb0
// Caches a peer session for an address and drops up to five map entries unused for over 300 seconds
void AuthContext::PutSession(const IPAddr& address,AuthSession* session)
{
    AutoCrit lock(this->mDataCrit);
    if(session==NULL) return;
    SessionStruct& record=this->mSessionMap[address];
    time_t now=time(NULL);
    record.mLastUseTime=now;
    record.mSessionList.push_back(session);
    for(int i=0;i<5;++i) {
        if(this->mSessionCleanItr==this->mSessionMap.end()) {
            this->mSessionCleanItr=this->mSessionMap.begin();
            break;
        }
        if(now-this->mSessionCleanItr->second.mLastUseTime>300)
            this->mSessionMap.erase(this->mSessionCleanItr++);
        else ++this->mSessionCleanItr;
    }
}
// FUNCTION: WON 0x006df080
// Takes a session used within 300 seconds for the address out of the cache; NULL when none
SmartPtr<AuthSession> AuthContext::GetSession(const IPAddr& address)
{
    AutoCrit lock(this->mDataCrit);
    SessionMap::iterator found=this->mSessionMap.find(address);
    if(found==this->mSessionMap.end()) return NULL;
    time_t now=time(NULL);
    SessionStruct& record=found->second;
    record.mLastUseTime=now;
    SessionList& sessions=record.mSessionList;
    SessionList::iterator next=sessions.begin();
    while(next!=sessions.end()) {
        SmartPtr<AuthSession> session=*next;
        sessions.erase(next++);
        if(now-session->GetLastUseTime()<300) return session;
    }
    return NULL;
}
// FUNCTION: WON 0x006de7f0
// Starts a certificate refresh when the current auto-refresh timer fires (not when killed)
void AuthContext::AutoRefreshCallback(AsyncOp* op)
{
    AutoCrit lock(this->mDataCrit);
    if(op!=this->mAutoRefreshOp.get()) return;
    this->mAutoRefreshOp=NULL;
    if(op->mKilled) return;
    this->RefreshAsync(NULL,INFINITE);
}
// FUNCTION: WON 0x006de8e0
// Kills and drops the pending auto-refresh timer
void AuthContext::StopAutoRefresh()
{
    AutoCrit lock(this->mDataCrit);
    if(this->mAutoRefreshOp.get()!=NULL) {
        this->mAutoRefreshOp->Kill(true);
        this->mAutoRefreshOp=NULL;
    }
}
// FUNCTION: WON 0x006deb30
// Turns certificate auto-refresh on or off and restarts the timer
void AuthContext::SetDoAutoRefresh(bool enabled)
{
    AutoCrit lock(this->mDataCrit);
    this->mDoAutoRefresh=enabled;
    this->StartAutoRefresh(false);
}
// FUNCTION: WON 0x006df260
// Kills the running certificate request and the auto-refresh timer
void AuthContext::Kill()
{
    AutoCrit lock(this->mDataCrit);
    if(this->mGetCertOp.get()!=NULL) this->mGetCertOp->Kill(true);
    this->StopAutoRefresh();
}

// FUNCTION: WON 0x006de760
// Forwards the auto-refresh timer completion to its context
void AuthContext::StaticAutoRefreshCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> parameter)
{
    static_cast<AuthContext*>(parameter.get())->AutoRefreshCallback(op.get());
}
// FUNCTION: WON 0x006de970
// Restarts the auto-refresh timer: 2 minutes before the certificate expires (at least 2), or 5 when forced
void AuthContext::StartAutoRefresh(bool force)
{
    AutoCrit lock(this->mDataCrit);
    this->StopAutoRefresh();
    if(this->mDoAutoRefresh) {
        bool refreshTimer=force;
        time_t wait=300;
        if(this->mPeerData->IsValid()) {
            wait=this->mPeerData->GetExpireDelta()-120;
            if(wait<120) wait=120;
            refreshTimer=true;
        }
        if(refreshTimer) {
            this->mAutoRefreshOp=new AsyncOp;
            this->mAutoRefreshOp->mCompletion=
                new ParamCompletion<SmartPtr<AsyncOp>,SmartPtr<RefCount> >(&AuthContext::StaticAutoRefreshCallback,this);
            this->mAutoRefreshOp->RunAsync(static_cast<long>(static_cast<unsigned long>(wait)*1000UL));
        }
    }
}


inline AuthContext::SecretData::SecretData() : mExpireTime(0) {}
inline AuthContext::SecretData::SecretData(time_t expires,const ByteBuffer* secret)
    : mExpireTime(expires),mSecret(secret) {}

// FUNCTION: WON 0x006e1240
// Reads the saved login secrets (expiry + 8 bytes each) from the secret file; false when it cannot open
bool AuthContext::LoadSecretList()
{
    AutoCrit lock(this->mDataCrit);
    this->mSecretList.clear();
    FILE* file=PlatFOpen(this->mLoginSecretFile.c_str(),"rb");
    if(file==NULL) return false;
    unsigned char count=static_cast<unsigned char>(fgetc(file));
    int i=0;
    while(!feof(file) && i<count) {
        int32_t expires=0;                  // the file's expiry is 32-bit (the original's time_t)
        fread(&expires,sizeof(expires),1,file);
        char* raw=new char[8];
        fread(raw,1,8,file);
        this->mSecretList.push_back(SecretData(expires,new ByteBuffer(raw,8,true)));
        ++i;
    }
    fclose(file);
    return true;
}
// FUNCTION: WON 0x006e1460
// Writes the login secrets to the secret file; false when it cannot be created
bool AuthContext::SaveSecretList()
{
    AutoCrit lock(this->mDataCrit);
    FILE* file=PlatFOpen(this->mLoginSecretFile.c_str(),"wb");
    if(file==NULL) return false;
    unsigned char count=static_cast<unsigned char>(this->mSecretList.size());
    fwrite(&count,1,1,file);
    for(SecretList::iterator next=this->mSecretList.begin();next!=this->mSecretList.end();++next) {
        int32_t expires=static_cast<int32_t>(next->mExpireTime);   // 32-bit in the file
        fwrite(&expires,sizeof(expires),1,file);
        fwrite(next->mSecret.get()!=NULL?next->mSecret->data():NULL,1,
               next->mSecret.get()!=NULL?next->mSecret->length():0,file);
    }
    fclose(file);
    return true;
}
// FUNCTION: WON 0x006e1560
// Appends a count byte and every login secret to the login request
void AuthContext::AppendLoginSecrets(WriteBuffer& output)
{
    AutoCrit lock(this->mDataCrit);
    SecretList::iterator next=this->mSecretList.begin();
    unsigned long countPosition=output.mLength;
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
// FUNCTION: WON 0x006e1650
// Adds a new uncommitted 8-byte login secret at the front and saves the list
void AuthContext::AddSecret(const void* secret)
{
    AutoCrit lock(this->mDataCrit);
    this->mSecretList.insert(this->mSecretList.begin(),SecretData(0,new ByteBuffer(secret,8,false)));
    this->SaveSecretList();
}
// FUNCTION: WON 0x006e1790
// Removes the matching uncommitted login secret and saves the list
void AuthContext::RemoveSecret(const void* secret)
{
    AutoCrit lock(this->mDataCrit);
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
// FUNCTION: WON 0x006e1890
// Gives uncommitted secrets an expiry, drops expired ones, caps the list below 256 entries and saves it
void AuthContext::CommitSecrets(time_t authTime,time_t duration)
{
    AutoCrit lock(this->mDataCrit);
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
// FUNCTION: WON 0x006e19e0
// Returns the current peer data (certificate and keys) under the lock
ConstSmartPtr<AuthPeerData> AuthContext::GetPeerData()
{
    AutoCrit lock(this->mDataCrit);
    return this->mPeerData;
}
// FUNCTION: WON 0x006e1a30
// Returns whether the auth2 certificate expires within the given number of seconds
bool AuthContext::IsExpired(time_t extra)
{
    AutoCrit lock(this->mDataCrit);
    return this->mPeerData->Certificate2Expired(extra);
}
// FUNCTION: WON 0x006e1aa0
// Replaces the peer data and clears the peer-session cache
void AuthContext::SetPeerData(const AuthPeerData* data)
{
    AutoCrit lock(this->mDataCrit);
    this->mPeerData=data;
    this->mSessionMap.clear();
    this->mSessionCleanItr=this->mSessionMap.begin();
}


// FUNCTION: WON 0x006e0c50
// Appends each hashed community's file hash and its hash keyed with the server challenge
void AuthContext::AppendHashes(WriteBuffer& output,const RawBuffer& challenge)
{
    AutoCrit lock(this->mDataCrit);
    int count=0;
    unsigned long countPosition=output.mLength;
    output.SkipBytes(1);
    std::map<std::wstring,AuthLoginCommunityData>::iterator next=this->mCommunityMap.begin();
    while(next!=this->mCommunityMap.end()) {
        AuthLoginCommunityData& data=next->second;
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
// FUNCTION: WON 0x006e0e50
// Appends the login's community names and typed records (CD keys, joins, user data, secrets, nicknames)
void AuthContext::AppendCommunityData(WriteBuffer& output)
{
    AutoCrit lock(this->mDataCrit);
    output.AppendByte(0);
    output.AppendByte(static_cast<unsigned char>(this->mCommunityMap.size()));
    std::map<std::wstring,AuthLoginCommunityData>::iterator community=this->mCommunityMap.begin();
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
    std::map<std::wstring,CDKey>::iterator joined=this->mCDKeyCommunityJoinMap.begin();
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
        ++joined; //the original advances the iterator before the held key temporary is destroyed
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

// FUNCTION: WON 0x006de560
// Forwards the certificate request completion to its context
void AuthContext::StaticGetCertCallback(SmartPtr<AsyncOp> op,SmartPtr<RefCount> param)
{
    static_cast<AuthContext*>(param.get())->GetCertCallback(static_cast<GetCertOp*>(op.get()));
}
// FUNCTION: WON 0x006de5f0
// Hands the new peer data and status to every waiting refresh op, then restarts auto-refresh
void AuthContext::GetCertCallback(GetCertOp* op)
{
    AutoCrit lock(this->mDataCrit);
    if(op!=this->mGetCertOp.get()) return;
    ConstSmartPtr<AuthPeerData> peerData=op->GetPeerData();
    std::set<SmartPtr<LightGetCertOp>,SmartPtr<LightGetCertOp>::Comp>::iterator next=this->mRefreshSet.begin();
    while(next!=this->mRefreshSet.end()) {
        LightGetCertOp* waiting=next->get();
        waiting->SetPeerData(peerData);
        waiting->Finish(op->GetStatus());
        ++next;
    }
    this->mRefreshSet.clear();
    this->mGetCertOp=NULL;
    this->mRefreshStatus=op->GetStatus();
    this->StartAutoRefresh(true);
}
// FUNCTION: WON 0x006deba0
// Queues a refresh op and starts a certificate request when none is running
void AuthContext::RefreshAsync(LightGetCertOp* op,DWORD timeout)
{
    AutoCrit lock(this->mDataCrit);
    this->StopAutoRefresh();
    if(op!=NULL) {
        this->mRefreshSet.insert(op);
        op->RunAsync((long)timeout);
    }
    if(this->mGetCertOp.get()==NULL) {
        this->mGetCertOp=new GetCertOp(this);
        this->mGetCertOp->mCompletion=new ParamCompletion<SmartPtr<AsyncOp>,SmartPtr<RefCount> >(AuthContext::StaticGetCertCallback,this);
        this->mRefreshStatus=this->mGetCertOp->Run(1,60000);
    }
}
// FUNCTION: WON 0x006dedf0
// Fetches a new certificate synchronously; restarts auto-refresh on success and returns the status
WONStatus AuthContext::RefreshBlock(DWORD timeout)
{
    SmartPtr<GetCertOp> op;
    op=new GetCertOp(this);
    this->mRefreshStatus=op->Run(2,timeout);
    if(this->mRefreshStatus==0) this->StartAutoRefresh(false);
    WONStatus status=this->mRefreshStatus;
    return status;
}


// File hashing uses the WONCommon MD5 class (0x58 bytes), not WONCrypt MD5Digest
// FUNCTION: WON 0x006e0380
// Sets the CD key sent for a community
void AuthContext::SetCDKey(const std::wstring& community,const CDKey& key)
{
    AutoCrit lock(this->mDataCrit);
    this->mCommunityMap[community].mCDKey=key;
}
// FUNCTION: WON 0x006e0510
// Clears the CD-key community joins
void AuthContext::ClearCDKeyCommunityJoins()
{
    AutoCrit lock(this->mDataCrit);
    this->mCDKeyCommunityJoinMap.clear();
}
// FUNCTION: WON 0x006e05e0
// Clears the per-community user data to set
void AuthContext::ClearUserData()
{
    AutoCrit lock(this->mDataCrit);
    this->mSetCommunityUserDataMap.clear();
}
// FUNCTION: WON 0x006e06b0
// Clears the one-time user data and CD-key joins
void AuthContext::ClearOneTimeData()
{
    this->ClearUserData();
    this->ClearCDKeyCommunityJoins();
}
// FUNCTION: WON 0x006e06d0
// Hashes the running executable for the community; false when its path is unavailable
// (GetModuleFileNameA: the data root is the executable's directory, unless EE_DATA_ROOT moves it on POSIX)
bool AuthContext::SetHashFile(const std::wstring& community)
{
    char directory[PLAT_MAX_PATH];
    char name[PLAT_MAX_PATH];
    if(PlatPathGetDataRoot(directory,PLAT_MAX_PATH)>0 && PlatPathGetExecutableName(name,PLAT_MAX_PATH)>0)
        return this->SetHashFile(community,std::string(directory)+name);
    return false;
}
// FUNCTION: WON 0x006e07f0
// Hashes one file for the community
bool AuthContext::SetHashFile(const std::wstring& community,const std::string& file)
{
    return this->SetHashFileList(community,std::list<std::string>(1,file));
}
// FUNCTION: WON 0x006e08e0
// Computes the community's MD5 of the files plus one MD5 per 16 KB section; false when a file cannot open
bool AuthContext::SetHashFileList(const std::wstring& community,const std::list<std::string>& files)
{
    AutoCrit lock(this->mDataCrit);
    AuthLoginCommunityData& data=this->mCommunityMap[community];
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


// FUNCTION: WON 0x006df620
// Sets the login user name
void AuthContext::SetUserName(const std::wstring& name)
{
    AutoCrit lock(this->mDataCrit);
    this->mName=name;
}
// FUNCTION: WON 0x006df760
// Sets the login password
void AuthContext::SetPassword(const std::wstring& password)
{
    AutoCrit lock(this->mDataCrit);
    this->mPassword=password;
}
// FUNCTION: WON 0x006df8a0
// Adds a community to log in to (no change when already present)
void AuthContext::AddCommunity(const std::wstring& community)
{
    AutoCrit lock(this->mDataCrit);
    this->mCommunityMap.insert(std::map<std::wstring,AuthLoginCommunityData>::value_type(community,AuthLoginCommunityData()));
}
// FUNCTION: WON 0x006dfcc0
// Makes the given community the only one, keeping its data when it was already present
void AuthContext::SetCommunity(const std::wstring& community)
{
    AutoCrit lock(this->mDataCrit);
    std::map<std::wstring,AuthLoginCommunityData>::iterator found=this->mCommunityMap.find(community);
    if(found!=this->mCommunityMap.end()) {
        AuthLoginCommunityData data=found->second;
        this->mCommunityMap.clear();
        this->mCommunityMap[community]=data;
    }
    else {
        this->mCommunityMap.clear();
        this->AddCommunity(community);
    }
}
// FUNCTION: WON 0x006e02d0
// Removes a community from the login
void AuthContext::RemoveCommunity(const std::wstring& community)
{
    AutoCrit lock(this->mDataCrit);
    this->mCommunityMap.erase(community);
}

// FUNCTION: WON 0x006df440
// Loads the signing public key from a file; false when the file is missing or the key invalid
bool AuthContext::LoadVerifierKey(const std::string& filename)
{
    AuthContext::mCheckedVerifierFile=true;
    FILE* file=PlatFOpen(filename.c_str(),"rb");
    if(file==NULL) return false;
    unsigned char buffer[1024];
    RawBuffer key;
    while(!feof(file)) {
        int count=fread(buffer,1,1024,file);
        if(count>0) key.append(buffer,count);
    }
    fclose(file);
    AutoCrit lock(GetVerifierCrit());
    return AuthContext::mVerifierKey.SetPublicKey(key.data(),key.length());
}

// FUNCTION: WON 0x006df330
// Sets the auth server list
void AuthContext::SetServerContext(ServerContext* context)
{
    AutoCrit lock(this->mDataCrit);
    this->mServerContext=context;
}
// FUNCTION: WON 0x006df3e0
// Returns the auth server list
SmartPtr<ServerContext> AuthContext::GetServerContext()
{
    AutoCrit lock(this->mDataCrit);
    return this->mServerContext;
}

}

namespace WONAPI {

// No original address identified: WON SDK support body
AuthContext::AuthContext(const std::wstring &theUserName, const std::wstring& theCommunity,
						 const std::wstring &thePassword) : 
	mName(theUserName),  
	mPassword(thePassword)
{
	this->Init();
	this->AddCommunity(theCommunity);
}

// No original address identified: WON SDK support body
bool AuthContext::AddAddress(const IPAddr &theAddr)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->AddAddress(theAddr);
}

// No original address identified: WON SDK support body
bool AuthContext::AddAddressesFromDir(const DirEntityList &theDir, const wchar_t *theNameFilter)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->AddAddressesFromDir(theDir, theNameFilter);
}

// No original address identified: WON SDK support body
void AuthContext::RemoveAllCommunity()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mCommunityMap.clear();
}

// No original address identified: WON SDK support body
void AuthContext::AddCDKeyCommunityJoin(const std::wstring &theCommunity, const CDKey &theKey)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mCDKeyCommunityJoinMap[theCommunity] = theKey;
}

// No original address identified: WON SDK support body
void AuthContext::AddUserData(const std::wstring &theCommunity, const ByteBuffer *theData)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mSetCommunityUserDataMap[theCommunity] = theData;
}

// No original address identified: WON SDK support body
void AuthContext::SetLoginSecretFile(const std::string &theFile)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mLoginSecretFile = theFile;
}

// No original address identified: WON SDK support body
void AuthContext::AddNickname(const std::wstring &theKey)
{
	AutoCrit aCrit(this->mDataCrit);
	this->mNicknameMap[theKey] = L""; 
}

// No original address identified: WON SDK support body
void AuthContext::SetNickname(const std::wstring &theKey, const std::wstring &theName)
{ 
	AutoCrit aCrit(this->mDataCrit);
	this->mNicknameMap[theKey] = theName; 
}

// No original address identified: WON SDK support body
void AuthContext::ClearNicknames()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mNicknameMap.clear();
}
}

namespace WONAPI {

// No original address identified: WON SDK locked server-context API
bool AuthContext::RemoveAddress(const IPAddr &theAddr)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->RemoveAddress(theAddr);
}

// No original address identified: WON SDK locked server-context API
void AuthContext::RemoveAllAddresses()
{
	AutoCrit aCrit(this->mDataCrit);
	this->mServerContext->RemoveAllAddresses();
}

// No original address identified: WON SDK locked server-context API
bool AuthContext::AddAddressesFromDir(const DirEntityMap &theDir, const std::wstring &thePath, const wchar_t *theNameFilter)
{
	AutoCrit aCrit(this->mDataCrit);
	return this->mServerContext->AddAddressesFromDir(theDir, thePath, theNameFilter);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddf90
// (protected: virtual void * __thiscall WONAPI::AuthContext::`scalar deleting destructor'(unsigned int)    [??_GAuthContext@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006dfae0
// (public: __thiscall WONAPI::AuthLoginCommunityData::~AuthLoginCommunityData(void)    [??1AuthLoginCommunityData@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006dfbd0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006dff00
// (public: __thiscall WONAPI::AuthLoginCommunityData::AuthLoginCommunityData(void)    [??0AuthLoginCommunityData@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddfb0
// (public: __thiscall std::set<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct WONAPI::SmartPtr<class WONAPI::LightGetCertOp>::Comp,class std)
// TEMPLATE: WON 0x006de070
// (public: __thiscall std::map<class WONAPI::IPAddr,struct WONAPI::AuthContext::SessionStruct,struct std::less<class WONAPI::IPAddr>,class std::allocator)
// TEMPLATE: WON 0x006de130
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006de1f0
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006de2b0
// (public: __thiscall std::map<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,clas)
// TEMPLATE: WON 0x006dff60
// (public: __thiscall WONAPI::AuthLoginCommunityData::AuthLoginCommunityData(class WONAPI::AuthLoginCommunityData const &)    [??0AuthLoginCommunityData@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddda0
// (class WONAPI::CriticalSection & __cdecl GetVerifierCrit(void)    [?GetVerifierCrit@@YAAAVCriticalSection@WONAPI@@XZ])
// TEMPLATE: WON 0x006e0140
// (public: class WONAPI::CDKey & __thiscall WONAPI::CDKey::operator=(class WONAPI::CDKey const &)    [??4CDKey@WONAPI@@QAEAAV01@ABV01@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ddde0
// (_$E72    [_$E72])
// TEMPLATE: WON 0x006df2d0
// (_$E77    [_$E77])
// TEMPLATE: WON 0x006df2e0
// (_$E73    [_$E73])
// TEMPLATE: WON 0x006df300
// (_$E76    [_$E76])
// TEMPLATE: WON 0x006df310
// (_$E74    [_$E74])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006cac70
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9530
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006e9630
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006ea2e0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006ea3b0
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006ea440
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006ea550
// (public: class std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> > & __thiscall std::basic_str)
// TEMPLATE: WON 0x006ea760
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Eos(unsigned i)
// TEMPLATE: WON 0x006ea780
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Freeze(void)  )
// TEMPLATE: WON 0x006ea820
// (private: bool __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Grow(unsigned )
// TEMPLATE: WON 0x006eaa00
// (private: bool __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Inside(unsigne)
// TEMPLATE: WON 0x006eb590
// (public: bool __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthSess)
// TEMPLATE: WON 0x006eb750
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_Map)
// TEMPLATE: WON 0x006eb890
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct)
// TEMPLATE: WON 0x006eb8e0
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006ebc90
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
// TEMPLATE: WON 0x006ebd40
// (protected: struct std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::_Node * __thiscall )
// TEMPLATE: WON 0x006ebe30
// (public: static unsigned char * __cdecl std::char_traits<unsigned char>::copy(unsigned char *,unsigned char const *,unsigned int)    [?copy@?$char_trai)
// TEMPLATE: WON 0x006ebe60
// (public: static unsigned char * __cdecl std::char_traits<unsigned char>::move(unsigned char *,unsigned char const *,unsigned int)    [?move@?$char_trai)
// TEMPLATE: WON 0x006ebeb0
// (public: unsigned int __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::max_size)
// TEMPLATE: WON 0x006ebec0
// (private: void __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::_Copy(unsigned )
// TEMPLATE: WON 0x006ed960
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct std::_Setk<c)
// TEMPLATE: WON 0x006ee020
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,str)
// TEMPLATE: WON 0x006ee8d0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ee910
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ee950
// (protected: struct std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,str)
// TEMPLATE: WON 0x006ee9a0
// (public: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short>)
// TEMPLATE: WON 0x006ef0a0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ef0e0
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ef120
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006ef650
// (protected: void __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned sho)
// TEMPLATE: WON 0x006ef890
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct)
// TEMPLATE: WON 0x006ef8f0
// (protected: struct std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,str)
// TEMPLATE: WON 0x006f0090
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006e9400
// (public: __thiscall std::basic_string<unsigned char,struct std::char_traits<unsigned char>,class std::allocator<unsigned char> >::basic_string<unsigned)
// TEMPLATE: WON 0x006e98b0
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_Map)
// TEMPLATE: WON 0x006e9e20
// (public: struct std::pair<class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned)
// TEMPLATE: WON 0x006ea1d0
// (public: __thiscall WONAPI::AuthContext::SessionStruct::~SessionStruct(void))
// TEMPLATE: WON 0x006ea230
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>::~pair<class WONAPI::IPAddr const ,struct WONAPI::)
// TEMPLATE: WON 0x006eb180
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct std::_Setk<c)
// TEMPLATE: WON 0x006eb230
// (protected: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct std::_Set)
// TEMPLATE: WON 0x006eb530
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthSess)
// TEMPLATE: WON 0x006eb5b0
// (public: struct std::pair<class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>)
// TEMPLATE: WON 0x006eb6a0
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_Map)
// TEMPLATE: WON 0x006eb7a0
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>::pair<class WONAPI::IPAddr const ,struct WONAPI::A)
// TEMPLATE: WON 0x006eb990
// (protected: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,stru)
// TEMPLATE: WON 0x006ede50
// (protected: void __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,class WONAPI::SmartPtr<class WONAPI::LightGetCertOp>,struct)
// TEMPLATE: WON 0x006ede90
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthS)
// TEMPLATE: WON 0x006edfe0
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,str)
// TEMPLATE: WON 0x006ee060
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>,struct std::_)
// TEMPLATE: WON 0x006ee360
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006eea00
// (protected: void __thiscall std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::_Incsize(u)
// TEMPLATE: WON 0x006eeb50
// (public: class std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,struct )
// TEMPLATE: WON 0x006eff70
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct> *,struct std::pair<class WONAPI::)
// TEMPLATE: WON 0x006f0050
// (public: void * __thiscall std::pair<class WONAPI::IPAddr const ,struct WONAPI::AuthContext::SessionStruct>::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006f00b0
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)
// TEMPLATE: WON 0x006f00d0
// (public: void * __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aec20
// (public: __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class std::list<class WONAPI::IPAddr,class std::alloca)
// TEMPLATE: WON 0x006e00d0
// (public: __thiscall WONAPI::CDKey::CDKey(class WONAPI::CDKey const &))
// TEMPLATE: WON 0x006e9f20
// (public: __thiscall std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> > con)
// TEMPLATE: WON 0x006ea030
// (public: __thiscall std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::~list<struct WONAP)
// TEMPLATE: WON 0x006ea090
// (public: class std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::iterator __thiscall std)
// TEMPLATE: WON 0x006eabd0
// (public: __thiscall std::_Tree<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsigned short> >,st)
// TEMPLATE: WON 0x006ebce0
// (public: class std::list<struct WONAPI::AuthContext::SecretData,class std::allocator<struct WONAPI::AuthContext::SecretData> >::iterator __thiscall std)
// TEMPLATE: WON 0x006efb80
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<unsigned short,struct std::char_traits<unsigned short>,class std::allocator<unsi)
// TEMPLATE: WON 0x006f0110
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::AuthSession>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::AuthSess)
// TEMPLATE: WON 0x006aaa50
// (public: __thiscall std::map<class WONAPI::IPAddr,class std::list<class WONAPI::IPAddr,class std::allocator<class WONAPI::IPAddr> >::iterator,struct st)
