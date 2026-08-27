# ClassicSetup Handoff

## Current milestone

- M9.1 완료: Recommended 최종 UX 방향, 확장 disk policy 구조, XP Setup 스타일 TUI 기반 정리.
- Recommended는 Keyboard를 건너뛰고 future GTK boundary 이후 Disk -> Network -> Windows Version -> Download -> Installation Options -> Summary로 진행한다.
- Advanced는 Keyboard -> Installation Mode -> Disk -> Partition -> Format -> M7/M8 apply 흐름을 유지한다.

## Changed files

- Policy/identity: `include/classicsetup/{disk,recommended}.h`, `src/core/{disk,recommended,apply}.c`.
- State/app/API: `include/classicsetup/{state,recommended_tui}.h`, `src/{state,app}.c`.
- TUI theme/screens: `include/classicsetup/tui.h`, `src/tui/{tui,welcome,setup_mode_selection,keyboard,install_mode_selection,disk_selection,partition_selection,format_selection,apply,format_apply,after_format,quit,recommended}.c`.
- Tests/docs: `tests/{recommended,state,apply}_test.c`, `README.md`, `.ai/handoff.md`.

## Implementation result

### UX direction and state flow

- `CLASSICSETUP_STATE_RECOMMENDED_GUI_TRANSITION` is the ncurses/GTK ownership boundary; no GTK initialization is performed yet.
- Recommended flow: Setup Mode -> GUI Transition -> Recommended Disk -> Network -> Windows Version -> Windows Download -> Install Options -> Summary -> Result -> Next Stage.
- Network/version/download/options are explicit placeholders and do not mutate external state.
- Advanced flow and ENTER/B/ESC/Q/A/C/D/U behavior are unchanged.

### Recommended disk classification

| Disk class | Detection fact | Policy | Selectable now | Future action |
|---|---|---|---|---|
| `RAW_EMPTY` | Safe scan, no partitions, sufficient identity | `AUTO_INSTALL_ALLOWED` | Yes, UEFI only | Existing GPT/Quick engine |
| `PARTITIONED_EMPTY` | Every partition explicitly confirmed empty | `REINITIALIZE_WITH_WARNING` | No | Warning/reinitialize |
| `WINDOWS` | Read-only evidence marks Windows | `KEEP_FILES_FUTURE` | No | Preserve/reinstall |
| `WINDOWS_ENCRYPTED_LOCKED` | Encryption evidence, locked | `EXPLICIT_ERASE_ONLY` | No | Recovery warning/explicit erase |
| `WINDOWS_ENCRYPTED_UNLOCKED` | Encryption evidence, unlocked | `KEEP_FILES_FUTURE` | No | Preserve analysis |
| `WINDOWS_COMPLEX` / `MULTI_OS` | Complex or multiple-OS evidence | `ADVANCED_ONLY` | No | Advanced diagnosis |
| `DATA_PRESENT` | Read-only user-data evidence | `EXPLICIT_ERASE_ONLY` | No | Backup/explicit erase |
| `UNKNOWN_FILESYSTEM` | Existing partitions without conclusive inspection | `ADVANCED_ONLY` | No | Future detector work |
| `SYSTEM` / `INSTALL_MEDIA` / `REMOVABLE` / `UNKNOWN` | Existing safety checks | `BLOCK` | No | Choose another disk |

- `classicsetup_disk_facts` stores discovered facts; `classicsetup_classify_disk_facts()` derives classification; `classicsetup_recommended_policy_for_disk()` derives action; presentation helpers supply UI text.
- Live sysfs assessment does not claim partitioned disks are empty or Windows. Without filesystem evidence it returns `UNKNOWN_FILESYSTEM`.
- Non-selectable ENTER never reaches plan creation or an executor.
- Legacy BIOS guidance now says to restart in UEFI mode or use Advanced; MBR destructive apply remains disabled.

### VM identity policy

- Physical/unknown environments still require model/path/name/size, serial or WWN, known 512-byte logical sector size, and known removable status.
- Allowlisted VMware/VirtualBox may use model, exact path/name, canonical sysfs path, size, 512-byte sector size, and non-removable status when serial/WWN is absent.
- VM fallback is used only after environment allowlist detection and safe system/mount classification; `RAW_EMPTY` proof is still required.
- Destructive revalidation now also compares canonical sysfs path. Existing M7/M8 checks remain authoritative.

### Functions and logic

