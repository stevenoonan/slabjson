#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--nativejson-data", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def load_json(path):
    with path.open("r", encoding="utf-8") as source:
        return json.load(source)


def write_json(path, value, pretty):
    text = json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        indent=0 if pretty else None,
        separators=None if pretty else (",", ":"),
    )
    path.write_text(text, encoding="utf-8", newline="\n")
    return len(text.encode("utf-8"))


def main():
    args = parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    twitter_path = args.nativejson_data / "twitter.json"
    citm_path = args.nativejson_data / "citm_catalog.json"
    canada_path = args.nativejson_data / "canada.json"

    twitter = load_json(twitter_path)
    citm = load_json(citm_path)
    canada = load_json(canada_path)

    profiles = []
    for count in (1, 4, 8):
        profiles.append(
            (
                f"twitter_{count}",
                {
                    "statuses": twitter["statuses"][:count],
                    "search_metadata": twitter["search_metadata"],
                },
            )
        )

    event_entries = list(citm["events"].items())
    for count in (10, 50, 100):
        profiles.append(
            (
                f"citm_events_{count}",
                {"events": dict(event_entries[:count])},
            )
        )

    source_feature = canada["features"][0]
    for count in (1, 8, 16):
        profiles.append(
            (
                f"canada_groups_{count}",
                {
                    "type": "FeatureCollection",
                    "features": [
                        {
                            "type": source_feature["type"],
                            "properties": source_feature["properties"],
                            "geometry": {
                                "type": source_feature["geometry"]["type"],
                                "coordinates": source_feature["geometry"][
                                    "coordinates"
                                ][:count],
                            },
                        }
                    ],
                },
            )
        )

    manifest_lines = ["profile\tvariant\tpath\tinput_bytes"]
    for profile, value in profiles:
        for variant, pretty in (("compact", False), ("pretty", True)):
            path = args.output / f"{profile}.{variant}.json"
            byte_count = write_json(path, value, pretty)
            manifest_lines.append(
                f"{profile}\t{variant}\t{path.name}\t{byte_count}"
            )

    (args.output / "corpus_manifest.tsv").write_text(
        "\n".join(manifest_lines) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    capacity_lines = ["name\tpath\tinput_bytes"]
    for path in (twitter_path, citm_path, canada_path):
        capacity_lines.append(f"{path.stem}\t{path}\t{path.stat().st_size}")
    (args.output / "capacity_manifest.tsv").write_text(
        "\n".join(capacity_lines) + "\n",
        encoding="utf-8",
        newline="\n",
    )


if __name__ == "__main__":
    main()
