# ClassicSetup Handoff

## Current milestone

- M8 완료
- M7/M7.5 partition apply 뒤 filesystem apply를 별도 Preview/Confirmation/Result 단계로 추가
- 실제 destructive format은 UEFI/GPT + 허용된 VMware/VirtualBox 테스트 VM에서만 가능하며 BIOS/MBR와 WSL은 차단

## Changed files

- Build/docs: `CMakeLists.txt`, `README.md`, `.ai/handoff.md`
- New API: `include/classicsetup/format_apply.h`, `include/classicsetup/format_apply_tui.h`
- New implementation: `src/core/format_apply.c`, `src/tui/format_apply.c`
- App/config/state: `include/classicsetup/config.h`, `include/classicsetup/state.h`, `src/app.c`, `src/config.c`, `src/state.c`
- Process wrapper: `include/classicsetup/process.h`, `src/core/process.c`
- Existing TUI text: `include/classicsetup/after_format.h`, `src/tui/after_format.c`, `src/tui/apply.c`
- Tests: `tests/format_apply_test.c`, `tests/partition_plan_test.c`, `tests/state_test.c`

## Implementation result

### Format policy and data

- Immutable `classicsetup_format_apply_plan`은 검증된 M7 `partition_apply_plan`, formatter 대상, exact sysfs device path/range를 보존한다.
- GPT formatter 순서: EFI FAT32 Quick -> Windows NTFS Quick/Full -> Recovery NTFS Quick. MSR에는 formatter를 호출하지 않고 filesystem이 없음을 검증한다.
- MBR plan은 System Reserved NTFS Quick -> Windows NTFS Quick/Full -> Recovery NTFS Quick으로 생성/preview 가능하지만 executor 진입 전에 `MBR_NOT_ENABLED`로 차단한다.
- 고정 label: EFI/System Reserved=`SYSTEM`, Windows=`Windows`, Recovery=`Recovery`.
- `classicsetup_format_result`는 overall code, failed role, safety code, child/verification status와 output, 완료 partition 수를 유지한다.

### Functions and logic

| Function | File | Role / flow |
|---|---|---|
| `classicsetup_build_format_apply_plan(...)` | `src/core/format_apply.c` | immutable M7 plan과 role format policy를 받고, scan된 partition을 range로 exact match한 temporary plan을 검증 후 commit한다. GPT MSR은 별도 no-filesystem target으로 보존한다. |
| `classicsetup_match_partition_device(...)` | `src/core/format_apply.c` | start sector + sector count가 유일하게 일치하는 sysfs scan 결과의 실제 `/dev` path를 반환한다. `sdX`, NVMe, MMC 이름을 조합하지 않는다. |
| `classicsetup_validate_format_apply_plan(...)` | `src/core/format_apply.c` | embedded apply plan, role/order, filesystem/mode/label, range/path uniqueness, GPT MSR 또는 MBR no-MSR invariant를 검증한다. |
| `classicsetup_resolve_format_tools(...)` | `src/core/format_apply.c` | 허용된 absolute path에서 `mkfs.fat`/`mkfs.vfat`, `mkfs.ntfs`, `blkid` executable을 찾는다. 누락 시 fail-closed다. |
| `classicsetup_build_format_arguments(...)` | `src/core/format_apply.c` | FAT32는 `-F 32 -n SYSTEM`, NTFS Quick은 `-f -L`, Full은 quick flag 없이 `-L` argv를 만든다. shell text를 만들지 않는다. |
| `classicsetup_build_blkid_arguments(...)` | `src/core/format_apply.c` | `blkid -p -o value -s TYPE <exact-device>` argv를 만든다. |
| `classicsetup_check_device_mounted_from(...)` | `src/core/format_apply.c` | `/proc/self/mountinfo`의 major:minor를 검사하고 mounted/not-mounted/unknown을 반환한다. parse/read 실패는 unknown이다. |
| `classicsetup_evaluate_format_safety(...)` | `src/core/format_apply.c` | WSL, VM allowlist, unlock, disk identity, system disk, 512-byte sector, plan/layout/device, mount, tool 조건을 순서대로 fail-closed 평가한다. MBR는 process 이전에 차단한다. |
| `collect_real_safety(...)` | `src/core/format_apply.c` | 각 target 직전에 environment/disk/sysfs/mount/tool 상태를 다시 수집한다. executor가 partition마다 두 번 호출한다. |
| `classicsetup_execute_format_apply_plan_with_ops(...)` | `src/core/format_apply.c` | 테스트 가능한 executor. target마다 double safety -> formatter -> verifier 순서이며 실패 즉시 이후 target을 중단한다. MSR은 double safety -> no-filesystem verification만 수행한다. |
| `classicsetup_execute_format_apply_plan(...)` | `src/core/format_apply.c` | 실제 callback을 연결한다. 기존 fork/execv process wrapper를 사용하며 shell/system/popen은 사용하지 않는다. |
| `classicsetup_run_process(...)` | `src/core/process.c` | stdin이 필요 없는 formatter/blkid용 wrapper이며 기존 pipe/fork/execv/waitpid 구현에 빈 입력을 전달한다. |
| `classicsetup_show_format_apply_preview(...)` | `src/tui/format_apply.c` | disk/scheme, exact path, FAT32/NTFS mode, MSR skip과 partial-retry 경고를 표시한다. ENTER/B/Q만 처리한다. |
| `classicsetup_show_format_apply_confirmation(...)` | `src/tui/format_apply.c` | destructive 경고 후 A/a만 APPLY를 반환한다. ENTER는 실행하지 않고 B/Q만 추가 처리한다. |
| `classicsetup_show_format_apply_result(...)` | `src/tui/format_apply.c` | success verification 또는 blocked/failed role/부분 완료 경고를 표시한다. 실패 시 ENTER 진행을 막는다. |

