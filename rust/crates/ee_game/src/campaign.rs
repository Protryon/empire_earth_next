//! A player's progress through a campaign (`EECampaignProgress`, world lane round 1) whole in Rust: the completed
//! scenarios, their civilization keys and civ points, read from and written to the player's progress archive
//! ("Users\<player>\<campaign><checksum>.ssa"), and the civilization a campaign scenario starts with. The C++ class is
//! a handle and one-statement forwards; each C++ call is one operation ([`EeCampaignCpp`]) answered by
//! `ee_campaigncpp` (`src/Empire Earth/Campaign/EECampaignProgress.cpp`).

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};

/// The progress' calls into the file system and the game (p: the object a call names).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeCampaignCpp {
    /// `gStartupVolume` into the sink a0
    StartupVolume = 1,
    StartupDirectory,
    /// the saved-game extension (`gU530B88`, ".ssa")
    ArchiveExtension,
    /// `new FSPath`
    PathNew,
    PathDereference,
    /// `AddDirectory` of "<startup>Users\<a0>\"
    PathAddUserDirectory,
    /// `FindFile(UString(a0), *(FSFileSpec**)a1)`
    PathFindFile,
    /// a new `FSFileSpec` of "<startup>Users\<a0>\" with the file a1
    SpecNewUser,
    SpecDelete,
    /// `Delete()` of the file
    SpecRemove,
    ArchiveNew,
    ArchiveOpen,
    ArchiveClose,
    ArchiveDelete,
    /// `GetTag(a0, value)` with the value a1 in and out (the sink a2) -> the result
    ArchiveGetTag,
    ArchiveSetTag,
    /// `FindFile(UString(a0), *(FSMemoryFile**)a1)`
    ArchiveFindFile,
    /// `Create(*(FSFileSpec*)a0, *(std::vector<UString>*)a1, 0x1000)`
    ArchiveCreate,
    /// `Copy(UString(a0), false, true)` of the archived file
    MemoryCopy,
    NamesNew,
    NamesPush,
    NamesDelete,
    /// `((std::vector<UWideString>*)p)->push_back(a0)`
    WidePush,
    WideClear,
    /// `PlatMbsICmp(a0, a1)`
    MbsICmp,
    BuiltInCampaign,
    AvailablePoints,
    AddPoints,
    CivBuilderEdit,
    CivBuilderSave,
    CivBuilderFree,
    ApplyCivilization,
    // ---- EECampaign::Load (p: the EECampaign, or the UWideString or file named)
    MemberRequiredCiv,
    MemberBackgroundImage,
    MemberCampaignName,
    MemberDescription,
    MemberAuthorName,
    MemberAuthorEmail,
    MemberPassword,
    /// `*(UWideString*)p = a0`
    WideAssign,
    /// `gDataPath->FindFile(UString(a0), *(FSFileSpec**)a1)`
    DataPathFindFile,
    DataPathExists,
    /// `GetTag(a0, *(UWideString*)a1)`
    ArchiveGetTagInto,
    /// a line of the list file into the sink a0 -> `UString::Load`'s result
    LineLoad,
    MemoryDelete,
    /// `*(UWideString*)a1 = DecryptPassword(a0)`
    Decrypt,
    /// `GetScenarioListFile()` into the sink a0
    ScenarioListFile,
    ScenariosClear,
    /// the scenario without its extension (the last four characters) pushed
    ScenariosPushStripped,
    ErrorReadingTag,
    ErrorCouldntOpen,
    ErrorScenarioMissing,
}

use EeCampaignCpp as O;

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::EeCampaignCpp;
    extern "C" {
        // EECampaignProgress.cpp
        pub fn ee_campaigncpp(op: EeCampaignCpp, p: usize, a: *const i64) -> i64;
    }
}

/// The tests run no C++: a call is a bug of the test.
#[cfg(test)]
mod sys {
    use super::EeCampaignCpp;
    pub unsafe fn ee_campaigncpp(op: EeCampaignCpp, _: usize, _: *const i64) -> i64 {
        panic!("no C++ in the tests ({op:?})")
    }
}

