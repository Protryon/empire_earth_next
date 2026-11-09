// CDKey: CD key validation, machine-bound encryption and registry/file storage. The key logic (the text form, the
// check byte, the storage key and cipher) is ee_won's (rust/crates/ee_won/src/cdkey.rs); this class keeps the
// product, the raw key, the machine id and the registry and file storage.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONBlowfish.h"
#include "platform/PlatFile.h"
#include "platform/PlatMessage.h"
namespace WONAPI {
// FUNCTION: WON 0x006e3090
// Creates an empty, invalid CD key
CDKey::CDKey() { this->mValid = false; }
// FUNCTION: WON 0x006e30c0
// Marks the key invalid
void CDKey::Invalidate() { this->mValid = false; }
// FUNCTION: WON 0x006e30d0
// Sets the product name the key belongs to and invalidates the key
void CDKey::SetProductString(const std::string& product)
{
    this->Invalidate();
    this->mProduct = product;
}
// FUNCTION: WON 0x006e3220
// Takes 8 raw key bytes and validates their check byte against a CRC of the product and key
bool CDKey::CalcKeyFromBuffer(const char* raw)
{
    this->mKey = new ByteBuffer(raw, 8, false);
    this->mValid = ee_won_cdkey_is_valid(reinterpret_cast<const uint8_t*>(this->mProduct.data()),
                                         this->mProduct.length(), reinterpret_cast<const uint8_t*>(raw));
    return this->mValid;
}
// FUNCTION: WON 0x006e3320
// Returns the machine's id (Windows: the volume serial number of the Windows drive); reports failure when there is none
DWORD CDKey::GetMachineId(bool* success)
{
    int found = 0;
    DWORD serial = PlatSystemMachineId(&found);
    if (success != NULL)
        *success = found != 0;
    return serial;
}
// FUNCTION: WON 0x006e35a0
// Builds the Blowfish key from the product name CRCs, optionally prefixed with the machine id
void CDKey::GetSymmetricKey(Blowfish& key, bool useMachine) const
{
    bool gotId = false;
    DWORD machineId = useMachine ? CDKey::GetMachineId(&gotId) : 0;
    uint8_t bytes[12];
    size_t length = ee_won_cdkey_symmetric_key(reinterpret_cast<const uint8_t*>(this->mProduct.data()),
                                               this->mProduct.length(), machineId, useMachine && gotId, bytes);
    key.SetKey(bytes, static_cast<int>(length));
}
// FUNCTION: WON 0x006e3880
// Decrypts a stored key with the machine-bound or the old key (trying the other on failure); true when valid
bool CDKey::InitEncrypted(const void* encrypted, unsigned long length)
{
    this->mValid = false;
    this->mKey = NULL;
    if (encrypted == NULL)
        return false;
    bool gotId = false;
    DWORD machineId = CDKey::GetMachineId(&gotId);
    EeWonCdKeyDecoded decoded;
    ee_won_cdkey_decrypt(reinterpret_cast<const uint8_t*>(this->mProduct.data()), this->mProduct.length(), machineId,
                         gotId, this->GetUseNewEncryptMethod(), static_cast<const uint8_t*>(encrypted), length, &decoded);
    if (decoded.has_raw) {
        this->mKey = new ByteBuffer(decoded.raw, 8, false);
        this->mValid = decoded.valid;
    }
    if (!decoded.used_machine)
        this->NotifyUsedOldMethodToDecrypt();
    return this->mValid;
}
// FUNCTION: WON 0x006e3ac0
// Loads and decrypts the product's key from a registry value; false when absent or invalid
bool CDKey::LoadFromRegistry(long root, const char* path)
{
    if (this->mProduct.empty())
        return false;
    RegKey key(root, std::string(path), false, true);
    if (!key.mIsOpen)
        return false;
    ByteBufferPtr encrypted;
    if (key.QueryBinary(this->mProduct, encrypted) != 0)
        return false;
    return this->InitEncrypted(encrypted->data(), encrypted->length());
}
}

