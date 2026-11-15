// WONAuth: what AuthContext and CDKey (ee/wonapi/WONAuthContext.h, header-only since net round 26) need the platform
// layer and the lobby's directory for: the auth context's files in the WON directory, the exe's path for the hash
// file, the CD key's registry (config.yaml: RegKey) and file storage
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <set>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "platform/PlatFile.h"
#include "platform/PlatPath.h"
namespace WONAPI {
// the certificate op the auth flow makes (ee_won authcpp.rs: one maker for the program)
static void* NewCertOp(void* inContext)
{
    return static_cast<RefCount*>(static_cast<AsyncOp*>(new GetCertOp(static_cast<AuthContext*>(static_cast<RefCount*>(inContext)))));
}
// FUNCTION: WON 0x006ddb70
// The context's Rust object (its calls over the core table: ee_won authcpp.rs) with the login secrets in the WON
// directory, a new server list and the directory's verifier key
void AuthContext::Init()
{
    std::string aFile = WONAPICore::GetDefaultFileDirectory() + "_wonlogin.ks";
    this->mCore = ee_won_authctx_new_core(static_cast<RefCount*>(this), WonCore(), NewCertOp, WonBytes(aFile),
                                          aFile.size());
    ee_won_authctx_set_server_context(this->mCore, static_cast<RefCount*>(new ServerContext));
    const std::string& aDir = WONAPICore::GetDefaultFileDirectory();
    ee_won_auth_verifier_init(WonBytes(aDir), aDir.size());
}
// FUNCTION: WON 0x006e06d0
// The community's hash file: the running exe in the data root
bool AuthContext::SetHashFile(const std::wstring& inCommunity)
{
    char directory[PLAT_MAX_PATH];
    char name[PLAT_MAX_PATH];
    if (PlatPathGetDataRoot(directory, PLAT_MAX_PATH) > 0 && PlatPathGetExecutableName(name, PLAT_MAX_PATH) > 0)
        return this->SetHashFile(inCommunity, std::string(directory) + name);
    return false;
}

// WON SDK default registry path (no original address identified)
const char* CDKey::mCDKeyPath = "SOFTWARE\\Sierra\\CDKeys";
// FUNCTION: WON 0x006e3ac0
// Loads and decrypts the product's key from a registry value; false when absent or invalid
bool CDKey::LoadFromRegistry(long inRoot, const char* inPath)
{
    if (this->mProduct.empty()) return false;
    RegKey aKey(inRoot, std::string(inPath), false, true);
    if (!aKey.mIsOpen) return false;
    ByteBufferPtr anEncrypted;
    if (aKey.QueryBinary(this->mProduct, anEncrypted) != 0) return false;
    return this->InitEncrypted(anEncrypted->data(), anEncrypted->length());
}
// No original address identified: WON SDK registry storage
bool CDKey::SaveToRegistry(long inRoot, const char* inPath)
{
    if (!this->IsValid()) return false;
    RegKey aKey(inRoot, inPath, true, true);
    if (!aKey.IsOpen()) return false;
    ByteBufferPtr anEncrypted = this->GetEncrypted();
    if (anEncrypted.get() == NULL) return false;
    return aKey.SetValue(this->mProduct, anEncrypted->data(), anEncrypted->length());
}
void CDKey::ClearFromRegistry(long inRoot, const char* inPath)
{
    if (this->mProduct.empty()) return;
    RegKey aKey(inRoot, inPath, false, true);
    aKey.DeleteValue(this->mProduct);
}
// No original address identified: WON SDK file storage (the encrypted key)
bool CDKey::SaveToFile(const std::string& inFileName)
{
    if (!this->IsValid()) return false;
    FILE* aFile = PlatFOpen(inFileName.c_str(), "wb");
    if (aFile == NULL) return false;
    ByteBufferPtr anEncrypted = this->GetEncrypted();
    if (anEncrypted.get() == NULL) return false;
    if (fwrite(anEncrypted->data(), 1, anEncrypted->length(), aFile) != anEncrypted->length()) return false;
    fclose(aFile);
    return true;
}
bool CDKey::LoadFromFile(const std::string& inFileName)
{
    FILE* aFile = PlatFOpen(inFileName.c_str(), "rb");
    if (aFile == NULL) return false;
    char aBuf[2048];
    int aReadLen = (int)fread(aBuf, 1, 2048, aFile);
    fclose(aFile);
    return this->InitEncrypted(aBuf, aReadLen);
}
}
