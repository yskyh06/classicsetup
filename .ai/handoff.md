# ClassicSetup Handoff

## Current milestone

- M11 GTK Wizard Visual/UX Refinement 완료.
- Recommended는 GTK Disk부터, Advanced는 TUI storage planning 뒤 GTK Network부터 공통 wizard shell을 사용한다.
- 공통 GTK 흐름은 `Network -> Windows Version -> Download -> Installation Options -> Summary`이다.
- 이번 변경은 GTK presentation 전용이다. M7/M8 safety, disk policy, NetworkManager, Microsoft source, download/verification/workspace 동작은 변경하지 않았다.
- Final Install executor와 Windows image apply는 아직 연결하지 않았다.

## Changed files

- `src/gui/gtk_frontend.c`: wizard shell, sidebar progress, responsive page containers, page presentation, Summary placeholder UX.
- `src/gui/classicsetup.css`: XP-era inspired colors, typography, lists, status blocks, navigation, progress styling.
- `.ai/handoff.md`: current GUI snapshot.

## Implementation result

- 모든 GTK page는 하나의 shell 안에서 `GtkStack` content만 교체한다.
- shell은 blue ClassicSetup/Windows Setup header, left progress sidebar, scrollable light content panel, fixed bottom navigation으로 구성된다.
- 기본 창은 1000x680, minimum 760x520이며 body/content는 확장된다. 각 page는 vertical `GtkScrolledWindow`로 감싸 작은 창에서 clipping 대신 scroll한다.
- 공통 `build_page_base()`가 wrapped title/subtitle/separator rhythm을 제공한다.
- 공통 `make_scrollable()`이 page content의 responsive boundary를 제공한다.
- 공통 `build_status_block()`이 storage transition 안내와 warning/status presentation을 제공한다.
- wrapped label은 word/character fallback wrapping과 horizontal expansion을 사용한다.
- sidebar는 Recommended에서 Disk부터, Advanced에서 Network부터 표시한다.
- sidebar 단계는 current `›`, completed `✓`, pending `•` 및 CSS state class로 갱신된다.
- 완료 판단은 기존 model만 읽는다: selected disk, Internet readiness, selected source, verified download, visited/default options.
- Advanced sidebar는 `Storage configuration is ready. No disk changes have been applied yet.` 안내를 지속 표시한다.
- Network page는 Wired panel, Wi-Fi list frame, Refresh/Connect, password, spinner/status를 구조화했다.
- Wi-Fi 장치가 없으면 기존 backend snapshot을 사용해 `No Wi-Fi device was detected.`를 표시한다.
- Windows Version page는 family radio와 discovered release/language dropdown을 기존 source model로 표시한다.
- Download page는 선택 release, language, architecture, state, progress, bytes/total, transfer rate, Download/Cancel을 표시한다.
- page 이동은 기존 download worker를 cancel하지 않는다.
- Installation Options는 실제 구현을 가장하지 않고 향후 Locale, Account, Privacy, compatibility, online-account, cleanup category를 placeholder row로 구분한다.
- Summary는 target/planned disk, network, Windows family/source, verification, options를 별도 wrapped row로 표시한다.
- Summary의 Install 버튼은 readiness gate를 유지하지만 executor를 호출하거나 GUI를 종료하지 않는다. 대신 미연결 안내 warning을 표시해 UI가 계속 responsive하다.
- XP-era 느낌은 classic blue sidebar/header, orange separator, square controls, bordered lists, gray navigation bar로 표현하며 Microsoft asset은 사용하지 않는다.

## Build/test result

- GTK ON + download ON Debug configure/build 성공; C17, `-Wall -Wextra -Wpedantic`, warning 0.
- GTK OFF + download OFF Debug configure/build 성공; stub frontend, warning 0.
- GTK ON/OFF CTest 각각 15/15 통과.
- 기존 Network/GUI page state navigation, download/source model, workspace, M7/M8 regression tests가 모두 통과했다.
- 실제 TUI -> GTK 진입 smoke 수행: GTK window 생성 및 CSS load, CSS parser warning 없음.
- smoke 환경에서 EGL/Mesa software-renderer warning이 있었으며 기능/backend 오류는 관찰되지 않았다.
- 외부 source discovery, ISO download, network connection 변경, destructive disk operation은 수행하지 않았다.

## Topics for ChatGPT to explain

- 하나의 wizard shell과 `GtkStack` page content를 분리하는 이유.
- sidebar progress를 backend 복제 없이 session/model state에서 파생하는 방식.
- current/completed/pending을 CSS class와 indicator로 표현하는 구조.
- `GtkScrolledWindow`, expand, minimum size를 조합한 responsive GTK layout.
- 공통 page title/status/navigation component가 UX 일관성을 만드는 방식.
- GTK CSS가 backend/state machine과 독립적인 presentation layer인 이유.
- async download 중 page navigation과 worker lifetime이 독립적인 이유.
- Summary placeholder가 destructive transaction boundary를 보존하는 방식.

## Issues/cautions

- pixel-perfect Windows XP 복제는 아니며 ClassicSetup 고유 wizard style foundation이다.
- 실제 GTK rendering은 desktop theme/font/GPU backend에 따라 일부 차이가 난다.
- 현재 environment smoke에서 EGL/Mesa warning이 발생해 GPU rendering 품질은 별도 VMware desktop 확인이 필요하다.
- Installation Options 기능은 placeholder이며 unattend/debloat/account/privacy backend가 없다.
- Final Install 버튼은 안내만 표시하며 partition/format/image/boot executor를 호출하지 않는다.
- source discovery는 official Microsoft flow 변화에 fail-closed하며 actual ISO integration test는 사용자가 명시적으로 수행해야 한다.
- Windows image apply, WIM/ESD inspection, edition selection, boot configuration은 후속 작업이다.
