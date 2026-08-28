# ClassicSetup Handoff

## Current milestone

- M11.x Windows Source Selection + Real Microsoft ISO Download Integration.
- Recommended and Advanced share the same GTK source-selection/download flow.
- Advanced storage planning remains non-destructive until a future final Install transaction.
- M7/M8 destructive safety, disk policy, NetworkManager behavior, and final executor timing are unchanged.

## Changed files

- `include/classicsetup/windows_source.h`: added x64/x86/ARM64 source architecture model and metadata fields.
- `include/classicsetup/gui.h`: added cascading source selections and source-change lifecycle APIs/state.
- `src/source/windows_source.c`: maps discovered releases, languages, and architectures without inventing unsupported combinations.
- `src/source/microsoft_source_curl.c`: refreshes official catalog/session data before resolving the selected source.
- `src/source/download_curl.c`: refined TLS/source errors and truthful verification-level reporting.
- `src/gui/gui.c`: owns cascading selection validity, invalidation, change requirements, and verified-source discard.
- `src/gui/gtk_frontend.c`: implements separate source controls, re-entry restoration, confirmations, retry, and expanded Summary.
- `src/app.c`: reconstructs source-selection state when restoring a GUI session.
- `tests/source_test.c`, `tests/gui_test.c`: cover architecture discovery, cascading reset, re-entry, cancellation, and cleanup.
- `.ai/handoff.md`: refreshed as the current implementation snapshot.

## Implementation result

- Windows Version now exposes independent Family, Release, Installation language, and Architecture controls.
- Architecture is a typed model: x64, x86 (32-bit), and ARM64; UI labels and backend tokens are separate.
- Release/language/architecture choices are derived only from official discovery candidates. Unsupported combinations are absent and cannot satisfy Next.
- Family changes invalidate release/language/architecture; release changes invalidate language/architecture; language changes invalidate architecture and resolved source.
- Recommended defaults prefer a discovered current release, Korean when available, and a native architecture when available.
- Version -> Download -> Back restores values and recalculates widget sensitivity from the session model, fixing the stale locked-controls bug.
- Before download, selections remain editable and resolved metadata is invalidated when the selection changes.
- During active transfer, changing the source requires `Cancel Download and Change`; cancellation completes before `.part` cleanup and editing resumes.
- With a verified ISO, changing the source requires `Discard and Change`; cleanup is delegated to the workspace manager rather than GTK calling `unlink()`.
- Download-page Back only navigates. It does not cancel the worker, so Options navigation remains independent of transfer lifetime.
- Retry starts cleanly after FAILED/CANCELLED state and does not append to a stale partial file.
- The existing real pipeline is connected unchanged in principle: official resolve -> free-space check -> `windows.iso.part` -> libcurl HTTPS -> flush/fsync -> size/basic ISO/optional SHA-256 verification -> atomic `windows.iso` promotion.
- TLS peer/host verification remains enabled. Unsupported/unofficial/non-HTTPS candidates fail closed, and signed URL query data is not shown.
- Summary separately reports family, release, language, architecture, official source, verification level, target, and unapplied storage plan.
- Live metadata-only discovery on 2026-08-29 returned Windows 11 25H2 x64 and Windows 10 22H2 x64/x86 for English and Korean.
- ARM64 exists in the model but the current consumer discovery backend returned no ARM64 candidate; no guessed URL or alternative endpoint was added.
- The live Microsoft signed-link resolve was rejected/failed in the Codex environment. The backend returned an error and performed no ISO transfer; no anti-abuse workaround was attempted.
- No multi-gigabyte ISO download and no destructive disk operation was executed by Codex.
- Manual VMware path: connect Internet; select Family/Release/Language/Architecture; start Download; inspect progress; Cancel and confirm `.part` cleanup; Retry; observe VERIFYING then COMPLETE; confirm workspace `windows.iso`; verify no signed URL/token is logged.

## Build/test result

- Clean Debug GTK ON + download ON: success, warning 0 under C17 `-Wall -Wextra -Wpedantic`.
- Clean Debug GTK OFF/stub + download ON: success, warning 0.
- Clean Debug GTK ON + download OFF/stub: success, warning 0.
- CTest: 15/15 passed in all three build configurations.
- ASan/UBSan GTK-OFF core/mock build: 15/15 passed; LeakSanitizer disabled for environment compatibility.
- `git diff --check`: passed.
- Tests cover x64/x86/ARM64 metadata, unsupported combinations, cascading resets, preservation/re-entry, active-download change policy, verified ISO discard, URI sanitation, progress/cancel/retry/verify/workspace regressions, and no destructive executor invocation.

## Topics for ChatGPT to explain

- Cascading selection models and stale dependent-state invalidation.
- UI architecture labels versus backend source tokens.
- Official-source discovery, ephemeral signed URLs, and fail-closed integration.
- Async libcurl download ownership versus GTK main-thread updates.
- `.part` files, fsync, verification, and atomic rename promotion.
- Verification levels when an official SHA-256 is unavailable.
- Cancellation/join ordering and workspace-owned cleanup.
- Why edition selection waits for install.wim/install.esd inspection.

## Issues/cautions

- Microsoft consumer endpoints and session requirements can change; current metadata discovery works, but signed-link resolution still needs manual VMware verification.
- Confirmed discovery is Windows 11 x64 and Windows 10 x64/x86; ARM64 download discovery is not integrated yet.
- Official SHA-256 may be absent for a candidate; the UI reports basic verification rather than claiming hash verification.
- Crash-resume remains limited; Retry is a clean restart and cancellation removes the partial artifact.
- Verified ISO is retained until future image apply or explicit source discard/post-install cleanup.
- Edition/WIM inspection, Windows image apply, final disk transaction, and boot configuration remain unimplemented.
