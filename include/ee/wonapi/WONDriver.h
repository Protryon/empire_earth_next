// ee/wonapi/WONDriver.h: the WON classes' members that only the tests/won drivers use live in tests/won/drv (R6 WON
// round 38). A class body (or namespace) names its file with `#include EE_WON_DRV(name)`: the drivers, built with
// EE_WON_DRIVER (tools/xbuild.py build_won_test), include tests/won/drv/won_drv_<name>.inl there; the game includes
// this header again, which is empty the second time.
#ifndef EE_WONAPI_WONDRIVER_H
#define EE_WONAPI_WONDRIVER_H
#define EE_WON_DRV_TEXT(x) #x
#ifdef EE_WON_DRIVER
#define EE_WON_DRV(name) EE_WON_DRV_TEXT(won_drv_##name.inl)
#else
#define EE_WON_DRV(name) "ee/wonapi/WONDriver.h"
#endif
#endif
