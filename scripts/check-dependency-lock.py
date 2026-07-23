#!/usr/bin/env python3
"""Validate that the dependency lock, manifest, and SPDX catalog agree."""

from __future__ import annotations

import json
import re
import sys
import tomllib
from datetime import date
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OSV_LOCK = ROOT / "osv-scanner-custom.json"
VCPKG_MANIFEST = ROOT / "vcpkg.json"
SBOM = ROOT / "docs/security/sbom/libcimbar.spdx.json"
OSV_CONFIG = ROOT / "osv-scanner.toml"
COMMIT = re.compile(r"^[0-9a-f]{40}$")
GITHUB_LOCATION = re.compile(
    r"^git\+https://github\.com/(?P<repo>[^@]+)@(?P<commit>[0-9a-f]{40})$",
    re.IGNORECASE,
)

EXPECTED_OSV_PACKAGES = 15
EXPECTED_VCPKG_DIRECT = {"glfw3", "opencv4", "opengl-registry"}
EXPECTED_IGNORED_VULNS = {
    "OSV-2022-394",
    "OSV-2023-444",
    "OSV-2025-16",
    "OSV-2025-17",
    "OSV-2025-51",
    "OSV-2025-68",
    "OSV-2025-190",
}


def load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"cannot read valid JSON from {path.relative_to(ROOT)}: {exc}") from exc


def osv_coordinates(document: object) -> set[tuple[str, str]]:
    if not isinstance(document, dict) or not isinstance(document.get("results"), list):
        raise ValueError("OSV lock must contain a results array")

    coordinates: set[tuple[str, str]] = set()
    for result in document["results"]:
        if not isinstance(result, dict) or not isinstance(result.get("packages"), list):
            raise ValueError("every OSV result must contain a packages array")
        for entry in result["packages"]:
            package = entry.get("package") if isinstance(entry, dict) else None
            name = package.get("name") if isinstance(package, dict) else None
            commit = package.get("commit") if isinstance(package, dict) else None
            if not isinstance(name, str) or not name.lower().startswith("github.com/"):
                raise ValueError(f"invalid GitHub package name in OSV lock: {name!r}")
            if not isinstance(commit, str) or not COMMIT.fullmatch(commit):
                raise ValueError(f"invalid commit for {name!r}: {commit!r}")
            coordinate = (name.removeprefix("github.com/").lower(), commit)
            if coordinate in coordinates:
                raise ValueError(f"duplicate OSV coordinate: {name}@{commit}")
            coordinates.add(coordinate)

    if len(coordinates) != EXPECTED_OSV_PACKAGES:
        raise ValueError(
            f"OSV lock has {len(coordinates)} packages; expected {EXPECTED_OSV_PACKAGES}"
        )
    return coordinates


def sbom_coordinates(document: object) -> set[tuple[str, str]]:
    if not isinstance(document, dict) or not isinstance(document.get("packages"), list):
        raise ValueError("SPDX SBOM must contain a packages array")

    coordinates: set[tuple[str, str]] = set()
    for package in document["packages"]:
        if not isinstance(package, dict):
            raise ValueError("SPDX package entry must be an object")
        location = package.get("downloadLocation")
        if not isinstance(location, str):
            continue
        match = GITHUB_LOCATION.fullmatch(location)
        if match is None:
            continue
        repo = match.group("repo").lower()
        if repo == "rory-hughes/libcimbar":
            continue
        coordinates.add((repo, match.group("commit")))

    return coordinates


