# ClassicSetup Handoff

## Current milestone

- M11 GTK background-shell / foreground-dialog visual refinement completed.
- Recommended enters GTK at Disk; Advanced enters the shared GTK flow at Network after non-destructive storage planning.
- M7/M8 safety, disk policy, storage planning/execution, NetworkManager, source discovery, download verification, and workspace cleanup are unchanged.
- Final Install still does not invoke partition, format, image, or boot executors.

## Changed files

- `src/gui/gtk_frontend.c`: separated setup background and foreground dialog, refreshed progress/sidebar and Network page layout, changed Network-page Next gate.
- `src/gui/classicsetup.css`: added explicit background, progress, dialog, status, and network list styling.
- `include/classicsetup/network.h`, `src/network/network.c`: added a read-only established-connection predicate while retaining Internet readiness separately.
- `include/classicsetup/gui.h`, `src/gui/gui.c`: added GTK-independent Ethernet presentation rows with multi-entry capacity.
- `tests/network_test.c`, `tests/gui_test.c`: covered local connection gating and multi-Ethernet presentation.
- `.ai/handoff.md`: refreshed current snapshot.

## Implementation result

- The main window is now an XP-inspired blue installation background with branding and progress on the left.
- A centered, bounded light-gray foreground setup dialog owns the page stack, caption, content, and Back/Next footer.
- All Disk, Network, Version, Download, Options, and Summary pages reuse that dialog; only stack content changes.
- Progress rows use explicit pending/current/done CSS states with gray, amber, and green indicators. Advanced shows Disk preparation as already complete.
- The dialog remains bounded on large displays; its content is scrollable and wrapped, while the background expands with the window.
- Network presents wired connections and wireless networks as separate bordered lists. Linux interface names remain hidden.
- The Ethernet presentation model accepts multiple display rows; the current aggregate backend maps to `Local Area Connection` and can expand later without changing widgets.
- No adapter messages are `No wired network adapter was detected.` and `No wireless adapter was detected.`
- Network status distinguishes established local connection from verified Internet reachability.
- Network-page Next is enabled when Ethernet or Wi-Fi is actually connected, even when Internet is not yet verified. Disconnected/adapters-only states remain disabled.
- Existing `classicsetup_network_can_continue()` still means Internet reachable and remains the Summary/download readiness requirement.
- No ISO download, network connection mutation, or destructive disk operation was run.

## Build/test result

- Clean Debug GTK ON build: success; GTK4 and download support enabled; warning 0 under C17 `-Wall -Wextra -Wpedantic`.
- Clean Debug GTK OFF/stub build: success; warning 0.
- CTest: 15/15 passed for both GTK ON and GTK OFF builds.
- ASan/UBSan GTK-OFF build: 15/15 passed with leak detection disabled because LeakSanitizer cannot run under this environment's ptrace restrictions.
- Added checks cover Ethernet connected, Wi-Fi connected, both disconnected, no Wi-Fi plus Ethernet, connected-without-Internet, and multiple Ethernet presentation rows.
- TUI-to-GTK smoke reached the GUI with no GTK CSS parser/widget initialization warning; the VM still emitted its existing EGL/Mesa software-renderer warnings.

## Topics for ChatGPT to explain

- Background installation shell versus foreground wizard dialog ownership.
- Deriving progress indicators from GUI session state without copying backend logic.
- Local network connection versus Internet reachability and why their gates differ.
- GTK-independent presentation models and future multiple-adapter support.
- Responsive expansion of the background while keeping a bounded, scrollable dialog.
- Why final download/Summary readiness still requires verified Internet access.

## Issues/cautions

- NetworkManager currently exposes aggregate Ethernet availability/connection facts, so the GUI model is multi-entry ready but currently receives one generic wired row.
- Actual desktop rendering varies with GTK theme, font, compositor, and VMware graphics; visual inspection in the target VM is still recommended.
- Internet verification may remain pending when Network Next becomes available; source discovery/download must still fail closed if Internet is unavailable.
- Installation Options, Windows image apply, final disk transaction, and boot configuration remain unimplemented.