type P = usize;

fn h(op: O, p: P, a: &[i64]) -> i64 {
    let mut ai = [0i64; 4];
    ai[..a.len()].copy_from_slice(a);
    // SAFETY: the host's operation over the C++ objects the caller holds
    unsafe { sys::ee_campaigncpp(op, p, ai.as_ptr()) }
}

/// The text sink the C++ fills (`ee_campaign_text_push`).
pub struct EeCampaignText(Vec<u8>);

/// Appends `length` bytes to a sink.
///
/// # Safety
/// `sink` is a sink the caller handed out; `bytes` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_text_push(sink: *mut EeCampaignText, bytes: *const u8, length: usize) {
    if length != 0 {
        // SAFETY: the caller's guarantee
        unsafe { (*sink).0.extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
    }
}

fn text(op: O, p: P, a: &[i64]) -> Vec<u8> {
    let mut sink = EeCampaignText(Vec::new());
    let mut ai = vec![&mut sink as *mut EeCampaignText as usize as i64];
    ai.extend_from_slice(a);
    h(op, p, &ai);
    sink.0
}

/// A C string for one call.
struct C(Vec<u8>);

impl C {
    fn new(bytes: &[u8]) -> Self {
        let mut v = bytes.split(|&c| c == 0).next().unwrap_or(&[]).to_vec();
        v.push(0);
        C(v)
    }
    fn a(&self) -> i64 {
        self.0.as_ptr() as usize as i64
    }
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

const ERROR_OK: c_long = 0;
const SCENARIO_NAME_KEY: &str = "ScenarioName";
const NUM_SCENARIOS_COMPLETED_KEY: &[u8] = b"NumScenariosCompleted";
const NUM_CIV_POINTS_AVAILABLE_KEY: &str = "NumCivPointsAvailable";
const CHECKSUM_KEY: &[u8] = b"Checksum";
/// `gU5656B8`, `gU5656C8`: the campaign civilizations' folder and extension
const CIVILIZATIONS: &[u8] = b"Civilizations";
const CIV_EXTENSION: &[u8] = b".civ";

/// `atoi`: leading white space, a sign, the digits (wrapping as the C runtime's 32-bit `long`).
pub fn atoi(text: &[u8]) -> c_long {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let negative = match text.get(i) {
        Some(b'-') => {
            i += 1;
            true
        }
        Some(b'+') => {
            i += 1;
            false
        }
        _ => false,
    };
    let mut value: i32 = 0;
    while let Some(&c) = text.get(i).filter(|c| c.is_ascii_digit()) {
        value = value.wrapping_mul(10).wrapping_add((c - b'0') as i32);
        i += 1;
    }
    (if negative { value.wrapping_neg() } else { value }) as c_long
}

/// `MakeKey`: a saved civilization's key "<campaign>.<scenario>.<checksum>".
pub fn make_key(campaign: &[u8], scenario: &[u8], checksum: c_ulong) -> Vec<u8> {
    cat(&[campaign, b".", scenario, b".", format!("{}", checksum as c_long).as_bytes()])
}

/// "<name><index, at least three digits>" (`"%s%.03ld"`).
fn indexed(name: &str, index: c_ulong) -> Vec<u8> {
    format!("{}{:03}", name, index as c_long).into_bytes()
}

/// `EECampaignProgress`' lists (the C++ class's handle).
#[derive(Debug, Default)]
pub struct EeCampaignProgress {
    pub completed: Vec<Vec<u8>>,
    pub keys: Vec<Vec<u8>>,
    pub points: Vec<c_long>,
    pub count: c_long,
}

/// A tag's value through the C++'s `GetTag`, which writes it only when the tag is there (`value` in and out).
fn get_tag(archive: P, name: &[u8], value: &mut Vec<u8>) -> c_long {
    let name = C::new(name);
    let current = C::new(value);
    let mut sink = EeCampaignText(Vec::new());
    let result =
        h(O::ArchiveGetTag, archive, &[name.a(), current.a(), &mut sink as *mut EeCampaignText as usize as i64]);
    *value = sink.0;
    result as c_long
}

fn user_file(user: &[u8], tail: &[&[u8]]) -> Vec<u8> {
    let volume = text(O::StartupVolume, 0, &[]);
    let directory = text(O::StartupDirectory, 0, &[]);
    let mut parts: Vec<&[u8]> = vec![&volume, &directory, b"Users\\", user, b"\\"];
    parts.extend_from_slice(tail);
    cat(&parts)
}

impl EeCampaignProgress {
    fn clear(&mut self) {
        self.keys.clear();
        self.completed.clear();
        self.points.clear();
    }

