// SPDX-License-Identifier: GPL-3.0-or-later
use std::path::Path;
use std::sync::atomic::AtomicBool;

use serde::Serialize;
use tauri::{AppHandle, Manager};

use crate::cache::FileResult;
use crate::paths;
use crate::transfer::{self, Progress, Transfers};

use super::github::{self, ReleaseAsset};

#[derive(Clone, Serialize)]
pub struct Download0Release {
    pub tag: String,
    pub size: u64,
    pub digest: String,
    pub published_at: String,
    pub prerelease: bool,
}

#[tauri::command]
pub async fn download0_releases(repo: Option<String>) -> Result<Vec<Download0Release>, String> {
    Ok(github::release_assets(repo.as_deref())
        .await?
        .into_iter()
        .map(|asset| Download0Release {
            tag: asset.tag,
            size: asset.size,
            digest: asset.digest,
            published_at: asset.published_at,
            prerelease: asset.prerelease,
        })
        .collect())
}

fn choose(assets: Vec<ReleaseAsset>, tag: Option<&str>) -> Option<ReleaseAsset> {
    match tag {
        Some(wanted) => assets.into_iter().find(|asset| asset.tag == wanted),
        None => assets.into_iter().find(|asset| !asset.prerelease),
    }
}

pub async fn fetch_into(
    root: &Path,
    repo: Option<&str>,
    tag: Option<&str>,
    progress: &(dyn Fn(Progress) + Send + Sync),
    cancel: &AtomicBool,
) -> Result<FileResult, String> {
    let chosen = choose(github::release_assets(repo).await?, tag)
        .ok_or("that release does not carry a verifiable download0.dat")?;

    let dir = paths::download0_dir(root);
    std::fs::create_dir_all(&dir).map_err(|error| error.to_string())?;
    let destination = paths::download0_file(root);
    let _ = std::fs::remove_file(dir.join("tag.txt"));

    transfer::download(
        progress,
        cancel,
        transfer::DownloadRequest {
            label: "download0",
            url: &chosen.url,
            destination: &destination,
            size: chosen.size,
            sha256: &chosen.digest,
            allow_bad_certificate: false,
        },
    )
    .await?;

    Ok(FileResult {
        label: chosen.tag,
        path: destination.display().to_string(),
        size: chosen.size,
        digest: chosen.digest,
    })
}

#[tauri::command]
pub async fn fetch_download0(
    app: AppHandle,
    repo: Option<String>,
    tag: Option<String>,
) -> Result<FileResult, String> {
    let root = paths::data_dir(&app)?;
    let cancel = app.state::<Transfers>().start();
    let progress = transfer::emitter(&app);
    fetch_into(&root, repo.as_deref(), tag.as_deref(), &progress, &cancel).await
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    #[ignore = "downloads about 337 MB from GitHub"]
    async fn fetches_the_latest_release() {
        let root = std::env::temp_dir().join(format!("y2jb-d0-latest-{}", std::process::id()));
        std::fs::remove_dir_all(&root).ok();
        let cancel = AtomicBool::new(false);
        let result = fetch_into(&root, None, None, &|_| {}, &cancel)
            .await
            .expect("download0 failed");
        assert_eq!(result.size, 336789504);
        assert!(result.label.starts_with('v'));
        std::fs::remove_dir_all(&root).ok();
    }

    #[tokio::test]
    #[ignore = "downloads about 337 MB from GitHub"]
    async fn repairs_a_stale_same_size_cache() {
        let root = std::env::temp_dir().join(format!("y2jb-d0-{}", std::process::id()));
        std::fs::remove_dir_all(&root).ok();
        std::fs::create_dir_all(paths::download0_dir(&root)).unwrap();
        let destination = paths::download0_file(&root);
        let file = std::fs::File::create(&destination).unwrap();
        file.set_len(336789504).unwrap();
        drop(file);

        let before = crate::digest::sha256_file(&destination).unwrap();
        let cancel = AtomicBool::new(false);
        let result = fetch_into(&root, None, None, &|_| {}, &cancel)
            .await
            .expect("download0 failed");
        let after = crate::digest::sha256_file(Path::new(&result.path)).unwrap();
        assert_ne!(before, after, "the stale file was not replaced");
        assert_eq!(std::fs::metadata(&result.path).unwrap().len(), 336789504);
        std::fs::remove_dir_all(&root).ok();
    }
}
