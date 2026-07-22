#!/usr/bin/env python3
"""Generate and verify deterministic libcimbar fountain fuzz corpora."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PACKET_SIZE = 122


@dataclass(frozen=True)
class CorpusCase:
    target: str
    filename: str
    data: bytes
    description: str

    @property
    def path(self) -> Path:
        return ROOT / self.target / self.filename


def metadata(encode_id: int, size: int, block_id: int) -> bytes:
    return bytes(
        (
            (encode_id & 0x7F) | ((size >> 17) & 0x80),
            (size >> 16) & 0xFF,
            (size >> 8) & 0xFF,
            size & 0xFF,
            (block_id >> 8) & 0xFF,
            block_id & 0xFF,
        )
    )


def start_frame(
    encode_id: int,
    object_size: int,
    block_id: int,
    payload_byte: int = 0x41,
) -> bytes:
    """State-harness operation 1 followed by its fixed 122-byte payload."""
    object_seed = object_size - 1
    return (
        bytes((1, encode_id & 0x7F))
        + struct.pack("<HH", object_seed, block_id)
        + bytes((payload_byte,)) * PACKET_SIZE
    )


def complete_transfer(encode_id: int, extra_size: int = 0) -> bytes:
    """State-harness operation 8 with enough source bytes for completion."""
    payload = bytes(((encode_id + index) & 0xFF) for index in range(32))
    return bytes((8, encode_id & 0x7F, extra_size & 0xFF)) + payload


def raw_frame(frame: bytes) -> bytes:
    if not 1 <= len(frame) <= 256:
        raise ValueError("raw operation supports 1..256 bytes")
    return bytes((0, len(frame) - 1)) + frame


CASES = (
    CorpusCase("fountain_metadata", "empty.bin", b"", "Empty metadata input."),
    CorpusCase("fountain_metadata", "one-byte.bin", b"\x7f", "One-byte truncated header."),
    CorpusCase("fountain_metadata", "five-byte.bin", b"\xff\x01\x02\x03\x04", "Header truncated before the low block byte."),
    CorpusCase("fountain_metadata", "zero-fields.bin", metadata(0, 0, 0), "Canonical all-zero fields."),
    CorpusCase("fountain_metadata", "ordinary.bin", metadata(7, 1200, 42), "Ordinary canonical transfer metadata."),
    CorpusCase("fountain_metadata", "size-high-bit.bin", metadata(1, 1 << 24, 1), "Twenty-fifth object-size bit set."),
    CorpusCase("fountain_metadata", "maximum-fields.bin", metadata(0x7F, 0x1FFFFFF, 0xFFFF), "Maximum encodable ID, object size, and block ID."),
    CorpusCase("fountain_state", "frame-too-short.bin", raw_frame(metadata(1, 200, 0)[:5]), "Raw frame shorter than the six-byte metadata header."),
    CorpusCase("fountain_state", "frame-misaligned.bin", raw_frame(metadata(1, 200, 0) + bytes(121)), "Non-empty 127-byte frame rejected before state mutation."),
    CorpusCase("fountain_state", "one-block-invalid.bin", start_frame(1, 100, 0), "Object requiring one Wirehair block fails configuration admission."),
    CorpusCase("fountain_state", "block-policy-limit.bin", start_frame(2, 200, 97), "Block ID one above the selected policy maximum."),
    CorpusCase("fountain_state", "stream-slot-conflict.bin", start_frame(0, 200, 0) + b"\x03", "Encode IDs separated by eight collide in a storage slot but retain distinct full IDs."),
    CorpusCase("fountain_state", "duplicate-no-progress-cancel.bin", start_frame(3, 200, 0) + b"\x02\x05", "Duplicate packets consume no unique-block budget and eventually trigger bounded cancellation."),
    CorpusCase("fountain_state", "timeout-release.bin", start_frame(4, 200, 0) + b"\x06", "Expiry sweep releases an incomplete transfer and its reservations."),
    CorpusCase("fountain_state", "reset-release.bin", start_frame(5, 200, 0) + b"\x07", "Explicit reset clears active and terminal state."),
    CorpusCase("fountain_state", "recover-unknown.bin", b"\x04\xef\xbe\xad\xde", "Recovery request for an unknown little-endian identifier."),
    CorpusCase("fountain_state", "mixed-batch-metadata.bin", b"\x09", "Self-generated two-packet batch with conflicting transfer metadata."),
    CorpusCase(
        "fountain_state",
        "completion-replay-after-eviction.bin",
        complete_transfer(1) + complete_transfer(2) + complete_transfer(3) + complete_transfer(1),
        "Completed encode ID remains terminal after bounded detail-record eviction.",
    ),
    CorpusCase(
        "fountain_state",
        "completion-reuse-after-reset.bin",
        complete_transfer(6) + b"\x02\x07" + complete_transfer(6),
        "Replay is refused before reset and the encode ID may be reused only after reset.",
    ),
)


def manifest_bytes() -> bytes:
    entries = []
    for case in CASES:
        entries.append(
            {
                "description": case.description,
                "filename": case.filename,
                "sha256": hashlib.sha256(case.data).hexdigest(),
                "size": len(case.data),
                "target": case.target,
            }
        )
    document = {"schema_version": 1, "entries": entries}
    return (json.dumps(document, indent=2, sort_keys=True) + "\n").encode("utf-8")


def expected_files() -> dict[Path, bytes]:
    files = {case.path: case.data for case in CASES}
    files[ROOT / "manifest.json"] = manifest_bytes()
    return files


def verify() -> list[str]:
    problems = []
    expected = expected_files()
    for path, data in expected.items():
        if not path.is_file():
            problems.append(f"missing: {path.relative_to(ROOT)}")
        elif path.read_bytes() != data:
            problems.append(f"content mismatch: {path.relative_to(ROOT)}")

    expected_bins = {path.resolve() for path in expected if path.suffix == ".bin"}
    actual_bins = {path.resolve() for path in ROOT.glob("*/*.bin")}
    for path in sorted(actual_bins - expected_bins):
        problems.append(f"unexpected: {path.relative_to(ROOT)}")
    return problems


def generate() -> None:
    for path, data in expected_files().items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify committed files without modifying them")
    args = parser.parse_args()

    if args.check:
        problems = verify()
        if problems:
            for problem in problems:
                print(problem, file=sys.stderr)
            return 1
        print(f"Corpus check passed: {len(CASES)} seeds")
        return 0

    generate()
    print(f"Generated {len(CASES)} seeds and manifest under {ROOT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
