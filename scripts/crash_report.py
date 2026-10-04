#!/usr/bin/env python3
"""Luma3DS 크래시 덤프(.dmp)를 읽고, 빌드한 .elf로 주소를 소스 위치로 바꿔 보여준다.

사용법:
  python scripts/crash_report.py <crash_dump_XXXXXXXX.dmp> <app.elf>

덤프는 3DS SD 카드의 /luma/dumps/arm11/ 에 있다. 크래시 화면에서 A를 눌러야 저장된다.
.elf는 크래시가 난 .3dsx와 같은 빌드에서 나온 것이어야 주소가 맞는다.
덤프 형식: Luma3DS arm9/source/types.h 의 ExceptionDumpHeader, k11_extension fatalExceptionHandlersMain.c
"""
import os, re, struct, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(ROOT, "tools", "devkitpro", "devkitARM", "bin")
EXE = ".exe" if os.name == "nt" else ""

EXC_NAMES = ["FIQ", "undefined instruction", "prefetch abort", "data abort"]
REG_NAMES_11 = ["R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12",
                "SP", "LR", "PC", "CPSR", "DFSR", "IFSR", "FAR", "FPEXC", "FPINST", "FPINST2"]
FAULT_STATUS = {0b1: "Alignment", 0b100: "Instr.cache maintenance op.",
                0b1100: "Ext.Abort on translation - Lv1", 0b1110: "Ext.Abort on translation - Lv2",
                0b101: "Translation - Section", 0b111: "Translation - Page",
                0b11: "Access bit - Section", 0b110: "Access bit - Page",
                0b1001: "Domain - Section", 0b1011: "Domain - Page",
                0b1101: "Permission - Section", 0b1111: "Permission - Page",
                0b1000: "Precise External Abort", 0b10110: "Imprecise External Abort", 0b10: "Debug event"}


def tool(name):
    return os.path.join(BIN, "arm-none-eabi-" + name + EXE)


def exec_ranges(elf):
    """실행 가능한 섹션(.text 등)의 주소 범위"""
    out = subprocess.run([tool("readelf"), "-SW", elf], capture_output=True, text=True).stdout
    ranges = []
    for line in out.splitlines():
        m = re.search(r"\]\s+(\S+)\s+\S+\s+([0-9a-f]{8})\s+[0-9a-f]+\s+([0-9a-f]+)\s+\S+\s+(\S+)", line)
        if m and "X" in m.group(4):
            start = int(m.group(2), 16)
            ranges.append((start, start + int(m.group(3), 16)))
    return ranges


