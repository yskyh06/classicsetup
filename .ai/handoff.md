# Current milestone

Fido build-tree resolver asset staging.

# Changed files

- `CMakeLists.txt`: stages the pinned script and license through an unconditional
  `ALL` target attached to `classicsetup`.
- `src/source/retail_fido.c`: retains executable-relative build/install lookup.
- `tests/fido_staging_test.sh`: verifies the real build-tree script, license,
  pinned hash, and exact source/staged contents.

# Implementation result

- Source assets are `${CMAKE_SOURCE_DIR}/tools/fido/fido-linux.ps1` and
  `LICENSE.txt`.
- Build assets are under
  `${CMAKE_BINARY_DIR}/lib/classicsetup/tools/fido/`.
- `classicsetup_fido_asset` creates the directory, uses `copy_if_different` for
  both files, depends on both source files, is in `ALL`, and is a direct
  dependency of `classicsetup` and the source test.
- The target is not conditional on GTK, WebKit, UUP, installation, or testing.
- The VM screenshot uses a separate checkout from this workspace. These CMake
  edits are currently uncommitted, so that checkout cannot execute the staging
  rule until the changes are synchronized there.
- Runtime lookup remains executable-relative and has no cwd/source-tree fallback.

# Build/test result

- Clean GTK configure/build: passed; staging target visibly executed.
- `find build -iname fido-linux.ps1` printed
  `build/lib/classicsetup/tools/fido/fido-linux.ps1`.
- Script and `LICENSE.txt` readability/content checks: passed.
- Script SHA-256:
  `cfd466e79ae8c687a09447249534a99903ed5c52ce3b0cc4fb71475058ab66f7`.
- Source timestamp update caused the staging command to rerun.
- CTest: 17/17 passed.
- `git diff --check`: passed.

# Topics for ChatGPT to explain

- Build-target asset staging and synchronizing changes between separate checkouts.

# Issues/cautions

- The Ubuntu VM checkout must receive the modified CMake/test/runtime files
  before repeating its clean build.
- No Fido protocol, Microsoft request, WebView, downloader, or storage behavior
  was changed.
