#!/usr/bin/env python3
"""Fixture tests for tools/check_sizes.py."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS_DIR = Path(__file__).resolve().parent.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import check_sizes  # noqa: E402


def _make_function(name: str, body_lines: int) -> str:
    """Create a C++ function whose brace-to-brace span is exactly body_lines."""
    if body_lines <= 1:
        return f"void {name}() {{}}\n"
    inner = "".join(f"    int v{i} = {i};\n" for i in range(body_lines - 2))
    return f"void {name}() {{\n{inner}}}\n"


def _make_struct(name: str, member_count: int) -> str:
    fields = "".join(f"    int m{i} = {i};\n" for i in range(member_count))
    return f"struct {name} {{\n{fields}}};\n"


class StripCppTests(unittest.TestCase):
    def test_ignores_braces_in_comments_strings_and_raw_strings(self) -> None:
        src = (
            '// struct Fake { int a; };\n'
            '/* void fake_fn() {\n'
            '   }\n'
            '*/\n'
            'const char* s1 = "void fake2() { { { \\" }";\n'
            'const char* s2 = R"glsl(\n'
            '    struct InRaw {\n'
            '        int a; int b;\n'
            '    };\n'
            '    void raw_fn() {\n'
            + '        int x = 0;\n' * 90
            + '    }\n'
            ')glsl";\n'
            "int digit_sep = 1'000'000;\n"
            "char quote = '\\'';\n"
            "void real_fn() {\n"
            "    int x = 1;\n"
            "}\n"
        )
        findings = check_sizes.analyze_source(src, "modules/core/src/sample.cpp")
        self.assertEqual(findings, [])

    def test_preserves_line_numbers_across_multiline_constructs(self) -> None:
        prefix = (
            "/* line 1\n"
            "   line 2\n"
            "   line 3 */\n"
            'const char* r = R"(\n'
            "line 5\n"
            "line 6\n"
            ')";\n'
        )
        src = prefix + _make_function("too_long", 81)
        findings = check_sizes.analyze_source(src, "modules/core/src/lines.cpp")
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].kind, "function")
        self.assertEqual(findings[0].name, "too_long")
        self.assertEqual(findings[0].line, 8)
        self.assertEqual(findings[0].value, 81)


class BudgetRuleTests(unittest.TestCase):
    def test_file_line_limit(self) -> None:
        ok_src = "// line\n" * 600
        bad_src = "// line\n" * 601
        self.assertEqual(check_sizes.analyze_source(ok_src, "modules/core/src/ok.cpp"), [])
        findings = check_sizes.analyze_source(bad_src, "modules/core/src/bad.cpp")
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].kind, "file")
        self.assertEqual(findings[0].value, 601)

    def test_function_line_limit_and_macros(self) -> None:
        src = (
            "ASTRAL_REFLECT(Foo, x, y)\n"
            "namespace astral {\n"
            + _make_function("exactly_80", 80)
            + "template <class T>\n"
            + _make_function("over_81", 81)
            + "struct Holder {\n"
            "    int a = 0;\n"
            "    int b = 0;\n"
            "    Holder() : a{1}, b{2} {\n"
            + "".join(f"        a += {i};\n" for i in range(80))
            + "    }\n"
            "};\n"
            "} // namespace astral\n"
        )
        findings = check_sizes.analyze_source(src, "modules/world/include/astral/world/sample.hpp")
        names = [(f.kind, f.name, f.value) for f in findings]
        self.assertEqual(names, [
            ("function", "over_81", 81),
            ("function", "Holder::Holder", 82),
        ])

    def test_struct_member_counting(self) -> None:
        src = (
            "struct Small {\n"
            "    struct ForwardDecl;\n"
            "    class OtherForward;\n"
            "    enum class Kind : int;\n"
            "    using Alias = int;\n"
            "    typedef float Real;\n"
            "    friend class Buddy;\n"
            "    static_assert(true);\n"
            "    static constexpr int kConst = 42;\n"
            "    struct Nested { int n1; int n2; int n3; };\n"
            "    enum Mode { A, B } mode = A;\n"
            "    class App& app_ref;\n"
            "    void (*cb)(int) = nullptr;\n"
            "    std::function<void(int, float)> fn;\n"
            "    int arr[4] = {1, 2, 3, 4};\n"
            "    int m1, m2, m3;\n"
            + "".join(f"    int f{i} = 0;\n" for i in range(17))
            + "    void method(int x);\n"
            "    int inline_method() const { return 1; }\n"
            "};\n"
        )
        # mode (1) + app_ref (1) + cb (1) + fn (1) + arr (1) + m1..m3 (3) + f0..f16 (17) = 25 (passes)
        self.assertEqual(check_sizes.analyze_source(src, "modules/core/src/members.cpp"), [])

        # Add one more data member -> 26 (fails)
        over_src = src.replace("    int inline_method()", "    int extra = 0;\n    int inline_method()")
        findings = check_sizes.analyze_source(over_src, "modules/core/src/members.cpp")
        self.assertEqual(len(findings), 1)
        self.assertEqual(findings[0].kind, "members")
        self.assertEqual(findings[0].name, "Small")
        self.assertEqual(findings[0].value, 26)

    def test_nested_struct_field_and_independent_check(self) -> None:
        src = (
            "struct Outer {\n"
            "    struct Inner {\n"
            + "".join(f"        int x{i} = 0;\n" for i in range(26))
            + "    } inner;\n"
            "};\n"
        )
        findings = check_sizes.analyze_source(src, "modules/core/src/nested.cpp")
        self.assertEqual([(f.kind, f.name, f.value) for f in findings], [
            ("members", "Outer::Inner", 26),
        ])


class CliAndDiscoveryTests(unittest.TestCase):
    def _run_cli(self, argv: list[str]) -> tuple[int, str]:
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            code = check_sizes.main(argv)
        return code, buf.getvalue()

    def test_discovery_excludes_tests_and_deps_and_strict_mode(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            # Budgeted offenders
            (root / "modules" / "demo" / "src").mkdir(parents=True)
            (root / "modules" / "demo" / "src" / "big.cpp").write_text(
                _make_struct("BigStruct", 26) + _make_function("big_fn", 85),
                encoding="utf-8",
            )
            # Excluded directories: tests and _deps must be ignored
            (root / "modules" / "demo" / "tests").mkdir(parents=True)
            (root / "modules" / "demo" / "tests" / "huge_test.cpp").write_text(
                "// line\n" * 700, encoding="utf-8"
            )
            (root / "tools" / "_deps").mkdir(parents=True)
            (root / "tools" / "_deps" / "third_party.cpp").write_text(
                "// line\n" * 700, encoding="utf-8"
            )
            (root / "tools" / "tests").mkdir(parents=True)
            (root / "tools" / "tests" / "fixture.cpp").write_text(
                "// line\n" * 700, encoding="utf-8"
            )

            # Without budget file: default mode exits 0, --strict exits 1
            code, _ = self._run_cli(["--root", str(root)])
            self.assertEqual(code, 0)
            code, out = self._run_cli(["--root", str(root), "--strict"])
            self.assertEqual(code, 1)
            self.assertIn("BigStruct", out)
            self.assertIn("big_fn", out)
            self.assertNotIn("huge_test.cpp", out)
            self.assertNotIn("third_party.cpp", out)
            self.assertNotIn("fixture.cpp", out)

            # Summary prints at most 3 lines with counts and over budget / exceptions
            code, summary = self._run_cli(["--root", str(root), "--summary"])
            self.assertEqual(code, 0)
            lines = [line for line in summary.splitlines() if line.strip()]
            self.assertLessEqual(len(lines), 3)
            self.assertIn("over budget: 2, exceptions: 0", summary)

            # Add exceptions for both -> --strict exits 0
            budget = root / "tools" / "size_budget.json"
            exceptions = [
                {"path": "modules/demo/src/big.cpp", "kind": "members", "name": "BigStruct", "reason": "test"},
                {"path": "modules/demo/src/big.cpp", "kind": "function", "name": "big_fn", "reason": "test"},
            ]
            budget.write_text(json.dumps(exceptions), encoding="utf-8")
            code, summary = self._run_cli(["--root", str(root), "--summary", "--strict"])
            self.assertEqual(code, 0)
            self.assertIn("over budget: 0, exceptions: 2", summary)

            # Removing one exception -> --strict exits 1
            budget.write_text(json.dumps(exceptions[:1]), encoding="utf-8")
            code, summary = self._run_cli(["--root", str(root), "--summary", "--strict"])
            self.assertEqual(code, 1)
            self.assertIn("over budget: 1, exceptions: 1", summary)

    def test_exception_cap_stale_entries_and_near_limit(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            src = root / "modules" / "demo" / "src"
            src.mkdir(parents=True)
            (src / "grow.cpp").write_text(_make_function("grow_fn", 90), encoding="utf-8")
            (src / "close.cpp").write_text(_make_function("close_fn", 75), encoding="utf-8")
            budget = root / "tools" / "size_budget.json"
            budget.parent.mkdir(parents=True)

            # An exception with room under its cap passes.
            entry = {"path": "modules/demo/src/grow.cpp", "kind": "function", "name": "grow_fn", "reason": "t", "max": 95}
            budget.write_text(json.dumps([entry]), encoding="utf-8")
            code, out = self._run_cli(["--root", str(root), "--strict"])
            self.assertEqual(code, 0)
            self.assertIn("[EXCEPT]", out)
            # Grown beyond its cap: over budget again.
            entry["max"] = 85
            budget.write_text(json.dumps([entry]), encoding="utf-8")
            code, out = self._run_cli(["--root", str(root), "--strict"])
            self.assertEqual(code, 1)
            self.assertIn("exception cap 85 exceeded", out)

            # A function under its limit but within 90% is listed as near, without failing.
            self.assertIn("[NEAR]   function modules/demo/src/close.cpp", out)
            code, out = self._run_cli(["--root", str(root), "--near", "1"])
            self.assertNotIn("[NEAR]", out)

            # Fixed offender: its exception is stale and --strict fails until it is removed.
            (src / "grow.cpp").write_text(_make_function("grow_fn", 10), encoding="utf-8")
            code, out = self._run_cli(["--root", str(root), "--strict"])
            self.assertEqual(code, 1)
            self.assertIn("[STALE]", out)
            self.assertIn("stale exceptions: 1", out)
            budget.write_text("[]", encoding="utf-8")
            code, _ = self._run_cli(["--root", str(root), "--strict"])
            self.assertEqual(code, 0)

    def test_near_findings_are_not_over(self) -> None:
        near = check_sizes.analyze_source(_make_function("f", 76), "modules/core/src/n.cpp", near_ratio=0.9)
        self.assertEqual([(f.kind, f.over) for f in near], [("function", False)])
        self.assertEqual(check_sizes.analyze_source(_make_function("f", 76), "modules/core/src/n.cpp"), [])


if __name__ == "__main__":
    unittest.main()
