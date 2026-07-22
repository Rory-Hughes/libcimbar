#!/usr/bin/env python3
"""Fail when CodeQL SARIF contains a first-party production finding."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from urllib.parse import unquote, urlparse


def source_path(uri: str) -> str:
    parsed = urlparse(uri)
    path = unquote(parsed.path if parsed.scheme else uri).replace("\\", "/")
    return "/" + path.lstrip("/")


def is_first_party_production(path: str) -> bool:
    lowered = path.lower()
    if "/src/third_party_lib/" in lowered or "/test/" in lowered:
        return False
    return any(root in lowered for root in ("/src/lib/", "/src/profile/", "/src/exe/"))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("sarif_root", type=Path)
    args = parser.parse_args()

    sarif_files = sorted(args.sarif_root.rglob("*.sarif"))
    if not sarif_files:
        print(f"No CodeQL SARIF found under {args.sarif_root}", file=sys.stderr)
        return 2

    actionable: list[tuple[str, str, int, str]] = []
    excluded = 0
    for sarif_file in sarif_files:
        document = json.loads(sarif_file.read_text(encoding="utf-8"))
        for run in document.get("runs", []):
            for result in run.get("results", []):
                locations = result.get("locations", [])
                if not locations:
                    excluded += 1
                    continue
                physical = locations[0].get("physicalLocation", {})
                path = source_path(physical.get("artifactLocation", {}).get("uri", ""))
                if not is_first_party_production(path):
                    excluded += 1
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
        print(f"CodeQL found {len(actionable)} first-party production issue(s).")
        return 1

    print(f"CodeQL: 0 first-party production findings; {excluded} excluded result(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
