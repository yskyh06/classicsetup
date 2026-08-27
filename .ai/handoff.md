# ClassicSetup Handoff

## Current milestone

- M7.5 완료
- 설치 모드를 UEFI/GPT와 Legacy BIOS/MBR로 분리하고, GPT 실제 적용 경로는 유지한 채 MBR은 planning/preview/render까지만 지원

## Changed files

- Build/docs: `CMakeLists.txt`, `README.md`, `.ai/handoff.md`
- New: `include/classicsetup/install_mode.h`, `include/classicsetup/install_mode_selection.h`, `src/core/install_mode.c`, `src/tui/install_mode_selection.c`
- Core/API: `include/classicsetup/apply.h`, `include/classicsetup/config.h`, `include/classicsetup/partition_plan.h`, `include/classicsetup/partition_selection.h`, `include/classicsetup/state.h`
- Implementation: `src/app.c`, `src/config.c`, `src/core/apply.c`, `src/core/format.c`, `src/core/partition_plan.c`, `src/state.c`, `src/tui/apply.c`, `src/tui/partition_selection.c`
- Tests: `tests/apply_test.c`, `tests/format_test.c`, `tests/partition_plan_test.c`, `tests/state_test.c`

## Implementation result

### Installation mode abstraction

- `classicsetup_install_mode`: `CLASSICSETUP_INSTALL_UEFI_GPT`, `CLASSICSETUP_INSTALL_BIOS_MBR`; zero/default 값은 UEFI/GPT
- `classicsetup_config.install_mode`이 선택값을 유지하며 흐름은 `Welcome -> Keyboard -> Installation Mode -> Disk`
- Installation Mode 화면은 UP/DOWN, ENTER, B, Q를 사용하고 기본 선택은 UEFI with GPT
- 모드가 실제로 바뀔 때 partition plan, original count, selected target/snapshot, format plans, apply plan/result를 초기화

### GPT regression

- 기존 UEFI 자동 layout의 EFI 260 MiB, MSR 16 MiB, Windows remainder, Recovery 1024 MiB 정책과 1 MiB alignment 유지
- 기존 `classicsetup_plan_create_windows_layout()`, `classicsetup_plan_has_windows_layout()`, `classicsetup_plan_undo_windows_layout()`, `classicsetup_build_apply_plan()`은 UEFI/GPT compatibility wrapper로 유지
- GPT apply role/GUID/name/range 검증과 `label: gpt` sfdisk golden output 유지
- WSL block, VM allowlist, destructive unlock, disk identity/system-disk/existing-partition/sector-size 검증, double safety collection, fork/execv, post-write verification 변경 없음

### BIOS/MBR planning

- 자동 layout: System Reserved 550 MiB -> Windows remainder -> Recovery 1024 MiB
- 세 파티션은 1 MiB boundary에서 시작하고, Windows 최소 크기는 기존 simulation 정책 64 MiB
- `SYSTEM_RESERVED` role 추가; format policy는 NTFS Quick, Windows는 사용자 NTFS Quick/Full, Recovery는 NTFS Quick
- MBR planning은 existing/new + 자동 3개가 4 primary를 넘으면 거부하고, apply plan은 512-byte sector 기준 2 TiB 이하, EFI/MSR 금지, exact role/order/range를 검증
- MBR metadata: System Reserved `0x07` + bootable, Windows `0x07`, Recovery `0x27`
- renderer는 `label: dos`, `unit: sectors`, hex type과 System Reserved `bootable`을 생성
- MBR Preview는 가능하지만 A 입력 후 `CLASSICSETUP_APPLY_SAFETY_MBR_NOT_ENABLED`로 process 탐색/실행 전에 차단

### Functions and logic

#### `classicsetup_default_install_mode(void)`

- File: `src/core/install_mode.c`
- Role: 기존 동작을 보존하는 기본 설치 모드 제공
- Input: 없음
- Return: `CLASSICSETUP_INSTALL_UEFI_GPT`
- Logic: 고정 기본 정책값 반환
- Uses: `classicsetup_install_mode`
- Connected to: app config 초기화, Installation Mode 화면 fallback, tests

