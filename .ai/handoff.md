# Current milestone

Local Windows ISO discovery, selection, and read-only verification.

# Changed files

- `include/classicsetup/{gui,local_iso,retail}.h`, `src/source/{local_iso,retail_fido,retail_stub}.c`: local ISO catalog and path-based metadata inspection.
- `src/gui/{gui,gtk_frontend}.c`: existing-ISO selection, refresh, verification, readiness, and summary state.
- `CMakeLists.txt`, `.gitignore`, `iso/README.md`: build wiring and repository ISO folder.
- `tests/{gui,source}_test.c`: catalog, selection gate, and original-file preservation tests.

# Implementation result

- `build/classicsetup` resolves the repository `iso/` directory relative to its executable; installed builds use the configured shared-data directory. `CLASSICSETUP_ISO_DIRECTORY` is an absolute-path override.
- The source page lists up to 32 regular `.iso` files case-insensitively, sorted by name, with a refresh action. Symlinks and non-ISO files are ignored.
- The selected file is not downloaded, copied, renamed, or deleted. Existing ISO verification reads it directly and uses a separate temporary workspace only for WIM/ESD extraction.
- ISO filesystem, Windows family, Korean/English language, and x64 metadata must match the selected options before install readiness is granted.
- Fido, WebView, libcurl download behavior, and destructive storage/install paths were not changed.

# Build/test result

- GTK ON build: passed with zero project warnings; CTest 17/17 passed.
- GTK OFF build: passed; CTest 17/17 passed.
- Download backend OFF/stub build: passed; CTest 17/17 passed.
- `git diff --check`: passed. No live Microsoft request or real ISO download was performed.

# Topics for ChatGPT to explain

- Place ISO files directly in `classicsetup/iso`, choose **Use existing ISO**, select a file, then use **Use Selected ISO** to verify it.
- Local ISO readiness depends on metadata matching the chosen Windows version, language, and x64 architecture.

# Issues/cautions

- Discovery is intentionally non-recursive and limited to 32 regular files in `iso/`.
- A real Windows ISO still needs one manual GTK validation with installed `7z` and `wimlib-imagex`.
- Existing ISO originals remain user-owned and are never removed by ClassicSetup cleanup.
