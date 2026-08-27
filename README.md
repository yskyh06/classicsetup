# ClassicSetup

Linux 기반 Windows 설치 프로그램을 개발하기 위한 C 프로젝트입니다.

현재 M8 단계로, UEFI/GPT와 Legacy BIOS/MBR 설치 계획을 구분합니다.
제한된 테스트 VM의 UEFI/GPT 경로에서 partition apply 뒤 FAT32/NTFS
filesystem apply를 별도 확인 단계로 수행합니다. BIOS/MBR 실제 적용은 차단됩니다.

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
        -> Partition Apply Preview -> Partition Apply Confirmation
        -> Partition Apply Result -> Format Apply Preview
        -> Format Apply Confirmation -> Format Result
```

Ubuntu 테스트 VM의 실제 filesystem apply에는 `dosfstools`, `ntfs-3g`,
`util-linux` 패키지가 필요합니다. ClassicSetup은 누락된 도구를 설치하지 않고
format을 차단합니다.

공통 키는 `ENTER=Continue`, `B=Back`, `ESC=Cancel`, `Q=Quit`입니다.
파티션 화면은 `C=Create`, `D=Delete`, `U=Undo Layout`을 사용하며,
destructive confirmation 화면에서만 `A=Apply`를 사용합니다.