    /// `Load`: the player's progress archive for a campaign (its completed scenarios, their civ points and saved
    /// civilizations, optionally extracted to the player's folder); cleared on an error; no archive is no error.
    pub fn load(&mut self, user: &[u8], campaign: &[u8], checksum: c_ulong, extract: bool) -> c_long {
        let checksum_text = format!("{}", checksum as c_long);
        let path = h(O::PathNew, 0, &[]) as P;
        let user_c = C::new(user);
        h(O::PathAddUserDirectory, path, &[user_c.a()]);
        let file = C::new(&cat(&[campaign, checksum_text.as_bytes(), &text(O::ArchiveExtension, 0, &[])]));
        let mut spec: usize = 0;
        let mut result = h(O::PathFindFile, path, &[file.a(), &mut spec as *mut usize as usize as i64]) as c_long;
        let mut archive: P = 0;
        if result == ERROR_OK {
            archive = h(O::ArchiveNew, spec, &[]) as P;
            result = h(O::ArchiveOpen, archive, &[]) as c_long;
            if result == ERROR_OK {
                let mut scenario: Vec<u8> = Vec::new();
                let mut points: Vec<u8> = Vec::new();
                get_tag(archive, NUM_SCENARIOS_COMPLETED_KEY, &mut scenario);
                self.count = atoi(&scenario);
                self.completed.clear();
                self.keys.clear();
                self.points.clear();
                let mut i: c_ulong = 0;
                while i < self.count as c_ulong {
                    get_tag(archive, &indexed(SCENARIO_NAME_KEY, i), &mut scenario);
                    let key = make_key(campaign, &scenario, checksum);
                    let available =
                        if get_tag(archive, &indexed(NUM_CIV_POINTS_AVAILABLE_KEY, i), &mut points) == ERROR_OK {
                            atoi(&points)
                        } else {
                            0
                        };
                    self.completed.push(scenario.clone());
                    self.keys.push(key.clone());
                    self.points.push(available);
                    let civilization = C::new(&cat(&[CIVILIZATIONS, b"\\", &key, CIV_EXTENSION]));
                    let mut memory: usize = 0;
                    result =
                        h(O::ArchiveFindFile, archive, &[civilization.a(), &mut memory as *mut usize as usize as i64])
                            as c_long;
                    if result != ERROR_OK {
                        break;
                    }
                    i += 1;
                    if !extract {
                        continue;
                    }
                    let destination = C::new(&user_file(user, &[CIVILIZATIONS, b"\\", &key, CIV_EXTENSION]));
                    result = h(O::MemoryCopy, memory, &[destination.a()]) as c_long;
                    if result != ERROR_OK {
                        break;
                    }
                }
            }
            h(O::ArchiveClose, archive, &[]);
            h(O::SpecDelete, spec, &[]);
        } else {
            result = ERROR_OK;
        }
        if result != ERROR_OK {
            self.clear();
            self.count = 0;
        }
        h(O::ArchiveDelete, archive, &[]);
        h(O::PathDereference, path, &[]);
        result
    }

