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
    /// `AddDirectory(*(FSFileSpec*)a0, true)`
    PathAddDirectory,
    /// `FindFile(UString(a0), *(FSFileSpec**)a1)`
    PathFindFile,
    SpecDelete,
    /// `Delete()` of the file
    SpecRemove,
    ArchiveNew,
    ArchiveOpen,
    ArchiveClose,
    ArchiveDelete,
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
    /// `EEApplyCivilizationFile(*(UString*)p, gWorld.GetCurrentPlayer())`
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
    /// `new UString(a0)`
    LineNew,
    /// `((UString*)p)->Load(*(FSMemoryFile*)a0)`
    LineLoad,
    /// the `UString`'s text into the sink a0
    LineText,
    LineDelete,
    MemoryDelete,
    ScenariosClear,
    /// the scenario without its extension (the last four characters) pushed
    ScenariosPushStripped,
    ErrorReadingTag,
    ErrorCouldntOpen,
    // ---- EECampaign's editor side (Save, the file lists; p: the object named)
    /// a `UWideString`'s text into the sink a0
    WideText,
    /// `*(UWideString*)p += a0`
    WideAppend,
    /// `new UWideString`
    WideNew,
    WideDelete,
    MemberDirectory,
    MemberScenarios,
    /// `new std::vector<UWideString>`
    WideListNew,
    WideListDelete,
    WideListLen,
    /// element a1 into the sink a0
    WideListAt,
    /// `std::find` of a0 (`operator==`) in the list
    WideListContains,
    /// `gScenarioExtension` into the sink a0
    ScenarioExtension,
    /// `gCampaignExtension` into the sink a0
    CampaignExtension,
    /// `gU52A280` (the campaign scenarios' prefix) into the sink a0
    ScenarioPrefix,
    /// `gResource->GetString(a0, *(UWideString*)p)`
    ResourceString,
    /// `sprintf(*(UWideString*)p, *(UWideString*)a0, a1, a2)`
    Sprintf,
    /// `new FSFileSpec`
    SpecNew,
    SpecSetStartupVolume,
    SpecSetDirectory,
    SpecSetFile,
    SpecCreate,
    SpecOpenWrite,
    SpecClose,
    /// `Copy(UString(a0), false, false)`
    SpecCopy,
    /// volume + directory + file into the sink a0
    SpecPathText,
    /// `UString(a0).Save(*(FSFileSpec*)p)`
    LineSave,
    /// `gDataPath->GetFileArchiveStatus(a0, *(bool*)a1)`
    ArchiveStatus,
    /// `new EEPSaveGameHeader`
    HeaderNew,
    /// `LoadFromFile(*(FSFileSpec*)a0)`
    HeaderLoad,
    /// a copy of the header's media strings' `ListMediaFiles(a0, list a1, error a2, prefixed a3)`
    HeaderListMedia,
    HeaderDelete,
    /// `EELoadPersistent(a0, 6, *(FSPersistent**)a1)`
    PersistentLoad,
    /// the restorable's scenario manager, or 0
    PersistentManager,
    ManagerEffects,
    ManagerEffect,
    /// a media effect that plays a sound or sends a dialogue
    EffectIsSound,
    /// the effect's file name into the sink a0
    EffectFileName,
    /// the restorable's manager deleted (its pointer cleared)
    PersistentFreeManager,
    PersistentDelete,
    // ---- EECampaignFormManager (p: the manager)
    /// `&mCampaignName`
    FormCampaignName,
    /// `mChecksum = a0`
    FormSetChecksum,
    /// `EEGetFileChecksum(*(FSFileSpec*)p, UString(a0), 0)`
    FileChecksum,
    /// built-in campaign a1's name (gCampaignAOCRoman .. gCampaignEELearning) into the sink a0
    BuiltInName,
    /// built-in campaign a0's checksum (gAOCRomanChecksum ..)
    BuiltInChecksum,
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
    let wide = h(O::WideNew, 0, &[]) as P;
    set_wide(wide, value);
    let result = tag_into(archive, name, wide);
    *value = wide_text(wide);
    h(O::WideDelete, wide, &[]);
    result
}

