//! The lobby's configuration object (LobbyConfig.cpp, R6 WON round 33): the constructor (every WONLobby.cfg key with
//! its default, UseLocalIP from the registry) and HandleValueInstruction's LOBBYDEFINE line, in Rust over
//! one-statement C++ ops (`ConfHost` in LobbyConfig.cpp), in the original's order. The members stay the C++
//! object's; the constructor's registry key and value and the define's name and value are the host's statics, made
//! and released where the locals were.
//!
//! The base class's reader runs under `WonGuard`; its exception goes back to the C++ entry (`WonRethrow`).
//!
//! Reach: the LAN game constructs the configuration (the registry value absent); the registry value found and a
//! LOBBYDEFINE line run only in won_lobby33_test's golden cases.

use std::ffi::c_void;

/// The host: `(config, op, a, b, c) -> result`.
pub type EeWonLobbyConfHostFn =
    Option<unsafe extern "C" fn(config: *mut c_void, op: EeWonLobbyConfOp, a: isize, b: isize, c: isize) -> isize>;

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyConfOp {
    /// `c->KeyAdd("DirServer", c->mDirServers, NULL, true)`.
    KeyDirServer = 1,
    /// `c->KeyAdd("RoomDirServer", c->mRoomDirServers, NULL, true)`.
    KeyRoomDirServer,
    /// `c->KeyAdd("AuthServers", c->mAuthServers, NULL, true)`.
    KeyAuthServers,
    /// `c->KeyAdd("ProductName", c->mProductName, NULL)`.
    KeyProductName,
    /// `c->KeyAdd("ProductDirectory", c->mProductDirectory, NULL)`.
    KeyProductDirectory,
    /// `c->KeyAdd("ResourceDirectory", c->mResourceDirectory, NULL)`.
    KeyResourceDirectory,
    /// `c->KeyAdd("Version", c->mVersion, NULL)`.
    KeyVersion,
    /// `c->KeyAdd("LanVersion", c->mLanVersion, NULL)`.
    KeyLanVersion,
    /// `c->KeyAdd("Language", c->mLanguage, "")`.
    KeyLanguage,
    /// `c->KeyAdd("CRCCheck", c->mCRCCheck, true)`.
    KeyCrcCheck,
    /// `c->KeyAdd("CDKeyCheck", c->mCDKeyCheck, true)`.
    KeyCdKeyCheck,
    /// `c->KeyAdd("Public", c->mPublic, false)`.
    KeyPublic,
    /// `c->KeyAdd("LanBroadcastPort", c->mLanBroadcastPort, 8585)`.
    KeyLanBroadcastPort,
    /// `c->KeyAdd("NoPingInProgress", c->mNoPingInProgress, true)`.
    KeyNoPingInProgress,
    /// `c->KeyAdd("LobbyPort", c->mLobbyPort, 8888)`.
    KeyLobbyPort,
    /// `c->KeyAdd("CheckForInternet", c->mCheckForInternet, true)`.
    KeyCheckForInternet,
    /// `c->KeyAdd("PingCeiling", c->mPingCeiling, 1000)`.
    KeyPingCeiling,
    /// `c->KeyAdd("AllowDuplicateNames", c->mAllowDuplicateNames, true)`.
    KeyAllowDuplicateNames,
    /// `c->KeyAdd("AppendNumberToDefaultLanName", c->mAppendNumberToDefaultLanName, true)`.
    KeyAppendNumberToDefaultLanName,
    /// `c->KeyAdd("AllowJoinGameWithoutPing", c->mAllowJoinGameWithoutPing, true)`.
    KeyAllowJoinGameWithoutPing,
    /// `sConfRegistry = new RegKey(2, "SOFTWARE\\Sierra\\WONLobby", false, true)`.
    RegistryOpen,
    /// `sConfRegistryValue = 0`.
    RegistryValueClear,
    /// `sConfRegistry->QueryDWORD("UseLocalIP", sConfRegistryValue)`.
    RegistryQuery,
    /// `sConfRegistryValue`.
    RegistryValue,
    /// `c->mUseLocalIP = a != 0`.
    SetUseLocalIp,
    /// `c->KeyAdd("UseLocalIP", gLobbyUseLocalIP, a != 0)`.
    KeyUseLocalIp,
    /// `delete sConfRegistry`.
    RegistryClose,
    /// `*(const std::string*)a == "LOBBYDEFINE"`.
    KeyIsLobbyDefine,
    /// `sConfName = sConfValue = std::string()`.
    DefineLocalsClear,
    /// `((StringParser*)a)->ReadString(sConfName, true)`.
    DefineReadName,
    /// `((StringParser*)a)->ReadString(sConfValue, false)`.
    DefineReadValue,
    /// `sConfName.empty()`.
    DefineNameEmpty,
    /// `c->mDefines[sConfName] = sConfValue`.
    DefineStore,
    /// `c->ConfigObject::HandleValueInstruction((ConfigParser*)a, *(const std::string*)b, *(StringParser*)cc)` (under
    /// `WonGuard`: `EE_WON_LOBBY_THROWN` when it threw, the C++ entry rethrows).
    BaseHandleValue,
}

