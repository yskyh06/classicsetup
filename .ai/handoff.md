# ClassicSetup Handoff

## Current milestone

- M12: the manually live-validated Microsoft UUP route is connected to the shared GTK
  Windows-source flow and is the preferred automatic source when UUP support is built.
- Recommended starts with Microsoft Windows Update/UUP without exposing UUPMediaCreator
  internals. Advanced joins the same GTK flow after non-destructive storage planning.
- Final Install remains disconnected. No partition, format, WIM-to-target, EFI, BCD, or
  reboot operation was invoked; M7/M8 safety and disk policy are unchanged.

## Changed files

- `include/classicsetup/{uup,windows_source,workspace,gui,process}.h`: UUP stages/errors,
  target/runtime model, edition and verified-source metadata, payload/workspace ownership,
  and cancellable process environment API.
- `src/source/uup.c`: validated Retail target, shell-free UUP download/conversion pipeline,
  payload artifact checks, ISO boot sanity, ISO image extraction, WIM verification, and
  actual-image metadata registration.
- `src/source/workspace.c`: private UUP/image directories and success/cancel/failure cleanup.
- `src/core/process.c`: bounded process-group cancellation, optional explicit environment,
  and rolling output-tail capture for truthful completion-marker validation.
- `src/gui/{gui,gtk_frontend}.c`: UUP default policy, async staged worker, status/progress,
  cancel/retry/back integration, edition display, and verified-image Summary.
- `CMakeLists.txt`, `packaging/uupmediacreator.manifest`: optional UUP build, pinned runtime
  paths/metadata, configurable DOTNET_ROOT/converter/libwim/7-Zip, and packaging seam.
- `tests/{uup,gui}_test.c`: UUP model/argv/payload/ISO/WIM/runtime/process and GUI policy tests.

## Implementation result

- Default automatic model: Windows 11, Korean (`ko-KR`), x64 (`amd64`), Professional,
  using the live-validated Retail request identity `10.0.22631.1`, Retail/Retail,
  `ni_release`. This is extensible, but only this validated combination is currently
  advertised; Windows 10/other combinations fail closed.
- GTK stages are truthful: CHECKING_TOOL, SEARCHING, RESOLVING, DOWNLOADING,
  VERIFYING_PAYLOAD, BUILDING_IMAGE, VERIFYING_IMAGE, COMPLETE/FAILED/CANCELLED. Unknown
  upstream progress is indeterminate; payload file/byte totals appear only after inspection.
- The worker invokes pinned UUPDownload `request-download`, never a shell, UUP Dump,
  runtime GitHub bootstrap, mirror, or arbitrary URL. An explicit CMake DOTNET_ROOT may be
  used for development; otherwise a valid inherited value or system apphost discovery is
  used. No developer home path is compiled by default.
- Download success requires exit 0 plus the retained completion marker. The payload must
  have one metadata-qualified build root, replay and Professional metadata, sane file count
  and size, no zero-byte files, and no `.part`/`.tmp` residue.
- UUPMediaConverter receives that exact validated payload root and a workspace-owned ISO
  destination. Exit 0 is insufficient: `[Done]`, regular ISO, ISO9660 PVD, and El Torito
  boot descriptor are required.
- Verification avoids the old validation-only mkisofs interception and privileged mounts.
  Configured `7z` reads ISO/UDF and extracts only `sources/install.wim` or `install.esd`;
  `wimlib-imagex verify/info` then requires Windows 11 Pro, x86_64, ko-KR, image count >= 1.
- The backend-neutral verified-source object owns backend=UUP, kind=ISO, verified path,
  edition/index, language/architecture, and actual WIM build. Summary uses this object, so
  a 26200 offer producing base WIM 26100.1 is shown as 26100.1, never as integrated 26200.
- Success retains only the verified ISO and cleans UUP, extracted WIM, and conversion
  intermediates. Failure/cancel cleans workspace-owned partials only. Cancel sends SIGTERM
  to the process group, waits a bounded interval, then SIGKILLs if needed; retry/exit waits
  for worker completion.
