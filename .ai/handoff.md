# ClassicSetup Handoff

## Current milestone

- M7 완료
- 목표: M6 partition plan을 제한된 GPT apply plan으로 변환하고, 검증된 VirtualBox/VMware 테스트 VM에서만 F10 확인 후 `sfdisk`로 적용
- 현재 WSL에서는 Preview와 Confirmation까지 가능하지만 destructive executor는 항상 차단

## Changed files

- `.ai/handoff.md`
- `CMakeLists.txt`
- `include/classicsetup/after_format.h`
- `include/classicsetup/apply.h`
- `include/classicsetup/apply_tui.h`
- `include/classicsetup/config.h`
- `include/classicsetup/environment.h`
- `include/classicsetup/process.h`
- `include/classicsetup/state.h`
- `include/classicsetup/system_disk.h`
- `src/app.c`
- `src/config.c`
- `src/core/apply.c`
- `src/core/environment.c`
- `src/core/partition_plan.c`
- `src/core/process.c`
- `src/core/system_disk.c`
- `src/state.c`
- `src/tui/after_format.c`
- `src/tui/apply.c`
- `tests/apply_test.c`
- `tests/format_test.c`
- `tests/partition_plan_test.c`
- `tests/state_test.c`
- `tests/fixtures/environment/wsl/*`
- `tests/fixtures/environment/virtualbox/*`
- `tests/fixtures/environment/vmware/*`
- `tests/fixtures/environment/unknown/*`

## Implementation result

- 흐름을 `Partition -> Format plan -> Apply Preview -> Apply Confirmation -> Apply Result -> placeholder`로 확장
- Format plan은 계속 메모리에만 저장하며 M7 executor는 GPT partition table만 대상으로 함
- mutable partition plan에서 별도 `classicsetup_apply_plan`을 transactional하게 생성
- M7 실제 apply 범위를 기존 partition 0개, 완전한 NEW EFI/MSR/Windows/Recovery 한 세트로 제한
- Existing/Deleted/Generic NEW가 포함된 layout은 Preview eligibility 단계에서 거부
- 자동 Windows layout은 GPT metadata를 위해 disk 앞뒤 1 MiB를 Unallocated로 남김
- role별 공식 GPT type GUID와 고정 partition name을 apply metadata로 저장
- named-field `sfdisk` script를 pure renderer로 생성
- shell/system/popen 없이 `fork()`/`execv()`/pipe/`waitpid()`로 `sfdisk` 실행
- `sfdisk` stdout/stderr를 capture하고 exit status를 apply result로 저장
- 정상 종료 후 sysfs partition range를 최대 1초 동안 재스캔하여 plan과 비교
- Preview는 read-only이며 Confirmation의 ENTER는 무시; F10만 executor 경로를 시작
- WSL F10 결과 화면에 `Destructive disk operations are disabled under WSL.` 표시
- filesystem 생성, mount, Windows image apply는 구현하지 않음

## Safety model

### WSL block

- `/proc/version`과 `/proc/sys/kernel/osrelease`를 모두 읽어 Microsoft/WSL marker 확인
- 두 파일 중 하나라도 읽을 수 없으면 environment를 UNKNOWN으로 유지
- WSL은 safety evaluation의 최우선 차단 사유
- WSL이면 disk revalidation, mount inspection, `sfdisk` process 실행 전에 반환
- `CLASSICSETUP_ALLOW_DESTRUCTIVE=YES`가 있어도 WSL 차단이 우선

### VM detection

- `/sys/class/dmi/id/product_name`, `sys_vendor`, `board_vendor` 검사
- VirtualBox marker가 있으면 `CLASSICSETUP_ENV_VIRTUALBOX`
- VMware marker가 있으면 `CLASSICSETUP_ENV_VMWARE`
- Oracle vendor 문자열만으로는 VirtualBox로 인정하지 않음
- UNKNOWN, bare metal, DMI 판단 실패는 apply 거부

### Destructive unlock