    /// `RestoreCivilization`: before a scenario of a player's campaign, the progress loaded and the current player
    /// given the civilization saved after the previous scenario and the civ points then available.
    pub fn restore_civilization(&mut self, user: &[u8], campaign: &[u8], scenario: &[u8], checksum: c_ulong) -> c_long {
        let path = h(O::PathNew, 0, &[]) as P;
        let campaign_c = C::new(campaign);
        if h(O::BuiltInCampaign, 0, &[campaign_c.a()]) as c_ulong != 0 {
            h(O::PathDereference, path, &[]);
            return ERROR_OK;
        }
        let mut extracted = false;
        let mut key: Vec<u8> = Vec::new();
        let mut result = self.load(user, campaign, checksum, false);
        if result == ERROR_OK {
            let checksum_text = format!("{}", checksum as c_long);
            let user_c = C::new(user);
            h(O::PathAddUserDirectory, path, &[user_c.a()]);
            let file = C::new(&cat(&[campaign, checksum_text.as_bytes(), &text(O::ArchiveExtension, 0, &[])]));
            let mut spec: usize = 0;
            result = h(O::PathFindFile, path, &[file.a(), &mut spec as *mut usize as usize as i64]) as c_long;
            if result == ERROR_OK {
                let archive = h(O::ArchiveNew, spec, &[]) as P;
                result = h(O::ArchiveOpen, archive, &[]) as c_long;
                if result == ERROR_OK {
                    let mut available: c_ulong = 0;
                    let mut previous: Vec<u8> = Vec::new();
                    let mut completed = self.completed.clone();
                    if !completed.iter().any(|c| c.as_slice() == scenario) {
                        completed.push(scenario.to_vec());
                    }
                    let scenario_c = C::new(scenario);
                    for i in 0..completed.len() {
                        if i > 0 {
                            previous = completed[i - 1].clone();
                            available = self.points[i - 1] as c_ulong;
                        }
                        let name = C::new(&completed[i]);
                        if h(O::MbsICmp, 0, &[name.a(), scenario_c.a()]) == 0 {
                            break;
                        }
                    }
                    if !previous.is_empty() {
                        key = make_key(campaign, &previous, checksum);
                        let civilization = C::new(&cat(&[CIVILIZATIONS, b"\\", &key, CIV_EXTENSION]));
                        let mut memory: usize = 0;
                        result = h(
                            O::ArchiveFindFile,
                            archive,
                            &[civilization.a(), &mut memory as *mut usize as usize as i64],
                        ) as c_long;
                        if result == ERROR_OK {
                            let destination = C::new(&user_file(user, &[CIVILIZATIONS, b"\\", &key, CIV_EXTENSION]));
                            result = h(O::MemoryCopy, memory, &[destination.a()]) as c_long;
                            if result == ERROR_OK {
                                extracted = true;
                                let key_c = C::new(&key);
                                result = h(O::ApplyCivilization, 0, &[key_c.a()]) as c_long;
                                if result == ERROR_OK && available > 0 {
                                    h(O::AddPoints, 0, &[available as i64]);
                                }
                            }
                        }
                    }
                }
                h(O::ArchiveClose, archive, &[]);
                h(O::ArchiveDelete, archive, &[]);
                h(O::SpecDelete, spec, &[]);
            } else {
                result = ERROR_OK;
            }
            if extracted {
                let temporary_name = C::new(&cat(&[CIVILIZATIONS, b"\\", &key, CIV_EXTENSION]));
                let mut temporary: usize = 0;
                if h(O::PathFindFile, path, &[temporary_name.a(), &mut temporary as *mut usize as usize as i64]) == 0 {
                    h(O::SpecRemove, temporary, &[]);
                    h(O::SpecDelete, temporary, &[]);
                }
            }
        }
        // the search path is released on every path (it was released only when a civilization was extracted)
        h(O::PathDereference, path, &[]);
        result
    }

