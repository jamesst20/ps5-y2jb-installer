// SPDX-License-Identifier: GPL-3.0-or-later
use std::fs;
use std::net::Ipv4Addr;
use std::path::PathBuf;

use serde::{Deserialize, Serialize};
use tauri::{AppHandle, Manager};

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Settings {
    pub console_ip: String,
    pub custom_download0_repos: Vec<String>,
    pub active_download0_repo: Option<String>,
}

fn settings_path(app: &AppHandle) -> Result<PathBuf, String> {
    let dir = app
        .path()
        .app_config_dir()
        .map_err(|error| error.to_string())?;
    fs::create_dir_all(&dir).map_err(|error| error.to_string())?;
    Ok(dir.join("settings.json"))
}

fn load(app: &AppHandle) -> Settings {
    settings_path(app)
        .ok()
        .and_then(|path| fs::read_to_string(path).ok())
        .and_then(|text| serde_json::from_str(&text).ok())
        .unwrap_or_default()
}

fn save(app: &AppHandle, settings: &Settings) -> Result<(), String> {
    let path = settings_path(app)?;
    let text = serde_json::to_string_pretty(settings).map_err(|error| error.to_string())?;
    fs::write(path, text).map_err(|error| error.to_string())
}

#[tauri::command]
pub fn get_settings(app: AppHandle) -> Settings {
    load(&app)
}

#[tauri::command]
pub fn set_console(app: AppHandle, ip: String) -> Result<Settings, String> {
    let trimmed = ip.trim();
    let console_ip = if trimmed.is_empty() {
        String::new()
    } else {
        trimmed
            .parse::<Ipv4Addr>()
            .map_err(|_| format!("{trimmed} is not an IPv4 address"))?
            .to_string()
    };
    let mut settings = load(&app);
    settings.console_ip = console_ip;
    save(&app, &settings)?;
    Ok(settings)
}

#[tauri::command]
pub fn set_download0_repo(
    app: AppHandle,
    repo: Option<String>,
    all_repos: Option<Vec<String>>,
) -> Result<Settings, String> {
    let mut settings = load(&app);
    if let Some(r) = repo {
        settings.active_download0_repo = if r.trim().is_empty() {
            None
        } else {
            Some(r.trim().to_string())
        };
    }
    if let Some(repos) = all_repos {
        settings.custom_download0_repos = repos;
    }
    save(&app, &settings)?;
    Ok(settings)
}
