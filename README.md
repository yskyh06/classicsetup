# ClassicSetup

ClassicSetup currently provides two interfaces over the same storage engine:

- Recommended prepares a future GTK flow and only auto-installs to a disk that
  policy classifies as safely raw and empty.
- Advanced keeps the ncurses firmware, disk, partition, format, and apply flow.

The Recommended state graph is: Setup Mode -> GUI transition boundary -> Disk
-> Network -> Windows Version -> Download -> Installation Options -> Summary.
The intermediate product screens are placeholders; no Windows source is
downloaded or applied.

Linux 기반 Windows 설치 프로그램을 개발하기 위한 C 프로젝트입니다.

현재 M9 단계로, Recommended와 Advanced 설치 흐름을 구분합니다.
Recommended는 강한 identity로 확인된 빈 디스크와 UEFI/GPT만 자동 계획하며,
Advanced는 M8의 partition/format 선택과 Preview 흐름을 그대로 제공합니다.
BIOS/MBR 실제 적용과 Windows image 처리는 아직 차단되어 있습니다.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run

```sh
./build/classicsetup
```

Recommended 흐름은 다음과 같습니다.

```text
Welcome -> Setup Mode -> Keyboard -> Recommended Disk -> Windows Source
        -> Install Summary -> Partition/Format orchestration -> Result
        -> GUI Transition placeholder
```

Advanced 흐름은 다음과 같습니다.

```text
Welcome -> Setup Mode -> Keyboard -> Installation Mode -> Disk
        -> Partition -> Format -> Partition Apply -> Format Apply
        -> GUI Transition placeholder
```

Ubuntu 테스트 VM의 실제 filesystem apply에는 `dosfstools`, `ntfs-3g`,
`util-linux` 패키지가 필요합니다. ClassicSetup은 누락된 도구를 설치하지 않고
format을 차단합니다.

공통 키는 `ENTER=Continue`, `B=Back`, `ESC=Cancel`, `Q=Quit`입니다.
파티션 화면은 `C=Create`, `D=Delete`, `U=Undo Layout`을 사용하며,
destructive confirmation 화면에서만 `A=Apply`를 사용합니다.
