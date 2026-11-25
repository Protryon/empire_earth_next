// WONAuth: what AuthContext and CDKey (ee/wonapi/WONAuthContext.h, header-only since net round 26) need here: the
// context's certificate op maker and server list (the rest of Init and the exe's hash file are Rust's since R6 WON
// lobby and network round 37: authcpp.rs, authlogin.rs), the CD key's registry path
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
#include "rust/won_net/authcpp.h"
#include "rust/won_net/cdkeystore.h"
namespace WONAPI {
// the certificate op the auth flow makes (ee_won authcpp.rs: one maker for the program)
static void* NewCertOp(void* inContext)
{
    return static_cast<RefCount*>(static_cast<AsyncOp*>(RustServerOp::NewGetCert(static_cast<AuthContext*>(static_cast<RefCount*>(inContext)))));
}
// The context's Rust object (its calls over the core table: ee_won authcpp.rs ee_won_authctx_new_default: the login
// secrets in the WON directory, the new server list, the directory's verifier key)
void AuthContext::Init()
{
    this->mCore = ee_won_authctx_new_default(static_cast<RefCount*>(this), WonCore(), NewCertOp,
                                             static_cast<RefCount*>(new ServerContext));
}

// WON SDK default registry path (no original address identified)
const char* CDKey::mCDKeyPath = "SOFTWARE\\Sierra\\CDKeys";
// the key and its stores are Rust's (ee_won cdkeystore.rs EeWonCdKeyObj, R6 WON round 33: CDKey whole)
}
