// SPDX-License-Identifier: GPL-3.0-or-later
use std::fs;
use std::path::{Path, PathBuf};

use tauri::{AppHandle, Manager};

pub fn data_dir(app: &AppHandle) -> Result<PathBuf, String> {
    let dir = app
        .path()
        .app_data_dir()
        .map_err(|error| error.to_string())?;
    fs::create_dir_all(&dir).map_err(|error| error.to_string())?;
    Ok(dir)
}

pub fn youtube_dir(root: &Path, build: &str, region: &str) -> Result<PathBuf, String> {
    let dir = root.join("pkgs").join(build).join(region);
    fs::create_dir_all(&dir).map_err(|error| error.to_string())?;
    Ok(dir)
}

pub fn merged_pkg(root: &Path, build: &str, region: &str) -> PathBuf {
    root.join("pkgs")
        .join(build)
        .join(region)
        .join("merged.pkg")
}

pub fn download0_dir(root: &Path) -> PathBuf {
    root.join("download0")
}

pub fn download0_file(root: &Path) -> PathBuf {
    download0_dir(root).join("download0.dat")
}

pub fn fakesignin_file(root: &Path) -> PathBuf {
    root.join("fakesignin").join("np-fake-signin-ps5.elf")
}
