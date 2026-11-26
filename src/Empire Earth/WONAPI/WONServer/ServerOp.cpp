// RustServerOp: an op of the Rust op base (ee_won hostop.rs) with ServerOp's members (hostbase.rs); its calls on C++
// objects are answered over the core table (hostcore.rs: HostCall is gone, net round 29). The class is header-only
// since R6 WON lobby and network round 37 (ee/wonapi/WONRouting.h); this unit registers its maker.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONPing.h" // the MultiPingOp helpers over RustServerOp
#include "ee/wonapi/WONAuthPeerData.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONBlowfish.h"
#include "ee/wonapi/WONElGamal.h"
#include <vector>
#include "ee/wonapi/WONLobbyResource.h" // WONFile
#include "ee/wonapi/WONLobbyGUI.h" // WONAPICore::GetDefaultFileDirectory
#include "rust/won_net/certhost.h"
#include "rust/won_net/fwhost.h"
#include "rust/won_net/hostbase.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/httpget.h"
#include "rust/won_net/peerop.h"
#include "rust/won_net/reqhost.h"
#include "rust/won_net/reqmake.h"
#include "rust/won_net/reqop.h"
namespace WONAPI {
namespace {
// the Rust objects' maker of a Rust op's C++ op (ee_won cppcore.rs new_host_op: the peer auth and HTTP get ops
// a Rust op makes, GameSpySupport's service lookup)
void* NewHostOp(EeWonHostOp* inOp) { return static_cast<RefCount*>(static_cast<AsyncOp*>(new RustServerOp(inOp))); }
[[maybe_unused]] const bool sHostOpMaker = (ee_won_core_set_new_host_op(NewHostOp), true);
}
}
