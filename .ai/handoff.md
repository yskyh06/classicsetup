# ClassicSetup Handoff

## Current milestone

- M10.2.2 Advanced Deferred Storage Apply + Global Back-Key Normalization 완료.
- Advanced는 storage plan만 준비한 뒤 GTK Network로 진입하며 실제 partition/format apply는 수행하지 않는다.
- Recommended는 기존처럼 GTK Disk부터 시작하고 Advanced TUI/GTK는 Network 이후 공통 page 구현을 공유한다.
- M7/M8 destructive safety와 executor 제한, Recommended disk policy, NetworkManager backend는 변경하지 않았다.
- 최종 GUI Install transaction은 아직 연결하지 않았다.

## Changed files

- State/config/API: `include/classicsetup/config.h`, `include/classicsetup/format_selection.h`, `include/classicsetup/gui.h`.
- Routing/model: `src/app.c`, `src/state.c`, `src/config.c`, `src/gui/gui.c`.
- UI: `src/gui/gtk_frontend.c`, `src/tui/format_selection.c`.
- Tests/build: `tests/state_test.c`, `tests/gui_test.c`, `tests/format_test.c`, `tests/check_back_key_policy.cmake`, `CMakeLists.txt`.
- Snapshot: `.ai/handoff.md`.

## Implementation result

- Advanced 정상 흐름은 `Disk -> Partition Plan -> Format Plan -> GUI Transition -> Network`이다.
- `FORMAT + CONTINUE`는 `APPLY_PREVIEW`가 아니라 공통 `GUI_TRANSITION`으로 이동한다.
- partition/format Apply Preview, Confirmation, Result state와 executor API는 삭제하지 않았고 향후 최종 Install에서 재사용 가능하다.
- `classicsetup_config_set_format_plan()`이 성공하면 `advanced_storage_plan_ready`를 설정한다.
- `classicsetup_config_advanced_plan_is_ready()`는 Advanced mode, selected disk, valid partition plan, 최신 install target, NTFS Quick/Full format plan을 확인한다.
- planning state 변경/reset 시 format plan과 readiness가 함께 무효화된다.
- GUI 진입 전 execution result는 지우지만 installation config의 disk/partition/format plan은 유지한다.
- immutable apply plan은 제한 경로에서 build 가능한 경우에만 optional snapshot으로 보관한다.
- existing/deleted 등 M7 restricted layout에서 apply-plan build가 실패해도 planning GUI 진입은 막지 않는다.
- 실패한 apply-plan build는 executor restriction을 우회하지 않으며 실제 실행은 여전히 향후 M7 gate를 통과해야 한다.
- GUI entry context를 `CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN`으로 명확히 변경했다.
- Recommended initial page는 Disk, Advanced Plan initial page는 Network다.
- Advanced GUI session은 target disk, install mode, partition plan, selected/role format plans, optional immutable apply plan을 snapshot으로 가진다.
- GUI session reset은 installation config나 storage plan을 지우지 않는다.
- Advanced Network Back/window close/GTK fallback Back은 `FORMAT` planning 화면으로 복귀한다.
- Summary는 Advanced disk를 `Planned target disk`로 표시하고 disk 변경이 아직 적용되지 않았음을 안내한다.
- GUI Summary의 placeholder Install은 partition/format executor를 호출하지 않는다.
- Format Selection의 page navigation은 `B=Back`; ESC handler와 `ESC=Cancel` footer를 제거했다.
- ESC는 Create/Delete/Undo/error/quit 같은 modal 취소·닫기에만 남아 있다.
- 최종 TUI 정책: `ENTER` safe continue/select, `B` page back, `Q` quit, `A` explicit action, `ESC` modal cancel, `C/D/U` partition actions.
- `tui_back_key_policy` source audit가 ESC page-back 문구와 Format handler/footer 불일치를 회귀 검사한다.

## Build/test result

- GTK4 ON clean Debug configure/build 성공; GTK frontend enabled.
- GTK OFF clean Debug configure/build 성공; stub frontend enabled.
- 두 구성 모두 C17, `-Wall -Wextra -Wpedantic`, warning 0.
- GTK ON CTest 14/14 통과.
- GTK OFF CTest 14/14 통과.
- ASan/UBSan GTK OFF CTest 14/14 통과 (`detect_leaks=0`; 실행 환경 ptrace 때문에 LSan만 비활성).
- 상태 테스트가 Advanced Format에서 GUI로 직접 이동하고 Apply states를 방문하지 않음을 검증한다.
- GUI 테스트가 entry별 initial page/Back, planning snapshot 보존, stale GUI reset을 검증한다.
- format/config 테스트가 planning readiness 생성과 reset 무효화를 검증한다.
- 기존 apply/format safety와 restricted-layout 회귀 테스트가 모두 통과했다.
- 실제 disk write, partition apply, mkfs, 임의 Wi-Fi 연결은 실행하지 않았다.

## Topics for ChatGPT to explain

- partition/format planning과 destructive execution을 분리하는 구조.
- Windows source 확보 전에 destructive work를 미루는 이유.
- installation config, GUI session, executor result의 서로 다른 ownership.
- 여러 frontend 흐름이 공통 GUI state machine으로 수렴하는 방식.
- GUI entry context와 current page의 차이.
- Advanced가 GUI Disk page를 다시 실행하지 않는 이유.
- optional immutable apply-plan snapshot과 executor eligibility의 차이.
- page navigation `B`와 modal cancellation `ESC`를 분리하는 이유.
- key mapping을 UI state-machine input contract로 다루는 방식.
- 최종 Summary를 전체 install transaction boundary로 삼는 설계.

## Issues/cautions

- 최종 GUI Install은 아직 partition apply, format apply, image apply, boot configuration과 연결되지 않았다.
- M7 actual apply는 existing/deleted가 포함된 unsupported layout을 계속 차단한다.
- 기존 disk의 실제 preserve/erase/install 지원은 후속 작업이다.
- Windows source discovery/download, ISO/WIM apply, boot configuration은 미구현이다.
- Apply Preview/Confirmation/Result TUI는 보존되어 있지만 현재 Advanced 정상 planning 경로 밖에 있다.
- NetworkManager 기능은 유지되며 captive portal, WPA Enterprise, static IP 등은 미지원이다.
- MBR destructive partition apply와 format은 계속 비활성이다.
