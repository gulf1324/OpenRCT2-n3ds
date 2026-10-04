#!/usr/bin/env python3
"""3DS의 ftpd와 파일을 주고받는다. 3DS에서 ftpd를 켜 둔 상태여야 한다.

사용법:
  python scripts/n3ds_ftp.py ls   [원격 경로]            예: ls /3ds
  python scripts/n3ds_ftp.py get  <원격 파일> [로컬 경로]
  python scripts/n3ds_ftp.py put  <로컬 파일|폴더> <원격 폴더>   폴더는 통째로 올린다
  python scripts/n3ds_ftp.py dumps                        크래시 덤프를 crashdumps/ 로 모두 가져온다
  python scripts/n3ds_ftp.py pull <원격 파일|폴더> <로컬 폴더>   폴더는 통째로 받는다 (백업용)

IP는 local.env의 N3DS_IP, 포트는 N3DS_FTP_PORT(기본 5000).
ftpd는 EPSV를 지원하지 않으므로 PASV로 접속한다. LIST·RETR에 경로를 붙이면 거부하므로 먼저 CWD 한다. 응답이 몇 초씩 느릴 수 있어 시간 제한을 넉넉히 둔다.
"""
import ftplib, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def env():
    cfg = {}
    p = os.path.join(ROOT, "local.env")
    if os.path.exists(p):
        for line in open(p, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                k, v = line.split("=", 1)
                cfg[k.strip()] = v.strip()
    return cfg


def connect():
    cfg = env()
    ip = cfg.get("N3DS_IP")
    if not ip:
        sys.exit("local.env에 N3DS_IP가 없습니다")
    f = ftplib.FTP(timeout=60)
    f.connect(ip, int(cfg.get("N3DS_FTP_PORT", "5000")))
    f.login()
    f.set_pasv(True)
    return f


def put(f, local, remote_dir):
    if os.path.isdir(local):
        target = remote_dir.rstrip("/") + "/" + os.path.basename(os.path.normpath(local))
        try:
            f.mkd(target)
        except ftplib.error_perm:
            pass
        for name in sorted(os.listdir(local)):
            put(f, os.path.join(local, name), target)
    else:
        dest = remote_dir.rstrip("/") + "/" + os.path.basename(local)
        print("put", dest)
        with open(local, "rb") as fp:
            f.storbinary("STOR " + dest, fp, blocksize=64 * 1024)


def pull(f, remote, local_dir):
    """원격 파일이나 폴더를 local_dir 아래로 그대로 받는다"""
    name = remote.rstrip("/").rsplit("/", 1)[-1]
    parent = remote.rstrip("/").rsplit("/", 1)[0] or "/"
    lines = []
    f.cwd(parent)
    f.retrlines("LIST", lines.append)
    entry = next((l for l in lines if l.split(None, 8)[-1] == name), None)
    if entry is None:
        print("not found", remote)
        return
    if entry.startswith("d"):
        dest = os.path.join(local_dir, name)
        os.makedirs(dest, exist_ok=True)
        f.cwd(remote)
        children = []
        f.retrlines("LIST", children.append)
        for c in children:
            pull(f, remote.rstrip("/") + "/" + c.split(None, 8)[-1], dest)
    else:
        os.makedirs(local_dir, exist_ok=True)
        dest = os.path.join(local_dir, name)
        with open(dest, "wb") as fp:
            f.retrbinary("RETR " + name, fp.write)
        print("saved", dest, os.path.getsize(dest))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    cmd, args = sys.argv[1], sys.argv[2:]
    f = connect()
    if cmd == "ls":
        f.cwd(args[0] if args else "/")
        f.retrlines("LIST")
    elif cmd == "get":
        dest = args[1] if len(args) > 1 else os.path.basename(args[0])
        d, n = args[0].rsplit("/", 1) if "/" in args[0] else (".", args[0])
        f.cwd(d or "/")
        with open(dest, "wb") as fp:
            f.retrbinary("RETR " + n, fp.write)
        print("saved", dest)
    elif cmd == "put":
        put(f, args[0], args[1])
    elif cmd == "pull":
        pull(f, args[0], args[1])
    elif cmd == "dumps":
        out = os.path.join(ROOT, "crashdumps")
        os.makedirs(out, exist_ok=True)
        f.cwd("/luma/dumps/arm11")
        for name in f.nlst():
            name = name.rsplit("/", 1)[-1]
            if not name.endswith(".dmp"):
                continue
            dest = os.path.join(out, name)
            if os.path.exists(dest):
                continue
            with open(dest, "wb") as fp:
                f.retrbinary("RETR " + name, fp.write)
            print("saved", dest)
    else:
        print(__doc__)
        return 1
    f.quit()
    return 0


if __name__ == "__main__":
    sys.exit(main())
