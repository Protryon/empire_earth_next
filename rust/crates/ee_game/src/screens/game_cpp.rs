//! The one-statement C++ shims the UI lane's moved host cases call (UI lane round 7: "calling C++ from Rust is
//! fine"). Each shim is an `extern "C"` function of ONE statement in the host's own C++ file (`ee_screenscpp_*`, the
//! lane's files): a call on a C++ object of another lane (a form's backgrounds, a texture, the communications, the
//! campaign managers, the file specs), no logic. The logic that called them is Rust's (`game_calls.rs` and the
//! other `*_calls.rs` modules), generic over [`Cpp`] so that its tests run on [`Fake`], which logs every call.
//!
//! [`shims!`] makes, from one list, the trait, the FFI implementation ([`Ffi`], absent from a test binary) and the
//! fake. A shim's arguments are plain numbers, pointers (`usize`), [`EeScreensShimText`] (bytes handed in) and
//! [`EeScreensShimOut`] (a text the shim appends to with `ee_screens_shim_out`).

use core::ffi::c_void;
use core::fmt;

/// Bytes handed to a shim (valid for the call): `ee/EEScreensShims.h`'s `EeScreensShimText`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeScreensShimText {
    pub ptr: *const u8,
    pub len: usize,
}

impl EeScreensShimText {
    pub fn of(bytes: &[u8]) -> Self {
        EeScreensShimText { ptr: bytes.as_ptr(), len: bytes.len() }
    }
}

impl fmt::Debug for EeScreensShimText {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // SAFETY: a text lives for the call it is handed to (`of`'s slice)
        let bytes = if self.len == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(self.ptr, self.len) } };
        write!(f, "{:?}", String::from_utf8_lossy(bytes))
    }
}

/// A text a shim writes: the `Vec<u8>` the shim's `ee_screens_shim_out` calls append to.
#[repr(transparent)]
#[derive(Clone, Copy)]
pub struct EeScreensShimOut(pub *mut c_void);

impl EeScreensShimOut {
    pub fn of(out: &mut Vec<u8>) -> Self {
        EeScreensShimOut(out as *mut Vec<u8> as *mut c_void)
    }
}

impl fmt::Debug for EeScreensShimOut {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("out")
    }
}

/// A number a shim writes (an out argument of the call it makes: `unsigned long&`, `FSFileSpec*&`, `int32_t*`).
macro_rules! out_types {
    ($($(#[$m:meta])* $name:ident: $t:ty),*) => {$(
        $(#[$m])*
        #[repr(transparent)]
        #[derive(Clone, Copy)]
        pub struct $name(pub *mut $t);

        impl $name {
            pub fn of(out: &mut $t) -> Self {
                $name(out)
            }
        }

        impl fmt::Debug for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.write_str("out")
            }
        }

        impl ShimArg for $name {
            #[cfg(test)]
            fn fake(self, name: &'static str, fake: &mut Fake) {
                let v = fake.outs.get_mut(name).and_then(|q| if q.is_empty() { None } else { Some(q.remove(0)) });
                // SAFETY: the out number of the call
                unsafe { *self.0 = v.unwrap_or(0) as $t };
            }
        }
    )*};
}
out_types!(
    /// An `unsigned long&`.
    EeScreensShimULongOut: core::ffi::c_ulong,
    /// A pointer out (`FSFileSpec*&`, as an address).
    EeScreensShimPtrOut: usize,
    /// An `int32_t*`.
    EeScreensShimI32Out: i32,
    /// A `long&`.
    EeScreensShimLongOut: core::ffi::c_long
);

/// Appends `length` bytes at `text` to the shim's out text (`EeScreensShimOut`).
///
/// # Safety
/// `out` is an `EeScreensShimOut` of a live `Vec<u8>`; `text` holds `length` bytes (or `length` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shim_out(out: *mut c_void, text: *const u8, length: usize) {
    if out.is_null() || text.is_null() || length == 0 {
        return;
    }
    // SAFETY: the contract
    unsafe { (*(out as *mut Vec<u8>)).extend_from_slice(core::slice::from_raw_parts(text, length)) };
}

/// A shim's argument type.
pub(crate) trait ShimArg: Copy + fmt::Debug {
    /// The fake's part of the call (an out text gets the fake's next text for the shim).
    #[cfg(test)]
    fn fake(self, _name: &'static str, _fake: &mut Fake) {}
}

macro_rules! plain_args {
    ($($t:ty),*) => { $(impl ShimArg for $t {})* };
}
plain_args!(u8, u32, i32, u64, i64, usize, isize, f32, bool, EeScreensShimText);

impl ShimArg for EeScreensShimOut {
    #[cfg(test)]
    fn fake(self, name: &'static str, fake: &mut Fake) {
        let text = fake.texts.get_mut(name).and_then(|q| if q.is_empty() { None } else { Some(q.remove(0)) });
        // SAFETY: the out text of the call (a live Vec<u8>)
        unsafe { (*(self.0 as *mut Vec<u8>)).extend_from_slice(&text.unwrap_or_default()) };
    }
}

/// A shim's answer type (the fake's answers are i64s).
pub(crate) trait ShimRet {
    fn from_fake(v: i64) -> Self;
}

macro_rules! rets {
    ($($t:ty),*) => { $(impl ShimRet for $t { fn from_fake(v: i64) -> Self { v as $t } })* };
}
rets!(u8, u32, i32, u64, i64, usize, isize, f32);

impl ShimRet for () {
    fn from_fake(_: i64) -> Self {}
}

impl ShimRet for bool {
    fn from_fake(v: i64) -> Self {
        v != 0
    }
}

/// The recording fake: every call logged as `name(args)`; answers and out texts from per-shim queues (0 and an
/// empty text once a queue is empty).
#[cfg(test)]
#[derive(Default)]
pub(crate) struct Fake {
    pub log: Vec<String>,
    pub answers: std::collections::BTreeMap<&'static str, Vec<i64>>,
    pub texts: std::collections::BTreeMap<&'static str, Vec<Vec<u8>>>,
    pub outs: std::collections::BTreeMap<&'static str, Vec<i64>>,
}

#[cfg(test)]
impl Fake {
    /// The shim `name` answers `values`, one per call.
    pub fn answer(&mut self, name: &'static str, values: &[i64]) -> &mut Self {
        self.answers.entry(name).or_default().extend_from_slice(values);
        self
    }
    /// The shim `name`'s out text is `text` (one per call).
    pub fn text(&mut self, name: &'static str, text: &[u8]) -> &mut Self {
        self.texts.entry(name).or_default().push(text.to_vec());
        self
    }
    /// The shim `name`'s out number is `value` (one per call).
    pub fn out(&mut self, name: &'static str, value: i64) -> &mut Self {
        self.outs.entry(name).or_default().push(value);
        self
    }
    fn next(&mut self, name: &'static str) -> i64 {
        match self.answers.get_mut(name) {
            Some(q) if !q.is_empty() => q.remove(0),
            _ => 0,
        }
    }
    /// The log, each call on its own line.
    pub fn calls(&self) -> String {
        self.log.join("\n")
    }
}

/// The FFI implementation of [`Cpp`]: the shims themselves.
#[cfg(not(test))]
pub(crate) struct Ffi;

macro_rules! shims {
    ($( fn $name:ident ( $($arg:ident : $ty:ty),* ) $(-> $ret:ty)? = $sym:ident ; )*) => {
        /// The C++ shims (one statement each).
        #[allow(clippy::too_many_arguments)]
        pub(crate) trait Cpp {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)?; )*
        }

        #[cfg(not(test))]
        mod sys {
            #[allow(unused_imports)]
            use super::*;
            extern "C" {
                $( pub fn $sym($($arg: $ty),*) $(-> $ret)?; )*
            }
        }