- 외부에서 설정한 `CLASSICSETUP_ALLOW_DESTRUCTIVE` 값이 정확히 `YES`일 때만 통과
- unset, `yes`, 공백이 붙은 값 등은 거부
- 프로그램은 환경변수를 생성하거나 변경하지 않음

### Disk identity revalidation

- F10 처리 안에서 `/sys/block`을 다시 스캔
- selected name, `/dev/<name>` path, byte size, model을 다시 비교
- target device에 `stat()`을 수행하고 block device인지 확인
- apply 직전 전체 safety collection을 한 번 더 반복하여 TOCTOU window를 축소
- post-apply verification에서도 동일 identity를 다시 확인

### System and mounted disk protection

- `/proc/self/mountinfo`의 major:minor를 `/sys/dev/block` symlink와 연결
- `/`, `/boot`, `/boot/efi`가 target whole disk에 속하면 거부
- target disk가 다른 mount point에서 사용 중이어도 거부
- protected filesystem은 단순 ext4/xfs와 EFI vfat 계열만 판단 가능
- dm/md/virtual/stacked block hierarchy 또는 block-backed mount 판별 실패는 UNKNOWN으로 거부
- root mount를 찾지 못해도 UNKNOWN으로 거부

### Existing partition restriction

- initial sysfs scan에서 기존 partition이 하나라도 있으면 apply plan 생성 거부
- F10 직전 sysfs partition rescan에서 partition이 하나라도 나타나면 executor 호출 전 거부
- 기존 partition delete/edit/preserve 실제 적용은 M7 범위 밖
- Preview는 ineligible 이유를 표시하고 Continue를 허용하지 않음

### Plan and sector restrictions

- full M6 validator와 complete Windows layout helper를 모두 통과해야 함
- EFI/MSR/Windows/Recovery 순서, role, GUID, name, non-overlap, disk bounds 검사
- 각 start sector는 2048-sector(1 MiB) boundary여야 함
- disk 처음과 끝에 각각 최소 2048 sectors를 예약
- sysfs logical block size가 정확히 512 bytes가 아니거나 읽기 실패하면 거부
- renderer가 허용하는 name/GUID는 builder가 만든 고정 값뿐이며 user text를 script에 삽입하지 않음

### F10 confirmation

- Preview ENTER는 Confirmation으로만 이동하며 executor 호출 없음
- Confirmation ENTER는 동작 없음
- `KEY_F(10)`만 app의 `classicsetup_execute_apply_plan()` 호출 경로로 연결
- B는 이전 단계, F3는 공통 Quit Confirmation으로 유지
- F10 뒤에도 모든 safety check를 처음부터 두 번 수행한 뒤 process 실행

### Process restrictions

- `system()`, `popen()`, shell command string 사용 없음
- `/usr/sbin/sfdisk` 또는 `/sbin/sfdisk`의 executable 존재를 확인하고 절대 경로로 `execv()`
- arguments는 `--lock --wipe never <validated /dev/name>`로 고정
- `--wipe-partitions`, filesystem signature wipe, mkfs, mount 옵션은 사용하지 않음
- process failure 또는 nonzero status는 success로 처리하지 않음

## Functions and logic

### `classicsetup_detect_environment(enum classicsetup_environment *environment)` / `classicsetup_detect_environment_from(...)`

- File: `src/core/environment.c`
- Role: WSL/VirtualBox/VMware/Unknown 환경 판별
- Input: production fixed paths 또는 test fixture paths, output enum
- Return: argument 오류 `-1`, 탐지 수행 `0`; 불충분한 정보는 UNKNOWN
- Logic:
  1. version과 osrelease 두 파일을 모두 읽는다.
  2. Microsoft/WSL marker를 case-insensitive하게 먼저 찾는다.
  3. WSL이 아니면 세 DMI file에서 VirtualBox/VMware marker를 찾는다.
  4. 판별 실패는 UNKNOWN으로 남긴다.
