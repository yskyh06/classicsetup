# ClassicSetup Handoff

## Current milestone

- M12 workspace-root refinement for tmpfs environments.
- The GTK UUP pipeline remains connected and preferred for the validated automatic Windows
  source; final disk/image apply is still disconnected.
- M7/M8 destructive safety, Recommended/Advanced disk policy, NetworkManager, source
  discovery, payload verification, and ISO/WIM verification were not changed.

## Changed files

- `include/classicsetup/workspace.h`: workspace creation result/diagnostic types, selected
  base path, reserve-aware creation, capacity, and diagnostic APIs.
- `src/source/workspace.c`: configurable root selection, private child validation, selected
  filesystem capacity checks, and cleanup path hardening.
- `include/classicsetup/uup.h`, `src/source/uup.c`: shared 24 GiB reserve and workspace
  diagnostic state; the pipeline rechecks the filesystem containing the actual workspace.
- `src/gui/gtk_frontend.c`: reserve-aware workspace creation, unchanged user-facing
  out-of-space message, and debug-only sanitized capacity diagnostics.
- `CMakeLists.txt`: optional `CLASSICSETUP_WORKSPACE_ROOT` configure-time staging root.
- `tests/source_test.c`: override, capacity, symlink, relative-path, and cleanup-boundary
  regressions.

## Implementation result

- Runtime root precedence is:
  1. non-empty `CLASSICSETUP_WORKSPACE_ROOT` environment override;
  2. non-empty CMake `CLASSICSETUP_WORKSPACE_ROOT` configured staging directory;
  3. `/var/tmp`;
  4. `/tmp` as the final fallback.
- `/var/tmp` is therefore preferred over `/tmp` for multi-GiB artifacts. This fixes the
  observed Ubuntu VM case where `/tmp` is a roughly 2.7 GiB tmpfs while the persistent root
  filesystem has about 50 GiB free.
- An explicit/configured root must be absolute, must be a real directory rather than a
  symlink, and must be writable/searchable. Invalid explicit roots fail closed instead of
  silently redirecting files elsewhere.
- Under the selected base, ClassicSetup creates or validates a private
  `classicsetup-<effective-uid>` directory owned by the process user with exact mode 0700,
  then creates a unique `workspace-XXXXXX` child with mode 0700.
- The workspace manager records both the selected base and unique root. Artifact cleanup
  remains scoped to validated paths beneath that unique workspace and rejects empty, `.` or
  `..` path components; it does not recursively remove the selected base or private parent.
- `statvfs()` is run on the selected private parent before creation and again on the actual
  unique workspace. The UUP pipeline repeats the check on its actual workspace path before
  downloading. It no longer consults `/tmp` when another filesystem owns the workspace.
- The conservative reserve remains 24 GiB (`CLASSICSETUP_UUP_WORKSPACE_RESERVE_BYTES`).
  Insufficient capacity still presents `There is not enough temporary storage.`
- Debug builds may report only the selected workspace root, available bytes/GiB, and required
  bytes/GiB. No URL token, cookie, credential, or unrelated path is included.
- No user home path is hardcoded. A production image may set the CMake staging root; local
  development can use, for example, an absolute persistent path through the environment.
- The existing UUP lifecycle remains unchanged: partial/conversion artifacts are
  workspace-owned; verified source retention and cancellation/failure cleanup keep their
  established boundaries.

## Build/test result

- Clean Debug GTK ON + UUP ON: build succeeds with C17 and no
  `-Wall -Wextra -Wpedantic` warnings; CTest 16/16 passes.
- Clean Debug GTK OFF + UUP ON: build succeeds with no warnings; CTest 16/16 passes.
- ASan+UBSan GTK OFF + UUP ON: CTest 16/16 passes with leak detection disabled because
  LeakSanitizer cannot operate under this environment's ptrace supervision.
- Tests cover a small-capacity candidate versus a large alternate root, selected-root
  insufficient capacity, relative override rejection, symlink-root rejection, 0700 private
  ownership, diagnostic formatting, and cleanup escape prevention.
- `git diff --check` passes. No ISO/UUP payload download or destructive disk operation ran.

## Topics for ChatGPT to explain

- Why `/tmp` may be tmpfs and why `/var/tmp` is a safer default for large installer staging.
- Environment/config/default precedence and why invalid explicit configuration fails closed.
- `statvfs()` capacity checks on the filesystem that will actually own an artifact.
- Secure temporary-directory construction with absolute paths, ownership, 0700 permissions,
  symlink rejection, and unique `mkdtemp()` children.
- Workspace ownership boundaries and why recursive cleanup must never escape the unique root.
- Difference between user-facing storage errors and sanitized debug capacity diagnostics.

## Issues/cautions

- The 24 GiB gate is intentionally conservative and may need measurement-based refinement
  for future direct-WIM or target-disk staging paths.
- Intermediate ancestor symlink policy is delegated to the administrator-selected absolute
  base; the selected base itself and ClassicSetup-owned child are validated and fail closed.
- `/var/tmp` and `/tmp` fallback availability still depends on runtime permissions. If neither
  is safe/usable, workspace creation fails rather than weakening ownership checks.
- Actual GTK UUP multi-GiB transfer was not repeated for this refinement.
- Direct WIM, final image apply, disk transaction, EFI/BCD, and boot configuration remain
  future work; existing UUPMediaCreator/libwim rootfs ABI cautions remain applicable.
