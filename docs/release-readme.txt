OpenRCT2 for New Nintendo 3DS  {version}
https://github.com/gulf1324/OpenRCT2-n3ds

RollerCoaster Tycoon 2 on a New 3DS. This download is the game, built and ready to
install: there is nothing to compile.              (한국어 안내는 아래에 있습니다)


YOU NEED

  * A New 3DS, New 3DS XL or New 2DS XL with custom firmware (Luma3DS) and the
    homebrew apps FBI and ftpd. The original 3DS, 3DS XL and 2DS do not work.
    No custom firmware yet? Start at https://3ds.hacks.guide
  * Your own RollerCoaster Tycoon 2 (Steam or GOG), installed on this PC. Its data
    files are not in this download: the installer copies them from your PC.
  * About 1 GB free on the SD card (about 500 MB without the ride music).


INSTALL

  1. Unpack this whole ZIP into a folder (right-click the ZIP, "Extract All...").
     install.cmd does not work from inside the ZIP.
  2. On the 3DS, start ftpd and leave it open. The PC and the 3DS have to be on
     the same Wi-Fi.
  3. On the PC, double-click install.cmd and answer its questions.
       Windows       Nothing to install first: this download brings its own Python,
                     in the python folder. If Windows warns about a file from the
                     internet, choose "More info", then "Run anyway".
       macOS, Linux  Run  python3 scripts/install.py  in a terminal.
  4. On the 3DS, close ftpd and open FBI:  SD > cias > openrct2.cia > Install CIA

  OpenRCT2 is now on the HOME menu.


THE FIRST START IS A BLACK SCREEN FOR ABOUT TWO MINUTES

  That is normal: do not turn the console off and do not delete the game. It reads
  all 2122 object files once. Every start after that takes a few seconds.


GOOD TO KNOW

  * Over Wi-Fi the copy takes about half an hour. Keep the 3DS open and on its
    charger. With the SD card in the PC it takes a minute or two: choose 2 in the
    installer.
  * If the copy is interrupted, run install.cmd again: it goes on where it stopped.
  * No sound? The game needs /3ds/dspfirm.cdc on the SD card. On the 3DS hold
    L + D-pad Down + Select, then Miscellaneous options > Dump DSP firmware.
    Once is enough.
  * Language: in the game, file menu (the disk button) > Options > the units tab.
  * Saving is by hand, from the file menu. There is no autosave.

  Controls, troubleshooting and everything else are on the web page above.


IN THIS DOWNLOAD

  install.cmd, scripts/   the installer
  release/                the game: openrct2.cia for the HOME menu,
                          openrct2.3dsx for the Homebrew Launcher
  sdcard/                 OpenRCT2's own data files
  python/                 for PCs without Python: the official "Windows embeddable
                          package (64-bit)" of Python {python} from python.org,
                          unchanged. Already have Python 3? You can delete this
                          folder: install.cmd then uses yours.
  LICENSE, licenses/      the licences

  OpenRCT2 and this port are free software under the GPLv3 (LICENSE). The source
  code is on the web page above, under the tag {version}. This port is not affiliated
  with or endorsed by the OpenRCT2 developers, Atari, Chris Sawyer or Nintendo.


================================================================================

New 닌텐도 3DS용 OpenRCT2  {version}
https://github.com/gulf1324/OpenRCT2-n3ds

New 3DS에서 하는 롤러코스터 타이쿤 2입니다. 이 파일은 빌드가 끝난 게임이라 바로
설치하면 됩니다: 컴파일할 것이 없습니다.


필요한 것

  * 커스텀 펌웨어(Luma3DS)와 홈브루 앱 FBI, ftpd가 있는 New 3DS, New 3DS XL,
    New 2DS XL. 구형 3DS, 3DS XL, 2DS에서는 안 됩니다.
    커스텀 펌웨어가 아직 없다면 https://3ds.hacks.guide 부터 보세요.
  * 본인의 RollerCoaster Tycoon 2 (Steam 또는 GOG. 이 PC에 설치된 것). 게임의
    데이터 파일은 이 파일에 없고, 설치 스크립트가 본인 PC에서 복사합니다.
  * SD 카드 여유 공간 약 1GB (놀이기구 음악을 빼면 약 500MB).


설치

  1. 이 ZIP 전체를 폴더에 풉니다 (ZIP을 오른쪽 클릭, "압축 풀기").
     ZIP 안에서 바로 install.cmd를 실행하면 안 됩니다.
  2. 3DS에서 ftpd를 실행하고 켜 둡니다. PC와 3DS가 같은 Wi-Fi에 있어야 합니다.
  3. PC에서 install.cmd를 더블클릭하고 질문에 답합니다.
       Windows       먼저 설치할 것이 없습니다: 설치 스크립트가 쓰는 Python이
                     python 폴더에 들어 있습니다. Windows가 인터넷에서 받은
                     파일이라고 경고하면 "추가 정보", "실행"을 누르세요.
       macOS, Linux  터미널에서  python3 scripts/install.py
  4. 3DS에서 ftpd를 닫고 FBI를 엽니다:  SD > cias > openrct2.cia > Install CIA

  이제 홈 메뉴에 OpenRCT2 아이콘이 있습니다.


첫 실행은 약 2분 동안 검은 화면입니다

  정상입니다: 전원을 끄거나 게임을 지우지 마세요. 오브젝트 파일 2122개를 한 번
  다 읽는 시간입니다. 그다음부터는 몇 초 만에 켜집니다.


알아 둘 것

  * Wi-Fi 복사는 약 30분 걸립니다. 3DS를 열어 두고 충전기를 꽂아 두세요. SD
    카드를 PC에 꽂으면 1~2분입니다: 설치 스크립트에서 2를 고르세요.
  * 복사가 중간에 끊기면 install.cmd를 다시 실행하세요: 이어서 합니다.
  * 소리가 안 나면 SD 카드에 /3ds/dspfirm.cdc가 없는 것입니다. 3DS에서
    L + 십자키 아래 + Select, 그다음 Miscellaneous options > Dump DSP firmware.
    한 번만 하면 됩니다.
  * 언어: 게임 안의 파일 메뉴(디스크 버튼) > 옵션 > 단위 탭.
  * 저장은 파일 메뉴에서 직접 합니다. 자동 저장은 없습니다.

  조작, 문제 해결, 그 밖의 것은 위의 웹 페이지(한국어 README)에 있습니다.


이 파일에 든 것

  install.cmd, scripts/   설치 스크립트
  release/                게임: 홈 메뉴용 openrct2.cia,
                          Homebrew Launcher용 openrct2.3dsx
  sdcard/                 OpenRCT2 자체 데이터 파일
  python/                 Python이 없는 PC를 위한 것: python.org의 공식 Python
                          {python} "Windows embeddable package (64-bit)" 그대로.
                          이미 Python 3이 있다면 이 폴더를 지워도 됩니다:
                          그러면 install.cmd가 PC의 Python을 씁니다.
  LICENSE, licenses/      라이선스

  OpenRCT2와 이 포팅은 GPLv3의 자유 소프트웨어입니다 (LICENSE). 소스 코드는 위의
  웹 페이지에서 태그 {version}로 받을 수 있습니다. OpenRCT2 개발자들, Atari,
  Chris Sawyer, 닌텐도와 관계가 없는 비공식 프로젝트입니다.
