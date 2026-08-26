# ClassicSetup Handoff

## Current milestone

- M7 system-disk safety detection revision 완료
- VMware Ubuntu Desktop의 unrelated Snap loop mounts 오탐 수정

## Changed files

- `src/core/system_disk.c`
- `tests/apply_test.c`
- `.ai/handoff.md`

## Implementation result

- 실제 원인: 모든 nonzero-major mount를 whole disk로 해석하던 기존 로직이 `/dev/loop*`의 `/virtual/` sysfs 경로에서 실패한 뒤 즉시 UNKNOWN을 반환함
- mountinfo의 filesystem/source를 함께 파싱하여 unrelated `/dev/loop*`, squashfs/Snap, pseudo filesystem을 system-disk 판별 실패 원인에서 제외
- `/`, `/boot`, `/boot/efi`는 backing whole disk 해석 실패 시 계속 UNKNOWN
- critical mount 또는 임의 mount의 whole disk가 target이면 TARGET_IN_USE
- target child partition mount도 TARGET_IN_USE; unrelated physical disk mount는 SAFE 유지
- dm/LVM/md/RAID/기타 non-loop block mount가 해석되지 않으면 UNKNOWN을 유지하여 fail-closed 보존
- `/dev/sda2 -> sda` parent resolution, M7 apply/state/executor 로직 및 다른 safety gate는 변경하지 않음

## Build/test result

- CMake Debug build 성공; C17, `-Wall -Wextra -Wpedantic` 경고 없음
- CTest 8/8 통과
- ASan/UBSan build 및 CTest 8/8 통과 (`detect_leaks=0`)
- 신규 검증: root sda2 + target sdb + loop0~loop10 Snap mounts = SAFE
- 신규 검증: root/target 동일, target child mount = TARGET_IN_USE
- 신규 검증: unrelated sdc mount와 unresolved loop = SAFE
- 신규 검증: unresolved critical root, dm root, unresolved stacked block mount = UNKNOWN
- 실제 disk write 및 `sfdisk` 실행 없음

## Topics for ChatGPT to explain

- mountinfo의 major:minor, mount point, filesystem, source를 함께 사용하는 이유
- sysfs `/sys/dev/block/<major>:<minor>`에서 partition을 whole disk로 해석하는 흐름
- unrelated loop/pseudo 제외와 critical/stacked fail-closed를 동시에 유지하는 기준

## Issues/cautions

- 안전하게 해석되지 않는 non-loop block hierarchy는 target 연관성을 배제할 수 없어 UNKNOWN 처리
- dm/LVM/md/RAID parent graph의 완전한 재귀 해석은 아직 구현하지 않음
- 실제 VMware 환경 재검증은 사용자가 수행해야 하며 Codex는 destructive apply를 실행하지 않음
