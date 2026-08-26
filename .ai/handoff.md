# ClassicSetup Handoff

## Current milestone

- M7 key mapping revision 완료
- Function Key 의존성을 제거하고 Quit을 Q, destructive Apply를 A로 변경

## Changed files

- `CMakeLists.txt`, `README.md`
- `include/classicsetup/keymap.h`
- `src/tui/keymap.c`
- `src/tui/welcome.c`, `keyboard.c`, `disk_selection.c`
- `src/tui/partition_selection.c`, `format_selection.c`, `quit.c`
- `src/tui/apply.c`, `after_format.c`
- `tests/key_mapping_test.c`, `tests/check_no_function_keys.cmake`
- `.ai/handoff.md`

## Implementation result

- 모든 TUI와 modal의 F3 Quit 입력/문구를 Q/q로 교체
- Quit Confirmation은 Q/q만 Exit를 확정하고 ESC는 기존 setup state를 유지
- Apply Confirmation은 A/a만 APPLY 결과를 반환하며 ENTER와 Function Key는 무시
- 기존 ENTER/B/ESC/C/D/U 동작과 `QUIT_REQUEST` event/state 구조는 유지
- M7 executor와 WSL block, VM allowlist, unlock, disk/system 보호, plan 검증, 이중 safety 검사, post-write 검증은 변경하지 않음

| Screen | Key mapping |
|---|---|
| Welcome | `ENTER=Continue`, `Q=Quit` |
| Keyboard | `UP/DOWN=Select`, `ENTER=Continue`, `B=Back`, `Q=Quit` |
| Disk | `UP/DOWN=Select`, `ENTER=Continue`, `B=Back`, `Q=Quit` |
| Partition | `UP/DOWN=Select`, `ENTER=Install/Auto Layout`, `C=Create`, `D=Delete`, `U=Undo Layout`, `B=Back`, `Q=Quit` |
| Create | `ENTER=Create`, `ESC=Cancel`, `Q=Quit` |
| Delete | `D=Delete`, `ESC=Cancel`, `Q=Quit` |
| Undo | `U=Confirm`, `ESC=Cancel`, `Q=Quit` |
| Format | `UP/DOWN=Select`, `ENTER=Continue`, `ESC=Cancel`, `Q=Quit` |
| Apply Preview | `ENTER=Continue`, `B=Back`, `Q=Quit` |
| Apply Confirmation | `A=Apply`, `B=Back`, `Q=Quit` |
| Apply Result | 성공 시 `ENTER=Continue`, `B=Back`, `Q=Quit` |
| Quit Confirmation | `Q=Quit Setup`, `ESC=Continue Setup` |
| After Format | `ENTER=Finish`, `B=Back`, `Q=Quit` |

## Build/test result

- CMake Debug build 성공; C17, `-Wall -Wextra -Wpedantic` 경고 없음
- CTest 8/8 통과
- `key_mapping`: q/Q Quit, a/A Apply, 기존 F3/F10 상당 key 및 ENTER 비동작 검증
- `tui_no_function_keys`: 모든 `src/tui/*.c`에서 Function Key handler/legend 부재 검증
- 실제 executor 또는 디스크 쓰기 실행 없음

## Topics for ChatGPT to explain

- `classicsetup_key_is_quit()`/`classicsetup_key_is_apply()`로 입력 정책을 한 곳에서 테스트하는 구조
- 문자 입력 변경과 기존 `QUIT_REQUEST`/APPLY event 의미를 분리한 이유
- Apply trigger 변경이 M7 safety/executor contract에 영향을 주지 않는 흐름

## Issues/cautions

- A는 Apply Confirmation 화면에서만 destructive apply를 요청함
- Q는 대소문자를 허용하지만 ENTER는 Quit Confirmation/Apply Confirmation에서 확정 키가 아님
- 실제 VirtualBox/VMware destructive apply는 이번 개정에서도 수행하지 않음