- Uses: `fopen()`, `fgets()`, `tolower()`, sysfs/procfs
- Connected to: `collect_apply_safety()`, environment fixture tests

### `classicsetup_environment_allows_apply(...)` / `classicsetup_destructive_unlock_enabled(...)`

- File: `src/core/environment.c`
- Role: VM allowlist와 exact unlock value 검사
- Input: environment enum 또는 environment value string
- Return: 통과 여부
- Logic:
  1. VirtualBox/VMware만 allowlist에 포함한다.
  2. unlock은 `strcmp(value, "YES") == 0`만 허용한다.
- Uses: environment enum, `strcmp()`
- Connected to: pure safety evaluator, unit tests

### `classicsetup_check_system_disk(const char *target_disk_name)` / `classicsetup_check_system_disk_from(...)`

- File: `src/core/system_disk.c`
- Role: target이 running Linux 또는 mounted disk인지 fail-closed 판별
- Input: target whole-disk name, mountinfo/sys-dev-block paths
- Return: SAFE, TARGET_IN_USE, UNKNOWN
- Logic:
  1. mountinfo에서 major:minor와 mount point를 읽는다.
  2. `/sys/dev/block/<major:minor>` symlink의 `/block/<disk>` component를 추출한다.
  3. target과 같은 disk의 모든 mount를 TARGET_IN_USE로 처리한다.
  4. root/boot/EFI mount source를 확인하고 unsupported stacked hierarchy는 UNKNOWN 처리한다.
  5. root mount 미발견, read error, nonzero-major unresolved mount는 UNKNOWN 처리한다.
- Uses: `fopen()`, `fgets()`, `sscanf()`, `readlink()`, mountinfo, sysfs
- Connected to: safety collection의 system disk gate, temp symlink fixture tests

### `guid_for_role(...)` / `name_for_role(...)`

- File: `src/core/apply.c`
- Role: supported automatic role을 고정 GPT metadata로 mapping
- Input: partition role
- Return: constant GUID/name 또는 unsupported `NULL`
- Logic:
  1. EFI, MSR, Windows, Recovery만 mapping한다.
  2. Generic/NONE은 M7 실제 apply에서 거부한다.
- Uses:
  - EFI: `c12a7328-f81f-11d2-ba4b-00a0c93ec93b`
  - MSR: `e3c9e316-0b5c-4db8-817d-f92df00215ae`
  - Windows Basic Data: `ebd0a0a2-b9e5-4433-87c0-68b6b72699c7`
  - Recovery: `de94bba4-06d1-4d40-a16a-bfd50179d6ac`
- Connected to: apply plan builder/validator/renderer golden test
- Reference: Microsoft `PARTITION_INFORMATION_GPT`, `MBR2GPT`, UEFI/GPT partition guidance

### `classicsetup_build_apply_plan(...)`

- File: `src/core/apply.c`
- Role: mutable planned layout을 검증된 restricted immutable apply plan으로 변환
- Input: selected disk snapshot, partition plan, original partition count, output apply plan
- Return: 성공 `0`, unsupported/invalid `-1`
- Logic:
  1. disk identity shape와 M6 plan validator를 검사한다.
  2. original partition count 0과 complete automatic layout을 요구한다.
  3. temporary apply plan에 NEW automatic role만 순서대로 복사한다.
  4. role별 GUID/name과 sector range를 기록한다.
  5. apply validator 성공 후에만 caller output에 commit한다.
  6. 실패 시 caller output과 source plan을 변경하지 않는다.
- Uses: struct copy, M6 validator/layout helper, role mapping
- Connected to: app Preview 진입, apply plan/generic/existing/incomplete tests

### `classicsetup_validate_apply_plan(const struct classicsetup_apply_plan *apply_plan)`

