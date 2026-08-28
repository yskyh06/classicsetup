# ClassicSetup Handoff

## Current milestone

- M11 Windows Source Discovery + Async Download + Verification + Temporary Workspace/Cleanup 완료.
- 공통 GTK 흐름은 `Network -> Windows Version/source discovery -> Download -> Options -> Summary`이다.
- Recommended는 GTK Disk부터, Advanced는 보존된 storage plan 뒤 GTK Network부터 같은 흐름을 사용한다.
- 실제 ISO 다운로드 경로를 구현했지만 Codex 환경에서는 외부 source discovery나 대용량 다운로드를 실행하지 않았다.
- Final Install, partition/format execution, ISO mount, WIM/ESD parsing/apply, boot configuration은 연결하지 않았다.
- M7/M8 destructive safety, Advanced storage plan, Recommended disk policy는 변경하지 않았다.

## Changed files

- Public model/API: `include/classicsetup/windows_source.h`, `include/classicsetup/download.h`, `include/classicsetup/workspace.h`.
- Installation/GUI ownership: `include/classicsetup/config.h`, `include/classicsetup/gui.h`, `src/app.c`, `src/gui/gui.c`.
- GTK frontend: `src/gui/gtk_frontend.c`.
- Source backend: `src/source/windows_source.c`, `src/source/microsoft_source_curl.c`, `src/source/microsoft_source_stub.c`.
- Download/workspace: `src/source/download_model.c`, `src/source/download_curl.c`, `src/source/download_stub.c`, `src/source/workspace.c`.
- Build/tests: `CMakeLists.txt`, `tests/gui_test.c`, `tests/source_test.c`.
- Snapshot: `.ai/handoff.md`.

## Implementation result

- `classicsetup_windows_release/catalog`은 family, discovered release, Korean/English language, x86_64 architecture, Microsoft product/SKU metadata, optional official SHA-256, ephemeral download URI를 분리한다.
- Windows Version page는 선택한 Windows 11/10 family에 대해 worker thread에서 official Microsoft landing/connector discovery를 수행한다.
- GUI는 임의 release URL을 하드코딩하지 않고 discovery 결과만 표시하며 Microsoft HTTPS host가 아닌 URI는 fail-closed 거부한다.
- 전체 signed URI는 출력하지 않는다. sanitizer는 query/fragment를 제거하며 worker 종료 시 session에 복사되기 전에 resolved URI buffer를 지운다.
- libcurl backend는 HTTPS-only redirects, peer/hostname verification, HTTP failure handling, progress callback, cancellation을 사용한다. shell 실행은 없다.
- source discovery와 download/verification은 `GTask` worker에서 실행되고 GTK widget 갱신은 main-context callback에서만 수행된다.
- Download page는 상태, bytes/total, rate, progress, Download/Cancel을 표시한다. 다른 page로 이동해도 transfer는 계속된다.
- GUI 종료 중 worker가 있으면 cancellation을 요청하고 completion까지 기다린 뒤 main loop를 종료해 dangling worker를 남기지 않는다.
- 상태 모델은 `NOT_STARTED -> PREPARING -> DOWNLOADING -> VERIFYING -> COMPLETE`; cancel/failure는 별도 terminal state다.
- Summary Ready gate는 Internet reachable, source selected, options valid, download COMPLETE, verified workspace를 모두 요구한다.
- verified source/catalog/download/workspace snapshot은 GUI 종료 시 installation config로 인계되어 frontend session과 파일 ownership을 분리한다.
- workspace는 `mkdtemp(/tmp/classicsetup-XXXXXX)`와 mode 0700을 사용하며 known paths만 관리한다.
- 다운로드는 `windows.iso.part`에 mode 0600으로 쓰고 flush/fsync 후 size, ISO `CD001`, 사용 가능한 official SHA-256을 검증한다.
- 검증 성공 때만 같은 filesystem의 atomic `rename()`으로 `windows.iso`가 된다. verify 실패 파일은 final source가 되지 않는다.
- 시작 전 expected size 또는 보수적 8 GiB fallback과 512 MiB overhead 기준으로 `statvfs()` free space를 검사한다.

| Artifact | Success | Failure | Cancel | After install API |
|---|---|---|---|---|
| `.part` | 삭제/atomic rename | 삭제 | 삭제 | 삭제 |
| verified ISO | image apply까지 유지 | 생성 안 함 | 기존 ISO 보존 | 기본 삭제, `keep_iso`면 유지 |
| metadata temp | 삭제 | 삭제 | 삭제 | 삭제 |
| debug artifact | 기본 생성 안 함/삭제 | 삭제 | 삭제 | 삭제 |

