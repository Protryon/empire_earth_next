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
// The encrypted key's stores are Rust's (ee_won cdkeystore.rs: config.yaml through the Rust RegKey, the file's bytes)
bool CDKey::LoadFromRegistry(long inRoot, const char* inPath)
{
    std::string aBytes;
    return !this->mProduct.empty()
        && ee_won_cdkey_registry_read(inRoot, inPath, WonBytes(this->mProduct), this->mProduct.size(), WonStringSink, &aBytes)
        && this->InitEncrypted(aBytes.data(), aBytes.size());
}
bool CDKey::SaveToRegistry(long inRoot, const char* inPath)
{
    ByteBufferPtr anEncrypted = this->GetEncrypted();
    return anEncrypted.get() != NULL
        && ee_won_cdkey_registry_write(inRoot, inPath, WonBytes(this->mProduct), this->mProduct.size(),
                                       reinterpret_cast<const uint8_t*>(anEncrypted->data()), anEncrypted->length());
}
void CDKey::ClearFromRegistry(long inRoot, const char* inPath)
{
    if (!this->mProduct.empty()) ee_won_cdkey_registry_clear(inRoot, inPath, WonBytes(this->mProduct), this->mProduct.size());
}
bool CDKey::SaveToFile(const std::string& inFileName)
{
    ByteBufferPtr anEncrypted = this->GetEncrypted();
    return anEncrypted.get() != NULL
        && ee_won_cdkey_file_write(inFileName.c_str(), reinterpret_cast<const uint8_t*>(anEncrypted->data()),
                                   anEncrypted->length());
}
bool CDKey::LoadFromFile(const std::string& inFileName)
{
    std::string aBytes;
    return ee_won_cdkey_file_read(inFileName.c_str(), WonStringSink, &aBytes) && this->InitEncrypted(aBytes.data(), aBytes.size());
}
}
