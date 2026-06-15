#!/usr/bin/env python3

import argparse
import json
import sys
from pathlib import Path


TIME_UNIT_TO_NS = {
    "ns": 1.0,
    "us": 1_000.0,
    "ms": 1_000_000.0,
    "s": 1_000_000_000.0,
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Show paired SlabJson and cJSON benchmark timings."
    )
    parser.add_argument(
        "report",
        nargs="?",
        type=Path,
        default=Path("build-bench/benchmark-results/timings.json"),
        help="Google Benchmark JSON report",
    )
    return parser.parse_args()


def timing_ns(record):
    unit = record.get("time_unit")
    if unit not in TIME_UNIT_TO_NS:
        raise ValueError(f"unsupported time unit: {unit!r}")
    return record["cpu_time"] * TIME_UNIT_TO_NS[unit]


def parse_name(name):
    suffix = "_mean"
    if not name.endswith(suffix):
        return None

    parts = name[: -len(suffix)].split("/")
    if len(parts) < 3:
        raise ValueError(f"unexpected benchmark name: {name}")

    family, implementation, *fixture = parts
    if implementation not in ("SlabJson", "cJSON"):
        return None
    return "/".join((family, *fixture)), implementation


def load_rows(path):
    with path.open(encoding="utf-8") as source:
        report = json.load(source)

    rows = {}
    order = []
    for record in report.get("benchmarks", []):
        if record.get("aggregate_name") != "mean":
            continue

        parsed = parse_name(record.get("name", ""))
        if parsed is None:
            continue

        test, implementation = parsed
        if test not in rows:
            rows[test] = {}
            order.append(test)
        rows[test][implementation] = timing_ns(record)

    result = []
    for test in order:
        values = rows[test]
        slabjson_ns = values.get("SlabJson")
        cjson_ns = values.get("cJSON")
        delta = None
        if slabjson_ns is not None and cjson_ns is not None:
            delta = ((slabjson_ns - cjson_ns) / cjson_ns) * 100.0
        result.append(
            (
                test,
                None if slabjson_ns is None else round(slabjson_ns),
                None if cjson_ns is None else round(cjson_ns),
                delta,
            )
        )
    return result


def print_rows(rows):
    headers = ("Test", "SlabJson (ns)", "cJSON (ns)", "Delta")
    formatted = [
        (
            test,
            "-" if slabjson_ns is None else f"{slabjson_ns:d}",
            "-" if cjson_ns is None else f"{cjson_ns:d}",
            "-" if delta is None else f"{delta:+.1f}%",
        )
        for test, slabjson_ns, cjson_ns, delta in rows
    ]

    widths = [
        max(len(headers[index]), *(len(row[index]) for row in formatted))
        for index in range(len(headers))
    ]

    print(
        f"{headers[0]:<{widths[0]}}  "
        f"{headers[1]:>{widths[1]}}  "
        f"{headers[2]:>{widths[2]}}  "
        f"{headers[3]:>{widths[3]}}"
    )
    print(
        f"{'-' * widths[0]}  "
        f"{'-' * widths[1]}  "
        f"{'-' * widths[2]}  "
        f"{'-' * widths[3]}"
    )
    for row in formatted:
        print(
            f"{row[0]:<{widths[0]}}  "
            f"{row[1]:>{widths[1]}}  "
            f"{row[2]:>{widths[2]}}  "
            f"{row[3]:>{widths[3]}}"
        )

    print()
    print("Delta: negative is faster for SlabJson; positive is slower.")


def main():
    args = parse_args()
    try:
        rows = load_rows(args.report)
        if not rows:
            raise ValueError("report contains no paired mean benchmark results")
        print_rows(rows)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
