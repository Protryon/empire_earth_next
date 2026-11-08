// CDKey: CD key validation, machine-bound encryption and registry/file storage
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
#include "ee/wonapi/WONCRC16.h"
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
    CRC16 crc;
    crc.Put(this->mProduct.data(), this->mProduct.length());
    crc.Put(raw, 3);
    crc.Put(raw+4, 4);
    char check = static_cast<char>((crc.Get() & 0x0fff) >> 4);
    this->mValid = check == raw[3];
    return this->mValid;
}
// FUNCTION: WON 0x006e3320
// Returns the volume serial number of the Windows drive; reports failure when there is no drive letter
DWORD CDKey::GetMachineId(bool* success)
{
    if (success != NULL)
        *success = true;
    char path[MAX_PATH];
    char drive[_MAX_DRIVE];
    char directory[_MAX_DIR];
    char filename[_MAX_FNAME];
    char extension[_MAX_EXT];
    GetWindowsDirectoryA(path, MAX_PATH);
    _splitpath(path, drive, directory, filename, extension);
    std::string root = drive;
    if (!root.empty()) {
        if (root[root.length()-1] != '\\')
            root += '\\';
        DWORD serial = 0;
        GetVolumeInformationA(root.c_str(), NULL, 0, &serial, NULL, NULL, NULL, 0);
        return serial;
    }
    if (success != NULL)
        *success = false;
    return 0;
}
// FUNCTION: WON 0x006e35a0
// Builds the Blowfish key from the product name CRCs, optionally prefixed with the machine id
void CDKey::GetSymmetricKey(Blowfish& key, bool useMachine) const
{
    CRC16 crc;
    RawBuffer bytes;
    if (useMachine) {
        bool gotId;
        DWORD machineId = CDKey::GetMachineId(&gotId);
        if (gotId)
            bytes.append(reinterpret_cast<const unsigned char*>(&machineId), 4);
    }
    crc.Put(this->mProduct.data(), this->mProduct.length());
    unsigned short check = crc.Get();
    bytes.append(reinterpret_cast<const unsigned char*>(&check), 2);
    for (int i=0; i<3 && i<this->mProduct.length(); ++i) {
        char letter = this->mProduct[i];
        crc.Put(&letter, 1);
        check = crc.Get();
        bytes.append(reinterpret_cast<const unsigned char*>(&check), 2);
    }
    key.SetKey(bytes.data(), bytes.length());
}
// FUNCTION: WON 0x006e3880
// Decrypts a stored key with the machine-bound or the old key (trying the other on failure); true when valid
bool CDKey::InitEncrypted(const void* encrypted, unsigned long length)
{
    this->mValid = false;
    this->mKey = NULL;
    if (encrypted == NULL)
        return false;
    bool success = false;
    bool useMachine = this->GetUseNewEncryptMethod();
    Blowfish key;
    this->GetSymmetricKey(key, useMachine);
    ByteBufferPtr decoded = key.Decrypt(encrypted, length);
    if (decoded.get() != NULL)
        success = this->CalcKeyFromBuffer(decoded->data());
    if (!success) {
        useMachine = !useMachine;
        this->GetSymmetricKey(key, useMachine);
        decoded = key.Decrypt(encrypted, length);
        if (decoded.get() != NULL)
            success = this->CalcKeyFromBuffer(decoded->data());
    }
    if (!useMachine)
        this->NotifyUsedOldMethodToDecrypt();
    return success;
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
namespace WONAPI {
using namespace std;
namespace {
const char* SKIPCHAR_MAP = "- \t";
const char* STRINGKEY_MAP = "CVCNCVCNCVCNCVCNNNNN";
const unsigned int STRINGKEY_LEN = 20;
const int DASH_OFFSET = 4;
const unsigned char BETA_MASK = 0x01;
const unsigned char FORCE_MASK = 0x80;

// No original address identified: WON SDK key codec helper
static void AddBits(char *theBuf, int &theOffset, int theBits, int theNumBits)
{
	int aBytePos = theOffset/8;
	int aBitPos = theOffset%8;

	int aMask = (theBits<<aBitPos)&0xff;
	theBuf[aBytePos] |= aMask;

	int extra = aBitPos + theNumBits - 8;
	if(extra>0)
	{
		aMask = (theBits>>(theNumBits - extra))&0xff;
		theBuf[aBytePos+1] |= aMask;
	}
		
	theOffset+=theNumBits;
}

// No original address identified: WON SDK key codec helper
static bool ProcessCChar(char *theBuf, int& theOffset, char theChar)
{
	int aVal = 0;

	// Determine mask based on char
	switch (toupper(theChar))
	{
		case 'B':
			aVal = 0;   break;
		case 'C':
			aVal = 1;   break;
		case 'D':
			aVal = 2;   break;
		case 'F':
			aVal = 3;   break;
		case 'G':
			aVal = 4;   break;
		case 'J':
			aVal = 5;   break;
		case 'L':
			aVal = 6;   break;
		case 'M':
			aVal = 7;   break;
		case 'N':
			aVal = 8;   break;
		case 'P':
			aVal = 9;   break;
		case 'R':
			aVal = 10;  break;
		case 'S':
			aVal = 11;  break;
		case 'T':
			aVal = 12;  break;
		case 'W':
			aVal = 13;  break;
		case 'X':
			aVal = 14;  break;
		case 'Z':
			aVal = 15;  break;
		default:
			return false;
	}

	AddBits(theBuf,theOffset,aVal,4);
	return true;
}

// No original address identified: WON SDK key codec helper
static bool ProcessVChar(char *theBuf, int& theOffset, char theChar)
{
	int aVal = 0;

	// Determine mask based on char
	switch (toupper(theChar))
	{
		case 'A':
			aVal = 0;  break;
		case 'E':
			aVal = 1;  break;
		case 'U':
			aVal = 2;  break;
		case 'Y':
			aVal = 3;  break;
		default:
			return false;
	}

	AddBits(theBuf,theOffset,aVal,2);
	return true;
}

// No original address identified: WON SDK key codec helper
static bool ProcessNChar(char *theBuf, int& theOffset, char theChar)
{
	if(!isdigit(theChar) || theChar<='1') // has to be '2'-'9'
		return false;

	int aVal = (theChar - '0') - 2;
	AddBits(theBuf,theOffset,aVal,3);
	return true;
}

// No original address identified: WON SDK key codec helper
static int ValFromBits(const char *theBuf, int &theOffset, int theNumBits)
{
	int aBytePos = theOffset/8;
	int aBitPos = theOffset%8;

	int aVal = ((unsigned char)theBuf[aBytePos]>>aBitPos)&((1<<theNumBits)-1)&0xff;
	
	int extra = theNumBits + aBitPos - 8;
	if(extra>0)
	{
		int extraBits = theBuf[aBytePos+1] & ((1<<extra)-1);
		extraBits <<= theNumBits-extra;
		aVal |= extraBits;
	}

	theOffset+=theNumBits;
	return aVal;
}

// No original address identified: WON SDK key codec helper
static char BuildCChar(const char *theBuf, int& theOffset) 
{
	char aChar = 0;

	// Determine mask based on char
	switch (ValFromBits(theBuf, theOffset, 4))
	{
	case 0:
		aChar = 'B';   break;
	case 1:
		aChar = 'C';   break;
	case 2:
		aChar = 'D';   break;
	case 3:
		aChar = 'F';   break;
	case 4:
		aChar = 'G';   break;
	case 5:
		aChar = 'J';   break;
	case 6:
		aChar = 'L';   break;
	case 7:
		aChar = 'M';   break;
	case 8:
		aChar = 'N';   break;
	case 9:
		aChar = 'P';   break;
	case 10:
		aChar = 'R';   break;
	case 11:
		aChar = 'S';   break;
	case 12:
		aChar = 'T';   break;
	case 13:
		aChar = 'W';   break;
	case 14:
		aChar = 'X';   break;
	case 15:
		aChar = 'Z';   break;
	}

	return aChar;
}

// No original address identified: WON SDK key codec helper
static char BuildVChar(const char *theBuf, int& theOffset) 
{
	char aChar = 0;

	// Determine mask based on char
	switch (ValFromBits(theBuf, theOffset, 2))
	{
		case 0:
			aChar = 'A';   break;
		case 1:
			aChar = 'E';   break;
		case 2:
			aChar = 'U';   break;
		case 3:
			aChar = 'Y';   break;
	}

	return aChar;
}

// No original address identified: WON SDK key codec helper
static char BuildNChar(const char *theBuf, int& theOffset)
{
	return ValFromBits(theBuf, theOffset, 3) + '0' + 2;
}
}

// No original address identified: WON SDK method
CDKey::CDKey(const string& theProduct) : mProduct(theProduct)
{
	this->mValid = false;
}

// No original address identified: WON SDK method
bool CDKey::Init(const string& theKey)
{
	this->mValid = false;
	this->mKey = NULL;

	// remove skip chars such as dashes
	string aKey;
	int aPos = 0;
	while(aPos < theKey.length()) 
	{
		unsigned char aChar = theKey[aPos];
		if(strchr(SKIPCHAR_MAP,aChar)==NULL) // not a skip char
			aKey+=aChar;

		aPos++;
	}

	// Validate length
	if (aKey.size() != STRINGKEY_LEN)
	{
		this->mValid = false;
		return false;
	}

	char aBuf[8];
	memset(aBuf,0,8);
	
	int anOffset = 0;
	for (int i=0; i < STRINGKEY_LEN; i++)
	{
		bool aTst = true;
		switch (STRINGKEY_MAP[i])
		{
			case 'C':
				aTst = ProcessCChar(aBuf, anOffset, aKey[i]);  break;
			case 'V':
				aTst = ProcessVChar(aBuf, anOffset, aKey[i]);  break;
			case 'N':
				aTst = ProcessNChar(aBuf, anOffset, aKey[i]);  break;
		}

		if (! aTst)
		{
			this->mValid = false;
			return false;
		}
	}

	return this->CalcKeyFromBuffer(aBuf);
}

// No original address identified: WON SDK method
string CDKey::GetString() const
{
	if(this->mKey.get()==NULL)
		return "";

	const char *aBuf = this->mKey->data();
	int anOffset = 0;
	string aKey = "";

	for (int i=0; i < STRINGKEY_LEN;)
	{
		switch (STRINGKEY_MAP[i])
		{
			case 'C':
				aKey += BuildCChar(aBuf, anOffset);  break;
			case 'V':
				aKey += BuildVChar(aBuf, anOffset);  break;
			case 'N':
				aKey += BuildNChar(aBuf, anOffset);  break;
		}

		// Add dash every four chars
		if ((((++i) % DASH_OFFSET) == 0) && (i < STRINGKEY_LEN))
			aKey += '-';
	}

	return aKey;
}

// No original address identified: WON SDK method
ByteBufferPtr CDKey::GetEncrypted() const
{

	if(!this->IsValid())
		return NULL;

	Blowfish aSymKey;
	this->GetSymmetricKey(aSymKey, this->GetUseNewEncryptMethod());
	return aSymKey.Encrypt(this->mKey->data(),this->mKey->length());
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
	
	FILE *aFile = fopen(theFileName.c_str(),"wb");
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
	FILE *aFile = fopen(theFileName.c_str(),"rb");
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
	if(this->mKey.get()==NULL)
		return false;
	else
		return (*(this->mKey->data()) & BETA_MASK);
}

// No original address identified: WON SDK method
bool CDKey::IsForceCheck() const
{
	if(this->mKey->length()<3)
		return false;
	else
		return ((this->mKey->data())[2] & FORCE_MASK ? true : false);
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