- File: `src/core/apply.c`
- Role: renderer/executor가 받는 immutable contract 검사
- Input: apply plan
- Return: valid boolean
- Logic:
  1. `/dev/<safe-name>` identity와 disk size/sector count를 검사한다.
  2. 정확히 4 partitions와 EFI→MSR→Windows→Recovery 순서를 요구한다.
  3. role별 GUID와 fixed name이 정확한지 검사한다.
  4. 1 MiB alignment, nonzero range, no overlap, disk bounds를 검사한다.
  5. 앞뒤 GPT reservation 1 MiB를 침범하면 거부한다.
- Uses: role mapping, sector arithmetic
- Connected to: builder, renderer, safety, post-write verifier

### `classicsetup_render_sfdisk_script(...)`

- File: `src/core/apply.c`
- Role: validated apply plan을 sfdisk named-field script로 변환하는 pure renderer
- Input: const apply plan, caller buffer/capacity
- Return: 성공 `0`, invalid/truncation `-1`
- Logic:
  1. apply plan을 재검증한다.
  2. `label: gpt`, `unit: sectors` header를 출력한다.
  3. 각 partition의 explicit start/size/type/name line을 순서대로 출력한다.
  4. `vsnprintf()` truncation을 검사한다.
- Uses: `vsnprintf()`, fixed GPT metadata
- Connected to: executor stdin, exact golden script test

### `classicsetup_disk_identity_matches(...)` / `classicsetup_revalidate_target_disk(...)`

- File: `src/core/apply.c`
- Role: selected snapshot을 current sysfs/block node와 비교
- Input: selected/current disk or selected disk/output current
- Return: match boolean 또는 성공 `0`/실패 `-1`
- Logic:
  1. `/sys/block`을 다시 scan한다.
  2. name/path/size/model을 모두 비교한다.
  3. selected `/dev` path에 `stat()`을 수행한다.
  4. `S_ISBLK`가 아니면 거부한다.
- Uses: M4 disk scanner, `stat()`, `S_ISBLK`
- Connected to: pre-apply safety 두 회, post-apply verification, identity mismatch tests

### `target_uses_512_byte_logical_sectors(...)`

- File: `src/core/apply.c`
- Role: M6/M7 sector arithmetic과 target logical sector size 일치 검사
- Input: disk name
- Return: sysfs value가 정확히 512일 때만 참
- Logic:
  1. `/sys/block/<name>/queue/logical_block_size`를 읽는다.
  2. trailing whitespace를 제거해 numeric value를 확인한다.
  3. read/parse failure와 4Kn 등 non-512 값을 거부한다.
- Uses: `fopen()`, `fgets()`, `strtoul()`, sysfs
- Connected to: safety collection

### `classicsetup_evaluate_apply_safety(const struct classicsetup_apply_safety_inputs *inputs)`

- File: `src/core/apply.c`
- Role: side effect 없는 ordered safety decision
- Input: gathered environment/unlock/identity/system/partition/sector/plan/tool facts
- Return: first blocking safety code 또는 OK
- Logic:
  1. WSL을 최우선으로 차단한다.
  2. VM allowlist와 unlock을 검사한다.
  3. identity와 system disk 판단을 검사한다.
  4. existing partitions와 logical sector size를 검사한다.
  5. apply plan과 tool availability를 마지막으로 검사한다.
- Uses: environment/system status enum
- Connected to: executor의 두 safety passes, safety matrix tests

### `collect_apply_safety(...)`

- File: `src/core/apply.c`
- Role: production paths에서 safety facts를 fail-closed로 수집
- Input: apply plan, resolved sfdisk path, output safety inputs
- Return: 없음; 실패 fact는 false/UNKNOWN
- Logic:
  1. environment/unlock/plan/tool facts를 수집한다.
  2. WSL이면 block-device read 단계 전에 반환한다.
  3. target identity와 mounted/system disk status를 확인한다.
  4. current partitions를 sysfs에서 다시 scan한다.
  5. logical block size를 확인한다.
- Uses: environment, disk, partition, system-disk modules, `getenv()`
- Connected to: executor pre-render pass와 immediate pre-process pass

### `classicsetup_run_process_with_input(...)`