def validate_vcpkg(document: object) -> None:
    if not isinstance(document, dict):
        raise ValueError("vcpkg manifest must be a JSON object")
    baseline = document.get("builtin-baseline")
    if not isinstance(baseline, str) or not COMMIT.fullmatch(baseline):
        raise ValueError("vcpkg builtin-baseline must be a lowercase 40-character commit")

    if document.get("dependencies") != []:
        raise ValueError("base vcpkg dependencies must stay empty for the hardened-only profile")
    if document.get("default-features") != ["compatibility-apps"]:
        raise ValueError("compatibility-apps must remain the only default vcpkg feature")
    features = document.get("features")
    compatibility = features.get("compatibility-apps") if isinstance(features, dict) else None
    dependencies = compatibility.get("dependencies") if isinstance(compatibility, dict) else None
    if not isinstance(dependencies, list):
        raise ValueError("compatibility-apps must contain its direct dependencies")
    actual = {
        item if isinstance(item, str) else item.get("name")
        for item in dependencies
        if isinstance(item, (str, dict))
    }
    if actual != EXPECTED_VCPKG_DIRECT:
        raise ValueError(
            "vcpkg direct dependencies differ from the audited set: "
            f"expected {sorted(EXPECTED_VCPKG_DIRECT)}, got {sorted(actual)}"
        )

    opencv = next(
        (item for item in dependencies if isinstance(item, dict) and item.get("name") == "opencv4"),
        None,
    )
    if opencv is None or opencv.get("default-features") is not False:
        raise ValueError("opencv4 must disable its broad default feature set")
    if set(opencv.get("features", [])) != {"calib3d", "jpeg", "png", "thread"}:
        raise ValueError("opencv4 features must remain limited to calib3d, jpeg, png, and thread")


def validate_osv_exceptions() -> None:
    try:
        config = tomllib.loads(OSV_CONFIG.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as exc:
        raise ValueError(f"cannot read valid TOML from {OSV_CONFIG.name}: {exc}") from exc

    entries = config.get("IgnoredVulns")
    if not isinstance(entries, list):
        raise ValueError("osv-scanner.toml must contain IgnoredVulns entries")
    actual = {entry.get("id") for entry in entries if isinstance(entry, dict)}
    if actual != EXPECTED_IGNORED_VULNS or len(entries) != len(EXPECTED_IGNORED_VULNS):
        raise ValueError(
            "OSV exception set differs from the reviewed OpenCV findings: "
            f"expected {sorted(EXPECTED_IGNORED_VULNS)}, got {sorted(actual)}"
        )
    for entry in entries:
        expiry = entry.get("ignoreUntil")
        reason = entry.get("reason")
        if not isinstance(expiry, date) or expiry < date.today():
            raise ValueError(f"OSV exception {entry.get('id')} is expired or has no valid expiry")
        if not isinstance(reason, str) or len(reason.strip()) < 20:
            raise ValueError(f"OSV exception {entry.get('id')} has no meaningful reason")

    forbidden_calls = ("cv::imread", "cv::imdecode", "cv::split")
    for path in (ROOT / "src").rglob("*"):
        if not path.is_file() or path.suffix not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
            continue
        if "test" in path.relative_to(ROOT / "src").parts or "third_party_lib" in path.parts:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for forbidden in forbidden_calls:
            if forbidden in content:
                raise ValueError(
                    f"OSV reachability exception is invalid: {forbidden} appears in "
                    f"{path.relative_to(ROOT)}"
                )


def main() -> int:
    try:
        locked = osv_coordinates(load_json(OSV_LOCK))
        cataloged = sbom_coordinates(load_json(SBOM))
        validate_vcpkg(load_json(VCPKG_MANIFEST))
        validate_osv_exceptions()

        if locked != cataloged:
            missing = sorted(locked - cataloged)
            extra = sorted(cataloged - locked)
            raise ValueError(
                "OSV and SPDX Git coordinates differ; regenerate the SBOM and update both locks. "
                f"missing from SPDX={missing}; extra in SPDX={extra}"
            )
    except ValueError as exc:
        print(f"dependency lock check failed: {exc}", file=sys.stderr)
        return 1

    print(
        f"dependency lock check passed: {len(locked)} Git commits and "
        f"{len(EXPECTED_VCPKG_DIRECT)} direct vcpkg dependencies, and "
        f"{len(EXPECTED_IGNORED_VULNS)} live reachability exceptions"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
