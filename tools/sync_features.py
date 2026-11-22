#!/usr/bin/env python3
"""Keeps the copies of the shared features (features/<name>/) in every game that uses them identical.

  sync_features.py            copy features/<name>/{src,assets} into each game that has src/features/<name>/
  sync_features.py --check    exit 1 and list differences instead of copying
A game "uses" a feature when its src/features/<name>/ folder exists (create it to opt in)."""
import filecmp
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHARED = os.path.join(ROOT, "features")


def games():
    for base in ("examples", "templates"):
        folder = os.path.join(ROOT, base)
        if os.path.isdir(folder):
            for name in sorted(os.listdir(folder)):
                game = os.path.join(folder, name)
                if os.path.isdir(os.path.join(game, "src", "features")):
                    yield game


def files(folder):
    for dirpath, _, names in os.walk(folder):
        for n in names:
            yield os.path.relpath(os.path.join(dirpath, n), folder)


def main():
    check = "--check" in sys.argv
    problems = []
    for feature in sorted(os.listdir(SHARED)):
        source = os.path.join(SHARED, feature)
        if not os.path.isdir(source):
            continue
        for game in games():
            if not os.path.isdir(os.path.join(game, "src", "features", feature)):
                continue
            pairs = [(os.path.join(source, "src"), os.path.join(game, "src", "features", feature)),
                     (os.path.join(source, "assets"), os.path.join(game, "assets", "features", feature))]
            for src, dst in pairs:
                for rel in files(src):
                    a, b = os.path.join(src, rel), os.path.join(dst, rel)
                    if os.path.exists(b) and filecmp.cmp(a, b, shallow=False):
                        continue
                    if check:
                        problems.append(f"{os.path.relpath(b, ROOT)} differs from {os.path.relpath(a, ROOT)} (run tools/sync_features.py)")
                    else:
                        os.makedirs(os.path.dirname(b), exist_ok=True)
                        shutil.copyfile(a, b)
                        print("updated", os.path.relpath(b, ROOT))
    for p in problems:
        print(p, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
