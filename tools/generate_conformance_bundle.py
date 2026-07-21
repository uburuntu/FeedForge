#!/usr/bin/env python3
"""Build the deterministic FeedForge ITCH 5.0 conformance bundle."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import tarfile
import tempfile
import tomllib
from typing import Any, Iterable
import zipfile


BUNDLE_NAME = "feedforge-itch50-conformance-v1"
ARCHIVE_EPOCH = (1980, 1, 1, 0, 0, 0)
EXPECTED_FIXTURE_COUNT = 23


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=True, indent=2, sort_keys=True) + "\n").encode("ascii")


def parse_payload(raw_hex: Any, fixture_path: Path) -> bytes:
    if not isinstance(raw_hex, str):
        raise ValueError(f"{fixture_path}: raw_hex must be a string")
    try:
        return bytes.fromhex(raw_hex)
    except ValueError as error:
        raise ValueError(f"{fixture_path}: raw_hex is not valid hexadecimal") from error


def require(condition: bool, fixture_path: Path, description: str) -> None:
    if not condition:
        raise ValueError(f"{fixture_path}: {description}")


def load_fixtures(fixtures_dir: Path) -> list[dict[str, Any]]:
    fixture_paths = sorted(fixtures_dir.glob("*.toml"))
    if len(fixture_paths) != EXPECTED_FIXTURE_COUNT:
        raise ValueError(
            f"{fixtures_dir}: expected {EXPECTED_FIXTURE_COUNT} fixtures, found {len(fixture_paths)}"
        )

    loaded: list[dict[str, Any]] = []
    message_types: set[str] = set()
    message_names: set[str] = set()
    for ordinal, fixture_path in enumerate(fixture_paths, start=1):
        with fixture_path.open("rb") as fixture_file:
            fixture = tomllib.load(fixture_file)

        message_type = fixture.get("message_type")
        message_name = fixture.get("message_name")
        payload = parse_payload(fixture.get("raw_hex"), fixture_path)
        expected_prefix = f"{ordinal:02d}_"

        require(fixture_path.name.startswith(expected_prefix), fixture_path, "fixture order is not contiguous")
        require(fixture.get("format_version") == 1, fixture_path, "format_version must be 1")
        require(fixture.get("review_status") == "approved", fixture_path, "fixture is not approved")
        require(isinstance(message_type, str) and len(message_type) == 1, fixture_path,
                "message_type must contain one byte")
        require(isinstance(message_name, str) and message_name, fixture_path,
                "message_name must be non-empty")
        require(fixture_path.stem == f"{ordinal:02d}_{message_name}", fixture_path,
                "file name does not match message_name")
        require(message_type not in message_types, fixture_path, "duplicate message_type")
        require(message_name not in message_names, fixture_path, "duplicate message_name")
        require(fixture.get("raw_size") == len(payload), fixture_path, "raw_size does not match raw_hex")
        require(payload[:1] == message_type.encode("ascii"), fixture_path,
                "payload discriminator does not match message_type")

        fields = fixture.get("expected_fields")
        all_messages = fixture.get("expected_all_messages")
        order_events = fixture.get("expected_order_events")
        negative = fixture.get("negative")
        require(isinstance(fields, dict) and fields.get("message_type") == message_type,
                fixture_path, "expected_fields.message_type does not match")
        require(isinstance(all_messages, dict) and all_messages.get("result") == "emit" and
                all_messages.get("event") == message_name,
                fixture_path, "expected_all_messages is inconsistent")
        require(isinstance(order_events, dict) and order_events.get("result") in {"emit", "skip"},
                fixture_path, "expected_order_events.result is invalid")
        require(isinstance(negative, dict), fixture_path, "negative cases are missing")
        require(negative.get("size_minus_one", {}).get("size") == len(payload) - 1,
                fixture_path, "size_minus_one metadata is inconsistent")
        require(negative.get("size_plus_one", {}).get("size") == len(payload) + 1,
                fixture_path, "size_plus_one metadata is inconsistent")
        require(negative.get("size_minus_one", {}).get("expected_error") ==
                "invalid_message_size", fixture_path, "size_minus_one error is inconsistent")
        require(negative.get("size_plus_one", {}).get("expected_error") ==
                "invalid_message_size", fixture_path, "size_plus_one error is inconsistent")

        message_types.add(message_type)
        message_names.add(message_name)
        loaded.append({"path": fixture_path, "data": fixture, "payload": payload})
    return loaded


def binary_file(payloads: Iterable[bytes], *, complete: bool) -> bytes:
    output = bytearray()
    for payload in payloads:
        if len(payload) > 0xFFFF:
            raise ValueError("BinaryFILE payload exceeds the 16-bit length limit")
        output.extend(len(payload).to_bytes(2, "big"))
        output.extend(payload)
    if complete:
        output.extend(b"\x00\x00")
    return bytes(output)


def bundle_readme() -> bytes:
    return b"""# FeedForge synthetic ITCH 5.0 conformance bundle