- File: `src/core/process.c`
- Role: shell 없이 bounded stdin/stdout child process 실행
- Input: absolute executable, argv array, stdin string, process result
- Return: wrapper 성공 `0`, pipe/fork/I/O/wait 실패 `-1`
- Logic:
  1. stdin pipe와 combined stdout/stderr pipe를 만든다.
  2. `fork()` 후 child에서 `dup2()`로 descriptors를 연결한다.
  3. `execv()` 실패 시 exit 127, setup 실패 시 126으로 종료한다.
  4. parent는 SIGPIPE를 임시 무시하고 input을 모두 쓴다.
  5. output을 bounded buffer에 capture하고 나머지를 drain한다.
  6. `waitpid()` 후 normal exit/signal/status를 구조체에 기록한다.
- Uses: `pipe()`, `fork()`, `dup2()`, `execv()`, `read()`, `write()`, `waitpid()`, `sigaction()`
- Connected to: sfdisk executor, `/bin/true`/`/bin/false` process tests

### `classicsetup_verify_partition_ranges(...)` / `verify_applied_layout(...)`

- File: `src/core/apply.c`
- Role: sfdisk 성공 뒤 sysfs result와 immutable plan 비교
- Input: apply plan과 scanned partitions
- Return: exact count/start/size match 여부
- Logic:
  1. apply plan을 재검증한다.
  2. partition count가 정확히 4인지 검사한다.
  3. start sector와 sector count를 순서대로 비교한다.
  4. production wrapper는 disk identity를 재확인하면서 100 ms 간격으로 최대 10회 rescan한다.
- Uses: M5 partition scanner, `nanosleep()`
- Connected to: executor success condition, range verification mock test

### `classicsetup_execute_apply_plan(...)`

- File: `src/core/apply.c`
- Role: F10 뒤 safety, sfdisk process, post-write verification을 조정
- Input: const apply plan, output apply result
- Return: API argument 오류 `-1`, handled success/block/failure `0`
- Logic:
  1. result를 NOT_RUN으로 초기화한다.
  2. 첫 safety collection/evaluation을 수행한다.
  3. validated sfdisk script를 render한다.
  4. process 직전에 전체 safety를 다시 수집/evaluate한다.
  5. absolute `sfdisk --lock --wipe never <device>`를 child로 실행한다.
  6. wrapper/exit failure를 PROCESS_FAILED로 저장한다.
  7. sysfs rescan/range match 실패를 VERIFY_FAILED로 저장한다.
  8. 모든 단계 통과만 SUCCESS로 저장한다.
- Uses: safety collector, renderer, process wrapper, post verifier
- Connected to: app F10 handler, WSL rejection test

### `classicsetup_show_apply_preview(...)`

- File: `src/tui/apply.c`
- Role: disk identity와 planned GPT layout read-only preview
- Input: apply plan, eligibility flag
- Return: Continue, Back, Quit
- Logic:
  1. warning과 disk path/model/capacity를 표시한다.
  2. EFI/MSR/Windows/Recovery와 binary size를 표시한다.
  3. invalid plan은 Continue를 비활성화한다.
  4. ENTER는 Confirmation으로만 이동한다.
- Uses: ncurses, const apply plan
- Connected to: app `show_apply_preview()`; executor 호출 없음

### `classicsetup_show_apply_confirmation(...)`

- File: `src/tui/apply.c`
- Role: 최종 destructive F10 confirmation
- Input: validated apply plan
- Return: Apply, Back, Quit
- Logic:
  1. target path/model/size와 data loss warning을 표시한다.
  2. ENTER와 ESC를 처리하지 않는다.
  3. F10만 APPLY 결과를 반환한다.
  4. B/F3는 기존 의미를 유지한다.
- Uses: ncurses `KEY_F(10)`, `KEY_F(3)`
- Connected to: app executor call site

### `classicsetup_show_apply_result(...)`

