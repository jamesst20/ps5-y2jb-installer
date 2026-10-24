// SPDX-License-Identifier: GPL-3.0-or-later
mod cache;
mod digest;
mod discover;
mod manifest;
mod paths;
mod payload;
mod ports;
mod settings;
mod sources;
mod transfer;
mod upload;

pub const USER_AGENT: &str = concat!("y2jb-installer/", env!("CARGO_PKG_VERSION"));

/// The bundle version, kept in sync with VERSION by `scripts/version.mjs`.
#[tauri::command]
fn app_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(transfer::Transfers::default())
        .invoke_handler(tauri::generate_handler![
            app_version,
            settings::get_settings,
            settings::set_console,
            discover::discover,
            discover::probe,
            payload::payload_info,
            payload::send_payload,
            payload::wait_for_api,
            sources::youtube::fetch_youtube,
            sources::download0::fetch_download0,
            sources::download0::download0_releases,
            sources::fakesignin::fetch_fakesignin,
            cache::cache_status,
            cache::clear_cache,
            cache::clear_download0_cache,
            manifest::manifest_view,
            manifest::expected_build,
            upload::upload_merged,
            upload::upload_download0,
            upload::upload_fakesignin,
            upload::cancel_transfer,
        ])
        .run(tauri::generate_context!())
        .expect("error while running Y2JB Installer");
}
