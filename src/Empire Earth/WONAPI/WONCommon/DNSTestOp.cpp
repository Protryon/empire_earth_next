// DNSTestOp: resolves a host name on a helper thread (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
// The original default-constructs the host string and address before assigning the host in the body
// FUNCTION: WON 0x006d95a0
// Creates a DNS test op for the given host name
DNSTestOp::DNSTestOp(const std::string& inHost)
{
    this->mHostString = inHost;
}
// FUNCTION: WON 0x006d9740
// Destroys the op (members only)
DNSTestOp::~DNSTestOp() {}

}

namespace WONAPI {
// FUNCTION: WON 0x006d97f0
// Starts a self-deleting thread that resolves the host
void DNSTestOp::RunHook()
{
    DNSTestThread* thread=new DNSTestThread(this);
    thread->SetAutoDelete(true);
    thread->Start();
}
// FUNCTION: WON 0x006d9920
// Resolves the op's host and finishes it with success or the host-lookup failure status
void DNSTestThread::ThreadFunc()
{
    this->mOp->mHostAddr.SetWithDefaultPort(this->mOp->mHostString,0);
    if (this->mOp->mHostAddr.IsValid()) this->mOp->Finish(WS_Success);
    else this->mOp->Finish(WS_InvalidAddress);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d9720
// (protected: virtual void * __thiscall WONAPI::DNSTestOp::`scalar deleting destructor'(unsigned int)    [??_GDNSTestOp@WONAPI@@MAEPAXI@Z])
// TEMPLATE: WON 0x006d98b0
// (public: virtual __thiscall WONAPI::DNSTestThread::~DNSTestThread(void)    [??1DNSTestThread@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d9890
// (public: virtual void * __thiscall WONAPI::DNSTestThread::`scalar deleting destructor'(unsigned int)    [??_GDNSTestThread@WONAPI@@UAEPAXI@Z])
