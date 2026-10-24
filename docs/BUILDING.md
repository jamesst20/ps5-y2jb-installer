# Building

## Prerequisites

- Git with submodules
- Node.js 24
- Rust stable
- GNU Make and a POSIX shell
- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) with clang/lld 18
- [Tauri prerequisites](https://v2.tauri.app/start/prerequisites/) for your platform

Install the SDK:

```sh
curl -L -o sdk.zip https://github.com/ps5-payload-dev/sdk/releases/latest/download/ps5-payload-sdk.zip
sudo unzip -q sdk.zip -d /opt
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
```

### Platform packages

**macOS**

```sh
xcode-select --install
brew install llvm@18
export LLVM_CONFIG=$(brew --prefix llvm@18)/bin/llvm-config
rustup target add aarch64-apple-darwin x86_64-apple-darwin
```

**Linux (Debian/Ubuntu)**

```sh
sudo apt-get install -y build-essential curl wget file unzip patchelf tcl gnupg zsync \
  libwebkit2gtk-4.1-dev libappindicator3-dev librsvg2-dev
```

The SDK needs clang/lld 18:

```sh
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key |
  sudo gpg --dearmor -o /usr/share/keyrings/llvm.gpg
. /etc/os-release
echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/${VERSION_CODENAME}/ llvm-toolchain-${VERSION_CODENAME}-18 main" |
  sudo tee /etc/apt/sources.list.d/llvm.list
sudo apt-get update && sudo apt-get install -y clang-18 lld-18 llvm-18
export LLVM_CONFIG=/usr/lib/llvm-18/bin/llvm-config
```

**Windows (WSL2)**

The packaged application builds natively with MSVC, but the payload SDK targets POSIX
hosts, so the payload build runs in WSL2. WSL is not required by the packaged application.

```powershell
wsl --install -d Ubuntu-22.04
```

Inside Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y build-essential curl wget file unzip tar tcl gnupg ca-certificates
wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key |
  sudo gpg --dearmor -o /usr/share/keyrings/llvm.gpg
. /etc/os-release
echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/${VERSION_CODENAME}/ llvm-toolchain-${VERSION_CODENAME}-18 main" |
  sudo tee /etc/apt/sources.list.d/llvm.list
sudo apt-get update
sudo apt-get install -y clang-18 lld-18 llvm-18
```

## Build

```sh
git submodule update --init --recursive
make test                 # host tests; no SDK required
scripts/build_deps.sh     # libmicrohttpd and SQLite
make payload              # build/y2jb-installer.elf
make app                  # UI, payload, and desktop application
```

## Development

```sh
cd app
npm run dev
```

## Release builds

**macOS universal DMG**

```sh
cd app
CI=true npx tauri build --target universal-apple-darwin --bundles dmg
```

**Linux AppImage and deb**

```sh
cd app
npx tauri build --bundles appimage,deb
cd ..
scripts/appimage-update.sh x86_64 amd64   # use aarch64/aarch64 on arm64
```

`scripts/appimage-update.sh` repacks the AppImage with GitHub Releases update
information and creates the matching `.zsync` file. It is also run by the Linux
release workflow.

**Windows executable**

```sh
cd app
npx tauri build --no-bundle
```

The executable is `app/src-tauri/target/release/y2jb-installer.exe`. Package it with
`packaging/windows/README.txt`.
