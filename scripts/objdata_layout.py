"""SD 카드에서 RCT2 ObjData 폴더를 하위 폴더로 나누는 규칙 (sd_sync.py, setup_emulator.py 공용).

이유: 3DS는 SD 카드(FAT)의 파일을 열 때마다 그 폴더의 목록을 처음부터 훑는다. ObjData에는 파일이
2122개 있어서 파일 하나를 여는 데 약 0.23초가 걸렸다 (실기 측정 2026-10-02: 시나리오 하나가 쓰는
오브젝트 392개를 여는 데 88.6초, 읽는 데는 6.4초). 폴더당 파일 수를 줄이면 여는 시간이 그만큼 준다.

배치: ObjData/00, ObjData/01, ... 에 이름순으로 GROUP_SIZE개씩. 게임은 ObjData를 하위 폴더까지
훑으므로(ObjectRepository.cpp) 게임 쪽 코드는 그대로다. 경로가 바뀌므로 다음 실행 때 objects.idx를
한 번 다시 만든다.

GROUP_SIZE: 파일 하나를 열려면 ObjData에서 하위 폴더를 찾고(폴더 수 = 2122/N), 그 안에서 파일을
찾는다(N). 둘의 합이 가장 작은 N은 sqrt(2122) = 46 근처다.
"""
import os

OBJDATA = "ObjData"
GROUP_SIZE = 48
OTHER = "99"   # 원본 목록에 없는 파일(사용자가 넣은 오브젝트)이 가는 폴더


def table(rct2_dir):
    """{파일 이름(대문자): 하위 폴더 이름}. rct2_dir는 원본 배치(ObjData에 파일이 바로 있음)의 RCT2 폴더"""
    names = [n for n in os.listdir(os.path.join(rct2_dir, OBJDATA))
             if os.path.isfile(os.path.join(rct2_dir, OBJDATA, n))]
    ordered = sorted(names, key=lambda n: n.upper())
    return {n.upper(): "%02d" % (i // GROUP_SIZE) for i, n in enumerate(ordered)}


def placed(rel, tbl):
    """원본 배치의 상대 경로('ObjData/X.DAT')를 SD 카드 배치('ObjData/NN/X.DAT')로. 그 밖의 경로는 그대로"""
    parts = rel.split("/")
    if len(parts) == 2 and parts[0].upper() == OBJDATA.upper():
        return "%s/%s/%s" % (parts[0], tbl.get(parts[1].upper(), OTHER), parts[1])
    return rel
