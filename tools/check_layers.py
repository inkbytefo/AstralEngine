#!/usr/bin/env python3
"""Validates AstralEngine module dependency edges against the committed layer allowlist.

Reads the configure-time module graph exported by astral_export_module_graph()
(build/<preset>/astral_module_graph.json) and verifies it against tools/module_layers.json:
  - every registered module is listed in the allowlist (and vice versa)
  - every internal public or private dependency is explicitly allowed
  - every edge respects layer ordering (source layer >= target layer)
  - the dependency graph is acyclic (both in the allowlist and in the configured manifest)
  - unrecognized Astral:: / astral_* internal aliases are rejected
  - third-party / system library targets are ignored; non-module Astral:: wrappers
    (such as Astral::ImGui) must be explicitly allowlisted for the modules that link them

Usage:
  check_layers.py --manifest <path> [--allowlist <path>]
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys
from typing import Any, Mapping, Sequence

_GENEX_NAMED = re.compile(r"^\$<[A-Z0-9_]+:(.+)>$")
_GENEX_COND = re.compile(r"^\$<\$<[^>]+>:(.+)>$")


@dataclass(frozen=True)
class ModulePolicy:
    name: str
    layer: int
    allowed_deps: tuple[str, ...]


@dataclass(frozen=True)
class ThirdPartyAliasPolicy:
    alias: str
    allowed_from: frozenset[str]
    reason: str


@dataclass(frozen=True)
class Allowlist:
    modules: Mapping[str, ModulePolicy]
    third_party_aliases: Mapping[str, ThirdPartyAliasPolicy]


@dataclass(frozen=True)
class ManifestModule:
    name: str
    target: str
    alias: str
    public_deps: tuple[str, ...]
    private_deps: tuple[str, ...]


@dataclass(frozen=True)
class CheckResult:
    errors: tuple[str, ...]
    module_count: int
    edge_count: int


def unwrap_genex(raw: str) -> str:
    """Strip simple CMake generator-expression wrappers around a target name."""
    val = raw.strip()
    while True:
        match = _GENEX_NAMED.match(val) or _GENEX_COND.match(val)
        if not match:
            return val
        val = match.group(1).strip()


def is_internal_reference(name: str) -> bool:
    """Return True if a link item looks like an internal Astral alias or target."""
    lower = name.lower()
    return "astral::" in lower or lower.startswith("astral_")


def find_cycles(graph: Mapping[str, Sequence[str]]) -> list[list[str]]:
    """Return deterministic cycle paths in a directed graph (empty if acyclic)."""
    # 0 = unvisited, 1 = on current DFS stack, 2 = completely explored
    state: dict[str, int] = {node: 0 for node in graph}
    stack: list[str] = []
    seen_cycles: set[tuple[str, ...]] = set()
    cycles: list[list[str]] = []

    def canonical_cycle(cycle_nodes: list[str]) -> tuple[str, ...]:
        min_idx = min(range(len(cycle_nodes)), key=cycle_nodes.__getitem__)
        rotated = cycle_nodes[min_idx:] + cycle_nodes[:min_idx]
        return tuple(rotated)

    def dfs(node: str) -> None:
        state[node] = 1
        stack.append(node)
        for dep in graph.get(node, ()):
            if dep not in state:
                continue
            if state[dep] == 1:
                idx = stack.index(dep)
                body = stack[idx:]
                key = canonical_cycle(body)
                if key not in seen_cycles:
                    seen_cycles.add(key)
                    cycles.append(body + [dep])
            elif state[dep] == 0:
                dfs(dep)
        stack.pop()
        state[node] = 2

    for node in sorted(graph):
        if state[node] == 0:
            dfs(node)
    return cycles


def parse_allowlist(data: Any) -> tuple[Allowlist | None, list[str]]:
    """Parse and self-validate the committed allowlist JSON structure."""
    errors: list[str] = []
    if not isinstance(data, dict):
        return None, ["allowlist root must be a JSON object"]

    raw_modules = data.get("modules")
    if not isinstance(raw_modules, dict) or not raw_modules:
        return None, ["allowlist 'modules' must be a non-empty JSON object"]

    modules: dict[str, ModulePolicy] = {}
    for name, spec in raw_modules.items():
        if not isinstance(name, str) or not name.strip():
            errors.append("allowlist contains an empty or non-string module name")
            continue
        if not isinstance(spec, dict):
            errors.append(f"allowlist entry for '{name}' must be an object")
            continue
        layer = spec.get("layer")
        if not isinstance(layer, int) or isinstance(layer, bool):
            errors.append(f"allowlist entry for '{name}' must have an integer 'layer'")
            continue
        raw_deps = spec.get("allowed_deps", [])
        if not isinstance(raw_deps, list) or not all(isinstance(d, str) for d in raw_deps):
            errors.append(f"allowlist entry for '{name}' must have a string list 'allowed_deps'")
            continue
        modules[name] = ModulePolicy(name=name, layer=layer, allowed_deps=tuple(raw_deps))

    raw_tp = data.get("third_party_aliases", {})
    if not isinstance(raw_tp, dict):
        errors.append("allowlist 'third_party_aliases' must be a JSON object")
        raw_tp = {}

    third_party_aliases: dict[str, ThirdPartyAliasPolicy] = {}
    for alias, spec in raw_tp.items():
        if not isinstance(alias, str) or not isinstance(spec, dict):
            errors.append(f"allowlist third_party_aliases entry '{alias}' is malformed")
            continue
        allowed_from = spec.get("allowed_from", [])
        if not isinstance(allowed_from, list) or not all(isinstance(m, str) for m in allowed_from):
            errors.append(f"third_party_aliases '{alias}' must specify a string list 'allowed_from'")
            continue
        for mod in allowed_from:
            if mod not in modules:
                errors.append(f"third_party_aliases '{alias}' references unknown module '{mod}'")
        third_party_aliases[alias] = ThirdPartyAliasPolicy(
            alias=alias,
            allowed_from=frozenset(allowed_from),
            reason=str(spec.get("reason", "")),
        )

    # Validate internal consistency of allowlist edges.
    allow_graph: dict[str, list[str]] = {name: [] for name in modules}
    for name, policy in modules.items():
        for dep in policy.allowed_deps:
            if dep == name:
                errors.append(f"allowlist self-dependency: {name} -> {name}")
                continue
            if dep not in modules:
                errors.append(f"allowlist edge '{name} -> {dep}' references unknown module '{dep}'")
                continue
            if policy.layer < modules[dep].layer:
                errors.append(
                    f"allowlist layer violation: {name} (layer {policy.layer}) "
                    f"cannot depend on higher-layer {dep} (layer {modules[dep].layer})"
                )
            allow_graph[name].append(dep)

    for cycle in find_cycles(allow_graph):
        errors.append(f"allowlist dependency cycle detected: {' -> '.join(cycle)}")

    return Allowlist(modules=modules, third_party_aliases=third_party_aliases), errors


def parse_manifest(data: Any) -> tuple[list[ManifestModule] | None, list[str]]:
    """Parse the configure-time JSON manifest exported by CMake."""
    errors: list[str] = []
    if not isinstance(data, dict):
        return None, ["manifest root must be a JSON object"]

    raw_modules = data.get("modules")
    if not isinstance(raw_modules, list):
        return None, ["manifest 'modules' must be a JSON list"]

    modules: list[ManifestModule] = []
    seen: set[str] = set()
    for idx, entry in enumerate(raw_modules):
        if not isinstance(entry, dict):
            errors.append(f"manifest module entry #{idx} must be a JSON object")
            continue
        name = entry.get("name")
        target = entry.get("target")
        alias = entry.get("alias")
        if not isinstance(name, str) or not name.strip():
            errors.append(f"manifest module entry #{idx} is missing string 'name'")
            continue
        if name in seen:
            errors.append(f"duplicate module '{name}' in manifest")
            continue
        seen.add(name)
        if not isinstance(target, str) or not target.strip():
            target = f"astral_{name}"
        if not isinstance(alias, str) or not alias.strip():
            alias = f"Astral::{name[:1].upper()}{name[1:]}"
        pub = entry.get("public_deps", [])
        priv = entry.get("private_deps", [])
        if not isinstance(pub, list) or not all(isinstance(d, str) for d in pub):
            errors.append(f"manifest module '{name}' has invalid 'public_deps'")
            continue
        if not isinstance(priv, list) or not all(isinstance(d, str) for d in priv):
            errors.append(f"manifest module '{name}' has invalid 'private_deps'")
            continue
        modules.append(
            ManifestModule(
                name=name,
                target=target,
                alias=alias,
                public_deps=tuple(pub),
                private_deps=tuple(priv),
            )
        )

    return modules, errors


def validate_graph(manifest_data: Any, allowlist_data: Any) -> CheckResult:
    """Validate manifest_data against allowlist_data and return all findings."""
    allowlist, allow_errors = parse_allowlist(allowlist_data)
    manifest_modules, manifest_errors = parse_manifest(manifest_data)
    errors: list[str] = [*allow_errors, *manifest_errors]

    if allowlist is None or manifest_modules is None:
        return CheckResult(errors=tuple(errors), module_count=0, edge_count=0)

    # Build lookup tables from the manifest's canonical module registry.
    by_name = {m.name: m for m in manifest_modules}
    canon_by_alias = {m.alias: m.name for m in manifest_modules}
    canon_by_target = {m.target: m.name for m in manifest_modules}

    # Verify manifest and allowlist agree on the set of engine modules.
    for mod_name in sorted(by_name):
        if mod_name not in allowlist.modules:
            errors.append(
                f"unregistered module '{mod_name}' is not listed in allowlist"
            )
    for mod_name in sorted(allowlist.modules):
        if mod_name not in by_name:
            errors.append(
                f"allowlist module '{mod_name}' is missing from configured manifest"
            )

    # Resolve all declared public and private dependencies.
    resolved_graph: dict[str, list[str]] = {m.name: [] for m in manifest_modules}
    resolved_edges: list[tuple[str, str, str]] = []  # (src, dst, scope)
    unique_edges: set[tuple[str, str]] = set()

    for mod in manifest_modules:
        scoped_lists = (("public", mod.public_deps), ("private", mod.private_deps))
        for scope, deps in scoped_lists:
            for raw_dep in deps:
                dep = unwrap_genex(raw_dep)
                if not dep:
                    continue

                if dep in allowlist.third_party_aliases:
                    tp = allowlist.third_party_aliases[dep]
                    if mod.name not in tp.allowed_from:
                        errors.append(
                            f"forbidden third-party alias ({scope}): "
                            f"{mod.name} -> {dep} (only allowed from: {', '.join(sorted(tp.allowed_from))})"
                        )
                    continue

                target_mod: str | None = None
                if dep in by_name:
                    target_mod = dep
                elif dep in canon_by_alias:
                    target_mod = canon_by_alias[dep]
                elif dep in canon_by_target:
                    target_mod = canon_by_target[dep]
                elif is_internal_reference(dep):
                    errors.append(
                        f"unrecognized internal alias ({scope}): {mod.name} -> {dep}"
                    )
                    continue
                else:
                    # Third-party / system library target (e.g. Vulkan::Vulkan, Threads::Threads, Jolt, m).
                    continue

                if target_mod == mod.name:
                    errors.append(
                        f"dependency cycle detected ({scope}): {mod.name} -> {mod.name}"
                    )
                    continue

                resolved_edges.append((mod.name, target_mod, scope))
                if target_mod not in resolved_graph[mod.name]:
                    resolved_graph[mod.name].append(target_mod)
                unique_edges.add((mod.name, target_mod))

    # Detect cycles across the combined public + private module graph.
    for cycle in find_cycles(resolved_graph):
        errors.append(f"dependency cycle detected: {' -> '.join(cycle)}")

    # Check every resolved edge against the allowlist and layer ordering.
    for src, dst, scope in resolved_edges:
        src_policy = allowlist.modules.get(src)
        dst_policy = allowlist.modules.get(dst)
        if src_policy is None or dst_policy is None:
            continue
        if src_policy.layer < dst_policy.layer:
            errors.append(
                f"layer violation ({scope}): {src} (layer {src_policy.layer}) "
                f"-> {dst} (layer {dst_policy.layer})"
            )
        if dst not in src_policy.allowed_deps:
            errors.append(
                f"forbidden {scope} dependency: {src} -> {dst} "
                f"(not in allowlist; update tools/module_layers.json and docs/architecture.md if intentional)"
            )

    return CheckResult(
        errors=tuple(errors),
        module_count=len(manifest_modules),
        edge_count=len(unique_edges),
    )


def load_json_file(path: Path, label: str) -> Any:
    if not path.is_file():
        raise FileNotFoundError(f"{label} file not found: {path}")
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON in {label} file {path}: {exc}") from exc


def main(argv: Sequence[str] | None = None) -> int:
    default_allowlist = Path(__file__).resolve().parent / "module_layers.json"
    parser = argparse.ArgumentParser(
        description="Verify CMake-exported Astral module dependencies against the layer allowlist."
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        required=True,
        help="Path to the configure-time astral_module_graph.json manifest",
    )
    parser.add_argument(
        "--allowlist",
        type=Path,
        default=default_allowlist,
        help="Path to tools/module_layers.json",
    )
    args = parser.parse_args(argv)

    try:
        manifest_data = load_json_file(args.manifest, "manifest")
        allowlist_data = load_json_file(args.allowlist, "allowlist")
    except (FileNotFoundError, ValueError, OSError) as exc:
        print(f"layer_check: error: {exc}", file=sys.stderr)
        return 2

    result = validate_graph(manifest_data, allowlist_data)
    if result.errors:
        print(f"layer_check: FAILED ({len(result.errors)} error(s))")
        for err in result.errors:
            print(f"  - {err}")
        return 1

    print(
        f"layer_check: ok ({result.module_count} modules, {result.edge_count} directed edges)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
