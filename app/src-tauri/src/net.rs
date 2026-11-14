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
///
/// A refusal is not a stall: the console answered, and nothing is listening on the port
/// (elfldr or the payload is not running). Waiting 45 s would only delay a message the user
/// can act on immediately.
pub fn connect_retry(addr: &SocketAddr, total: Duration) -> std::io::Result<TcpStream> {
    let deadline = Instant::now() + total;
    loop {
        match TcpStream::connect_timeout(addr, ATTEMPT) {
            Ok(stream) => return Ok(stream),
            Err(error) => {
                if error.kind() == std::io::ErrorKind::ConnectionRefused
                    || Instant::now() + PAUSE >= deadline
                {
                    return Err(error);
                }
                std::thread::sleep(PAUSE);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;

    #[test]
    fn a_refused_connection_is_not_retried() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let addr = listener.local_addr().unwrap();
        drop(listener);

        let started = Instant::now();
        let error = connect_retry(&addr, Duration::from_secs(30)).unwrap_err();
        assert_eq!(error.kind(), std::io::ErrorKind::ConnectionRefused);
        // Windows can take a couple of seconds to refuse a loopback connection. A retrying
        // implementation would use the whole 30 s budget, so anything well under it proves
        // the refusal was not retried.
        assert!(
            started.elapsed() < Duration::from_secs(10),
            "a refused connection should fail immediately"
        );
    }
}
