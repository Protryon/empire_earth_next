// C:\EEX\Low-Level Engine\Utilities\Utilities.cpp (file name inferred: no __FILE__; by analogy with Error System.cpp:
// sorts between UString.cpp and UWideString.cpp)
// Whole in Rust (rust/crates/ee_util system.rs, LLE lane round 7): the shutdown list and the system questions; the
// exported names forward.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/ee_util.h"

// FUNCTION: LLE 0x100837e5
// Returns the user's default language id
unsigned short UGetOSLanguage() { return ee_util_os_language(); }

// FUNCTION: LLE 0x100837eb
// Returns the Windows family: 1 = 95, 2 = 98/ME, 3 = NT 5 or later, 4 = NT 4 or older (or a 9x major below 4)
unsigned long UGetOSType() { return ee_util_os_type(); }

// FUNCTION: LLE 0x10083826
// Registers a function to call at engine shutdown; always returns kErrorOK
long URegisterShutdown(void (*inFunction)(bool)) { return ee_util_register_shutdown(inFunction); }

// FUNCTION: LLE 0x10083838
// Calls every registered shutdown function, last registered first; with inTerminate, ends the platform's services
// and exits (inForced: exit(0), else abort)
void UShutdown(bool inForced, bool inTerminate) { ee_util_shutdown(inForced, inTerminate); }

// FUNCTION: LLE 0x10083891
// Returns whether UShutdown has started
bool UShutdownInProgress() { return ee_util_shutdown_in_progress(); }