        #[cfg(not(test))]
        impl Cpp for Ffi {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)? {
                // SAFETY: the shim's own contract: the pointers the moved logic hands it are the C++ host's live
                // objects, as the C++ case had them
                unsafe { sys::$sym($($arg),*) }
            } )*
        }

        #[cfg(test)]
        impl Cpp for Fake {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)? {
                let args: Vec<String> = vec![$(format!("{:?}", $arg)),*];
                $( ShimArg::fake($arg, stringify!($name), self); )*
                self.log.push(format!("{}({})", stringify!($name), args.join(", ")));
                ShimRet::from_fake(self.next(stringify!($name)))
            } )*
        }
    };
}

shims! {
    // EEScreens.cpp's (game_calls.rs)
    fn slider_event_new(form: usize, label: u32) -> usize = ee_screenscpp_slider_event_new;
    fn control_set_range_trigger(form: usize, control: u32, event: usize) = ee_screenscpp_control_set_range_trigger;
    fn slider_event_add_string(event: usize, text: EeScreensShimText) = ee_screenscpp_slider_event_add_string;
    fn event_dereference(event: usize) = ee_screenscpp_event_dereference;
    fn set_game_view(left: f32, top: f32, right: f32, bottom: f32) = ee_screenscpp_set_game_view;
    fn sound_object(id: u32) -> usize = ee_screenscpp_sound_object;
    fn sound_load_media(sound: usize) = ee_screenscpp_sound_load_media;
    fn sound_play(sound: usize) = ee_screenscpp_sound_play;
    fn sound_hot_key_alarm() -> usize = ee_screenscpp_sound_hot_key_alarm;
    fn game_window() -> usize = ee_screenscpp_game_window;
    fn game_window_os(window: usize) -> usize = ee_screenscpp_game_window_os;
    fn settings_window_mode() -> u32 = ee_screenscpp_settings_window_mode;
    fn window_set_fixed_size(window: usize, fixed: i32) = ee_screenscpp_window_set_fixed_size;
    fn window_set_fullscreen(window: usize, full: i32) = ee_screenscpp_window_set_fullscreen;
    fn display_logical_size(width: EeScreensShimI32Out, height: EeScreensShimI32Out) -> i32 = ee_screenscpp_display_logical_size;
    fn window_set_size(window: usize, width: i32, height: i32) = ee_screenscpp_window_set_size;
    fn civ_clear_upgrades() = ee_screenscpp_civ_clear_upgrades;
    fn civ_reset_points() = ee_screenscpp_civ_reset_points;
    fn civ_builder_points() -> i32 = ee_screenscpp_civ_builder_points;
    fn civ_set_points_and_max(points: i32) = ee_screenscpp_civ_set_points_and_max;
    fn open_civ_builder() = ee_screenscpp_open_civ_builder;
    fn lobby_options_write_lock() = ee_screenscpp_lobby_options_write_lock;
    fn options_from_last_game() = ee_screenscpp_options_from_last_game;
    fn screen_event_new(event: u32) -> usize = ee_screenscpp_screen_event_new;
    fn post_event(event: usize) = ee_screenscpp_post_event;
    fn server_dynamic() -> usize = ee_screenscpp_server_dynamic;
    fn server_paused(server: usize) -> bool = ee_screenscpp_server_paused;
    fn server_pause(server: usize) = ee_screenscpp_server_pause;
    fn post_pause_information(paused: bool) = ee_screenscpp_post_pause_information;
    fn intro_set_flag(which: u32, value: bool) = ee_screenscpp_intro_set_flag;
    fn intro_get_sound(spec: usize) = ee_screenscpp_intro_get_sound;
    fn spec_new() -> usize = ee_screenscpp_spec_new;
    fn spec_file(spec: usize, out: EeScreensShimOut) = ee_screenscpp_spec_file;
    fn spec_delete(spec: usize) = ee_screenscpp_spec_delete;
    fn sound_set_latency(milliseconds: u32) = ee_screenscpp_sound_set_latency;
    fn load_sound(file: EeScreensShimText, sample: EeScreensShimULongOut, id: EeScreensShimULongOut) = ee_screenscpp_load_sound;
    fn sound_register_one_shot(sample: u32, id: u32) = ee_screenscpp_sound_register_one_shot;
    fn form_background(form: usize, id: u32) -> usize = ee_screenscpp_form_background;
    fn intro_get_map(spec: usize) = ee_screenscpp_intro_get_map;
    fn background_texture_file(background: usize, out: EeScreensShimOut) = ee_screenscpp_background_texture_file;
    fn form_remove_background(form: usize, background: usize) = ee_screenscpp_form_remove_background;
    fn intro_open_map() -> bool = ee_screenscpp_intro_open_map;
    fn form_lock_background(form: usize) = ee_screenscpp_form_lock_background;
    fn map_background_new(id: u32, file: EeScreensShimText, description: usize, form: usize, priority: u32) -> usize = ee_screenscpp_map_background_new;
    fn background_open(background: usize) = ee_screenscpp_background_open;
    fn background_load(background: usize) = ee_screenscpp_background_load;
    fn form_unlock_background(form: usize) = ee_screenscpp_form_unlock_background;
    fn options_write_lock() = ee_screenscpp_options_write_lock;
    fn options_write_unlock() = ee_screenscpp_options_write_unlock;
    fn options_read_lock() = ee_screenscpp_options_read_lock;
    fn options_read_unlock() = ee_screenscpp_options_read_unlock;
    fn options_set_defaults() = ee_screenscpp_options_set_defaults;
    fn options_set_load_type(load_type: i32) = ee_screenscpp_options_set_load_type;
    fn options_campaign_difficulty() = ee_screenscpp_options_campaign_difficulty;
    fn options_campaign_name() = ee_screenscpp_options_campaign_name;
    fn options_campaign_checksum() = ee_screenscpp_options_campaign_checksum;
    fn options_campaign_scenario() = ee_screenscpp_options_campaign_scenario;
    fn options_campaign_first_map() = ee_screenscpp_options_campaign_first_map;
    fn load_scenario_name_from_intro() = ee_screenscpp_load_scenario_name_from_intro;
    fn options_map_type_from_intro() = ee_screenscpp_options_map_type_from_intro;
    fn options_set_multiplayer(multiplayer: bool) = ee_screenscpp_options_set_multiplayer;
    fn delete_communications() = ee_screenscpp_delete_communications;
    fn new_local_server() = ee_screenscpp_new_local_server;
    fn server_start_threads() = ee_screenscpp_server_start_threads;
    fn server_set_load_responses(value: bool) = ee_screenscpp_server_set_load_responses;
    fn server_stop_ping() = ee_screenscpp_server_stop_ping;
    fn server_set_load_state(value: bool) = ee_screenscpp_server_set_load_state;
    fn set_local_player_out(value: bool) = ee_screenscpp_set_local_player_out;
    fn send_scenario_start_loading() = ee_screenscpp_send_scenario_start_loading;
    fn campaign_background_image(out: EeScreensShimOut) = ee_screenscpp_campaign_background_image;
    fn scenario_prefix(out: EeScreensShimOut) = ee_screenscpp_scenario_prefix;
    fn data_find_file(file: EeScreensShimText, spec: EeScreensShimPtrOut) -> i32 = ee_screenscpp_data_find_file;
    fn campaign_set_difficulty(level: i32) = ee_screenscpp_campaign_set_difficulty;
    fn set_difficulty_level(level: i32) = ee_screenscpp_set_difficulty_level;
    fn set_difficulty_level2(level: i32) = ee_screenscpp_set_difficulty_level2;
    fn options_set_difficulty(level: i32) = ee_screenscpp_options_set_difficulty;
    fn combo_text_colour(form: usize, control: u32, red: f32, green: f32, blue: f32) = ee_screenscpp_combo_text_colour;
    fn label_text_colour(form: usize, control: u32, red: f32, green: f32, blue: f32) = ee_screenscpp_label_text_colour;
    fn remove_player_new(slot: u8) -> usize = ee_screenscpp_remove_player_new;
    fn post_action(action: usize) = ee_screenscpp_post_action;
    fn action_dereference(action: usize) = ee_screenscpp_action_dereference;
    fn won_net() -> usize = ee_screenscpp_won_net;
    fn won_clear_game_list() = ee_screenscpp_won_clear_game_list;
    fn won_rejoin_room(flag: bool) -> bool = ee_screenscpp_won_rejoin_room;
    fn won_leave_room() = ee_screenscpp_won_leave_room;
    fn won_info_reset() = ee_screenscpp_won_info_reset;
    fn lobby_start_time_now() = ee_screenscpp_lobby_start_time_now;
    fn server_allow_new_players(allow: bool) = ee_screenscpp_server_allow_new_players;
    fn server_set_game_state(state: u32) = ee_screenscpp_server_set_game_state;
    fn won_info_load() = ee_screenscpp_won_info_load;
    fn won_info_flags() -> u32 = ee_screenscpp_won_info_flags;
    fn won_info_set_flags(flags: u32) = ee_screenscpp_won_info_set_flags;
    fn won_info_set_players(players: u8) = ee_screenscpp_won_info_set_players;
    fn won_info_set_humans(humans: u8) = ee_screenscpp_won_info_set_humans;
    fn won_info_players() -> u8 = ee_screenscpp_won_info_players;
    fn won_info_humans() -> u8 = ee_screenscpp_won_info_humans;
    fn won_info_store() = ee_screenscpp_won_info_store;
    fn server_reset_xfer() = ee_screenscpp_server_reset_xfer;
    fn send_full_player(index: u32) = ee_screenscpp_send_full_player;
    fn find_generator(source: u32, line: EeScreensShimULongOut) -> i32 = ee_screenscpp_find_generator;
    fn lobby_boot() -> usize = ee_screenscpp_lobby_boot;
    fn server_refuse_boot(server: usize) = ee_screenscpp_server_refuse_boot;
    fn lobby_has_file() -> bool = ee_screenscpp_lobby_has_file;
    fn server_abort_xfer(id: u32) = ee_screenscpp_server_abort_xfer;
    fn server_xfer_lobby_file(slot: u8, id: EeScreensShimULongOut) = ee_screenscpp_server_xfer_lobby_file;
    fn physics_post(message: u32) = ee_screenscpp_physics_post;
    fn post_enter_scenario_editor() -> i32 = ee_screenscpp_post_enter_scenario_editor;
    fn user_pause_new() -> usize = ee_screenscpp_user_pause_new;
    fn form_music(form: u32, out: EeScreensShimOut) = ee_screenscpp_form_music;
    fn form_set_music(form: u32, text: EeScreensShimText) = ee_screenscpp_form_set_music;
    fn manager_pop() = ee_screenscpp_manager_pop;
    fn manager_swap(form: u32) = ee_screenscpp_manager_swap;
    fn save_main_game_form() = ee_screenscpp_save_main_game_form;
    // EEScreens.cpp's texts (game_texts.rs)
    fn pdcs_selection(selection: EeScreensShimULongOut) -> i32 = ee_screenscpp_pdcs_selection;
    fn pdcs_line(selection: u32, out: EeScreensShimOut) = ee_screenscpp_pdcs_line;
    fn display_rasterizer_count() -> u32 = ee_screenscpp_display_rasterizer_count;
    fn display_rasterizer_name(index: u32, out: EeScreensShimOut) = ee_screenscpp_display_rasterizer_name;
    fn display_screen_count(rasterizer: u32) -> u32 = ee_screenscpp_display_screen_count;
    fn display_screen_name(rasterizer: u32, screen: u32, out: EeScreensShimOut) = ee_screenscpp_display_screen_name;
    fn rasterizer_count() -> u32 = ee_screenscpp_rasterizer_count;
    fn rasterizer_name(index: u32, out: EeScreensShimOut) = ee_screenscpp_rasterizer_name;
    fn form_control(form: usize, control: u32) -> usize = ee_screenscpp_form_control;
    fn check_box_label(check: usize) -> usize = ee_screenscpp_check_box_label;
    fn label_text(label: usize, out: EeScreensShimOut) = ee_screenscpp_label_text;
    fn archive_new() -> usize = ee_screenscpp_archive_new;
    fn archive_set(archive: usize, spec: usize) = ee_screenscpp_archive_set;
    fn data_add_archive(archive: usize) = ee_screenscpp_data_add_archive;
    fn set_data_paths() = ee_screenscpp_set_data_paths;
    fn archive_delete(archive: usize) = ee_screenscpp_archive_delete;
    fn scenario_extension(out: EeScreensShimOut) = ee_screenscpp_scenario_extension;
    fn header_new() -> usize = ee_screenscpp_header_new;
    fn header_delete(header: usize) = ee_screenscpp_header_delete;
    fn header_load(header: usize, spec: usize) -> i32 = ee_screenscpp_header_load;
    fn header_set_major(header: usize) = ee_screenscpp_header_set_major;
    fn header_set_minor(header: usize) = ee_screenscpp_header_set_minor;
    fn campaign_update_checksum() = ee_screenscpp_campaign_update_checksum;
    fn intro_media_from_header(header: usize) = ee_screenscpp_intro_media_from_header;
    fn intro_prefix_media() = ee_screenscpp_intro_prefix_media;
    fn intro_set_file(spec: usize) = ee_screenscpp_intro_set_file;
    fn parse_cheat(text: EeScreensShimText, code: EeScreensShimLongOut, parameter: EeScreensShimLongOut) = ee_screenscpp_parse_cheat;
    fn send_cheat(code: i64, parameter: i64) = ee_screenscpp_send_cheat;
    fn button_font(form: usize, control: u32) -> usize = ee_screenscpp_button_font;
    fn button_set_text(form: usize, control: u32, text: EeScreensShimText, font: usize) = ee_screenscpp_button_set_text;
    fn last_auto_save(out: EeScreensShimOut) = ee_screenscpp_last_auto_save;
    fn last_quick_save(out: EeScreensShimOut) = ee_screenscpp_last_quick_save;
    fn open_save_game_file(name: EeScreensShimText, directory: EeScreensShimText, spec: EeScreensShimPtrOut) -> i32 = ee_screenscpp_open_save_game_file;
    fn spec_set_file(spec: usize, file: EeScreensShimText) = ee_screenscpp_spec_set_file;
    fn post_load_saved_game(spec: usize) = ee_screenscpp_post_load_saved_game;
    fn display_message(text: EeScreensShimText, flag: bool) = ee_screenscpp_display_message;
    fn manager_form_control(form: u32, control: u32) -> usize = ee_screenscpp_manager_form_control;
    fn label_delete_array(label: usize) = ee_screenscpp_label_delete_array;
    fn label_append_text(label: usize, text: EeScreensShimText) = ee_screenscpp_label_append_text;
    fn campaign_extension(out: EeScreensShimOut) = ee_screenscpp_campaign_extension;
    fn campaign_error_reset() = ee_screenscpp_campaign_error_reset;
    fn campaign_load(file: EeScreensShimText) -> bool = ee_screenscpp_campaign_load;
    fn send_connection_request(name: EeScreensShimText) = ee_screenscpp_send_connection_request;
    fn path_new() -> usize = ee_screenscpp_path_new;
    fn path_dereference(path: usize) = ee_screenscpp_path_dereference;
    fn spec_set_volume_startup(spec: usize) = ee_screenscpp_spec_set_volume_startup;
    fn spec_set_directory_startup(spec: usize, directory: EeScreensShimText) = ee_screenscpp_spec_set_directory_startup;
    fn path_add_directory(path: usize, spec: usize) = ee_screenscpp_path_add_directory;
    fn file_list_add_extension(list: usize, extension: EeScreensShimText) = ee_screenscpp_file_list_add_extension;
    fn file_list_set_paths(list: usize, path: usize) = ee_screenscpp_file_list_set_paths;
    fn file_list_create(list: usize) = ee_screenscpp_file_list_create;
    fn file_list_find(list: usize, name: EeScreensShimText, index: EeScreensShimULongOut) -> bool = ee_screenscpp_file_list_find;
    fn list_select(list: usize, index: u32) = ee_screenscpp_list_select;
    fn list_scroll_to(list: usize, index: u32) = ee_screenscpp_list_scroll_to;
    fn spec_open_read(spec: usize) -> i32 = ee_screenscpp_spec_open_read;
    fn spec_close(spec: usize) = ee_screenscpp_spec_close;
    fn shim_message_reset() = ee_screenscpp_shim_message_reset;
    fn validate_save_game(spec: usize) -> bool = ee_screenscpp_validate_save_game;
    fn display_shim_message() = ee_screenscpp_display_shim_message;
    fn lobby_options_write_unlock() = ee_screenscpp_lobby_options_write_unlock;
    fn lobby_options_set_defaults() = ee_screenscpp_lobby_options_set_defaults;
    fn lobby_options_set_load_type(load_type: i32) = ee_screenscpp_lobby_options_set_load_type;
    fn lobby_options_map_type_from_spec(spec: usize) = ee_screenscpp_lobby_options_map_type_from_spec;
    fn load_saved_game_name_from_spec(spec: usize) = ee_screenscpp_load_saved_game_name_from_spec;
    fn game_state_single() = ee_screenscpp_game_state_single;
    fn set_game_state_load_type(load_type: i32) = ee_screenscpp_set_game_state_load_type;
    fn load_scenario_name_from_spec(spec: usize) = ee_screenscpp_load_scenario_name_from_spec;
    fn lobby_header_delete() = ee_screenscpp_lobby_header_delete;
    fn lobby_header_new() = ee_screenscpp_lobby_header_new;
    fn lobby_file_clear() = ee_screenscpp_lobby_file_clear;
    fn lobby_load(file: EeScreensShimText, directory: EeScreensShimText, load_type: u32) -> u32 = ee_screenscpp_lobby_load;
    fn debug_sound_label(id: u32) -> usize = ee_screenscpp_debug_sound_label;
    fn label_set_text(label: usize, text: EeScreensShimText) = ee_screenscpp_label_set_text;
    fn pgs_send(host: EeScreensShimText, port: u32, payload: EeScreensShimText) = ee_screenscpp_pgs_send;
    fn pdcs_name_reset() = ee_screenscpp_pdcs_name_reset;
    fn pdcs_upgrades_clear() = ee_screenscpp_pdcs_upgrades_clear;
    fn pdcs_load(file: EeScreensShimText) -> u32 = ee_screenscpp_pdcs_load;
    // EEScreens.cpp's more calls (game_calls_more.rs)
    fn player_event_new() -> usize = ee_screenscpp_player_event_new;
    fn hot_key_unit_x(index: u32) -> f32 = ee_screenscpp_hot_key_unit_x;
    fn hot_key_unit_y(index: u32) -> f32 = ee_screenscpp_hot_key_unit_y;
    fn hot_key_unit_z(index: u32) -> f32 = ee_screenscpp_hot_key_unit_z;
    fn camera_set_changed() = ee_screenscpp_camera_set_changed;
    fn camera_set_location(x: f32, y: f32, z: f32, flag1: bool, flag2: bool) = ee_screenscpp_camera_set_location;
    fn mini_map_refresh() = ee_screenscpp_mini_map_refresh;
    fn set_camera_following(following: bool) = ee_screenscpp_set_camera_following;
    fn hot_key_new() -> usize = ee_screenscpp_hot_key_new;
    fn hot_key_set_key(key: usize, code: u32) = ee_screenscpp_hot_key_set_key;
    fn hot_key_set_just_pressed(key: usize, event: usize) = ee_screenscpp_hot_key_set_just_pressed;
    fn chat_set_enter_trigger(form: usize, event: usize) = ee_screenscpp_chat_set_enter_trigger;
    fn form_add_hot_key(form: usize, key: usize) = ee_screenscpp_form_add_hot_key;
    fn hot_key_delete(key: usize) = ee_screenscpp_hot_key_delete;
    fn ghosts_lock_enter() = ee_screenscpp_ghosts_lock_enter;
    fn world_clear_ghosts() = ee_screenscpp_world_clear_ghosts;
    fn ghosts_lock_leave() = ee_screenscpp_ghosts_lock_leave;
    fn spec_delete_file(spec: usize) -> i32 = ee_screenscpp_spec_delete_file;
    fn spec_create(spec: usize) = ee_screenscpp_spec_create;
    fn spec_fopen_append(spec: usize) -> i32 = ee_screenscpp_spec_fopen_append;
    fn add_chat_log(text: EeScreensShimText, value: i32) = ee_screenscpp_add_chat_log;
    fn world_dump_media(spec: usize, leaks: bool) = ee_screenscpp_world_dump_media;
    fn send_enter_shift() = ee_screenscpp_send_enter_shift;
    fn send_exit_shift() = ee_screenscpp_send_exit_shift;
    fn protocol_clear() = ee_screenscpp_protocol_clear;
    fn protocol_load() = ee_screenscpp_protocol_load;
    fn world_move_player_to_editor() = ee_screenscpp_world_move_player_to_editor;
    fn time_of_day(time: usize) = ee_screenscpp_time_of_day;
    fn saved_game_extension(out: EeScreensShimOut) = ee_screenscpp_saved_game_extension;
    fn settings_set_last_quick_save(name: EeScreensShimText) = ee_screenscpp_settings_set_last_quick_save;
    fn preferences_save() -> bool = ee_screenscpp_preferences_save;
    fn send_save_to_all(name: EeScreensShimText) = ee_screenscpp_send_save_to_all;
    fn current_player() -> usize = ee_screenscpp_current_player;
    fn lead_unit_id(player: u64) -> u64 = ee_screenscpp_lead_unit_id;
    fn world_unit(id: u64) -> usize = ee_screenscpp_world_unit;
    fn camera_follow(unit: usize, follow: bool) = ee_screenscpp_camera_follow;
    fn main_group_selected_unit() -> usize = ee_screenscpp_main_group_selected_unit;
    fn unit_has_complex(unit: usize) -> bool = ee_screenscpp_unit_has_complex;
    fn unit_has_class(unit: usize) -> bool = ee_screenscpp_unit_has_class;
    fn unit_class_command(unit: usize, index: u32) -> u32 = ee_screenscpp_unit_class_command;
    fn formation_new(kind: i32) -> usize = ee_screenscpp_formation_new;
    fn send_change_formation(kind: i32) = ee_screenscpp_send_change_formation;
    fn main_form_group_disable(group: u32) = ee_screenscpp_main_form_group_disable;
    fn main_form_background_visible(id: u32, visible: bool) = ee_screenscpp_main_form_background_visible;
    fn formation_set_b5c(formation: usize) = ee_screenscpp_formation_set_b5c;
    fn selected_group_set_formation(formation: usize) = ee_screenscpp_selected_group_set_formation;
    fn formation_dereference(formation: usize) = ee_screenscpp_formation_dereference;
    fn bitmap_set_corners(form: usize, control: u32, tlu: f32, tlv: f32, tru: f32, trv: f32, bru: f32, brv: f32, blu: f32, blv: f32) = ee_screenscpp_bitmap_set_corners;
    fn button_text_player_colour(form: usize, control: u32, player: u32) = ee_screenscpp_button_text_player_colour;
    fn label_text_player_colour(form: usize, control: u32, player: u32) = ee_screenscpp_label_text_player_colour;
    fn control_id(control: usize) -> u32 = ee_screenscpp_control_id;
    fn control_index(control: usize) -> u32 = ee_screenscpp_control_index;
    fn control_border(control: usize) -> u32 = ee_screenscpp_control_border;
    fn pgs_status_bar_new(id: u32, form: usize, index: u32, border: u32) -> usize = ee_screenscpp_pgs_status_bar_new;
    fn status_bar_clear_text(bar: usize) = ee_screenscpp_status_bar_clear_text;
    fn form_add_control(form: usize, control: usize, group: u32) = ee_screenscpp_form_add_control;
    fn control_string_id(form: usize, control: u32) -> u32 = ee_screenscpp_control_string_id;
    fn control_set_string_id(form: usize, control: u32, id: u32) = ee_screenscpp_control_set_string_id;
    fn bar_copy_text_style(form: usize, from: u32, to: u32) = ee_screenscpp_bar_copy_text_style;
    fn form_ui_bit_depth(form: usize) -> u32 = ee_screenscpp_form_ui_bit_depth;
    fn form_screen(form: usize) -> u32 = ee_screenscpp_form_screen;
    fn texture_load(name: EeScreensShimText, description: usize) -> usize = ee_screenscpp_texture_load;
    fn texture_alpha_modulate(texture: usize) = ee_screenscpp_texture_alpha_modulate;
    fn texture_color_modulate(texture: usize) = ee_screenscpp_texture_color_modulate;
    fn texture_min_filter(texture: usize, mode: u32) = ee_screenscpp_texture_min_filter;
    fn texture_mag_filter(texture: usize, mode: u32) = ee_screenscpp_texture_mag_filter;
    fn texture_mip_point(texture: usize) = ee_screenscpp_texture_mip_point;
    fn texture_mip_bias(texture: usize, bias: f32) = ee_screenscpp_texture_mip_bias;
    fn texture_clamp(texture: usize) = ee_screenscpp_texture_clamp;
    fn bitmap_new() -> usize = ee_screenscpp_bitmap_new;
    fn bitmap_set_texture(bitmap: usize, texture: usize, screen: u32) = ee_screenscpp_bitmap_set_texture;
    fn bitmap_stretch_to_control(bitmap: usize, form: usize, control: u32, width: f32, height: f32, screen: u32) = ee_screenscpp_bitmap_stretch_to_control;
    fn ui_bitmap_new(id: u32, bitmap: usize, form: usize, rectangle: u32, index: u32) -> usize = ee_screenscpp_ui_bitmap_new;
    fn bitmap_dereference(bitmap: usize) = ee_screenscpp_bitmap_dereference;
    fn spec_first_file(search: usize, pattern: EeScreensShimText, found: usize) -> i32 = ee_screenscpp_spec_first_file;
    fn spec_next_file(search: usize, found: usize) -> i32 = ee_screenscpp_spec_next_file;
    fn form_remove_control(form: usize, control: usize, flag: u32) = ee_screenscpp_form_remove_control;
    fn control_delete(control: usize) = ee_screenscpp_control_delete;
    fn pgs_campaign_first_from_civ() = ee_screenscpp_pgs_campaign_first_from_civ;
    fn pgs_campaign_description() = ee_screenscpp_pgs_campaign_description;
    fn pgs_campaign_is_demo() -> bool = ee_screenscpp_pgs_campaign_is_demo;
    fn pgs_campaign_is_learning() -> bool = ee_screenscpp_pgs_campaign_is_learning;
    fn pgs_campaign_set_all_open(open: bool) = ee_screenscpp_pgs_campaign_set_all_open;
    fn pgs_campaign_set_name(name: EeScreensShimText) = ee_screenscpp_pgs_campaign_set_name;
    fn pgs_campaign_set_first(name: EeScreensShimText) = ee_screenscpp_pgs_campaign_set_first;
    fn pgs_campaign_set_description(text: EeScreensShimText) = ee_screenscpp_pgs_campaign_set_description;
    fn lobby_options_read_lock() = ee_screenscpp_lobby_options_read_lock;
    fn lobby_options_read_unlock() = ee_screenscpp_lobby_options_read_unlock;
    fn background_left(background: usize) -> f32 = ee_screenscpp_background_left;
    fn background_top(background: usize) -> f32 = ee_screenscpp_background_top;
    fn background_right(background: usize) -> f32 = ee_screenscpp_background_right;
    fn background_set_rect(background: usize, left: f32, top: f32, right: f32, bottom: f32) = ee_screenscpp_background_set_rect;
    fn background_set_visible(background: usize, visible: bool) = ee_screenscpp_background_set_visible;
    fn cb_list_set_auto_select(list: usize, value: bool) = ee_screenscpp_cb_list_set_auto_select;
    fn cb_list_set_show_points(list: usize, value: bool) = ee_screenscpp_cb_list_set_show_points;
    fn cb_list_set_select_upgrades_only(list: usize, value: bool) = ee_screenscpp_cb_list_set_select_upgrades_only;
    fn cb_list_set_sort(list: usize, value: bool) = ee_screenscpp_cb_list_set_sort;
    fn cb_list_set_colour(list: usize, red: f32, green: f32, blue: f32) = ee_screenscpp_cb_list_set_colour;
    fn pdcs_civs_clear() = ee_screenscpp_pdcs_civs_clear;
    fn pdcs_civs_get() = ee_screenscpp_pdcs_civs_get;
    fn pdcs_civs_count() -> u32 = ee_screenscpp_pdcs_civs_count;
    fn civ_request_new() -> usize = ee_screenscpp_civ_request_new;
    fn comms_multiplayer() -> bool = ee_screenscpp_comms_multiplayer;
    fn send_to_server(message: usize) = ee_screenscpp_send_to_server;
    fn message_execute(message: usize) = ee_screenscpp_message_execute;
    fn message_dereference(message: usize) = ee_screenscpp_message_dereference;
    fn combo_add_extension(form: usize, control: u32, extension: EeScreensShimText) = ee_screenscpp_combo_add_extension;
    fn combo_set_paths(form: usize, control: u32, path: usize) = ee_screenscpp_combo_set_paths;
    fn combo_create(form: usize, control: u32) = ee_screenscpp_combo_create;
    fn combo_insert_line(form: usize, control: u32, text: EeScreensShimText, index: u32) = ee_screenscpp_combo_insert_line;
    fn combo_select(form: usize, control: u32, index: u32) = ee_screenscpp_combo_select;
    fn lobby_copy_exists() -> bool = ee_screenscpp_lobby_copy_exists;
    fn lobby_copy_new() = ee_screenscpp_lobby_copy_new;
    fn lobby_copy_from_options() = ee_screenscpp_lobby_copy_from_options;
    fn lobby_copy_clear_load_type() = ee_screenscpp_lobby_copy_clear_load_type;
    fn options_from_lobby_copy() = ee_screenscpp_options_from_lobby_copy;
    // EEUserInterface.cpp's (ui_calls.rs)
    fn gfx_post_pause() = ee_screenscpp_gfx_post_pause;
    fn gfx_paused() -> bool = ee_screenscpp_gfx_paused;
    fn sleep(milliseconds: u32) = ee_screenscpp_sleep;
    fn pointer_shown() -> bool = ee_screenscpp_pointer_shown;
    fn game_window_width() -> u32 = ee_screenscpp_game_window_width;
    fn game_window_height() -> u32 = ee_screenscpp_game_window_height;
    fn pointer_post(message: u32) = ee_screenscpp_pointer_post;
    fn pointer_post_surface(message: u32) = ee_screenscpp_pointer_post_surface;
    fn change_game_screen(width: u32, height: u32, flag: bool) = ee_screenscpp_change_game_screen;
    fn manager_relayout_all() = ee_screenscpp_manager_relayout_all;
    fn mini_map() -> usize = ee_screenscpp_mini_map;
    fn world_write_lock() = ee_screenscpp_world_write_lock;
    fn world_write_unlock() = ee_screenscpp_world_write_unlock;
    fn world_read_lock() = ee_screenscpp_world_read_lock;
    fn world_read_unlock() = ee_screenscpp_world_read_unlock;
    fn mini_map_redraw_all() = ee_screenscpp_mini_map_redraw_all;
    fn game_window_client_origin() = ee_screenscpp_game_window_client_origin;
    fn rasterizer_screen_count() -> u32 = ee_screenscpp_rasterizer_screen_count;
    fn rasterizer_release_screen(index: u32) = ee_screenscpp_rasterizer_release_screen;
    fn delete_effect_batch_buffers() = ee_screenscpp_delete_effect_batch_buffers;
    fn rasterizer_deactivate() -> i32 = ee_screenscpp_rasterizer_deactivate;
    fn verify_rasterizer_deactivated(result: i32) = ee_screenscpp_verify_rasterizer_deactivated;
    fn rasterizer_select(index: i32) = ee_screenscpp_rasterizer_select;
    fn gfx_post_set_rasterizer(screen: i32) = ee_screenscpp_gfx_post_set_rasterizer;
    fn manager_change_rasterizer() = ee_screenscpp_manager_change_rasterizer;
    fn menu_width() -> u32 = ee_screenscpp_menu_width;
    fn menu_height() -> u32 = ee_screenscpp_menu_height;
    fn manager_lock_for_copy() = ee_screenscpp_manager_lock_for_copy;
    fn manager_unlock_for_copy() = ee_screenscpp_manager_unlock_for_copy;
    fn manager_form_visible(form: u32) -> bool = ee_screenscpp_manager_form_visible;
    fn control_set_display_mode(control: usize, mode: u32) = ee_screenscpp_control_set_display_mode;
    fn label_append_wide(label: usize, text: usize) = ee_screenscpp_label_append_wide;
    fn select_server_event_new() -> usize = ee_screenscpp_select_server_event_new;
    fn manager_form_set_open_event(form: u32, event: usize) = ee_screenscpp_manager_form_set_open_event;
    fn manager_form_set_close_event(form: u32, event: usize) = ee_screenscpp_manager_form_set_close_event;
    fn check_box_checked(check: usize) -> bool = ee_screenscpp_check_box_checked;
    fn won_password_from_edit(edit: usize) = ee_screenscpp_won_password_from_edit;
    fn won_password_clear() = ee_screenscpp_won_password_clear;
    fn multiplayer_string_from_edit(edit: usize) = ee_screenscpp_multiplayer_string_from_edit;
    fn manager_pop_to(form: u32) = ee_screenscpp_manager_pop_to;
    fn manager_push(form: u32) = ee_screenscpp_manager_push;
    fn manager_form(form: u32) -> usize = ee_screenscpp_manager_form;
    fn keyboard_refresh() = ee_screenscpp_keyboard_refresh;
    fn mouse_input_lock_enter() = ee_screenscpp_mouse_input_lock_enter;
    fn mouse_refresh() = ee_screenscpp_mouse_refresh;
    fn mouse_update() = ee_screenscpp_mouse_update;
    fn mouse_input_lock_leave() = ee_screenscpp_mouse_input_lock_leave;
    fn lobby_shown() -> bool = ee_screenscpp_lobby_shown;
    fn manager_update_ui() = ee_screenscpp_manager_update_ui;
    fn actions_process() = ee_screenscpp_actions_process;
    fn ui_resume_graphics(ui: usize) = ee_screenscpp_ui_resume_graphics;
    fn continents_clear() = ee_screenscpp_continents_clear;
    fn continents_build_regions() = ee_screenscpp_continents_build_regions;
    fn player_register_herds(player: u32) = ee_screenscpp_player_register_herds;
    fn player_assign_herds(player: u32) = ee_screenscpp_player_assign_herds;
    fn player_register_towns(player: u32) = ee_screenscpp_player_register_towns;
    fn player_assign_towns(player: u32) = ee_screenscpp_player_assign_towns;
    fn player_refresh_resource_centres(player: u32) = ee_screenscpp_player_refresh_resource_centres;
    fn player_refresh_town_groups(player: u32) = ee_screenscpp_player_refresh_town_groups;
    fn mini_map_redraw() = ee_screenscpp_mini_map_redraw;
    fn ambient_enable_from_settings() = ee_screenscpp_ambient_enable_from_settings;
    fn ambient_restart() = ee_screenscpp_ambient_restart;
    fn world_options_set_load_type(load_type: i32) = ee_screenscpp_world_options_set_load_type;
    fn world_options_set_map_type(name: EeScreensShimText) = ee_screenscpp_world_options_set_map_type;
    fn header_load_file(header: usize, file: usize) -> i32 = ee_screenscpp_header_load_file;
    fn server_set_humans(humans: u32) = ee_screenscpp_server_set_humans;
    fn server_set_multiplayer(multiplayer: bool) = ee_screenscpp_server_set_multiplayer;
    fn options_map_type_from_file(file: usize) = ee_screenscpp_options_map_type_from_file;
    fn mouse_resume() = ee_screenscpp_mouse_resume;
    fn keyboard_resume() = ee_screenscpp_keyboard_resume;
    fn mouse_suspend() = ee_screenscpp_mouse_suspend;
    fn keyboard_suspend() = ee_screenscpp_keyboard_suspend;
    fn set_locale_all(locale: EeScreensShimText) = ee_screenscpp_set_locale_all;
    fn set_locale_numeric_english() = ee_screenscpp_set_locale_numeric_english;
    fn set_locale_ctype_c() = ee_screenscpp_set_locale_ctype_c;
    fn query_locale() = ee_screenscpp_query_locale;
    fn ui_render_object_new() = ee_screenscpp_ui_render_object_new;
    fn gfx_push_ui_render_object() = ee_screenscpp_gfx_push_ui_render_object;
    fn gfx_set_flag54(flag: bool) = ee_screenscpp_gfx_set_flag54;
    fn gfx_post_resume() = ee_screenscpp_gfx_post_resume;
    fn fpu_set_precision_53() = ee_screenscpp_fpu_set_precision_53;
    fn fpu_precision_is_53() -> bool = ee_screenscpp_fpu_precision_is_53;
    fn fpu_rounding_mode() -> u32 = ee_screenscpp_fpu_rounding_mode;
    fn fpu_set_rounding_mode(mode: u32) = ee_screenscpp_fpu_set_rounding_mode;
    fn camera_update(seconds: f32) = ee_screenscpp_camera_update;
    fn gfx_release_scene() = ee_screenscpp_gfx_release_scene;
    fn gfx_wait_50a() = ee_screenscpp_gfx_wait_50a;
    fn physics_post_init() = ee_screenscpp_physics_post_init;
    fn physics_run_owed_ticks() = ee_screenscpp_physics_run_owed_ticks;
    fn physics_init() = ee_screenscpp_physics_init;
    fn communications() -> usize = ee_screenscpp_communications;
    fn new_client() = ee_screenscpp_new_client;
    fn client_start_threads() = ee_screenscpp_client_start_threads;
    fn boot_client_new(id: i32) -> usize = ee_screenscpp_boot_client_new;
    fn music_pause() = ee_screenscpp_music_pause;
    fn ambient_stop_sounds() = ee_screenscpp_ambient_stop_sounds;
    fn ambient_pause() = ee_screenscpp_ambient_pause;
    fn chat_log_clear() = ee_screenscpp_chat_log_clear;
    fn civ_builder_button_blinking(blinking: bool) = ee_screenscpp_civ_builder_button_blinking;
    fn player_civilization_chosen() = ee_screenscpp_player_civilization_chosen;
    fn player_civilization_locked() = ee_screenscpp_player_civilization_locked;
    fn event_process(event: usize, entity: usize) = ee_screenscpp_event_process;
    fn quick_save_set_load(action: usize) = ee_screenscpp_quick_save_set_load;
    fn chat_entry_new() -> usize = ee_screenscpp_chat_entry_new;
    fn chat_entry_text_from_dialog(entry: usize, dialog: usize) = ee_screenscpp_chat_entry_text_from_dialog;
    fn chat_entry_set_type(entry: usize, kind: u32) = ee_screenscpp_chat_entry_set_type;
    fn chat_log_add(entry: usize) = ee_screenscpp_chat_log_add;
    fn chat_entry_delete(entry: usize) = ee_screenscpp_chat_entry_delete;
    fn set_quit_flag() = ee_screenscpp_set_quit_flag;
    fn boot_client_host_gone_new() -> usize = ee_screenscpp_boot_client_host_gone_new;
    fn client_disconnect() = ee_screenscpp_client_disconnect;
    fn conductor() -> usize = ee_screenscpp_conductor;
    fn conductor_volume() -> f32 = ee_screenscpp_conductor_volume;
    fn sound_manager() -> usize = ee_screenscpp_sound_manager;
    fn master_volume() -> f32 = ee_screenscpp_master_volume;
    fn ime_composition(text: EeScreensShimText, attributes: EeScreensShimText, cursor: u32) = ee_screenscpp_ime_composition;
    // EEUIFMainGame.cpp's (main_form_calls.rs)
    fn mf_set_ui_state(rec: usize, state: i32) = ee_screenscpp_mf_set_ui_state;
    fn mf_has_wall(rec: usize) -> bool = ee_screenscpp_mf_has_wall;
    fn mf_wall_cancel(rec: usize) = ee_screenscpp_mf_wall_cancel;
    fn mf_gate_cancel(rec: usize) = ee_screenscpp_mf_gate_cancel;
    fn mouse_left_just_pressed() -> bool = ee_screenscpp_mouse_left_just_pressed;
    fn mouse_left_down() -> bool = ee_screenscpp_mouse_left_down;
    fn mouse_left_just_unpressed() -> bool = ee_screenscpp_mouse_left_just_unpressed;
    fn mouse_right_just_pressed() -> bool = ee_screenscpp_mouse_right_just_pressed;
    fn mouse_right_down() -> bool = ee_screenscpp_mouse_right_down;
    fn mouse_right_just_unpressed() -> bool = ee_screenscpp_mouse_right_just_unpressed;
    fn key_shift_value() -> i32 = ee_screenscpp_key_shift_value;
    fn key_exists(key: u32) -> bool = ee_screenscpp_key_exists;
    fn key_value(key: u32) -> bool = ee_screenscpp_key_value;
    fn key_just_pressed(key: u32) -> bool = ee_screenscpp_key_just_pressed;
    fn rasterizer_view_contains(x: f32, y: f32) -> bool = ee_screenscpp_rasterizer_view_contains;
    fn view_contains(x: f32, y: f32) -> bool = ee_screenscpp_view_contains;
    fn picked_owned_by_current(rec: usize) -> bool = ee_screenscpp_picked_owned_by_current;
    fn picked_owner_hostile(rec: usize) -> bool = ee_screenscpp_picked_owner_hostile;
    fn picked_seen_by_current(rec: usize) -> bool = ee_screenscpp_picked_seen_by_current;
    fn mf_first_from_group(rec: usize) -> bool = ee_screenscpp_mf_first_from_group;
    fn mf_first_clear(rec: usize) = ee_screenscpp_mf_first_clear;
    fn mf_first_selected(rec: usize) -> bool = ee_screenscpp_mf_first_selected;
    fn mf_has_first(rec: usize) -> bool = ee_screenscpp_mf_has_first;
    fn first_distance_squared(rec: usize, x: f32, y: f32, z: f32) -> f32 = ee_screenscpp_first_distance_squared;
    fn first_min_range(rec: usize) -> f32 = ee_screenscpp_first_min_range;
    fn player_add_event(x: i64, y: i64, kind: i32) = ee_screenscpp_player_add_event;
    fn ghost_count() -> u32 = ee_screenscpp_ghost_count;
    fn ghost_can_place(ghost: u32) -> bool = ee_screenscpp_ghost_can_place;
    fn ghost_type(ghost: u32) -> u32 = ee_screenscpp_ghost_type;
    fn ghost_tile_x(ghost: u32) -> i64 = ee_screenscpp_ghost_tile_x;
    fn ghost_tile_y(ghost: u32) -> i64 = ee_screenscpp_ghost_tile_y;
    fn build_set_queued(rec: usize, queued: bool) = ee_screenscpp_build_set_queued;
    fn build_at(rec: usize, x: f32, y: f32) = ee_screenscpp_build_at;
    fn camera_clear_unit_a() = ee_screenscpp_camera_clear_unit_a;
    fn camera_clear_unit_b() = ee_screenscpp_camera_clear_unit_b;
    fn hack_update(rec: usize, x: f32, y: f32) = ee_screenscpp_hack_update;
    fn hack_point(rec: usize, x: f32, y: f32) = ee_screenscpp_hack_point;
    fn hack_place(rec: usize, x: f32, y: f32) = ee_screenscpp_hack_place;
    fn mf_mouse_x(rec: usize) -> f32 = ee_screenscpp_mf_mouse_x;
    fn mf_mouse_y(rec: usize) -> f32 = ee_screenscpp_mf_mouse_y;
    fn mf_mouse_mode(rec: usize) -> i64 = ee_screenscpp_mf_mouse_mode;
    fn cinematic_ending() -> bool = ee_screenscpp_cinematic_ending;
    fn post_cinematic_screen_shot() = ee_screenscpp_post_cinematic_screen_shot;
    fn end_cinematic_event_new() -> usize = ee_screenscpp_end_cinematic_event_new;
    fn mf_start_selection(rec: usize) = ee_screenscpp_mf_start_selection;
    fn mf_start_command(rec: usize) = ee_screenscpp_mf_start_command;
    fn mf_wall_build(rec: usize) = ee_screenscpp_mf_wall_build;
    fn mf_gate_build(rec: usize, x: f32, y: f32, direction: i32) = ee_screenscpp_mf_gate_build;
    fn mf_gate_create_ghost(rec: usize) = ee_screenscpp_mf_gate_create_ghost;
    fn gate_preview(x: f32, y: f32, direction: EeScreensShimLongOut) = ee_screenscpp_gate_preview;
    fn mf_wall_update(rec: usize) = ee_screenscpp_mf_wall_update;
    fn mf_gate_update(rec: usize) = ee_screenscpp_mf_gate_update;
    fn mf_error_lock_enter(rec: usize) = ee_screenscpp_mf_error_lock_enter;
    fn mf_error_lock_leave(rec: usize) = ee_screenscpp_mf_error_lock_leave;
    fn mf_group_reset(rec: usize) = ee_screenscpp_mf_group_reset;
    fn mf_group_refresh(rec: usize) = ee_screenscpp_mf_group_refresh;
    fn mf_group_select(rec: usize) = ee_screenscpp_mf_group_select;
    fn mf_group_wants_refresh(rec: usize) -> bool = ee_screenscpp_mf_group_wants_refresh;
    fn mf_pick(rec: usize) -> usize = ee_screenscpp_mf_pick;
    fn mf_picked_is_wall(rec: usize) -> bool = ee_screenscpp_mf_picked_is_wall;
    fn mf_picked_tile_x(rec: usize) -> i64 = ee_screenscpp_mf_picked_tile_x;
    fn mf_picked_tile_y(rec: usize) -> i64 = ee_screenscpp_mf_picked_tile_y;
    fn mouse_tile(x: EeScreensShimLongOut, y: EeScreensShimLongOut) = ee_screenscpp_mouse_tile;
    fn mf_picked_dereference(rec: usize) = ee_screenscpp_mf_picked_dereference;
    fn mf_picked_clear(rec: usize) = ee_screenscpp_mf_picked_clear;
    fn mf_form_gate_create_ghost(rec: usize) = ee_screenscpp_mf_form_gate_create_ghost;
    fn gate_preview_at_mouse(direction: EeScreensShimLongOut) = ee_screenscpp_gate_preview_at_mouse;
    fn mf_wall_start_x(rec: usize) -> f32 = ee_screenscpp_mf_wall_start_x;
    fn mf_wall_start_y(rec: usize) -> f32 = ee_screenscpp_mf_wall_start_y;
    fn team_wonder_countdown(team: u32, value: EeScreensShimULongOut) -> usize = ee_screenscpp_team_wonder_countdown;
    fn mf_info_set_text(rec: usize) = ee_screenscpp_mf_info_set_text;
    fn mf_control(rec: usize, control: u32) -> usize = ee_screenscpp_mf_control;
    fn mf_info_unit_type(rec: usize) -> u32 = ee_screenscpp_mf_info_unit_type;
    fn unit_class_count() -> u32 = ee_screenscpp_unit_class_count;
    fn unit_class_exists(kind: u32) -> bool = ee_screenscpp_unit_class_exists;
    fn player_class_has_button(kind: u32) -> bool = ee_screenscpp_player_class_has_button;
    fn mf_info_set_face(rec: usize, bitmap: usize, portrait: usize) = ee_screenscpp_mf_info_set_face;
    fn ui_bitmap_set_bitmap(control: usize, bitmap: usize) = ee_screenscpp_ui_bitmap_set_bitmap;
    fn control_refresh_opacity(control: usize) = ee_screenscpp_control_refresh_opacity;
    fn mf_info_load_sound(rec: usize, sample: EeScreensShimULongOut, length: EeScreensShimULongOut) = ee_screenscpp_mf_info_load_sound;
    fn mf_poll_new(rec: usize, milliseconds: u32) = ee_screenscpp_mf_poll_new;
    fn mf_poll_start(rec: usize) = ee_screenscpp_mf_poll_start;
    fn game_size_width() -> u32 = ee_screenscpp_game_size_width;
    fn game_size_height() -> u32 = ee_screenscpp_game_size_height;
    fn mf_scroll_new(rec: usize) = ee_screenscpp_mf_scroll_new;
    fn mf_scroll_set_edges(rec: usize, left: f32, top: f32, right: f32, bottom: f32) = ee_screenscpp_mf_scroll_set_edges;
    fn mf_mouse_click_new(rec: usize) = ee_screenscpp_mf_mouse_click_new;
    fn mf_lasso_new(rec: usize) = ee_screenscpp_mf_lasso_new;
    fn mf_build_new(rec: usize) = ee_screenscpp_mf_build_new;
    fn mf_calamity_new(rec: usize) = ee_screenscpp_mf_calamity_new;
    fn mf_wheel_zoom_new(rec: usize, code: u32) = ee_screenscpp_mf_wheel_zoom_new;
    fn mf_set_wheel_event(rec: usize) = ee_screenscpp_mf_set_wheel_event;
    fn mf_grab_scroll_new(rec: usize, code: u32) = ee_screenscpp_mf_grab_scroll_new;
    fn mf_set_middle_pressed_event(rec: usize) = ee_screenscpp_mf_set_middle_pressed_event;
    fn mf_set_middle_unpressed_event(rec: usize) = ee_screenscpp_mf_set_middle_unpressed_event;
    fn mf_add_hot_key(rec: usize, key: u32, modifier1: u32, modifier2: u32, modifier3: u32, event1: usize, event2: usize, event3: usize, event4: usize, id: u32, flag: bool) = ee_screenscpp_mf_add_hot_key;
    fn mf_unit_load(rec: usize, unit: u32) = ee_screenscpp_mf_unit_load;
    fn mf_unit(rec: usize) -> usize = ee_screenscpp_mf_unit;
    fn mf_ghost_of_node(rec: usize) = ee_screenscpp_mf_ghost_of_node;
    fn mf_ghost_of_calamity(rec: usize) = ee_screenscpp_mf_ghost_of_calamity;
    fn mf_gate_set_technology(rec: usize, technology: i32) = ee_screenscpp_mf_gate_set_technology;
    fn mf_gate_wall_type_from_node(rec: usize) = ee_screenscpp_mf_gate_wall_type_from_node;
    fn mf_gate_class_from_node(rec: usize) = ee_screenscpp_mf_gate_class_from_node;
    fn mf_gate_initialize(rec: usize) = ee_screenscpp_mf_gate_initialize;
    fn mf_wall_set_technology(rec: usize, technology: i32) = ee_screenscpp_mf_wall_set_technology;
    fn mf_wall_initialize(rec: usize) = ee_screenscpp_mf_wall_initialize;
    fn selected_count() -> u32 = ee_screenscpp_selected_count;
    fn mf_caster_first(rec: usize) = ee_screenscpp_mf_caster_first;
    fn mf_caster_clear(rec: usize) = ee_screenscpp_mf_caster_clear;
    fn mf_has_caster(rec: usize) -> bool = ee_screenscpp_mf_has_caster;
    fn mf_calamity_set(rec: usize, calamity: i32) = ee_screenscpp_mf_calamity_set;
    fn mf_calamity_set_caster(rec: usize) = ee_screenscpp_mf_calamity_set_caster;
    fn mf_calamity_target_kind(rec: usize) -> i64 = ee_screenscpp_mf_calamity_target_kind;
    fn mouse_shift_off() = ee_screenscpp_mouse_shift_off;
    fn mouse_set_pointer(pointer: i32) = ee_screenscpp_mouse_set_pointer;
    fn mouse_set_bounds(left: f32, top: f32, right: f32, bottom: f32) = ee_screenscpp_mouse_set_bounds;
    fn server_unpause(server: usize) = ee_screenscpp_server_unpause;
    fn graphics() -> usize = ee_screenscpp_graphics;
    fn gfx_clear_render_objects() = ee_screenscpp_gfx_clear_render_objects;
    fn mf_cancel_button_up(rec: usize, event: usize) = ee_screenscpp_mf_cancel_button_up;
    fn mf_cancel_right_button_up(rec: usize, event: usize) = ee_screenscpp_mf_cancel_right_button_up;
    fn intro_set_blinking(blinking: bool) = ee_screenscpp_intro_set_blinking;
    fn mf_main_group_open(rec: usize) = ee_screenscpp_mf_main_group_open;
    fn mf_main_group_clear_l20(rec: usize) = ee_screenscpp_mf_main_group_clear_l20;
    fn post_graphics_all() = ee_screenscpp_post_graphics_all;
    fn gfx_fade_in(value: u32) = ee_screenscpp_gfx_fade_in;
    fn mf_media_load(rec: usize) = ee_screenscpp_mf_media_load;
    fn mf_old_media_unload(rec: usize) = ee_screenscpp_mf_old_media_unload;
    fn shell_op(control: usize, op: u32) = ee_screenscpp_shell_op;
    fn mf_background(rec: usize, id: u32) -> usize = ee_screenscpp_mf_background;
    fn background_draw(background: usize) = ee_screenscpp_background_draw;
    fn mf_fps(rec: usize) -> f32 = ee_screenscpp_mf_fps;
}