/// "<startup volume><startup directory>Users\<user>\<file>" as a new spec (the caller deletes it).
fn user_spec(user: &[u8], file: &[u8]) -> P {
    let directory = cat(&[&text(O::StartupDirectory, 0, &[]), b"Users\\", user, b"\\"]);
    new_spec(&directory, file)
}

/// The player's folder added to a path.
fn add_user_directory(path: P, user: &[u8]) {
    let spec = user_spec(user, b"");
    h(O::PathAddDirectory, path, &[spec as i64]);
    h(O::SpecDelete, spec, &[]);
}

/// A `UString` of `value` for one call (`EEApplyCivilizationFile` takes a non-const one), deleted after.
fn with_line<T>(value: &[u8], f: impl FnOnce(P) -> T) -> T {
    let value = C::new(value);
    let line = h(O::LineNew, 0, &[value.a()]) as P;
    let result = f(line);
    h(O::LineDelete, line, &[]);
    result
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
        add_user_directory(path, user);
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
            add_user_directory(path, user);
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
                                result = with_line(&key, |line| h(O::ApplyCivilization, line, &[]) as c_long);
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
        add_user_directory(path, user);
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
            let file = cat(&[campaign, checksum_text.as_bytes(), &text(O::ArchiveExtension, 0, &[])]);
            let spec = user_spec(user, &file);
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
                    set_wide(member, &decrypt_password(&password));
                } else {
                    set_wide(member, b"");
                }
                let list_file = C::new(&scenario_list_file());
                let mut list: usize = 0;
                if h(O::ArchiveFindFile, archive, &[list_file.a(), &mut list as *mut usize as usize as i64]) as c_long
                    == ERROR_OK
                {
                    h(O::ScenariosClear, campaign, &[]);
                    result = true;
                    loop {
                        let mut sink = EeCampaignText(Vec::new());
                        let loaded = with_line(b"", |line| {
                            let loaded = h(O::LineLoad, line, &[list as i64]) as c_long;
                            h(O::LineText, line, &[&mut sink as *mut EeCampaignText as usize as i64]);
                            loaded
                        });
                        if loaded != ERROR_OK {
                            break;
                        }
                        let scenario = C::new(&sink.0);
                        if check {
                            let path = C::new(&cat(&[b"Scenarios\\", &sink.0]));
                            if h(O::DataPathExists, 0, &[path.a()]) != 0 {
                                h(O::ScenariosPushStripped, campaign, &[scenario.a()]);
                            } else {
                                format_error(error, IDS_SCENARIO_DIDNT_EXIST_ON_DISK, &[&sink.0]);
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

// ---- EECampaign's editor side (the campaign editor's Save and file list)

/// The string ids the editor side reads (ee/EEStringIDs.h).
const IDS_THERE_WAS_ERROR_WRITING_CAMPAIGN: i64 = 7115;
const IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN: i64 = 7120;
const IDS_COULDNT_OPEN_CAMPAIGN_FOR_WRITING: i64 = 7125;
const IDS_FILES_BELOW_ARE_USED_IN_SCENARIO: i64 = 7126;
const IDS_SCENARIO_DIDNT_EXIST_ON_DISK: i64 = 7163;
const IDS_SE_MAP_NONE: i64 = 20009;
const IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS: i64 = 31847;

/// The archive's tag keys (the original's kEECampaign*Key).
const TAG_REQUIRED_CIV: &[u8] = b"RequiredCivName";
const TAG_BACKGROUND_IMAGE: &[u8] = b"BackgroundImage";
const TAG_CAMPAIGN_NAME: &[u8] = b"CampaignName";
const TAG_DESCRIPTION: &[u8] = b"CampaignDescription";
const TAG_AUTHOR_NAME: &[u8] = b"AuthorName";
const TAG_AUTHOR_EMAIL: &[u8] = b"AuthorEmail";
const TAG_PASSWORD: &[u8] = b"CampaignPassword";
const SCENARIO_LIST_KEY: &[u8] = b"ScenarioList.txt";

fn wide_text(wide: P) -> Vec<u8> {
    text(O::WideText, wide, &[])
}

fn member_text(op: O, campaign: P) -> Vec<u8> {
    wide_text(h(op, campaign, &[]) as P)
}

fn wide_append(wide: P, value: &[u8]) {
    let value = C::new(value);
    h(O::WideAppend, wide, &[value.a()]);
}

fn list_items(list: P) -> Vec<Vec<u8>> {
    (0..h(O::WideListLen, list, &[])).map(|i| text(O::WideListAt, list, &[i])).collect()
}

fn list_push(list: P, value: &[u8]) {
    let value = C::new(value);
    h(O::WidePush, list, &[value.a()]);
}

fn list_contains(list: P, value: &[u8]) -> bool {
    let value = C::new(value);
    h(O::WideListContains, list, &[value.a()]) != 0
}

/// `gDataPath->FindFile(UString(file), spec)`: the found spec (the caller deletes it).
fn find_data(file: &[u8]) -> Option<P> {
    let file = C::new(file);
    let mut spec: usize = 0;
    (h(O::DataPathFindFile, 0, &[file.a(), &mut spec as *mut usize as usize as i64]) as c_long == ERROR_OK)
        .then_some(spec)
}

fn data_exists(file: &[u8]) -> bool {
    let file = C::new(file);
    h(O::DataPathExists, 0, &[file.a()]) != 0
}

/// `NotArchived`: the file lies outside the game's archives.
fn not_archived(file: &[u8]) -> bool {
    let file = C::new(file);
    let mut archived = false;
    h(O::ArchiveStatus, 0, &[file.a(), &mut archived as *mut bool as usize as i64]);
    !archived
}

/// `ToLower` (`PlatMbsLwr`: the UTF-8 fold in place).
fn lower(text: &[u8]) -> Vec<u8> {
    let mut v = text.to_vec();
    ee_base::plat_core::utf8::map_in_place(&mut v, ee_base::plat_core::utf8::fold);
    v
}

/// `UString::Find(needle, 0)`: the first position, or the length.
fn find(text: &[u8], needle: &[u8]) -> usize {
    ee_base::ustring::find(text, needle, 0) as usize
}

/// `sprintf(*error, <string id>, args...)` (at most two `%s` arguments).
fn format_error(error: P, id: i64, args: &[&[u8]]) {
    let format = h(O::WideNew, 0, &[]) as P;
    h(O::ResourceString, format, &[id]);
    let first = C::new(args.first().copied().unwrap_or(b""));
    let second = C::new(args.get(1).copied().unwrap_or(b""));
    h(O::Sprintf, error, &[format as i64, first.a(), second.a()]);
    h(O::WideDelete, format, &[]);
}

fn resource_text(id: i64) -> Vec<u8> {
    let wide = h(O::WideNew, 0, &[]) as P;
    h(O::ResourceString, wide, &[id]);
    let value = wide_text(wide);
    h(O::WideDelete, wide, &[]);
    value
}

/// The codec's output into a `Vec<u8>` (the context).
unsafe extern "C" fn codec_sink(context: *mut c_void, bytes: *const u8, length: usize) {
    // SAFETY: the context is the caller's vector; the codec hands `length` bytes
    let out = unsafe { &mut *(context as *mut Vec<u8>) };
    out.clear();
    if length != 0 {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

/// The codec's text as the C++'s `UString` kept it: up to its first NUL.
fn up_to_nul(mut text: Vec<u8>) -> Vec<u8> {
    if let Some(end) = text.iter().position(|&c| c == 0) {
        text.truncate(end);
    }
    text
}

/// `EECampaign::EncryptPassword` (`UEEncryptUString` of a `UString` made from the text: capacity length + 1).
pub fn encrypt_password(password: &[u8]) -> Vec<u8> {
    let password = up_to_nul(password.to_vec());
    let mut out = Vec::new();
    // SAFETY: the bytes and the sink's vector live through the call
    unsafe {
        ee_base::codec::ffi::ee_ue_encrypt_ustring(
            password.as_ptr(),
            password.len(),
            password.len() + 1,
            codec_sink,
            &mut out as *mut Vec<u8> as *mut c_void,
        )
    };
    up_to_nul(out)
}

/// `EECampaign::DecryptPassword` (`UEDecryptUString`; a failed checksum still gives its text).
pub fn decrypt_password(password: &[u8]) -> Vec<u8> {
    let password = up_to_nul(password.to_vec());
    let mut out = Vec::new();
    // SAFETY: as encrypt_password
    unsafe {
        ee_base::codec::ffi::ee_ue_decrypt_ustring(
            password.as_ptr(),
            password.len(),
            codec_sink,
            &mut out as *mut Vec<u8> as *mut c_void,
        )
    };
    up_to_nul(out)
}

/// `GetScenarioListFile`: "Campaigns\<prefix>ScenarioList.txt".
fn scenario_list_file() -> Vec<u8> {
    cat(&[b"Campaigns\\", &text(O::ScenarioPrefix, 0, &[]), SCENARIO_LIST_KEY])
}

/// A spec of "<startup volume><directory><file>".
fn new_spec(directory: &[u8], file: &[u8]) -> P {
    let spec = h(O::SpecNew, 0, &[]) as P;
    h(O::SpecSetStartupVolume, spec, &[]);
    let directory = C::new(directory);
    h(O::SpecSetDirectory, spec, &[directory.a()]);
    let file = C::new(file);
    h(O::SpecSetFile, spec, &[file.a()]);
    spec
}

/// The file name a sound effect's file has on disk: as it is when it ends in .wav or .mp3, else with .wav, else
/// with .mp3; empty when none exists (`exists` answers for the data path).
pub fn sound_file(name: &[u8], exists: &mut dyn FnMut(&[u8]) -> bool) -> Vec<u8> {
    let n = name.len();
    if n > 4 && (name[n - 4..].eq_ignore_ascii_case(b".wav") || name[n - 4..].eq_ignore_ascii_case(b".mp3")) {
        return if exists(name) { name.to_vec() } else { Vec::new() };
    }
    for extension in [&b".wav"[..], b".mp3"] {
        let file = cat(&[name, extension]);
        if exists(&file) {
            return file;
        }
    }
    Vec::new()
}

/// `inPrefixed`'s file name: the prefix inserted where the sound's name starts (unchanged when it is not there).
pub fn prefixed_at(file: &[u8], name: &[u8], prefix: &[u8]) -> Vec<u8> {
    let position = find(file, name);
    if position == file.len() {
        return file.to_vec();
    }
    cat(&[&file[..position], prefix, &file[position..]])
}

/// `ListEffectSound`: a play-sound or dialogue effect's sound (.wav or .mp3, optionally prefixed) into the list
/// unless archived or listed; false with its name as the error when the sound is missing.
fn list_effect_sound(effect: P, files: P, error: &mut Vec<u8>, prefixed: bool) -> bool {
    if h(O::EffectIsSound, effect, &[]) == 0 {
        return true;
    }
    let sound = text(O::EffectFileName, effect, &[]);
    if sound.is_empty() {
        return true;
    }
    let name = cat(&[b"Sounds\\", &sound]);
    let mut file = sound_file(&name, &mut |file| data_exists(file));
    if file.is_empty() {
        *error = cat(&[&name, b"\n"]);
        return false;
    }
    if not_archived(&file) {
        if prefixed {
            file = prefixed_at(&file, &sound, &text(O::ScenarioPrefix, 0, &[]));
        }
        if !list_contains(files, &file) {
            list_push(files, &file);
        }
    }
    true
}

/// `ListEffectSounds`: the sounds of every effect of the scenario's saved scenario manager; the missing ones'
/// names appended to `error` (a `UWideString`).
fn list_effect_sounds(file: &[u8], files: P, error: P, prefixed: bool) -> bool {
    let mut result = false;
    let file = C::new(file);
    let mut persistent: usize = 0;
    if h(O::PersistentLoad, 0, &[file.a(), &mut persistent as *mut usize as usize as i64]) as c_long == ERROR_OK {
        let manager = h(O::PersistentManager, persistent, &[]) as P;
        if manager != 0 {
            result = true;
            let mut missing = Vec::new();
            for i in 0..h(O::ManagerEffects, manager, &[]) {
                let effect = h(O::ManagerEffect, manager, &[i]) as P;
                if !list_effect_sound(effect, files, &mut missing, prefixed) {
                    result = false;
                    wide_append(error, &missing);
                }
            }
            h(O::PersistentFreeManager, persistent, &[]);
        }
        h(O::PersistentDelete, persistent, &[]);
    }
    result
}

/// `ListScenarioMedia`: a scenario's media files (lower case) and effect sounds that are not archived into the list
/// (no duplicates); false with an error naming the missing ones (or the differing scenario).
fn list_scenario_media(campaign: P, file: &[u8], files: P, error: P, prefixed: bool) -> bool {
    let media_error = h(O::WideNew, 0, &[]) as P;
    let mut result = false;
    let mut media_failed = false;
    if let Some(spec) = find_data(file) {
        let header = h(O::HeaderNew, 0, &[]) as P;
        if h(O::HeaderLoad, header, &[spec as i64]) as c_long == ERROR_OK {
            let media = h(O::WideListNew, 0, &[]) as P;
            let name = C::new(file);
            if h(O::HeaderListMedia, header, &[name.a(), media as i64, media_error as i64, prefixed as i64]) != 0 {
                for item in list_items(media) {
                    if !not_archived(&item) {
                        continue;
                    }
                    let item = lower(&item);
                    if !list_contains(files, &item) {
                        list_push(files, &item);
                    }
                }
                result = true;
            } else {
                media_failed = true;
            }
            h(O::WideListDelete, media, &[]);
            if !list_effect_sounds(file, files, media_error, prefixed) {
                result = false;
                media_failed = true;
            }
        }
        h(O::HeaderDelete, header, &[]);
        h(O::SpecDelete, spec, &[]);
    }
    if !result {
        if media_failed {
            format_error(error, IDS_FILES_BELOW_ARE_USED_IN_SCENARIO, &[file]);
            wide_append(error, &cat(&[b"\n\n", &wide_text(media_error)]));
        } else {
            let name = member_text(O::MemberCampaignName, campaign);
            format_error(error, IDS_CAMPAIGN_HAS_SCENARIO_WHICH_DIFFERS, &[&name, file]);
        }
    }
    h(O::WideDelete, media_error, &[]);
    result
}

/// `ListScenarioFiles`: every scenario file (when `add`; optionally prefixed) with its media and sounds, then the
/// background image; false at the first scenario whose files are missing.
pub fn list_scenario_files(campaign: P, files: P, error: P, add: bool, prefixed: bool) -> bool {
    let extension = text(O::ScenarioExtension, 0, &[]);
    let prefix = text(O::ScenarioPrefix, 0, &[]);
    let mut result = true;
    for scenario in list_items(h(O::MemberScenarios, campaign, &[]) as P) {
        let file = cat(&[b"Scenarios\\", &scenario, &extension]);
        if add {
            list_push(
                files,
                &if prefixed { cat(&[b"Scenarios\\", &prefix, &scenario, &extension]) } else { file.clone() },
            );
        }
        if !list_scenario_media(campaign, &file, files, error, prefixed) {
            result = false;
            break;
        }
    }
    let none = resource_text(IDS_SE_MAP_NONE);
    let background = member_text(O::MemberBackgroundImage, campaign);
    if !background.is_empty() && background != none {
        list_push(files, &cat(&[b"Scenarios\\", if prefixed { &prefix } else { b"" }, &background]));
    }
    result
}

/// `CopyScenarioFiles`: each file of `sources` copied beside itself under the prefixed name `copies` gets (the
/// prefixed list, filled here); whether every one was copied.
fn copy_scenario_files(campaign: P, sources: P, copies: P) -> bool {
    let error = h(O::WideNew, 0, &[]) as P;
    let mut result = false;
    if list_scenario_files(campaign, copies, error, true, true) {
        let sources = list_items(sources);
        let copies = list_items(copies);
        let mut copied = 0;
        for (k, copy) in copies.iter().enumerate() {
            let Some(source) = sources.get(k) else { break };
            let Some(spec) = find_data(source) else { break };
            let path = text(O::SpecPathText, spec, &[]);
            let position = find(&lower(&path), &lower(source)).min(path.len());
            let destination = C::new(&cat(&[&path[..position], copy]));
            let failed = h(O::SpecCopy, spec, &[destination.a()]) as c_long != ERROR_OK;
            h(O::SpecDelete, spec, &[]);
            if failed {
                break;
            }
            copied += 1;
        }
        result = copied == copies.len();
    }
    h(O::WideDelete, error, &[]);
    result
}

/// `CreateScenarioList`: the scenario list (a scenario file name per line) written to `file` in the campaign's
/// directory; false with an error text.
fn create_scenario_list(campaign: P, file: &[u8], error: P) -> bool {
    let spec = new_spec(&member_text(O::MemberDirectory, campaign), file);
    h(O::SpecRemove, spec, &[]);
    let mut result = h(O::SpecCreate, spec, &[]) as c_long;
    if result == ERROR_OK {
        result = h(O::SpecOpenWrite, spec, &[]) as c_long;
    }
    if result == ERROR_OK {
        let extension = text(O::ScenarioExtension, 0, &[]);
        for scenario in list_items(h(O::MemberScenarios, campaign, &[]) as P) {
            if result != ERROR_OK {
                break;
            }
            let line = C::new(&cat(&[&scenario, &extension]));
            result = h(O::LineSave, spec, &[line.a()]) as c_long;
        }
        if result != ERROR_OK {
            h(O::ResourceString, error, &[IDS_THERE_WAS_ERROR_WRITING_CAMPAIGN]);
        }
    } else {
        h(O::ResourceString, error, &[IDS_COULDNT_OPEN_CAMPAIGN_FOR_WRITING]);
    }
    let written = result == ERROR_OK && h(O::SpecClose, spec, &[]) as c_long == ERROR_OK;
    h(O::SpecDelete, spec, &[]);
    written
}

/// `SaveArchive`: the archive made of the scenario list file and the copies of every scenario file, the copies
/// deleted afterwards.
fn save_archive(campaign: P, archive: P, error: P) -> bool {
    let files = h(O::WideListNew, 0, &[]) as P;
    let copied = h(O::WideListNew, 0, &[]) as P;
    let mut result = false;
    if list_scenario_files(campaign, files, error, true, false) {
        let list_file = scenario_list_file();
        if create_scenario_list(campaign, &list_file, error) {
            if copy_scenario_files(campaign, files, copied) {
                let names = h(O::NamesNew, 0, &[]) as P;
                for name in list_items(copied).iter().chain(std::iter::once(&list_file)) {
                    let name = C::new(name);
                    h(O::NamesPush, names, &[name.a()]);
                }
                let directory = h(O::SpecNew, 0, &[]) as P;
                let path = C::new(&member_text(O::MemberDirectory, campaign));
                h(O::SpecSetDirectory, directory, &[path.a()]);
                if h(O::ArchiveCreate, archive, &[directory as i64, names as i64]) as c_long == ERROR_OK {
                    result = true;
                } else {
                    h(O::ResourceString, error, &[IDS_THERE_WAS_ERROR_WRITING_CAMPAIGN]);
                }
                h(O::SpecDelete, directory, &[]);
                h(O::NamesDelete, names, &[]);
            }
            if !list_file.is_empty() {
                list_push(copied, &list_file);
            }
            for name in list_items(copied) {
                if let Some(spec) = find_data(&name) {
                    h(O::SpecRemove, spec, &[]);
                    h(O::SpecDelete, spec, &[]);
                }
            }
        }
    }
    h(O::WideListDelete, files, &[]);
    h(O::WideListDelete, copied, &[]);
    result
}

/// `EECampaign::Save`: the archive "Campaigns\<name><extension>" in the campaign's directory with its tags (the
/// password encrypted), then its files; needs a name; false with an error text.
pub fn campaign_save(campaign: P, error: P) -> bool {
    let name = member_text(O::MemberCampaignName, campaign);
    if name.is_empty() {
        h(O::ResourceString, error, &[IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN]);
        return false;
    }
    let extension = text(O::CampaignExtension, 0, &[]);
    let spec = new_spec(&member_text(O::MemberDirectory, campaign), &cat(&[b"Campaigns\\", &name, &extension]));
    let archive = h(O::ArchiveNew, spec, &[]) as P;
    let mut required = member_text(O::MemberRequiredCiv, campaign);
    if required.is_empty() {
        required = b" ".to_vec();
    }
    let set_tag = |key: &[u8], value: &[u8]| {
        let (key, value) = (C::new(key), C::new(value));
        h(O::ArchiveSetTag, archive, &[key.a(), value.a()]) as c_long == ERROR_OK
    };
    let mut result = false;
    if set_tag(TAG_REQUIRED_CIV, &required)
        && set_tag(TAG_BACKGROUND_IMAGE, &member_text(O::MemberBackgroundImage, campaign))
        && set_tag(TAG_CAMPAIGN_NAME, &name)
        && set_tag(TAG_DESCRIPTION, &member_text(O::MemberDescription, campaign))
        && set_tag(TAG_AUTHOR_NAME, &member_text(O::MemberAuthorName, campaign))
        && set_tag(TAG_AUTHOR_EMAIL, &member_text(O::MemberAuthorEmail, campaign))
    {
        let password = member_text(O::MemberPassword, campaign);
        let saved = password.is_empty() || set_tag(TAG_PASSWORD, &encrypt_password(&password));
        if saved {
            result = save_archive(campaign, archive, error);
        }
    }
    h(O::ArchiveDelete, archive, &[]);
    h(O::SpecDelete, spec, &[]);
    result
}

/// `EECampaignFormManager::UpdateChecksum`: the chosen campaign file's checksum ("Campaigns\<name><extension>" on
/// the data path; 0 when it is missing).
pub fn update_checksum(manager: P) {
    let name = cat(&[&wide_text(h(O::FormCampaignName, manager, &[]) as P), &text(O::CampaignExtension, 0, &[])]);
    let checksum = match find_data(&cat(&[b"Campaigns\\", &name])) {
        Some(spec) => {
            let (empty, name) = (C::new(b""), C::new(&name));
            h(O::SpecSetFile, spec, &[empty.a()]);
            let checksum = h(O::FileChecksum, spec, &[name.a()]);
            h(O::SpecDelete, spec, &[]);
            checksum
        }
        None => 0,
    };
    h(O::FormSetChecksum, manager, &[checksum]);
}

/// The built-in campaigns (the three AoC campaigns, the demo, the tutorial: `BuiltInName`'s indices).
const BUILT_IN_CAMPAIGNS: i64 = 5;

/// `EECampaignFormManager::GetBuiltInCampaign`: the expected checksum of a built-in campaign (the last that matches,
/// ignoring case), else 0.
pub fn built_in_campaign(name: &[u8]) -> c_ulong {
    let name = C::new(name);
    let mut result: c_ulong = 0;
    for index in 0..BUILT_IN_CAMPAIGNS {
        let built_in = C::new(&text(O::BuiltInName, 0, &[index]));
        if h(O::MbsICmp, 0, &[name.a(), built_in.a()]) == 0 {
            result = h(O::BuiltInChecksum, 0, &[index]) as c_ulong;
        }
    }
    result
}

/// # Safety
/// `manager` is a live `EECampaignFormManager`.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_update_checksum(manager: *mut c_void) {
    update_checksum(manager as P)
}

/// # Safety
/// `name` is a C string or null.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_built_in(name: *const c_char) -> c_ulong {
    // SAFETY: the caller's guarantee
    built_in_campaign(unsafe { bytes(name) })
}

/// `EECampaign::Save` (`campaign` the C++ object, `error` its `UWideString&`).
///
/// # Safety
/// The objects are live.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_save(campaign: *mut c_void, error: *mut c_void) -> bool {
    campaign_save(campaign as P, error as P)
}

/// `EECampaign::ListScenarioFiles` (`files` a `std::vector<UWideString>*`, `error` a `UWideString*`).
///
/// # Safety
/// The objects are live.
#[no_mangle]
pub unsafe extern "C" fn ee_campaign_list_scenario_files(
    campaign: *mut c_void,
    files: *mut c_void,
    error: *mut c_void,
    add: bool,
    prefixed: bool,
) -> bool {
    list_scenario_files(campaign as P, files as P, error as P, add, prefixed)
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
    #[test]
    fn a_sound_is_found_as_named_or_with_an_extension() {
        let have = |names: &'static [&'static [u8]]| move |file: &[u8]| names.contains(&file);
        assert_eq!(sound_file(b"Sounds/a.WAV", &mut have(&[b"Sounds/a.WAV"])), b"Sounds/a.WAV");
        assert_eq!(sound_file(b"Sounds/a.mp3", &mut have(&[])), b"");
        assert_eq!(sound_file(b"Sounds/a", &mut have(&[b"Sounds/a.wav", b"Sounds/a.mp3"])), b"Sounds/a.wav");
        assert_eq!(sound_file(b"Sounds/a", &mut have(&[b"Sounds/a.mp3"])), b"Sounds/a.mp3");
        assert_eq!(sound_file(b"Sounds/a", &mut have(&[])), b"");
        // four characters or fewer never count as an extension
        assert_eq!(sound_file(b".wav", &mut have(&[b".wav.wav"])), b".wav.wav");
    }

    #[test]
    fn the_prefix_goes_where_the_name_starts() {
        assert_eq!(prefixed_at(b"Sounds/a.wav", b"a", b"Pre_"), b"Sounds/Pre_a.wav");
        assert_eq!(prefixed_at(b"Sounds/b.wav", b"x", b"Pre_"), b"Sounds/b.wav");
        assert_eq!(find(b"abcabc", b"c"), 2);
        assert_eq!(find(b"abc", b"d"), 3);
    }

    #[test]
    fn a_password_survives_its_encryption() {
        for password in [&b"secret"[..], b"a", b"Pass word 123"] {
            let encrypted = encrypt_password(password);
            assert_ne!(encrypted, password);
            assert_eq!(decrypt_password(&encrypted), password);
        }
    }

    #[test]
    fn lower_folds_as_platmbslwr() {
        assert_eq!(lower(b"Scenarios/ABC.Scn"), b"scenarios/abc.scn");
    }

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
