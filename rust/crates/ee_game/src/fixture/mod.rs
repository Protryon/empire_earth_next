//! The in-process UI fixture (TEST ONLY; on only with EE_TEST_UI): the test driver of tools/harness/fixture_game.py
//! (the gates) and posix_game.py. Every 250 ms a thread of its own takes the UI manager's lock and
//!   - writes <dir>/ui-state.txt (atomically: .tmp + rename) in the presentation bridge's format: "form <id> controls
//!     <n>", "command <seq>", "pid <pid>", "stack <ids>", "mouseover <i>", "window <w> <h>", "control <i>" (controls
//!     with a 0x3f0 trigger event), "combo <i> sel <s> lines <n> a|b|..", "check <i> <0|1>", "list <i> sel <s> lines
//!     <n> a|b|.."; the game telemetry: "game ms <t> ticks <n> over <0|1> players <n> map <0|1> local <i>", "comm ct
//!     <n> last <n> paused <0|1> multiplayer <0|1>", one "player <i> observer <0|1> result <r> population <n> victory
//!     <n> civ <locked> <chosen> stock <f>/<w>/<s>/<g>/<i>" per world player, "clock <num>/<den> ms <t> frames <n>",
//!     with a map "camera <x> <y> <z> target <x> <y> zoom <z> mode <m> valid <0|1> <0|1> size <n>"; the box
//!     selection's and the order's lines ([`boxsel`], [`orders`]) and the WON lobby's snapshot;
//!   - when <dir>/frame.request exists, removes it and writes the next presented frame to <dir>/frame.bmp;
//!   - executes the newest line of <dir>/test.command, "<seq> <op> <form> <a> [<b> [<c>]]", once per sequence
//!     number, only while <form> is on top: click, select, check, lselect (a control's own methods), zoom, pointer
//!     (held ms milliseconds), lclick, key, wheel (the platform's events), clickevent, screenevent, chat, boxselect,
//!     cast ([`cast`]), order, and the lobby's won* commands (run on the lobby's own loop); the verdict goes to
//!     stderr ("TEST UI ...").
//!
//! The C++ side (Empire Earth/EETestFixture.cpp) is the host ([`host`]): the UI's controls, the input devices, the
//! game's messages and actions, the picker, the units' methods, the WON lobby's walk. The world's members are read
//! through the world views.

/// cbindgen:ignore
pub mod boxsel;
/// cbindgen:ignore
pub mod cast;
pub mod ffi;
pub mod host;
/// cbindgen:ignore
pub mod orders;
/// cbindgen:ignore
pub mod poll;
/// cbindgen:ignore
pub mod text;

#[cfg(test)]
mod tests;
