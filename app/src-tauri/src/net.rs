// SPDX-License-Identifier: GPL-3.0-or-later
use std::net::{SocketAddr, TcpStream};
use std::time::{Duration, Instant};

/// How long a single connection attempt may take before it is retried.
const ATTEMPT: Duration = Duration::from_secs(3);
/// Pause between attempts.
const PAUSE: Duration = Duration::from_millis(500);

/// Connects to the console, retrying for up to `total`.
///
/// A jailbroken console periodically stops accepting TCP connections for 10–20 s at a time
/// (every process on it, not just the payload: elfldr on 9021 goes quiet in the same
/// windows while ICMP keeps answering). A single 5 s connect therefore fails often enough
/// to look like a lost console. Retrying across such a window makes the operation succeed
/// instead of asking the user to try again.
pub fn connect_retry(addr: &SocketAddr, total: Duration) -> std::io::Result<TcpStream> {
    let deadline = Instant::now() + total;
    loop {
        match TcpStream::connect_timeout(addr, ATTEMPT) {
            Ok(stream) => return Ok(stream),
            Err(error) => {
                if Instant::now() + PAUSE >= deadline {
                    return Err(error);
                }
                std::thread::sleep(PAUSE);
            }
        }
    }
}
