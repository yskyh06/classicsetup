# ClassicSetup Handoff

## Current milestone

- M10.2 Recommended Network Backend + GTK Network Page 완료.
- Common TUI는 `Welcome -> License Agreement -> Setup Mode`를 유지한다.
- Recommended는 ncurses 종료 후 GTK의 `Disk -> Network -> Windows Version -> Download -> Options -> Summary`를 사용한다.
- Advanced ncurses 흐름과 M7/M8 destructive safety는 변경하지 않았다.
- Recommended destructive 범위는 계속 `RAW_EMPTY + UEFI/GPT`이며 GUI는 executor를 호출하지 않는다.

## Changed files

- Network API/model: `include/classicsetup/network.h`, `src/network/network.c`.
- NetworkManager backend: `src/network/network_manager_gdbus.c`, `src/network/network_manager_stub.c`.
- GUI session/API: `include/classicsetup/gui.h`, `src/gui/gui.c`.
- GTK page/style: `src/gui/gtk_frontend.c`, `src/gui/classicsetup.css`.
- Build/tests: `CMakeLists.txt`, `tests/network_test.c`, `tests/gui_test.c`.
- Snapshot: `.ai/handoff.md`.

## Implementation result

### Current flow and boundary

- Disk page의 Next는 기존 Recommended disk assessment를 통과한 선택이 필요하다.
- Network page 진입 시 비동기 refresh가 시작되고 Internet reachable 전에는 Next가 비활성화된다.
- Network 성공 후 기존 Windows Version placeholder로 이동한다.
- Back으로 Network page에 재진입하면 snapshot을 다시 확인하며, GUI session reset은 stale network snapshot을 제거한다.
- Network page와 backend는 partition/apply/format API를 호출하지 않는다.

### Network architecture

- `classicsetup_network_snapshot`은 GTK 타입이 없는 Ethernet/Wi-Fi/connectivity snapshot이다.
- `classicsetup_network_backend_ops`는 refresh/connect/destroy 비동기 backend 경계다.
- `classicsetup_network_controller`는 `SCANNING`, `CONNECTING`, `CONNECTED`, `ERROR`, `UNAVAILABLE` 상태와 observer 통지를 관리한다.
- GTK-enabled build는 GIO/GDBus NetworkManager backend를 사용한다.
- GTK-disabled build는 network backend stub과 기존 GTK stub을 사용한다.
- `src/core`와 Advanced TUI에는 GTK/GIO 의존성을 추가하지 않았다.

### NetworkManager backend

- NetworkManager의 system bus `org.freedesktop.NetworkManager`를 사용한다.
- `GetDevices`, device/AP properties, `RequestScan`, `GetAllAccessPoints`, `CheckConnectivity`를 worker에서 호출한다.
- Ethernet availability/link와 Wi-Fi SSID/signal/security/active AP를 snapshot으로 변환한다.
- 같은 SSID는 연결 AP 또는 가장 강한 AP 하나로 합친다; hidden SSID는 생략한다.
- enterprise Wi-Fi는 표시하되 M10.2 연결 대상으로 허용하지 않는다.
- Wi-Fi 연결은 `AddAndActivateConnection2`와 `persist=memory` profile을 사용한다.
- shell, `system()`, `popen()`, `nmcli`를 사용하지 않는다.
- NetworkManager/system bus가 없으면 사용자용 `UNAVAILABLE`/`ERROR` 상태로 종료한다.

### GTK async flow and UX

- `GTask` worker가 blocking system-bus I/O를 수행하고 completion은 GTK main context에서 전달된다.
- refresh/connect 중 spinner와 상태 문구를 표시하고 버튼 중복 입력을 막는다.
- Network page는 wired 상태, Wi-Fi 목록, Refresh, password entry, Connect를 제공한다.
- secured Wi-Fi password entry는 숨김 표시이며 연결 요청 직후 UI에서 지운다.
- backend operation buffer도 완료/취소 시 명시적으로 지우고 password를 snapshot/status/log에 저장하지 않는다.
- connection profile은 installer process 동안만 유지되는 NetworkManager memory profile 정책이다.
- NetworkManager connectivity `FULL`만 Recommended online flow의 Next를 허용한다.
- local link/network만 존재하면 Internet unavailable 상태로 분리하고 Next를 막는다.

