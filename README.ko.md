# New 닌텐도 3DS용 OpenRCT2

New 3DS에서 하는 롤러코스터 타이쿤 2: [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2) v0.0.5의 비공식 포팅입니다.
공원은 위 화면에, 창과 버튼은 터치 화면에 나오고, 터치 화면의 모든 조작은 버튼으로도 됩니다.

[English](README.md)

<img src="docs/title.png" width="300" alt="3DS 두 화면의 타이틀 화면">

## 네 단계로 설치하기

필요한 것:

- 커스텀 펌웨어(Luma3DS)가 설치된 **New** 3DS, New 3DS XL, New 2DS XL 중 하나와, 거기 깔린 홈브루 앱 **FBI**와
  **ftpd**. 구형 3DS·3DS XL·2DS는 속도와 메모리가 모자라 안 됩니다.
- PC에 설치된 **RollerCoaster Tycoon 2**(Steam 또는 GOG). 게임의 데이터 파일은 이 저장소에 들어 있지 않습니다:
  설치 스크립트가 본인의 게임에서 가져옵니다.
- PC의 **Python 3**([python.org](https://www.python.org/downloads/). 다른 것은 설치할 필요 없습니다), 3DS SD 카드의
  여유 공간 약 1GB.

순서:

1. **이 저장소를 받습니다**: [ZIP](https://github.com/gulf1324/OpenRCT2-n3ds/archive/refs/heads/main.zip)을 받아
   풉니다(또는 `git clone`).
2. **3DS에서 ftpd를 실행**하고 켜 둡니다. PC와 3DS가 같은 Wi-Fi에 있어야 합니다.
3. **PC에서 `install.cmd`를 실행**하고(더블클릭. macOS·Linux는 `python3 scripts/install.py`) 질문에 답합니다:

   ```
   Language / 언어:  1 English  2 한국어 [1]: 2
   이 PC에 있는 RollerCoaster Tycoon 2 폴더의 경로 [C:\...\steamapps\common\Rollercoaster Tycoon 2]:
   RollerCoaster Tycoon 1 폴더의 경로 (시나리오용. 없으면 그냥 Enter):
   놀이기구 음악도 복사할까요? 440MB라 Wi-Fi로는 약 15분이 더 걸립니다 [Y/n]:
   어떻게 복사할까요?
     1  Wi-Fi로. 지금 3DS에서 ftpd를 실행하고 켜 둔 채로 두세요.
     2  SD 카드를 이 PC에 꽂았습니다 (훨씬 빠릅니다) [1]:
   3DS의 ftpd 화면에 나오는 주소 (예: 192.168.0.12): 192.168.0.12
   복사할 파일 2571개, 932MB (0개는 이미 있습니다).
   Wi-Fi로 약 31분 걸립니다. 3DS를 열어 두고 충전기를 꽂아 두세요.
   시작할까요? [Y/n]:
   ```

   대괄호 안의 폴더는 Steam과 GOG가 보통 설치하는 자리에서 찾아 준 것입니다: 맞으면 Enter만 누르면 됩니다.
   복사가 중간에 끊기면 다시 실행하세요: 끊긴 데서부터 이어서 합니다.
4. **3DS에서 ftpd를 닫고 FBI를 엽니다**: `SD` > `cias` > `openrct2.cia` > `Install CIA`.

이제 홈 메뉴에 OpenRCT2 아이콘이 있습니다. **첫 실행은 검은 화면으로 약 2분 걸립니다**(게임이 오브젝트 목록을
만듭니다). 그다음부터는 몇 초 만에 켜집니다. 언어는 게임 안에서 고릅니다: 파일 메뉴 > 옵션 > 단위 탭. 첫 실행은
본체의 언어를 따릅니다(한국어 본체면 한국어).

소리가 안 나나요? 게임은 본인 기기의 소리 펌웨어 `/3ds/dspfirm.cdc`가 있어야 소리를 내고, 없으면 아무 말 없이
무음으로 돕니다. 없으면 설치 스크립트가 알려 줍니다. 만드는 법: L + 십자키 아래 + Select(Luma3DS의 Rosalina 메뉴)
> Miscellaneous options > Dump DSP firmware. 한 번만 하면 됩니다.

## 조작

| | |
|---|---|
| 서클패드 | 위 화면 커서. 가장자리로 밀면 화면이 움직입니다 |
| A / B | 커서 자리에서 왼쪽 / 오른쪽 클릭 (B를 누른 채 움직이면 화면 끌기) |
| 십자키 | 터치 화면 창의 컨트롤 사이를 이동. 창이 없으면 화면 스크롤 |
| 십자키 포커스에서 A / B | 포커스된 컨트롤 누르기 / 뒤로 |
| 터치 | 탭 = 클릭, 끌기 = 목록 스크롤이나 큰 창 이동, 길게 누르기 = 누른 채 유지 |
| L / R | 축소 / 확대 |
| X | 화면 회전 |
| Y | 맨 앞 창 닫기 |
| ZL / ZR | 놓는 물건 회전 / Shift |
| START | 일시정지 |
| SELECT | 건설 취소 |
| START + SELECT | 종료 |

저장은 파일 메뉴(디스크 버튼)에서 직접 합니다: 자동 저장은 없습니다.

## 되는 것

인트로, 타이틀 화면, RCT2 시나리오, RollerCoaster Tycoon 1 시나리오(설치할 때 RCT1 폴더를 주면), 저장과 불러오기,
효과음, 놀이기구 음악.

| | |
|---|---|
| 속도 | 손님 약 1600명인 공원에서 기본 배율로 25~32fps, 끝까지 축소하고 가만히 있으면 약 10fps |
| 시작 | 홈 메뉴 아이콘에서 인트로까지 약 8초 |
| 시나리오 불러오기 | 약 6초 |
| 확인한 환경 | New 3DS XL, Steam판 RCT2와 RCT1 Deluxe. 다른 New 기종과 GOG판은 될 것으로 보지만 확인하지 못했습니다 |

언어: 게임의 기본 글꼴로 되는 언어들(영어, 독일어, 프랑스어, 스페인어, 이탈리아어, 네덜란드어, 스웨덴어, 포르투갈어
등)과 한국어(게임에 넣은 픽셀 글꼴로 그립니다). 일부러 뺀 것: 멀티플레이, 트루타입 글꼴(그래서 일본어·중국어·러시아어는
없습니다), 타이틀 시퀀스 편집기 등 PC 전용 옵션.

## 문제가 생기면

- **설치 스크립트가 연결하지 못함**: 3DS에서 ftpd가 켜져 있어야 하고, 두 기기가 같은 Wi-Fi에 있어야 합니다. 주소는
  ftpd가 위 화면에 보여 주는 그대로 입력하세요.
- **소리가 안 남**: `/3ds/dspfirm.cdc`가 없습니다(위 참고).
- **모든 공원이 "Unable to load file"**, 또는 **공원 하나 불러오는 데 1분 넘게 걸림**: SD 카드의 게임 데이터가 설치
  스크립트가 올린 것이 아닙니다. 설치 스크립트를 다시 실행하세요. 전에 `ObjData`를 손으로 복사했다면 먼저 SD 카드의
  `/3ds/openrct2/rct2/ObjData`를 지우세요.
- 게임은 `/3ds/openrct2/user/log.txt`에 로그를 씁니다(지난 실행은 `log_prev.txt`). 버그를 알릴 때 이 파일과
  타이틀 화면 왼쪽 아래의 빌드 이름을 같이 주세요.
- 크래시가 나면 Luma3DS가 빨간 화면을 띄웁니다. 거기서 A를 눌러야 `/luma/dumps/arm11/`에 덤프가 저장됩니다.

## 설치 스크립트가 하는 일

모든 것은 SD 카드의 `/3ds/openrct2`로, 게임은 `/cias/openrct2.cia`로 갑니다. 이미 같은 크기로 있는 파일은
건너뜁니다.

| SD 카드에서 | |
|---|---|
| `/3ds/openrct2/data/` | OpenRCT2 자체 데이터 (이 저장소의 `sdcard/`) |
| `/3ds/openrct2/rct2/` | 본인의 RCT2 데이터. `ObjData`는 48개씩 하위 폴더로 나눕니다: 3DS는 파일을 열 때마다 폴더를 처음부터 훑는데, 한 폴더에 2122개가 있으면 파일 하나에 0.25초가 걸립니다 |
| `/3ds/openrct2/user/objdata.pak` | 오브젝트 파일 전부를 바이트 그대로 하나로 묶은 파일(191MB). PC에서 만듭니다: 공원을 몇 초 만에 불러옵니다 |
| `/3ds/openrct2/rct1/Scenarios/` | RCT1 폴더를 줬다면 그 시나리오 (타이틀 음악은 `rct2/Data/css50.dat`로) |
| `/3ds/openrct2/user/` | 게임이 설정, 세이브, 캐시를 두는 곳 |
| `/cias/openrct2.cia` | 게임. 이 저장소의 `release/`에 있는 것 |

질문 없이: Wi-Fi로는 `python scripts/install.py --rct2 <폴더> [--rct1 <폴더>] [--no-music] --ip <주소>`, PC에 꽂은
SD 카드로는 `--sd <드라이브>`. `--dry-run`은 무엇을 복사할지만 알려 줍니다.

CIA 없이: `release/openrct2.3dsx`를 SD 카드의 `/3ds/`에 넣고 Homebrew Launcher에서 실행합니다. 같은 데이터를
읽습니다.

나중에 `ObjData`의 파일을 바꾸면(커스텀 오브젝트) 설치 스크립트를 다시 실행하세요: 게임의 오브젝트 목록도 지워
줍니다(게임은 그 목록을 믿고 폴더를 다시 보지 않습니다).

## 개발자용

| | |
|---|---|
| `external/OpenRCT2/` | 게임: OpenRCT2 v0.0.5에 포팅을 얹은 소스. 이 저장소의 첫 커밋이 손대지 않은 v0.0.5이므로 `git diff <첫 커밋> HEAD -- external/OpenRCT2`가 포팅이 바꾼 전부입니다 |
| `scripts/` | 설치 스크립트, 툴체인 설치, 빌드, 기기로 파일 보내기 |
| `sdcard/` | OpenRCT2 자체 데이터(`g2.dat`, 언어, 타이틀 시퀀스). v0.0.5 릴리스의 것 그대로입니다 |
| `release/` | 빌드한 게임: `openrct2.cia`, `openrct2.3dsx` |
| `cia/`, `cmake/` | 홈 메뉴 아이콘과 배너, 프로그램 설정, CMake 툴체인 파일 |

포팅 코드는 `src/platform/n3ds.c`(경로, 메모리, 성능 로그), `src/platform/n3ds_input.cpp`(입력, 버튼 이동, 두
화면 출력), `src/drawing/n3ds_drawing.c`(크기를 바꿔 그리기)와 그 밖의 `#ifdef __3DS__` 블록에 있습니다.
`n3ds port:`로 시작하는 주석이 원본을 왜 바꿨는지 적은 것입니다. v0.0.5 위에 이후 OpenRCT2 버전의 버그 수정 약
40개를 얹었습니다. v0.0.5인 이유: 원본 `rct2.exe` 없이 단독으로 도는 첫 OpenRCT2 버전이고, 콘솔에 올릴 만큼
작습니다(C와 약간의 C++, 소프트웨어 렌더링, 스크립팅 없음).

빌드는 Windows + Git Bash에서 합니다. 모든 도구는 이 폴더 안(`tools/`)에만 설치되고 그 밖은 건드리지 않습니다.

```
bash scripts/setup_toolchain.sh     # devkitARM, libctru (devkitPro 패키지를 tools/에 풉니다)
bash scripts/setup_hosttools.sh     # CMake 3.31, ninja
bash scripts/setup_ciatools.sh      # makerom, bannertool
bash scripts/fetch_sources.sh       # SDL2, libzip, speexdsp를 external/에
source ./env.sh
bash scripts/build_deps.sh          # 그 셋을 3DS용으로 빌드
cmake -S external/OpenRCT2 -B build/openrct2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/3ds-win.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/openrct2        # build/openrct2/openrct2.3dsx
python scripts/make_cia.py          # build/openrct2/openrct2.cia
```

`make_cia.py`는 배너의 소리를 `gamedata/rct2/Data/css17.dat`에서 가져옵니다(RCT2 폴더를 `gamedata/rct2`로
복사하거나 연결). 없으면 배너는 무음입니다. 그 밖의 스크립트: `send.cmd`(3dslink로 .3dsx를 무선 전송),
`ftp.cmd`(ftpd와 파일 주고받기), `crash_report.cmd`(같은 빌드의 `.elf`로 Luma3DS 크래시 덤프 읽기). 3DS의 주소는
설치 스크립트가 써 두는 `local.env`에서 읽습니다.

## 라이선스

OpenRCT2와 같은 GPLv3입니다: [LICENSE](LICENSE). OpenRCT2는
[OpenRCT2 개발자들](external/OpenRCT2/contributors.md)의 작업입니다. 이 포팅은 그들이나 Atari, Chris Sawyer,
닌텐도와 관계가 없는 비공식 프로젝트입니다. RollerCoaster Tycoon은 Atari의 상표입니다. devkitPro, libctru, SDL2로
만들었습니다. 한글은 이민서 님의 [갈무리](https://github.com/quiple/galmuri) 글꼴로 그립니다(SIL Open Font License:
`external/OpenRCT2/src/platform/n3ds/galmuri-OFL.txt`).
