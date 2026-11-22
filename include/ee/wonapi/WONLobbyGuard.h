// A C++ exception never unwinds through a Rust frame (R6 WON round 33). A host op whose statement can throw runs it
// under WonGuard: the exception is kept and the op answers kWonThrown; the Rust stops, undoes what the unwinding
// would have (the locals it released) and answers kWonThrown too; the C++ entry point rethrows it (WonRethrow).
#pragma once
#include <stdint.h>
#include <exception>
#include <utility>

namespace WONAPI {
static const intptr_t kWonThrown = INTPTR_MIN; // the Rust's EE_WON_LOBBY_THROWN
inline std::exception_ptr& WonPending()
{
    static thread_local std::exception_ptr sPending;
    return sPending;
}
// f() as the op's answer, or kWonThrown with the exception kept
template <class F> intptr_t WonGuard(F f)
{
    try {
        return (intptr_t)f();
    } catch (...) {
        WonPending() = std::current_exception();
        return kWonThrown;
    }
}
// f() run (no answer: 0), or kWonThrown with the exception kept
template <class F> intptr_t WonGuardV(F f)
{
    return WonGuard([&] { f(); return 0; });
}
// the Rust's answer, the kept exception rethrown when it is kWonThrown
inline intptr_t WonRethrow(intptr_t inResult)
{
    if (inResult == kWonThrown) std::rethrow_exception(std::exchange(WonPending(), nullptr));
    return inResult;
}
}