### Process execution and Quick/Full

- 기존 `classicsetup_run_process_with_input()`의 pipe/fork/execv/waitpid 및 stdout/stderr capture를 재사용한다.
- executable과 argv는 검증된 absolute path와 고정 option/label로 구성하며 shell을 통하지 않는다.
- NTFS Quick은 현재 `mkfs.ntfs` 문서의 `-f`를 사용한다. Full은 `-f/-Q` 없이 호출하는 ClassicSetup 정책이며 Windows Setup의 full format과 동일하다고 보장하지 않는다.
- FAT32는 `mkfs.fat`/`mkfs.vfat -F 32`; 강제 우회용 formatter option은 사용하지 않는다.
- formatter exit 0 뒤에도 `blkid` TYPE을 확인한다. MSR은 `blkid`가 type을 식별하지 못한 exit 2를 성공 조건으로 사용한다.

### Safety and control/data flow

- GPT: Partition Apply SUCCESS -> sysfs rescan -> immutable format plan -> Format Preview -> A confirmation -> per-partition double safety -> EFI formatter/verify -> Windows formatter/verify -> Recovery formatter/verify -> MSR no-filesystem verify -> Result.
- MBR: format plan/preview 가능 -> A -> `CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED` -> formatter callback/process 0회.
- M7 safety를 유지한다: WSL block, VMware/VirtualBox allowlist, explicit unlock, disk identity revalidation, root/system/target mount protection, 512-byte sector, immutable plan, shell 없는 exec.
- M8 추가 safety: exact child range/path matching, block-device 재확인, partition별 mount 검사, formatter/verifier availability, 실행 직전 두 번째 safety collection.
- 중간 실패 시 이후 formatter를 실행하지 않는다. 성공한 filesystem은 rollback하지 않으며 result/UI가 partial state를 알린다.

### Key mapping

| Screen | ENTER | B | ESC | Q | A |
|---|---|---|---|---|---|
| Partition Apply Result | Continue on success | Preview | - | Quit | - |
| Format Apply Preview | Continue | Partition Apply Result | - | Quit | - |
| Format Apply Confirmation | no action | Preview | - | Quit | Apply |
| Format Result | Continue on success only | Preview | - | Quit | - |

- 기존 Function-key-free `ENTER/B/ESC/Q/A/C/D/U` 정책과 공통 Quit Confirmation을 유지한다.

## Build/test result

- Clean CMake Debug build 성공: GCC 15.2, C17, `-Wall -Wextra -Wpedantic`, 경고 없음.
- CTest 9/9 통과: 기존 M7.5 suite + `format_apply`.
- ASan/UBSan Debug build 및 CTest 9/9 통과 (`ASAN_OPTIONS=detect_leaks=0`).
- 검증 항목: GPT/MBR format plan, NVMe/MMC exact range matching, mismatch transactional rejection, FAT32/NTFS Quick/Full/blkid argv golden, mountinfo, safety matrix, MBR process 0, double safety, process/verify 중간 실패 중단, MSR formatter 0/no-filesystem verification, WSL block.
- 현재 환경은 WSL2로 확인됨. 실제 `/dev` write, `sfdisk`, `mkfs.*`, destructive VM test는 실행하지 않았다.
- 현재 환경의 `mkfs.fat`/`mkfs.vfat`와 `mkfs.ntfs`는 미설치, `/usr/sbin/blkid`는 `util-linux`에서 확인. Ubuntu VM 준비 패키지: `dosfstools`, `ntfs-3g`, `util-linux`.
- Manual VMware: UEFI VM의 Linux boot disk와 별도 빈 GPT test disk를 준비 -> 패키지 설치 -> `CLASSICSETUP_ALLOW_DESTRUCTIVE=YES`를 사용자가 설정 -> UEFI/GPT 자동 layout -> NTFS mode -> partition A apply 성공 -> format preview 확인 -> format A apply -> EFI/Windows/Recovery verified 확인. 대상 device를 매 단계 재확인한다.

## Topics for ChatGPT to explain

- `classicsetup_format_apply_plan`: mutable config와 immutable execution input 분리.
- `classicsetup_match_partition_device()`: 이름 조합 대신 sector range로 NVMe/MMC path를 찾는 이유.
- `classicsetup_evaluate_format_safety()`/`collect_real_safety()`: M7 safety 재사용, partition mount 보호, double check와 TOCTOU 제한.
- `classicsetup_build_format_arguments()`: FAT32/NTFS와 Quick/non-quick argv 정책; MSR을 포맷하지 않는 이유.
- `classicsetup_run_process()`: shell 없이 fork/execv/waitpid로 formatter를 실행하는 흐름.
- `verify_real_filesystem()`: child exit status와 실제 signature/type verification을 함께 확인하는 이유.
- `classicsetup_format_result`: partial failure와 rollback 부재를 표현하는 방식.

## Issues/cautions

- BIOS/MBR partition apply와 filesystem apply는 destructive 실행이 계속 비활성화되어 있다.
- NTFS Full은 `mkfs.ntfs` non-quick invocation 정책이며 Windows Setup의 surface scan/full format과 동등하지 않을 수 있다.
- formatter 성공 뒤 후속 target 실패 시 부분 포맷 상태가 남고 자동 rollback은 없다.
- 실제 VMware UEFI/GPT filesystem apply는 사용자가 별도 test disk에서 검증해야 한다.
- filesystem UUID/label의 post-check는 필수 TYPE 검증 외에는 아직 하지 않는다.
- Windows image apply, boot files/BCD, mount 및 Windows 설치는 아직 구현하지 않았다.
