#!/usr/bin/env python3
"""Generate and verify deterministic libcimbar fuzz corpora."""

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
FRAME_SEQUENCE_OP_DELIVER_NEXT = 0
FRAME_SEQUENCE_OP_DROP_NEXT = 1
FRAME_SEQUENCE_OP_DUPLICATE_LAST = 2
FRAME_SEQUENCE_OP_DELIVER_SELECTED = 3
FRAME_SEQUENCE_OP_MUTATE_SELECTED = 4
FRAME_SEQUENCE_OP_DELAY = 5
FRAME_SEQUENCE_OP_RESET = 6
FRAME_SEQUENCE_OP_BATCH_NEXT = 7
FRAME_SEQUENCE_OP_COMPLETE = 8
FRAME_SEQUENCE_OP_TAKE_CHECK = 9


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


def raw_frame_case(control: int, width: int, height: int, size_mode: int, payload: bytes) -> bytes:
    return bytes((control & 0xFF, width & 0xFF, height & 0xFF, size_mode & 0xFF)) + payload


def gradient_payload(length: int, salt: int) -> bytes:
    return bytes(((index * 37 + salt) & 0xFF) for index in range(length))


def frame_sequence_case(
    encode_id: int,
    object_seed: int,
    source_seed: bytes,
    operations: bytes,
) -> bytes:
    if not 1 <= len(source_seed) <= 32:
        raise ValueError("frame-sequence source seed must be 1..32 bytes")
    return (
        bytes((encode_id & 0x7F,))
        + struct.pack("<H", object_seed & 0xFFFF)
        + bytes((len(source_seed) - 1,))
        + source_seed
        + operations
    )


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
    CorpusCase("raw_frame", "valid-rgb.bin", raw_frame_case(0x10, 16, 16, 0, gradient_payload(64, 1)), "Valid tightly packed RGB envelope with generated gradient pixels."),
    CorpusCase("raw_frame", "valid-rgba.bin", raw_frame_case(0x20, 16, 16, 0, gradient_payload(64, 2)), "Valid RGBA envelope exercising alpha-stripping conversion."),
    CorpusCase("raw_frame", "valid-nv12.bin", raw_frame_case(0x30, 16, 16, 0, gradient_payload(64, 3)), "Valid even-dimension NV12 envelope exercising YUV conversion."),
    CorpusCase("raw_frame", "valid-i420.bin", raw_frame_case(0x40, 16, 16, 0, gradient_payload(64, 4)), "Valid even-dimension I420 envelope exercising planar YUV conversion."),
    CorpusCase("raw_frame", "cropped-pattern.bin", raw_frame_case(0x10, 33, 47, 0x0C, b"\x00\xff\x00\xff"), "Small cropped-looking checker pattern through RGB geometry scanning."),
    CorpusCase("raw_frame", "rotated-pattern.bin", raw_frame_case(0x12, 61, 53, 0x08, bytes(range(32))), "Non-square rotated-looking gradient pattern through scanner and deskew paths."),
    CorpusCase("raw_frame", "noisy-frame.bin", raw_frame_case(0x10, 64, 64, 0x10, gradient_payload(256, 99)), "Noisy generated RGB frame for scanner candidate stress."),
    CorpusCase("raw_frame", "overexposed-frame.bin", raw_frame_case(0x10, 64, 64, 0x04, b"\xff"), "All-white overexposed frame that should fail extraction cheaply."),
    CorpusCase("raw_frame", "damaged-short-buffer.bin", raw_frame_case(0x10, 32, 32, 0x01, gradient_payload(31, 7)), "RGB frame with one byte missing from the declared tight size."),
    CorpusCase("raw_frame", "damaged-long-buffer.bin", raw_frame_case(0x10, 32, 32, 0x02, gradient_payload(31, 8)), "RGB frame with one extra byte beyond the declared tight size."),
    CorpusCase("raw_frame", "odd-nv12-dimensions.bin", raw_frame_case(0x30, 17, 16, 0x00, b"\x80"), "Odd-width NV12 envelope rejected before OpenCV conversion."),
    CorpusCase("raw_frame", "unsupported-format.bin", raw_frame_case(0x50, 16, 16, 0x00, b"\x55"), "Unsupported format selector rejected at the raw-frame boundary."),
    CorpusCase("raw_frame", "null-input-selection.bin", raw_frame_case(0x90, 16, 16, 0x00, b"\xaa"), "Null-input selector must return the C ABI null-pointer error."),
    CorpusCase(
        "raw_frame",
        "nonfinite-geometry.bin",
        raw_frame_case(0x10, 16, 16, 0x00, b"\x00" * 4 + struct.pack("<dddd", float("nan"), float("inf"), -float("inf"), 0.0)),
        "Line-geometry seeds include NaN and infinity and must not produce finite-looking invalid midpoints.",
    ),
    CorpusCase(
        "raw_frame",
        "degenerate-corners.bin",
        raw_frame_case(0x10, 16, 16, 0x00, b"\x34\x12\x34\x12\x34\x12\x34\x12\x34\x12\x34\x12\x34\x12\x34\x12"),
        "Degenerate repeated deskew corners are rejected before perspective warp.",
    ),
    CorpusCase(
        "frame_sequence",
        "valid-completion.bin",
        frame_sequence_case(
            8,
            384,
            b"valid-frame-sequence",
            bytes((FRAME_SEQUENCE_OP_COMPLETE,)),
        ),
        "Canonical generated fountain sequence completes and recovers exact source bytes.",
    ),
    CorpusCase(
        "frame_sequence",
        "reordered-duplicates.bin",
        frame_sequence_case(
            9,
            259,
            b"reorder-duplicate",
            bytes(
                (
                    FRAME_SEQUENCE_OP_DELIVER_SELECTED,
                    2,
                    FRAME_SEQUENCE_OP_DELIVER_SELECTED,
                    0,
                    FRAME_SEQUENCE_OP_DUPLICATE_LAST,
                    FRAME_SEQUENCE_OP_DELIVER_SELECTED,
                    3,
                    FRAME_SEQUENCE_OP_COMPLETE,
                )
            ),
        ),
        "Out-of-order packets and duplicates still complete at most once with exact output.",
    ),
    CorpusCase(
        "frame_sequence",
        "dropped-delayed-no-output.bin",
        frame_sequence_case(
            10,
            511,
            b"drop-delay",
            bytes(
                (
                    FRAME_SEQUENCE_OP_DELIVER_NEXT,
                    FRAME_SEQUENCE_OP_DROP_NEXT,
                    FRAME_SEQUENCE_OP_DELAY,
                    FRAME_SEQUENCE_OP_DELIVER_NEXT,
                    FRAME_SEQUENCE_OP_TAKE_CHECK,
                )
            ),
        ),
        "Dropped packets and transfer timeout leave no completed object visible.",
    ),
    CorpusCase(
        "frame_sequence",
        "mutated-envelope-no-output.bin",
        frame_sequence_case(
            11,
            200,
            b"mutated-envelope",
            bytes(
                (
                    FRAME_SEQUENCE_OP_MUTATE_SELECTED,
                    0,
                    3,
                    0,
                    FRAME_SEQUENCE_OP_TAKE_CHECK,
                )
            ),
        ),
        "A mutated block identifier is rejected before object output.",
    ),
    CorpusCase(
        "frame_sequence",
        "batched-reset-replay.bin",
        frame_sequence_case(
            12,
            640,
            b"batch-reset-replay",
            bytes(
                (
                    FRAME_SEQUENCE_OP_BATCH_NEXT,
                    3,
                    FRAME_SEQUENCE_OP_COMPLETE,
                    FRAME_SEQUENCE_OP_RESET,
                    FRAME_SEQUENCE_OP_COMPLETE,
                )
            ),
        ),
        "Batched packets complete once, reset clears terminal state, and replay completes exactly again.",
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
