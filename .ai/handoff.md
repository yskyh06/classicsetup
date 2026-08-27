# ClassicSetup Handoff

## Current milestone

- M9 완료
- Recommended/Advanced UX 분리, 보수적 disk classification, 자동 GPT/Quick 계획과 단일 Install 확인 오케스트레이션 구현
- Windows source와 TUI->GUI transition은 state/interface placeholder만 추가; download/image apply/GTK는 미구현

## Changed files

- New core/API: `include/classicsetup/setup_mode.h`, `include/classicsetup/recommended.h`, `src/core/setup_mode.c`, `src/core/recommended.c`
- New TUI: `include/classicsetup/setup_mode_selection.h`, `include/classicsetup/recommended_tui.h`, `src/tui/setup_mode_selection.c`, `src/tui/recommended.c`
- App/state/config: `include/classicsetup/config.h`, `include/classicsetup/state.h`, `src/app.c`, `src/config.c`, `src/state.c`
- Disk identity/revalidation: `include/classicsetup/disk.h`, `src/core/disk.c`, `src/core/apply.c`
- Build/docs: `CMakeLists.txt`, `README.md`, `.ai/handoff.md`
- Tests/fixtures: `tests/recommended_test.c`, `tests/state_test.c`, `tests/disk_test.c`, `tests/apply_test.c`, `tests/partition_plan_test.c`, `tests/fixtures/sys_block/{sda,nvme0n1}` identity files

## Implementation result

### Recommended vs Advanced

- `classicsetup_setup_mode`: `RECOMMENDED`(기본), `ADVANCED`.
- Welcome 다음 Setup Mode 화면에서 UP/DOWN, ENTER, Q로 선택한다.
- Recommended: Welcome -> Setup Mode -> Keyboard -> Recommended Disk -> Windows Source placeholder -> Install Summary -> Recommended Result -> GUI Transition placeholder.
- Recommended는 Installation Mode, Partition, Format, partition/format Preview를 노출하지 않는다.
- Advanced: Setup Mode/GUI boundary만 추가되고 M8의 Installation Mode -> Disk -> Partition -> Format -> 두 apply Preview/Confirmation/Result 경로와 C/D/U/B/Q/A 동작은 유지한다.
- setup mode 변경 시 selected disk, partition/format/apply/recommended snapshot을 초기화한다.

### Disk classification policy and identity

- `classicsetup_disk_class`: EMPTY, HAS_UNALLOCATED_SPACE, HAS_EXISTING_PARTITIONS, SYSTEM, INSTALL_MEDIA, REMOVABLE, UNKNOWN.
- Recommended selectable policy는 오직 `EMPTY`; 나머지는 화면에 이유를 표시하고 ENTER를 차단한다.
- system mount target은 SYSTEM, mounted removable target은 INSTALL_MEDIA, 독립 removable은 REMOVABLE로 분류한다.
- partition/sysfs/system mapping 실패 또는 identity 부족은 UNKNOWN; fail-open하지 않는다.
- Recommended strong identity는 name/path/model/size, serial 또는 WWN, 512-byte logical sector size, known removable flag를 요구한다.
- sysfs scanner가 `device/serial`, `device/wwid` 또는 `wwid`, `queue/logical_block_size`, `removable`, optional `device/transport`를 읽는다.
- M7/M8 disk revalidation은 기존 name/path/model/size에 선택 시 존재했던 serial, WWN, sector size, removable, transport를 추가 비교한다. Advanced의 기존 fallback은 유지한다.

### Functions and logic

