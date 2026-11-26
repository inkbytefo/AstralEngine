import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_docs  # noqa: E402


class CheckDocsTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "modules/core/src").mkdir(parents=True)
        (self.root / "modules/core/src/a.cpp").write_text("1\n2\n3\n", encoding="utf-8")
        (self.root / "docs/modules").mkdir(parents=True)
        (self.root / "docs/guide.md").write_text("x", encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def problems(self, text):
        md = self.root / "docs/modules/core.md"
        md.write_text(text, encoding="utf-8")
        return check_docs.check_file(self.root, "docs/modules/core.md")

    def test_valid_links_and_references_pass(self):
        self.assertEqual(self.problems(
            "[g](../guide.md) [c](../../modules/core/src/a.cpp:2) `modules/core/src/a.cpp:1-3` "
            "[web](https://example.com) [anchor](#x) [g2](../guide.md#part)"), [])

    def test_broken_link_is_reported(self):
        self.assertEqual(len(self.problems("[g](../nope.md)")), 1)

    def test_reference_past_end_of_file_is_reported(self):
        self.assertIn("out of range", self.problems("`modules/core/src/a.cpp:4`")[0])
        self.assertIn("out of range", self.problems("`modules/core/src/a.cpp:2-9`")[0])

    def test_reference_to_missing_file_is_reported(self):
        self.assertIn("missing file", self.problems("`modules/core/src/b.cpp:1`")[0])

    def test_fenced_code_is_ignored(self):
        self.assertEqual(self.problems("```\n[g](../nope.md) `modules/x.cpp:9`\n```\n"), [])


if __name__ == "__main__":
    unittest.main()