namespace WONAPI {
using namespace std;
// No original address identified: WON SDK method
CDKey::CDKey(const string& theProduct) : mProduct(theProduct)
{
	this->mValid = false;
}
// No original address identified: WON SDK method
// Reads a key's text ("XXXX-XXXX-XXXX-XXXX-XXXX", dashes and white space optional); true when its check byte is right
bool CDKey::Init(const string& theKey)
{
	this->mValid = false;
	this->mKey = NULL;
	uint8_t raw[8];
	if (!ee_won_cdkey_parse(reinterpret_cast<const uint8_t*>(theKey.data()), theKey.length(), raw))
		return false;
	return this->CalcKeyFromBuffer(reinterpret_cast<const char*>(raw));
}
// No original address identified: WON SDK method
// The key's text; empty without a key
string CDKey::GetString() const
{
	if(this->mKey.get()==NULL || this->mKey->length()<8)
		return "";
	char text[25];
	ee_won_cdkey_format(reinterpret_cast<const uint8_t*>(this->mKey->data()), text);
	return text;
}
// No original address identified: WON SDK method
// The key encrypted for storage; NULL when invalid
ByteBufferPtr CDKey::GetEncrypted() const
{
	if(!this->IsValid())
		return NULL;
	bool useMachine = this->GetUseNewEncryptMethod();
	bool gotId = false;
	DWORD machineId = useMachine ? CDKey::GetMachineId(&gotId) : 0;
	ByteBufferPtr result;
	ee_won_cdkey_encrypt(reinterpret_cast<const uint8_t*>(this->mProduct.data()), this->mProduct.length(), machineId,
	                     useMachine && gotId, reinterpret_cast<const uint8_t*>(this->mKey->data()), WonByteBufferSink,
	                     &result);
	return result;
}
// No original address identified: WON SDK method
bool CDKey::InitRaw(const void* theRawKey)
{
	this->mValid = false;
	this->mKey = NULL;
	return this->CalcKeyFromBuffer(static_cast<const char*>(theRawKey));
}
// No original address identified: WON SDK method
bool CDKey::SaveToFile(const std::string &theFileName)
{
	if(!this->IsValid())
		return false;
	FILE *aFile = PlatFOpen(theFileName.c_str(),"wb");
	if(aFile==NULL)
		return false;
	ByteBufferPtr anEncrypt = this->GetEncrypted();
	if(anEncrypt.get()==NULL)
		return false;
	if(fwrite(anEncrypt->data(),1,anEncrypt->length(),aFile)!=anEncrypt->length())
		return false;
	fclose(aFile);
	return true;
}
// No original address identified: WON SDK method
bool CDKey::LoadFromFile(const std::string &theFileName)
{
	FILE *aFile = PlatFOpen(theFileName.c_str(),"rb");
	if(aFile==NULL)
		return false;
	char aBuf[2048];
	int aReadLen = fread(aBuf,1,2048,aFile);
	fclose(aFile);
	return this->InitEncrypted(aBuf,aReadLen);
}
// No original address identified: WON SDK method
bool CDKey::IsBeta() const
{
	return this->mKey.get()!=NULL
	    && ee_won_cdkey_flag(reinterpret_cast<const uint8_t*>(this->mKey->data()), this->mKey->length(), 0);
}
// No original address identified: WON SDK method
bool CDKey::IsForceCheck() const
{
	return this->mKey.get()!=NULL
	    && ee_won_cdkey_flag(reinterpret_cast<const uint8_t*>(this->mKey->data()), this->mKey->length(), 1);
}
}
namespace WONAPI {
// WON SDK default registry path (no original address identified)
const char* CDKey::mCDKeyPath = "SOFTWARE\\Sierra\\CDKeys";
// No original address identified: WON SDK registry storage
bool CDKey::SaveToRegistry(long theRoot, const char* thePath)
{
	if(!this->IsValid())
		return false;
	RegKey aKey(theRoot, thePath, true, true);
	if(!aKey.IsOpen())
		return false;
	ByteBufferPtr anEncrypt = this->GetEncrypted();
	if(anEncrypt.get()==NULL)
		return false;
	return aKey.SetValue(this->mProduct,anEncrypt->data(),anEncrypt->length());
}
// No original address identified: WON SDK registry storage
void CDKey::ClearFromRegistry(long theRoot, const char* thePath)
{
	if(this->mProduct.empty())
		return;
	RegKey aKey(theRoot, thePath, false, true);
	aKey.DeleteValue(this->mProduct);
}
}