- File: `src/tui/apply.c`
- Role: blocked/process/verify/success 결과 표시
- Input: apply result
- Return: success Continue, Back, Quit
- Logic:
  1. safety block은 reason-specific message를 표시한다.
  2. process failure는 rollback 미수행을 표시한다.
  3. verify failure와 success를 구분한다.
  4. 실패 시 ENTER 진행을 허용하지 않는다.
- Uses: apply result/safety message
- Connected to: Apply Result state

### app/state changes

- Files: `src/app.c`, `src/state.c`
- Role: Preview/Confirmation/Result state orchestration
- Input: TUI result와 config apply state
- Return: setup state transition
- Logic:
  1. Format Continue 후 Apply Preview로 이동한다.
  2. Preview에서 apply plan을 rebuild하고 eligibility를 결정한다.
  3. Confirmation F10에서만 executor를 호출한다.
  4. Result success만 placeholder로 진행하며 failure는 Back/Quit만 허용한다.
  5. Partition 재진입, target 변경, format 변경, Undo에서 stale apply state를 clear한다.
- Uses: new state enum, config apply fields
- Connected to: main app loop

### `classicsetup_plan_create_windows_layout(...)` M7 edge reservation 변경

- File: `src/core/partition_plan.c`
- Role: automatic layout을 GPT-apply 가능한 usable range에 배치
- Input: selected Unallocated range
- Return: 기존 transactional result
- Logic:
  1. first automatic partition start를 최소 sector 2048로 올린다.
  2. disk 끝까지 이어지는 free range에서는 마지막 2048 sectors를 제외한다.
  3. 기존 EFI/MSR/Windows/Recovery 계산과 validator를 유지한다.
- Uses: 2048 sectors per MiB policy
- Connected to: M6.3 Unallocated ENTER, M7 apply plan validator/golden test

## Data structures

### `enum classicsetup_environment`

- `UNKNOWN`, `WSL`, `VIRTUALBOX`, `VMWARE`
- UNKNOWN은 bare metal과 판별 실패를 함께 나타내며 실제 apply 불가

### `struct classicsetup_apply_partition`

- `role`: EFI/MSR/Windows/Recovery
- `start_sector`, `sector_count`: immutable executor range
- `type_guid`: validated GPT partition type GUID
- `name`: validated fixed GPT partition name

### `struct classicsetup_apply_plan`

- `target_disk`: name/path/model/size snapshot
- `partitions[]`, `partition_count`: exact executor input
- `disk_sector_count`: range validation bound
- mutable M6 plan과 별도이며 const pointer로 renderer/executor에 전달

### `enum classicsetup_apply_safety_code`

- OK, WSL, unsupported VM, locked, disk identity, system disk, system unknown, existing partitions, unsupported sector size, invalid plan, tool unavailable
- UI result message와 test assertion에 사용

### `struct classicsetup_apply_safety_inputs`

- environment, unlock, identity, system status, existing count, 512-sector support, plan validity, tool availability
- pure ordered safety evaluation input

### `enum classicsetup_apply_result_code` / `struct classicsetup_apply_result`

- NOT_RUN, BLOCKED, PROCESS_FAILED, VERIFY_FAILED, SUCCESS
- safety code, detected environment, child exit/signal/output를 보존

### `struct classicsetup_process_result`

- normal exit 여부/status, signal 여부/number, bounded combined output

### config/state additions

- `classicsetup_config`: apply plan, `has_apply_plan`, apply result 추가
- setup states: APPLY_PREVIEW, APPLY_CONFIRMATION, APPLY_RESULT 추가
- stale partition/format state 변경 시 apply state도 clear

## Key mapping

| Screen | ENTER | F10 | B | ESC | F3 |
|---|---|---|---|---|---|
| Partition | Install/auto layout | - | Disk | 기본 화면 동작 없음 | Quit request |
| Format | Continue to Preview | - | - | Partition cancel | Quit request |
| Apply Preview | Confirmation | - | Format | 동작 없음 | Quit request |
| Apply Confirmation | 동작 없음 | Apply request | Preview | 동작 없음 | Quit request |
| Apply Result success | Continue | - | Preview | 동작 없음 | Quit request |
| Apply Result failure | 동작 없음 | - | Preview | 동작 없음 | Quit request |
| Common Quit Confirmation | - | - | - | Continue Setup | Confirm Quit |

