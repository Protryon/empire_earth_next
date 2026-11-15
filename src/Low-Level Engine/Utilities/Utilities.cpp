// C:\EEX\Low-Level Engine\Utilities\Utilities.cpp (file name inferred: no __FILE__; by analogy with Error System.cpp:
// sorts between UString.cpp and UWideString.cpp)
// Whole in Rust (rust/crates/ee_util system.rs, LLE lane round 7): the shutdown list and the system questions; the
// exported names forward.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/ee_util.h"

// Returns the user's default language id
unsigned short UGetOSLanguage() { return ee_util_os_language(); }

// Registers a function to call at engine shutdown; always returns kErrorOK
long URegisterShutdown(void (*inFunction)(bool)) { return ee_util_register_shutdown(inFunction); }

// Calls every registered shutdown function, last registered first; with inTerminate, ends the platform's services
// and exits (inForced: exit(0), else abort)
void UShutdown(bool inForced, bool inTerminate) { ee_util_shutdown(inForced, inTerminate); }

// Returns whether UShutdown has started
bool UShutdownInProgress() { return ee_util_shutdown_in_progress(); }