#### `classicsetup_show_install_mode_selection(enum classicsetup_install_mode *)`

- File: `src/tui/install_mode_selection.c`
- Role: UEFI/GPT 또는 BIOS/MBR 선택 화면
- Input: 현재/선택 모드 포인터
- Return: CONTINUE, BACK, QUIT
- Logic: 파란 TUI 출력 -> UP/DOWN 선택 -> ENTER 저장; B/Q 이벤트 반환
- Uses: ncurses `getch`, `KEY_UP`, `KEY_DOWN`, common TUI/keymap
- Connected to: `show_install_mode()` -> app event/state transition

#### `classicsetup_config_set_install_mode(struct classicsetup_config *, enum classicsetup_install_mode)`

- File: `src/config.c`
- Role: 모드 변경 commit과 stale scheme state 제거
- Input: config, 새 모드
- Return: 없음
- Logic: 입력/동일값 검사 -> 모드 저장 -> `classicsetup_config_reset_partition_plan()`
- Uses: config reset/clear helpers
- Connected to: Installation Mode ENTER; 결과는 Disk/Partition/Format/Apply 전 단계에 전달

#### `classicsetup_plan_create_uefi_windows_layout(...)`

- File: `src/core/partition_plan.c`
- Role: 기존 GPT 자동 Windows layout 생성
- Input: plan, Unallocated index, Windows index output
- Return: 성공 0, 검증/공간/계산 실패 -1
- Logic: temporary plan -> alignment/기존 size policy -> EFI/MSR/Windows/Recovery -> rebuild/validate -> commit
- Uses: UEFI size policy, append/rebuild/validator helpers
- Connected to: scheme dispatcher와 기존 compatibility wrapper

#### `classicsetup_plan_create_bios_windows_layout(...)`

- File: `src/core/partition_plan.c`
- Role: BIOS/MBR 자동 layout 생성
- Input: plan, Unallocated index, Windows index output
- Return: 성공 0, invalid/공간 부족/2 TiB 초과 -1
- Logic: plan/MBR limit 검증 -> 1 MiB align -> System Reserved/Windows/Recovery 계산 -> temporary append/rebuild/validate -> Windows lookup/commit
- Uses: `CLASSICSETUP_MBR_MAX_SECTORS`, System Reserved/Recovery/Windows policy constants
- Connected to: `classicsetup_plan_prepare_install_target_for_mode()`; Partition Unallocated ENTER

#### `classicsetup_plan_prepare_install_target_for_mode(...)`

- File: `src/core/partition_plan.c`
- Role: ENTER 대상과 install mode에 따른 layout dispatcher
- Input: plan, install mode, selected index, target index output
- Return: 설치 대상 준비 성공 0, 불가/실패 -1
- Logic: Existing/New install target은 그대로 선택; Unallocated는 UEFI 또는 BIOS builder 호출
- Uses: install-target predicate, scheme builders
- Connected to: Partition TUI -> selected Windows item -> config -> Format

#### `classicsetup_plan_has_windows_layout_for_mode(...)`

- File: `src/core/partition_plan.c`
- Role: scheme별 완전한 자동 layout 존재 여부 확인
- Input: plan, install mode
- Return: 존재 1, 불완전/혼합/invalid 0
- Logic: UEFI는 EFI/MSR/Windows/Recovery, BIOS는 System Reserved/Windows/Recovery의 NEW exact set/order/adjacency 확인
- Uses: plan validator, role counters
- Connected to: Undo 표시/허용, apply plan builder

#### `classicsetup_plan_undo_windows_layout_for_mode(...)`

- File: `src/core/partition_plan.c`
- Role: scheme별 자동 layout transactional Undo
- Input: plan, install mode, restored Unallocated index output
- Return: 성공 0, layout 없음/재검증 실패 -1
- Logic: temporary copy -> 해당 자동 roles만 제거 -> Unallocated rebuild/merge -> validate -> restored range lookup -> commit
- Uses: scheme role predicates, rebuild/validator
- Connected to: `classicsetup_config_undo_windows_layout()`; Generic New/Existing은 보존

