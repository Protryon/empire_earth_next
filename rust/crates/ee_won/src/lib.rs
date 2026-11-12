//! The WON library in Rust (docs/RUST_PLAN.md R6, decision D8), leaves first: the message buffers, the text
//! conversions, CRC-16, MD5, the random generator, Blowfish, big integers, ElGamal, the auth blocks and the CD key
//! logic; the plain text the lobby shows where it showed HTML (`plaintext`); and the network layer on the platform
//! layer's sockets: the status names, NetStats, IPAddr, AsyncSocket's core and the socket ops' algorithms. The C++
//! classes of `src/Empire Earth/WONAPI` that remain call these through `ffi` and `ffi_net`.

pub mod admin;
pub mod asyncop;
pub mod auth;
pub mod authctx;
pub mod authflow;
pub mod bigint;
pub mod blowfish;
mod blowfish_tables;
pub mod buffer;
pub mod cachemap;
pub mod cdkey;
pub mod cert;
pub mod chat;
pub mod config;
pub mod configobj;
pub mod crc16;
pub mod db;
pub mod der;
pub mod dialogs;
pub mod dir;
pub mod dirops;
pub mod elgamal;
pub mod ffi;
pub mod ffi_lan;
pub mod ffi_net;
pub mod ffi_ops;
pub mod ffi_parse;
pub mod file;
pub mod flow;
pub mod ftp;
pub mod guicfg;
pub mod guiconf;
pub mod guictl;
pub mod guidraw;
pub mod guiedit;
pub mod guievent;
pub mod guifocus;
pub mod guilayout;
pub mod guilist;
pub mod guimenu;
pub mod guimulti;
pub mod guiscroll;
pub mod guistring;
pub mod guitext;
pub mod guitree;
pub mod guiwin;
pub mod guiwrap;
pub mod http;
pub mod httpengine;
/// cbindgen:ignore
pub mod ipaddr;
pub mod ipobj;
pub mod lan;
pub mod lanlogic;
pub mod md5;
pub mod miscops;
pub mod model;
/// cbindgen:ignore
pub mod netstats;
pub mod opobj;
pub mod ops;
pub mod parser;
pub mod patch;
pub mod path;
pub mod peerauth;
pub mod persist;
pub mod plaintext;
pub mod qsock;
pub mod random;
pub mod regkey;
pub mod resource;
pub mod routemsg;
pub mod routing;
pub mod servctx;
pub mod server;
pub mod socket;
pub mod sockobj;
pub mod sockop;
pub mod sockops;
pub mod sockthread;
pub mod staging;
/// cbindgen:ignore
pub mod status;
pub mod strcase;
pub mod text;
pub mod thread;
pub mod titlecfg;
pub mod udpmgr;
pub mod wire;

#[cfg(test)]
mod buffer_golden;
#[cfg(test)]
mod chat_golden;
#[cfg(test)]
mod golden;
#[cfg(test)]
mod parser_golden;
