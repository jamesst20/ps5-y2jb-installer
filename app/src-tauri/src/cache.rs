// SPDX-License-Identifier: GPL-3.0-or-later
use std::path::{Path, PathBuf};

use serde::Serialize;
use tauri::AppHandle;

use crate::{digest, paths};

#[derive(Serialize)]
pub struct FileResult {
    pub label: String,
    pub path: String,
    pub size: u64,
    /// The digest this fetch verified, so the UI does not have to hash the file again.
    pub digest: String,
}

#[derive(Serialize)]
pub struct CachedFile {
    pub path: String,
    pub size: u64,
    pub digest: String,
}

#[derive(Serialize)]
pub struct PkgCache {
    pub build: String,
    pub region: String,
    pub path: String,
    pub base: bool,
    pub sc: bool,
    pub merged: bool,
}

#[derive(Serialize)]
pub struct CacheStatus {
    pub download0: Option<CachedFile>,
    pub fakesignin: Option<CachedFile>,
    pub pkgs: Vec<PkgCache>,
    /// Where YouTube builds and regions are cached, even when the directory is empty.
    pub pkgs_path: String,
}

fn file_size(path: &Path) -> Option<u64> {
    std::fs::metadata(path)
        .ok()
        .filter(|metadata| metadata.is_file())
        .map(|metadata| metadata.len())
}

fn cached_file(path: &Path) -> Option<CachedFile> {
    let size = file_size(path)?;
    Some(CachedFile {
        path: path.display().to_string(),
        size,
        digest: digest::sha256_file(path).unwrap_or_default(),
    })
}

pub fn build(root: Option<PathBuf>) -> CacheStatus {
    let mut status = CacheStatus {
        download0: None,
        fakesignin: None,
        pkgs: Vec::new(),
        pkgs_path: String::new(),
    };
    let Some(root) = root else {
        return status;
    };

    status.download0 = cached_file(&paths::download0_file(&root));
    status.fakesignin = cached_file(&paths::fakesignin_file(&root));

    let pkgs = root.join("pkgs");
    status.pkgs_path = pkgs.display().to_string();
    if let Ok(builds) = std::fs::read_dir(&pkgs) {
        for build in builds.flatten() {
            if let Ok(regions) = std::fs::read_dir(build.path()) {
                for region in regions.flatten() {
                    let dir = region.path();
                    if !dir.is_dir() {
                        continue;
                    }
                    status.pkgs.push(PkgCache {
                        build: build.file_name().to_string_lossy().into_owned(),
                        region: region.file_name().to_string_lossy().into_owned(),
                        path: dir.display().to_string(),
                        base: file_size(&dir.join("base.pkg")).is_some(),
                        sc: file_size(&dir.join("sc.pkg")).is_some(),
                        merged: file_size(&dir.join("merged.pkg")).is_some(),
                    });
                }
            }
        }
    }
    status
        .pkgs
        .sort_by(|a, b| (&a.build, &a.region).cmp(&(&b.build, &b.region)));
    status
}

#[tauri::command]
pub async fn cache_status(app: AppHandle) -> CacheStatus {
    let root = paths::data_dir(&app).ok();
    tauri::async_runtime::spawn_blocking(move || build(root))
        .await
        .unwrap_or_else(|_| CacheStatus {
            download0: None,
            fakesignin: None,
            pkgs: Vec::new(),
            pkgs_path: String::new(),
        })
}

#[tauri::command]
pub fn clear_cache(app: AppHandle) -> Result<(), String> {
    let root = paths::data_dir(&app)?;
    for name in ["pkgs", "download0", "fakesignin"] {
        let path = root.join(name);
        if let Err(error) = std::fs::remove_dir_all(&path) {
            if error.kind() != std::io::ErrorKind::NotFound {
                return Err(format!("{name}: {error}"));
            }
        }
    }
    Ok(())
}

/// Removes only the download0 cache, leaving the PKG and fake-sign-in caches alone.
#[tauri::command]
pub fn clear_download0_cache(app: AppHandle) -> Result<(), String> {
    let root = paths::data_dir(&app)?;
    match std::fs::remove_dir_all(paths::download0_dir(&root)) {
        Ok(()) => Ok(()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(error) => Err(format!("download0: {error}")),
    }
}

fn clear_youtube_cache_dir(root: &Path) -> Result<(), String> {
    match std::fs::remove_dir_all(root.join("pkgs")) {
        Ok(()) => Ok(()),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(()),
        Err(error) => Err(format!("YouTube cache: {error}")),
    }
}

/// Removes every cached YouTube build and region, leaving the other caches alone.
#[tauri::command]
pub fn clear_youtube_cache(app: AppHandle) -> Result<(), String> {
    let root = paths::data_dir(&app)?;
    clear_youtube_cache_dir(&root)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reports_the_cached_digest() {
        let root = std::env::temp_dir().join(format!("y2jb-cache-{}", std::process::id()));
        std::fs::remove_dir_all(&root).ok();
        std::fs::create_dir_all(paths::download0_dir(&root)).unwrap();
        std::fs::write(paths::download0_file(&root), b"hello").unwrap();

        let status = build(Some(root.clone()));
        let cached = status.download0.expect("download0 missing");
        assert_eq!(
            cached.digest,
            "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824"
        );
        std::fs::remove_dir_all(&root).ok();
    }

    #[test]
    fn clears_all_youtube_regions() {
        let root = std::env::temp_dir().join(format!("y2jb-youtube-cache-{}", std::process::id()));
        std::fs::remove_dir_all(&root).ok();
        let build = "v01.000.030";
        let usa = paths::youtube_dir(&root, build, "usa").unwrap();
        let europe = paths::youtube_dir(&root, build, "europe").unwrap();
        std::fs::write(usa.join("merged.pkg"), b"usa").unwrap();
        std::fs::write(europe.join("merged.pkg"), b"europe").unwrap();

        clear_youtube_cache_dir(&root).unwrap();

        assert!(!root.join("pkgs").exists());
        std::fs::remove_dir_all(&root).ok();
    }
}