| Signature | File | Role / connection |
|---|---|---|
| `classicsetup_classify_disk_facts(disk, facts, environment)` | `src/core/recommended.c` | Validates identity/system facts and derives a disk class. |
| `classicsetup_recommended_policy_for_disk(disk_class)` | `src/core/recommended.c` | Maps classification to automatic/warning/future/block action. |
| `classicsetup_disk_class_presentation(disk_class)` | `src/core/recommended.c` | Returns UI text without exposing enum names. |
| `classicsetup_recommended_policy_reason(disk_class, firmware)` | `src/core/recommended.c` | Returns fail-closed and Legacy BIOS guidance. |
| `classicsetup_assess_disk_in_environment(disk, env, assessment)` | `src/core/recommended.c` | Combines partition scan, mount status, class, policy, and presentation. |
| `classicsetup_disk_has_vm_test_identity(disk)` | `src/core/disk.c` | Checks restricted VM fallback identity. |
| `classicsetup_disk_identity_matches(selected, current)` | `src/core/apply.c` | Includes canonical sysfs path in destructive revalidation. |
| `classicsetup_next_state_for_setup_mode(state, event, mode)` | `src/state.c` | Branches Recommended before Keyboard and preserves Advanced M8 flow. |
| `classicsetup_tui_draw_frame/list_row/footer/warning(...)` | `src/tui/tui.c` | Shared safe drawing primitives for TUI screens. |
| `classicsetup_show_*_placeholder()` | `src/tui/recommended.c` | Defines future GUI product-state boundaries without external work. |

### TUI redesign

- Dark Setup blue, title underline, left-aligned instructions, bordered lists, full-row white/blue selection, yellow warnings, and gray bottom key bar mirror the XP text-mode setup structure.
- Setup Mode now explains Recommended versus Advanced.
- Welcome, setup/keyboard/install mode, disk/partition/format, apply/format preview-confirm-result, quit, and Recommended placeholders use common theme helpers where applicable.
- Drawing helpers clip text and omit invalid frames on small terminals instead of writing outside the screen.

### Safety and control flow

- Recommended Summary remains the only user-facing `A=Install` confirmation; executor calls are zero before it.
- Actual Recommended planning remains restricted to `RAW_EMPTY` + UEFI/GPT. Other classes are policy structure only.
- WSL block, VM allowlist, unlock, system/mount protection, range matching, immutable plans, double safety, shell-free exec, post-write/format verification, and MBR block were not weakened.
- No ISO/WIM work, Windows.old move, BitLocker unlock, shrink, GTK implementation, or boot configuration was added.

## Build/test result

- Clean Debug build succeeded: GCC 15.2, C17, `-Wall -Wextra -Wpedantic`, warning 0.
- CTest 10/10 passed: M7/M8 regressions plus state branching, disk classifications, policy separation, physical identity regression, VMware fallback, Legacy BIOS block, UEFI auto plan, orchestration short-circuit, and stale reset.
- ASan/UBSan Debug build and CTest 10/10 passed with leak detection disabled.
- TUI smoke passed: redesigned Welcome -> Q -> Quit Confirmation -> Q -> normal exit.
- `git diff --check` passed; no function-key, `system()`, or `popen()` path was introduced.
- Codex executed no destructive disk, `sfdisk`, formatter, or VMware apply test.

## Topics for ChatGPT to explain

- Disk facts -> classification -> action policy -> presentation separation and GTK reuse.
- Fail-closed disk UX and why uninspected partitioned disks become `UNKNOWN_FILESYSTEM`.
- Physical strong identity versus allowlisted VM fallback and canonical sysfs revalidation.
- Setup-mode state branching and the Recommended GUI boundary.
- Reusable ncurses theme/layout helpers and small-terminal clipping.
- Future read-only Windows/BitLocker evidence collectors without erase authorization.

## Issues/cautions

- Partitioned-empty detection is policy-ready but live detection is intentionally absent; low usage or unmounted state is not considered empty.
- Windows, multi-OS, data, and BitLocker classes require future read-only evidence collectors; uncertainty stays blocked.
- Windows preservation, Windows.old, explicit erase/reinitialize, backup, shrink, and BitLocker unlock are unimplemented.
- Recommended actual apply remains UEFI/GPT `RAW_EMPTY` only; BIOS/MBR destructive execution stays blocked.
- GTK, network, Windows source/download, image apply, unattended setup, and Windows boot configuration remain placeholders.