/// The calls in.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyConfCall {
    /// `LobbyConfig::LobbyConfig()`'s body.
    Construct = 1,
    /// `HandleValueInstruction((ConfigParser*)a, *(const std::string*)b, *(StringParser*)c)`.
    HandleValue,
}

use EeWonLobbyConfOp as Op;

/// The constructor's keys, in order.
const KEYS: [Op; 20] = [
    Op::KeyDirServer,
    Op::KeyRoomDirServer,
    Op::KeyAuthServers,
    Op::KeyProductName,
    Op::KeyProductDirectory,
    Op::KeyResourceDirectory,
    Op::KeyVersion,
    Op::KeyLanVersion,
    Op::KeyLanguage,
    Op::KeyCrcCheck,
    Op::KeyCdKeyCheck,
    Op::KeyPublic,
    Op::KeyLanBroadcastPort,
    Op::KeyNoPingInProgress,
    Op::KeyLobbyPort,
    Op::KeyCheckForInternet,
    Op::KeyPingCeiling,
    Op::KeyAllowDuplicateNames,
    Op::KeyAppendNumberToDefaultLanName,
    Op::KeyAllowJoinGameWithoutPing,
];

fn call(
    which: EeWonLobbyConfCall,
    a: isize,
    b: isize,
    c: isize,
    op3: &dyn Fn(Op, isize, isize, isize) -> isize,
) -> isize {
    let op = |o: Op, a: isize| op3(o, a, 0, 0);
    let on = |o: Op| op(o, 0);
    match which {
        EeWonLobbyConfCall::Construct => {
            for k in KEYS {
                on(k);
            }
            // UseLocalIP: the registry's value (the config key writes a global nothing reads)
            on(Op::RegistryOpen);
            on(Op::RegistryValueClear);
            let use_local_ip = on(Op::RegistryQuery) as i32 == 0 && on(Op::RegistryValue) as u32 != 0;
            op(Op::SetUseLocalIp, use_local_ip as isize);
            op(Op::KeyUseLocalIp, use_local_ip as isize);
            on(Op::RegistryClose);
            0
        }
        EeWonLobbyConfCall::HandleValue => {
            if op(Op::KeyIsLobbyDefine, b) == 0 {
                return op3(Op::BaseHandleValue, a, b, c);
            }
            on(Op::DefineLocalsClear);
            op(Op::DefineReadName, c);
            op(Op::DefineReadValue, c);
            if on(Op::DefineNameEmpty) == 0 {
                on(Op::DefineStore);
            }
            1
        }
    }
}

/// One of LobbyConfig's methods on `config` over the host `host`.
///
/// # Safety
/// `host` answers the ops for `config` (ConfHost).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_call(
    config: *mut c_void,
    host: EeWonLobbyConfHostFn,
    which: EeWonLobbyConfCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    let op = |o: Op, a: isize, b: isize, c: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(config, o, a, b, c) },
        None => 0,
    };
    call(which, a, b, c, &op)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(c: EeWonLobbyConfCall, answer: &dyn Fn(Op) -> isize) -> Vec<String> {
        let log = RefCell::new(Vec::new());
        call(c, 2, 3, 4, &|o, a, _, _| {
            log.borrow_mut().push(format!("{o:?} {a}"));
            answer(o)
        });
        log.into_inner()
    }

    #[test]
    fn the_keys_then_the_registry() {
        let log = run(EeWonLobbyConfCall::Construct, &|o| if o == Op::RegistryValue { 1 } else { 0 });
        assert_eq!(log.len(), 27);
        assert_eq!(
            log[20..],
            [
                "RegistryOpen 0",
                "RegistryValueClear 0",
                "RegistryQuery 0",
                "RegistryValue 0",
                "SetUseLocalIp 1",
                "KeyUseLocalIp 1",
                "RegistryClose 0"
            ]
        );
        // not found: the value is not read
        let log = run(EeWonLobbyConfCall::Construct, &|o| (o == Op::RegistryQuery) as isize * 2);
        assert_eq!(log[23], "SetUseLocalIp 0");
    }

    #[test]
    fn a_define_line() {
        let log = run(EeWonLobbyConfCall::HandleValue, &|o| (o == Op::KeyIsLobbyDefine) as isize);
        assert_eq!(
            log[1..],
            ["DefineLocalsClear 0", "DefineReadName 4", "DefineReadValue 4", "DefineNameEmpty 0", "DefineStore 0"]
        );
        assert_eq!(run(EeWonLobbyConfCall::HandleValue, &|_| 0), ["KeyIsLobbyDefine 3", "BaseHandleValue 2"]);
    }
}
