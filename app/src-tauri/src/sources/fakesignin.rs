// SPDX-License-Identifier: GPL-3.0-or-later
use std::path::Path;
use std::sync::atomic::AtomicBool;

use tauri::{AppHandle, Manager};

use crate::cache::FileResult;
use crate::paths;
use crate::transfer::{self, Progress, Transfers};

const URL: &str =
    "https://git.etawen.dev/earthonion/np-fake-signin/releases/download/v1.4/np-fake-signin-ps5.elf";
const SIZE: u64 = 121552;
const SHA256: &str = "9ec8562b3e3f6508071e280037e225371d8a6c297e74b8ae4d240e357ee38eca";

pub async fn fetch_into(
    root: &Path,
    progress: &(dyn Fn(Progress) + Send + Sync),
    cancel: &AtomicBool,
) -> Result<FileResult, String> {
    let destination = paths::fakesignin_file(root);
    let dir = destination
        .parent()
        .ok_or("invalid fake-sign-in cache path")?;
    std::fs::create_dir_all(dir).map_err(|error| error.to_string())?;
    transfer::download(
        progress,
        cancel,
        transfer::DownloadRequest {
            label: "fakesignin",
            url: URL,
            destination: &destination,
            size: SIZE,
            sha256: SHA256,
            allow_bad_certificate: false,
        },
    )
    .await?;
    Ok(FileResult {
        label: "v1.4".into(),
        path: destination.display().to_string(),
        size: SIZE,
        digest: SHA256.into(),
    })
}

#[tauri::command]
pub async fn fetch_fakesignin(app: AppHandle) -> Result<FileResult, String> {
    let root = paths::data_dir(&app)?;
    let cancel = app.state::<Transfers>().start();
    let progress = transfer::emitter(&app);
    fetch_into(&root, &progress, &cancel).await
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    #[ignore = "downloads about 120 KB"]
    async fn fetches_the_pinned_asset() {
        let root = std::env::temp_dir().join(format!("y2jb-signin-{}", std::process::id()));
        std::fs::remove_dir_all(&root).ok();
        let cancel = AtomicBool::new(false);
        let result = fetch_into(&root, &|_| {}, &cancel)
            .await
            .expect("fetch failed");
        assert_eq!(result.size, SIZE);
        std::fs::remove_dir_all(&root).ok();
    }
}
