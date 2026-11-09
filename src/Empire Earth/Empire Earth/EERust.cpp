// The game's start of the Rust library (rust/crates/ee_rust; docs/RUST_PLAN.md).
#include <stdio.h>
#include <string>
#include "ee/EEGameLog.h"
#include "platform/PlatMessage.h"
#include "rust/ee_rust.h"

// A Rust panic aborts the process and a GUI exe has no stderr: the line goes to the debugger's output (the harness
// logs it), stderr, and the game log when a game is running.
static void EERustPanicLine(const char* inLine)
{
    std::string theLine(inLine);
    theLine += "\n";
    PlatDebugOutput(theLine.c_str());
    fputs(theLine.c_str(), stderr);
    fflush(stderr);
    if (EEGameLogOn())
        EEGameLogLine(theLine.c_str());
}

void EERustInit()
{
    ee_rust_init(EERustPanicLine);
}
