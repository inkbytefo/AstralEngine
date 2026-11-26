#!/usr/bin/env python3
"""Checks the current documentation against the repository.

For every tracked Markdown file under docs/ (except docs/archive/, which is history) and the root
README.md and AGENTS.md:
  * relative links [text](target) must point to an existing file or folder;
  * source references `path:line` or `path:line-line` (in backticks or as link targets) must name an
    existing file that has at least that many lines.
A reference is a repo-relative path (modules/..., tools/..., cmake/..., examples/..., features/...,
templates/..., tests/..., docs/...) or a path relative to the Markdown file.

Usage: check_docs.py --root <repo>   (exit 1 when anything is broken)
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
CODE_REF = re.compile(r"`([A-Za-z0-9_./-]+\.[A-Za-z0-9]+):(\d+)(?:-(\d+))?`")
LINE_SUFFIX = re.compile(r"^(.*?):(\d+)(?:-(\d+))?$")
REPO_PREFIXES = ("modules/", "tools/", "cmake/", "examples/", "features/", "templates/", "tests/", "docs/", "bench/")


def tracked_markdown(root: Path) -> list[str]:
    out = subprocess.run(["git", "-C", str(root), "ls-files", "*.md"], capture_output=True, text=True, check=True)
    files = []
    for rel in out.stdout.split():
        if rel.startswith("docs/archive/"):
            continue
        if rel.startswith("docs/") or rel in ("README.md", "AGENTS.md"):
            files.append(rel)
    return files


def resolve(root: Path, md: Path, target: str) -> Path:
    if target.startswith(REPO_PREFIXES) and not (md.parent / target).exists():
        return root / target
    return (md.parent / target).resolve()


def line_count(path: Path) -> int:
    with path.open("rb") as f:
        return sum(1 for _ in f)


def check_line_ref(root: Path, md: Path, path_text: str, first: str, last: str | None) -> str | None:
    path = resolve(root, md, path_text)
    if not path.is_file():
        return f"missing file {path_text}"
    end = int(last or first)
    lines = line_count(path)
    if int(first) < 1 or end > lines or end < int(first):
        return f"{path_text}:{first}{'-' + last if last else ''} out of range ({lines} lines)"
    return None


def check_file(root: Path, rel: str) -> list[str]:
    md = root / rel
    text = md.read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"```.*?```", "", text, flags=re.S)  # fenced code is not a reference
    problems = []
    for m in LINK.finditer(text):
        target = m.group(1)
        if re.match(r"^[a-z][a-z0-9+.-]*:", target) or target.startswith("#"):
            continue
        target = target.split("#", 1)[0]
        if not target:
            continue
        ref = LINE_SUFFIX.match(target)
        if ref and ref.group(2):
            err = check_line_ref(root, md, ref.group(1), ref.group(2), ref.group(3))
            if err:
                problems.append(f"{rel}: link {m.group(1)}: {err}")
            continue
        if not resolve(root, md, target).exists():
            problems.append(f"{rel}: broken link {m.group(1)}")
    for m in CODE_REF.finditer(text):
        err = check_line_ref(root, md, m.group(1), m.group(2), m.group(3))
        if err:
            problems.append(f"{rel}: reference `{m.group(0).strip('`')}`: {err}")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    args = parser.parse_args()
    root = Path(args.root).resolve()
    files = tracked_markdown(root)
    problems = [p for rel in files for p in check_file(root, rel)]
    for p in problems:
        print(p)
    print(f"check_docs: {len(files)} files, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
