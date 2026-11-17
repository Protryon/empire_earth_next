//! The campaign editor's model whole (the original's `EECampaignEditorBehavior`, `Campaign/EECampaignEditorBehavior.cpp`,
//! 0x28d82d..0x28f0cb): the campaigns on disk, the scenarios not in the campaign, the entered password and the
//! new/edited flags, over the edited campaign (`EECampaign`, which stays C++: its file format) and the files, both
//! behind [`CampaignModelHost`] (`UI Events/EEScreens.cpp`'s `ee_screenscpp_ce_campaign`).
//!
//! The editor's events (`campaign_editor.rs`) reach it through the ops they always used (`EE_SCREENS_GAME_CE_*`,
//! `EE_SCREENS_TEXT_CE_*`): [`CampaignModel::op`] answers them. Texts are the game's UTF-8 bytes; the scenario lists
//! are sorted in byte order (`UWideString`'s `operator<` is `PlatMbsCmp`), names compare bytewise (`operator==`).

use crate::screens::campaign_editor::*;

/// The host's calls (`EE_SCREENS_CEM_*`): the edited campaign's fields and files, the strings, the disk.
pub const EE_SCREENS_CEM_STRING: u32 = 0; // (id) -> GetString's result; the text kept
pub const EE_SCREENS_CEM_SET_FIELD: u32 = 1; // (field) text: the campaign's setter
pub const EE_SCREENS_CEM_SET_SCENARIOS: u32 = 2; // text (each name ended by 0): SetCampaignScenarios
pub const EE_SCREENS_CEM_LOAD: u32 = 3; // text: the campaign's Load("Campaigns\" + name, error, true); the error kept
pub const EE_SCREENS_CEM_SAVE: u32 = 4; // -> the campaign's Save(error); the error kept
pub const EE_SCREENS_CEM_LIST: u32 = 5; // -> ListScenarioFiles(lines, error, false, false); both kept
pub const EE_SCREENS_CEM_DELETE: u32 = 6; // text: deletes data\Campaigns\<text> -> 1 when it worked
pub const EE_SCREENS_CEM_RESET_BEGIN: u32 = 7; // Reset's start: the campaign's directory, the two paths
pub const EE_SCREENS_CEM_LIST_FILES: u32 = 8; // (which) the campaigns' or scenarios' files into the kept set
pub const EE_SCREENS_CEM_BUILT_IN: u32 = 9; // text -> EECampaignFormManager::GetBuiltInCampaign
pub const EE_SCREENS_CEM_CHECK: u32 = 10; // (warned) text: the archived copy's sum against the file's; -> 1 warned now
pub const EE_SCREENS_CEM_RESET_END: u32 = 11; // Reset's end: the paths and files released
/// The host's texts (`ee_screenscpp_ce_campaign_text`).
pub const EE_SCREENS_CEM_TEXT_FIELD: u32 = 0; // (field) the campaign's getter (the password: the campaign's)
pub const EE_SCREENS_CEM_TEXT_SCENARIOS: u32 = 1; // the campaign's scenarios, each ended by 0
pub const EE_SCREENS_CEM_TEXT_KEPT: u32 = 2; // the last string or error
pub const EE_SCREENS_CEM_TEXT_LINES: u32 = 3; // the last scenario list's lines, each ended by 0
pub const EE_SCREENS_CEM_TEXT_FILES: u32 = 4; // the kept file set, each ended by 0
pub const EE_SCREENS_CEM_TEXT_EXTENSION: u32 = 5; // (which) gCampaignExtension, gScenarioExtension

/// [`EE_SCREENS_CEM_LIST_FILES`]' and [`EE_SCREENS_CEM_TEXT_EXTENSION`]'s directories.
pub const EE_SCREENS_CEM_CAMPAIGNS: u32 = 0;
pub const EE_SCREENS_CEM_SCENARIOS: u32 = 1;