#### `classicsetup_format_policy_for_role(...)`

- File: `src/core/format.c`
- Role: role별 format plan 정책 생성
- Input: role, Windows 선택 mode, format plan output
- Return: 지원 role 0, invalid -1
- Logic: 기존 UEFI 정책 유지; System Reserved에 NTFS Quick 추가
- Uses: filesystem/mode enums
- Connected to: config selected target/role format plan 저장; 실제 mkfs 호출 없음

#### `classicsetup_build_apply_plan_for_mode(...)`

- File: `src/core/apply.c`
- Role: mutable planned layout을 scheme-aware immutable apply plan으로 변환
- Input: install mode, disk identity, partition plan, original count, output
- Return: 성공 0, restricted/invalid -1
- Logic: no-existing/exact-layout 검사 -> table type 결정 -> role/range 복사 -> GPT GUID 또는 MBR metadata 지정 -> full validation -> commit
- Uses: GPT GUID constants, MBR type/boot mapping, plan validator
- Connected to: Apply Preview 준비; executor는 config를 재해석하지 않음

#### `classicsetup_validate_apply_plan(...)`

- File: `src/core/apply.c`
- Role: disk identity/sector count 및 scheme별 immutable plan invariant 검증
- Input: apply plan
- Return: valid 1, invalid 0
- Logic: common identity 검사 -> GPT exact validator 또는 MBR exact validator dispatch
- Uses: GUID/name/type/boot metadata, role order, alignment, disk bounds, MBR 2 TiB/primary limits
- Connected to: renderer, safety collection, executor, post-write verifier

#### `classicsetup_render_sfdisk_script(...)`

- File: `src/core/apply.c`
- Role: 검증된 apply plan을 shell 없는 sfdisk stdin script로 렌더링
- Input: apply plan, output buffer/size
- Return: 성공 0, invalid/buffer 부족 -1
- Logic: GPT는 기존 golden 형식; MBR은 DOS label, `type=07/27`, System Reserved bootable 출력
- Uses: `vsnprintf`, `append_script`
- Connected to: GPT executor; MBR unit/golden dry-run만 사용

#### `classicsetup_evaluate_apply_safety(...)` / `classicsetup_execute_apply_plan(...)`

- File: `src/core/apply.c`
- Role: scheme-aware destructive eligibility와 실행 gate
- Input: safety inputs 또는 immutable apply plan/result
- Return: safety code; executor API 성공 0/호출 오류 -1
- Logic: 기존 GPT gates 유지; MBR는 `MBR_NOT_ENABLED` 반환하고 process lookup/render/fork 전에 종료
- Uses: environment/disk/system-disk/apply validation; GPT에서만 기존 process wrapper
- Connected to: Apply Confirmation A -> Apply Result; MBR executor invocation count 0

### Data structures

- `classicsetup_install_mode`: UEFI/GPT와 BIOS/MBR 선택; config에 저장
- `classicsetup_partition_table_type`: immutable apply plan의 GPT/MBR scheme
- `CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED`: BIOS boot files용 planned role; 기존 role 값 뒤에 추가
- `classicsetup_apply_partition.mbr_type`: DOS partition type byte metadata
- `classicsetup_apply_partition.bootable`: System Reserved active/boot flag metadata
- `classicsetup_apply_plan.table_type`: renderer/validator/executor scheme source; mutable config 재해석 방지
- `classicsetup_apply_safety_inputs.table_type`: actual eligibility에서 MBR dry-run-only gate 전달

### Key mapping

| Screen | ENTER | B | ESC | Q | A | C | D | U | UP/DOWN |
|---|---|---|---|---|---|---|---|---|---|
| Welcome | Continue | - | - | Quit | - | - | - | - | - |
| Keyboard | Continue | Welcome | - | Quit | - | - | - | - | Select |
| Installation Mode | Continue | Keyboard | - | Quit | - | - | - | - | Select |
| Disk | Continue | Installation Mode | - | Quit | - | - | - | - | Select |
| Partition | Install/auto layout | Disk | - | Quit | - | Create | Delete | Undo exact auto layout | Select |
| Create/Delete/Undo modal | Confirm where applicable | - | Cancel | Quit | - | Create | Delete | Undo | - |
| Format | Continue | - | Partition | Quit | - | - | - | - | Select |
| Apply Preview | Continue | Format | - | Quit | - | - | - | - | - |
| Apply Confirmation | - | Preview | - | Quit | Apply | - | - | - | - |
| Apply Result | Continue | Preview | - | Quit | - | - | - | - | - |

