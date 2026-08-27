# ClassicSetup

Linux 기반 Windows 설치 프로그램을 개발하기 위한 C 프로젝트입니다.

현재 M7.5 단계로, UEFI/GPT와 Legacy BIOS/MBR 설치 계획을 구분합니다.
실제 destructive apply는 제한된 테스트 VM의 UEFI/GPT 경로만 지원합니다.

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

주요 화면 흐름은 다음과 같습니다.

```text
Welcome -> Keyboard -> Installation Mode -> Disk -> Partition -> Format
        -> Apply Preview -> Apply Confirmation -> Apply Result
```

공통 키는 `ENTER=Continue`, `B=Back`, `ESC=Cancel`, `Q=Quit`입니다.
파티션 화면은 `C=Create`, `D=Delete`, `U=Undo Layout`을 사용하며,
destructive confirmation 화면에서만 `A=Apply`를 사용합니다.