    /// `SaveCompletedScenario`: the current civilization saved, then the player's progress archive rewritten with
    /// every completed scenario, its civ points and civilization file.
    pub fn save_completed_scenario(
        &mut self,
        user: &[u8],
        campaign: &[u8],
        scenario: &[u8],
        checksum: c_ulong,
    ) -> c_long {
        let key = make_key(campaign, scenario, checksum);
        let checksum_text = format!("{}", checksum as c_long);
        let path = h(O::PathNew, 0, &[]) as P;
        let user_c = C::new(user);
        h(O::PathAddUserDirectory, path, &[user_c.a()]);
        let available = h(O::AvailablePoints, 0, &[]) as c_long;
        let already = self.completed.iter().any(|c| c.as_slice() == scenario);
        h(O::CivBuilderEdit, 0, &[]);
        let key_c = C::new(&key);
        let mut result = h(O::CivBuilderSave, 0, &[key_c.a()]) as c_long;
        if result == ERROR_OK {
            if !already {
                self.count = self.count.wrapping_add(1);
                self.completed.push(scenario.to_vec());
                self.keys.push(key.clone());
                self.points.push(available);
            }
            let file = C::new(&cat(&[campaign, checksum_text.as_bytes(), &text(O::ArchiveExtension, 0, &[])]));
            let spec = h(O::SpecNewUser, 0, &[user_c.a(), file.a()]) as P;
            let archive = h(O::ArchiveNew, spec, &[]) as P;
            let set = |name: &[u8], value: &[u8]| {
                let (n, v) = (C::new(name), C::new(value));
                h(O::ArchiveSetTag, archive, &[n.a(), v.a()]);
            };
            set(CHECKSUM_KEY, checksum_text.as_bytes());
            set(NUM_SCENARIOS_COMPLETED_KEY, format!("{}", self.count).as_bytes());
            let names = h(O::NamesNew, 0, &[]) as P;
            let mut temporary: Vec<Vec<u8>> = Vec::new();
            let mut complete = true;
            let scenario_c = C::new(scenario);
            let mut i: c_ulong = 0;
            while i < self.count as c_ulong {
                let index = i as usize;
                set(&indexed(SCENARIO_NAME_KEY, i), &self.completed[index]);
                let name = C::new(&self.completed[index]);
                let points = if already && h(O::MbsICmp, 0, &[name.a(), scenario_c.a()]) == 0 {
                    available
                } else {
                    self.points[index]
                };
                set(&indexed(NUM_CIV_POINTS_AVAILABLE_KEY, i), format!("{}", points).as_bytes());
                let file_name = cat(&[CIVILIZATIONS, b"\\", &self.keys[index], CIV_EXTENSION]);
                let file_c = C::new(&file_name);
                let mut found: usize = 0;
                if h(O::PathFindFile, path, &[file_c.a(), &mut found as *mut usize as usize as i64]) != 0 {
                    complete = false;
                    break;
                }
                h(O::NamesPush, names, &[file_c.a()]);
                temporary.push(file_name);
                h(O::SpecDelete, found, &[]);
                i += 1;
            }
            if complete {
                result = h(O::ArchiveCreate, archive, &[spec as i64, names as i64]) as c_long;
            }
            for name in &temporary {
                let name_c = C::new(name);
                let mut found: usize = 0;
                if h(O::PathFindFile, path, &[name_c.a(), &mut found as *mut usize as usize as i64]) == 0 {
                    h(O::SpecRemove, found, &[]);
                    h(O::SpecDelete, found, &[]);
                }
            }
            h(O::PathDereference, path, &[]);
            h(O::ArchiveClose, archive, &[]);
            h(O::ArchiveDelete, archive, &[]);
            h(O::NamesDelete, names, &[]);
            h(O::SpecDelete, spec, &[]);
        }
        // the edited civilization deleted (ee_screens civ_whole.rs)
        h(O::CivBuilderFree, 0, &[]);
        result
    }
}

// ---- EECampaign::Load (a campaign archive's tags, password and scenario list)

/// A tag into a C++ `UWideString` member (`GetTag(key, *member)`: left as it was when the tag is missing).
fn tag_into(archive: P, key: &[u8], member: P) -> c_long {
    let key = C::new(key);
    h(O::ArchiveGetTagInto, archive, &[key.a(), member as i64]) as c_long
}

fn set_wide(member: P, value: &[u8]) {
    let value = C::new(value);
    h(O::WideAssign, member, &[value.a()]);
}

/// `EECampaign::Load`: the campaign archive's tags (the required civilization, " " for none), its decrypted password
/// and scenario list (each scenario checked on disk when asked, its extension dropped); false with an error text.
pub fn campaign_load(campaign: P, file: &[u8], error: P, check: bool) -> bool {
    let file = C::new(file);
    let mut spec: usize = 0;
    let mut result = false;
    if h(O::DataPathFindFile, 0, &[file.a(), &mut spec as *mut usize as usize as i64]) as c_long == ERROR_OK {
        let archive = h(O::ArchiveNew, spec, &[]) as P;
        if h(O::ArchiveOpen, archive, &[]) as c_long == ERROR_OK {
            let mut required: Vec<u8> = Vec::new();
            get_tag(archive, b"RequiredCivName", &mut required);
            let member = h(O::MemberRequiredCiv, campaign, &[]) as P;
            set_wide(member, if required.as_slice() == b" " { b"" } else { &required });
            tag_into(archive, b"BackgroundImage", h(O::MemberBackgroundImage, campaign, &[]) as P);
            if tag_into(archive, b"CampaignName", h(O::MemberCampaignName, campaign, &[]) as P) == ERROR_OK
                && tag_into(archive, b"CampaignDescription", h(O::MemberDescription, campaign, &[]) as P) == ERROR_OK
                && tag_into(archive, b"AuthorName", h(O::MemberAuthorName, campaign, &[]) as P) == ERROR_OK
                && tag_into(archive, b"AuthorEmail", h(O::MemberAuthorEmail, campaign, &[]) as P) == ERROR_OK
            {
                let mut password: Vec<u8> = Vec::new();
                get_tag(archive, b"CampaignPassword", &mut password);
                let member = h(O::MemberPassword, campaign, &[]) as P;
                if !password.is_empty() {
                    let password = C::new(&password);
                    h(O::Decrypt, campaign, &[password.a(), member as i64]);
                } else {
                    set_wide(member, b"");
                }
                let list_file = C::new(&text(O::ScenarioListFile, campaign, &[]));
                let mut list: usize = 0;
                if h(O::ArchiveFindFile, archive, &[list_file.a(), &mut list as *mut usize as usize as i64]) as c_long
                    == ERROR_OK
                {
                    h(O::ScenariosClear, campaign, &[]);
                    result = true;
                    loop {
                        let mut sink = EeCampaignText(Vec::new());
                        if h(O::LineLoad, list, &[&mut sink as *mut EeCampaignText as usize as i64]) as c_long
                            != ERROR_OK
                        {
                            break;
                        }
                        let scenario = C::new(&sink.0);
                        if check {
                            let path = C::new(&cat(&[b"Scenarios\\", &sink.0]));
                            if h(O::DataPathExists, 0, &[path.a()]) != 0 {
                                h(O::ScenariosPushStripped, campaign, &[scenario.a()]);
                            } else {
                                h(O::ErrorScenarioMissing, error, &[scenario.a()]);
                                h(O::ScenariosClear, campaign, &[]);
                                result = false;
                                break;
                            }
                        } else {
                            h(O::ScenariosPushStripped, campaign, &[scenario.a()]);
                        }
                    }
                    h(O::MemoryDelete, list, &[]);
                } else {
                    h(O::ErrorReadingTag, error, &[]);
                }
            } else {
                h(O::ErrorReadingTag, error, &[]);
            }
        } else {
            h(O::ErrorCouldntOpen, error, &[]);
        }
        h(O::ArchiveDelete, archive, &[]);
        h(O::SpecDelete, spec, &[]);
    } else {
        h(O::ErrorCouldntOpen, error, &[]);
    }
    result
}

/// `EECampaign::Load` (`campaign` the C++ object, `error` its `UWideString&`).
///
/// # Safety
/// `file` is a C string; the objects are live.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_load(
    campaign: *mut c_void,
    file: *const c_char,
    error: *mut c_void,
    check: bool,
) -> bool {
    // SAFETY: the caller's guarantee
    campaign_load(campaign as P, unsafe { bytes(file) }, error as P, check)
}

