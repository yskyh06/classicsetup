# ClassicSetup Handoff

## Current milestone

- M10.1 Recommended GTK GUI Foundation 완료.
- Recommended 선택 시 별도 GUI 경계로 진입하고, Advanced 선택 시 기존 ncurses 흐름을 유지한다.
- M7/M8 destructive safety와 Recommended의 `RAW_EMPTY + UEFI/GPT` 제한은 변경하지 않았다.

## Changed files

- GUI API/model: `include/classicsetup/gui.h`, `src/gui/gui.c`, `src/gui/session.c`.
- GTK frontend/fallback: `src/gui/gtk_frontend.c`, `src/gui/gtk_stub.c`, `src/gui/classicsetup.css`.
- App/state/policy connection: `src/app.c`, `src/state.c`, `include/classicsetup/recommended.h`, `src/core/recommended.c`.
- Recommended fallback UI: `include/classicsetup/recommended_tui.h`, `src/tui/recommended.c`.
- Build/tests: `CMakeLists.txt`, `tests/state_test.c`, `tests/gui_test.c`.
- Documentation: `.ai/handoff.md`.

## Implementation result

### Current flow

- Common start: `Welcome -> License / Risk Agreement -> Setup Mode`.
- Recommended: `Setup Mode -> RECOMMENDED_GUI_TRANSITION -> GTK GUI Disk -> Network -> Windows Version -> Download -> Options -> Summary`.
- Advanced: `Setup Mode -> Keyboard -> Installation Mode -> Disk -> Partition -> Format -> M7/M8 apply`.
- GUI Summary의 `Install (later)`는 placeholder로만 종료하며 partition/format/image executor를 호출하지 않는다.

### GTK / TUI architecture

- `classicsetup_gui_session`과 page transition 함수는 GTK 타입이 없는 frontend/model 경계다.
- `src/core`에는 GTK include가 없고, GTK 위젯은 `src/gui/gtk_frontend.c`에만 존재한다.
- App은 GUI 진입 전에 `classicsetup_tui_shutdown()`을 호출하고 GUI 반환 후 다시 초기화한다. GTK4가 없거나 초기화 오류면 ncurses fallback에서 Advanced 선택 또는 종료를 안내한다.
- CMake `CLASSICSETUP_ENABLE_GTK`는 기본 ON이며 GTK4/pkg-config가 발견될 때만 GTK frontend와 CSS 경로를 연결한다. 미발견 또는 OFF이면 stub을 사용해 기존 빌드를 유지한다.

### GUI pages

- Disk page는 `classicsetup_scan_disks()`, `classicsetup_assess_disk()`, `classicsetup_recommended_assessment_is_selectable()` 결과를 표시한다.
- 각 항목에 model, capacity, classification/policy presentation, device path와 unavailable 상태를 표시하며 selectable disk만 선택 가능하다.
- Network, Windows Version, Download, Installation Options, Summary는 placeholder page다. Windows 10/11 선택은 session에만 유지한다.
- Back/Next는 명시적인 `classicsetup_gui_page_next/back()`으로 이동하며 Disk의 Back은 GUI를 닫고 Setup Mode로 복귀한다.

### Core reuse and policy

- Recommended selectable 판정은 TUI와 GUI가 공통 `classicsetup_recommended_assessment_is_selectable()`을 호출한다. GUI에 별도 disk safety 판단을 복제하지 않았다.
- `classicsetup_gui_session_init()`은 현재 firmware와 disk assessment snapshot을 준비할 뿐 plan/apply/format을 만들지 않는다.

### Functions added/changed

- `classicsetup_gui_session_reset()` / `classicsetup_gui_session_init()`: page, Windows version, disk assessment session을 초기화하고 기존 Recommended core로 디스크를 읽는다.
- `classicsetup_gui_page_next()` / `classicsetup_gui_page_back()`: six GUI page의 경계 전이를 순수 함수로 처리한다.
- `classicsetup_gui_select_disk()` / `classicsetup_gui_set_windows_version()`: selectable UEFI disk와 placeholder version만 session에 저장한다.
- `classicsetup_gui_run()`: GTK 구현은 `GtkApplication` main loop를 실행하고, stub은 `UNAVAILABLE`을 반환한다.
- `show_recommended_gui()`: ncurses shutdown -> session init -> GUI run -> ncurses reinit 및 app event 변환을 담당한다.
- `classicsetup_recommended_assessment_is_selectable()`: firmware와 기존 assessment policy를 함께 검사하는 공통 helper다.
- `classicsetup_show_recommended_gui_unavailable()`: GTK disabled/error 시 안전하게 Setup Mode로 돌아가는 fallback 화면이다.

## Build/test result

- Debug CMake build 성공: C17, `-Wall -Wextra -Wpedantic`, warning 0.
- GTK4 미설치 환경에서 default(자동 탐색) build는 `GTK4 not found`로 stub을 선택했고 성공했다.
- `CLASSICSETUP_ENABLE_GTK=OFF` build 성공.
- CTest 12/12 통과: 기존 회귀 + GUI page navigation, session reset, selectable disk fail-closed, Windows version state, GTK-disabled stub result 포함.
- ASan/UBSan Debug( GTK OFF ) CTest 12/12 통과.
- PTY에서 Recommended 선택 시 ncurses 종료/재초기화와 GTK-disabled fallback -> B로 Setup Mode 복귀를 확인했다. 실제 GTK 화면은 GTK4 개발 패키지가 없어 실행하지 못했다.
- 실제 destructive disk operation과 partition/format executor 호출은 수행하지 않았다.

## Topics for ChatGPT to explain

- GTK `GtkApplication`과 main loop, activate/close/button signal callback.
- TUI shutdown과 GUI startup 경계 및 GUI 반환 후 terminal 재초기화.
- Core/model/view 분리와 공통 Recommended assessment 재사용.
- GUI page enum 기반 navigation과 callback spaghetti 방지.
- CMake optional GTK4/pkg-config detection 및 stub fallback.
- Recommended GUI가 아직 executor를 호출하지 않는 이유와 향후 async network/download 경계.

## Issues/cautions

- 현재 환경에는 GTK4 개발 패키지가 없어 GTK-enabled compile/runtime은 검증하지 못했다. GTK 설치 후 `-DCLASSICSETUP_ENABLE_GTK=ON` 재검증이 필요하다.
- GUI Disk page는 assessment snapshot을 보여주며 config/recommended apply plan에 아직 연결하지 않는다.
- Network, Windows source/version validation, download, options, WIM/image apply, GTK 전체 설치 UX는 placeholder다.
- GUI 창 닫기는 Setup Mode 복귀로 처리하며 별도 GUI Quit Confirmation/key accelerator는 후속 UX 범위다.
- Existing/partitioned/encrypted disk 보존·erase 정책, BIOS/MBR actual apply, M7/M8 safety 구현은 그대로 제한된다.
