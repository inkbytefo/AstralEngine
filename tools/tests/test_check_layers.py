#!/usr/bin/env python3
"""Fixture tests for tools/check_layers.py and tools/module_layers.json."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS_DIR = Path(__file__).resolve().parent.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import check_layers  # noqa: E402


def _make_allowlist(
    modules: dict[str, tuple[int, list[str]]],
    third_party_aliases: dict[str, dict[str, object]] | None = None,
) -> dict[str, object]:
    data: dict[str, object] = {
        "schema_version": 1,
        "modules": {
            name: {"layer": layer, "allowed_deps": deps}
            for name, (layer, deps) in modules.items()
        },
    }
    if third_party_aliases is not None:
        data["third_party_aliases"] = third_party_aliases
    return data


def _make_manifest(
    modules: list[tuple[str, list[str], list[str]]],
) -> dict[str, object]:
    entries = []
    for name, pub, priv in modules:
        alias = f"Astral::{name[:1].upper()}{name[1:]}"
        entries.append(
            {
                "name": name,
                "target": f"astral_{name}",
                "alias": alias,
                "public_deps": pub,
                "private_deps": priv,
            }
        )
    return {"schema_version": 1, "modules": entries}


class LayerCheckFixtureTests(unittest.TestCase):
    def _base_allowlist(self) -> dict[str, object]:
        return _make_allowlist(
            {
                "core": (0, []),
                "gpu": (1, ["core"]),
                "render": (2, ["core", "gpu"]),
                "world": (2, ["core"]),
                "app": (3, ["core", "gpu", "render", "world"]),
            },
            third_party_aliases={
                "Astral::ImGui": {
                    "allowed_from": ["app"],
                    "reason": "Dear ImGui wrapper target",
                }
            },
        )

    def test_allowed_graph_with_public_private_and_third_party_deps_passes(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", ["Threads::Threads"], []),
                ("gpu", ["Astral::Core"], ["Vulkan::Vulkan", "$<BUILD_INTERFACE:Vulkan::Headers>"]),
                ("render", ["core"], ["Astral::Gpu", "glslang::glslang"]),
                ("world", ["astral_core"], ["m"]),
                ("app", ["core", "gpu", "render", "world", "Astral::ImGui"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertEqual(result.errors, ())
        self.assertEqual(result.module_count, 5)
        self.assertEqual(result.edge_count, 8)

    def test_forbidden_public_edge_fails(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu", "world"], []),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("forbidden public dependency: render -> world" in e for e in result.errors),
            result.errors,
        )

    def test_forbidden_private_edge_fails(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu"], ["Astral::World"]),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("forbidden private dependency: render -> world" in e for e in result.errors),
            result.errors,
        )

    def test_forbidden_genex_wrapped_private_edge_fails(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu"], ["$<LINK_ONLY:$<BUILD_INTERFACE:Astral::World>>"]),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("forbidden private dependency: render -> world" in e for e in result.errors),
            result.errors,
        )

    def test_cycle_across_public_and_private_edges_fails(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu", "world"], []),
                ("world", ["core"], ["Astral::Render"]),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("dependency cycle detected" in e for e in result.errors),
            result.errors,
        )

    def test_self_dependency_cycle_fails(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", ["core"], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu"], []),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("dependency cycle detected" in e and "core -> core" in e for e in result.errors),
            result.errors,
        )

    def test_allowlist_cycle_is_rejected(self) -> None:
        allowlist = _make_allowlist(
            {
                "core": (0, []),
                "a": (1, ["core", "b"]),
                "b": (1, ["core", "a"]),
            }
        )
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("a", ["core", "b"], []),
                ("b", ["core", "a"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("allowlist dependency cycle detected" in e for e in result.errors),
            result.errors,
        )

    def test_unrecognized_internal_alias_and_target_are_rejected(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core", "Astral::Bogus"], []),
                ("render", ["core", "gpu"], ["astral_phantom"]),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("unrecognized internal alias (public): gpu -> Astral::Bogus" in e for e in result.errors),
            result.errors,
        )
        self.assertTrue(
            any("unrecognized internal alias (private): render -> astral_phantom" in e for e in result.errors),
            result.errors,
        )

    def test_third_party_alias_from_disallowed_module_fails(self) -> None:
        allowlist = self._base_allowlist()
        manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu", "Astral::ImGui"], []),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world", "Astral::ImGui"], []),
            ]
        )
        result = check_layers.validate_graph(manifest, allowlist)
        self.assertTrue(
            any("forbidden third-party alias (public): render -> Astral::ImGui" in e for e in result.errors),
            result.errors,
        )

    def test_cli_works_from_configured_tree_with_spaces(self) -> None:
        allowlist = self._base_allowlist()
        valid_manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core"], []),
                ("render", ["core", "gpu"], []),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world", "Astral::ImGui"], []),
            ]
        )
        invalid_manifest = _make_manifest(
            [
                ("core", [], []),
                ("gpu", ["core", "render"], []),
                ("render", ["core", "gpu"], []),
                ("world", ["core"], []),
                ("app", ["core", "gpu", "render", "world"], []),
            ]
        )
        with tempfile.TemporaryDirectory() as tmp:
            spaced_build = Path(tmp) / "build tree with spaces"
            spaced_rules = Path(tmp) / "rules dir with spaces"
            spaced_build.mkdir(parents=True)
            spaced_rules.mkdir(parents=True)

            allowlist_path = spaced_rules / "module layers.json"
            valid_path = spaced_build / "astral module graph.json"
            invalid_path = spaced_build / "invalid module graph.json"

            allowlist_path.write_text(json.dumps(allowlist), encoding="utf-8")
            valid_path.write_text(json.dumps(valid_manifest), encoding="utf-8")
            invalid_path.write_text(json.dumps(invalid_manifest), encoding="utf-8")

            script = TOOLS_DIR / "check_layers.py"
            ok_proc = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "--manifest",
                    str(valid_path),
                    "--allowlist",
                    str(allowlist_path),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(ok_proc.returncode, 0, ok_proc.stdout + ok_proc.stderr)
            self.assertIn("layer_check: ok", ok_proc.stdout)

            bad_proc = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "--manifest",
                    str(invalid_path),
                    "--allowlist",
                    str(allowlist_path),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(bad_proc.returncode, 1, bad_proc.stdout + bad_proc.stderr)
            self.assertIn("dependency cycle detected", bad_proc.stdout)
            self.assertIn("layer violation", bad_proc.stdout)

    def test_committed_allowlist_is_self_consistent(self) -> None:
        allowlist_path = TOOLS_DIR / "module_layers.json"
        data = check_layers.load_json_file(allowlist_path, "allowlist")
        allowlist, errors = check_layers.parse_allowlist(data)
        self.assertIsNotNone(allowlist)
        self.assertEqual(errors, [])

        # Confirm CLI exit 2 on missing manifest file.
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            code = check_layers.main(
                ["--manifest", str(TOOLS_DIR / "does_not_exist.json"), "--allowlist", str(allowlist_path)]
            )
        self.assertEqual(code, 2)


if __name__ == "__main__":
    unittest.main()
