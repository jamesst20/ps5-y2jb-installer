# Releasing

1. Bump the version and update the manifests:

   ```sh
   echo 1.0.1 > VERSION
   node scripts/version.mjs --write
   node scripts/version.mjs --check
   ```

2. Commit and tag. The tag must be `v` plus the exact `VERSION`:

   ```sh
   git add -A
   git commit -m "Release 1.0.1"
   git tag v1.0.1
   git push origin master --tags
   ```

3. CI runs the three release workflows:

   | Workflow | Artifact |
   |---|---|
   | `build-macos.yml` | Universal DMG |
   | `build-linux.yml` | AppImage and `.deb` for amd64 and arm64 |
   | `build-windows.yml` | Windows x64 ZIP |

4. Check the draft release:

   - All platform artifacts and `SHA256SUMS-*` files are attached.
   - Checksums match the artifacts.
   - The application reports the expected version.
   - The DMG, Windows ZIP, and Linux packages launch and connect to a console.

5. Publish the release.

Artifacts are unsigned (macOS is ad-hoc signed and not notarized). See the unsigned-build
notice in [README](../README.md).
