//! LobbyConfig::PostParse (LobbyConfig.cpp, R6 WON round 32): the checks that WONLobby.cfg names directory
//! servers, product, version and language, and the LAN version, resource and product directories and the
//! Product define filled in, in Rust over one-statement C++ ops (`CfgHost` in LobbyConfig.cpp).
//!
//! Reach: every lobby start parses WONLobby.cfg (the LAN game, the tour); the four error branches run only in
//! the unit tests (the shipped config names all four).

use std::ffi::c_void;

/// The host: `(config, parser, op) -> result`.
pub type EeWonLobbyCfgHostFn =
    Option<unsafe extern "C" fn(config: *mut c_void, parser: *mut c_void, op: EeWonLobbyCfgOp) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCfgOp {
    /// `mDirServers.empty()`.
    DirServersEmpty = 1,
    /// `mProductName.empty()`.
    ProductNameEmpty,
    /// `mVersion.empty()`.
    VersionEmpty,
    /// `mLanguage.empty()`.
    LanguageEmpty,
    /// `parser->SetError("No Directory Servers specified.", false)`.
    ErrorNoDirServers,
    /// `parser->SetError("No product name specified.", false)`.
    ErrorNoProductName,
    /// `parser->SetError("No version specified.", false)`.
    ErrorNoVersion,
    /// `parser->SetError("No language specified.", false)`.
    ErrorNoLanguage,
    /// `mLanVersion.empty()`.
    LanVersionEmpty,
    /// `mLanVersion = mVersion`.
    LanVersionFromVersion,
    /// `mResourceDirectory.empty()`.
    ResourceDirEmpty,
    /// `mResourceDirectory = ".\\" + mProductName`.
    ResourceDirDefault,
    /// `mProductDirectory.empty()`.
    ProductDirEmpty,
    /// `mProductDirectory = L"/" + StringToWString(mProductName)`.
    ProductDirDefault,
    /// `mDefines["Product"] = GetProductName()`.
    DefineProduct,
}

use EeWonLobbyCfgOp as Op;

fn post_parse(op: &dyn Fn(Op) -> bool) -> bool {
    for (empty, error) in [
        (Op::DirServersEmpty, Op::ErrorNoDirServers),
        (Op::ProductNameEmpty, Op::ErrorNoProductName),
        (Op::VersionEmpty, Op::ErrorNoVersion),
        (Op::LanguageEmpty, Op::ErrorNoLanguage),
    ] {
        if op(empty) {
            op(error);
            return false;
        }
    }
    for (empty, default) in [
        (Op::LanVersionEmpty, Op::LanVersionFromVersion),
        (Op::ResourceDirEmpty, Op::ResourceDirDefault),
        (Op::ProductDirEmpty, Op::ProductDirDefault),
    ] {
        if op(empty) {
            op(default);
        }
    }
    op(Op::DefineProduct);
    true
}

/// `LobbyConfig::PostParse(parser)` of the config `config` over the host `host`.
///
/// # Safety
/// `host` answers the ops for `config` and `parser`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbycfg_post_parse(
    config: *mut c_void,
    parser: *mut c_void,
    host: EeWonLobbyCfgHostFn,
) -> bool {
    let op = |o: Op| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(config, parser, o) != 0 },
        None => false,
    };
    post_parse(&op)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(empty: &[Op]) -> (bool, Vec<Op>) {
        let log = RefCell::new(Vec::new());
        let op = |o: Op| {
            log.borrow_mut().push(o);
            empty.contains(&o)
        };
        let r = post_parse(&op);
        (r, log.into_inner())
    }

    #[test]
    fn the_checks_and_the_defaults() {
        let (ok, log) = run(&[]);
        assert!(ok);
        assert_eq!(log.len(), 8);
        assert_eq!(log.last(), Some(&Op::DefineProduct));
        let (ok, log) = run(&[Op::LanVersionEmpty, Op::ProductDirEmpty]);
        assert!(ok);
        assert!(log.contains(&Op::LanVersionFromVersion) && log.contains(&Op::ProductDirDefault));
        assert!(!log.contains(&Op::ResourceDirDefault));
        let (ok, log) = run(&[Op::VersionEmpty, Op::LanguageEmpty]);
        assert!(!ok);
        assert_eq!(log, [Op::DirServersEmpty, Op::ProductNameEmpty, Op::VersionEmpty, Op::ErrorNoVersion]);
        let (ok, log) = run(&[Op::DirServersEmpty]);
        assert_eq!((ok, log), (false, vec![Op::DirServersEmpty, Op::ErrorNoDirServers]));
    }
}