### Functions added/changed

- `classicsetup_network_snapshot_reset()` (`src/network/network.c`): safe unavailable 기본 snapshot 생성.
- `classicsetup_network_can_continue()` (`src/network/network.c`): `CONNECTED + INTERNET` 정책 판정.
- `classicsetup_network_controller_refresh()` (`src/network/network.c`): scan 상태 전환 후 backend async refresh 요청.
- `classicsetup_network_controller_connect_wifi()` (`src/network/network.c`): 입력 검증, connecting 상태, password 비보존 전달.
- `classicsetup_network_manager_backend_create()` (`src/network/network_manager_gdbus.c`): GTask/GDBus backend 생성; disabled build는 stub 반환.
- `collect_snapshot()` (`src/network/network_manager_gdbus.c`): system bus device/AP/connectivity 정보를 frontend-neutral snapshot으로 변환.
- `connect_wifi()` (`src/network/network_manager_gdbus.c`): memory-only NetworkManager connection 생성 및 활성화.
- `build_network_page()` (`src/gui/gtk_frontend.c`): wired/Wi-Fi/password/actions/status GTK widgets 구성.
- `network_snapshot_changed()` (`src/gui/gtk_frontend.c`): async result를 GUI session에 복사하고 page/navigation 갱신.
- `update_navigation()` (`src/gui/gtk_frontend.c`): Network page Next를 Internet reachable에 연결.
- `classicsetup_gui_session_reset()` (`src/gui/gui.c`): network model도 초기 unavailable 상태로 reset.

## Build/test result

- GTK4 4.22.4 / GIO 2.88.0 환경에서 GTK-enabled clean Debug build 성공.
- `CLASSICSETUP_ENABLE_GTK=OFF` clean Debug/stub build 성공.
- C17, `-Wall -Wextra -Wpedantic`, warning 0.
- GTK-enabled CTest 13/13 통과; GTK-disabled CTest 13/13 통과.
- ASan/UBSan GTK-disabled Debug CTest 13/13 통과.
- 새 mock tests: unavailable, wired disconnected/connected, secured/open Wi-Fi metadata, scanning/connecting/connected/error, Internet Next policy, enterprise rejection, session reset, password 비노출.
- 실제 Wi-Fi 연결, package 설치, destructive disk operation은 실행하지 않았다.
- 추가 development dependency는 없었다; runtime에는 NetworkManager system service가 필요하다.

## Topics for ChatGPT to explain

- D-Bus와 NetworkManager가 system bus에서 service/object/interface를 노출하는 방식.
- session bus와 system bus 차이 및 `dbus-launch` warning이 NetworkManager system service와 다른 이유.
- GTK main loop, `GTask`, worker completion callback과 UI thread ownership.
- blocking I/O가 GUI를 멈추는 이유와 main-context dispatch 경계.
- backend/model/view 분리 및 향후 Advanced/debug frontend 재사용.
- Wi-Fi `DISCONNECTED -> CONNECTING -> CONNECTED/ERROR` state machine.
- link/network/Internet reachability를 분리하고 fail-closed Next를 적용한 이유.
- password lifetime, memory-only NetworkManager profile, 민감정보 buffer 정리.

## Issues/cautions

- captive portal, hidden SSID 입력, WPA Enterprise, static IP, VPN, proxy는 미지원이다.
- NetworkManager가 없거나 system bus 접근이 불가능하면 Recommended online flow는 진행하지 못한다.
- AP scan은 NetworkManager cache를 사용하므로 radio/driver 상태에 따라 결과 갱신이 지연될 수 있다.
- memory-only connection은 process 이후 재사용을 보장하지 않으며 Back 동작은 이미 연결된 network를 강제로 disconnect하지 않는다.
- final boot ISO에 NetworkManager와 system-bus 정책을 포함할지는 확정되지 않았다.
- Windows source query/download, ISO/WIM 처리, partition/format GUI executor 연결은 아직 없다.