### Control/Data flow

- UEFI: Mode UEFI/GPT -> Disk -> Unallocated ENTER -> EFI/MSR/Windows/Recovery -> Format plan -> GPT apply plan -> GPT renderer -> A -> unchanged M7 safety/executor
- BIOS: Mode BIOS/MBR -> Disk -> Unallocated ENTER -> System Reserved/Windows/Recovery -> Format plan -> MBR apply plan -> MBR renderer/Preview -> A -> `MBR_NOT_ENABLED` -> executor not called
- Mode change: Installation Mode ENTER -> changed mode -> clear plan/selection/format/apply snapshots -> Disk -> new scheme planning
- Undo: U -> scheme-aware exact auto set removal -> rebuild/validate -> restored Unallocated selection -> config target/format/apply invalidation

## Build/test result

- Clean CMake Debug build 성공: GCC 15.2, C17, `-Wall -Wextra -Wpedantic`, 경고 없음
- CTest 8/8 통과: state, disk, partition, partition plan, format, apply safety/render, key mapping, no-function-keys
- ASan/UBSan Debug build 및 CTest 8/8 통과 (`ASAN_OPTIONS=detect_leaks=0`)
- GPT regression: default UEFI, 기존 layout sizes/ranges/GUID, exact sfdisk golden, VMware/VirtualBox/WSL/system-disk safety tests 통과
- MBR dry-run: 3-role layout/order/format, 4-primary/2 TiB limit, undo, DOS golden render, `0x07/0x27`, bootable, MBR safety block 검증 통과
- Mode change: GPT -> BIOS, BIOS -> GPT에서 partition/format/apply stale state 제거 검증 통과
- 실제 `/dev` write, `sfdisk`, filesystem format, destructive VM test는 수행하지 않음
- Manual VM test: 사용자가 VMware UEFI VM에서 UEFI with GPT -> 별도 빈 `/dev/sdb` -> Unallocated ENTER -> GPT Preview -> A -> SUCCESS 경로만 재검증; BIOS/MBR A 테스트 금지

## Topics for ChatGPT to explain

- `classicsetup_install_mode`/`table_type`에서 firmware mode와 partition scheme을 분리해 전달하는 구조
- UEFI EFI/MSR과 BIOS System Reserved의 역할 차이 및 `format.c` 정책 연결
- `classicsetup_plan_create_bios_windows_layout()`의 MBR primary count/2 TiB/alignment 정책
- `mbr_metadata_for_role()`의 active partition, `0x07`, `0x27` metadata
- `classicsetup_build_apply_plan_for_mode()`와 scheme별 renderer 분리 이유
- compatibility wrapper로 GPT 동작과 golden output을 보존한 방식
- MBR planning/render와 destructive enablement를 분리한 fail-closed 설계

## Issues/cautions

- BIOS/MBR destructive apply는 명시적으로 비활성화되어 실제 VM에서 검증하지 않음
- System Reserved 550 MiB, Recovery 1024 MiB, Windows 최소 64 MiB는 현재 ClassicSetup 개발 정책값
- MBR boot code, BCD, 실제 active boot 가능성은 아직 구현/검증하지 않음
- filesystem formatting, Windows image 적용, mount는 아직 없음
- MBR에서 existing partition 보존/삭제/extended-logical partition 적용은 지원하지 않음
- 현재 Linux boot firmware와 선택 mode를 `/sys/firmware/efi`로 대조하는 검증은 향후 항목
- Windows 11 설치 주력 경로는 UEFI/GPT이며, 실제 destructive testing도 현재 GPT 경로만 허용
