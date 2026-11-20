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
    fn game_plat_window() -> usize = ee_screenscpp_game_plat_window;
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
    fn settings_last_save(auto_save: bool, out: EeScreensShimOut) = ee_screenscpp_settings_last_save;
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
}
