//! WONLobby: the lobby's screens, logic and data model, and the Empire Earth lobby glue.

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

#[cfg(test)]
mod chat_golden;
pub mod chatctl;
pub mod chatobj;
pub mod dialogs;
pub mod dlglogic;
pub mod eecolor;
pub mod eegame;
pub mod eeglue;
pub mod eeresource;
pub mod eesound;
pub mod eexfer;
pub mod ffi_lan;
pub mod flow;
pub mod lan;
pub mod lanlogic;
pub mod lanobj;
pub mod lanown;
pub mod lobbyctl;
pub mod lobbydlg;
pub mod lobbyhost;
pub mod lobbymap;
pub mod lobbypopup;
pub mod logics;
pub mod model;
pub mod modelobj;
pub mod netctl;
pub mod persist;
pub mod persistobj;
pub mod prefsobj;
pub mod resource;
pub mod routelogic;
pub mod routing;
pub mod scrolltext;
pub mod srvlistctl;
pub mod staging;
pub mod stagingobj;
pub mod statusstr;