// the strings (ee/EEStringIDs.h)
const IDS_CAMPAIGN_MUST_HAVE_AT_LEAST: u32 = 7114;
const IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN: u32 = 7120;
const IDS_EECAMPAIGN: u32 = 7121;
const IDS_SETUP_NONE_PROVIDED: u32 = 7122;
const IDS_SETUP_NONE_PROVIDED_1BD3: u32 = 7123;
const IDS_NO_DESCRIPTION_PROVIDED: u32 = 7124;
const IDS_CAMPAIGN_COULDNT_BE_DELETED: u32 = 7127;

/// The edited campaign and the disk, as the model reaches them.
pub trait CampaignModelHost {
    /// `EE_SCREENS_CEM_*` with an argument and a text; its answer.
    fn call(&mut self, op: u32, a: u32, text: &[u8]) -> u32;
    /// `EE_SCREENS_CEM_TEXT_*`: a pure read.
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
}

/// The texts of a list ended each by a 0 byte.
pub fn split_lines(bytes: &[u8]) -> Vec<Vec<u8>> {
    let mut lines = Vec::new();
    let mut start = 0;
    for (i, &b) in bytes.iter().enumerate() {
        if b == 0 {
            lines.push(bytes[start..i].to_vec());
            start = i + 1;
        }
    }
    lines
}

/// The lines, each ended by a 0 byte.
pub fn join_lines(lines: &[Vec<u8>]) -> Vec<u8> {
    let mut out = Vec::new();
    for line in lines {
        out.extend_from_slice(line);
        out.push(0);
    }
    out
}

/// The text before its first 0 byte (a C string's view: the comparisons and the length stop there).
fn c_view(text: &[u8]) -> &[u8] {
    text.iter().position(|&b| b == 0).map_or(text, |n| &text[..n])
}

/// `UWideString ==`: `PlatMbsCmp` == 0.
fn same(a: &[u8], b: &[u8]) -> bool {
    c_view(a) == c_view(b)
}

/// The characters of a text (`UWideString::GetLength`: `PlatMbsLen`).
fn characters(text: &[u8]) -> usize {
    let mut z = c_view(text).to_vec();
    z.push(0);
    unsafe { ee_base::plat_core::string::PlatMbsLen(z.as_ptr()) }
}

/// `GetIndexedSubstring(first, last)`: from character `first`, `last - first + 1` characters.
fn indexed(text: &[u8], first: usize, last: usize) -> Vec<u8> {
    let mut z = c_view(text).to_vec();
    z.push(0);
    unsafe {
        let start = ee_base::plat_core::string::PlatMbsNInc(z.as_ptr(), first);
        let end = ee_base::plat_core::string::PlatMbsNInc(start, last.wrapping_sub(first).wrapping_add(1));
        let from = start.offset_from(z.as_ptr()) as usize;
        let to = end.offset_from(z.as_ptr()) as usize;
        z[from..to].to_vec()
    }
}

/// `UWideString::ToLower` (`PlatMbsLwr`).
fn lower(text: &[u8]) -> Vec<u8> {
    let mut z = c_view(text).to_vec();
    z.push(0);
    unsafe { ee_base::plat_core::string::PlatMbsLwr(z.as_mut_ptr()) };
    z.pop();
    z
}

/// A listed file's name without its extension when it has that extension (Reset's: longer than four characters, the
/// last four lowered equal it).
pub fn strip_extension(name: &[u8], extension: &[u8]) -> Vec<u8> {
    let length = characters(name);
    if length > 4 && same(&lower(&indexed(name, length - 4, length - 1)), extension) {
        return indexed(name, 0, length - 5);
    }
    name.to_vec()
}

/// `std::sort` of the scenario names (`operator<`: byte order; the names are distinct).
fn sort(lines: &mut [Vec<u8>]) {
    lines.sort_by(|a, b| c_view(a).cmp(c_view(b)));
}

/// The model's state (`EECampaignEditorBehavior`'s members but the campaign, plus the host's kept results of the
/// editor's last list and error).
#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub struct CampaignModel {
    /// The user campaigns on disk (`mScenarios`).
    pub campaigns: Vec<Vec<u8>>,
    /// The scenarios not in the campaign, sorted (`mAvailableScenarios`).
    pub available: Vec<Vec<u8>>,
    /// The entered password (`mPassword`).
    pub password: Vec<u8>,
    /// A new campaign (`mIsNew`).
    pub is_new: bool,
    /// Edited since the last load or save (`mPasswordEntered`).
    pub password_entered: bool,
    /// The last Save, Load, Delete or scenario list's error (the host's `sCampaignEditorError`).
    pub error: Vec<u8>,
    /// The last scenario list's result.
    pub list_ok: bool,
}

