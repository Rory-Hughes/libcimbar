#!/usr/bin/env python3
"""Fail when Clang SARIF contains an actionable first-party production finding."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from urllib.parse import unquote, urlparse


def source_path(uri: str) -> str:
    parsed = urlparse(uri)
    path = unquote(parsed.path if parsed.scheme else uri)
    return path.replace("\\", "/")


def is_first_party_production(path: str) -> bool:
    lowered = path.lower()
    if "/src/third_party_lib/" in lowered or "/test/" in lowered:
        return False
    return any(root in lowered for root in ("/src/lib/", "/src/profile/", "/src/exe/"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("sarif_root", type=Path)
    args = parser.parse_args()

    sarif_files = sorted(args.sarif_root.rglob("results-merged.sarif"))
    if not sarif_files:
        print(f"No merged Clang SARIF found under {args.sarif_root}", file=sys.stderr)
        return 2

    actionable: list[tuple[str, str, int, str]] = []
    ignored = 0
    for sarif_file in sarif_files:
        document = json.loads(sarif_file.read_text(encoding="utf-8"))
        for run in document.get("runs", []):
            for result in run.get("results", []):
                locations = result.get("locations", [])
                if not locations:
                    ignored += 1
                    continue
                physical = locations[0].get("physicalLocation", {})
                uri = physical.get("artifactLocation", {}).get("uri", "")
                path = source_path(uri)
                if not is_first_party_production(path):
                    ignored += 1
                    continue
                region = physical.get("region", {})
                actionable.append((
                    result.get("ruleId", "unknown"),
                    path,
                    int(region.get("startLine", 0)),
                    result.get("message", {}).get("text", ""),
                ))

    for rule, path, line, message in actionable:
        print(f"{rule}: {path}:{line}: {message}")

    if actionable:
        print(f"Clang static analyzer found {len(actionable)} first-party production issue(s).")
        return 1

    print(f"Clang static analyzer: 0 first-party production findings; {ignored} excluded result(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
