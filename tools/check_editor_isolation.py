#!/usr/bin/env python3
"""Verifies that runtime game/server engine layers never depend on editor modules (ADR-0035)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

EDITOR_MODULES = frozenset({"editor_core", "editor"})


def check_isolation(manifest_path: Path) -> list[str]:
    data = json.loads(manifest_path.read_text(encoding="utf-8"))
    by_name = {m["name"]: m for m in data.get("modules", [])}
    errors: list[str] = []

    for mod_name, mod in by_name.items():
        if mod_name in EDITOR_MODULES:
            continue
        deps = list(mod.get("public_deps", [])) + list(mod.get("private_deps", []))
        for dep in deps:
            if dep in EDITOR_MODULES or dep in {"Astral::Editor", "Astral::EditorCore"}:
                errors.append(f"non-editor module '{mod_name}' illegally depends on '{dep}'")

    if "editor_core" not in by_name or "editor" not in by_name:
        errors.append("manifest is missing 'editor_core' or 'editor' module entries")
    else:
        ec_deps = set(by_name["editor_core"].get("public_deps", [])) | set(by_name["editor_core"].get("private_deps", []))
        for forbidden in ("gpu", "platform", "render", "app", "Astral::ImGui"):
            if forbidden in ec_deps:
                errors.append(f"editor_core must stay GPU/UI-free, but depends on '{forbidden}'")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    args = parser.parse_args()
    errors = check_isolation(args.manifest)
    if errors:
        for e in errors:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1
    print("editor_isolation_check: OK (runtime modules have zero editor dependencies; editor_core is GPU/UI-free)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
