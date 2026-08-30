# Current milestone

M16-WEB-RETAIL visible XP-style GTK download/source UX. The proven Retail
protocol remains Fido first, followed once by WebKit fallback.

# Changed files

- `include/classicsetup/gui.h`, `src/gui/gui.c`: Retail source mode and
  single-attempt Fido/WebView state.
- `include/classicsetup/retail_browser.h`, `src/source/retail_browser.c`:
  Korean/English page mapping and fail-safe WebView visibility policy.
- `src/source/retail_fido.c`: Korean/English x64 catalog, Fido argv, and image
  language verification.
- `src/gui/gtk_frontend.c`, `src/gui/classicsetup.css`: visible XP-style source,
  preparation, download, and persistent sidebar-progress UI.
- `tests/gui_test.c`, `tests/source_test.c`: focused fallback, visibility,
  language, argv, and metadata coverage.

# Implementation result

- Source selection now uses classic icon/radio/description rows. Automatic
  Microsoft download is selected by default; Existing ISO and Custom remain
  clearly unavailable.
- Automatic mode invokes Fido once. A source-resolution failure starts WebKit
  once; a successful Fido resolution goes directly to the existing libcurl
  downloader.
- WebKit remains hidden during normal edition/language/x64 preparation. It is
  revealed only for Microsoft's real final download control or as a full-page
  manual fallback after automation failure/challenge/timeout.
- Korean (`ko-KR`, Fido `Korean`) and English (`en-US`, Fido `English`) flow
  through selection, page locale, resolver argv, metadata, and verification.
- Download UI now shows separate size, rate, ETA, classic progress/action
  layout, and background-download guidance. Next continues to later
  non-destructive pages without cancelling the worker.
- A persistent left-panel status block follows preparation/download/
  verification/completion/failure while other pages are visible.
- Signed URI policy/lifetime, libcurl pipeline, storage/destructive code,
  Fido protocol, Mido/msdl status, and frozen UUP policy were not redesigned.

# Build/test result

- GTK ON + WebKit Retail ON + UUP ON: built with WebKitGTK 6.0 SDK.
- GTK ON + WebKit Retail OFF + UUP OFF: built.
- GTK OFF + UUP ON: built.
- C17 `-Wall -Wextra -Wpedantic`: zero project warnings.
- CTest: 17/17 passed in all tested configurations.
- `git diff --check`: passed.

# Topics for ChatGPT to explain

- One-shot Fido-to-WebView state transitions and why WebView stays hidden.
- Background download ownership versus wizard-page navigation.
- Korean/English propagation into resolver and verification expectations.

# Issues/cautions

- Current public IP is blocked by Microsoft's consumer ISO service; live
  Retail validation requires a normal network/mobile tethering. Do not treat
  error 715-123130 here as a regression or add bypass logic.
- Manual VM confirmation remains: inspect the redesigned source/download
  pages, both languages, hidden preparation, real-control reveal, persistent
  sidebar progress, and continued transfer after Next.
- Existing ISO and Custom download backends remain intentionally unavailable.
- Full ISO completion and Final Install were not run; destructive execution
  paths were not modified.