- `classicsetup_microsoft_source_discover/resolve()`은 official metadata/ephemeral link를 가져오며 curl 또는 unavailable stub을 사용한다.
- `classicsetup_windows_source_parse_catalog/download()`은 GTK 없는 deterministic parser/policy boundary다.
- `classicsetup_download_windows_iso()`는 transfer, cancellation, verification, promotion을 소유하고 progress snapshot을 callback으로 제공한다.
- `classicsetup_verify_source_file()`은 size/ISO marker/OpenSSL EVP SHA-256을 read-only 검증한다.
- `classicsetup_workspace_*()`은 unique path, capacity, promotion, 성공/실패/취소/post-install cleanup을 단일 소유한다.
- `start_source_discovery()/start_download()`과 completion callbacks는 worker lifetime과 GTK main-thread dispatch를 연결한다.
- `classicsetup_gui_summary_is_ready()`는 widget과 무관한 source readiness policy다.
- CMake option `CLASSICSETUP_ENABLE_DOWNLOAD`은 libcurl+OpenSSL을 optional 탐지하고 없거나 OFF이면 fail-graceful stubs를 빌드한다.

## Build/test result

- GTK ON + download ON clean Debug configure/build 성공; GTK4, libcurl, OpenSSL 감지, warning 0.
- GTK OFF + download OFF clean Debug configure/build 성공; GUI/source/download stub 경로, warning 0.
- GTK ON + download OFF configure/build 성공; GTK frontend가 unavailable download backend를 정상 사용한다.
- GTK ON/OFF CTest 각각 15/15 통과.
- ASan/UBSan GTK OFF + download ON CTest 15/15 통과 (`detect_leaks=0`; 실행 환경 제약으로 LSan 비활성).
- parser/official-host rejection/URI redaction, unique 0700 workspace, capacity, ISO/size/hash verification, `.part` promotion/cleanup, summary gate, unavailable backend를 fixture/mock 수준에서 검증했다.
- 외부 Microsoft endpoint, 실제 ISO, disk executor, mkfs, Wi-Fi 변경은 테스트에서 호출하지 않았다.

## Topics for ChatGPT to explain

- async file download와 GTK main loop, GTask worker/main-context callback의 역할.
- libcurl easy transfer를 worker에 격리한 구조와 향후 multi/resume 확장점.
- HTTP redirect와 TLS peer/hostname verification의 기본 보안 경계.
- `.part`와 verified final ISO를 나누고 atomic rename을 사용하는 이유.
- OpenSSL EVP SHA-256, official hash availability, ISO basic sanity의 검증 강도 차이.
- `statvfs()` free-space check와 향후 extraction overhead 계산.
- `mkdtemp()`/0700/known-path cleanup이 symlink/path race를 줄이는 방식.
- GUI, installation config, downloader, verifier, workspace manager의 ownership/lifetime.
- cancellation, worker join, progress throttling, cache/temp/debug artifact lifecycle.

## Issues/cautions

- Microsoft consumer download connector는 안정된 public API가 아니며 profile/schema/anti-abuse 정책이 바뀔 수 있다. 현재 resolver는 browser-attestation/Sentinel 거부 시 fail-closed한다.
- Windows 11 landing/catalog 구조를 기준으로 구현했다. Windows 10 official source 제공/markup 차이는 실제 integration에서 추가 검증이 필요하다.
- Korean/English x64만 노출하며 ARM64와 edition 선택은 미지원이다. Edition은 후속 WIM/ESD metadata 검사 단계에서 결정한다.
- official SHA-256을 landing metadata에서 얻지 못하면 HTTPS+size(있을 때)+ISO marker만 검사하고 hash verified라고 표시하지 않는다.
- crash resume와 persistent cache recovery는 미구현이다. Cancel은 partial을 삭제하고 ordinary page navigation은 active download를 유지한다.
- 실제 Microsoft integration 수동 확인: Network reachable -> Version discovery -> language 선택 -> Download -> progress/cancel/retry -> verified Summary -> workspace 확인.
- Ubuntu 개발 패키지는 `libgtk-4-dev`, `libcurl4-openssl-dev`, `libssl-dev`, `pkg-config`가 필요하다.
- verified ISO의 최종 삭제 시점은 future image apply 성공 후 `classicsetup_workspace_cleanup_after_install()` 호출이다.
- WIM apply와 최종 disk transaction은 미연결이며 GUI Summary는 destructive executor를 호출하지 않는다.
