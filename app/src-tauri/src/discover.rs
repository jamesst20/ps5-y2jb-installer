// SPDX-License-Identifier: GPL-3.0-or-later
use serde::Serialize;
use std::collections::BTreeSet;
use std::net::{IpAddr, Ipv4Addr, SocketAddr, TcpStream};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use crate::ports::{API_PORT, LOADER_PORT};

const PROBE_TIMEOUT: Duration = Duration::from_millis(250);
const WORKERS: usize = 64;

/// A host that answered on at least one of the two ports.
#[derive(Clone, Debug, Serialize)]
pub struct Found {
    pub ip: String,
    pub loader: bool,
    pub api: bool,
}

fn probe_host(ip: Ipv4Addr) -> Found {
    let loader =
        TcpStream::connect_timeout(&SocketAddr::new(IpAddr::V4(ip), LOADER_PORT), PROBE_TIMEOUT)
            .is_ok();
    let api = TcpStream::connect_timeout(&SocketAddr::new(IpAddr::V4(ip), API_PORT), PROBE_TIMEOUT)
        .is_ok();
    Found {
        ip: ip.to_string(),
        loader,
        api,
    }
}

/// The /24 network of every local IPv4 interface. Only the /24 is swept, never a wider
/// range, so a /16 or a VPN address cannot turn a scan into thousands of probes.
fn local_networks() -> Vec<Ipv4Addr> {
    let mut networks = BTreeSet::new();
    if let Ok(interfaces) = if_addrs::get_if_addrs() {
        for interface in interfaces {
            if let IpAddr::V4(v4) = interface.ip() {
                if v4.is_loopback() {
                    continue;
                }
                let o = v4.octets();
                networks.insert(Ipv4Addr::new(o[0], o[1], o[2], 0));
            }
        }
    }
    networks.into_iter().collect()
}

fn scan() -> Vec<Found> {
    let mut hosts = Vec::new();
    for network in local_networks() {
        let o = network.octets();
        for host in 1..=254u8 {
            hosts.push(Ipv4Addr::new(o[0], o[1], o[2], host));
        }
    }
    if hosts.is_empty() {
        return Vec::new();
    }

    let hosts = Arc::new(hosts);
    let next = Arc::new(Mutex::new(0usize));
    let results = Arc::new(Mutex::new(Vec::new()));

    let mut handles = Vec::new();
    for _ in 0..WORKERS {
        let hosts = Arc::clone(&hosts);
        let next = Arc::clone(&next);
        let results = Arc::clone(&results);
        handles.push(thread::spawn(move || loop {
            let index = {
                let mut cursor = match next.lock() {
                    Ok(cursor) => cursor,
                    Err(_) => return,
                };
                if *cursor >= hosts.len() {
                    break;
                }
                let index = *cursor;
                *cursor += 1;
                index
            };
            let found = probe_host(hosts[index]);
            if found.loader || found.api {
                if let Ok(mut list) = results.lock() {
                    list.push(found);
                }
            }
        }));
    }
    for handle in handles {
        let _ = handle.join();
    }

    let mut found = match results.lock() {
        Ok(list) => list.clone(),
        Err(_) => return Vec::new(),
    };
    // A payload that is already running comes first, then a bare loader, then by address.
    found.sort_by_key(|found| (!found.api, !found.loader, found.ip.parse::<Ipv4Addr>().ok()));
    found
}

#[tauri::command]
pub async fn discover() -> Vec<Found> {
    tauri::async_runtime::spawn_blocking(scan)
        .await
        .unwrap_or_default()
}

/// Checks one address, so a manually typed IP is treated like a scanned one.
#[tauri::command]
pub async fn probe(ip: String) -> Found {
    let parsed: Option<Ipv4Addr> = ip.parse().ok();
    match parsed {
        Some(address) => tauri::async_runtime::spawn_blocking(move || probe_host(address))
            .await
            .unwrap_or(Found {
                ip,
                loader: false,
                api: false,
            }),
        None => Found {
            ip,
            loader: false,
            api: false,
        },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn local_networks_are_not_loopback() {
        for network in local_networks() {
            assert!(!network.is_loopback());
            assert_eq!(network.octets()[3], 0);
        }
    }

    #[test]
    fn probing_loopback_returns_its_address() {
        let found = probe_host(Ipv4Addr::LOCALHOST);
        assert_eq!(found.ip, "127.0.0.1");
    }

    /// Probes the real network. Ignored by default because it depends on the LAN; run it
    /// with `cargo test -- --ignored --nocapture`.
    #[test]
    #[ignore = "probes the local network"]
    fn scan_returns_only_open_hosts() {
        let found = scan();
        for host in &found {
            eprintln!("{} loader={} api={}", host.ip, host.loader, host.api);
            assert!(host.loader || host.api);
        }
    }
}