- Backends remain Microsoft UUP, fail-closed Microsoft Retail ISO, and future Existing ISO.
  UUP-disabled builds default to Retail and reject unavailable UUP selection.
- ISO production remains behind the verified-source boundary, leaving a future audited
  direct-WIM producer possible without GTK changes. No temporary-WIM race hack was added.

- LIVE VERIFIED MANUALLY BEFORE M12: pinned OSTooling/UUPMediaCreator v3.1.9.3, commit
  `e0c4ce00dc5415bb0441e599aa9a86a2f6021707`, archive SHA-256
  `4a73e28321d893e4fed5f0e774702722995930e4864c6965e78e586d19803ce9`;
  Retail discovery, 384/384 payload, Korean amd64 Professional, bootable ISO, and WIM
  verify/info succeeded. Actual WIM was Windows 11 Pro ko-KR x86_64 build 26100.1.
- LIVE VERIFIED THROUGH CLASSICSETUP GTK: not performed in M12; Codex did not repeat the
  multi-GB transfer. The real GTK worker path is connected and fixture-tested.

- Manual GTK validation: provide GTK4, .NET 8, CA certificates, `wimtools`, and
  `p7zip-full`; configure `CLASSICSETUP_UUP_TOOL_ROOT` to the hash-verified bundle,
  `CLASSICSETUP_UUP_CONVERTER_ROOT` to the validated converter runtime, and optionally
  `CLASSICSETUP_UUP_DOTNET_ROOT` for development. Set the verifier to
  `/usr/bin/wimlib-imagex` and extractor to `/usr/bin/7z`.
- Launch ClassicSetup normally, enter GTK, confirm Network, select validated Windows
  11/Korean/x64/Pro, and start Download. Observe each stage; optionally Cancel, wait for
  Retry, then retry. A full run must reach VERIFYING_IMAGE and COMPLETE, and Summary must
  show actual image metadata. Do not press final Install; it remains a placeholder.

## Build/test result

- Clean Debug GTK ON + UUP ON: C17 build succeeds without `-Wall -Wextra -Wpedantic`
  warnings; CTest 16/16 passes.
- Clean Debug GTK OFF + UUP ON: build succeeds; CTest 16/16 passes.
- Clean Debug GTK ON + UUP OFF: build succeeds without UUP warnings; CTest 16/16 passes.
- ASan+UBSan GTK OFF/UUP ON: build succeeds; CTest 16/16 passes with leak detection disabled.
- `git diff --check` passes. Tests use sparse/local fixtures and mock tools; no Microsoft
  payload, destructive executor, or target-disk operation was run.

## Topics for ChatGPT to explain

- GTK worker/main-context ownership and why UUP subprocesses cannot run on the UI thread.
- Artifact verification versus trusting exit status or a requested update label.
- Reporting identity, offered update metadata, and authoritative WIM metadata.
- ISO9660/UDF extraction with 7-Zip versus privileged loop mounting.
- Process groups, graceful cancellation, bounded kill escalation, join, and clean retry.
- Backend-neutral verified source and the ISO-producer/direct-WIM producer seam.
- DOTNET_ROOT configuration and native SONAME/ABI pinning in a reproducible rootfs.

## Issues/cautions

- `p7zip-full`/`7z` is required for unprivileged ISO/UDF extraction and was not installed in
  the Codex environment; extraction was tested with a deterministic mock. The final rootfs
  must pin and validate it.
- The live WSL overlay used system libwim because bundled libwim expects `libfuse3.so.3`
  while WSL has `.so.4`. Production must bundle matching `.so.3`, rebuild/pin libwim, or
  formally pin a compatible rootfs libwim; arbitrary ABI substitution is prohibited.
- v3.1.9.3 does not integrate downloaded KBs. The validated output is base 26100.1 even
  when Microsoft offers a 26200 update.
- Existing ISO UI, direct WIM production, image apply, final disk transaction, and boot
  configuration remain unimplemented. Retail Sentinel rejection remains fail-closed.
