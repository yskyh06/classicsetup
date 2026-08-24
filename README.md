# ClassicSetup

Linux 기반 Windows 설치 프로그램을 개발하기 위한 C 프로젝트입니다.

현재 M2 단계로, ncurses 생명주기와 화면 전환을 중앙에서 관리하는 상태
머신을 제공합니다.

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

화면 흐름은 다음과 같습니다.

```text
Welcome --ENTER--> Ready --ENTER--> Exit
   |                   |
   F3                  +--ESC--> Welcome
   |
   +---------------------------> Exit
```
