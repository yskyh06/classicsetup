# Current milestone

M16-WEB-RETAIL prototype: conditionally embed the official Microsoft Windows
11 page in GTK, capture the user's real Microsoft ISO download click, and hand
the ephemeral URI to the existing ClassicSetup libcurl pipeline.

# Changed files

- `CMakeLists.txt`: added optional `CLASSICSETUP_ENABLE_WEBKIT_RETAIL` and
  `webkitgtk-6.0` detection/linking only for an enabled GTK frontend.
- `include/classicsetup/retail_browser.h`: browser stages, safe URI policy,
  capture, fallback, and URI-clearing API.
- `src/source/retail_browser.c`: narrow Microsoft navigation/download policy
  and backend-neutral browser state machine.
- `include/classicsetup/gui.h`, `src/gui/gui.c`: browser state in the GUI
  session and reset/discard lifecycle.
- `src/gui/gtk_frontend.c`: ephemeral WebKit view, ordinary page preparation,
  real-link capture, WebKit cancellation, and existing downloader handoff.
- `tests/gui_test.c`: state, URI allow/reject, capture/clear, and full-page
  fallback coverage.

# Implementation result

- WebKitGTK is GTK-only and optional. GTK OFF and dependency-missing builds use
  the existing frontend/stub paths without WebKit headers or linkage.
- Ubuntu 26.04 provides GTK4-compatible `libwebkitgtk-6.0-dev`, pkg-config
  module `webkitgtk-6.0`, version `2.52.3`.
- The official page is
  `https://www.microsoft.com/ko-kr/software-download/windows11`.
- A live page-source check on 2026-08-30 confirmed current IDs:
  `product-edition`, `submit-product-edition`, `product-languages`,
  `submit-sku`, and `SoftwareDownload_DownloadLinks`.
- Automation selects only a page-provided x64 edition and a page-provided
  Korean/ko-KR language, then clicks Microsoft's normal confirmation controls.
  It does not supply product/SKU IDs or interact with challenges.
- If a challenge, missing DOM, script failure, or timeout is detected, the
  complete Microsoft page remains visible for manual use.
- Once the final area exists, CSS focuses the original Microsoft container;
  no ClassicSetup replacement download button is created.
- An ephemeral `WebKitNetworkSession` is used with persistent credential
  storage disabled. No browser cookies/history/download database are written
  to the ClassicSetup workspace.
- Both WebKit `download-started` and navigation policy paths are handled.
  WebKit's transfer is cancelled/ignored after accepting the real click.
- Accepted delivery URIs must be absolute HTTPS, end in `.iso`, and use exactly
  `software.download.prss.microsoft.com`. Unexpected top-level navigation is
  blocked; arbitrary Microsoft subdomains are not accepted.
- The signed URI is memory-only, never logged or stored in handoff, is wiped
  from temporary release objects, and is re-obtained after Retry.
- The URI is copied into the existing asynchronous libcurl download request;
  `windows.iso.part`, free-space checks, TLS, progress/cancel, ISO/image
  inspection, promotion, and cleanup remain owned by existing modules.
- If the page exposes the official Korean SHA-256, it is captured and passed to
  the existing verifier. Without it, no hash-verification claim is made.
- Browser stages cover page preparation/waiting, user click, download,
  ISO verification, image inspection, completion, failure, and cancellation.
- Direct package footprint is about 122 MiB for WebKitGTK and JavaScriptCore
  runtime libraries, before their supporting runtime dependencies; final
  rootfs sizing remains pending.
- Fido and msdl are retained unchanged, Mido remains rejected, and UUP remains
  frozen. No destructive/storage/install execution code was modified or run.

# Build/test result

- GTK ON + WebKit Retail ON + UUP ON: built with the exact Ubuntu 2.52.3
  packages extracted into an isolated `/tmp` SDK; no package was installed.
- GTK ON + WebKit Retail OFF + UUP OFF: built.
- GTK OFF + WebKit Retail request ON + UUP ON: built with WebKit cleanly
  disabled because the GTK frontend is unavailable.
- C17 with `-Wall -Wextra -Wpedantic`: zero project warnings.
- CTest: 17/17 passed in all three normal build configurations.
- ASan/UBSan: 16/16 applicable tests passed with leak detection disabled for
  the ptrace-constrained environment; the UUP self-execution test was excluded.
- `git diff --check`: passed.
- The Microsoft public page and DOM were fetched successfully, but WebKitGTK is
  not installed in this WSL host and no Ubuntu Desktop GUI live run occurred.
- Therefore automatic preparation, final-control visibility, user-click
  capture, and libcurl transfer start are implemented but not live verified.

# Topics for ChatGPT to explain

- WebKitGTK network-session privacy and GTK main-loop ownership.
- `decide-policy` versus `download-started` and why both capture paths exist.
- Fail-open-to-full-page DOM automation without challenge bypasses.
- Narrow signed-URI policy, in-memory lifetime, and redaction boundaries.
- Existing libcurl downloader reuse and minimal ISO/WIM verification.
- Conditional dependency detection and expected rootfs size impact.

# Issues/cautions

- Install `libwebkitgtk-6.0-dev` on the Ubuntu Desktop validation VM; without
  pkg-config module `webkitgtk-6.0`, the feature intentionally compiles out.
- The M16 success gate is not yet live verified through GTK. Manual test:
  launch ClassicSetup on Ubuntu Desktop, reach Download, open the Microsoft
  page, confirm Windows 11/Korean/x64, click Microsoft's real x64 control,
  verify WebKit disappears and libcurl starts, then Cancel promptly.
- Microsoft can change its DOM or present a consent/challenge. The safe result
  is the full official page, never guessed selectors or security automation.
- Only `software.download.prss.microsoft.com` is approved for ISO delivery.
  Any newly observed host requires explicit review before code changes.
- Existing ISO UI remains disabled because that fallback is not implemented.
- No full ISO was downloaded; ISO/WIM verification was not live exercised.
- Final Install, partition/format apply, WIM apply, EFI, BCD, and reboot remain
  disconnected and untouched.
