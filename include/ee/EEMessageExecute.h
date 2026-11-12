// New (port): the game messages' Execute decisions (docs/RUST_PLAN.md R5b.4 leaf 4): functions of plain values that
// EEMessage.cpp's Execute bodies (and EEGetTradePrice) ask; the C++ keeps the objects, the locks and the world calls.
// They are Rust's (ee_netmsg execute.rs, declared in rust/ee_netmsg.h): the selection id check and the toggles' passes,
// the take test, the market price, the exchange step, the tribute's orders and viewer, the disband refund. The
// capture's NMDC record (ee/EEMessageCapture.h) records every call's inputs and outputs, in game and over a sweep.
#ifndef EE_EEMESSAGEEXECUTE_H
#define EE_EEMESSAGEEXECUTE_H

#include <stdint.h>
#include "rust/ee_netmsg.h"

#endif