// ---- the C++ class's forwards (EECampaignProgress.cpp)

/// # Safety
/// A C string or null.
unsafe fn bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        // SAFETY: the caller's guarantee
        unsafe { CStr::from_ptr(text) }.to_bytes()
    }
}

#[no_mangle]
pub extern "C" fn ee_campaign_progress_new() -> *mut EeCampaignProgress {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` came from `ee_campaign_progress_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_progress_free(p: *mut EeCampaignProgress) {
    if !p.is_null() {
        // SAFETY: the caller's guarantee
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `GetCompletedScenarios` into `out` (a C++ `std::vector<UWideString>*`).
///
/// # Safety
/// `p` is a live progress; `out` a live vector.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_progress_completed(p: *const EeCampaignProgress, out: *mut c_void) {
    // SAFETY: the caller's guarantee
    let completed = unsafe { (*p).completed.clone() };
    h(O::WideClear, out as P, &[]);
    for scenario in &completed {
        let c = C::new(scenario);
        h(O::WidePush, out as P, &[c.a()]);
    }
}

/// `Load` (C strings).
///
/// # Safety
/// `p` is a live progress; the texts are C strings.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_progress_load(
    p: *mut EeCampaignProgress,
    user: *const c_char,
    campaign: *const c_char,
    checksum: c_ulong,
    extract: bool,
) -> c_long {
    // SAFETY: the caller's guarantee
    unsafe { (*p).load(bytes(user), bytes(campaign), checksum, extract) }
}

/// `RestoreCivilization` (C strings).
///
/// # Safety
/// As `ee_campaign_progress_load`.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_progress_restore(
    p: *mut EeCampaignProgress,
    user: *const c_char,
    campaign: *const c_char,
    scenario: *const c_char,
    checksum: c_ulong,
) -> c_long {
    // SAFETY: the caller's guarantee
    unsafe { (*p).restore_civilization(bytes(user), bytes(campaign), bytes(scenario), checksum) }
}

