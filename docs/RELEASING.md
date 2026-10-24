# Releasing

1. Bump the version and update the manifests:

   ```sh
   echo 1.1.0 > VERSION
   node scripts/version.mjs --write
   node scripts/version.mjs --check
   ```

2. Commit and tag. The tag must be `v` plus the exact `VERSION`:

   ```sh
   git add -A
   git commit -m "Release 1.1.0"
   git tag v1.1.0
   git push origin master --tags
   ```

3. Configure the Android signing secrets once in the repository settings:

   - `ANDROID_KEYSTORE_BASE64`
   - `ANDROID_STORE_PASSWORD`
   - `ANDROID_KEY_ALIAS`
   - `ANDROID_KEY_PASSWORD`

4. CI runs one `Release` workflow with a single payload build and platform jobs:

   | Job | Artifact |
   |---|---|
   | `payload` | Shared PS5 payload ELF |
   | `macos` | Universal DMG |
   | `linux` | AppImage and AppImage `.zsync` for amd64 and arm64 |
   | `windows` | Windows x64 ZIP |
   | `android` | Signed universal APK |

5. Check the draft release:

   - All platform artifacts and AppImage `.zsync` files are attached.
   - The application reports the expected version.
   - The DMG, Windows ZIP, Linux packages, and Android APK launch and connect to a console.
   - AppImageUpdate can discover the latest AppImage from the GitHub release.

6. Publish the release.

Desktop artifacts are unsigned (macOS is ad-hoc signed and not notarized). The Android APK
is signed with a self-signed release key. See the unsigned-build notice in
[README](../README.md).

## Refreshing an existing release

To rebuild and replace assets on an existing release without moving its tag, run the
`Release` workflow manually from `master` and set `release_tag` to the existing tag, for
example `v1.1.0`. The workflow builds from `master`, verifies that its `VERSION` matches
the requested tag, and uploads with `--clobber`.

This is intended for CI-only fixes. Use a new patch release when application code
changes. Remove obsolete assets manually; `--clobber` replaces only files with the same
name.
