# ClassicSetup Handoff

## Current milestone

- M10.2.1 Advanced -> Common GTK Flow Integration 완료.
- GTK는 Recommended 전용 frontend가 아니라 공통 Windows setup frontend가 되었다.
- Recommended는 GTK Disk부터, Advanced는 성공한 storage preparation 뒤 GTK Network부터 시작한다.
- M7/M8 destructive safety, Recommended disk policy, NetworkManager backend는 변경하지 않았다.
- GUI는 partition/format/download/image executor를 추가로 호출하지 않는다.

## Changed files

- State/API: `include/classicsetup/state.h`, `include/classicsetup/gui.h`.
- App routing: `src/app.c`, `src/state.c`.
- GUI model/frontend: `src/gui/gui.c`, `src/gui/gtk_frontend.c`.
- TUI fallback: `include/classicsetup/recommended_tui.h`, `src/tui/recommended.c`.
- Tests: `tests/state_test.c`, `tests/gui_test.c`.
- Snapshot: `.ai/handoff.md`.

## Implementation result

### Unified GUI flow

- Recommended: `Setup Mode -> GUI Transition -> Disk -> Network -> Windows Version -> Download -> Options -> Summary`.
- Advanced: `Keyboard -> Installation Mode -> Disk -> Partition -> Format -> Partition Apply -> Format Apply -> GUI Transition -> Network -> Windows Version -> Download -> Options -> Summary`.
- 기존 `RECOMMENDED_GUI_TRANSITION` state는 공통 의미의 `GUI_TRANSITION`으로 일반화했다.
- Advanced의 `FORMAT_APPLY_RESULT + CONTINUE`는 placeholder 대신 `GUI_TRANSITION`으로 이동한다.
- format result 화면은 success일 때만 Continue를 반환하며 app도 apply success와 format verification success를 다시 확인한다.

### GUI entry context

- `CLASSICSETUP_GUI_ENTRY_RECOMMENDED`: initial page `DISK`, 기존 Recommended scan/assessment/policy 사용.
- `CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED`: initial page `NETWORK`, GUI disk scan/selection을 실행하지 않음.
- session은 entry mode, current page, network/version 상태, prepared-storage flag와 prepared disk snapshot을 보관한다.
- GTK pointer는 session에 없고 installation `config`는 GUI session reset 대상이 아니다.
- Advanced session 생성은 partition apply success와 format verification success가 모두 필요하다.
- Advanced target disk는 config에서 snapshot으로 복사되어 Summary에 표시된다.

### Back, close, and failure policy

- Recommended Network Back은 공통 page model을 통해 Disk로 이동한다.
- Advanced Network Back은 GUI를 종료하고 TUI의 Format Apply Result로 복귀한다.
- Recommended Disk Back/window close는 Setup Mode로 복귀한다.
- Advanced window close도 Format Apply Result로 복귀하며 apply/format config를 보존한다.
- GTK disabled/init error는 공통 fallback을 표시한다.
- Recommended fallback Back은 Setup Mode, Advanced fallback Back은 Format Apply Result로 이동한다.
- Advanced fallback 문구는 storage preparation 결과가 보존됨을 명시한다.

### Shared frontend ownership

- Network, Windows Version, Download, Options, Summary page는 두 entry mode가 동일 구현을 사용한다.
- Advanced용 duplicate GTK page를 추가하지 않았다.
- Network page는 M10.2의 system-bus NetworkManager/GTask backend를 그대로 사용한다.
- Recommended의 Disk page만 기존 disk assessment core에 연결된다.
- GTK application ID와 window/sidebar 문구를 공통 Windows setup 의미로 정리했다.
- Summary는 Recommended GUI-selected disk 또는 Advanced prepared disk를 entry context에 따라 표시한다.

### Functions added/changed

- `classicsetup_gui_session_reset_for_entry()` (`src/gui/gui.c`): entry context와 initial page를 초기화.
- `classicsetup_gui_page_back_for_entry()` (`src/gui/gui.c`): page 이동 또는 TUI exit를 순수 transition으로 결정.
- `classicsetup_gui_session_init_after_advanced()` (`src/gui/gui.c`): success gate 확인 후 prepared disk snapshot과 Network initial page 구성.
- `show_common_gui()` (`src/app.c`): Recommended/Advanced 공통 ncurses shutdown, GUI run, ncurses reinit lifecycle.
- `show_format_apply_result()` (`src/app.c`): apply/format success를 재확인한 경우에만 GUI transition event 허용.
- `classicsetup_next_state_for_setup_mode()` (`src/state.c`): 공통 GUI 합류 및 mode별 Back destination 처리.
- `on_back_clicked()` (`src/gui/gtk_frontend.c`): callback 자체가 아닌 entry-aware page model의 결과를 적용.
- `update_summary()` (`src/gui/gtk_frontend.c`): entry별 disk source를 표시.
- `classicsetup_show_gui_unavailable()` (`src/tui/recommended.c`): mode에 맞는 safe fallback 안내.

## Build/test result

- GTK4-enabled clean Debug build 성공.
- `CLASSICSETUP_ENABLE_GTK=OFF` clean Debug/stub build 성공.
- C17, `-Wall -Wextra -Wpedantic`, warning 0.
- GTK-enabled CTest 13/13 통과.
- GTK-disabled CTest 13/13 통과.
- ASan/UBSan GTK-disabled CTest 13/13 통과.
- 검증 항목: Recommended initial Disk, Advanced initial Network, Advanced success gate, failure/verify-failure rejection, mode별 Network Back, mode별 GUI fallback/close destination, prepared disk 보존, Recommended regression.
- 실제 partition/format/network connection/destructive operation은 실행하지 않았다.

## Topics for ChatGPT to explain

- 두 frontend 흐름이 하나의 공통 state machine으로 수렴하는 구조.
- entry context와 current page가 서로 다른 책임을 갖는 이유.
- Recommended Disk entry와 Advanced Network entry가 동일 GTK stack을 공유하는 방식.
- TUI shutdown -> GTK main loop -> TUI reinit lifecycle 재사용.
- installation config ownership과 GUI session ownership 분리.
- session reset이 Advanced apply result를 지우지 않는 이유.
- page transition model에서 Back/close destination을 결정하는 장점.
- Advanced가 GUI Disk page와 storage executor를 다시 실행하면 안 되는 이유.

## Issues/cautions

- Network는 실제 NetworkManager backend지만 captive portal/WPA Enterprise 등은 아직 미지원이다.
- Windows Version, Download, Options, Summary의 install action은 placeholder다.
- Windows source discovery/download, ISO/WIM apply, boot configuration은 미구현이다.
- GUI session은 한 번의 `classicsetup_gui_run()` 동안만 유지되며 application-wide persistence는 아직 없다.
- Advanced storage 결과는 config에 유지되지만 GUI Summary는 현재 disk snapshot만 표시한다.
- `AFTER_FORMAT` placeholder state는 호환을 위해 남아 있으나 정상 Advanced success 경로에서는 사용하지 않는다.
- MBR destructive apply와 format은 계속 차단된다.
