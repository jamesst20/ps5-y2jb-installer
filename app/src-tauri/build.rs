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

    // Android 15+ supports 16 KiB page sizes. NDK r27 and older need explicit ELF
    // alignment flags; without them Android 17 warns and may refuse to load the library.
    let target = std::env::var("TARGET").unwrap_or_default();
    if target.contains("android") {
        println!("cargo:rustc-link-arg=-Wl,-z,max-page-size=16384");
        println!("cargo:rustc-link-arg=-Wl,-z,common-page-size=16384");
    }

    tauri_build::build()
}
