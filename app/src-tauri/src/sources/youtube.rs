// SPDX-License-Identifier: GPL-3.0-or-later
use std::path::Path;
use std::sync::atomic::AtomicBool;

use serde::Serialize;
use tauri::{AppHandle, Manager};

use crate::manifest;
use crate::paths;
use crate::transfer::{self, Progress, Transfers};

#[derive(Serialize)]
pub struct FetchResult {
    pub base: String,
    pub sc: String,
    pub merged: String,
    pub directory: String,
    pub size: u64,
}

pub async fn fetch_into(
    root: &Path,
    build: &str,
    region: &str,
    progress: &(dyn Fn(Progress) + Send + Sync),
    cancel: &AtomicBool,
) -> Result<FetchResult, String> {
    let source = manifest::build(build).ok_or("unknown YouTube build")?;
    let sc = source.sc.get(region).ok_or("unknown region")?;
    let dir = paths::youtube_dir(root, build, region)?;
    let base_path = dir.join("base.pkg");
    let sc_path = dir.join("sc.pkg");
    let merged_path = paths::merged_pkg(root, build, region);

    // The base host serves a certificate for a different name, so it answers over plain
    // HTTP. The pinned hash remains the trust anchor either way.
    let base_url = source.base.url.replacen("https://", "http://", 1);
    transfer::download(
        progress,
        cancel,
        transfer::DownloadRequest {
            label: "base",
            url: &base_url,
            destination: &base_path,
            size: source.base.size,
            sha256: &source.base.sha256,
            allow_bad_certificate: false,
        },
    )
    .await?;
    transfer::download(
        progress,
        cancel,
        transfer::DownloadRequest {
            label: "sc",
            url: &sc.url,
            destination: &sc_path,
            size: sc.size,
            sha256: &sc.sha256,
            allow_bad_certificate: true,
        },
    )
    .await?;

    let expected = source.base.size + sc.size;
    let merged_size = std::fs::metadata(&merged_path).ok().map(|meta| meta.len());
    if merged_size != Some(expected) {
        progress(Progress {
            kind: "join".into(),
            label: "merged".into(),
            done: 0,
            total: expected,
        });
        let (base_copy, sc_copy, merged_copy) =
            (base_path.clone(), sc_path.clone(), merged_path.clone());
        tokio::task::spawn_blocking(move || transfer::join(&base_copy, &sc_copy, &merged_copy))
            .await
            .map_err(|error| error.to_string())??;
        progress(Progress {
            kind: "join".into(),
            label: "merged".into(),
            done: expected,
            total: expected,
        });
    }

    Ok(FetchResult {
        base: base_path.display().to_string(),
        sc: sc_path.display().to_string(),
        merged: merged_path.display().to_string(),
        directory: dir.display().to_string(),
        size: expected,
    })
}

#[tauri::command]
pub async fn fetch_youtube(
    app: AppHandle,
    build: String,
    region: String,
) -> Result<FetchResult, String> {
    let root = paths::data_dir(&app)?;
    let cancel = app.state::<Transfers>().start();
    let progress = transfer::emitter(&app);
    fetch_into(&root, &build, &region, &progress, &cancel).await
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Downloads a real build from Sony and joins it. Ignored by default because it needs
    /// the network; run with `cargo test -- --ignored --nocapture`.
    #[tokio::test]
    #[ignore = "downloads about 107 MB from Sony"]
    async fn fetches_and_joins_a_real_build() {
        let root = std::env::temp_dir().join(format!("y2jb-fetch-{}", std::process::id()));
        std::fs::remove_dir_all(&root).ok();
        let cancel = AtomicBool::new(false);
        let result = fetch_into(&root, "v01.000.030", "usa", &|_| {}, &cancel)
            .await
            .expect("fetch failed");
        assert_eq!(result.size, 101515264);
        assert_eq!(std::fs::metadata(&result.merged).unwrap().len(), 101515264);
        std::fs::remove_dir_all(&root).ok();
    }
}