def symbolize(elf, addrs):
    if not addrs:
        return {}
    out = subprocess.run([tool("addr2line"), "-f", "-C", "-e", elf] + ["%x" % a for a in addrs],
                         capture_output=True, text=True).stdout.splitlines()
    res = {}
    for i, a in enumerate(addrs):
        fn = out[2 * i] if 2 * i < len(out) else "??"
        loc = out[2 * i + 1] if 2 * i + 1 < len(out) else "??"
        res[a] = "%s  (%s)" % (fn, loc)
    return res


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    data = open(sys.argv[1], "rb").read()
    elf = sys.argv[2] if len(sys.argv) > 2 else None

    (m0, m1, vmin, vmaj, proc, core, etype, total,
     regsz, codesz, stacksz, addsz) = struct.unpack_from("<2I4H6I", data, 0)
    if (m0, m1) != (0xDEADC0DE, 0xDEADCAFE):
        print("Luma3DS 덤프 파일이 아닙니다 (magic 불일치)")
        return 1
    off = 40
    regs = list(struct.unpack_from("<%dI" % (regsz // 4), data, off)); off += regsz
    code = data[off:off + codesz]; off += codesz
    stack = data[off:off + stacksz]; off += stacksz
    extra = data[off:off + addsz]
    names = REG_NAMES_11 if proc == 11 else ["R%d" % i for i in range(13)] + ["SP", "LR", "PC", "CPSR"]
    R = {names[i]: v for i, v in enumerate(regs) if i < len(names)}
    thumb = bool(R.get("CPSR", 0) & 0x20)

    print("=== Luma3DS crash dump v%d.%d ===" % (vmaj, vmin))
    print("Processor : Arm%d (core %d)" % (proc, core))
    kind = EXC_NAMES[etype] if etype < len(EXC_NAMES) else "type %d" % etype
    if etype == 2 and len(code) >= 4 and not thumb:
        last = struct.unpack_from("<I", code, len(code) - 4)[0]
        kind += {0xE12FFF7E: " (kernel panic)", 0xEF00003C: " (svcBreak)"}.get(last, "")
    elif etype == 2 and len(code) >= 2 and thumb and struct.unpack_from("<H", code, len(code) - 2)[0] == 0xDF3C:
        kind += " (kernel panic)"
    print("Exception : %s" % kind)
    if proc == 11 and etype >= 2:
        xfsr = R["IFSR"] if etype == 2 else R["DFSR"]
        fs = (xfsr & 0xF) | ((xfsr >> 6) & 0x10)
        print("Fault     : %s" % FAULT_STATUS.get(fs, "unknown (0x%X)" % fs))
    if etype == 3 and "FAR" in R:
        print("FAR       : %08X  (%s)" % (R["FAR"], "Write" if R["DFSR"] & (1 << 11) else "Read"))
    process = ""
    if len(extra) >= 16:
        process = extra[:8].split(b"\0")[0].decode("ascii", "replace")
        print("Process   : %s  (title %016X)" % (process, struct.unpack_from("<Q", extra, 8)[0]))
    print("Mode      : %s" % ("Thumb" if thumb else "ARM"))
    print()
    for i in range(0, min(len(regs), len(names)), 4):
        print("  ".join("%-8s%08X" % (names[j], regs[j]) for j in range(i, min(i + 4, len(regs), len(names)))))

    # 게임의 프로세스 이름: Homebrew Launcher로 실행하면 3dsx_app, 홈 메뉴에 설치한 것은 OpenRCT2
    if process and process not in ("3dsx_app", "OpenRCT2"):
        print("\n이 덤프는 게임이 아니라 '%s' 프로세스의 것이다 (menu = 홈 메뉴)." % process)
        print("게임의 .elf로는 주소를 풀 수 없으므로 소스 위치는 보여 주지 않는다.")
        print("게임을 띄우거나 끝내는 순간이었다면 게임 쪽 꾸러미(CIA의 로고·배너·아이콘)나 종료 과정을 볼 것.")
        words = [struct.unpack_from("<I", stack, i)[0] for i in range(0, len(stack) - 3, 4)]
        print("스택: " + " ".join("%08X" % w for w in words))
        return 0

    if not elf:
        print("\n.elf를 함께 주면 PC/LR과 스택의 호출 흔적을 소스 위치로 바꿔 보여줍니다.")
        return 0

    ranges = exec_ranges(elf)
    in_text = lambda a: any(s <= a < e for s, e in ranges)
    words = [struct.unpack_from("<I", stack, i)[0] for i in range(0, len(stack) - 3, 4)]
    cand = []
    for i, w in enumerate(words):
        a = w & ~1
        if in_text(a) and a not in cand:
            cand.append(a)
    keys = [R["PC"], R["LR"] & ~1] + cand
    sym = symbolize(elf, [k for k in keys if in_text(k)])

    print("\n=== 소스 위치 ===")
    print("PC  %08X  %s" % (R["PC"], sym.get(R["PC"], "<.elf 코드 범위 밖: 잘못된 함수 포인터나 스택 손상 의심>")))
    print("LR  %08X  %s" % (R["LR"], sym.get(R["LR"] & ~1, "<.elf 코드 범위 밖>")))
    print("\n=== 스택에서 찾은 코드 주소 (호출 흔적 후보, 위쪽이 최근) ===")
    for a in cand[:40]:
        print("  %08X  %s" % (a, sym.get(a, "")))

    print("\n=== 해석 힌트 ===")
    far = R.get("FAR", 0)
    if etype == 3 and far < 0x1000:
        print("- FAR이 0x%X: NULL 포인터의 0x%X 위치 필드를 읽거나 쓴 것. 어떤 포인터가 NULL인지 PC의 명령어 기준으로 찾을 것" % (far, far))
    if proc == 11 and etype == 3 and ((R["DFSR"] & 0xF) == 0b1):
        print("- 정렬 오류: 4바이트 값을 4의 배수가 아닌 주소에서 읽음. #pragma pack 구조체나 포인터 캐스팅 확인")
    if not in_text(R["PC"]):
        print("- PC가 코드 밖: 해제된 객체의 함수 포인터 호출, 스택 덮어쓰기로 망가진 복귀 주소 등")
    if "svcBreak" in kind or "panic" in kind:
        print("- svcBreak/panic: abort(), assert 실패, libctru 치명 오류. LR과 스택 후보에서 호출한 쪽을 볼 것")
    return 0


if __name__ == "__main__":
    sys.exit(main())
