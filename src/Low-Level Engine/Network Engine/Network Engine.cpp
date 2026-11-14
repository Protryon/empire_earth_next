// C:\EEX\Low-Level Engine\Network Engine\Network Engine.cpp (file name inferred: no __FILE__; by analogy with
// Error System.cpp / Thread System.cpp: sorts after NETimestampedMessage.cpp)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEProtocol.h"
#include "rust/ee_net_tcp.h"

// FUNCTION: LLE 0x1004b190
// Registers the network protocol, TCP/IP (the original loaded every 'ntwk' plug-in in the directory; the one there
// was is linked in, Rust: rust/crates/ee_net_tcp); fails if its plug-in is not usable
long NEInitialize(FSFileSpec& inDirectory)
{
    long theResult = URegisterShutdown(NEShutdown);
    if (theResult != kErrorOK)
        return theResult;
    NEProtocol* theProtocol = new NEProtocol();
    if (!ee_tcp_is_valid())
        delete theProtocol;
    return NEProtocol::GetCount() > 0 ? kErrorOK : kErrorNotFound;
}

// FUNCTION: LLE 0x1004b235
// Destroys every registered network protocol at shutdown
void NEShutdown(bool inForced)
{
    NEProtocol* theProtocol = NULL;
    while (NEProtocol::GetCount() > 0) {
        NEProtocol::GetProtocol(0, theProtocol);
        delete theProtocol;
    }
}
