"""Writes the GitHub "new release" link with the tag, title and body (the notes file) filled in.
`python make_release_url.py vX.Y.Z "title"`."""
import os
import sys
import urllib.parse

version, title = sys.argv[1], sys.argv[2]
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "release") + "/"
body = open(out + version + "-notes.md", encoding="utf-8").read()
url = ("https://github.com/gulf1324/OpenRCT2-n3ds/releases/new?tag=%s&title=%s&body=%s"
       % (version, urllib.parse.quote(title, safe=""), urllib.parse.quote(body, safe="")))
open(out + version + "-new-release-url.txt", "w", encoding="utf-8", newline="\n").write(url + "\n")
print("wrote", out + version + "-new-release-url.txt", len(url), "chars")
