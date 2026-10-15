#!/usr/bin/env python3

import csv
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPORT_EXECUTABLE = sys.argv.pop(1)


class MemoryReportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.corpus = self.root / "corpus.tsv"
        self.capacity = self.root / "capacity.tsv"
        self.report = self.root / "memory.csv"
        self.write_corpus('{"a":1}')
        self.write_capacity('"' + "a" * 70000 + '"')

    def write_corpus(self, text):
        path = self.root / "document.json"
        data = text.encode("utf-8")
        path.write_bytes(data)
        self.corpus.write_text(
            "profile\tvariant\tpath\tinput_bytes\n"
            f"sample\tcompact\t{path.name}\t{len(data)}\n"
            f"sample\tpretty\t{path.name}\t{len(data)}\n",
            encoding="utf-8",
        )

    def write_capacity(self, text, declared_size=None):
        path = self.root / "capacity.json"
        data = text.encode("utf-8")
        path.write_bytes(data)
        size = len(data) if declared_size is None else declared_size
        self.capacity.write_text(
            "name\tpath\tinput_bytes\n"
            f"capacity\t{path}\t{size}\n",
            encoding="utf-8",
        )

    def run_report(self):
        result = subprocess.run(
            [REPORT_EXECUTABLE, "--output", str(self.report),
             "--corpus-manifest", str(self.corpus),
             "--capacity-manifest", str(self.capacity)],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertTrue(self.report.exists(), result.stderr)
        with self.report.open(encoding="utf-8", newline="") as source:
            rows = list(csv.DictReader(source))
        return result, rows

    def test_expected_operations_and_capacity_failure_succeed(self):
        result, rows = self.run_report()
        self.assertEqual(result.returncode, 0, result.stderr)
        measured = [row for row in rows if row["operation"] != "capacity_check"]
        self.assertEqual(len(measured), 14)
        self.assertTrue(all(row["success"] == "true" for row in measured))
        capacity = rows[-1]
        self.assertEqual(capacity["operation"], "capacity_check")
        self.assertEqual(capacity["success"], "false")
        self.assertEqual(capacity["slab_used"], "0")

    def test_failed_corpus_operations_fail_the_report(self):
        self.write_corpus("!invalid")
        result, rows = self.run_report()
        self.assertEqual(result.returncode, 1, result.stderr)
        failed_parses = [row for row in rows if row["operation"] == "parse"]
        self.assertEqual(len(failed_parses), 4)
        self.assertTrue(all(row["success"] == "false" for row in failed_parses))
        self.assertIn("memory measurement failed", result.stderr)

    def test_unexpected_capacity_success_is_measured_and_rejected(self):
        self.write_capacity("true")
        result, rows = self.run_report()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(rows[-1]["success"], "true")
        self.assertGreater(int(rows[-1]["slab_used"]), 0)
        self.assertIn("unexpected capacity-check result", result.stderr)

    def test_syntax_error_is_not_a_capacity_failure(self):
        self.write_capacity("!invalid")
        result, rows = self.run_report()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(rows[-1]["success"], "false")
        self.assertIn("unexpected capacity-check result", result.stderr)

    def test_capacity_manifest_size_must_match_document(self):
        self.write_capacity('"' + "a" * 70000 + '"', declared_size=1)
        result, rows = self.run_report()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(rows[-1]["input_bytes"], "70002")
        self.assertIn("unexpected capacity-check result", result.stderr)


if __name__ == "__main__":
    unittest.main()
