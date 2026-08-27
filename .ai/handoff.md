# ClassicSetup Handoff

## Current milestone

- M9.1 TUI visual refinement 완료.
- Advanced/ncurses 화면을 80x25 Windows XP text-mode Setup 밀도와 영역 구성에 맞게 정리.
- Core behavior, state machine, disk policy, key mapping, destructive safety는 변경하지 않음.

## Changed files

- Shared layout: `include/classicsetup/tui.h`, `src/tui/tui.c`.
- Retouched screens: `src/tui/{welcome,setup_mode_selection,keyboard,install_mode_selection,disk_selection,partition_selection,format_selection,apply,format_apply,after_format,quit,recommended}.c`.
- Layout test/build: `tests/tui_layout_test.c`, `CMakeLists.txt`.
- Documentation: `.ai/handoff.md`.

## Implementation result

### Visual issues fixed

- 실제 터미널 전체 높이/너비를 사용해 생기던 과도한 빈 공간과 세로 분산을 제거.
- 내용 없는 대형 frame을 제거하고 선택 목록 frame을 실제 표시 항목 수에 맞춤.
- 일반 안내, warning, confirmation, result를 중앙 정렬 중심에서 compact left alignment로 변경.
- Disk metadata를 목록 바로 아래 `Model / Device / Capacity` 블록으로 배치.
- 모든 화면 footer 간격과 색을 공통 처리.

### Logical canvas strategy

- `classicsetup_tui_canvas_width()` / `classicsetup_tui_canvas_height()`가 최대 80x25 논리 영역을 반환.
- 큰 터미널에서는 80x25 canvas를 수평/수직 중앙에 유지하고 내부 요소를 확장하지 않음.
- 80x25 이하에서는 실제 terminal 크기를 canvas로 사용하며 기존 clipping/fail-safe drawing을 유지.
- Header는 canvas 첫 줄, underline은 바로 다음 줄, footer는 canvas 마지막 줄에 고정.

### Dynamic lists and details

- `classicsetup_tui_compact_list_height(item_count, maximum_rows)`가 항목 수와 화면별 최대 행을 사용해 목록 높이를 결정.
- Advanced Disk는 최대 7행, Partition은 최대 8행, Recommended Disk는 최대 4개 항목을 표시하고 선택 위치에 따라 scroll window를 이동.
- Keyboard, Installation Mode, Setup Mode, Format은 고정된 작은 frame만 사용.
- Selection은 frame 내부 전체 행을 white/gray 배경과 dark blue text로 표시.

### Footer and shared helpers

- `classicsetup_tui_draw_footer()`는 80-column logical footer bar에 gray background/black text를 적용.
- `classicsetup_tui_draw_frame()`과 `classicsetup_tui_draw_list_row()`가 logical-to-terminal 좌표 변환과 clipping을 담당.
- `classicsetup_tui_draw_bullet()`은 Welcome/placeholder action 문구를 classic Setup 형식으로 출력.
- `classicsetup_tui_draw_metadata()`는 compact label/value detail block을 출력.
- `classicsetup_tui_draw_warning()`은 yellow 강조를 warning line에만 제한.

### Screens retouched

- Welcome: giant empty frame 제거, intro와 action bullet을 상단에 compact 배치.
- Setup Mode: modern card 느낌을 줄이고 two-choice Setup list와 짧은 설명으로 변경.
- Keyboard / Installation Mode / Format: vertical centering 제거, instruction -> compact frame -> footer 리듬 통일.
- Disk / Partition: dynamic framed list와 바로 이어지는 detail block 적용.
- Partition create/delete/undo, Quit, Apply/Format confirmation/result: left-aligned warning/confirmation 구조 적용.
- Recommended GUI transition과 future-state placeholders: 빈 frame 없이 간단한 action text만 표시.

## Build/test result

- Clean CMake Debug build 성공: GCC 15.2, C17, `-Wall -Wextra -Wpedantic`, warning 0.
- CTest 11/11 통과; 새 `tui_compact_layout`에서 empty/exact/capped list height 검증.
- ASan/UBSan Debug build 및 CTest 11/11 통과 (`ASAN_OPTIONS=detect_leaks=0`).
- 80x25 smoke: Welcome, Setup Mode, Advanced Keyboard, Installation Mode, Disk, Partition, Quit 확인.
- 120x40 medium 및 160x60 large smoke: 80x25 canvas가 늘어나지 않고 중앙에 유지되며 title/footer clipping 없음 확인.
- 실제 destructive disk operation은 실행하지 않음.

## Topics for ChatGPT to explain

- Physical terminal coordinates와 80x25 logical canvas 좌표 분리.
- Item-count 기반 dynamic frame height와 selected-item scrolling.
- Shared ncurses helper가 화면별 좌표 중복과 visual drift를 줄이는 방식.
- Full-row selection color pair와 logical footer bar 구성.
- Small terminal에서 invalid frame을 생략하고 text를 clip하는 fail-safe drawing.

## Issues/cautions

- 80x25보다 작은 터미널에서는 중요 text/footer를 우선하고 일부 frame 또는 긴 문구가 생략/절단될 수 있음.
- ncurses glyph와 gray/white 색감은 terminal palette/font에 따라 다르게 보일 수 있음.
- Recommended GTK 전환 이후의 실제 GUI visual system은 아직 구현되지 않음.
- 자동 screenshot regression은 없으며 layout helper는 deterministic unit test, 실제 화면은 PTY smoke로 확인함.
