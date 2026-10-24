// SPDX-License-Identifier: GPL-3.0-or-later
use serde::Serialize;
use std::io::Write;
use std::net::{Shutdown, SocketAddr, TcpStream};
use std::time::{Duration, Instant};
use tauri::{AppHandle, Emitter};

use crate::ports::{API_PORT, LOADER_PORT};

const CHUNK: usize = 64 * 1024;

/// The payload from the same build as the app. It is compiled in, so there is no file to
/// find at runtime and the app can never send a payload it was not built with.
const PAYLOAD: &[u8] = include_bytes!("../../../build/y2jb-installer.elf");

#[derive(Clone, Serialize)]
struct Progress {
    sent: u64,
    total: u64,
}

#[derive(Serialize)]
pub struct PayloadInfo {
    size: u64,
}

#[tauri::command]
pub fn payload_info() -> PayloadInfo {
    PayloadInfo {
        size: PAYLOAD.len() as u64,
    }
}

fn send(app: &AppHandle, ip: &str) -> Result<u64, String> {
    let addr: SocketAddr = format!("{ip}:{LOADER_PORT}")
        .parse()
        .map_err(|e: std::net::AddrParseError| e.to_string())?;
    let mut socket = TcpStream::connect_timeout(&addr, Duration::from_secs(5))
        .map_err(|e| format!("cannot reach the loader on {addr}: {e}"))?;
    socket.set_write_timeout(Some(Duration::from_secs(30))).ok();

    let total = PAYLOAD.len() as u64;
    let mut sent = 0u64;
    for chunk in PAYLOAD.chunks(CHUNK) {
        socket
            .write_all(chunk)
            .map_err(|e| format!("the loader closed the connection: {e}"))?;
        sent += chunk.len() as u64;
        let _ = app.emit("payload-progress", Progress { sent, total });
    }
    // Half-close so the loader sees the end of the ELF.
    let _ = socket.shutdown(Shutdown::Write);
    Ok(sent)
}

#[tauri::command]
pub async fn send_payload(app: AppHandle, ip: String) -> Result<u64, String> {
    tauri::async_runtime::spawn_blocking(move || send(&app, &ip))
        .await
        .map_err(|e| e.to_string())?
}

fn wait(ip: &str, timeout_ms: u64) -> bool {
    let addr: SocketAddr = match format!("{ip}:{API_PORT}").parse() {
        Ok(addr) => addr,
        Err(_) => return false,
    };
    let deadline = Instant::now() + Duration::from_millis(timeout_ms);
    loop {
        if TcpStream::connect_timeout(&addr, Duration::from_millis(400)).is_ok() {
            return true;
        }
        if Instant::now() >= deadline {
            return false;
        }
        std::thread::sleep(Duration::from_millis(500));
    }
}

#[tauri::command]
pub async fn wait_for_api(ip: String, timeout_ms: u64) -> bool {
    tauri::async_runtime::spawn_blocking(move || wait(&ip, timeout_ms))
        .await
        .unwrap_or(false)
}
