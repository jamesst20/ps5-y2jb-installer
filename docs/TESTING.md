# Testing

Run every check from the repository root:

```sh
make test
node scripts/version.mjs --check
npm --prefix ui run check
npm --prefix ui run build
cargo fmt --manifest-path app/src-tauri/Cargo.toml --all -- --check
cargo clippy --manifest-path app/src-tauri/Cargo.toml --all-targets -- -D warnings
cargo test --manifest-path app/src-tauri/Cargo.toml
```

Network and console tests are opt-in:

```sh
cargo test --manifest-path app/src-tauri/Cargo.toml -- --ignored --nocapture
```

The host and unit tests need no console.
