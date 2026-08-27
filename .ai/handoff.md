# ClassicSetup Handoff

## Current milestone

- M9.1 TUI UX/safety refinement 완료.
- 공통 흐름은 `Welcome -> License / Risk Agreement -> Setup Mode`로 변경했으며 Recommended와 Advanced 모두 동의 화면을 통과한다.
- Disk policy, Recommended 제한, M7/M8 destructive safety와 기존 화면 key mapping은 변경하지 않았다.

## Changed files

- Agreement/state: `include/classicsetup/license_agreement.h`, `include/classicsetup/state.h`, `src/tui/license_agreement.c`, `src/state.c`, `src/app.c`.
- Shared layout/wrapping: `include/classicsetup/tui.h`, `src/tui/tui.c`.
- Wrapped TUI text: `src/tui/{welcome,setup_mode_selection,keyboard,install_mode_selection,disk_selection,partition_selection,format_selection,apply,format_apply,after_format,quit,recommended}.c`.
- Tests/build: `tests/state_test.c`, `tests/tui_layout_test.c`, `tests/recommended_test.c`, `CMakeLists.txt`.
- Documentation: `.ai/handoff.md`.

## Implementation result

### Agreement state

- `CLASSICSETUP_STATE_LICENSE_AGREEMENT`와 XP Setup 스타일 risk notice 화면을 추가했다.
- `A/a`만 동의 및 진행, `B/b`는 Welcome 복귀, `Q/q`는 기존 app-level Quit Confirmation 요청이다. ENTER와 그 외 키는 화면을 유지한다.
- 안내에는 partition table/filesystem/boot configuration 변경 가능성, 영구 데이터 삭제 위험, 사전 backup, hardware/layout 비보장, target 확인 및 backup 책임, 위험 인지 확인을 포함한다.
- `CLASSICSETUP_RISK_AGREEMENT_VERSION` 상수를 준비했으며 persistent acceptance는 저장하지 않는다.

### Word wrapping

- `classicsetup_tui_wrap_text()`는 word boundary, 명시적 newline, 긴 단어 분할, 출력 buffer 제한을 처리하고 소비 line 수를 반환한다.
- `classicsetup_tui_draw_wrapped_text()`가 logical canvas 좌표와 clipping을 유지하며 래핑 결과를 출력한다.
- `classicsetup_tui_draw_warning()`, bullet, metadata와 warning/error/agreement/policy reason/confirmation/descriptive text를 wrapped rendering으로 전환했다.
- Legacy BIOS Recommended 차단 사유는 content width 안에서 여러 줄로 표시된다.

### Adaptive logical canvas

- terminal `COLS/LINES`를 한 곳에서 profile로 변환한다: Compact는 80x25 미만의 실제 크기, Classic은 80x25, Medium은 100x30, Large는 최대 120x40이다.
- physical terminal과 logical canvas를 분리하고 큰 terminal에서는 logical canvas를 중앙 정렬한다. Header는 canvas 상단, footer gray bar는 canvas 하단에 고정된다.
- 기존 item-count 기반 dynamic list height는 유지하며 추가 공간은 frame 확대가 아니라 wrapping 폭과 가독성에 사용한다.
- ncurses는 terminal row/column만 사용한다. GNOME Terminal font는 변경하지 않으며 향후 boot ISO가 framebuffer/HiDPI에 맞는 Linux console font를 선택하는 영역으로 남긴다.

## Build/test result

- Clean CMake Debug build 성공: C17, `-Wall -Wextra -Wpedantic`, warning 0.
- CTest 11/11 통과: Agreement 전이/키, exact-width·multi-line·long-word·newline wrapping, Compact/Classic/Medium/Large 선택, maximum cap, small terminal, Legacy BIOS reason wrapping 포함.
- ASan/UBSan Debug build와 CTest 11/11 통과 (`ASAN_OPTIONS=detect_leaks=0`).
- PTY smoke: 80x25, 100x30, 120x40, 160x60에서 Welcome/Agreement/footer 확인. ENTER 비동의, Q common quit, large-terminal 120x40 cap/centering을 확인했다.
- 실제 destructive disk operation은 실행하지 않았다.

## Topics for ChatGPT to explain

- Agreement가 별도 app state이고 입력 결과를 기존 event/state 흐름으로 전달하는 구조.
- Word-boundary wrapping, explicit newline, 긴 단어 fallback과 consumed-line 계산.
- Physical terminal `COLS/LINES`와 adaptive logical canvas profile의 분리.
- Logical canvas centering과 safe clipped drawing이 small/large terminal을 함께 처리하는 방식.
- Font scaling을 ncurses가 아닌 향후 boot environment에서 담당해야 하는 이유.

## Issues/cautions

- Agreement acceptance는 실행 중 전이만 의미하며 disk나 persistent config에 기록하지 않는다.
- 극단적으로 작은 terminal에서는 안전하게 clipping하지만 모든 agreement 문장을 동시에 표시할 수는 없으며 scrolling은 아직 없다.
- Unicode display width는 다루지 않고 현재 영문 ASCII 문구 기준으로 wrapping한다.
- Terminal palette/font에 따라 XP 스타일 색과 glyph가 다르게 보일 수 있다.
- 현재 환경에는 VMware GUI terminal이 없어 160x60 PTY로 large-terminal 동작을 대신 확인했다.
- Boot ISO의 console font 선택과 automated screenshot regression은 후속 범위다.
