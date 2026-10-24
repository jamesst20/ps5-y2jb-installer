# Android (experimental)

The Android target is experimental. The full workflow has been exercised on a Pixel 9
running Android 17 (API 37): connecting, sending the payload, downloading and installing
the YouTube PKG, and downloading and uploading `download0.dat`.

The OnePlus 13 on Android 16 (API 36) has also been verified for connecting and sending
the payload. The desktop application remains the primary supported target, but Android is
now included in the release artifacts.

## Requirements

- JDK 21
- Android SDK platform 36 and build tools
- Android NDK `27.2.12479018`
- Rust Android targets:

  ```sh
  rustup target add aarch64-linux-android armv7-linux-androideabi \
    i686-linux-android x86_64-linux-android
  ```

Set the build environment:

```sh
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
export ANDROID_HOME="$HOME/Library/Android/sdk"
export NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
```

## Build and install

For a debug APK, from `app/`:

```sh
./node_modules/.bin/tauri android build --debug --apk --target aarch64
adb install -r src-tauri/gen/android/app/build/outputs/apk/universal/debug/app-universal-debug.apk
```

For a signed universal release APK:

```sh
export Y2JB_ANDROID_KEYSTORE="$HOME/.config/y2jb-installer/android-release/y2jb-release.keystore"
export Y2JB_ANDROID_STORE_PASSWORD="$(cat "$HOME/.config/y2jb-installer/android-release/password.txt")"
export Y2JB_ANDROID_KEY_ALIAS=y2jb
export Y2JB_ANDROID_KEY_PASSWORD="$Y2JB_ANDROID_STORE_PASSWORD"

cd app
npx tauri android build --apk
```

The release APK is written to
`src-tauri/gen/android/app/build/outputs/apk/universal/release/app-universal-release.apk`.

The release build allows cleartext HTTP for the local payload API. Use the app only on a
trusted LAN. The Android release workflow expects these repository secrets:

- `ANDROID_KEYSTORE_BASE64` — base64-encoded release keystore
- `ANDROID_STORE_PASSWORD`
- `ANDROID_KEY_ALIAS`
- `ANDROID_KEY_PASSWORD`

To encode the keystore for the secret:

```sh
base64 -i "$HOME/.config/y2jb-installer/android-release/y2jb-release.keystore" | pbcopy
```

The target SDK is pinned to 36. Android 17 grants legacy local-network access to apps
targeting below 37. If the target is raised to 37, add and request
`android.permission.ACCESS_LOCAL_NETWORK`.

The Rust libraries are linked with 16 KiB ELF page alignment in `src-tauri/build.rs`, so
Android 15+ devices with 16 KiB pages do not show the compatibility warning.

## Current limitations

- Manual IP or the existing `/24` scan; no mDNS.
- Large YouTube and `download0.dat` transfers are not adapted for Android background
  restrictions or storage limits.
- The generated Android project is checked in. Re-running `tauri android init` may
  overwrite the target SDK, signing configuration, cleartext setting, 16 KiB
  configuration, and the local-CLI build task changes.
