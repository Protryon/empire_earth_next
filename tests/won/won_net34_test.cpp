// R6 WON round 34 (net, second batch): DNSTestOp and DNSTestThread whole in Rust (ee_won_net dnstest.rs, a Rust op
// on the op base made by WONRouting.h's NewDnsTestOp) against the classes as they were at ab1b33bcb (copied below,
// renamed Orig*). Each host text (dotted addresses with and without ports, names that resolve locally, empty and bad
// texts) is run on both sides, asynchronously and blocking, the core pumped until the op finished: the status, the
// address's six bytes (GetHostAddr against AddrOf(EE_WON_DNSTEST_ADDR); the invalid ones' bytes are not read: the
// callers read the address only after a success), whether it succeeded and the op's references afterwards.
// EE_W34_MUTATE=n changes the original side in one way (1..3): the run must then report mismatches.
// The exit code is the number of mismatching cases.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "rust/won_net/dnstest.h"
#include "rust/won_net/status_codes.h"

using namespace WONAPI;

static int sMutate;
static bool Mutate(int n) { return sMutate == n; }
static std::string N(long long v) { return std::to_string(v); }

// ---- DNSTestOp and DNSTestThread (WONRouting.h at ab1b33bcb)
class OrigDNSTestOp : public AsyncOp {
    friend class OrigDNSTestThread;
public:
    OrigDNSTestOp(const std::string& inHost) { this->mHostString = inHost; }
    const IPAddr& GetHostAddr() { return this->mHostAddr; }
protected:
    virtual ~OrigDNSTestOp() {}
    virtual void RunHook();
    std::string mHostString;
    IPAddr mHostAddr;
};
typedef SmartPtr<OrigDNSTestOp> OrigDNSTestOpPtr;
class OrigDNSTestThread : public Thread {
public:
    OrigDNSTestThread(OrigDNSTestOp* inOp) : mOp(inOp) {}
    virtual void ThreadFunc()
    {
        this->mOp->mHostAddr.SetWithDefaultPort(this->mOp->mHostString, Mutate(2) ? 1 : 0);
        this->mOp->Finish(Mutate(1) || this->mOp->mHostAddr.IsValid() ? WS_Success : WS_InvalidAddress);
    }
    OrigDNSTestOpPtr mOp;
};
inline void OrigDNSTestOp::RunHook()
{
    if (Mutate(3)) { this->Finish(WS_Success); return; }
    OrigDNSTestThread* aThread = new OrigDNSTestThread(this);
    aThread->SetAutoDelete(true);
    aThread->Start();
}

static WONAPICoreEx* sCore;
static int sCases, sFailures;
static void Compare(const std::string& inOrig, const std::string& inNew, const std::string& inWhat)
{
    sCases++;
    if (inOrig == inNew) return;
    if (++sFailures <= 10) printf("MISMATCH %s\n  orig %s\n  new  %s\n", inWhat.c_str(), inOrig.c_str(), inNew.c_str());
}
static std::string Six(const IPAddr& a)
{
    std::string s;
    const unsigned char* b = reinterpret_cast<const unsigned char*>(a.GetSixByte());
    for (int i = 0; i < 6; i++) s += N(b[i]) + ".";
    return s;
}
// runs the op and waits (at most 20 s) for it to finish; its state as a line
template <class T, class F> static std::string RunOne(T* inOp, bool inBlock, F inAddr)
{
    SmartPtr<T> anOp = inOp;
    if (inBlock) anOp->RunBlock(20000);
    else anOp->RunAsync(20000);
    for (int i = 0; i < 4000 && anOp->GetStatus() == WS_Pending; i++) sCore->Pump(5);
    std::string s = "status " + N(anOp->GetStatus()) + " ok " + N(anOp->Succeeded());
    if (anOp->Succeeded()) {
        IPAddr a = inAddr(anOp.get());
        s += " valid " + N(a.IsValid()) + " six " + Six(a) + " port " + N(a.GetPort());
    }
    for (int i = 0; i < 50; i++) sCore->Pump(2); // the thread's reference given back
    s += " refs " + N(anOp->mRefCount);
    return s;
}
static void DnsPhase(int inRounds)
{
    static const char* const kHosts[] = {"127.0.0.1", "127.0.0.1:15100", "10.1.2.3:80", "192.168.0.255:65535", "localhost",
                                         "localhost:8080", "", ":", "1.2.3", "256.1.1.1", "127.0.0.1:", "a b c",
                                         "no.such.host.invalid", "0.0.0.0", "127.0.0.1:99999"};
    for (int q = 0; q < inRounds; q++)
        for (size_t h = 0; h < sizeof(kHosts) / sizeof(kHosts[0]); h++)
            for (int b = 0; b < 2; b++) {
                std::string o = RunOne(new OrigDNSTestOp(kHosts[h]), b != 0, [](OrigDNSTestOp* x) { return x->GetHostAddr(); });
                std::string n = RunOne(NewDnsTestOp(kHosts[h]), b != 0, [](RustServerOp* x) { return x->AddrOf(EE_WON_DNSTEST_ADDR); });
                Compare(o, n, std::string("host \"") + kHosts[h] + "\"" + (b ? " blocking" : " async") + " round " + N(q));
            }
}

int main(int argc, char** argv)
{
    const char* m = getenv("EE_W34_MUTATE");
    sMutate = m != NULL ? atoi(m) : 0;
    AsyncSocket::StartWinsock();
    WONAPICoreEx aCore;
    aCore.SetDefaultFileDirectory("./");
    aCore.Startup();
    sCore = &aCore;
    DnsPhase(sMutate ? 1 : 3);
    printf("won_net34_test%s: %d cases, %d mismatches\n", sMutate ? (" (mutation " + N(sMutate) + ")").c_str() : "", sCases, sFailures);
    return sFailures;
}
