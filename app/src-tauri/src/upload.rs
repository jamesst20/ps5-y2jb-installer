// SPDX-License-Identifier: GPL-3.0-or-later
use std::fs::{self, File};
use std::io::{Read, Write};
use std::net::{Shutdown, SocketAddr, TcpStream};
use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use tauri::{AppHandle, Manager};

use crate::digest;
use crate::manifest;
use crate::paths;
use crate::ports;
use crate::transfer::{self, Progress, Transfers};

/// Uploads with a raw HTTP/1.1 request so Content-Length is always present and exact; the
/// payload rejects chunked bodies.
fn put(
    progress: &(dyn Fn(Progress) + Send + Sync),
    cancel: &AtomicBool,
    ip: &str,
    slot: &str,
    path: &Path,
    label: &str,
) -> Result<u64, String> {
    let size = fs::metadata(path)
        .map_err(|error| format!("{label}: {error}"))?
        .len();
    let sha256 = digest::sha256_file(path)?;
    let address: SocketAddr = format!("{ip}:{}", ports::API_PORT)
        .parse()
        .map_err(|error: std::net::AddrParseError| error.to_string())?;
    let mut socket = TcpStream::connect_timeout(&address, Duration::from_secs(5))
        .map_err(|error| format!("cannot reach the console on {address}: {error}"))?;
    socket.set_write_timeout(Some(Duration::from_secs(60))).ok();
    socket.set_read_timeout(Some(Duration::from_secs(20))).ok();

    write!(
        socket,
        "PUT /api/upload?slot={slot}&sha256={sha256} HTTP/1.1\r\n\
         Host: {ip}\r\n\
         Content-Type: application/octet-stream\r\n\
         Content-Length: {size}\r\n\
         Connection: close\r\n\r\n"
    )
    .map_err(|error| error.to_string())?;

    let mut file = File::open(path).map_err(|error| error.to_string())?;
    let mut buffer = vec![0u8; 64 * 1024];
    let mut sent = 0u64;
    let mut last_emit = 0u64;
    loop {
        if cancel.load(Ordering::Relaxed) {
            let _ = socket.shutdown(Shutdown::Both);
            return Err(format!("{label}: cancelled"));
        }
        let read = file.read(&mut buffer).map_err(|error| error.to_string())?;
        if read == 0 {
            break;
        }
        socket
            .write_all(&buffer[..read])
            .map_err(|error| format!("{label}: {error}"))?;
        sent += read as u64;
        if sent - last_emit >= 1024 * 1024 {
            last_emit = sent;
            progress(Progress {
                kind: "upload".into(),
                label: label.into(),
                done: sent,
                total: size,
            });
        }
    }
    socket.shutdown(Shutdown::Write).ok();
    progress(Progress {
        kind: "upload".into(),
        label: label.into(),
        done: size,
        total: size,
    });

    let mut response = String::new();
    socket
        .read_to_string(&mut response)
        .map_err(|error| format!("{label}: {error}"))?;
    let status = response.lines().next().unwrap_or("");
    let succeeded = status
        .split_whitespace()
        .nth(1)
        .map(|code| code == "200")
        .unwrap_or(false);
    if succeeded {
        return Ok(sent);
    }
    let body = response
        .split_once("\r\n\r\n")
        .map(|(_, body)| body.trim())
        .unwrap_or("");
    Err(if body.is_empty() {
        format!("{label}: {status}")
    } else {
        body.to_string()
    })
}

async fn upload(
    app: &AppHandle,
    ip: &str,
    slot: &str,
    path: &Path,
    label: &str,
) -> Result<u64, String> {
    if !path.is_file() {
        return Err(format!("{label} is not downloaded yet"));
    }
    let cancel = app.state::<Transfers>().start();
    let progress = transfer::emitter(app);
    let ip = ip.to_string();
    let slot = slot.to_string();
    let label = label.to_string();
    let path = path.to_path_buf();
    tokio::task::spawn_blocking(move || put(&progress, &cancel, &ip, &slot, &path, &label))
        .await
        .map_err(|error| error.to_string())?
}

#[tauri::command]
pub async fn upload_merged(
    app: AppHandle,
    ip: String,
    build: String,
    region: String,
) -> Result<u64, String> {
    if manifest::build(&build)
        .and_then(|entry| entry.sc.get(&region))
        .is_none()
    {
        return Err("unknown YouTube build or region".into());
    }
    let root = paths::data_dir(&app)?;
    let path = paths::merged_pkg(&root, &build, &region);
    upload(&app, &ip, "merged", &path, "merged").await
}

#[tauri::command]
pub async fn upload_download0(app: AppHandle, ip: String) -> Result<u64, String> {
    let root = paths::data_dir(&app)?;
    upload(
        &app,
        &ip,
        "download0",
        &paths::download0_file(&root),
        "download0",
    )
    .await
}

#[tauri::command]
pub async fn upload_fakesignin(app: AppHandle, ip: String) -> Result<u64, String> {
    let root = paths::data_dir(&app)?;
    upload(
        &app,
        &ip,
        "fakesignin",
        &paths::fakesignin_file(&root),
        "fakesignin",
    )
    .await
}

#[tauri::command]
pub fn cancel_transfer(state: tauri::State<'_, Transfers>) {
    state.cancel.store(true, Ordering::Relaxed);
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Uploads a small file to a real console. Ignored by default; run it with
    /// `Y2JB_TEST_CONSOLE=192.168.x.y cargo test -- --ignored --nocapture`.
    #[test]
    #[ignore = "needs a console; set Y2JB_TEST_CONSOLE"]
    fn uploads_a_file_to_the_console() {
        let Ok(ip) = std::env::var("Y2JB_TEST_CONSOLE") else {
            eprintln!("Y2JB_TEST_CONSOLE is not set");
            return;
        };
        let path = std::env::temp_dir().join(format!("y2jb-upload-{}.bin", std::process::id()));
        let data: Vec<u8> = (0..1024 * 1024).map(|i| (i % 251) as u8).collect();
        std::fs::write(&path, &data).unwrap();
        let cancel = AtomicBool::new(false);
        let sent = put(&|_| {}, &cancel, &ip, "fakesignin", &path, "test");
        std::fs::remove_file(&path).ok();
        assert_eq!(sent.unwrap(), data.len() as u64);
    }
}