/// `SaveCompletedScenario` (C strings).
///
/// # Safety
/// As `ee_campaign_progress_load`.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_progress_save(
    p: *mut EeCampaignProgress,
    user: *const c_char,
    campaign: *const c_char,
    scenario: *const c_char,
    checksum: c_ulong,
) -> c_long {
    // SAFETY: the caller's guarantee
    unsafe { (*p).save_completed_scenario(bytes(user), bytes(campaign), bytes(scenario), checksum) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn atoi_reads_as_the_c_runtime() {
        assert_eq!(atoi(b"  42x"), 42);
        assert_eq!(atoi(b"-7"), -7);
        assert_eq!(atoi(b"+3"), 3);
        assert_eq!(atoi(b"abc"), 0);
        assert_eq!(atoi(b""), 0);
    }

    #[test]
    fn keys_and_tags_are_the_sprintf_texts() {
        assert_eq!(make_key(b"Camp", b"Scen 1", 0xffff_ffff), b"Camp.Scen 1.-1".to_vec());
        assert_eq!(make_key(b"A", b"B", 12), b"A.B.12".to_vec());
        assert_eq!(indexed(SCENARIO_NAME_KEY, 7), b"ScenarioName007".to_vec());
        assert_eq!(indexed(NUM_CIV_POINTS_AVAILABLE_KEY, 1234), b"NumCivPointsAvailable1234".to_vec());
    }

    #[test]
    fn a_new_progress_is_empty() {
        let p = ee_campaign_progress_new();
        // SAFETY: just made
        unsafe {
            assert!((*p).completed.is_empty() && (*p).count == 0);
            ee_campaign_progress_free(p);
        }
    }
}