| Function | File | Role |
|---|---|---|
| `classicsetup_default_setup_mode()` | `src/core/setup_mode.c` | Recommended 기본값 반환 |
| `classicsetup_detect_firmware_from()` | `src/core/recommended.c` | sysfs firmware root/efi directory로 UEFI, BIOS candidate, UNKNOWN 구분 |
| `classicsetup_classify_disk()` | `src/core/recommended.c` | identity, partition scan, unallocated 여부, system mount 상태를 disk class로 변환 |
| `classicsetup_assess_disk()` | `src/core/recommended.c` | partition scan + system-disk 검사 + class/selectable 결과 생성 |
| `classicsetup_disk_has_recommended_identity()` | `src/core/disk.c` | Recommended 자동 변경에 필요한 strong identity 확인 |
| `classicsetup_build_recommended_plan()` | `src/core/recommended.c` | UEFI + EMPTY만 temporary plan에서 EFI/MSR/Windows/Recovery, Windows target, role formats, immutable GPT apply plan 생성 후 commit |
| `populate_format_policy()` | `src/core/recommended.c` | role별 기존 M8 policy를 Quick mode로 생성; Windows NTFS Quick 자동 선택 |
| `classicsetup_execute_recommended_plan_with_ops()` | `src/core/recommended.c` | mock 가능한 orchestration: EMPTY 재검증 -> partition apply -> format plan -> format apply; 실패 시 즉시 후속 단계 중단 |
| `classicsetup_execute_recommended_plan()` | `src/core/recommended.c` | 기존 M7/M8 actual executor와 post-partition sysfs scan을 orchestration callbacks에 연결 |
| `classicsetup_recommended_result_can_continue()` | `src/core/recommended.c` | format까지 SUCCESS인 경우에만 next stage 진행 허용 |
| `classicsetup_config_set_setup_mode()` | `src/config.c` | UX mode commit과 stale state 제거 |
| `classicsetup_config_set_recommended_plan()` | `src/config.c` | 자동 plan을 config partition/selection/format/apply snapshot에 저장 |
| `classicsetup_next_state_for_setup_mode()` | `src/state.c` | Recommended/Advanced 분기와 Windows Source/GUI boundary 전이 |
| `classicsetup_show_recommended_disk_selection()` | `src/tui/recommended.c` | model/size/class 중심 disk UI; device path는 보조 표시; non-selectable ENTER 차단 |
| `classicsetup_show_install_summary()` | `src/tui/recommended.c` | 한 번의 destructive A=Install 확인; ENTER는 apply하지 않음 |

### Recommended orchestration and safety

- Disk ENTER 시 UEFI/EMPTY/strong identity를 다시 검증하고 기존 UEFI auto layout 및 M8 Quick policy로 immutable apply plan을 준비한다.
- Summary 전까지 executor 호출은 0회다. A 입력 뒤에만 orchestration을 시작한다.
- A 이후 target을 다시 scan/classify하여 여전히 exact same EMPTY disk인지 확인한다.
- partition apply가 SUCCESS일 때만 sysfs ranges를 실제 child paths에 match하여 immutable format apply plan을 만든다.
- format failure 또는 verification failure 시 GUI transition으로 진행하지 않는다.
- 기존 WSL block, VM allowlist, explicit unlock, system/mount protection, exact range matching, immutable plans, double safety, shell-free exec, post-write/format verify, MBR block을 모두 재사용한다.
- Recommended는 UI 확인 횟수만 한 번으로 줄이며 내부 safety check는 생략하지 않는다.

## Build/test result

- Clean CMake Debug build 성공: GCC 15.2, C17, `-Wall -Wextra -Wpedantic`, 경고 없음.
- CTest 10/10 통과: 기존 M8 suite + `recommended_policy`.
- ASan/UBSan Debug build 및 CTest 10/10 통과 (`ASAN_OPTIONS=detect_leaks=0`).
- 테스트: default mode, UEFI/BIOS/UNKNOWN detection, identity scanner, disk classes/selectability, EMPTY auto GPT/NTFS Quick, BIOS/existing reject, Recommended state skip, Advanced flow, A 전 invocation 0, execution order, partition failure format 0, format failure continuation block, mode-change reset.
- TUI smoke: Welcome Q -> common Quit Confirmation Q -> 정상 종료 확인.
- 현재 환경에서 실제 `/dev` write, `sfdisk`, `mkfs.*`, Windows source 처리, destructive VM test는 수행하지 않았다.

## Topics for ChatGPT to explain

- `classicsetup_classify_disk()`의 policy layer와 M7/M8 executor engine 분리.
- strong disk identity와 `classicsetup_disk_identity_matches()` 재검증이 TOCTOU 위험을 줄이는 방식.
- UNKNOWN/non-empty/system/removable을 non-selectable로 만드는 fail-closed UX.
- `classicsetup_build_recommended_plan()`이 Advanced partition/format 엔진을 자동 policy로 재사용하는 구조.
- Summary의 사용자 확인 1회와 executor 내부 다중 safety 검증의 차이.
- `classicsetup_execute_recommended_plan_with_ops()`의 orchestration 순서와 실패 단락.
- Windows Source/GUI Transition state가 향후 TUI와 GTK/core 경계를 만드는 방식.

## Issues/cautions

- Recommended actual apply는 strong identity가 있는 EMPTY UEFI/GPT disk만 지원한다.
- serial/WWN, logical sector, removable 정보를 sysfs에서 확인하지 못하면 Recommended에서 UNKNOWN으로 차단된다.
- 기존 partition 보존, 전체 erase 선택, HAS_UNALLOCATED_SPACE 자동 사용은 미구현이다.
- BIOS/MBR actual partition/format apply는 계속 비활성화되어 Advanced 안내만 가능하다.
- Windows download/ISO/image apply와 GTK GUI는 placeholder이며 실제 구현이 없다.
- partition apply 성공 후 format 실패 시 부분 변경 rollback은 없고 다음 단계 진행은 차단된다.
