// SPDX-License-Identifier: GPL-3.0-or-later
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

use serde::Serialize;
use sha2::{Digest, Sha256};
use tauri::{AppHandle, Emitter};
use tokio::io::AsyncWriteExt;

use crate::digest;

#[derive(Default)]
pub struct Transfers {
    pub cancel: Arc<AtomicBool>,
}

impl Transfers {
    pub fn start(&self) -> Arc<AtomicBool> {
        self.cancel.store(false, Ordering::Relaxed);
        Arc::clone(&self.cancel)
    }
}

#[derive(Clone, Serialize)]
pub struct Progress {
    pub kind: String,
    pub label: String,
    pub done: u64,
    pub total: u64,
}

pub fn emitter(app: &AppHandle) -> impl Fn(Progress) + Send + Sync + 'static {
    let app = app.clone();
    move |progress| {
        let _ = app.emit("transfer-progress", progress);
    }
}

fn partial_of(path: &Path) -> PathBuf {
    PathBuf::from(format!("{}.partial", path.display()))
}

fn file_size(path: &Path) -> Option<u64> {
    std::fs::metadata(path)
        .ok()
        .filter(|metadata| metadata.is_file())
        .map(|metadata| metadata.len())
}

async fn has_digest(path: &Path, expected: &str) -> bool {
    let path = path.to_path_buf();
    let actual = tokio::task::spawn_blocking(move || digest::sha256_file(&path))
        .await
        .ok()
        .and_then(Result::ok);
    actual.as_deref() == Some(expected)
}

pub struct DownloadRequest<'a> {
    pub label: &'a str,
    pub url: &'a str,
    pub destination: &'a Path,
    pub size: u64,
    pub sha256: &'a str,
    pub allow_bad_certificate: bool,
}

async fn download_to(
    progress: &(dyn Fn(Progress) + Send + Sync),
    cancel: &AtomicBool,
    request: &DownloadRequest<'_>,
) -> Result<(), String> {
    let DownloadRequest {
        label,
        url,
        destination,
        size,
        sha256,
        allow_bad_certificate,
    } = *request;
    let client = reqwest::Client::builder()
        .danger_accept_invalid_certs(allow_bad_certificate)
        .connect_timeout(Duration::from_secs(20))
        .timeout(Duration::from_secs(30 * 60))
        .build()
        .map_err(|error| error.to_string())?;
    let mut response = client
        .get(url)
        .header(reqwest::header::USER_AGENT, crate::USER_AGENT)
        .send()
        .await
        .map_err(|error| format!("{label}: {error}"))?;
    if !response.status().is_success() {
        return Err(format!("{label}: HTTP {}", response.status()));
    }

    let partial = partial_of(destination);
    let mut file = tokio::fs::File::create(&partial)
        .await
        .map_err(|error| format!("{label}: {error}"))?;
    let mut hasher = Sha256::new();
    let mut done = 0u64;
    let mut last_emit = 0u64;

    while let Some(chunk) = response
        .chunk()
        .await
        .map_err(|error| format!("{label}: {error}"))?
    {
        if cancel.load(Ordering::Relaxed) {
            return Err(format!("{label}: cancelled"));
        }
        if done + chunk.len() as u64 > size {
            return Err(format!("{label}: response is larger than the pinned size"));
        }
        file.write_all(&chunk)
            .await
            .map_err(|error| format!("{label}: {error}"))?;
        hasher.update(&chunk);
        done += chunk.len() as u64;
        if done - last_emit >= 1024 * 1024 {
            last_emit = done;
            progress(Progress {
                kind: "fetch".into(),
                label: label.into(),
                done,
                total: size,
            });
        }
    }

    file.flush()
        .await
        .map_err(|error| format!("{label}: {error}"))?;
    file.sync_all()
        .await
        .map_err(|error| format!("{label}: {error}"))?;
    drop(file);

    let actual = hex::encode(hasher.finalize());
    if done != size || actual != sha256 {
        return Err(format!(
            "{label}: size or SHA-256 does not match the pinned value"
        ));
    }
    tokio::fs::rename(&partial, destination)
        .await
        .map_err(|error| format!("{label}: {error}"))?;
    progress(Progress {
        kind: "fetch".into(),
        label: label.into(),
        done,
        total: size,
    });
    Ok(())
}

/// Fetches a file unless a cached copy already has the pinned size and digest. The partial
/// file is removed on every failure, including cancellation.
pub async fn download(
    progress: &(dyn Fn(Progress) + Send + Sync),
    cancel: &AtomicBool,
    request: DownloadRequest<'_>,
) -> Result<(), String> {
    if file_size(request.destination) == Some(request.size)
        && has_digest(request.destination, request.sha256).await
    {
        return Ok(());
    }

    let result = download_to(progress, cancel, &request).await;
    if result.is_err() {
        let _ = tokio::fs::remove_file(partial_of(request.destination)).await;
    }
    result
}

/// Concatenates base and SC after checking the SC magic. Returns the merged size.
pub fn join(base: &Path, sc: &Path, destination: &Path) -> Result<u64, String> {
    use std::io::{Read, Write};

    let mut sc_file = std::fs::File::open(sc).map_err(|error| error.to_string())?;
    let mut magic = [0u8; 4];
    sc_file
        .read_exact(&mut magic)
        .map_err(|error| error.to_string())?;
    if magic != [0x7F, b'C', b'N', b'T'] {
        return Err("the region PKG is not an SC package".into());
    }
    drop(sc_file);

    let partial = partial_of(destination);
    let mut out = std::fs::File::create(&partial).map_err(|error| error.to_string())?;
    let mut total = 0u64;
    for path in [base, sc] {
        let mut input = std::fs::File::open(path).map_err(|error| error.to_string())?;
        let mut buffer = vec![0u8; 64 * 1024];
        loop {
            let read = input.read(&mut buffer).map_err(|error| error.to_string())?;
            if read == 0 {
                break;
            }
            out.write_all(&buffer[..read])
                .map_err(|error| error.to_string())?;
            total += read as u64;
        }
    }
    out.sync_all().map_err(|error| error.to_string())?;
    drop(out);
    std::fs::rename(&partial, destination).map_err(|error| error.to_string())?;
    Ok(total)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sc_bytes() -> Vec<u8> {
        let mut bytes = vec![0x7F, b'C', b'N', b'T'];
        bytes.extend_from_slice(b"payload");
        bytes
    }

    #[test]
    fn join_concatenates_base_then_sc() {
        let dir = std::env::temp_dir().join(format!("y2jb-join-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let base = dir.join("base");
        let sc = dir.join("sc");
        let out = dir.join("out");
        std::fs::write(&base, b"BASE").unwrap();
        std::fs::write(&sc, sc_bytes()).unwrap();

        let size = join(&base, &sc, &out).unwrap();
        assert_eq!(size, 4 + 11);
        assert_eq!(std::fs::read(&out).unwrap(), b"BASE\x7fCNTpayload");

        std::fs::write(&sc, b"not an sc package").unwrap();
        assert!(join(&base, &sc, &dir.join("bad")).is_err());
        std::fs::remove_dir_all(&dir).ok();
    }
}