## Control/Data flow

`M6 planned layout -> full plan validate -> original partition count check -> automatic layout check -> temporary immutable apply plan -> role/GUID/name/range validate -> Apply Preview -> ENTER -> F10 Confirmation -> F10 -> production safety collection -> render sfdisk script -> immediate second safety collection -> fork/execv sfdisk -> waitpid/exit status -> disk identity revalidation -> sysfs partition rescan -> expected range verification -> Apply Result`

WSL:

`F10 -> detect /proc Microsoft/WSL marker -> WSL safety code -> BLOCKED result -> no disk scan requiring write -> no process -> no sfdisk`

Back:

`Confirmation B -> Preview -> B -> Format -> ESC -> Partition`

Quit:

`Preview/Confirmation/Result F3 -> common Quit Confirmation -> F3 Exit / ESC same state`

## Process execution

- renderer output은 parent memory buffer에만 존재
- parent가 두 pipes를 만들고 child를 fork
- child stdin은 input pipe, stdout/stderr는 output pipe로 `dup2`
- child는 shell 없이 absolute sfdisk path와 argv array를 `execv`
- parent는 script bytes를 pipe로 쓰고 descriptor를 닫음
- parent는 최대 2047 output bytes를 보존하고 초과 output은 drain
- parent는 `waitpid` EINTR을 재시도하고 exit/signal status를 저장
- executable arguments:
  - `/usr/sbin/sfdisk` 또는 `/sbin/sfdisk`
  - `--lock`
  - `--wipe never`
  - validated `/dev/<disk-name>`
- generated script header: `label: gpt`, `unit: sectors`
- partition lines: explicit `start=`, `size=`, `type=`, fixed `name=`
- process 실패 뒤 자동 rollback은 수행하지 않음

## Build/test result

- CMake Debug clean verbose build 성공
- C17, `-Wall -Wextra -Wpedantic`, compile warning 없음
- CTest: 6/6 통과
  - 기존 state/disk/partition/plan/format tests 유지
  - 신규 `apply_safety_and_render` 통과
- ASan/UBSan Debug build 성공, `ASAN_OPTIONS=detect_leaks=0` CTest 6/6 통과
- LeakSanitizer는 현재 ptrace 환경에서 동작하지 않아 leak detection만 비활성화
- environment fixtures: WSL/VirtualBox/VMware/Unknown 및 unreadable proc fail-closed 통과
- safety matrix: WSL, unknown, VM allow, unlock values, identity mismatch, system disk/unknown, existing partitions, 512-sector restriction, invalid plan, tool unavailable 통과
- apply plan: ordered roles, exact GUIDs, GPT edge reservation, incomplete/existing/Generic rejection, transactional output 통과
- exact sfdisk golden script test 통과
- process wrapper: `/bin/true` success와 `/bin/false` nonzero exit capture 통과
- system disk tests: root target block, other disk safe, mounted target block, missing mapping unknown, dm/virtual unknown 통과
- post-apply range verification mock: exact match success, count/range mismatch failure 통과
- actual WSL test에서 `CLASSICSETUP_ALLOW_DESTRUCTIVE=YES`를 주어도 BLOCKED/WSL이고 process result는 untouched임을 확인
- actual TUI: Preview 표시, Confirmation ENTER 무시, F10 후 WSL block Result 확인
- WSL F10 확인 전후 `/sys/block/sdd`에 child partition entry 없음 확인
- 현재 환경에서 실제 `/dev` 대상 `sfdisk` 실행 없음
- 실제 VirtualBox/VMware destructive apply는 자동 또는 Codex 테스트로 수행하지 않음

## Manual VM test instructions

