// SPDX-License-Identifier: GPL-3.0-or-later
fn main() {
    // cargo can start before Tauri's BeforeDevCommand finishes, so include_bytes! would fail
    // with a bare ENOENT. Fail early with an instruction instead.
    let root = std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR");
    let payload = std::path::PathBuf::from(root).join("../../build/y2jb-installer.elf");
    if !payload.exists() {
        panic!(
            "the payload ELF is missing: {}\n\
             run `make payload` first, or use `npm run dev` from app/ (which builds it first).",
            payload.display()
        );
    }
    println!("cargo:rerun-if-changed={}", payload.display());

    tauri_build::build()
}