This bundle contains deterministic synthetic payloads derived from FeedForge's
23 independently reviewed Nasdaq TotalView-ITCH 5.0 fixture manifests. It is a
software conformance aid, not exchange certification and not captured market
data.

## Layout

- `payloads/`: one valid raw ITCH payload for every reviewed message type.
- `streams/all-messages.binaryfile`: all 23 payloads in fixture order, framed
  with two-byte big-endian lengths and a zero-length end-of-session marker.
- `expected/`: normalized JSON outcomes for positive, decode-negative, and
  BinaryFILE cases. Fixed-width ASCII strings retain trailing spaces.
- `negative/payloads/`: empty, unknown-type, and size-boundary payloads.
- `negative/framing/`: complete, incomplete, malformed, and decode-error
  BinaryFILE inputs.
- `PROVENANCE.json`: fixture review and locked official-source metadata.
- `manifest.json`: byte sizes and SHA-256 hashes for every other bundle file.
- `LICENSE.txt`: the FeedForge Apache License 2.0 text.

All paths are relative to this versioned directory. JSON is UTF-8 with sorted
object keys and a final newline. Archive member order, modes, owners, and
timestamps are fixed. The ZIP uses stored entries; the tar.gz uses deterministic
stored DEFLATE blocks.

Verify a file by hashing its bytes with SHA-256 and comparing the result with
`manifest.json`. The manifest intentionally omits itself to avoid a recursive
hash.
"""


def write_file(root: Path, relative_path: str, data: bytes) -> None:
    destination = root / relative_path
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def normalized_message(fixture: dict[str, Any], ordinal: int) -> dict[str, Any]:
    data = fixture["data"]
    return {
        "expected_all_messages": data["expected_all_messages"],
        "expected_fields": data["expected_fields"],
        "expected_order_events": data["expected_order_events"],
        "fixture": fixture["path"].name,
        "message_name": data["message_name"],
        "message_type": data["message_type"],
        "payload": f"payloads/{fixture['path'].stem}.bin",
        "raw_size": len(fixture["payload"]),
        "spec_pages": data["spec_pages"],
        "spec_section": data["spec_section"],
        "ordinal": ordinal,
    }


def build_tree(root: Path, fixtures: list[dict[str, Any]], source_lock: Path,
               license_path: Path, generator_path: Path) -> None:
    write_file(root, "README.md", bundle_readme())
    write_file(root, "LICENSE.txt", license_path.read_bytes())

    messages: list[dict[str, Any]] = []
    negative_payload_cases: list[dict[str, Any]] = []
    payloads: list[bytes] = []
    for ordinal, fixture in enumerate(fixtures, start=1):
        payload = fixture["payload"]
        stem = fixture["path"].stem
        payload_path = f"payloads/{stem}.bin"
        write_file(root, payload_path, payload)
        payloads.append(payload)
        messages.append(normalized_message(fixture, ordinal))

        variants = (
            ("size-minus-one", payload[:-1]),
            ("size-plus-one", payload + b"\x00"),
        )
        for variant_name, variant_payload in variants:
            case_path = f"negative/payloads/{stem}.{variant_name}.bin"
            write_file(root, case_path, variant_payload)
            negative_payload_cases.append({
                "actual_size": len(variant_payload),
                "expected_size": len(payload),
                "message_name": fixture["data"]["message_name"],
                "message_type": fixture["data"]["message_type"],
                "path": case_path,
                "status": "invalid_message_size",
            })

    direct_negative_cases = (
        ("empty-payload", b"", "empty_payload"),
        ("unknown-message-type", b"?", "unknown_message_type"),
    )
    for case_id, payload, status in direct_negative_cases:
        case_path = f"negative/payloads/{case_id}.bin"
        write_file(root, case_path, payload)
        negative_payload_cases.append({
            "actual_size": len(payload),
            "expected_size": 0,
            "message_name": "",
            "message_type": "" if not payload else "?",
            "path": case_path,
            "status": status,
        })

    complete_stream = binary_file(payloads, complete=True)
    incomplete_stream = binary_file(payloads, complete=False)
    write_file(root, "streams/all-messages.binaryfile", complete_stream)
    write_file(root, "expected/messages.json", json_bytes({
        "format_version": 1,
        "messages": messages,
        "stream": {
            "frame_count": len(messages),
            "path": "streams/all-messages.binaryfile",
            "status": "complete",
        },
    }))
    write_file(root, "expected/negative.json", json_bytes({
        "cases": negative_payload_cases,
        "format_version": 1,
    }))

    first_payload = payloads[0]
    framing_inputs = {
        "complete-empty": b"\x00\x00",
        "incomplete-all-messages": incomplete_stream,
        "truncated-length-prefix": b"\x00",
        "truncated-payload": b"\x00\x02S",
        "trailing-data-after-end-marker": b"\x00\x00\xff",
        "unknown-message-type": binary_file([b"?"], complete=True),
        "invalid-message-size": binary_file([first_payload[:-1]], complete=True),
    }
    framing_expectations = {
        "complete-empty": {"bytes_consumed": 2, "frames_seen": 0, "status": "complete"},
        "incomplete-all-messages": {
            "bytes_consumed": len(incomplete_stream), "frames_seen": len(payloads),
            "status": "incomplete",
        },
        "truncated-length-prefix": {
            "bytes_consumed": 0, "error_offset": 0, "frames_seen": 0,
            "framing_error": "truncated_length_prefix", "status": "framing_error",
        },
        "truncated-payload": {
            "bytes_consumed": 0, "error_offset": 0, "frames_seen": 0,
            "framing_error": "truncated_payload", "status": "framing_error",
        },
        "trailing-data-after-end-marker": {
            "bytes_consumed": 2, "error_offset": 2, "frames_seen": 0,
            "framing_error": "trailing_data_after_end_marker", "status": "framing_error",
        },
        "unknown-message-type": {
            "bytes_consumed": 3, "decode_error": "unknown_message_type", "error_offset": 2,
            "frames_seen": 1, "status": "decode_error",
        },
        "invalid-message-size": {
            "bytes_consumed": 13, "decode_error": "invalid_message_size", "error_offset": 2,
            "frames_seen": 1, "status": "decode_error",
        },
    }
    framing_cases: list[dict[str, Any]] = []
    for case_id in sorted(framing_inputs):
        case_path = f"negative/framing/{case_id}.binaryfile"
        write_file(root, case_path, framing_inputs[case_id])
        framing_cases.append({
            "expected": framing_expectations[case_id],
            "id": case_id,
            "path": case_path,
        })
    write_file(root, "expected/framing.json", json_bytes({
        "cases": framing_cases,
        "format_version": 1,
    }))

    with source_lock.open("rb") as source_file:
        source_metadata = tomllib.load(source_file)
    fixture_provenance = [
        {
            "byte_source": fixture["data"]["byte_source"],
            "fixture": fixture["path"].name,
            "review_date": fixture["data"]["review_date"],
            "review_status": fixture["data"]["review_status"],
            "reviewer": fixture["data"]["reviewer"],
            "sha256": sha256_bytes(fixture["path"].read_bytes()),
        }
        for fixture in fixtures
    ]
    provenance = {
        "bundle_format_version": 1,
        "bundle_name": BUNDLE_NAME,
        "generator": {
            "path": "tools/generate_conformance_bundle.py",
            "sha256": sha256_bytes(generator_path.read_bytes()),
        },
        "notice": (
            "Synthetic reviewed test vectors; not exchange certification and not captured market data."
        ),
        "reviewed_fixtures": fixture_provenance,
        "source_lock": {
            "path": "schemas/sources.lock.toml",
            "sha256": sha256_bytes(source_lock.read_bytes()),
        },
        "sources": source_metadata,
    }
    write_file(root, "PROVENANCE.json", json_bytes(provenance))

    manifest_files = []
    for path in sorted(path for path in root.rglob("*") if path.is_file()):
        data = path.read_bytes()
        manifest_files.append({
            "bytes": len(data),
            "path": path.relative_to(root).as_posix(),
            "sha256": sha256_bytes(data),
        })
    write_file(root, "manifest.json", json_bytes({
        "bundle_format_version": 1,
        "bundle_name": BUNDLE_NAME,
        "files": manifest_files,
        "hash_algorithm": "sha256",
    }))


def archive_entries(root: Path) -> list[tuple[str, Path | None]]:
    entries: list[tuple[str, Path | None]] = [(f"{BUNDLE_NAME}/", None)]
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        archive_name = f"{BUNDLE_NAME}/{relative}"
        entries.append((archive_name + "/" if path.is_dir() else archive_name,
                        None if path.is_dir() else path))
    return entries


def deterministic_tar(root: Path) -> bytes:
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode="w", format=tarfile.USTAR_FORMAT) as archive:
        for name, path in archive_entries(root):
            info = tarfile.TarInfo(name=name)
            info.uid = 0
            info.gid = 0
            info.uname = ""
            info.gname = ""
            info.mtime = 0
            if path is None:
                info.type = tarfile.DIRTYPE
                info.mode = 0o755
                info.size = 0
                archive.addfile(info)
            else:
                data = path.read_bytes()
                info.mode = 0o644
                info.size = len(data)
                archive.addfile(info, io.BytesIO(data))
    return output.getvalue()


def deterministic_gzip(data: bytes) -> bytes:
    compressed = bytearray(b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff")
    if not data:
        compressed.extend(b"\x01\x00\x00\xff\xff")
    else:
        position = 0
        while position < len(data):
            block = data[position:position + 0xFFFF]
            position += len(block)
            compressed.append(1 if position == len(data) else 0)
            compressed.extend(struct.pack("<HH", len(block), 0xFFFF - len(block)))
            compressed.extend(block)
    compressed.extend(struct.pack("<II", binascii.crc32(data) & 0xFFFFFFFF,
                                  len(data) & 0xFFFFFFFF))
    return bytes(compressed)


def write_zip(root: Path, destination: Path) -> None:
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, path in archive_entries(root):
            info = zipfile.ZipInfo(name, date_time=ARCHIVE_EPOCH)
            info.create_system = 3
            info.compress_type = zipfile.ZIP_STORED
            if path is None:
                info.external_attr = (0o40755 << 16) | 0x10
                data = b""
            else:
                info.external_attr = 0o100644 << 16
                data = path.read_bytes()
            archive.writestr(info, data)


def remove_existing(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.is_dir():
        shutil.rmtree(path)


def generate_bundle(fixtures_dir: Path, source_lock: Path, license_path: Path,
                    output_dir: Path, generator_path: Path) -> tuple[Path, Path, Path]:
    fixtures = load_fixtures(fixtures_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=".feedforge-conformance-", dir=output_dir))
    try:
        staged_root = work / BUNDLE_NAME
        staged_root.mkdir()
        build_tree(staged_root, fixtures, source_lock, license_path, generator_path)

        staged_tar_gz = work / f"{BUNDLE_NAME}.tar.gz"
        staged_zip = work / f"{BUNDLE_NAME}.zip"
        staged_tar_gz.write_bytes(deterministic_gzip(deterministic_tar(staged_root)))
        write_zip(staged_root, staged_zip)

        final_root = output_dir / BUNDLE_NAME
        final_tar_gz = output_dir / staged_tar_gz.name
        final_zip = output_dir / staged_zip.name
        for destination in (final_root, final_tar_gz, final_zip):
            remove_existing(destination)
        os.replace(staged_root, final_root)
        os.replace(staged_tar_gz, final_tar_gz)
        os.replace(staged_zip, final_zip)
        return final_root, final_tar_gz, final_zip
    finally:
        shutil.rmtree(work, ignore_errors=True)


def parse_args() -> argparse.Namespace:
    repository_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixtures-dir", type=Path,
                        default=repository_root / "tests/fixtures/itch50")
    parser.add_argument("--source-lock", type=Path,
                        default=repository_root / "schemas/sources.lock.toml")
    parser.add_argument("--license", dest="license_path", type=Path,
                        default=repository_root / "LICENSE")
    parser.add_argument("--output-dir", type=Path,
                        default=repository_root / "out/conformance")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generated = generate_bundle(
        args.fixtures_dir.resolve(),
        args.source_lock.resolve(),
        args.license_path.resolve(),
        args.output_dir.resolve(),
        Path(__file__).resolve(),
    )
    for path in generated:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
