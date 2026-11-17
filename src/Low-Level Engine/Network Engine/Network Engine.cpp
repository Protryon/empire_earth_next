// C:\EEX\Low-Level Engine\Network Engine\Network Engine.cpp (file name inferred: no __FILE__; by analogy with
// Error System.cpp / Thread System.cpp: sorts after NETimestampedMessage.cpp)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEProtocol.h"
#include "rust/ee_net.h"

// Registers the network protocol, TCP/IP (the original loaded every 'ntwk' plug-in in the directory; the one there
// was is linked in, Rust: rust/crates/ee_net/src/net_tcp); fails if its plug-in is not usable (ee_net ffi_protocol.rs)
long NEInitialize(FSFileSpec& inDirectory)
{
    long theResult = URegisterShutdown(NEShutdown);
    if (theResult != kErrorOK)
        return theResult;
    return ee_net_pr_initialize();
}

// Destroys every registered network protocol at shutdown
void NEShutdown(bool inForced)
{
    ee_net_pr_shutdown();
}