impl CampaignModel {
    fn scenarios<H: CampaignModelHost + ?Sized>(h: &mut H) -> Vec<Vec<u8>> {
        split_lines(&h.text(EE_SCREENS_CEM_TEXT_SCENARIOS, 0))
    }
    fn set_scenarios<H: CampaignModelHost + ?Sized>(h: &mut H, lines: &[Vec<u8>]) {
        h.call(EE_SCREENS_CEM_SET_SCENARIOS, 0, &join_lines(lines));
    }
    /// `gResource->GetString(id, text)`: its result and the text.
    fn string<H: CampaignModelHost + ?Sized>(h: &mut H, id: u32) -> (i32, Vec<u8>) {
        let r = h.call(EE_SCREENS_CEM_STRING, id, &[]) as i32;
        (r, h.text(EE_SCREENS_CEM_TEXT_KEPT, 0))
    }
    fn field<H: CampaignModelHost + ?Sized>(h: &mut H, which: u32) -> Vec<u8> {
        h.text(EE_SCREENS_CEM_TEXT_FIELD, which)
    }

    /// `AddScenario`: an available scenario to the end of the campaign's list; whether it was available.
    pub fn add_scenario<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, scenario: &[u8]) -> bool {
        let Some(i) = self.available.iter().position(|s| same(s, scenario)) else {
            return false;
        };
        self.available.remove(i);
        let mut scenarios = Self::scenarios(h);
        scenarios.push(scenario.to_vec());
        Self::set_scenarios(h, &scenarios);
        self.password_entered = true;
        true
    }

    /// `MoveScenario`: a campaign scenario by `delta` places (the original's position arithmetic).
    pub fn move_scenario<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, scenario: &[u8], delta: i32) -> bool {
        let mut scenarios = Self::scenarios(h);
        let mut found = false;
        let mut index: i32 = 0;
        for s in &scenarios {
            if found {
                break;
            }
            if same(s, scenario) {
                found = true;
            }
            index = index.wrapping_add(1);
        }
        if !found {
            return false;
        }
        let mut position = index.wrapping_add(delta).wrapping_sub(1);
        if position < 0 || position >= scenarios.len() as i32 {
            return false;
        }
        let at = scenarios.iter().position(|s| same(s, scenario)).unwrap_or(0);
        scenarios.remove(at);
        if position > index {
            position -= 1;
        }
        scenarios.insert(position as usize, scenario.to_vec());
        Self::set_scenarios(h, &scenarios);
        self.password_entered = true;
        true
    }

    /// `RemoveScenario`: a campaign scenario back to the sorted available list.
    pub fn remove_scenario<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, scenario: &[u8]) -> bool {
        let mut scenarios = Self::scenarios(h);
        let Some(i) = scenarios.iter().position(|s| same(s, scenario)) else {
            return false;
        };
        scenarios.remove(i);
        Self::set_scenarios(h, &scenarios);
        self.available.push(scenario.to_vec());
        sort(&mut self.available);
        self.password_entered = true;
        true
    }

    /// `New`: the default texts, no scenarios, no password; the first string error.
    pub fn new_campaign<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H) -> i32 {
        let steps = [
            (IDS_EECAMPAIGN, EE_SCREENS_CE_FIELD_NAME),
            (IDS_SETUP_NONE_PROVIDED, EE_SCREENS_CE_FIELD_EMAIL),
            (IDS_SETUP_NONE_PROVIDED_1BD3, EE_SCREENS_CE_FIELD_AUTHOR),
            (IDS_NO_DESCRIPTION_PROVIDED, EE_SCREENS_CE_FIELD_DESCRIPTION),
        ];
        for (id, which) in steps {
            let (r, text) = Self::string(h, id);
            if r != 0 {
                return r;
            }
            h.call(EE_SCREENS_CEM_SET_FIELD, which, &text);
        }
        for scenario in Self::scenarios(h) {
            self.remove_scenario(h, &scenario);
        }
        self.password.clear();
        self.password_entered = false;
        self.is_new = true;
        0
    }

    /// `Delete`: a campaign file from the disk and the list; the error when it fails.
    pub fn delete<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, name: &[u8]) -> bool {
        if h.call(EE_SCREENS_CEM_DELETE, 0, name) == 0 {
            self.error = Self::string(h, IDS_CAMPAIGN_COULDNT_BE_DELETED).1;
            return false;
        }
        let length = characters(name);
        let name = if length > 4 { indexed(name, 0, length - 5) } else { c_view(name).to_vec() };
        if let Some(i) = self.campaigns.iter().position(|s| same(s, &name)) {
            self.campaigns.remove(i);
        }
        self.password_entered = false;
        true
    }

    /// `GetIsUnlocked`: new, no password, or the entered one matches.
    pub fn unlocked<H: CampaignModelHost + ?Sized>(&self, h: &mut H) -> bool {
        let password = Self::field(h, EE_SCREENS_CE_FIELD_PASSWORD);
        self.is_new || same(&password, b"") || same(&self.password, &password)
    }

    /// `Reset`: the user campaigns and the scenarios not in the campaign (sorted); a campaign scenario whose archived
    /// copy differs from the file warns once.
    pub fn reset<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H) -> bool {
        h.call(EE_SCREENS_CEM_RESET_BEGIN, 0, &[]);
        h.call(EE_SCREENS_CEM_LIST_FILES, EE_SCREENS_CEM_CAMPAIGNS, &[]);
        let files = split_lines(&h.text(EE_SCREENS_CEM_TEXT_FILES, 0));
        let extension = h.text(EE_SCREENS_CEM_TEXT_EXTENSION, EE_SCREENS_CEM_CAMPAIGNS);
        self.campaigns.clear();
        for file in &files {
            let name = strip_extension(file, &extension);
            if h.call(EE_SCREENS_CEM_BUILT_IN, 0, &name) == 0 {
                self.campaigns.push(name);
            }
        }
        // (the original lists the scenarios into the same set: the campaigns' files come round again)
        h.call(EE_SCREENS_CEM_LIST_FILES, EE_SCREENS_CEM_SCENARIOS, &[]);
        let files = split_lines(&h.text(EE_SCREENS_CEM_TEXT_FILES, 0));
        let extension = h.text(EE_SCREENS_CEM_TEXT_EXTENSION, EE_SCREENS_CEM_SCENARIOS);
        self.available.clear();
        let mut warned = false;
        for file in &files {
            let name = strip_extension(file, &extension);
            if !Self::scenarios(h).iter().any(|s| same(s, &name)) {
                self.available.push(name);
                sort(&mut self.available);
            } else if h.call(EE_SCREENS_CEM_CHECK, warned as u32, &name) != 0 {
                warned = true;
            }
        }
        h.call(EE_SCREENS_CEM_RESET_END, 0, &[]);
        true
    }

    /// `ValidateName`, `ValidateOther` (any) and `ValidateScenarios`: the first error.
    fn validate<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H) -> bool {
        if same(&Self::field(h, EE_SCREENS_CE_FIELD_NAME), b"") {
            self.error = Self::string(h, IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN).1;
            return false;
        }
        if Self::scenarios(h).is_empty() {
            self.error = Self::string(h, IDS_CAMPAIGN_MUST_HAVE_AT_LEAST).1;
            return false;
        }
        true
    }

    /// `Save`: validated, the entered password set, saved, the lists rescanned.
    pub fn save<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H) -> bool {
        self.error.clear();
        if !self.validate(h) {
            return false;
        }
        let password = self.password.clone();
        h.call(EE_SCREENS_CEM_SET_FIELD, EE_SCREENS_CE_FIELD_PASSWORD, &password);
        let saved = h.call(EE_SCREENS_CEM_SAVE, 0, &[]) != 0;
        self.error = h.text(EE_SCREENS_CEM_TEXT_KEPT, 0);
        if !saved {
            return false;
        }
        let r = self.reset(h);
        self.password_entered = false;
        r
    }

    /// `Load`: a campaign by name, the lists rescanned, locked until its password is entered.
    pub fn load<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, name: &[u8]) -> bool {
        self.error.clear();
        let loaded = h.call(EE_SCREENS_CEM_LOAD, 0, name) != 0;
        self.error = h.text(EE_SCREENS_CEM_TEXT_KEPT, 0);
        if !loaded {
            return false;
        }
        let r = self.reset(h);
        self.password_entered = false;
        self.is_new = false;
        self.password.clear();
        r
    }

    /// The editor's op as `campaign_editor.rs` issues it, answered here: `Some(answer)` for a call or put,
    /// `None` for an op the model does not own.
    pub fn op<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, op: u32) -> Option<u32> {
        Some(match op {
            EE_SCREENS_GAME_CE_RESET => {
                self.reset(h);
                0
            }
            EE_SCREENS_GAME_CE_NEW => self.new_campaign(h) as u32,
            EE_SCREENS_GAME_CE_UNLOCKED => self.unlocked(h) as u32,
            EE_SCREENS_GAME_CE_PASSWORD_ENTERED => self.password_entered as u32,
            EE_SCREENS_GAME_CE_SAVE => self.save(h) as u32,
            EE_SCREENS_GAME_CE_LIST_OK => self.list_ok as u32,
            _ => return None,
        })
    }

    /// The model's texts (`EE_SCREENS_TEXT_CE_FIELD`, `_LIST`, `_ERROR`); `None` for another.
    pub fn text<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, which: u32, arg: u32) -> Option<Vec<u8>> {
        Some(match which {
            EE_SCREENS_TEXT_CE_FIELD if arg >= EE_SCREENS_CE_FIELD_PASSWORD => self.password.clone(),
            EE_SCREENS_TEXT_CE_FIELD => Self::field(h, arg),
            EE_SCREENS_TEXT_CE_LIST => match arg {
                EE_SCREENS_CE_LIST_CAMPAIGNS => join_lines(&self.campaigns),
                EE_SCREENS_CE_LIST_AVAILABLE => join_lines(&self.available),
                EE_SCREENS_CE_LIST_SCENARIOS => join_lines(&Self::scenarios(h)),
                _ => {
                    self.list_ok = h.call(EE_SCREENS_CEM_LIST, 0, &[]) != 0;
                    self.error = h.text(EE_SCREENS_CEM_TEXT_KEPT, 0);
                    h.text(EE_SCREENS_CEM_TEXT_LINES, 0)
                }
            },
            EE_SCREENS_TEXT_CE_ERROR => self.error.clone(),
            _ => return None,
        })
    }

    /// The model's puts (`EE_SCREENS_TEXT_CE_SET_FIELD`, `_ADD`, `_REMOVE`, `_MOVE`, `_DELETE`, `_LOAD`); `None` for
    /// another.
    pub fn put<H: CampaignModelHost + ?Sized>(&mut self, h: &mut H, which: u32, arg: u32, text: &[u8]) -> Option<u32> {
        Some(match which {
            EE_SCREENS_TEXT_CE_SET_FIELD if arg >= EE_SCREENS_CE_FIELD_PASSWORD => {
                self.password = c_view(text).to_vec();
                self.password_entered = true;
                0
            }
            EE_SCREENS_TEXT_CE_SET_FIELD => {
                h.call(EE_SCREENS_CEM_SET_FIELD, arg, text);
                0
            }
            EE_SCREENS_TEXT_CE_ADD => self.add_scenario(h, text) as u32,
            EE_SCREENS_TEXT_CE_REMOVE => self.remove_scenario(h, text) as u32,
            EE_SCREENS_TEXT_CE_MOVE => self.move_scenario(h, text, arg as i32) as u32,
            EE_SCREENS_TEXT_CE_DELETE => {
                self.error.clear();
                self.delete(h, text) as u32
            }
            EE_SCREENS_TEXT_CE_LOAD => self.load(h, text) as u32,
            _ => return None,
        })
    }
}

#[cfg(test)]
mod tests;