1. VirtualBox 또는 VMware VM snapshot을 만든다. UEFI firmware를 사용한다.
2. Disk 0은 Linux/ClassicSetup boot 전용으로 유지하고, 완전히 빈 32~64 GiB Disk 1을 새로 추가한다.
3. VM 안에서 `sfdisk`가 포함된 util-linux package와 ncurses 개발 package를 준비하고 Debug build/CTest를 먼저 통과시킨다.
4. Disk 1의 hypervisor size/model을 기록해 ClassicSetup Disk Selection에서 교차 확인한다.
5. VM shell에서 직접 `export CLASSICSETUP_ALLOW_DESTRUCTIVE=YES`를 설정한다.
6. root 권한이 필요하면 해당 변수만 보존하여 `sudo --preserve-env=CLASSICSETUP_ALLOW_DESTRUCTIVE ./build/classicsetup`을 실행한다.
7. Disk 1만 선택하고 Unallocated ENTER로 automatic Windows layout을 만든다.
8. Format plan을 고른 뒤 Preview의 device path/model/size와 4개 partition을 다시 확인한다.
9. Confirmation에서 Disk 0이 아닌지 마지막으로 확인한다. ENTER는 적용하지 않는다.
10. 테스트를 진행할 때만 F10을 누른다. Result가 SUCCESS와 verified layout을 표시하는지 확인한다.
11. 실패 시 VM을 중지하고 snapshot으로 복구한다. 같은 disk에 즉시 재시도하거나 수동 rollback하지 않는다.
12. M7은 filesystem을 만들지 않으므로 성공 뒤에도 실제 Windows install/boot를 시도하지 않는다.

## Topics for ChatGPT to explain

- `classicsetup_run_process_with_input()`의 fork/pipe/dup2/execv/waitpid 흐름
- shell command 조합과 argv 기반 `execv()`의 차이
- `classicsetup_build_apply_plan()`에서 mutable working plan과 immutable executor contract를 분리한 이유
- `collect_apply_safety()` 두 회와 disk revalidation에서 남는 TOCTOU 문제
- environment/system disk 판단 실패를 거부하는 fail-closed safety
- `guid_for_role()`의 EFI/MSR/Basic Data/Recovery GPT type 의미
- `classicsetup_render_sfdisk_script()`의 named-field script와 explicit sector range
- process normal exit, nonzero exit, signal을 구분하는 result 구조
- Preview ENTER와 Confirmation F10을 분리한 destructive confirmation 설계
- `verify_applied_layout()`의 post-write sysfs range verification과 검증 한계

## Issues/cautions

- 실제 VirtualBox/VMware disk apply는 아직 수행/검증하지 않음
- current partition table backup과 automatic rollback은 구현하지 않음
- process/verify 실패 시 disk가 부분 변경됐을 수 있으므로 VM snapshot 복구 필요
- 기존 partition preserve/delete/edit 실제 적용 미지원
- Generic NEW 실제 적용 미지원
- 정확히 하나의 automatic EFI/MSR/Windows/Recovery layout만 지원
- 512-byte logical sector disk만 지원; 4Kn은 거부
- system disk 판별은 ext4/xfs root와 단순 block hierarchy 중심; dm/md/LVM/RAID/복잡한 mount는 안전하게 거부
- disk serial/WWN을 아직 저장하지 않아 identity는 name/path/size/model 기반
- TOCTOU window를 재검증으로 줄였지만 kernel device replacement를 완전히 제거하지 못함
- Recovery GPT attributes는 아직 설정하지 않고 type GUID만 설정
- sfdisk `--wipe never`를 사용하므로 기존 signature가 있는 nominally empty disk에서 tool이 실패할 수 있음
- partition table backup/restore 기능 없음
- 실제 FAT32/NTFS formatting 없음; format plan만 메모리에 유지
- EFI/MSR/Recovery filesystem 및 attribute 적용은 M8 이후
- 실제 Windows image apply, boot files, Windows boot 가능 여부 미검증
